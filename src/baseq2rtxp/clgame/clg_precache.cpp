/********************************************************************
*
*
*	ClientGame: Deal with optional client specific precaching.
*
*
********************************************************************/
#include "clg_local.h"

//! Stores qhandles to all precached client game media.
precached_media_t precache;

/**
*	@brief	Called right before loading all received configstring (server-) sounds.
**/
void PF_PrecacheClientSounds( void ) {
    char    name[ MAX_QPATH ];

    precache.cl_sfx_ric1 = clgi.S_RegisterSound( "world/ric1.wav" );
    precache.cl_sfx_ric2 = clgi.S_RegisterSound( "world/ric2.wav" );
    precache.cl_sfx_ric3 = clgi.S_RegisterSound( "world/ric3.wav" );
    precache.cl_sfx_lashit = clgi.S_RegisterSound( "weapons/lashit.wav" );
    precache.cl_sfx_flare = clgi.S_RegisterSound( "weapons/flare.wav" );
    precache.cl_sfx_spark5 = clgi.S_RegisterSound( "world/spark5.wav" );
    precache.cl_sfx_spark6 = clgi.S_RegisterSound( "world/spark6.wav" );
    precache.cl_sfx_spark7 = clgi.S_RegisterSound( "world/spark7.wav" );
    precache.cl_sfx_railg = clgi.S_RegisterSound( "weapons/railgf1a.wav" );
    precache.cl_sfx_rockexp = clgi.S_RegisterSound( "weapons/rocklx1a.wav" );
    precache.cl_sfx_grenexp = clgi.S_RegisterSound( "weapons/grenlx1a.wav" );
    precache.cl_sfx_watrexp = clgi.S_RegisterSound( "weapons/xpld_wat.wav" );

    clgi.S_RegisterSound( "player/land1.wav" );
    clgi.S_RegisterSound( "player/fall2.wav" );
    clgi.S_RegisterSound( "player/fall1.wav" );

    for ( int32_t i = 0; i < 4; i++ ) {
        Q_snprintf( name, sizeof( name ), "player/step%i.wav", i + 1 );
        precache.cl_sfx_footsteps[ i ] = clgi.S_RegisterSound( name );
    }

    precache.cl_sfx_lightning = clgi.S_RegisterSound( "weapons/tesla.wav" );
    precache.cl_sfx_disrexp = clgi.S_RegisterSound( "weapons/disrupthit.wav" );
}

/**
*	@brief	Called right before loading all received configstring (server-) models.
**/
void PF_PrecacheClientModels( void ) {
    precache.cl_mod_explode = clgi.R_RegisterModel( "models/objects/explode/tris.md2" );
    precache.cl_mod_smoke = clgi.R_RegisterModel( "models/objects/smoke/tris.md2" );
    precache.cl_mod_flash = clgi.R_RegisterModel( "models/objects/flash/tris.md2" );
    precache.cl_mod_parasite_segment = clgi.R_RegisterModel( "models/monsters/parasite/segment/tris.md2" );
    precache.cl_mod_grapple_cable = clgi.R_RegisterModel( "models/ctf/segment/tris.md2" );
    precache.cl_mod_explo4 = clgi.R_RegisterModel( "models/objects/r_explode/tris.md2" );
    precache.cl_mod_explosions[ 0 ] = clgi.R_RegisterModel( "sprites/rocket_0.sp2" );
    precache.cl_mod_explosions[ 1 ] = clgi.R_RegisterModel( "sprites/rocket_1.sp2" );
    precache.cl_mod_explosions[ 2 ] = clgi.R_RegisterModel( "sprites/rocket_5.sp2" );
    precache.cl_mod_explosions[ 3 ] = clgi.R_RegisterModel( "sprites/rocket_6.sp2" );
    precache.cl_mod_bfg_explo = clgi.R_RegisterModel( "sprites/s_bfg2.sp2" );
    precache.cl_mod_powerscreen = clgi.R_RegisterModel( "models/items/armor/effect/tris.md2" );
    precache.cl_mod_laser = clgi.R_RegisterModel( "models/objects/laser/tris.md2" );
    precache.cl_mod_dmspot = clgi.R_RegisterModel( "models/objects/dmspot/tris.md2" );

    precache.cl_mod_lightning = clgi.R_RegisterModel( "models/proj/lightning/tris.md2" );
    precache.cl_mod_heatbeam = clgi.R_RegisterModel( "models/proj/beam/tris.md2" );
    precache.cl_mod_explo4_big = clgi.R_RegisterModel( "models/objects/r_explode2/tris.md2" );

    // Enable 'vertical' display for explosion models.
    for ( int32_t i = 0; i < sizeof( precache.cl_mod_explosions ) / sizeof( *precache.cl_mod_explosions ); i++ ) {
        clgi.SetSpriteModelVerticality( precache.cl_mod_explosions[ i ] );
        //model_t *model = MOD_ForHandle( cl_mod_explosions[ i ] );

        //if ( model ) {
        //    model->sprite_vertical = true;
        //}
    }
}

