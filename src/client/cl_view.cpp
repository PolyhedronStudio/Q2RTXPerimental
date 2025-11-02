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


// WID: C++20: For linkage with .c
QEXTERN_C_ENCLOSE( cvar_t *cl_adjustfov; );
static cvar_t *cl_view_debug_lights = nullptr;


/**
*   @brief  Calculate the client's PVS which is a necessity for culling out
*           local client entities.
**/
void V_CalculateLocalPVS( const vec3_t viewOrigin ) {
    if ( !cl.collisionModel.cache ) {
        return;
    }

    cl.localPVS.leaf = BSP_PointLeaf( cl.collisionModel.cache->nodes, viewOrigin );

    if ( !cl.localPVS.leaf ) {
        // TODO: What do?
        Com_LPrintf( PRINT_DEVELOPER, "%s: warning, no BSP leaf returned for vieworg(%f, %f, %f)\n",
            __func__, viewOrigin[ 0 ], viewOrigin[ 1 ], viewOrigin[ 2 ] );
        return;
    }

    // Calculate the PVS bits.
    cl.localPVS.leafArea = cl.localPVS.leaf->area;
    cl.localPVS.leafCluster = cl.localPVS.leaf->cluster;

    if ( cl.localPVS.leafCluster >= 0 ) {
        CM_FatPVS( &cl.collisionModel, cl.localPVS.pvs, viewOrigin, DVIS_PVS );
        cl.localPVS.lastValidCluster = cl.localPVS.leafCluster;
    } else {
        BSP_ClusterVis( cl.collisionModel.cache, cl.localPVS.pvs, cl.localPVS.lastValidCluster, DVIS_PVS /*DVIS_PVS2*/ );
    }
}

/**
*   @brief Adds an entity to the view scene for the current frame.
**/
void V_AddEntity( entity_t *ent ) {
    if ( cl.viewScene.r_numentities >= MAX_ENTITIES )
        return;

    cl.viewScene.r_entities[ cl.viewScene.r_numentities++ ] = *ent;
}

/**
*   @brief Adds a particle to the view scene for the current frame.
**/
void V_AddParticle( particle_t *p ) {
    if ( cl.viewScene.r_numparticles >= MAX_PARTICLES )
        return;
    cl.viewScene.r_particles[ cl.viewScene.r_numparticles++ ] = *p;
}
/**
*   @brief Adds a dynamic spherical light to the view scene for the current frame.
**/
void V_AddSphereLight( const vec3_t org, float intensity, float r, float g, float b, float radius ) {
    dlight_t *dl;

    if ( cl.viewScene.r_numdlights >= MAX_DLIGHTS ) {
        return;
    }
    dl = &cl.viewScene.r_dlights[ cl.viewScene.r_numdlights++ ];
    memset( dl, 0, sizeof( dlight_t ) );
    VectorCopy( org, dl->origin );
    dl->intensity = intensity;
    dl->color[ 0 ] = r;
    dl->color[ 1 ] = g;
    dl->color[ 2 ] = b;
    dl->radius = radius;

    if ( cl_view_debug_lights->integer && cl.viewScene.r_numparticles < MAX_PARTICLES ) {
        particle_t *part = &cl.viewScene.r_particles[ cl.viewScene.r_numparticles++ ];

        VectorCopy( dl->origin, part->origin );
        part->radius = radius;
        part->brightness = std::max( r, std::max( g, b ) );
        part->color = -1;
        part->rgba.u8[ 0 ] = (uint8_t)std::max( 0.f, std::min( 255.f, r / part->brightness * 255.f ) );
        part->rgba.u8[ 1 ] = (uint8_t)std::max( 0.f, std::min( 255.f, g / part->brightness * 255.f ) );
        part->rgba.u8[ 2 ] = (uint8_t)std::max( 0.f, std::min( 255.f, b / part->brightness * 255.f ) );
        part->rgba.u8[ 3 ] = 255;
        part->alpha = 1.f;
    }
}
/**
*   @brief Adds a spot light to the view scene for the current frame.
**/
static dlight_t *add_spot_light_common( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b ) {
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
    if ( cl_view_debug_lights && cl.viewScene.r_numparticles < MAX_PARTICLES ) {
        particle_t *part = &cl.viewScene.r_particles[ cl.viewScene.r_numparticles++ ];
        VectorCopy( dl->origin, part->origin );
        part->radius = 4.0f;
        part->brightness = std::max( r, std::max( g, b ) );
        part->color = -1;
        part->rgba.u8[ 0 ] = (uint8_t)std::max( 0.f, std::min( 255.f, r / part->brightness * 255.f ) );
        part->rgba.u8[ 1 ] = (uint8_t)std::max( 0.f, std::min( 255.f, g / part->brightness * 255.f ) );
        part->rgba.u8[ 2 ] = (uint8_t)std::max( 0.f, std::min( 255.f, b / part->brightness * 255.f ) );
        part->rgba.u8[ 3 ] = 255;
        part->alpha = 1.f;
	}

    return dl;
}

