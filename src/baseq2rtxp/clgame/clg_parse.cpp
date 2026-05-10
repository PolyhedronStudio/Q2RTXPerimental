/********************************************************************
*
*
*	ClientGame: Server Message Parsing.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_events.h"
#include "clgame/clg_hud.h"
#include "clgame/clg_parse.h"
#include "clgame/clg_screen.h"
#include "clgame/clg_temp_entities.h"

#include "clgame/game_ui/clg_ui_main.h"

#include "sharedgame/sg_cmd_messages.h"
#include "sharedgame/sg_entity_flags.h"
#include "sharedgame/sg_entity_events.h"
#include "sharedgame/sg_game_ui.h"
#include "sharedgame/sg_tempentity_events.h"



/**
*
*
*	GameUI Menus:
*
*
**/
/**
*	@brief	Will open the specified menu, based on the menuID received from the server. 
*			This is for the server to be able to open a menu on the client, such as the 
*			team selection menu at the beginning of a (death-)match.
**/
static void CLG_ParseGameUI_OpenMenu( void ) {
	// Parse the menuID.
	const sg_game_ui_menu_id menuID = ( sg_game_ui_menu_id )clgi.MSG_ReadUint8();

	// Ensure the ID is an actual valid and existing one.
	if ( menuID <= sg_game_ui_menu_id::NONE || menuID >= sg_game_ui_menu_id::MAX_ID ) {
		clgi.Print( PRINT_DEVELOPER, "CLG_ParseGameUI_OpenMenu: Invalid menu ID %d received from server.\n", menuID );
		return;
	}
	
	// Pass it on to the actual menu system code for further processing.
	CLG_UI_OpenMenu( menuID );
}
/**
*	@brief	Will open the specified menu, based on the menuID received from the server.
*			This is for the server to be able to open a menu on the client, such as the
*			team selection menu at the beginning of a (death-)match.
**/
static void CLG_ParseGameUI_CloseMenu( void ) {
	// Parse the menuID.
	const int32_t menuID = clgi.MSG_ReadUint8();

	// Pass it on to the actual menu system code for further processing.
}

/***
*
*
*   Layout:
*
*
***/
/**
*	@brief	Parses the layout string for server cmd: svc_inventory.
**/
static void CLG_ParseLayout( void ) {
	clgi.MSG_ReadString( clgi.client->layout, sizeof( clgi.client->layout ) );
	clgi.ShowNet( 2, "    \"%s\"\n", clgi.client->layout );
}



/***
*
*
*   Inventory:
*
*
***/
/**
*	@brief	Parses the player inventory for server cmd: svc_inventory.
**/
static void CLG_ParseInventory( void ) {
	for ( int32_t i = 0; i < MAX_ITEMS; i++ ) {
		game.inventory[ i ] = clgi.MSG_ReadIntBase128();
	}
}

