/********************************************************************
*
*
*	ClientGame: View Scene Management.
*
*
********************************************************************/
#include "clg_local.h"

/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
// cl_view.c -- player rendering positioning

#include "cl_client.h"

//=============
//
// development tools for weapons
//
int32_t     gun_frame = 0;
qhandle_t   gun_model = 0;

//=============

static cvar_t *cl_add_particles = nullptr;
static cvar_t *cl_add_lights = nullptr;
static cvar_t *cl_show_lights = nullptr;
static cvar_t *cl_flashlight = nullptr;
static cvar_t *cl_flashlight_intensity = nullptr;
static cvar_t *cl_add_entities = nullptr;
static cvar_t *cl_add_blend = nullptr;
static cvar_t *cl_flashlight_offset = nullptr;
static vec3_t flashlight_offset;

#if USE_DEBUG
static cvar_t *cl_testparticles;
static cvar_t *cl_testentities;
static cvar_t *cl_testlights;
static cvar_t *cl_testblend;

static cvar_t *cl_stats;
#endif

/**
*   @brief  
**/
void PF_ClearViewScene( void ) {
    cl.viewScene.r_numdlights = 0;
    cl.viewScene.r_numentities = 0;
    cl.viewScene.r_numparticles = 0;
}


/**
*   @brief  
**/
void CLG_View_AddEntity( entity_t *ent ) {
    if ( cl.viewScene.r_numentities >= MAX_ENTITIES ) {
        return;
    }
    cl.viewScene.r_entities[ cl.viewScene.r_numentities++ ] = *ent;
}

/**
*   @brief  
**/
void CLG_View_AddParticle( particle_t *p ) {
    if ( cl.viewScener_numparticles >= MAX_PARTICLES ) {
        return;
    }
    cl.viewScene.r_particles[ r_numparticles++ ] = *p;
}

/**
*   @brief
**/
static dlight_t *add_spot_light_common( const Vector3 &org, const Vector3 &dir, const float intensity, const float r, const float g, const float b ) {
    dlight_t *dl;

    if ( cl.viewScene.r_numdlights >= MAX_DLIGHTS )
        return NULL;
    dl = &cl.viewScene.r_dlights[ cl.viewScene.r_numdlights++ ];
    memset( dl, 0, sizeof( dlight_t ) );
    VectorCopy( org, dl->origin );
    dl->intensity = intensity;
    dl->color[ 0 ] = r;
    dl->color[ 1 ] = g;
    dl->color[ 2 ] = b;
    dl->radius = 1.0f;
    dl->light_type = DLIGHT_SPOT;
    VectorCopy( dir, dl->spot.direction );

    // what would make sense for cl_show_lights here?

    return dl;
}
/**
*   @brief  
**/
void CLG_View_AddSphereLight( const Vector3 &org, const float intensity, const float r, const float g, const float b, const float radius ) {
    dlight_t *dl;

    if ( cl.viewScene.r_numdlights >= MAX_DLIGHTS )
        return;
    dl = &cl.viewScene.r_dlights[ cl.viewScene.r_numdlights++ ];
    memset( dl, 0, sizeof( dlight_t ) );
    VectorCopy( org, dl->origin );
    dl->intensity = intensity;
    dl->color[ 0 ] = r;
    dl->color[ 1 ] = g;
    dl->color[ 2 ] = b;
    dl->radius = radius;

    if ( cl_show_lights->integer && cl.viewScene.r_numparticles < MAX_PARTICLES ) {
        particle_t *part = &cl.viewScene.r_particles[ cl.viewScene.r_numparticles++ ];

        VectorCopy( dl->origin, part->origin );
        part->radius = radius;
        part->brightness = max( r, max( g, b ) );
        part->color = -1;
        part->rgba.u8[ 0 ] = (uint8_t)max( 0.f, min( 255.f, r / part->brightness * 255.f ) );
        part->rgba.u8[ 1 ] = (uint8_t)max( 0.f, min( 255.f, g / part->brightness * 255.f ) );
        part->rgba.u8[ 2 ] = (uint8_t)max( 0.f, min( 255.f, b / part->brightness * 255.f ) );
        part->rgba.u8[ 3 ] = 255;
        part->alpha = 1.f;
    }
}
/**
*   @brief
**/
void CLG_View_AddSpotLight( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, float falloff_angle ) {
    dlight_t *dl = add_spot_light_common( org, dir, intensity, r, g, b );
    if ( !dl )
        return;

    dl->spot.emission_profile = DLIGHT_SPOT_EMISSION_PROFILE_FALLOFF;
    dl->spot.cos_total_width = cosf( DEG2RAD( width_angle ) );
    dl->spot.cos_falloff_start = cosf( DEG2RAD( falloff_angle ) );
}
/**
*   @brief
**/
void CLG_View_AddSpotLightTexEmission( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, qhandle_t emission_tex ) {
    dlight_t *dl = add_spot_light_common( org, dir, intensity, r, g, b );
    if ( !dl )
        return;

    dl->spot.emission_profile = DLIGHT_SPOT_EMISSION_PROFILE_AXIS_ANGLE_TEXTURE;
    dl->spot.total_width = DEG2RAD( width_angle );
    dl->spot.texture = emission_tex;
}
/**
*   @brief
**/
void CLG_View_AddLight( const vec3_t org, float intensity, float r, float g, float b ) {
    CLG_View_AddSphereLight( org, intensity, r, g, b, 10.f );
}

