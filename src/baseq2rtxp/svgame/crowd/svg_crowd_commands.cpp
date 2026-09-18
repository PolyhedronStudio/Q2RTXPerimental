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

#include <cstdlib>
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
*	@brief	Command handler: crowd_move [crowdID] [x y z | or aim crosshair] [style] [spacing]
**/
static void SVG_Command_CrowdMove_f( void ) {
	const int32_t argc = SVG_Crowd_Argc();
	int32_t crowdID = 1;
	Vector3DP targetOrigin{ 0.0, 0.0, 0.0 };
	crowd_chase_target_type_t style = crowd_chase_target_type_t::CROWD_STYLE_ARROW;
	svg_crowd_params_t params = {};

	// Explicit coordinate format:
	// crowd_move <crowdID> <x> <y> <z> [style] [spacing]
	const bool hasExplicitCoords = ( argc >= 4 &&
		( std::strtod( SVG_Crowd_Argv( 1 ), nullptr ) != 0.0 || strcmp( SVG_Crowd_Argv( 1 ), "0" ) == 0 ) &&
		( std::strtod( SVG_Crowd_Argv( 2 ), nullptr ) != 0.0 || strcmp( SVG_Crowd_Argv( 2 ), "0" ) == 0 ) &&
		( std::strtod( SVG_Crowd_Argv( 3 ), nullptr ) != 0.0 || strcmp( SVG_Crowd_Argv( 3 ), "0" ) == 0 ) );

	if ( hasExplicitCoords ) {
		crowdID = static_cast<int32_t>( std::strtol( SVG_Crowd_Argv( 0 ), nullptr, 10 ) );
		targetOrigin.x = std::strtod( SVG_Crowd_Argv( 1 ), nullptr );
		targetOrigin.y = std::strtod( SVG_Crowd_Argv( 2 ), nullptr );
		targetOrigin.z = std::strtod( SVG_Crowd_Argv( 3 ), nullptr );

		if ( argc >= 5 ) {
			style = SVG_Crowd_ParseStyle( SVG_Crowd_Argv( 4 ) );
		}
		if ( argc >= 6 ) {
			const double spacing = std::strtod( SVG_Crowd_Argv( 5 ), nullptr );
			if ( spacing > 1.0 ) {
				params.lateralSpacing = spacing;
				params.longitudinalSpacing = spacing;
			}
		}
	} else {
		// Crosshair targeting format: crowd_move [crowdID] [style] [spacing]
		if ( !SVG_Crowd_TraceCrosshair( &targetOrigin, nullptr ) ) {
			gi.dprintf( "[CROWD] Crosshair trace did not hit walkable ground. Usage: crowd_move [crowdID] [style] [spacing] OR crowd_move <crowdID> <x> <y> <z> [style]\n" );
			return;
		}

		if ( argc >= 1 ) {
			char *endPtr = nullptr;
			const long val = std::strtol( SVG_Crowd_Argv( 0 ), &endPtr, 10 );
			if ( endPtr != SVG_Crowd_Argv( 0 ) && *endPtr == '\0' ) {
				crowdID = static_cast<int32_t>( val );
			} else {
				// Style string passed without numeric crowdID (e.g. crowd_move diamond)
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

	const bool success = MoveAStarCrowdOrigin( crowdID, targetOrigin, style, params );

	if ( success ) {
		gi.dprintf( "[CROWD] Crowd %d ordered to (%.1f, %.1f, %.1f) in style '%s'\n",
			crowdID, targetOrigin.x, targetOrigin.y, targetOrigin.z, SVG_Crowd_GetStyleName( style ) );
	} else {
		gi.dprintf( "[CROWD] Failed to order crowd %d: no living members found or invalid destination\n", crowdID );
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
	*	Report physical feet positions alongside targets so off-slot stalls are observable.
	**/
	for ( const int32_t entityNumber : group->memberEntityNumbers ) {
		svg_base_edict_t *member = g_edict_pool.EdictForNumber( entityNumber );
		// Ignore dead or removed members while retaining stable entity numbers in the log.
		if ( member == nullptr || !SVG_Entity_IsActive( member ) || member->health <= 0 ) {
			continue;
		}
		const Vector3DP feet = SVG_GetEntityFeetOriginDP( member );
		const Vector3 &goal = member->crowd.assignedGoalOrigin;
		const svg_monster_base_t *monster = dynamic_cast<svg_monster_base_t*>( member );
		gi.dprintf( "[crowd member] ent=%" PRId32 " inRank=%" PRId32 " egRank=%" PRId32 " slot=%" PRId32 " inRel=%d egRel=%d arrived=%d feet=(%.2f %.2f %.2f) goal=(%.2f %.2f %.2f) path=%zu/%zu nextthink=%" PRIu64 "\n",
			entityNumber, member->crowd.ingressQueueRank, member->crowd.egressQueueRank, member->crowd.slotIndex,
			member->crowd.ingressReleased, member->crowd.egressReleased, member->crowd.reachedGoal,
			feet.x, feet.y, feet.z, goal.x, goal.y, goal.z,
			monster != nullptr ? monster->stringPathPos : size_t{ 0 },
			monster != nullptr ? monster->stringPulledPath.size() : size_t{ 0 },
			static_cast<uint64_t>( member->nextthink.Milliseconds() ) );
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
			if ( member->crowd.ingressReleased && !member->crowd.reachedGoal ) {
				for ( size_t index = 0; index < monster->stringPulledPath.size(); index++ ) {
					const Vector3DP &point = monster->stringPulledPath[ index ];
					gi.dprintf( "[crowd route] ent=%" PRId32 " index=%zu point=(%.2f %.2f %.2f) forced=%d\n",
						entityNumber, index, point.x, point.y, point.z,
						index < monster->stringPulledWaypointForced.size() && monster->stringPulledWaypointForced[ index ] );
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
	}

	return false;
}
