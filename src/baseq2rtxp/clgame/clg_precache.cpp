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
    int     i;
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

    for ( i = 0; i < 4; i++ ) {
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
    for ( int i = 0; i < sizeof( precache.cl_mod_explosions ) / sizeof( *precache.cl_mod_explosions ); i++ ) {
        clgi.SetSpriteModelVerticality( precache.cl_mod_explosions[ i ] );
        //model_t *model = MOD_ForHandle( cl_mod_explosions[ i ] );

        //if ( model ) {
        //    model->sprite_vertical = true;
        //}
    }
}