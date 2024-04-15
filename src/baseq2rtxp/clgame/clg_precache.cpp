/********************************************************************
*
*
*	ClientGame: Deal with optional client specific precaching.
*
*
********************************************************************/
#include "clg_local.h"
#include "clg_parse.h"
#include "clg_precache.h"

/**
*
*
*   Reverb Effects Types:
*
*
**/
//!
//! Default reverb effect type properties:
//! 
sfx_reverb_properties_t cl_eax_default_properties = {
    .flDensity = 1.0,
    .flDiffusion = 1.0,

    .flGain = 0.f,
    .flGainHF = 1.0f,
    .flGainLF = 1.0f,

    .flDecayTime = 1.0f,
    .flDecayHFRatio = 1.0f,
    .flDecayLFRatio = 1.0f,

    .flReflectionsGain = 0.f,
    .flReflectionsDelay = 0.f,
    .flReflectionsPan = { 0.f, 0.f, 0.f },

    .flLateReverbGain = 1.f,
    .flLateReverbDelay = 0.f,
    .flLateReverbPan = { 0.f, 0.f, 0.f },

    .flEchoTime = 0.25f,
    .flEchoDepth = 0.f,

    .flModulationTime = 0.25f,
    .flModulationDepth = 0.f,

    .flAirAbsorptionGainHF = 1.f,
    .flHFReference = 5000.0f,
    .flLFReference = 250.f,

    .flRoomRolloffFactor = 0.f,

    // decay_limit
    .iDecayHFLimit = 1,
};
//!
//! Underwater reverb effect type properties:
//!
sfx_reverb_properties_t cl_eax_underwater_properties = {
    .flDensity = 0.3645,
    .flDiffusion = 1.0,

    .flGain = 0.316200f,
    .flGainHF = 0.01f,
    .flGainLF = 1.0f,

    .flDecayTime = 1.49f,
    .flDecayHFRatio = 0.1f,
    .flDecayLFRatio = 1.0f,

    .flReflectionsGain = 0.5963f,
    .flReflectionsDelay = 0.007f,
    .flReflectionsPan = { 0.f, 0.f, 0.f },

    .flLateReverbGain = 7.0795f,
    .flLateReverbDelay = 0.011f,
    .flLateReverbPan = { 0.f, 0.f, 0.f },

    .flEchoTime = 0.25f,
    .flEchoDepth = 0.f,

    .flModulationTime = 1.18f,
    .flModulationDepth = 0.348f,

    .flAirAbsorptionGainHF = 0.9943f,
    .flHFReference = 5000.0f,
    .flLFReference = 250.f,

    .flRoomRolloffFactor = 0.f,

    // decay_limit
    .iDecayHFLimit = 1,
};
//!
//! Metal - Small - reverb effect type properties:
//!
sfx_reverb_properties_t cl_eax_metal_s_properties = {
    .flDensity = 1.0,
    .flDiffusion = 0.7,

    .flGain = 0.316200f,
    .flGainHF = 0.4477f,
    .flGainLF = 1.0f,

    .flDecayTime = 1.51f,
    .flDecayHFRatio = 1.25f,
    .flDecayLFRatio = 1.14f,

    .flReflectionsGain = 0.8913f,
    .flReflectionsDelay = 0.02f,
    .flReflectionsPan = { 0.f, 0.f, 0.f },

    .flLateReverbGain = 1.4125f,
    .flLateReverbDelay = 0.03f,
    .flLateReverbPan = { 0.f, 0.f, 0.f },

    .flEchoTime = 0.179f,
    .flEchoDepth = 0.15f,

    .flModulationTime = 0.895f,
    .flModulationDepth = 0.19f,

    .flAirAbsorptionGainHF = 0.992f,
    .flHFReference = 5000.0f,
    .flLFReference = 250.f,

    .flRoomRolloffFactor = 0.f,

    // decay_limit
    .iDecayHFLimit = 0,
};
//!
//! Tunnel - Small - reverb effect type properties:
//!
sfx_reverb_properties_t cl_eax_tunnel_s_properties = {
    .flDensity = 1.0,
    .flDiffusion = 0.690,

    .flGain = 0.316200f,
    .flGainHF = 0.79430f,
    .flGainLF = 0.81930f,

    .flDecayTime = 3.28f,
    .flDecayHFRatio = 1.17f,
    .flDecayLFRatio = 0.91f,

    .flReflectionsGain = 0.4467f,
    .flReflectionsDelay = 0.4400f,
    .flReflectionsPan = { 0.f, 0.f, 0.f },

    .flLateReverbGain = 0.2818f,
    .flLateReverbDelay = 0.024f,
    .flLateReverbPan = { 0.f, 0.f, 0.f },

    .flEchoTime = 0.25f,
    .flEchoDepth = 0.20f,

    .flModulationTime = 0.25f,
    .flModulationDepth = 0.0f,

    .flAirAbsorptionGainHF = 0.9966f,
    .flHFReference = 5000.0f,
    .flLFReference = 250.f,

    .flRoomRolloffFactor = 0.f,

    // decay_limit
    .iDecayHFLimit = 1,
};
//!
//! Tunnel - Large - reverb effect type properties:
//!
sfx_reverb_properties_t cl_eax_tunnel_l_properties = {
    .flDensity = 1.0,
    .flDiffusion = 0.820,

    .flGain = 0.316200f,
    .flGainHF = 0.44670f,
    .flGainLF = 0.89130f,

    .flDecayTime = 3.57f,
    .flDecayHFRatio = 1.12f,
    .flDecayLFRatio = 0.91f,

    .flReflectionsGain = 0.3981f,
    .flReflectionsDelay = 0.0590f,
    .flReflectionsPan = { 0.f, 0.f, 0.f },

    .flLateReverbGain = 0.89130f,
    .flLateReverbDelay = 0.037f,
    .flLateReverbPan = { 0.f, 0.f, 0.f },

    .flEchoTime = 0.25f,
    .flEchoDepth = 0.14f,

    .flModulationTime = 0.25f,
    .flModulationDepth = 0.0f,

    .flAirAbsorptionGainHF = 0.992f,
    .flHFReference = 5000.0f,
    .flLFReference = 250.f,

    .flRoomRolloffFactor = 0.f,

    // decay_limit
    .iDecayHFLimit = 1,
};


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
*	@brief	Called right before loading all received configstring (server-) sounds.
**/
void PF_PrecacheClientSounds( void ) {
    char    name[ MAX_QPATH ];

    // Reverb Effects:
    precache.cl_eax_reverb_effects[ SOUND_EAX_EFFECT_DEFAULT ] = clgi.S_RegisterReverbEffect("default", &cl_eax_default_properties);
    precache.cl_eax_reverb_effects[ SOUND_EAX_EFFECT_UNDERWATER ] = clgi.S_RegisterReverbEffect( "underwater", &cl_eax_underwater_properties );
    precache.cl_eax_reverb_effects[ SOUND_EAX_EFFECT_METAL_S ] = clgi.S_RegisterReverbEffect( "metal_s", &cl_eax_metal_s_properties );
    precache.cl_eax_reverb_effects[ SOUND_EAX_EFFECT_TUNNEL_S ] = clgi.S_RegisterReverbEffect( "tunnel_s", &cl_eax_tunnel_s_properties );
    precache.cl_eax_reverb_effects[ SOUND_EAX_EFFECT_TUNNEL_L ] = clgi.S_RegisterReverbEffect( "tunnel_s", &cl_eax_tunnel_l_properties );    // We loaded 3 reverb effects, make sure the cache is aware of this.
    precache.cl_num_eax_reverb_effects = 4;

    // Ricochets SFX:
    precache.cl_sfx_ric1 = clgi.S_RegisterSound( "world/ric1.wav" );
    precache.cl_sfx_ric2 = clgi.S_RegisterSound( "world/ric2.wav" );
    precache.cl_sfx_ric3 = clgi.S_RegisterSound( "world/ric3.wav" );
    // Lasers SFX:
    precache.cl_sfx_lashit = clgi.S_RegisterSound( "weapons/lashit.wav" );
    // Flare/Sparks SFX:
    precache.cl_sfx_flare = clgi.S_RegisterSound( "weapons/flare.wav" );
    precache.cl_sfx_spark5 = clgi.S_RegisterSound( "world/spark5.wav" );
    precache.cl_sfx_spark6 = clgi.S_RegisterSound( "world/spark6.wav" );
    precache.cl_sfx_spark7 = clgi.S_RegisterSound( "world/spark7.wav" );
    // Weapon SFX:
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