/**
*
* 
*   Temp Entity Parsing:
* 
* 
**/
static void CLG_ParseTEntPacket( void ) {
    level.parsedMessage.events.tempEntity.type = clgi.MSG_ReadUint8();

    switch ( level.parsedMessage.events.tempEntity.type ) {
	#if 0
    //case TE_BLOOD:
    //case TE_GUNSHOT:
    //case TE_SPARKS:
    //case TE_BULLET_SPARKS:
    case TE_HEATBEAM_SPARKS:
    case TE_HEATBEAM_STEAM:
    //case TE_MOREBLOOD:
    case TE_ELECTRIC_SPARKS:
            clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
            clgi.MSG_ReadDir8( level.parsedMessage.events.tempEntity.dir );
        break;

//    case TE_SPLASH:
    case TE_LASER_SPARKS:
    case TE_WELDING_SPARKS:
    case TE_TUNNEL_SPARKS:
            level.parsedMessage.events.tempEntity.count = clgi.MSG_ReadUint8();
            clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
            clgi.MSG_ReadDir8( level.parsedMessage.events.tempEntity.dir );
            level.parsedMessage.events.tempEntity.color = clgi.MSG_ReadUint8();
        break;
    case TE_BUBBLETRAIL:
    case TE_DEBUG_TRAIL:
    case TE_BUBBLETRAIL2:
            clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
            clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos2, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
        break;
    //case TE_PLAIN_EXPLOSION:
    ////case TE_TELEPORT_EFFECT:
    //        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //    break;

    //case TE_FLASHLIGHT:
    //        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //        level.parsedMessage.events.tempEntity.entity1 = clgi.MSG_ReadInt16();
    //    break;


    case TE_STEAM:
            level.parsedMessage.events.tempEntity.entity1 = clgi.MSG_ReadInt16();
            level.parsedMessage.events.tempEntity.count = clgi.MSG_ReadUint8();
            clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
            clgi.MSG_ReadDir8( level.parsedMessage.events.tempEntity.dir );
            level.parsedMessage.events.tempEntity.color = clgi.MSG_ReadUint8();
            level.parsedMessage.events.tempEntity.entity2 = clgi.MSG_ReadInt16();
            if ( level.parsedMessage.events.tempEntity.entity1 != -1 ) {
                level.parsedMessage.events.tempEntity.time = clgi.MSG_ReadInt32();
            }
        break;

    //case TE_HEATBEAM:
    //case TE_MONSTER_HEATBEAM:
    //    level.parsedMessage.events.tempEntity.entity1 = clgi.MSG_ReadInt16();
    //    clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //    clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos2, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //    break;

    //case TE_GRAPPLE_CABLE:
    //    level.parsedMessage.events.tempEntity.entity1 = clgi.MSG_ReadInt16();
    //    clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //    clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos2, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //    clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.offset, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //    break;

    //case TE_LIGHTNING:
    //    level.parsedMessage.events.tempEntity.entity1 = clgi.MSG_ReadInt16();
    //    level.parsedMessage.events.tempEntity.entity2 = clgi.MSG_ReadInt16();
    //    clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //    clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos2, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //    break;

    //case TE_FORCEWALL:
    //    clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //    clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos2, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //    level.parsedMessage.events.tempEntity.color = clgi.MSG_ReadUint8();
    //    break;
	#endif

    case TE_DEBUG_TRAIL:
		clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
		clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos2, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
		break;
    case TE_DEBUG_BBOX:
    case TE_DEBUG_POINT:
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos2, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
        break;
	//case TE_DEBUG_BBOX:
	//	clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_NONE );
	//	clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.offset, MSG_POSITION_ENCODING_NONE );
	//	break;
	//case TE_DEBUG_POINT:
	//	clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, MSG_POSITION_ENCODING_NONE );
	//	clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.offset, MSG_POSITION_ENCODING_NONE );
	//	break;
    default:
        Com_WPrintf( "%s: unsupported temp entity type (%d)\n", __func__, level.parsedMessage.events.tempEntity.type );
        break;
    }
}


/***
*
*
*   Nav3 Debug Draw Message Parsing:
*
*
**/
/**
*   @brief  Read a world-space vector encoded as three floats.
*   @param  outVec3  [out] Destination vector.
**/
static void CLG_DebugDraw_ReadVec3( vec3_t outVec3 ) {
    outVec3[ 0 ] = clgi.MSG_ReadFloat();
    outVec3[ 1 ] = clgi.MSG_ReadFloat();
    outVec3[ 2 ] = clgi.MSG_ReadFloat();
}