/**
*   @brief
**/
void CLG_View_Flashlight( void ) {
    if ( cls.ref_type == REF_TYPE_VKPT ) {
        playecl.viewScene.r_state_t *ps = &cl.frame.ps;
        playecl.viewScene.r_state_t *ops = &cl.oldframe.ps;

        // Flashlight origin
        vec3_t light_pos;
        // Flashlight direction (as angles)
        vec3_t flashlight_angles;

        if ( cls.demo.playback ) {
            /* If a demo is played we don't have predicted_angles,
             * and we can't use cl.refdef.viewangles for the same reason
             * below. However, lerping the angles from the old & current frame
             * work nicely. */
            LerpAngles( cl.oldframe.ps.viewangles, cl.frame.ps.viewangles, cl.lerpfrac, flashlight_angles );
        } else {
            /* Use cl.playerEntityOrigin+viewoffset, playerEntityAngles instead of
             * cl.refdef.vieworg, cl.refdef.viewangles as as the cl.refdef values
             * are the camera values, but not the player "eye" values in 3rd person mode. */
            VectorCopy( cl.predictedState.view.angles, flashlight_angles );
        }
        // Add a bit of gun bob to the flashlight as well
        vec3_t gunangles;
        LerpVector( ops->gunangles, ps->gunangles, cl.lerpfrac, gunangles );
        VectorAdd( flashlight_angles, gunangles, flashlight_angles );

        vec3_t view_dir, right_dir, up_dir;
        AngleVectors( flashlight_angles, view_dir, right_dir, up_dir );

        /* Start off with the player eye position. */
        vec3_t viewoffset;
        LerpVector( ops->viewoffset, ps->viewoffset, cl.lerpfrac, viewoffset );
        VectorAdd( cl.playerEntityOrigin, viewoffset, light_pos );

        /* Slightly move position downward, right, and forward to get a position
         * that looks somewhat as if it was attached to the gun.
         * Generally, the spot light origin should be placed away from the player model
         * to avoid interactions with it (mainly unexpected shadowing).
         * When adjusting the offsets care must be taken that
         * the flashlight doesn't also light the view weapon. */
        VectorMA( light_pos, flashlight_offset[ 2 ] * cl_gunscale->value, view_dir, light_pos );
        float leftright = flashlight_offset[ 0 ] * cl_gunscale->value;
        if ( info_hand->integer == 1 )
            leftright = -leftright; // left handed
        else if ( info_hand->integer == 2 )
            leftright = 0.f; // "center" handed
        VectorMA( light_pos, leftright, right_dir, light_pos );
        VectorMA( light_pos, flashlight_offset[ 1 ] * cl_gunscale->value, up_dir, light_pos );

        V_AddSpotLightTexEmission( light_pos, view_dir, cl_flashlight_intensity->value, 1.f, 1.f, 1.f, 90.0f, flashlight_profile_tex );
    } else {
        // Flashlight is VKPT only
    }
}

