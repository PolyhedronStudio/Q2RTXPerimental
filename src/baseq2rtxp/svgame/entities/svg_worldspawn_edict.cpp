

/********************************************************************
*
*
*	SVGame: Edicts Functionalities:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_save.h"
#include "svgame/entities/svg_worldspawn_edict.h"



/**
*
*
*
*   Save Descriptor Field Setup: svg_worldspawn_edict_t
*
*
*
**/
/**
*   @brief  Save descriptor array definition for all the members of svg_worldspawn_edict_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_worldspawn_edict_t )
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_worldspawn_edict_t, sky, SD_FIELD_TYPE_GAME_QSTRING ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_worldspawn_edict_t, skyrotate, SD_FIELD_TYPE_FLOAT ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_worldspawn_edict_t, skyautorotate, SD_FIELD_TYPE_INT32 ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_worldspawn_edict_t, skyaxis, SD_FIELD_TYPE_VECTOR3 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_worldspawn_edict_t, gravity, SD_FIELD_TYPE_GAME_QSTRING ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_worldspawn_edict_t, nextmap, SD_FIELD_TYPE_GAME_QSTRING ),
	SAVE_DESCRIPTOR_DEFINE_FIELD( svg_worldspawn_edict_t, musictrack, SD_FIELD_TYPE_GAME_QSTRING ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_worldspawn_edict_t, svg_base_edict_t );




/**
*
*   Core:
*
**/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_worldspawn_edict_t::Reset( const bool retainDictionary ) {
    // Call upon the base class.
    Super::Reset( retainDictionary );
    // Print
    gi.dprintf( "%s: Resetting classname: %s\n", __func__, classname );
}


/**
*   @brief  Save the entity into a file using game_write_context.
*   @note   Make sure to call the base parent class' Save() function.
**/
void svg_worldspawn_edict_t::Save( struct game_write_context_t *ctx ) {
    // Call upon the base class.
    //sv_shared_edict_t<svg_base_edict_t, svg_client_t>::Save( ctx );
    Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_worldspawn_edict_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_worldspawn_edict_t::Restore( struct game_read_context_t *ctx ) {
    // Restore parent class fields.
    Super::Restore( ctx );
    // Restore all the members of this entity type.
    ctx->read_fields( svg_worldspawn_edict_t::saveDescriptorFields, this );
}


/**
*   @brief  Called for each cm_entity_t key/value pair for this entity.
*           If not handled, or unable to be handled by the derived entity type, it will return
*           set errorStr and return false. True otherwise.
**/
const bool svg_worldspawn_edict_t::KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) {
    // Ease of use.
    const std::string keyStr = keyValuePair->key;

    // Match: sky
    if ( keyStr == "sky" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        sky = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    }
    // Match: skyrotate
    else if ( keyStr == "skyrotate" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
        skyrotate = keyValuePair->value;
        return true;
    }
    // Match: skyautorotate
    else if ( keyStr == "skyautorotate" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
        skyautorotate = keyValuePair->integer;
        return true;
    }
    // Match: nextmap
    else if ( keyStr == "nextmap" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        nextmap = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    }
    // Match: musictrack
    else if ( keyStr == "musictrack" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        musictrack = svg_level_qstring_t::from_char_str( keyValuePair->string );
        return true;
    }
    // Match: gravity
    else if ( keyStr == "gravity" && keyValuePair->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
        gravity_str = svg_level_qstring_t::from_char_str( keyValuePair->string );
		gravity = std::strtof( gravity_str.ptr, nullptr );
        return true;
    }

    // Give Base class a shot.
	return Super::KeyValue( keyValuePair, errorStr );
}