/**
*   @brief Adds a spot light with a falloff profile to the view scene for the current frame.
**/
void V_AddSpotLight( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, float falloff_angle ) {
    dlight_t *dl = add_spot_light_common( org, dir, intensity, r, g, b );
    if ( !dl )
        return;

    dl->spot.emission_profile = DLIGHT_SPOT_EMISSION_PROFILE_FALLOFF;
    dl->spot.cos_total_width = cosf( DEG2RAD( width_angle ) );
    dl->spot.cos_falloff_start = cosf( DEG2RAD( falloff_angle ) );
}
/**
*   @brief Adds a spot light with a texture emission profile to the view scene for the current frame.
**/
void V_AddSpotLightTexEmission( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, qhandle_t emission_tex ) {
    dlight_t *dl = add_spot_light_common( org, dir, intensity, r, g, b );
    if ( !dl )
        return;

    dl->spot.emission_profile = DLIGHT_SPOT_EMISSION_PROFILE_AXIS_ANGLE_TEXTURE;
    dl->spot.total_width = DEG2RAD( width_angle );
    dl->spot.texture = emission_tex;
}
/**
*   @brief Adds a spherical light to the view scene for the current frame.
**/
void V_AddLight( const vec3_t org, float intensity, float r, float g, float b ) {
    V_AddSphereLight( org, intensity, r, g, b, 10.f );
}
/**
*   @brief Sets the value of a lightstyle for the current frame.
**/
void V_AddLightStyle( int style, float value ) {
    lightstyle_t *ls;

    if ( style < 0 || style >= MAX_LIGHTSTYLES )
        Com_Error( ERR_DROP, "Bad light style %i", style );
    ls = &cl.viewScene.r_lightstyles[ style ];
    ls->white = value;
}



//============================================================================
/**
*   @brief  Calculates the vertical field of view based on horizontal fov and screen dimensions.
**/
const float V_CalcFov( const float fov_x, const float width, const float height ) {
    return clge->CalculateFieldOfView( fov_x, width, height );
}

/**
*   @brief  Sets cl.refdef view values and sound spatialization params.
*           Usually called from CL_PrepareViewEntities, but may be directly called from the main
*           loop if rendering is disabled but sound is running.
**/
void CL_CalculateViewValues( void ) {
    // Update view values.
    clge->CalculateViewValues();
}

/**
*   @brief  Initializes the client view system.
**/
void V_Init(void)
{
    // Initialize our engine fov cvar.
    cl_adjustfov = Cvar_Get("cl_adjustfov", "1", 0);
	cl_view_debug_lights = Cvar_Get( "cl_view_debug_lights", "0", CVAR_CHEAT );

	// Now initialize the clientgame view system.
	clge->InitViewScene();
}
/**
*   @brief  Shutsdown the client view system.
**/
void V_Shutdown(void)
{
    // Shutdown client game view system.
	clge->ShutdownViewScene();
}