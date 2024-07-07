/********************************************************************
*
*
*	ClientGame: Deal with optional client specific precaching.
*
*
********************************************************************/
#include "clg_local.h"
#include "clg_eax.h"
#include "clg_eax_effects.h"
#include "clg_effects.h"
#include "clg_parse.h"
#include "clg_precache.h"



/**
*
*
*   Precaching:
*
*
**/
//! Stores qhandles to all precached client game media.
precached_media_t precache;

/**
*   @brief  Precaches('to load') all local 'sound path' registered files.
**/
void CLG_PrecacheLocalSounds() {
    // Iterate over the local sound path 'config' strings.
    for ( int32_t i = 1; i < precache.num_local_sounds; i++ ) {
        // Ensure that its name is valid.
        const char *name = precache.model_paths[ i ];
        if ( !name || name[ 0 ] == 0 || name[ 0 ] == '\0' ) {
            precache.local_sounds[ i ] = 0;
            continue;
        }

        // Precache the actual sound, retreive the qhandle_t and store it.
        precache.local_sounds[ i ] = clgi.S_RegisterSound( name );
    }
}
/**
*	@brief	Called right before loading all received configstring (server-) sounds, allowing us to load in
*           the client's own local(-entity 'precached' sound paths) sounds first.
**/
void PF_PrecacheClientSounds( void ) {
    /**
    *   Precache all EAX Reverb Effects:
    **/
    CLG_EAX_Precache();

    /**
    *   (Temp-) Entity Events:
    **/
    // Ricochets SFX:
    precache.sfx.ric1 = clgi.S_RegisterSound( "world/ricochet_01.wav" );
    precache.sfx.ric2 = clgi.S_RegisterSound( "world/ricochet_02.wav" );
    precache.sfx.ric3 = clgi.S_RegisterSound( "world/ricochet_03.wav" );
    // Lasers SFX:
    precache.sfx.lashit = clgi.S_RegisterSound( "world/lashit01.wav" );
    // Weapon SFX:
    precache.sfx.explosion_rocket = clgi.S_RegisterSound( "explosions/expl_rocket01.wav" );
    precache.sfx.explosion_grenade01 = clgi.S_RegisterSound( "explosions/expl_grenade01.wav" );
    precache.sfx.explosion_grenade02 = clgi.S_RegisterSound( "explosions/expl_grenade02.wav" );
    precache.sfx.explosion_water = clgi.S_RegisterSound( "explosions/expl_water01.wav" );

    // Precache player land/fall.
    clgi.S_RegisterSound( "player/land01.wav" );
    clgi.S_RegisterSound( "player/fall02.wav" );
    clgi.S_RegisterSound( "player/fall01.wav" );

    /**
    *   Material Footsteps:
    **/
    CLG_PrecacheFootsteps();

    /**
    *   Precaches all local entity 'sound path' registered files. This has to be done after the other local sounds are loaded,
    *   to prevent their indexes from mixing up.
    **/
    CLG_PrecacheLocalSounds();
}