/**
*
*   Worldspawn
*
**/
/**
*
*   TestDummy Callback Member Functions:
*
**/
/**
*   @brief  Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_worldspawn_edict_t, onSpawn )( svg_worldspawn_edict_t *self ) -> void {
    // Call upon the base class.
    Super::onSpawn( self );

    // Setup the world spawn entity.
    self->movetype = MOVETYPE_PUSH;
    self->solid = SOLID_BSP;
    self->inuse = true;                           // Just to make damn sure it is always properly set.
    self->s.modelindex = MODELINDEX_WORLDSPAWN;   // World model is always index 1

    //---------------

    // Reserve some spots for dead player bodies for coop / deathmatch
    SVG_Entities_InitBodyQue();

    // set configstrings for items
    SVG_SetItemNames();

    if ( self->nextmap ) {
        Q_strlcpy( level.nextmap, (const char *)self->nextmap, sizeof( level.nextmap ) );
    }

    // make some data visible to the server
    if ( self->message && self->message[ 0 ] ) {
        gi.configstring( CS_NAME, self->message );
        Q_strlcpy( level.level_name, self->message, sizeof( level.level_name ) );
    } else {
        Q_strlcpy( level.level_name, level.mapname, sizeof( level.level_name ) );
    }

    if ( self->sky && self->sky[ 0 ] ) {
        gi.configstring( CS_SKY, (const char *)self->sky );
    } else {
        gi.configstring( CS_SKY, "unit1_" );
    }
    gi.configstring( CS_SKYROTATE, va( "%f %d", self->skyrotate, self->skyautorotate ) );

    gi.configstring( CS_SKYAXIS, va( "%f %f %f",
        self->skyaxis[ 0 ], self->skyaxis[ 1 ], self->skyaxis[ 2 ] ) );

    gi.configstring( CS_MAXCLIENTS, va( "%i", (int)( maxclients->value ) ) );

    // Set air acceleration properly.
    if ( COM_IsUint( sv_airaccelerate->string ) || COM_IsFloat( sv_airaccelerate->string ) ) {
        gi.configstring( CS_AIRACCEL, sv_airaccelerate->string );
    } else {
        gi.configstring( CS_AIRACCEL, "0" );
    }

    // status bar program
    if ( deathmatch->value ) {
        gi.configstring( CS_STATUSBAR, "" );
    } else {
        gi.configstring( CS_STATUSBAR, "" );
    }

    // Gravity.
    if ( !self->gravity_str.count || !self->gravity ) {
        gi.cvar_set( "sv_gravity", "800" );
    } else {
        gi.cvar_set( "sv_gravity", (const char *)self->gravity_str );
    }

    // Certain precaches.
    gi.modelindex( "players/playerdummy/tris.iqm" );

    // help icon for statusbar
    //gi.imageindex("i_help");
    //level.pic_health = gi.imageindex("i_health");
    //gi.imageindex("help");
    //gi.imageindex("field_3");

    SVG_PrecacheItem( SVG_Item_FindByPickupName( "Fists" ) );
    SVG_PrecacheItem( SVG_Item_FindByPickupName( "Pistol" ) );

    // HUD Chat.
    gi.soundindex( "hud/chat01.wav" );

    // Body Gib.
    gi.soundindex( "world/gib01.wav" );
    gi.soundindex( "world/gib_drop01.wav" );

    // Item Respawn.
    gi.soundindex( "items/respawn01.wav" );

    // Login/Logout/Teleport.
    gi.soundindex( "world/mz_login.wav" );
    gi.soundindex( "world/mz_logout.wav" );
    gi.soundindex( "world/mz_respawn.wav" );
    gi.soundindex( "world/teleport01.wav" );

    // sexed sounds
    //gi.soundindex("*death1.wav");
    //gi.soundindex("*death2.wav");
    //gi.soundindex("*death3.wav");
    //gi.soundindex("*death4.wav");
    //gi.soundindex("*pain25_1.wav");
    //gi.soundindex("*pain25_2.wav");
    //gi.soundindex("*pain50_1.wav");
    //gi.soundindex("*pain50_2.wav");
    //gi.soundindex("*pain75_1.wav");
    //gi.soundindex("*pain75_2.wav");
    //gi.soundindex("*pain100_1.wav");
    //gi.soundindex("*pain100_2.wav");

    // Deaths:
    gi.soundindex( "player/death01.wav" );
    gi.soundindex( "player/death02.wav" );
    gi.soundindex( "player/death03.wav" );
    gi.soundindex( "player/death04.wav" );
    // Pains:
    gi.soundindex( "player/pain25_01.wav" );
    gi.soundindex( "player/pain50_01.wav" );
    gi.soundindex( "player/pain75_01.wav" );
    gi.soundindex( "player/pain100_01.wav" );
    // WID: All of these are now just burn01 and burn02 since the original sounds contained silly screams and all that.
    gi.soundindex( "player/burn01.wav" );  // standing in lava / slime
    //gi.soundindex( "player/lava_in.wav" );
    //gi.soundindex( "player/burn1.wav" );
    //gi.soundindex( "player/burn2.wav" );
    //gi.soundindex( "player/lava1.wav" );
    //gi.soundindex( "player/lava2.wav" );
    gi.soundindex( "player/burn01.wav" );
    gi.soundindex( "player/burn02.wav" );
    // Kinematics:
    gi.soundindex( "player/jump01.wav" );   // Player jump.
    gi.soundindex( "player/jump02.wav" ); // Player jump.
    gi.soundindex( "player/fall01.wav" );   // Player fall.
    gi.soundindex( "player/fall02.wav" );   // Player heavy fall.
    gi.soundindex( "player/land01.wav" );   // Player jump landing sound.
    // Water:
    gi.soundindex( "player/drown01.wav" );  // Drowning last breaths.
    gi.soundindex( "player/gasp01.wav" );   // Gasping for air.
    gi.soundindex( "player/gasp02.wav" );   // Head breaking surface, not gasping.
    gi.soundindex( "player/gurp01.wav" );   // Drowning damage 01.
    gi.soundindex( "player/gurp02.wav" );   // Drowning damage 02.
    gi.soundindex( "player/water_body_out01.wav" );     // Feet hitting water.
    gi.soundindex( "player/water_feet_in01.wav" );      // Feet hitting water.
    gi.soundindex( "player/water_feet_out01.wav" );     // Feet leaving water.
    gi.soundindex( "player/water_head_under01.wav" );   // Head going underwater.
    gi.soundindex( "player/water_splash_in01.wav" );    // Head going underwater.
    gi.soundindex( "player/water_splash_in02.wav" );    // Head going underwater.
    gi.soundindex( "world/water_land_splash01.wav" );   // Landing splash 01.
    gi.soundindex( "world/water_land_splash02.wav" );   // Landing splash 02.
    // Misc/World(Kinematics):
    gi.soundindex( "world/land01.wav" );    // Ground landing thud.

    //gi.soundindex("*pain25_1.wav");
    //gi.soundindex("*pain25_2.wav");
    //gi.soundindex("*pain50_1.wav");
    //gi.soundindex("*pain50_2.wav");
    //gi.soundindex("*pain75_1.wav");
    //gi.soundindex("*pain75_2.wav");
    //gi.soundindex("*pain100_1.wav");
    //gi.soundindex("*pain100_2.wav");

    // sexed models
    // THIS ORDER MUST MATCH THE DEFINES IN g_local.h
    // you can add more, max 15
    //gi.modelindex( "#w_fists.iqm" ); // #w_fists.iqm
    //gi.modelindex( "#w_pistol.iqm" );
    //gi.modelindex( "#w_shotgun.md2" );
    //gi.modelindex( "#w_sshotgun.md2" );
    //gi.modelindex( "#w_machinegun.md2" );
    //gi.modelindex( "#w_chaingun.md2" );
    //gi.modelindex( "#a_grenades.md2" );
    //gi.modelindex( "#w_glauncher.md2" );
    //gi.modelindex( "#w_rlauncher.md2" );
    //gi.modelindex( "#w_hyperblaster.md2" );
    //gi.modelindex( "#w_railgun.md2" );
    //gi.modelindex( "#w_bfg.md2" );

    //-------------------

    gi.modelindex( "models/objects/gibs/sm_meat/tris.md2" );
    gi.modelindex( "models/objects/gibs/arm/tris.md2" );
    gi.modelindex( "models/objects/gibs/bone/tris.md2" );
    gi.modelindex( "models/objects/gibs/bone2/tris.md2" );
    gi.modelindex( "models/objects/gibs/chest/tris.md2" );
    gi.modelindex( "models/objects/gibs/skull/tris.md2" );
    gi.modelindex( "models/objects/gibs/head2/tris.md2" );

    //
    // Setup light animation tables. 'a' is total darkness, 'z' is doublebright.
    //

        // 0 normal
    gi.configstring( CS_LIGHTS + 0, "m" );

    // 1 FLICKER (first variety)
    gi.configstring( CS_LIGHTS + 1, "mmnmmommommnonmmonqnmmo" );

    // 2 SLOW STRONG PULSE
    gi.configstring( CS_LIGHTS + 2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba" );

    // 3 CANDLE (first variety)
    gi.configstring( CS_LIGHTS + 3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg" );

    // 4 FAST STROBE
    gi.configstring( CS_LIGHTS + 4, "mamamamamama" );

    // 5 GENTLE PULSE 1
    gi.configstring( CS_LIGHTS + 5, "jklmnopqrstuvwxyzyxwvutsrqponmlkj" );

    // 6 FLICKER (second variety)
    gi.configstring( CS_LIGHTS + 6, "nmonqnmomnmomomno" );

    // 7 CANDLE (second variety)
    gi.configstring( CS_LIGHTS + 7, "mmmaaaabcdefgmmmmaaaammmaamm" );

    // 8 CANDLE (third variety)
    gi.configstring( CS_LIGHTS + 8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa" );

    // 9 SLOW STROBE (fourth variety)
    gi.configstring( CS_LIGHTS + 9, "aaaaaaaazzzzzzzz" );

    // 10 FLUORESCENT FLICKER
    gi.configstring( CS_LIGHTS + 10, "mmamammmmammamamaaamammma" );

    // 11 SLOW PULSE NOT FADE TO BLACK
    gi.configstring( CS_LIGHTS + 11, "abcdefghijklmnopqrrqponmlkjihgfedcba" );

    // styles 32-62 are assigned by the light program for switchable lights

    // 63 testing
    gi.configstring( CS_LIGHTS + 63, "a" );
}