/**
*   @brief
**/
void CLG_View_AddLightStyle( int style, float value ) {
    lightstyle_t *ls;

    if ( style < 0 || style >= MAX_LIGHTSTYLES )
        Com_Error( ERR_DROP, "Bad light style %i", style );
    ls = &cl.viewScene.r_lightstyles[ style ];
    ls->white = value;
}

#if USE_DEBUG
/**
*   @brief  If cl_testparticles is set, create 4096 particles in the view
**/
static void CLG_View_TestParticles( void ) {
    particle_t *p;
    int         i, j;
    float       d, r, u;

    cl.viewScene.r_numparticles = MAX_PARTICLES;
    for ( i = 0; i < cl.viewScene.r_numparticles; i++ ) {
        d = i * 0.25f;
        r = 4 * ( ( i & 7 ) - 3.5f );
        u = 4 * ( ( ( i >> 3 ) & 7 ) - 3.5f );
        p = &cl.viewScene.r_particles[ i ];

        for ( j = 0; j < 3; j++ )
            p->origin[ j ] = cl.refdef.vieworg[ j ] + cl.v_forward[ j ] * d +
            cl.v_right[ j ] * r + cl.v_up[ j ] * u;

        p->color = 8;
        p->alpha = 1;
    }
}

/**
*   @brief  If cl_testentities is set, create 32 player models
**/
static void CLG_View_TestEntities( void ) {
    int         i, j;
    float       f, r;
    entity_t *ent;

    cl.viewScene.r_numentities = 32;
    memset( cl.viewScene.r_entities, 0, sizeof( cl.viewScene.r_entities ) );

    for ( i = 0; i < cl.viewScene.r_numentities; i++ ) {
        ent = &cl.viewScene.r_entities[ i ];

        r = 64 * ( ( i % 4 ) - 1.5f );
        f = 64 * ( i / 4 ) + 128;

        for ( j = 0; j < 3; j++ )
            ent->origin[ j ] = cl.refdef.vieworg[ j ] + cl.v_forward[ j ] * f +
            cl.v_right[ j ] * r;

        ent->model = cl.baseclientinfo.model;
        ent->skin = cl.baseclientinfo.skin;
    }
}

/**
*   @brief  If cl_testlights is set, create 32 lights models
**/
static void CLG_View_TestLights( void ) {
    int         i, j;
    float       f, r;
    dlight_t *dl;

    if ( cl_testlights->integer != 1 ) {
        dl = &cl.viewScene.r_dlights[ 0 ];
        memset( dl, 0, sizeof( dlight_t ) );
        cl.viewScene.r_numdlights = 1;

        VectorMA( cl.refdef.vieworg, 256, cl.v_forward, dl->origin );
        if ( cl_testlights->integer == -1 )
            VectorSet( dl->color, -1, -1, -1 );
        else
            VectorSet( dl->color, 1, 1, 1 );
        dl->intensity = 256;
        dl->radius = 16;
        return;
    }

    cl.viewScene.r_numdlights = 32;
    memset( cl.viewScene.r_dlights, 0, sizeof( cl.viewScene.r_dlights ) );

    for ( i = 0; i < cl.viewScene.r_numdlights; i++ ) {
        dl = &cl.viewScene.r_dlights[ i ];

        r = 64 * ( ( i % 4 ) - 1.5f );
        f = 64 * ( i / 4 ) + 128;

        for ( j = 0; j < 3; j++ )
            dl->origin[ j ] = cl.refdef.vieworg[ j ] + cl.v_forward[ j ] * f +
            cl.v_right[ j ] * r;
        dl->color[ 0 ] = ( ( i % 6 ) + 1 ) & 1;
        dl->color[ 1 ] = ( ( ( i % 6 ) + 1 ) & 2 ) >> 1;
        dl->color[ 2 ] = ( ( ( i % 6 ) + 1 ) & 4 ) >> 2;
        dl->intensity = 200;
        dl->radius = 16;
    }
}