/**
*   @brief  Draw an oriented box by submitting twelve edge line segments.
**/
static void CLG_DebugDraw_DrawObb(
    const vec3_t center,
    const vec3_t extents,
    const vec3_t axisX,
    const vec3_t axisY,
    const vec3_t axisZ,
    const uint32_t color ) {
    // Build scaled basis vectors from normalized axes and half-size extents.
    vec3_t scaledAxisX = { axisX[ 0 ] * extents[ 0 ], axisX[ 1 ] * extents[ 0 ], axisX[ 2 ] * extents[ 0 ] };
    vec3_t scaledAxisY = { axisY[ 0 ] * extents[ 1 ], axisY[ 1 ] * extents[ 1 ], axisY[ 2 ] * extents[ 1 ] };
    vec3_t scaledAxisZ = { axisZ[ 0 ] * extents[ 2 ], axisZ[ 1 ] * extents[ 2 ], axisZ[ 2 ] * extents[ 2 ] };

    // Enumerate corner sign combinations.
    static constexpr int8_t cornerSigns[ 8 ][ 3 ] = {
        { -1, -1, -1 },
        {  1, -1, -1 },
        { -1,  1, -1 },
        {  1,  1, -1 },
        { -1, -1,  1 },
        {  1, -1,  1 },
        { -1,  1,  1 },
        {  1,  1,  1 },
    };

    // Build oriented box corners.
    vec3_t corners[ 8 ] = {};
    for ( int32_t i = 0; i < 8; i++ ) {
        corners[ i ][ 0 ] = center[ 0 ]
            + ( scaledAxisX[ 0 ] * cornerSigns[ i ][ 0 ] )
            + ( scaledAxisY[ 0 ] * cornerSigns[ i ][ 1 ] )
            + ( scaledAxisZ[ 0 ] * cornerSigns[ i ][ 2 ] );
        corners[ i ][ 1 ] = center[ 1 ]
            + ( scaledAxisX[ 1 ] * cornerSigns[ i ][ 0 ] )
            + ( scaledAxisY[ 1 ] * cornerSigns[ i ][ 1 ] )
            + ( scaledAxisZ[ 1 ] * cornerSigns[ i ][ 2 ] );
        corners[ i ][ 2 ] = center[ 2 ]
            + ( scaledAxisX[ 2 ] * cornerSigns[ i ][ 0 ] )
            + ( scaledAxisY[ 2 ] * cornerSigns[ i ][ 1 ] )
            + ( scaledAxisZ[ 2 ] * cornerSigns[ i ][ 2 ] );
    }

    // Render oriented box edges.
    static constexpr uint8_t edgePairs[ 12 ][ 2 ] = {
        { 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 },
        { 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
    };
    for ( int32_t i = 0; i < 12; i++ ) {
        clgi.R_DrawDebugLine( corners[ edgePairs[ i ][ 0 ] ], corners[ edgePairs[ i ][ 1 ] ], color );
    }
}

/**
*   @brief  Parse one `svc_debug_draw` payload and optionally submit primitives to renderer debug draw.
*   @param  shouldRender  True to submit decoded primitives to renderer, false to parse-only and discard.
**/
static void CLG_ParseDebugDraw( const bool shouldRender ) {
    // Validate protocol version so parse offsets remain deterministic.
    const int32_t protocolVersion = clgi.MSG_ReadUint8();
    if ( protocolVersion != SG_SVC_DEBUG_DRAW_VERSION ) {
        Com_Error( ERR_DROP, "%s: unsupported debug draw version (%d)", __func__, protocolVersion );
    }

    // Read bounded primitive count for this message chunk.
    const int32_t primitiveCount = clgi.MSG_ReadUint16();
    if ( primitiveCount < 0 || primitiveCount > 4096 ) {
        Com_Error( ERR_DROP, "%s: invalid primitive count (%d)", __func__, primitiveCount );
    }

    // Render only when caller allows rendering and local opt-in cvar is enabled.
    const bool renderPrimitives = shouldRender && clg_nav3_debug_draw && ( clg_nav3_debug_draw->integer != 0 );

    for ( int32_t i = 0; i < primitiveCount; i++ ) {
        // Parse primitive header shared by every primitive payload.
        const sg_svc_debug_draw_primitive_type_t primitiveType = static_cast<sg_svc_debug_draw_primitive_type_t>( clgi.MSG_ReadUint8() );
        const uint32_t color = static_cast<uint32_t>( clgi.MSG_ReadInt32() );
        const uint16_t styleFlags = static_cast<uint16_t>( clgi.MSG_ReadUint16() );
        const float thicknessPx = clgi.MSG_ReadFloat();
        const float outlineThicknessPx = clgi.MSG_ReadFloat();

        (void)styleFlags;
        (void)thicknessPx;
        (void)outlineThicknessPx;

        switch ( primitiveType ) {
        case sg_svc_debug_draw_primitive_type_t::Line: {
            vec3_t start = {};
            vec3_t end = {};
            CLG_DebugDraw_ReadVec3( start );
            CLG_DebugDraw_ReadVec3( end );

            if ( renderPrimitives ) {
                clgi.R_DrawDebugLine( start, end, color );
            }
            break;
        }
        case sg_svc_debug_draw_primitive_type_t::Aabb: {
            vec3_t mins = {};
            vec3_t maxs = {};
            CLG_DebugDraw_ReadVec3( mins );
            CLG_DebugDraw_ReadVec3( maxs );

            if ( renderPrimitives ) {
                clgi.R_DrawDebugBox( mins, maxs, color );
            }
            break;
        }
        case sg_svc_debug_draw_primitive_type_t::Obb: {
            vec3_t center = {};
            vec3_t extents = {};
            vec3_t axisX = {};
            vec3_t axisY = {};
            vec3_t axisZ = {};
            CLG_DebugDraw_ReadVec3( center );
            CLG_DebugDraw_ReadVec3( extents );
            CLG_DebugDraw_ReadVec3( axisX );
            CLG_DebugDraw_ReadVec3( axisY );
            CLG_DebugDraw_ReadVec3( axisZ );

            if ( renderPrimitives ) {
                CLG_DebugDraw_DrawObb( center, extents, axisX, axisY, axisZ, color );
            }
            break;
        }
        case sg_svc_debug_draw_primitive_type_t::Sphere: {
            vec3_t center = {};
            CLG_DebugDraw_ReadVec3( center );
            const float radius = clgi.MSG_ReadFloat();

            if ( renderPrimitives ) {
                clgi.R_DrawDebugSphere( center, radius, color );
            }
            break;
        }
        case sg_svc_debug_draw_primitive_type_t::Arrow: {
            vec3_t start = {};
            vec3_t end = {};
            CLG_DebugDraw_ReadVec3( start );
            CLG_DebugDraw_ReadVec3( end );
            const float headLength = clgi.MSG_ReadFloat();

            if ( renderPrimitives ) {
                clgi.R_DrawDebugArrow( start, end, headLength, color );
            }
            break;
        }
        case sg_svc_debug_draw_primitive_type_t::Text: {
            vec3_t origin = {};
            char label[ SG_SVC_DEBUG_DRAW_MAX_LABEL_CHARS ] = {};
            CLG_DebugDraw_ReadVec3( origin );
            clgi.MSG_ReadString( label, sizeof( label ) );

            if ( renderPrimitives ) {
                // Until world-space text rendering is exposed, render a small label anchor marker.
                vec3_t markerTop = { origin[ 0 ], origin[ 1 ], origin[ 2 ] + 12.0f };
                clgi.R_DrawDebugArrow( origin, markerTop, 4.0f, color );
            }
            break;
        }
        case sg_svc_debug_draw_primitive_type_t::Count:
        default:
            Com_Error( ERR_DROP, "%s: invalid primitive type (%d)", __func__, static_cast<int32_t>( primitiveType ) );
            break;
        }
    }
}



/***`
*
*
*   Damage Indicator:
*
*
***/
/**
*   @brief  
**/
typedef struct {
    uint8_t         damage : 5;
    bool            health : 1;
    bool            armor : 1;
} clg_svc_damage_t;
/**
*   @brief 
**/
static void CLG_ParseDamage( void ) {
    // First get count.
    uint8_t count = clgi.MSG_ReadUint8();
    // Process each indicator.
    for ( uint8_t i = 0; i < count; i++ ) {
        union {
            uint8_t         encoded;
            clg_svc_damage_t decoded;
        } data = { static_cast<uint8_t>( clgi.MSG_ReadUint8() ) };

        // Direction that comes in is absolute in world coordinates
        vec3_t dir = {};
        clgi.MSG_ReadDir8( dir );

        // Blend color.
        vec3_t colorBlend = { 0.f, 0.f, 0.f };
        if ( data.decoded.health ) {
            colorBlend[ 0 ] += 1.0f;
        } else if ( data.decoded.armor ) {
            colorBlend[ 0 ] += 1.0f;
            colorBlend[ 1 ] += 1.0f;
            colorBlend[ 2 ] += 1.0f;
        }

        VectorNormalize( colorBlend );

        //clgi.Print( PRINT_DEVELOPER, "%s: svc_damage received(damage=%i, color=[%f,%f,%f], dir=[%f,%f,%f])\n",
        //    __func__, data.decoded.damage, colorBlend[ 0 ], colorBlend[ 1 ], colorBlend[ 2 ], dir[ 0 ], dir[ 1 ], dir[ 2 ] );

        CLG_HUD_AddToDamageDisplay( data.decoded.damage, colorBlend, dir );
    }
}



///***
//*
//*
//*   MuzzleFlash:
//*
//*
//**/
/**
*   @brief  Parse the svc_muzzleflash packet(s).
**/
void CLG_ParseMuzzleFlashPacket( const int32_t mask ) {
    const int32_t entity = clgi.MSG_ReadInt16();
    if ( entity < 1 || entity >= MAX_EDICTS ) {
        Com_Error( ERR_DROP, "%s: bad entity", __func__ );
    }

    const int32_t weapon = clgi.MSG_ReadUint8();
    level.parsedMessage.events.muzzleFlash.silenced = weapon & mask;
    level.parsedMessage.events.muzzleFlash.weapon = weapon & ~mask;
    level.parsedMessage.events.muzzleFlash.entity = entity;
}



///***
//*
//*
//*   Printing:
//*
//*
//**/
/**
*   @brief  Parse the svc_print packet and print whichever it has to screen.
**/
static void CLG_ParsePrint( void ) {
    int level;
    char s[ MAX_STRING_CHARS ];
    const char *fmt;

    level = clgi.MSG_ReadUint8();
    clgi.MSG_ReadString( s, sizeof( s ) );

    clgi.ShowNet( 2, "    %i \"%s\"\n", level, s );

    if ( level != PRINT_CHAT ) {
        Com_Printf( "%s", s );
        if ( !clgi.IsDemoPlayback() ) {
            COM_strclr( s );
            clgi.Cmd_ExecTrigger( s );
        }
        return;
    }

    if ( clgi.CheckForIgnore( s ) ) {
        return;
    }

    #if USE_AUTOREPLY
    if ( !clgi.IsDemoPlayback() ) {
        clgi.CheckForVersion( s );
    }
    #endif

    clgi.CheckForIP( s );

    // Disable 'transparant' chat hud notifications.
    if ( !clg_chat_notify->integer ) {
        clgi.Con_SkipNotify( true );
    }

    // filter text
    if ( clg_chat_filter->integer ) {
        COM_strclr( s );
        fmt = "%s\n";
    } else {
        fmt = "%s";
    }

    clgi.Print( PRINT_TALK, fmt, s );
    //Com_LPrintf( PRINT_TALK, fmt, s );

    clgi.Con_SkipNotify( false );

    CLG_HUD_AddChatLine( s );

    // play sound
    clgi.S_StartLocalSoundOnce( "hud/chat01.wav" );
}

/**
*   @brief  Parse the Center Print message.
**/
static void CLG_ParseCenterPrint( void ) {
    char s[ MAX_STRING_CHARS ];

    clgi.MSG_ReadString( s, sizeof( s ) );
    clgi.ShowNet( 2, "    \"%s\"\n", s );
    SCR_CenterPrint( s );

    if ( !clgi.IsDemoPlayback() ) {
        COM_strclr( s );
        clgi.Cmd_ExecTrigger( s );
    }
}



///***
//*
//*
//*   Parsing of Messages:
//*
//*
//**/
/**
*	@brief	Called by the client when it receives a configstring update, this
*			allows us to interscept it and respond to it. If not interscepted the
*			control is passed back to the client again.
*
*	@return	True if we interscepted one succesfully, false otherwise.
**/
const qboolean PF_UpdateConfigString( const int32_t index ) {
    // Get config string.
    const char *s = (const char*)clgi.GetConfigString( index );//clgi.client->configstrings[ index ];

    if ( index == CS_AIRACCEL ) {
        //clgi.client->pmp.pm_air_accelerate = /*clgi.client->pmp.qwmode || */atoi( s );
        return true;
    }

    // TODO: Implement.
    //if ( index == CS_MODELS + 1 ) {
    //	if ( !Com_ParseMapName( cl.mapname, s, sizeof( cl.mapname ) ) )
    //		Com_Error( ERR_DROP, "%s: bad world model: %s", __func__, s );
    //	return true;
    //}

    if ( index >= CS_LIGHTS && index < CS_LIGHTS + MAX_LIGHTSTYLES ) {
        CLG_SetLightStyle( index - CS_LIGHTS, s );
        return true;
    }

    // Only do the following when not fully precached yet:
    if ( clgi.GetConnectionState() < ca_precached ) {
        // TODO: Implement.
        //if ( index >= CS_PLAYERSKINS && index < CS_PLAYERSKINS + MAX_CLIENTS ) {
        //	CLG_LoadClientinfo( &cl.clientinfo[ index - CS_PLAYERSKINS ], s );
        //	return;
        //}
        return false;
    }

    // Client proceeds to process said configstring index.
    return false;
}

/**
*	@brief	Called by the client BEFORE all server messages have been parsed.
**/
void PF_StartServerMessage( const qboolean isDemoPlayback ) {

}

/**
*	@brief	Called by the client AFTER all server messages have been parsed.
**/
void PF_EndServerMessage( const qboolean isDemoPlayback ) {

}

/**
*	@brief	Called by the client when it does not recognize the server message itself,
*			so it gives the client game a chance to handle and respond to it.
*	@return	True if the message was handled properly. False otherwise.
**/
const qboolean PF_ParseServerMessage( const int32_t serverMessage ) {
	switch ( serverMessage ) {
    case svc_print:
        CLG_ParsePrint();
        return true;
    break;
	case svc_centerprint:
        CLG_ParseCenterPrint();
		return true;
    break;
    case svc_debug_draw:
        CLG_ParseDebugDraw( true );
        return true;
    break;
    case svc_damage:
        CLG_ParseDamage();
        return true;
    break;
	case svc_inventory:
		CLG_ParseInventory();
		return true;
	break;
	case svc_game_ui_open:
		CLG_ParseGameUI_OpenMenu();
		return true;
	case svc_game_ui_close:
		CLG_ParseGameUI_CloseMenu();
		return true;
	case svc_layout:
		CLG_ParseLayout();
		return true;
	break;
    case svc_muzzleflash:
        CLG_ParseMuzzleFlashPacket( 0/*MZ_SILENCED*/ );
        CLG_MuzzleFlash();
        return true;
    break;
    case svc_temp_entity:
        CLG_ParseTEntPacket();
        CLG_TemporaryEntities_Parse();
        return true;
    break;
	default:
		return false;
	}

	return false;
}

/**
*	@brief	A variant of ParseServerMessage that skips over non-important action messages,
*			used for seeking in demos.
*	@return	True if the message was handled properly. False otherwise.
**/
const qboolean PF_SeekDemoMessage( const int32_t serverMessage ) {
    switch ( serverMessage ) {
    case svc_print:
        CLG_ParsePrint();
        return true;
    break;
	case svc_centerprint:
        CLG_ParseCenterPrint();
		return true;
    break;
    case svc_debug_draw:
        CLG_ParseDebugDraw( false );
        return true;
    break;
    case svc_damage:
        CLG_ParseDamage();
        return true;
    break;
    case svc_inventory:
        CLG_ParseInventory();
        return true;
    break;
    case svc_layout:
        CLG_ParseLayout();
        return true;
    break;
    case svc_temp_entity:
        CLG_ParseTEntPacket();
        return true;
    break;
    case svc_muzzleflash:
        CLG_ParseMuzzleFlashPacket( 0 );
        return true;
    break;
    default:
        return false;
    }

    return false;
}


/**
*	@brief	Parses a clientinfo configstring:
*           Breaks up playerskin into name (optional), model and skin components.
*           If model or skin are found to be invalid, replaces them with sane defaults.
**/
void PF_ParsePlayerSkin( char *name, char *model, char *skin, const char *s ) {
    size_t len;
    const char *t; // WID: C++20: Was without const

    // configstring parsing guarantees that playerskins can never
    // overflow, but still check the length to be entirely fool-proof
    len = strlen( s );
    if ( len >= MAX_QPATH ) {
        Com_Error( ERR_DROP, "%s: oversize playerskin", __func__ );
    }

    // isolate the player's name
    t = strchr( s, '\\' );
    if ( t ) {
        len = t - s;
        strcpy( model, t + 1 );
    } else {
        len = 0;
        strcpy( model, s );
    }

    // Copy the player's name
    if ( name ) {
        memcpy( name, s, len );
        name[ len ] = 0;
    }

    // Isolate the model name.
    t = strchr( model, '/' );
    if ( !t ) {
        t = strchr( model, '\\' );
    }
    if ( !t ) {
        goto default_model;
    }
    *(char *)t = 0; // WID: C++20: NOTE/WARNING: This might be really evil.

    // isolate the skin name
    strcpy( skin, t + 1 );

    // fix empty model to playerdummy
    if ( t == model ) {
        strcpy( model, "playerdummy" );
    }

    // apply restrictions on skins
    if ( clg_noskins->integer == 2 || !COM_IsPath( skin ) ) {
        goto default_skin;
    }

    if ( clg_noskins->integer || !COM_IsPath( model ) ) {
        goto default_model;
    }

    return;

default_skin:
        strcpy( skin, "playerdummy" );
default_model:
        strcpy( model, "playerdummy" );
        strcpy( skin, "playerdummy" );
}