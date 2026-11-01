/********************************************************************
*
*
*	ServerGame: Crowd Developer Console Commands
*	File: svg_crowd_commands.cpp
*	Description:
*		Developer console command handlers for steering, assigning,
*		and querying crowd formation groups using Vector3DP.
*		Supports point-and-click crosshair targeting for all squad commands.
*
*
********************************************************************/
#include "svgame/crowd/svg_crowd_commands.h"
#include "svgame/crowd/svg_crowd_manager.h"
#include "svgame/crowd/svg_crowd_formations.h"
#include "svgame/entities/svg_base_edict.h"
#include "svgame/entities/monster/svg_monster_base.h"
#include "svgame/svg_edict_pool.h"
#include "svgame/svg_utils.h"
#include "svgame/monsters/svg_mmove.h"
#include "shared/math/qm_vector3_dp.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <cstring>
#include <vector>
#include <unordered_map>

/**
*	Crosshair Raycast Helper:
**/

/**
*	@brief	Perform a trace from the first active player's view direction along the crosshair.
*	@param	outEndPos	[out] World-space impact origin in Vector3DP.
*	@param	outHitEnt	[out] Impact entity (or nullptr if hit world or no entity).
*	@return	True if a valid impact occurred.
**/
static bool SVG_Crowd_TraceCrosshair( Vector3DP *outEndPos, svg_base_edict_t **outHitEnt ) {
	if ( outEndPos ) {
		*outEndPos = Vector3DP{ 0.0, 0.0, 0.0 };
	}
	if ( outHitEnt ) {
		*outHitEnt = nullptr;
	}

	svg_base_edict_t *player = g_edict_pool.EdictForNumber( 1 );
	if ( !player || !player->inUse || !player->client ) {
		return false;
	}

	Vector3 vForward, vRight, vUp;
	QM_AngleVectors( player->client->viewMove.viewAngles, &vForward, &vRight, &vUp );

	Vector3 traceStart = player->currentOrigin;
	traceStart.z += ( ( player->viewheight != 0.0f ) ? player->viewheight : 22.0f );
	const Vector3 traceEnd = QM_Vector3MultiplyAdd( traceStart, 8192.0f, vForward );

	const svg_trace_t tr = SVG_Trace( traceStart, qm_vector3_null, qm_vector3_null, traceEnd, player, static_cast<cm_contents_t>( CM_CONTENTMASK_SOLID | CM_CONTENTMASK_MONSTERSOLID ) );

	if ( tr.fraction >= 1.0f ) {
		return false;
	}

	Vector3 hitPos = tr.endpos;

	// If the crosshair hit a vertical or steep wall (e.g. aiming at an interior room wall, doorway jamb, or barrier),
	// step into the walkable volume along the wall plane normal and drop a vertical trace to find the floor surface:
	if ( tr.plane.normal[ 2 ] < NAV_MIN_WALKABLE_Z ) {
		//! Wall standoff distance in world units when projecting floor trace from vertical wall impact.
		static constexpr float CROWD_WALL_TRACE_STANDOFF = 16.0f;
		//! Maximum vertical drop distance in world units to search for walkable floor below wall impact.
		static constexpr float CROWD_WALL_FLOOR_DROP_DIST = 1024.0f;

		const Vector3 floorProbeStart = QM_Vector3MultiplyAdd( hitPos, CROWD_WALL_TRACE_STANDOFF, tr.plane.normal );
		const Vector3 floorProbeEnd = QM_Vector3Add( floorProbeStart, Vector3{ 0.0f, 0.0f, -CROWD_WALL_FLOOR_DROP_DIST } );
		const svg_trace_t floorTr = SVG_Trace( floorProbeStart, qm_vector3_null, qm_vector3_null, floorProbeEnd, player, CM_CONTENTMASK_SOLID );
		if ( floorTr.fraction < 1.0f && !floorTr.startsolid && floorTr.plane.normal[ 2 ] >= NAV_MIN_WALKABLE_Z ) {
			hitPos = floorTr.endpos;
		}
	}

	if ( outEndPos ) {
		*outEndPos = Vector3DP( hitPos );
	}
	if ( outHitEnt ) {
		*outHitEnt = ( tr.ent && tr.ent->inUse && tr.ent != player ) ? tr.ent : nullptr;
	}

	return true;
}

/**
*	@brief	Get the command argument index offset depending on whether invoked via 'sv <cmd>' or direct console command '<cmd>'.
*	@return	Offset to add to 0-based argument index (2 if argv[0] == "sv", else 1).
**/
static inline int32_t SVG_Crowd_GetArgOffset( void ) {
	if ( gi.argc() > 1 && Q_stricmp( gi.argv( 0 ), "sv" ) == 0 ) {
		return 2;
	}
	return 1;
}

/**
*	@brief	Get effective count of arguments passed to the crowd command (excluding command names and 'sv').
*	@return	Number of user-provided arguments.
**/
static inline int32_t SVG_Crowd_Argc( void ) {
	const int32_t offset = SVG_Crowd_GetArgOffset();
	return std::max( 0, gi.argc() - offset );
}

/**
*	@brief	Get argument string at 0-based user argument index.
*	@param	argIndex	0-based index of argument (e.g. 0 is the first parameter after crowd_move).
*	@return	Pointer to argument string, or empty string if out of bounds.
**/
static inline const char *SVG_Crowd_Argv( const int32_t argIndex ) {
	const int32_t offset = SVG_Crowd_GetArgOffset();
	const int32_t rawIndex = offset + argIndex;
	if ( rawIndex < gi.argc() ) {
		return gi.argv( rawIndex );
	}
	return "";
}

