/********************************************************************
*
*
*	ServerGame: Crowd Developer Console Commands
*	File: svg_crowd_commands.cpp
*	Description:
*		Developer console command handlers for steering, assigning,
*		and querying crowd formation groups using Vector3DP.
*
*
********************************************************************/
#include "svgame/crowd/svg_crowd_commands.h"
#include "svgame/crowd/svg_crowd_manager.h"
#include "svgame/crowd/svg_crowd_formations.h"
#include "svgame/entities/svg_base_edict.h"
#include "svgame/svg_edict_pool.h"
#include "shared/math/qm_vector3_dp.h"

#include <cstdlib>
#include <cstring>
#include <vector>
#include <unordered_map>

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
	} else if ( Q_stricmp( token, "dashed" ) == 0 || Q_stricmp( token, "dashed_line" ) == 0 || Q_stricmp( token, "echelon" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_DASHED_LINE;
	} else if ( Q_stricmp( token, "cover" ) == 0 || Q_stricmp( token, "tactical_cover" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_TACTICAL_COVER;
	} else if ( Q_stricmp( token, "perimeter" ) == 0 || Q_stricmp( token, "surround" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER;
	} else if ( Q_stricmp( token, "column" ) == 0 || Q_stricmp( token, "march" ) == 0 ) {
		return crowd_chase_target_type_t::CROWD_STYLE_COLUMN_MARCH;
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
		default:
			return "unknown";
	}
}

/**
*	@brief	Command handler: crowd_move <crowdID> <x> <y> <z> [style] [spacing]
**/
static void SVG_Command_CrowdMove_f( void ) {
	if ( gi.argc() < 5 ) {
		gi.dprintf( "Usage: crowd_move <crowdID> <x> <y> <z> [style: line|arrow|circle|dashed|cover|perimeter|column] [spacing]\n" );
		return;
	}

	const int32_t crowdID = static_cast<int32_t>( std::strtol( gi.argv( 1 ), nullptr, 10 ) );
	const double x = std::strtod( gi.argv( 2 ), nullptr );
	const double y = std::strtod( gi.argv( 3 ), nullptr );
	const double z = std::strtod( gi.argv( 4 ), nullptr );

	crowd_chase_target_type_t style = crowd_chase_target_type_t::CROWD_STYLE_ARROW;
	if ( gi.argc() >= 6 ) {
		style = SVG_Crowd_ParseStyle( gi.argv( 5 ) );
	}

	svg_crowd_params_t params = {};
	if ( gi.argc() >= 7 ) {
		const double spacing = std::strtod( gi.argv( 6 ), nullptr );
		if ( spacing > 1.0 ) {
			params.lateralSpacing = spacing;
			params.longitudinalSpacing = spacing;
		}
	}

	const Vector3DP targetOrigin{ x, y, z };
	const bool success = MoveAStarCrowdOrigin( crowdID, targetOrigin, style, params );

	if ( success ) {
		gi.dprintf( "[CROWD] Crowd %d ordered to (%.1f, %.1f, %.1f) in style '%s'\n",
			crowdID, x, y, z, SVG_Crowd_GetStyleName( style ) );
	} else {
		gi.dprintf( "[CROWD] Failed to order crowd %d: no living members found or invalid destination\n", crowdID );
	}
}

/**
*	@brief	Command handler: crowd_follow <crowdID> <entityNum> [style] [spacing]
**/
static void SVG_Command_CrowdFollow_f( void ) {
	if ( gi.argc() < 3 ) {
		gi.dprintf( "Usage: crowd_follow <crowdID> <entityNum> [style: line|arrow|circle|dashed|cover|perimeter|column] [spacing]\n" );
		return;
	}

	const int32_t crowdID = static_cast<int32_t>( std::strtol( gi.argv( 1 ), nullptr, 10 ) );
	const int32_t entNum = static_cast<int32_t>( std::strtol( gi.argv( 2 ), nullptr, 10 ) );

	if ( entNum < 0 || entNum >= globals.edictPool->num_edicts ) {
		gi.dprintf( "[CROWD] Invalid entity number %d\n", entNum );
		return;
	}

	svg_base_edict_t *targetEnt = g_edict_pool.EdictForNumber( entNum );
	if ( !targetEnt || !SVG_Entity_IsActive( targetEnt ) || targetEnt->lifeStatus != entity_lifestatus_t::LIFESTATUS_ALIVE ) {
		gi.dprintf( "[CROWD] Entity %d is not in use or not alive\n", entNum );
		return;
	}

	crowd_chase_target_type_t style = crowd_chase_target_type_t::CROWD_STYLE_ARROW;
	if ( gi.argc() >= 4 ) {
		style = SVG_Crowd_ParseStyle( gi.argv( 3 ) );
	}

	svg_crowd_params_t params = {};
	if ( gi.argc() >= 5 ) {
		const double spacing = std::strtod( gi.argv( 4 ), nullptr );
		if ( spacing > 1.0 ) {
			params.lateralSpacing = spacing;
			params.longitudinalSpacing = spacing;
		}
	}

	const bool success = MoveAStarFollowEntity( crowdID, targetEnt, style, params );
	if ( success ) {
		gi.dprintf( "[CROWD] Crowd %d ordered to follow entity %d in style '%s'\n",
			crowdID, entNum, SVG_Crowd_GetStyleName( style ) );
	} else {
		gi.dprintf( "[CROWD] Failed to order crowd %d follow: no living members found\n", crowdID );
	}
}

/**
*	@brief	Command handler: crowd_style <crowdID> <style>
**/
static void SVG_Command_CrowdStyle_f( void ) {
	if ( gi.argc() < 3 ) {
		gi.dprintf( "Usage: crowd_style <crowdID> <style: line|arrow|circle|dashed|cover|perimeter|column>\n" );
		return;
	}

	const int32_t crowdID = static_cast<int32_t>( std::strtol( gi.argv( 1 ), nullptr, 10 ) );
	const crowd_chase_target_type_t style = SVG_Crowd_ParseStyle( gi.argv( 2 ) );

	SVG_Crowd_SetCrowdStyle( crowdID, style );
	gi.dprintf( "[CROWD] Crowd %d style updated to '%s'\n", crowdID, SVG_Crowd_GetStyleName( style ) );
}

/**
*	@brief	Command handler: crowd_stop <crowdID>
**/
static void SVG_Command_CrowdStop_f( void ) {
	if ( gi.argc() < 2 ) {
		gi.dprintf( "Usage: crowd_stop <crowdID>\n" );
		return;
	}

	const int32_t crowdID = static_cast<int32_t>( std::strtol( gi.argv( 1 ), nullptr, 10 ) );
	SVG_Crowd_StopCrowd( crowdID );
	gi.dprintf( "[CROWD] Crowd %d stopped and cover leases released\n", crowdID );
}

/**
*	@brief	Command handler: crowd_set <entityNum> <crowdID>
**/
static void SVG_Command_CrowdSet_f( void ) {
	if ( gi.argc() < 3 ) {
		gi.dprintf( "Usage: crowd_set <entityNum> <crowdID: -1=none, 0=neutral/NPC, >0=squad>\n" );
		return;
	}

	const int32_t entNum = static_cast<int32_t>( std::strtol( gi.argv( 1 ), nullptr, 10 ) );
	const int32_t crowdID = static_cast<int32_t>( std::strtol( gi.argv( 2 ), nullptr, 10 ) );

	if ( entNum < 1 || entNum >= globals.edictPool->num_edicts ) {
		gi.dprintf( "[CROWD] Invalid entity number %d\n", entNum );
		return;
	}

	svg_base_edict_t *ent = g_edict_pool.EdictForNumber( entNum );
	if ( !ent || !SVG_Entity_IsActive( ent ) ) {
		gi.dprintf( "[CROWD] Entity %d is not in use\n", entNum );
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

		gi.dprintf( "Crowd %d (%s) - %zu members, moving=%s, style='%s':\n  Entities: ",
			cid, ( cid == 0 ? "Neutral NPC" : "Combat Squad" ), ents.size(),
			isMoving ? "true" : "false", styleName );

		for ( size_t k = 0; k < ents.size(); k++ ) {
			gi.dprintf( "%d%s", ents[ k ], ( k + 1 < ents.size() ? ", " : "\n" ) );
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

	if ( Q_stricmp( cmd, "crowd_move" ) == 0 ) {
		SVG_Command_CrowdMove_f();
		return true;
	} else if ( Q_stricmp( cmd, "crowd_follow" ) == 0 ) {
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
	} else if ( Q_stricmp( cmd, "crowd_list" ) == 0 ) {
		SVG_Command_CrowdList_f();
		return true;
	}

	return false;
}