#endif

//============================================================================

// gun frame debugging functions
static void CLG_View_Gun_Next_f( void ) {
    gun_frame++;
    Com_Printf( "frame %i\n", gun_frame );
}

static void CLG_View_Gun_Prev_f( void ) {
    gun_frame--;
    if ( gun_frame < 0 )
        gun_frame = 0;
    Com_Printf( "frame %i\n", gun_frame );
}

static void CLG_View_Gun_Model_f( void ) {
    char    name[ MAX_QPATH ];

    if ( Cmd_Argc() != 2 ) {
        gun_model = 0;
        return;
    }
    Q_concat( name, sizeof( name ), "models/", Cmd_Argv( 1 ), "/tris.iqm" );
    gun_model = R_RegisterModel( name );
}

//============================================================================
/**
*   @brief  Prepares the current frame's view scene for the refdef by
*           emitting all frame data(entities, particles, dynamic lights, lightstyles,
*           and temp entities) to the refresh definition.
**/
void PF_PrepareViewEntites( void ) {
    CLG_AddPacketEntities();
    CLG_AddTEnts();
    CLG_AddParticles();
    CLG_AddDLights();
    CLG_AddLightStyles();
    CLG_AddTestModel();

#if USE_DEBUG
if ( cl_testparticles->integer ) {
    V_TestParticles();
}
if ( cl_testentities->integer ) {
    V_TestEntities();
}
if ( cl_testlights->integer ) {
    V_TestLights();
}
if ( cl_testblend->integer ) {
    cl.refdef.screen_blend[ 0 ] = 1;
    cl.refdef.screen_blend[ 1 ] = 0.5f;
    cl.refdef.screen_blend[ 2 ] = 0.25f;
    cl.refdef.screen_blend[ 3 ] = 0.5f;
}
#endif

    if ( cl_flashlight->integer ) {
        CLG_View_Flashlight();
    }
}


/*
=============
V_Viewpos_f
=============
*/
static void CLG_View_Viewpos_f( void ) {
    Com_Printf( "%s : %.f\n", vtos( cl.refdef.vieworg ), cl.refdef.viewangles[ YAW ] );
}

static const cmdreg_t v_cmds[] = {
    { "gun_next", V_Gun_Next_f },
    { "gun_prev", V_Gun_Prev_f },
    { "gun_model", V_Gun_Model_f },
    { "viewpos", V_Viewpos_f },
    { NULL }
};

static void cl_add_blend_changed( cvar_t *self ) {
}

//static void cl_flashlight_offset_changed( cvar_t *self ) {
//    sscanf( cl_flashlight_offset->string, "%f %f %f", &flashlight_offset[ 0 ], &flashlight_offset[ 1 ], &flashlight_offset[ 2 ] );
//}

/**
*   @brief  
**/
void CLG_View_Init( void ) {
    Cmd_Register( v_cmds );

    #if USE_DEBUG
    cl_testblend = Cvar_Get( "cl_testblend", "0", 0 );
    cl_testparticles = Cvar_Get( "cl_testparticles", "0", 0 );
    cl_testentities = Cvar_Get( "cl_testentities", "0", 0 );
    cl_testlights = Cvar_Get( "cl_testlights", "0", CVAR_CHEAT );

    cl_stats = Cvar_Get( "cl_stats", "0", 0 );
    #endif
}

/**
*   @brief
**/
void CLG_View_Shutdown( void ) {
    if ( flashlight_profile_tex != -1 )
        R_UnregisterImage( flashlight_profile_tex );

    Cmd_Deregister( v_cmds );
}