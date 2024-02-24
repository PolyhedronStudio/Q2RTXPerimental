/********************************************************************
*
*
*	ClientGame: Server Message Parsing.
*
*
********************************************************************/
#include "clg_local.h"

/**
*	@brief	Parses the layout string for server cmd: svc_inventory.
**/
static void CLG_ParseLayout( void ) {
	clgi.MSG_ReadString( clgi.client->layout, sizeof( clgi.client->layout ) );
	clgi.ShowNet( 2, "    \"%s\"\n", clgi.client->layout );
}

/**
*	@brief	Parses the player inventory for server cmd: svc_inventory.
**/
static void CLG_ParseInventory( void ) {
	for ( int32_t i = 0; i < MAX_ITEMS; i++ ) {
		clgi.client->inventory[ i ] = clgi.MSG_ReadIntBase128();
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
    case TE_BLOOD:
    case TE_GUNSHOT:
    case TE_SPARKS:
    case TE_BULLET_SPARKS:
    case TE_SCREEN_SPARKS:
    case TE_SHIELD_SPARKS:
    case TE_SHOTGUN:
    case TE_BLASTER:
    case TE_GREENBLOOD:
    case TE_BLASTER2:
    case TE_FLECHETTE:
    case TE_HEATBEAM_SPARKS:
    case TE_HEATBEAM_STEAM:
    case TE_MOREBLOOD:
    case TE_ELECTRIC_SPARKS:
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, false );
        clgi.MSG_ReadDir8( level.parsedMessage.events.tempEntity.dir );
        break;

    case TE_SPLASH:
    case TE_LASER_SPARKS:
    case TE_WELDING_SPARKS:
    case TE_TUNNEL_SPARKS:
        level.parsedMessage.events.tempEntity.count = clgi.MSG_ReadUint8();
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, false );
        clgi.MSG_ReadDir8( level.parsedMessage.events.tempEntity.dir );
        level.parsedMessage.events.tempEntity.color = clgi.MSG_ReadUint8();
        break;

    case TE_BLUEHYPERBLASTER:
    case TE_RAILTRAIL:
    case TE_BUBBLETRAIL:
    case TE_DEBUGTRAIL:
    case TE_BUBBLETRAIL2:
    case TE_BFG_LASER:
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, false );
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos2, false );
        break;

    case TE_GRENADE_EXPLOSION:
    case TE_GRENADE_EXPLOSION_WATER:
    case TE_EXPLOSION2:
    case TE_PLASMA_EXPLOSION:
    case TE_ROCKET_EXPLOSION:
    case TE_ROCKET_EXPLOSION_WATER:
    case TE_EXPLOSION1:
    case TE_EXPLOSION1_NP:
    case TE_EXPLOSION1_BIG:
    case TE_BFG_EXPLOSION:
    case TE_BFG_BIGEXPLOSION:
    case TE_BOSSTPORT:
    case TE_PLAIN_EXPLOSION:
    case TE_CHAINFIST_SMOKE:
    case TE_TRACKER_EXPLOSION:
    case TE_TELEPORT_EFFECT:
    case TE_DBALL_GOAL:
    case TE_WIDOWSPLASH:
    case TE_NUKEBLAST:
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, false );
        break;

    case TE_PARASITE_ATTACK:
    case TE_MEDIC_CABLE_ATTACK:
    case TE_HEATBEAM:
    case TE_MONSTER_HEATBEAM:
        level.parsedMessage.events.tempEntity.entity1 = clgi.MSG_ReadInt16();
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, false );
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos2, false );
        break;

    case TE_GRAPPLE_CABLE:
        level.parsedMessage.events.tempEntity.entity1 = clgi.MSG_ReadInt16();
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, false );
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos2, false );
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.offset, false );
        break;

    case TE_LIGHTNING:
        level.parsedMessage.events.tempEntity.entity1 = clgi.MSG_ReadInt16();
        level.parsedMessage.events.tempEntity.entity2 = clgi.MSG_ReadInt16();
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, false );
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos2, false );
        break;

    case TE_FLASHLIGHT:
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, false );
        level.parsedMessage.events.tempEntity.entity1 = clgi.MSG_ReadInt16();
        break;

    case TE_FORCEWALL:
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, false );
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos2, false );
        level.parsedMessage.events.tempEntity.color = clgi.MSG_ReadUint8();
        break;

    case TE_STEAM:
        level.parsedMessage.events.tempEntity.entity1 = clgi.MSG_ReadInt16();
        level.parsedMessage.events.tempEntity.count = clgi.MSG_ReadUint8();
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, false );
        clgi.MSG_ReadDir8( level.parsedMessage.events.tempEntity.dir );
        level.parsedMessage.events.tempEntity.color = clgi.MSG_ReadUint8();
        level.parsedMessage.events.tempEntity.entity2 = clgi.MSG_ReadInt16();
        if ( level.parsedMessage.events.tempEntity.entity1 != -1 ) {
            level.parsedMessage.events.tempEntity.time = clgi.MSG_ReadInt32();
        }
        break;

    case TE_WIDOWBEAMOUT:
        level.parsedMessage.events.tempEntity.entity1 = clgi.MSG_ReadInt16();
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, false );
        break;

    case TE_FLARE:
        level.parsedMessage.events.tempEntity.entity1 = clgi.MSG_ReadInt16();
        level.parsedMessage.events.tempEntity.count = clgi.MSG_ReadUint8();
        clgi.MSG_ReadPos( level.parsedMessage.events.tempEntity.pos1, false );
        clgi.MSG_ReadDir8( level.parsedMessage.events.tempEntity.dir );
        break;

    default:
        Com_Error( ERR_DROP, "%s: bad type", __func__ );
    }
}

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
    if ( !cl_chat_notify->integer ) {
        clgi.Con_SkipNotify( true );
    }

    // filter text
    if ( cl_chat_filter->integer ) {
        COM_strclr( s );
        fmt = "%s\n";
    } else {
        fmt = "%s";
    }

    Com_LPrintf( PRINT_TALK, fmt, s );

    clgi.Con_SkipNotify( false );

    SCR_AddToChatHUD( s );

    // play sound
    if ( cl_chat_sound->integer > 1 ) {
        clgi.S_StartLocalSoundOnce( "misc/talk1.wav" );
    } else if ( cl_chat_sound->integer > 0 ) {
        clgi.S_StartLocalSoundOnce( "misc/talk.wav" );
    }
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
	case svc_inventory:
		CLG_ParseInventory();
		return true;
	break;
	case svc_layout:
		CLG_ParseLayout();
		return true;
	break;
    case svc_muzzleflash:
        CLG_ParseMuzzleFlashPacket( MZ_SILENCED );
        CLG_MuzzleFlash();
        return true;
    break;
    case svc_muzzleflash2:
        CLG_ParseMuzzleFlashPacket( 0 );
        CLG_MuzzleFlash2();
        return true;
    break;
    case svc_temp_entity:
        CLG_ParseTEntPacket();
        CLG_ParseTEnt();
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
    case svc_muzzleflash2:
        CLG_ParseMuzzleFlashPacket( 0 );
        return true;
    break;
    default:
        return false;
    }

    return false;
}