/**
*   @brief  Called to precache/update precache of 'View'-models. (Mainly, weapons.)
**/
void PF_PrecacheViewModels( void ) {
    // Default to at least 1 'view/weapon'-model.
    precache.numViewModels = 1;
    strcpy( precache.viewModels[0], "weapon.md2");

    // Only default model when vwep is off.
    cvar_t *cl_vwep = clgi.CVar_Get( "cl_vwep", 0, 0 );
    if ( !cl_vwep->integer ) {
        return;
    }

    // Otherwise load all view models.
    for (int32_t i = 2; i < MAX_MODELS; i++) {
        // Acquire name.
        const char *name = clgi.client->configstrings[CS_MODELS + i];

        // Need a name, can't be empty.
        if (!name[0]) {
            break;
        }
        // View Models start with a # in their name.
        if ( name[0] != '#' ) {
            continue;
        }

        // Copy in viewmodels name.
        // Old Comment: "Special player weapon model."
        strcpy( precache.viewModels[ precache.numViewModels++ ], name + 1 );

        // Break when we've reached the maximum limit of view models.
        if ( precache.numViewModels == MAX_CLIENTVIEWMODELS ) {
            break;
        }
    }
}

/**
*	@brief	Called to precache client info specific media.
**/
void PF_PrecacheClientInfo( clientinfo_t *ci, const char *s ) {
    int         i;
    char        model_name[ MAX_QPATH ];
    char        skin_name[ MAX_QPATH ];
    char        model_filename[ MAX_QPATH ];
    char        skin_filename[ MAX_QPATH ];
    char        weapon_filename[ MAX_QPATH ];
    char        icon_filename[ MAX_QPATH ];

    PF_ParsePlayerSkin( ci->name, model_name, skin_name, s );

    // model file
    Q_concat( model_filename, sizeof( model_filename ),
        "players/", model_name, "/tris.md2" );
    ci->model = clgi.R_RegisterModel( model_filename );
    if ( !ci->model && Q_stricmp( model_name, "male" ) ) {
        strcpy( model_name, "male" );
        strcpy( model_filename, "players/male/tris.md2" );
        ci->model = clgi.R_RegisterModel( model_filename );
    }

    // skin file
    Q_concat( skin_filename, sizeof( skin_filename ),
        "players/", model_name, "/", skin_name, ".pcx" );
    ci->skin = clgi.R_RegisterSkin( skin_filename );

    // if we don't have the skin and the model was female,
    // see if athena skin exists
    if ( !ci->skin && !Q_stricmp( model_name, "female" ) ) {
        strcpy( skin_name, "athena" );
        strcpy( skin_filename, "players/female/athena.pcx" );
        ci->skin = clgi.R_RegisterSkin( skin_filename );
    }

    // if we don't have the skin and the model wasn't male,
    // see if the male has it (this is for CTF's skins)
    if ( !ci->skin && Q_stricmp( model_name, "male" ) ) {
        // change model to male
        strcpy( model_name, "male" );
        strcpy( model_filename, "players/male/tris.md2" );
        ci->model = clgi.R_RegisterModel( model_filename );

        // see if the skin exists for the male model
        Q_concat( skin_filename, sizeof( skin_filename ),
            "players/male/", skin_name, ".pcx" );
        ci->skin = clgi.R_RegisterSkin( skin_filename );
    }

    // if we still don't have a skin, it means that the male model
    // didn't have it, so default to grunt
    if ( !ci->skin ) {
        // see if the skin exists for the male model
        strcpy( skin_name, "grunt" );
        strcpy( skin_filename, "players/male/grunt.pcx" );
        ci->skin = clgi.R_RegisterSkin( skin_filename );
    }

    // weapon file
    for ( i = 0; i < precache.numViewModels; i++ ) {
        Q_concat( weapon_filename, sizeof( weapon_filename ),
            "players/", model_name, "/", precache.viewModels[ i ] );
        ci->weaponmodel[ i ] = clgi.R_RegisterModel( weapon_filename );
        if ( !ci->weaponmodel[ i ] && !Q_stricmp( model_name, "cyborg" ) ) {
            // try male
            Q_concat( weapon_filename, sizeof( weapon_filename ),
                "players/male/", precache.viewModels[ i ] );
            ci->weaponmodel[ i ] = clgi.R_RegisterModel( weapon_filename );
        }
    }

    // icon file
    Q_concat( icon_filename, sizeof( icon_filename ),
        "/players/", model_name, "/", skin_name, "_i.pcx" );
    ci->icon = clgi.R_RegisterPic2( icon_filename );

    strcpy( ci->model_name, model_name );
    strcpy( ci->skin_name, skin_name );

    // base info is expected/should be at least partially valid.
    if ( ci == &clgi.client->baseclientinfo ) {
        return;
    }

    // Otherwise make sure it loaded all data types to be valid.
    if ( !ci->skin || !ci->icon || !ci->model || !ci->weaponmodel[ 0 ] ) {
        // If we got here, it was invalid data.
        ci->skin = 0;
        ci->icon = 0;
        ci->model = 0;
        ci->weaponmodel[ 0 ] = 0;
        ci->model_name[ 0 ] = 0;
        ci->skin_name[ 0 ] = 0;
    }
}