/**
*   @brief  Precaches('to load') all local 'model path' registered files.
**/
void CLG_PrecacheLocalModels() {
    // Iterate over the local model path 'config' strings.
    for ( int32_t i = 1; i <= precache.num_local_draw_models; i++ ) {
        // Ensure that its name is valid.
        const char *name = precache.model_paths[ i ];
        if ( !name || name[ 0 ] == 0 || name[ 0 ] == '\0' ) {
            precache.local_draw_models[ i ] = 0;
            continue;
        }

        // Precache the actual model, retreive the qhandle_t and store it.
        precache.local_draw_models[ i ] = clgi.R_RegisterModel( name );
    }
}
/**
*	@brief	Called right before loading all received configstring (server-) models.
**/
void PF_PrecacheClientModels( void ) {
    precache.models.explode = clgi.R_RegisterModel( "models/objects/explode/tris.md2" );
    precache.models.smoke = clgi.R_RegisterModel( "models/objects/smoke/tris.md2" );
    precache.models.flash = clgi.R_RegisterModel( "models/objects/flash/tris.md2" );
    precache.models.parasite_segment = clgi.R_RegisterModel( "models/monsters/parasite/segment/tris.md2" );
    precache.models.grapple_cable = clgi.R_RegisterModel( "models/ctf/segment/tris.md2" );
    precache.models.explo4 = clgi.R_RegisterModel( "models/objects/r_explode/tris.md2" );
    precache.models.explosions[ 0 ] = clgi.R_RegisterModel( "sprites/rocket_0.sp2" );
    precache.models.explosions[ 1 ] = clgi.R_RegisterModel( "sprites/rocket_1.sp2" );
    precache.models.explosions[ 2 ] = clgi.R_RegisterModel( "sprites/rocket_5.sp2" );
    precache.models.explosions[ 3 ] = clgi.R_RegisterModel( "sprites/rocket_6.sp2" );
    precache.models.bfg_explo = clgi.R_RegisterModel( "sprites/s_bfg2.sp2" );
    precache.models.powerscreen = clgi.R_RegisterModel( "models/items/armor/effect/tris.md2" );
    precache.models.laser = clgi.R_RegisterModel( "models/objects/laser/tris.md2" );
    precache.models.dmspot = clgi.R_RegisterModel( "models/objects/dmspot/tris.md2" );

    precache.models.lightning = clgi.R_RegisterModel( "models/proj/lightning/tris.md2" );
    precache.models.heatbeam = clgi.R_RegisterModel( "models/proj/beam/tris.md2" );
    precache.models.explo4_big = clgi.R_RegisterModel( "models/objects/r_explode2/tris.md2" );

    // Enable 'vertical' display for explosion models.
    for ( int32_t i = 0; i < sizeof( precache.models.explosions ) / sizeof( *precache.models.explosions ); i++ ) {
        clgi.SetSpriteModelVerticality( precache.models.explosions[ i ] );
        //model_t *model = MOD_ForHandle( cl_mod_explosions[ i ] );

        //if ( model ) {
        //    model->sprite_vertical = true;
        //}
    }

    // Precaches all local 'model path' registered files.
    CLG_PrecacheLocalModels();
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
    char        view_model_filename[ MAX_QPATH ];
    char        icon_filename[ MAX_QPATH ];

    // Parse client info's player skin data.
    PF_ParsePlayerSkin( ci->name, model_name, skin_name, s );

    // model file
    Q_concat( model_filename, sizeof( model_filename ),
        "players/", model_name, "/tris.iqm" );
    ci->model = clgi.R_RegisterModel( model_filename );
    if ( !ci->model && Q_stricmp( model_name, "playerdummy" ) ) {
        strcpy( model_name, "playerdummy" );
        strcpy( model_filename, "players/playerdummy/tris.iqm" );
        ci->model = clgi.R_RegisterModel( model_filename );
    }

    //// model file
    //Q_concat( model_filename, sizeof( model_filename ),
    //    "players/", model_name, "/tris.md2" );
    //ci->model = clgi.R_RegisterModel( model_filename );
    //if ( !ci->model && Q_stricmp( model_name, "male" ) ) {
    //    strcpy( model_name, "male" );
    //    strcpy( model_filename, "players/male/tris.md2" );
    //    ci->model = clgi.R_RegisterModel( model_filename );
    //}

    // skin file
    Q_concat( skin_filename, sizeof( skin_filename ),
        "players/", model_name, "/", skin_name, ".pcx" );
    ci->skin = clgi.R_RegisterSkin( skin_filename );

    // if we don't have the skin and the model was female,
    // see if athena skin exists
    if ( !ci->skin && !Q_stricmp( model_name, "playerdummy" ) ) {
        strcpy( skin_name, "playerdummy" );
        strcpy( skin_filename, "players/playerdummy/skin.pcx" );
        ci->skin = clgi.R_RegisterSkin( skin_filename );
    }

    //// if we don't have the skin and the model wasn't male,
    //// see if the male has it (this is for CTF's skins)
    //if ( !ci->skin && Q_stricmp( model_name, "male" ) ) {
    //    // change model to male
    //    strcpy( model_name, "male" );
    //    strcpy( model_filename, "players/male/tris.md2" );
    //    ci->model = clgi.R_RegisterModel( model_filename );

    //    // see if the skin exists for the male model
    //    Q_concat( skin_filename, sizeof( skin_filename ),
    //        "players/male/", skin_name, ".pcx" );
    //    ci->skin = clgi.R_RegisterSkin( skin_filename );
    //}

    //// if we still don't have a skin, it means that the male model
    //// didn't have it, so default to grunt
    //if ( !ci->skin ) {
    //    // see if the skin exists for the male model
    //    strcpy( skin_name, "grunt" );
    //    strcpy( skin_filename, "players/male/grunt.pcx" );
    //    ci->skin = clgi.R_RegisterSkin( skin_filename );
    //}

    // Viewmodel files:
    for ( i = 0; i < precache.numViewModels; i++ ) {
        // Load weapon view model from player model_name directory.
        Q_concat( view_model_filename, sizeof( view_model_filename ),
            "players/", model_name, "/", precache.viewModels[ i ] );
        ci->weaponmodel[ i ] = clgi.R_RegisterModel( view_model_filename );

        // try male if not cyborg(genderless?)
        if ( !ci->weaponmodel[ i ] && !Q_stricmp( model_name, "cyborg" ) ) {
            Q_concat( view_model_filename, sizeof( view_model_filename ),
                "players/male/", precache.viewModels[ i ] );
            ci->weaponmodel[ i ] = clgi.R_RegisterModel( view_model_filename );
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

/**
*   @brief  Registers a model for local entity usage.
*   @return -1 on failure, otherwise a handle to the model index of the precache.local_models array.
**/
const qhandle_t CLG_RegisterLocalModel( const char *name ) {
    // Make sure name isn't empty.
    if ( !name || name[0] == 0 || name[0] == '\0' || strlen(name) == 0 ) {
        clgi.Print( PRINT_WARNING, "%s: empty model name detected!\n", __func__ );
        return 0;
    }

    // Increment here directly, the 0 indexed value of model_paths
    // will always be an empty string, it is left unused so we can
    // deal with modelindex == 0 being a non visible model.
    const int32_t index = precache.num_local_draw_models += 1;

    // Throw it into the localModelPaths array.
    if ( precache.num_local_draw_models >= MAX_MODELS ) {
        clgi.Error( "%s: num_local_draw_models >= MAX_MODELS!\n", __func__ );
        return 0;
    }

    // Copy the model name inside the next model_paths slot.
    Q_strlcpy( precache.model_paths[ index ], name, MAX_QPATH );

    // Success.
    return index;
}

/**
*   @brief  Registers a sound for local entity usage.
*   @return -1 on failure, otherwise a handle to the sounds index of the precache.local_sounds array.
**/
const qhandle_t CLG_RegisterLocalSound( const char *name ) {
    // Make sure name isn't empty.
    if ( !name || name[ 0 ] == 0 || name[ 0 ] == '\0' || strlen( name ) == 0 ) {
        clgi.Print( PRINT_WARNING, "%s: empty sound name detected!\n", __func__ );
        return 0;
    }

    // Increment here directly, the 0 indexed value of sound_paths
    // will always be an empty string, it is left unused so we can
    // deal with sound == 0 being a non visible model.
    const int32_t index = precache.num_local_sounds += 1;

    // Throw it into the localSoundPaths array.
    if ( precache.num_local_sounds >= MAX_SOUNDS ) {
        clgi.Error( "%s: num_local_sounds >= MAX_SOUNDS!\n", __func__ );
        return 0;
    }

    // Copy the model name inside the next sound_paths slot.
    Q_strlcpy( precache.sound_paths[ index ], name, MAX_QPATH );

    // Success.
    return index;
}

/**
*   @brief  Used by PF_ClearState.
**/
void CLG_Precache_ClearState() {
    // Reset the local precache paths.
    precache.num_local_draw_models = 0;
    memset( precache.model_paths, 0, MAX_MODELS * MAX_QPATH );
    precache.num_local_sounds = 0;
    memset( precache.sound_paths, 0, MAX_SOUNDS * MAX_QPATH );

    // Reset the number of view models.
    precache.numViewModels = 0;
    memset( precache.viewModels, 0, MAX_CLIENTVIEWMODELS * MAX_QPATH );

    // Reset the local precache paths.
    precache.num_local_draw_models = 0;
    memset( precache.model_paths, 0, MAX_MODELS * MAX_QPATH );
    precache.num_local_sounds = 0;
    memset( precache.sound_paths, 0, MAX_SOUNDS * MAX_QPATH );

    // Reset the number of view models.
    precache.numViewModels = 0;
    memset( precache.viewModels, 0, MAX_CLIENTVIEWMODELS * MAX_QPATH );
}