/**
*	@brief	Parsess entity events.
**/
void PF_ParseEntityEvent( const int32_t entityNumber ) {
    if ( entityNumber < 0 || entityNumber >= MAX_ENTITIES ) {
        clgi.Print( PRINT_WARNING, "PF_ParseEntityEvent: invalid range(%d) variable 'entityNumber'\n", entityNumber );
        return;
    }

    centity_t *cent = &clg_entities[ entityNumber ];//centity_t *cent = &cl_entities[number];

    // EF_TELEPORTER acts like an event, but is not cleared each frame
    if ( ( cent->current.effects & EF_TELEPORTER ) ) {
        CLG_TeleporterParticles( cent->current.origin );
    }

    switch ( cent->current.event ) {
    case EV_ITEM_RESPAWN:
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "items/respawn1.wav" ), 1, ATTN_IDLE, 0 );
        CLG_ItemRespawnParticles( cent->current.origin );
        break;
    case EV_PLAYER_TELEPORT:
        clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "misc/tele1.wav" ), 1, ATTN_IDLE, 0 );
        CLG_TeleportParticles( cent->current.origin );
        break;
    case EV_FOOTSTEP:
        if ( cl_footsteps->integer )
            clgi.S_StartSound( NULL, entityNumber, CHAN_BODY, precache.cl_sfx_footsteps[ Q_rand() & 3 ], 1, ATTN_NORM, 0 );
        break;
    case EV_FALLSHORT:
        clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/land1.wav" ), 1, ATTN_NORM, 0 );
        break;
    case EV_FALL:
        clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "*fall2.wav" ), 1, ATTN_NORM, 0 );
        break;
    case EV_FALLFAR:
        clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "*fall1.wav" ), 1, ATTN_NORM, 0 );
        break;
    }
}