/**
*	@brief	Parse a string token into its corresponding crowd formation style.
*	@param	token	String name of the style.
*	@return	Parsed crowd_chase_target_type_t enum value.
**/
static crowd_chase_target_type_t SVG_Crowd_ParseStyle( const char *token ) {
	if ( !token ) {
		return crowd_chase_target_type_t::CROWD_STYLE_ARROW;
	}

	if ( Q_stricmp( token, "line" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_LINE;
	} else if ( Q_stricmp( token, "arrow" ) == 0 || Q_stricmp( token, "wedge" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_ARROW;
	} else if ( Q_stricmp( token, "circle" ) == 0 || Q_stricmp( token, "circle_filled" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_CIRCLE_FILLED;
	} else if ( Q_stricmp( token, "dashed" ) == 0 || Q_stricmp( token, "dashed_line" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_DASHED_LINE;
	} else if ( Q_stricmp( token, "cover" ) == 0 || Q_stricmp( token, "tactical_cover" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_TACTICAL_COVER;
	} else if ( Q_stricmp( token, "perimeter" ) == 0 || Q_stricmp( token, "surround" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER;
	} else if ( Q_stricmp( token, "column" ) == 0 || Q_stricmp( token, "march" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_COLUMN_MARCH;
	} else if ( Q_stricmp( token, "staggered" ) == 0 || Q_stricmp( token, "staggered_column" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_STAGGERED_COLUMN;
	} else if ( Q_stricmp( token, "diamond" ) == 0 || Q_stricmp( token, "box" ) == 0 || Q_stricmp( token, "box_diamond" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_BOX_DIAMOND;
	} else if ( Q_stricmp( token, "echelon_left" ) == 0 || Q_stricmp( token, "echelon_l" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_ECHELON_LEFT;
	} else if ( Q_stricmp( token, "echelon" ) == 0 || Q_stricmp( token, "echelon_right" ) == 0 || Q_stricmp( token, "echelon_r" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_ECHELON_RIGHT;
	}

	return crowd_chase_target_type_t::CROWD_STYLE_ARROW;
}

/**
*	@brief	Get human-readable display name for a crowd chase style.
*	@param	style	Style enum value.
*	@return	String name.
**/
static const char *SVG_Crowd_GetStyleName( const crowd_chase_target_type_t style ) {
	switch ( style ) {
		case crowd_chase_target_type_t::CROWD_STYLE_LINE:
			return "line";
		case crowd_chase_target_type_t::CROWD_STYLE_ARROW:
			return "arrow";
		case crowd_chase_target_type_t::CROWD_STYLE_CIRCLE_FILLED:
			return "circle_filled";
		case crowd_chase_target_type_t::CROWD_STYLE_DASHED_LINE:
			return "dashed_line";
		case crowd_chase_target_type_t::CROWD_STYLE_TACTICAL_COVER:
			return "tactical_cover";
		case crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER:
			return "surround_perimeter";
		case crowd_chase_target_type_t::CROWD_STYLE_COLUMN_MARCH:
			return "column_march";
		case crowd_chase_target_type_t::CROWD_STYLE_STAGGERED_COLUMN:
			return "staggered_column";
		case crowd_chase_target_type_t::CROWD_STYLE_BOX_DIAMOND:
			return "box_diamond";
		case crowd_chase_target_type_t::CROWD_STYLE_ECHELON_LEFT:
			return "echelon_left";
		case crowd_chase_target_type_t::CROWD_STYLE_ECHELON_RIGHT:
			return "echelon_right";
		default:
			return "unknown";
	}
}

/**
*	@brief	Parse one complete finite coordinate token, including decimal zero.
*	@param	token	Console coordinate text.
*	@param	value	Parsed coordinate on success.
*	@return	True only for a complete finite token within double range.
**/
static bool SVG_Crowd_ParseCoordinate( const char *token, double &value ) {
	/**
	*	Reject empty tokens before conversion and reject partial or overflowing conversions.
	**/
	if ( token == nullptr || *token == '\0' ) {
		return false;
	}
	errno = 0;
	char *end = nullptr;
	const double parsed = std::strtod( token, &end );
	if ( end == token || *end != '\0' || errno == ERANGE || !std::isfinite( parsed ) ) {
		return false;
	}
	value = parsed;
	return true;
}

/**
*	@brief	Order a crowd to explicit coordinates or to the player's crosshair.
*	@note	Four or more arguments select explicit coordinates; invalid coordinates never become crosshair orders.
**/
static void SVG_Command_CrowdMove_f( void ) {
	/**
	*	Initialize default formation settings before selecting the command format.
	**/
	const int32_t argc = SVG_Crowd_Argc();
	int32_t crowdID = 1;
	Vector3DP targetOrigin{ 0.0, 0.0, 0.0 };
	crowd_chase_target_type_t style = crowd_chase_target_type_t::CROWD_STYLE_ARROW;
	svg_crowd_params_t params = {};

	/**
	*	Parse explicit coordinates without treating any valid representation of zero as missing.
	**/
	if ( argc >= 4 ) {
		// Require complete finite values so malformed commands cannot move a different formation.
		if ( !SVG_Crowd_ParseCoordinate( SVG_Crowd_Argv( 1 ), targetOrigin.x ) ||
			 !SVG_Crowd_ParseCoordinate( SVG_Crowd_Argv( 2 ), targetOrigin.y ) ||
			 !SVG_Crowd_ParseCoordinate( SVG_Crowd_Argv( 3 ), targetOrigin.z ) ) {
			gi.dprintf( "[CROWD] Invalid explicit coordinates; expected three finite numeric tokens.\n" );
			return;
		}
		// Validate the identifier before narrowing it to the crowd's fixed-width storage.
		errno = 0;
		char *end = nullptr;
		const int64_t identifier = std::strtoll( SVG_Crowd_Argv( 0 ), &end, 10 );
		if ( end == SVG_Crowd_Argv( 0 ) || *end != '\0' || errno == ERANGE ||
			 identifier < 0 || identifier > std::numeric_limits<int32_t>::max() ) {
			gi.dprintf( "[CROWD] Invalid crowd identifier.\n" );
			return;
		}
		crowdID = static_cast<int32_t>( identifier );
		// Optional formation and spacing follow the three coordinates.
		if ( argc >= 5 ) {
			style = SVG_Crowd_ParseStyle( SVG_Crowd_Argv( 4 ) );
		}
		if ( argc >= 6 ) {
			double spacing = 0.0;
			if ( SVG_Crowd_ParseCoordinate( SVG_Crowd_Argv( 5 ), spacing ) && spacing > 1.0 ) {
				params.lateralSpacing = spacing;
				params.longitudinalSpacing = spacing;
			}
		}
	} else {
		/**
		*	Resolve the crosshair destination before parsing optional identifier/style/spacing.
		**/
		if ( !SVG_Crowd_TraceCrosshair( &targetOrigin, nullptr ) ) {
			gi.dprintf( "[CROWD] Crosshair trace did not hit walkable ground. Usage: crowd_move [crowdID] [style] [spacing] OR crowd_move <crowdID> <x> <y> <z> [style]\n" );
			return;
		}
		// A nonnumeric first argument retains the shorthand style-only command.
		if ( argc >= 1 ) {
			char *end = nullptr;
			errno = 0;
			const int64_t identifier = std::strtoll( SVG_Crowd_Argv( 0 ), &end, 10 );
			if ( end != SVG_Crowd_Argv( 0 ) && *end == '\0' && errno != ERANGE &&
				 identifier >= 0 && identifier <= std::numeric_limits<int32_t>::max() ) {
				crowdID = static_cast<int32_t>( identifier );
			} else {
				style = SVG_Crowd_ParseStyle( SVG_Crowd_Argv( 0 ) );
			}
		}
		if ( argc >= 2 ) {
			style = SVG_Crowd_ParseStyle( SVG_Crowd_Argv( 1 ) );
		}
		if ( argc >= 3 ) {
			double spacing = 0.0;
			if ( SVG_Crowd_ParseCoordinate( SVG_Crowd_Argv( 2 ), spacing ) && spacing > 1.0 ) {
				params.lateralSpacing = spacing;
				params.longitudinalSpacing = spacing;
			}
		}
	}

	/**
	*	Publish success only after the movement order and its queue setup commit successfully.
	**/
	const bool success = MoveAStarCrowdOrigin( crowdID, targetOrigin, style, params );
	if ( success ) {
		gi.dprintf( "[CROWD] Crowd %" PRId32 " ordered to (%.1f, %.1f, %.1f) in style '%s'\n",
			crowdID, targetOrigin.x, targetOrigin.y, targetOrigin.z, SVG_Crowd_GetStyleName( style ) );
	} else {
		gi.dprintf( "[CROWD] Failed to order crowd %" PRId32 ": no living members found or invalid destination\n", crowdID );
	}
}

/**
*	@brief	Command handler: crowd_follow [crowdID] [entityNum | or aim crosshair at entity] [style] [spacing]
**/
static void SVG_Command_CrowdFollow_f( void ) {
	const int32_t argc = SVG_Crowd_Argc();
	int32_t crowdID = 1;
	int32_t targetEntityNum = ENTITYNUM_NONE;
	crowd_chase_target_type_t style = crowd_chase_target_type_t::CROWD_STYLE_ARROW;
	svg_crowd_params_t params = {};

	// Check if explicit entity number was passed: crowd_follow <crowdID> <entityNum> [style] [spacing]
	bool explicitTarget = false;
	if ( argc >= 2 ) {
		char *endPtr1 = nullptr;
		const long val1 = std::strtol( SVG_Crowd_Argv( 1 ), &endPtr1, 10 );
		if ( endPtr1 != SVG_Crowd_Argv( 1 ) && *endPtr1 == '\0' && val1 > 0 ) {
			crowdID = static_cast<int32_t>( std::strtol( SVG_Crowd_Argv( 0 ), nullptr, 10 ) );
			targetEntityNum = static_cast<int32_t>( val1 );
			explicitTarget = true;

			if ( argc >= 3 ) {
				style = SVG_Crowd_ParseStyle( SVG_Crowd_Argv( 2 ) );
			}
			if ( argc >= 4 ) {
				const double spacing = std::strtod( SVG_Crowd_Argv( 3 ), nullptr );
				if ( spacing > 1.0 ) {
					params.lateralSpacing = spacing;
					params.longitudinalSpacing = spacing;
				}
			}
		}
	}

	if ( !explicitTarget ) {
		// Crosshair targeting format: crowd_follow [crowdID] [style] [spacing]
		svg_base_edict_t *hitEnt = nullptr;
		Vector3DP hitPos;
		SVG_Crowd_TraceCrosshair( &hitPos, &hitEnt );

		if ( hitEnt && hitEnt->inUse && hitEnt->health > 0 ) {
			targetEntityNum = hitEnt->s.number;
		} else {
			// Fallback: follow player 1
			targetEntityNum = 1;
		}

		if ( argc >= 1 ) {
			char *endPtr = nullptr;
			const long val = std::strtol( SVG_Crowd_Argv( 0 ), &endPtr, 10 );
			if ( endPtr != SVG_Crowd_Argv( 0 ) && *endPtr == '\0' ) {
				crowdID = static_cast<int32_t>( val );
			} else {
				style = SVG_Crowd_ParseStyle( SVG_Crowd_Argv( 0 ) );
			}
		}
		if ( argc >= 2 ) {
			style = SVG_Crowd_ParseStyle( SVG_Crowd_Argv( 1 ) );
		}
		if ( argc >= 3 ) {
			const double spacing = std::strtod( SVG_Crowd_Argv( 2 ), nullptr );
			if ( spacing > 1.0 ) {
				params.lateralSpacing = spacing;
				params.longitudinalSpacing = spacing;
			}
		}
	}

	const bool success = MoveAStarFollowEntity( crowdID, targetEntityNum, style, params );
	if ( success ) {
		gi.dprintf( "[CROWD] Crowd %d ordered to follow entity %d in style '%s'\n",
			crowdID, targetEntityNum, SVG_Crowd_GetStyleName( style ) );
	} else {
		gi.dprintf( "[CROWD] Failed to order crowd %d follow: no living members found or invalid entity %d\n", crowdID, targetEntityNum );
	}
}

/**
*	@brief	Command handler: crowd_style [crowdID] <style>
**/
static void SVG_Command_CrowdStyle_f( void ) {
	const int32_t argc = SVG_Crowd_Argc();
	if ( argc < 1 ) {
		gi.dprintf( "Usage: crowd_style [crowdID] <style: line|arrow|circle|dashed|cover|perimeter|column|staggered|diamond|echelon_left|echelon_right>\n" );
		return;
	}

	int32_t crowdID = 1;
	crowd_chase_target_type_t style = crowd_chase_target_type_t::CROWD_STYLE_ARROW;

	if ( argc >= 2 ) {
		crowdID = static_cast<int32_t>( std::strtol( SVG_Crowd_Argv( 0 ), nullptr, 10 ) );
		style = SVG_Crowd_ParseStyle( SVG_Crowd_Argv( 1 ) );
	} else {
		char *endPtr = nullptr;
		const long val = std::strtol( SVG_Crowd_Argv( 0 ), &endPtr, 10 );
		if ( endPtr != SVG_Crowd_Argv( 0 ) && *endPtr == '\0' ) {
			crowdID = static_cast<int32_t>( val );
		} else {
			style = SVG_Crowd_ParseStyle( SVG_Crowd_Argv( 0 ) );
		}
	}

	SVG_Crowd_SetCrowdStyle( crowdID, style );
	gi.dprintf( "[CROWD] Crowd %d style updated to '%s'\n", crowdID, SVG_Crowd_GetStyleName( style ) );
}

/**
*	@brief	Command handler: crowd_stop [crowdID]
**/
static void SVG_Command_CrowdStop_f( void ) {
	const int32_t argc = SVG_Crowd_Argc();
	int32_t crowdID = 1;

	if ( argc >= 1 ) {
		crowdID = static_cast<int32_t>( std::strtol( SVG_Crowd_Argv( 0 ), nullptr, 10 ) );
	} else {
		// If looking at an entity belonging to a crowd, stop that crowd
		svg_base_edict_t *hitEnt = nullptr;
		Vector3DP hitPos;
		if ( SVG_Crowd_TraceCrosshair( &hitPos, &hitEnt ) && hitEnt && hitEnt->crowd.crowdID >= 0 ) {
			crowdID = hitEnt->crowd.crowdID;
		}
	}

	SVG_Crowd_StopCrowd( crowdID );
	gi.dprintf( "[CROWD] Crowd %d stopped and cover leases released\n", crowdID );
}

/**
*	@brief	Command handler: crowd_set [entityNum | or aim crosshair at entity] <crowdID>
**/
static void SVG_Command_CrowdSet_f( void ) {
	const int32_t argc = SVG_Crowd_Argc();
	int32_t entNum = ENTITYNUM_NONE;
	int32_t crowdID = 1;

	if ( argc >= 2 ) {
		// Explicit: crowd_set <entityNum> <crowdID>
		entNum = static_cast<int32_t>( std::strtol( SVG_Crowd_Argv( 0 ), nullptr, 10 ) );
		crowdID = static_cast<int32_t>( std::strtol( SVG_Crowd_Argv( 1 ), nullptr, 10 ) );
	} else if ( argc == 1 ) {
		// Crosshair targeting: crowd_set <crowdID>
		crowdID = static_cast<int32_t>( std::strtol( SVG_Crowd_Argv( 0 ), nullptr, 10 ) );
		svg_base_edict_t *hitEnt = nullptr;
		Vector3DP hitPos;
		if ( SVG_Crowd_TraceCrosshair( &hitPos, &hitEnt ) && hitEnt ) {
			entNum = hitEnt->s.number;
		} else {
			gi.dprintf( "[CROWD] No entity under crosshair. Usage: crowd_set [entityNum] <crowdID: -1=none, 0=neutral/NPC, >0=squad>\n" );
			return;
		}
	} else {
		gi.dprintf( "Usage: crowd_set [entityNum | or look at entity] <crowdID: -1=none, 0=neutral/NPC, >0=squad>\n" );
		return;
	}

	if ( entNum < 1 || entNum >= globals.edictPool->num_edicts ) {
		gi.dprintf( "[CROWD] Invalid entity number %d\n", entNum );
		return;
	}

	svg_base_edict_t *ent = g_edict_pool.EdictForNumber( entNum );
	if ( !ent || !SVG_Entity_IsActive( ent ) ) {
		gi.dprintf( "[CROWD] Entity %d is not in use\n", entNum );
		return;
	}

	if ( !ent->GetTypeInfo()->IsSubClassType<svg_monster_base_t>() ) {
		gi.dprintf( "[CROWD] Entity %d is not a monster (must be a derivative of svg_monster_base_t)\n", entNum );
		return;
	}

	if ( crowdID < 0 ) {
		SVG_Crowd_UnregisterMember( ent );
		gi.dprintf( "[CROWD] Entity %d removed from crowd\n", entNum );
	} else {
		SVG_Crowd_RegisterMember( ent, crowdID );
		gi.dprintf( "[CROWD] Entity %d assigned to crowd %d\n", entNum, crowdID );
	}
}

/**
*	@brief	Command handler: crowd_leader [crowdID] [entityNum | or aim crosshair at squad member]
**/
static void SVG_Command_CrowdLeader_f( void ) {
	const int32_t argc = SVG_Crowd_Argc();
	int32_t crowdID = 1;
	int32_t leaderEntNum = ENTITYNUM_NONE;

	if ( argc >= 2 ) {
		// Explicit: crowd_leader <crowdID> <entityNum>
		crowdID = static_cast<int32_t>( std::strtol( SVG_Crowd_Argv( 0 ), nullptr, 10 ) );
		leaderEntNum = static_cast<int32_t>( std::strtol( SVG_Crowd_Argv( 1 ), nullptr, 10 ) );
	} else {
		if ( argc >= 1 ) {
			crowdID = static_cast<int32_t>( std::strtol( SVG_Crowd_Argv( 0 ), nullptr, 10 ) );
		}

		// Crosshair targeting: designate entity under crosshair
		svg_base_edict_t *hitEnt = nullptr;
		Vector3DP hitPos;
		if ( SVG_Crowd_TraceCrosshair( &hitPos, &hitEnt ) && hitEnt ) {
			if ( !hitEnt->GetTypeInfo()->IsSubClassType<svg_monster_base_t>() ) {
				gi.dprintf( "[CROWD] Entity under crosshair (%d) is not a monster (must be a derivative of svg_monster_base_t)\n", hitEnt->s.number );
				return;
			}
			leaderEntNum = hitEnt->s.number;
			// Automatically register into squad if not already a member
			if ( hitEnt->crowd.crowdID != crowdID ) {
				SVG_Crowd_RegisterMember( hitEnt, crowdID );
			}
		} else {
			gi.dprintf( "[CROWD] No entity under crosshair. Usage: crowd_leader [crowdID] [entityNum]\n" );
			return;
		}
	}

	SVG_Crowd_SetLeader( crowdID, leaderEntNum );
	gi.dprintf( "[CROWD] Entity %d designated as Squad Leader for Crowd %d\n", leaderEntNum, crowdID );
}

/**
*	@brief	Command handler: crowd_list
**/
static void SVG_Command_CrowdList_f( void ) {
	gi.dprintf( "=== Active Crowd Groups ===\n" );

	std::unordered_map<int32_t, std::vector<int32_t>> crowdMembers;
	for ( int32_t i = 1; i < globals.edictPool->num_edicts; i++ ) {
		svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i );
		if ( ent && SVG_Entity_IsActive( ent ) && ent->crowd.crowdID >= 0 ) {
			crowdMembers[ ent->crowd.crowdID ].push_back( ent->s.number );
		}
	}

	if ( crowdMembers.empty() ) {
		gi.dprintf( "No entities currently assigned to any crowd.\n" );
		return;
	}

	for ( const auto &pair : crowdMembers ) {
		const int32_t cid = pair.first;
		const std::vector<int32_t> &ents = pair.second;

		const svg_crowd_group_t *group = SVG_Crowd_GetGroup( cid );
		const char *styleName = group ? SVG_Crowd_GetStyleName( group->style ) : "none";
		const bool isMoving = group ? group->isMoving : false;
		const int32_t leaderNum = group ? group->leaderEntityNumber : ENTITYNUM_NONE;

		gi.dprintf( "Crowd %d (%s) - %zu members, leader=%d, moving=%s, style='%s', squeeze=%.2f:\n  Entities: ",
			cid, ( cid == 0 ? "Neutral NPC" : "Combat Squad" ), ents.size(),
			leaderNum, isMoving ? "true" : "false", styleName,
			group ? group->dynamicSqueezeFactor : 1.0 );

		for ( size_t k = 0; k < ents.size(); k++ ) {
			const bool isLead = ( ents[ k ] == leaderNum );
			gi.dprintf( "%d%s%s", ents[ k ], isLead ? "(L)" : "", ( k + 1 < ents.size() ? ", " : "\n" ) );
		}
	}
}

/**
*	@brief	Print staging, slot and navigation state for the requested crowd.
*	@note	Runs only on explicit `sv crowd_status <id>` requests; adds no per-frame logging.
**/
static void SVG_Command_CrowdStatus_f( void ) {
	/**
	*	Resolve the requested group before inspecting its direct member registry.
	**/
	const int32_t crowdID = std::atoi( SVG_Crowd_Argv( 0 ) );
	const svg_crowd_group_t *group = SVG_Crowd_GetGroup( crowdID );
	// Missing groups have no state to report.
	if ( group == nullptr ) {
		gi.dprintf( "[crowd status] group=%" PRId32 " missing\n", crowdID );
		return;
	}
	gi.dprintf( "[crowd status] group=%" PRId32 " moving=%d ingress=%d egress=%d inHead=%" PRId32 "/%zu egHead=%" PRId32 "/%zu portal=(%.2f %.2f %.2f) inward=(%.3f %.3f) width=%.2f\n",
		crowdID, group->isMoving, group->hasSerializedIngress, group->hasSerializedEgress,
		group->ingressQueueHead, group->ingressQueueEntityNumbers.size(),
		group->egressQueueHead, group->egressQueueEntityNumbers.size(),
		group->ingressPortalOrigin.x, group->ingressPortalOrigin.y, group->ingressPortalOrigin.z,
		group->ingressPortalInward.x, group->ingressPortalInward.y, group->ingressPortalHalfWidth * 2.0 );
	/**
	*	Expose the current adjacent-cell obligation on explicit status requests without authorizing movement.
	**/
	const int32_t cell = group->ingressHeadStagingCell;
	// Only an active bounded staging leg has geometry to report; no diagnostic indexes a stale cell.
	if ( group->hasSerializedIngress && cell > 0 && static_cast<size_t>( cell ) < group->ingressStagingTransitRoutes.size() ) {
		const auto &transit = group->ingressStagingTransitRoutes[ cell ];
		gi.dprintf( "[crowd transit] group=%" PRId32 " head=%" PRId32 " cell=%" PRId32 " points=%zu claims=%zu\n",
			crowdID, group->ingressQueueHead, cell, transit.size(), group->ingressYieldClaims.size() );
		// Bound diagnostic output while explicitly reporting any omitted points.
		for ( size_t index = 0; index < std::min( transit.size(), size_t{ 128 } ); ++index ) {
			const auto &point = transit[ index ];
			gi.dprintf( "[crowd transit point] index=%zu feet=(%.9f %.9f %.9f)\n", index, point.x, point.y, point.z );
		}
		// A diagnostic limit cannot silently become complete geometry evidence.
		if ( transit.size() > 128 ) {
			gi.dprintf( "[crowd transit] incomplete=1 omitted=%zu\n", transit.size() - 128 );
		}
	}
	/**
	*	Report physical feet positions alongside targets so off-slot stalls are observable.
	**/
	for ( const int32_t entityNumber : group->memberEntityNumbers ) {
		svg_base_edict_t *member = g_edict_pool.EdictForNumber( entityNumber );
		// Ignore dead or removed members while retaining stable entity numbers in the log.
		if ( member == nullptr || !SVG_Entity_IsActive( member ) || member->health <= 0 ) {
			continue;
		}
		const Vector3DP feet = SVG_GetEntityFeetOriginDP( member );
		// Physical handoff and staging ownership include spawn count/incarnation; report them independently of arrival flags.
		gi.dprintf( "[crowd owner] ent=%" PRId32 " spawn=%" PRId32 " current_head=%d waiting_yield=%d\n",
			entityNumber, member->spawn_count, SVG_Crowd_IsCurrentIngressHead( member ), SVG_Crowd_IsWaitingForApproachYield( member ) );
		const Vector3 &goal = member->crowd.assignedGoalOrigin;
		const svg_monster_base_t *monster = dynamic_cast<svg_monster_base_t*>( member );
		gi.dprintf( "[crowd member] ent=%" PRId32 " inRank=%" PRId32 " egRank=%" PRId32 " slot=%" PRId32 " inRel=%d egRel=%d arrived=%d feet=(%.2f %.2f %.2f) goal=(%.2f %.2f %.2f) path=%zu/%zu nextthink=%" PRIu64 "\n",
			entityNumber, member->crowd.ingressQueueRank, member->crowd.egressQueueRank, member->crowd.slotIndex,
			member->crowd.ingressReleased, member->crowd.egressReleased, member->crowd.reachedGoal,
			feet.x, feet.y, feet.z, goal.x, goal.y, goal.z,
			monster != nullptr ? monster->stringPathPos : size_t{ 0 },
			monster != nullptr ? monster->stringPulledPath.size() : size_t{ 0 },
			static_cast<uint64_t>( member->nextthink.Milliseconds() ) );
		/**
		*	Report the exact rejection and staging owner independently of route availability.
		**/
		if ( monster != nullptr ) {
			gi.dprintf( "[crowd validation] ent=%" PRId32 " rejected=%d physical=%u geometric=%u clearance=%.4f edge=%" PRId32 " traces=%" PRIu32 " policy=(%.2f %.2f %.2f)\n",
				entityNumber, monster->navigationCommandRejected, static_cast<uint32_t>( monster->lastMovementValidation.reason ),
				static_cast<uint32_t>( monster->lastGeometricValidation.reason ), monster->lastGeometricValidation.minimum_clearance,
				monster->lastGeometricValidation.witness_edge, monster->navigationValidationTraceCount,
				monster->pathNavigationState.policy.max_step_height, monster->pathNavigationState.policy.max_drop_height,
				monster->pathNavigationState.policy.min_step_normal );
			const auto &route = monster->lastRouteValidation;
			gi.dprintf( "[crowd route validation] ent=%" PRId32 " reason=%u segment=%" PRId32 " physical=%u geometric=%u clearance=%.6f edge=%" PRId32 " samples=%" PRIu32 " traces=%" PRIu32 "\n",
				entityNumber, static_cast<uint32_t>( route.reason ), route.segment, static_cast<uint32_t>( route.physical ),
				static_cast<uint32_t>( route.geometric.reason ), route.geometric.minimum_clearance, route.geometric.witness_edge, route.samples, route.traces );
		}
		const int32_t rank = member->crowd.ingressQueueRank;
		// A queue rank must have matching geometry before its assigned goal can be assessed.
		if ( rank >= 0 && static_cast<size_t>( rank ) < group->ingressStagingPositions.size() ) {
			const Vector3DP &stage = group->ingressStagingPositions[ rank ];
			gi.dprintf( "[crowd stage] ent=%" PRId32 " rank=%" PRId32 " feet=(%.2f %.2f %.2f) error=%.2f\n",
				entityNumber, rank, stage.x, stage.y, stage.z, QM_Vector3Distance2DDP( feet, stage ) );
		}
		// Show the active path waypoint as well as the distant assignment to distinguish steering stalls.
		if ( monster != nullptr && monster->stringPathPos < monster->stringPulledPath.size() ) {
			const Vector3DP &waypoint = monster->stringPulledPath[ monster->stringPathPos ];
			gi.dprintf( "[crowd waypoint] ent=%" PRId32 " next=(%.2f %.2f %.2f) velocity=(%.2f %.2f %.2f)\n",
				entityNumber, waypoint.x, waypoint.y, waypoint.z, member->velocity.x, member->velocity.y, member->velocity.z );
			gi.dprintf( "[crowd ground] ent=%" PRId32 " support=%" PRId32 " grounded=%d final=(%.2f %.2f %.2f)\n",
				entityNumber, member->groundInfo.entityNumber, ( monster->monsterMove.state.mm_flags & MMF_ON_GROUND ) != 0,
				monster->stringPulledPath.back().x, monster->stringPulledPath.back().y, monster->stringPulledPath.back().z );
			gi.dprintf( "[crowd input] ent=%" PRId32 " velocity=(%.2f %.2f %.2f)\n", entityNumber,
				monster->monsterMove.state.previousVelocity.x, monster->monsterMove.state.previousVelocity.y,
				monster->monsterMove.state.previousVelocity.z );
			// The reservation owner's full route makes premature corner cutting observable.
			if ( SVG_Crowd_Argc() >= 2 && std::atoi( SVG_Crowd_Argv( 1 ) ) == entityNumber ) {
				for ( size_t index = 0; index < monster->stringPulledPath.size(); index++ ) {
					const Vector3DP &point = monster->stringPulledPath[ index ];
					gi.dprintf( "[crowd route] ent=%" PRId32 " index=%zu point=(%.2f %.2f %.2f) forced=%d\n",
						entityNumber, index, point.x, point.y, point.z,
						monster->SVG_Nav_IsWaypointMandatory( index ) );
				}
			}
			// A read-only sweep identifies physical obstructions independently of navmesh connectivity.
			Vector3DP nextCenter = waypoint;
			nextCenter.z -= static_cast<double>( member->mins.z );
			const svg_trace_t trace = SVG_MMove_Trace( Vector3DP( member->currentOrigin ), member->mins, member->maxs,
				nextCenter, member, CONTENTS_NONE, SVG_MMove_GetNativeShape( member ) );
			gi.dprintf( "[crowd sweep] ent=%" PRId32 " fraction=%.4f solid=%d/%d hit=%" PRId32 " brush=%" PRId32 " normal=(%.3f %.3f %.3f)\n",
				entityNumber, trace.fraction, trace.startsolid, trace.allsolid,
				trace.entityNumber, trace.brushID, trace.plane.normal[ 0 ], trace.plane.normal[ 1 ], trace.plane.normal[ 2 ] );
		}
	}
}

/**
*	@brief	Automated ingress completion checker for M1 regression and benchmark validation.
*	@note	Validates all arrivals, moving=0, ingress=0, inHead=N/N, unique final slots,
*			feet-to-final-slot error, floor support, and pairwise hull separation.
**/
static void SVG_Command_CrowdCheckIngress_f( void ) {
	// Validate crowd ID argument.
	if ( SVG_Crowd_Argc() < 1 ) {
		gi.dprintf( "Usage: sv crowd_check_ingress <crowdID> [max_seat_error] [min_separation]\n" );
		return;
	}

	const int32_t crowdID = std::atoi( SVG_Crowd_Argv( 0 ) );
	const double maxSeatErrorAllowed = ( SVG_Crowd_Argc() >= 2 ) ? std::atof( SVG_Crowd_Argv( 1 ) ) : 16.0;
	const double minSeparationRequired = ( SVG_Crowd_Argc() >= 3 ) ? std::atof( SVG_Crowd_Argv( 2 ) ) : 24.0;

	const svg_crowd_group_t *group = SVG_Crowd_GetGroup( crowdID );
	// Verify crowd group existence.
	if ( group == nullptr ) {
		gi.dprintf( "[ingress check] FAIL: group=%" PRId32 " does not exist\n", crowdID );
		return;
	}

	// Invalid thresholds must never turn comparisons into an automatic pass.
	if ( !std::isfinite( maxSeatErrorAllowed ) || maxSeatErrorAllowed < 0.0 ||
		 !std::isfinite( minSeparationRequired ) || minSeparationRequired < 0.0 ) {
		gi.dprintf( "[ingress check] FAIL: invalid finite acceptance thresholds\n" );
		return;
	}

	/**
	*	Validate controller completion before inspecting immutable final slot occupancy.
	**/
	bool pass = true;
	gi.dprintf( "[ingress check] ===== CROWD %" PRId32 " INGRESS VERIFICATION =====\n", crowdID );

	// Check whether the group is still in motion.
	if ( group->isMoving ) {
		gi.dprintf( "[ingress check] FAIL: group is still marked moving (isMoving=1)\n" );
		pass = false;
	}
	// Check whether serialized ingress coordination is still active.
	if ( group->hasSerializedIngress ) {
		gi.dprintf( "[ingress check] FAIL: group still has active serialized ingress (hasSerializedIngress=1)\n" );
		pass = false;
	}

	// Check whether the ingress queue head reached all admitted members.
	const size_t ingressTotal = group->ingressQueueEntityNumbers.size();
	if ( ingressTotal > 0 && group->ingressQueueHead < static_cast<int32_t>( ingressTotal ) ) {
		gi.dprintf( "[ingress check] FAIL: ingress queue not fully processed: head=%" PRId32 "/%zu\n",
			group->ingressQueueHead, ingressTotal );
		pass = false;
	}

	// Track member status for slot uniqueness and pairwise separation checks.
	struct member_check_info_t {
		int32_t entNum;
		int32_t slotIdx;
		Vector3DP feet;
		Vector3DP goal;
		double radius;
		double seatError;
		bool arrived;
		bool grounded;
	};
	std::vector<member_check_info_t> activeMembers;
	std::unordered_map<int32_t, int32_t> slotOwners;
	double maxObservedSeatError = 0.0;
	size_t arrivedCount = 0;

	// Iterate all members registered to this crowd group.
	for ( const int32_t entityNumber : group->memberEntityNumbers ) {
		svg_base_edict_t *member = g_edict_pool.EdictForNumber( entityNumber );
		// Skip inactive or dead entities.
		if ( member == nullptr || !SVG_Entity_IsActive( member ) || member->health <= 0 ) {
			continue;
		}

		const Vector3DP feet = SVG_GetEntityFeetOriginDP( member );
		const int32_t slotIndex = member->crowd.slotIndex;
		// A staging goal is transient; only the immutable formation slot proves final occupancy.
		if ( slotIndex < 0 || static_cast<size_t>( slotIndex ) >= group->slots.size() ||
			 !group->slots[ slotIndex ].isNavmeshValid ) {
			gi.dprintf( "[ingress check] FAIL: ent=%" PRId32 " has no valid final slot\n", entityNumber );
			pass = false;
			continue;
		}
		const Vector3DP &goal = group->slots[ slotIndex ].worldPosition;
		const svg_monster_base_t *monster = dynamic_cast<svg_monster_base_t*>( member );

		const double dx = feet.x - static_cast<double>( goal.x );
		const double dy = feet.y - static_cast<double>( goal.y );
		const double seatErr2D = std::sqrt( dx * dx + dy * dy );
		if ( seatErr2D > maxObservedSeatError ) {
			maxObservedSeatError = seatErr2D;
		}

		const bool isGrounded = ( monster != nullptr && ( monster->monsterMove.state.mm_flags & MMF_ON_GROUND ) != 0 )
			|| ( member->groundInfo.entityNumber != ENTITYNUM_NONE );

		member_check_info_t info = {};
		info.entNum = entityNumber;
		info.slotIdx = member->crowd.slotIndex;
		info.feet = feet;
		info.goal = goal;
		info.seatError = seatErr2D;
		info.radius = std::max( std::fabs( static_cast<double>( member->mins.x ) ), static_cast<double>( member->maxs.x ) );
		info.arrived = member->crowd.reachedGoal && seatErr2D <= maxSeatErrorAllowed &&
			std::fabs( feet.z - goal.z ) <= MONSTER_NAV_FINAL_GOAL_MAX_Z_DELTA &&
			( member->crowd.ingressQueueRank < 0 || member->crowd.ingressReleased );
		// Count physical final arrivals independently of unique slot ownership.
		if ( info.arrived ) {
			++arrivedCount;
		}
		info.grounded = isGrounded;
		activeMembers.push_back( info );

		// Check arrival status.
		if ( !info.arrived ) {
			gi.dprintf( "[ingress check] FAIL: ent=%" PRId32 " has not reached its final slot\n", entityNumber );
			pass = false;
		}

		// Check slot uniqueness.
		if ( info.slotIdx >= 0 ) {
			const auto it = slotOwners.find( info.slotIdx );
			if ( it != slotOwners.end() ) {
				gi.dprintf( "[ingress check] FAIL: duplicate slot %" PRId32 " shared by ent=%" PRId32 " and ent=%" PRId32 "\n",
					info.slotIdx, it->second, entityNumber );
				pass = false;
			} else {
				slotOwners[ info.slotIdx ] = entityNumber;
			}
		}

		// Check seating accuracy threshold.
		if ( seatErr2D > maxSeatErrorAllowed ) {
			gi.dprintf( "[ingress check] FAIL: ent=%" PRId32 " seat error %.2f exceeds allowed threshold %.2f\n",
				entityNumber, seatErr2D, maxSeatErrorAllowed );
			pass = false;
		}

		// Check grounding support.
		if ( !isGrounded ) {
			gi.dprintf( "[ingress check] FAIL: ent=%" PRId32 " is not grounded (no ground support)\n", entityNumber );
			pass = false;
		}

		gi.dprintf( "[ingress check member] ent=%" PRId32 " slot=%" PRId32 " arrived=%d grounded=%d seatErr=%.2f feet=(%.2f %.2f %.2f) goal=(%.2f %.2f %.2f)\n",
			entityNumber, info.slotIdx, info.arrived ? 1 : 0, info.grounded ? 1 : 0, seatErr2D,
			feet.x, feet.y, feet.z, goal.x, goal.y, goal.z );
	}

	// An empty/dead roster cannot prove the requested crowd filled its destination.
	if ( activeMembers.empty() || activeMembers.size() != group->memberEntityNumbers.size() ) {
		gi.dprintf( "[ingress check] FAIL: active roster does not match ordered members\n" );
		pass = false;
	}

	/**
	*	Compare every active hull pair against physical radii as well as the requested threshold.
	**/
	double minObservedSep2D = 1e9;
	int32_t closestPairA = -1;
	int32_t closestPairB = -1;

	// Check separation across all pairs of active members.
	for ( size_t i = 0; i < activeMembers.size(); ++i ) {
		// Each unordered pair is measured once; both native radii contribute to non-overlap.
		for ( size_t j = i + 1; j < activeMembers.size(); ++j ) {
			const double dx = activeMembers[ i ].feet.x - activeMembers[ j ].feet.x;
			const double dy = activeMembers[ i ].feet.y - activeMembers[ j ].feet.y;
			const double dist2D = std::sqrt( dx * dx + dy * dy );

			// Retain the closest pair for a reproducible separation witness.
			if ( dist2D < minObservedSep2D ) {
				minObservedSep2D = dist2D;
				closestPairA = activeMembers[ i ].entNum;
				closestPairB = activeMembers[ j ].entNum;
			}

			// Report separation violation if two agents are clipping into each other's footprint.
			const double requiredSeparation = std::max( minSeparationRequired, activeMembers[ i ].radius + activeMembers[ j ].radius );
			if ( dist2D < requiredSeparation ) {
				gi.dprintf( "[ingress check] FAIL: pair (%" PRId32 ", %" PRId32 ") separation %.2f < required %.2f\n",
					activeMembers[ i ].entNum, activeMembers[ j ].entNum, dist2D, requiredSeparation );
				pass = false;
			}
		}
	}

	gi.dprintf( "[ingress check] VERDICT: %s (members=%zu, arrived=%zu, unique_slots=%zu, max_seat_err=%.2f, min_sep2d=%.2f between ents %" PRId32 " and %" PRId32 ")\n",
		pass ? "PASS" : "FAIL", activeMembers.size(), arrivedCount, slotOwners.size(), maxObservedSeatError,
		( minObservedSep2D < 1e8 ? minObservedSep2D : 0.0 ), closestPairA, closestPairB );
}

/**
*	@brief	Register all crowd developer console commands.
**/
void SVG_Crowd_RegisterCommands( void ) {
	// Commands are dispatched via SVG_ServerCommand / SVG_ClientCommand strings.
}

/**
*	@brief	Dispatch crowd server commands issued via 'sv <cmd>'.
*	@param	cmd	Command token (e.g. 'crowd_move', 'crowd_follow', etc.).
*	@return	True if the command was recognized and handled.
**/
bool SVG_Crowd_ServerCommand( const char *cmd ) {
	if ( !cmd ) {
		return false;
	}

	if ( Q_stricmp( cmd, "crowd_move" ) == 0 || Q_stricmp( cmd, "crowd_mode" ) == 0 ||
		 Q_stricmp( cmd, "move_crowd" ) == 0 || Q_stricmp( cmd, "movecrowd" ) == 0 ) {
		SVG_Command_CrowdMove_f();
		return true;
	} else if ( Q_stricmp( cmd, "crowd_follow" ) == 0 || Q_stricmp( cmd, "follow_crowd" ) == 0 || Q_stricmp( cmd, "followcrowd" ) == 0 ) {
		SVG_Command_CrowdFollow_f();
		return true;
	} else if ( Q_stricmp( cmd, "crowd_style" ) == 0 ) {
		SVG_Command_CrowdStyle_f();
		return true;
	} else if ( Q_stricmp( cmd, "crowd_stop" ) == 0 ) {
		SVG_Command_CrowdStop_f();
		return true;
	} else if ( Q_stricmp( cmd, "crowd_set" ) == 0 ) {
		SVG_Command_CrowdSet_f();
		return true;
	} else if ( Q_stricmp( cmd, "crowd_leader" ) == 0 ) {
		SVG_Command_CrowdLeader_f();
		return true;
	} else if ( Q_stricmp( cmd, "crowd_list" ) == 0 ) {
		SVG_Command_CrowdList_f();
		return true;
	} else if ( Q_stricmp( cmd, "crowd_status" ) == 0 ) {
		SVG_Command_CrowdStatus_f();
		return true;
	} else if ( Q_stricmp( cmd, "crowd_check_ingress" ) == 0 || Q_stricmp( cmd, "check_ingress" ) == 0 ) {
		SVG_Command_CrowdCheckIngress_f();
		return true;
	}

	return false;
}
