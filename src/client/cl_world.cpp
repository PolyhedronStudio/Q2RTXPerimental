/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "cl_client.h"
#include "common/collisionmodel.h"
#include "client/cl_world.h"

/**
*	@return	A headnode that can be used for testing and/or clipping an
*			object 'hull' of mins/maxs size for the entity's said 'solid'.
**/
static mnode_t *CL_HullForEntity( const centity_t *ent/*, const bool includeSolidTriggers = false */) {
    if ( ent->current.solid == (cm_solid_t)BOUNDS_BRUSHMODEL /*|| ( includeSolidTriggers && ent->current.solid == SOLID_TRIGGER )*/ ) {
        // Subtract 1 to get the modelindex into a 0-based array.
        // ( Index 0 is reserved for no model )
        const int32_t i = ent->current.modelindex - 1;

        // explicit hulls in the BSP model
        if ( i <= 0 || i >= cl.collisionModel.cache->nummodels ) {
            Com_Error( ERR_DROP, "%s: inline model %d out of range", __func__, i );
            return nullptr;
        }

        return cl.collisionModel.cache->models[ i ].headnode;
    }

    // Create a temp hull from entity bounds and contents clipMask for the specific type of 'solid'.
    if ( ent->current.solid == SOLID_BOUNDS_OCTAGON ) {
        return CM_HeadnodeForOctagon( &cl.collisionModel, &ent->mins.x, &ent->maxs.x, ent->current.hullContents );
    } else {
        return CM_HeadnodeForBox( &cl.collisionModel, &ent->mins.x, &ent->maxs.x, ent->current.hullContents );
    }
}


/**
*   @brief  Clips the trace to all entities currently in-frame.
**/
static void CL_ClipMoveToEntities( cm_trace_t *tr, const Vector3 &start, const Vector3 *mins, const Vector3 *maxs, const Vector3 &end, const centity_t *passEntity, const cm_contents_t contentmask ) {
    cm_trace_t     trace = {
        .entityNumber = ENTITYNUM_NONE,
        .fraction = 0.0,
        .plane = {
            .normal = { 0.0f, 0.0f, 0.0f },
            .dist = 0.0f,
            .type = PLANE_NON_AXIAL,
            .signbits = 0,
        },

        .surface = &nulltexinfo.c,
        .material = &cm_default_material,

        .plane2 = {
            .normal = { 0.0f, 0.0f, 0.0f },
            .dist = 0.0f,
            .type = PLANE_NON_AXIAL,
            .signbits = 0,
        },
    };


    for ( int32_t i = 0; i < cl.numSolidEntities; i++ ) {
        // Acquire the entity state.
        centity_t *ent = cl.solidEntities[ i ];

        // Can't trace without proper data.
        if ( ent == nullptr ) {
            continue;
        }

        // Prevent tracing against non solids.
        if ( ent->current.solid == SOLID_NOT ) {
            continue;
        }
        // 
        //if ( !( contentmask & CONTENTS_PLAYERCLIP ) ) { // if ( !( contentmask & CONTENTS_PLAYER ) ) {
        //    continue;
        //}
		// Skip dead clients if not specified as a player in contentmask. (Assumed to be CONTENTS_DEADMONSTER)
        if ( ent->current.number <= cl.maxclients && !( contentmask & CONTENTS_PLAYER ) ) {
            continue;
        }


        // Prevent tracing against passEntity.
        if ( passEntity != nullptr && ent != nullptr && ( ent->current.number == passEntity->current.number ) ) {
            continue;
        }

        //// No need to continue if we're in all-solid.
        //if ( tr->allsolid ) {
        //    return;
        //}

        // Don't clip if we're owner of said entity.
        if ( passEntity ) {
            if ( ent->current.ownerNumber == passEntity->current.number ) {
                continue;    // Don't clip against own missiles.
            }
            if ( passEntity->current.ownerNumber == ent->current.number ) {
                continue;    // Don't clip against owner.
            }
        }



        //if ( !( contentmask & CONTENTS_DEADMONSTER )
        //    && ( ent->svFlags & SVF_DEADENTITY ) ) {
        //    continue;
        //}
        //if ( !( contentmask & CONTENTS_PROJECTILE )
        //    && ( ent->svFlags & SVF_PROJECTILE ) ) {
        //    continue;
        //}
        //if ( !( contentmask & CONTENTS_PLAYER ) {
        //    && ( ent->svFlags & SVF_PLAYER ) )
        //    continue;
        //}

        // BSP Brush Model Entity:
        mnode_t *headNode = CL_HullForEntity( ent );
        //if ( ent->current.solid == BOUNDS_BRUSHMODEL ) {
        //    // special value for bmodel
        //    cmodel = cl.model_clip[ ent->current.modelindex ];
        //    if ( !cmodel ) {
        //        continue;
        //    }
        //    headnode = cmodel->headnode;
        //    // Regular Entity, generate a temporary BSP Brush Box based on its mins/maxs:
        //} else {
        //    if ( ent->current.solid == SOLID_BOUNDS_OCTAGON ) {
        //        vec3_t _mins;
        //        vec3_t _maxs;
        //        MSG_UnpackSolidUint32( static_cast<cm_solid_t>(ent->current.bounds), _mins, _maxs );
        //        headnode = CM_HeadnodeForOctagon( &cl.collisionModel, _mins, _maxs, ent->current.hullContents );
        //    } else {
        //        headnode = CM_HeadnodeForBox( &cl.collisionModel, ent->mins, ent->maxs, ent->current.hullContents );
        //    }
        //}

        // Perform the BSP box sweep.
        CM_TransformedBoxTrace( &cl.collisionModel, &trace, start, end,
            mins, maxs, headNode, contentmask,
            &ent->current.origin.x, &ent->current.angles.x );

        // Determine clipped entity trace result.
        //CM_ClipEntity( &cl.collisionModel, tr, &trace, ent->current.number );
        #if 1
        if ( trace.allsolid || trace.fraction < tr->fraction ) {
            trace.entityNumber = ent->current.number;
            *tr = trace;
        } else if ( trace.startsolid ) {
            tr->startsolid = qtrue;
        }
        if ( tr->allsolid ) {
            return;
        }
        #endif
    }
}

/**
*   @brief  Substituting the below 'CL_PM_Trace' implementation:
**/
const cm_trace_t q_gameabi CL_Trace( const Vector3 &start, const Vector3 *mins, const Vector3 *maxs, const Vector3 &end, const centity_t *passEntity, const cm_contents_t contentmask ) {
    // Initialize to no collision for the initial trace.
    cm_trace_t trace = {
        .entityNumber = ENTITYNUM_NONE,
        .fraction = 1.0,
        .plane = {
            .normal = { 0.0f, 0.0f, 0.0f },
            .dist = 0.0f,
            .type = PLANE_NON_AXIAL,
            .signbits = 0,
        },

        .surface = &nulltexinfo.c,
        .material = &cm_default_material,

        .plane2 = {
            .normal = { 0.0f, 0.0f, 0.0f },
            .dist = 0.0f,
            .type = PLANE_NON_AXIAL,
            .signbits = 0,
        },
    };

    // Make sure we got world.
    if ( cl.collisionModel.cache ) {
        // First Clip to world.
        CM_BoxTrace( &cl.collisionModel, &trace, start, end, mins, maxs, cl.collisionModel.cache->nodes, contentmask );
 
        // Did we hit world? Set entity number to match with 'worldspawn' entity,
        // and return.
        //
	    // Otherwise we set the entity to 'None'(-1), and continue to test and clip to 
        // the remaining server entities.
        trace.entityNumber = trace.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
        //if ( trace.fraction == 0 || ( passEntity != nullptr && passEntity->current.number == ENTITYNUM_WORLD ) ) {
        //    return trace;		// blocked immediately by the world
        //}

	    // If we are not clipping to the world, and the trace fraction is 1.0,
        // test and clip to other solid entities.
        CL_ClipMoveToEntities( &trace, start, mins, maxs, end, passEntity, contentmask );
    }

    // Return trace.
    return trace;
}

/**
*   @brief  Will perform a clipping trace to the specified entity.
*           If clipEntity == nullptr, it'll perform a clipping trace against the World.
**/
const cm_trace_t q_gameabi CL_Clip( const Vector3 &start, const Vector3 *mins, const Vector3 *maxs, const Vector3 &end, const centity_t *clipEntity, const cm_contents_t contentmask ) {
    // Initialize to no collision for the initial trace.
    cm_trace_t trace = {
        .entityNumber = ENTITYNUM_NONE,
        .fraction = 1.0,
        .plane = {
            .normal = { 0.0f, 0.0f, 0.0f },
            .dist = 0.0f,
            .type = PLANE_NON_AXIAL,
            .signbits = 0,
        },

        .surface = &nulltexinfo.c,
        .material = &cm_default_material,

        .plane2 = {
            .normal = { 0.0f, 0.0f, 0.0f },
            .dist = 0.0f,
            .type = PLANE_NON_AXIAL,
            .signbits = 0,
        },
    };

    if ( cl.collisionModel.cache ) {
        // Clip against World:
        if ( clipEntity == nullptr || clipEntity == cl_entities ) {
            CM_BoxTrace( &cl.collisionModel, &trace, start, end, mins, maxs, cl.collisionModel.cache->nodes, contentmask );
        // Clip against clipEntity.
        } else {
            mnode_t *headNode = CL_HullForEntity( clipEntity );

            // Perform clip.
            if ( headNode != nullptr ) {
                CM_TransformedBoxTrace( &cl.collisionModel, &trace, start, end, mins, maxs, headNode, contentmask,
                    &clipEntity->current.origin.x, &clipEntity->current.angles.x );

                if ( trace.fraction < 1. ) {
                    trace.entityNumber = clipEntity->current.number;
                }
            }
        }
    }
    return trace;
}

/**
*   @brief  Player Move specific 'PointContents' implementation:
**/
const cm_contents_t q_gameabi CL_PointContents( const Vector3 &point ) {
    // Perform point contents against world.
    cm_contents_t contents = ( CM_PointContents( &cl.collisionModel, &point.x, cl.collisionModel.cache->nodes ) );
	// We hit world, so return contents.
	if ( contents != CONTENTS_NONE ) {
		return contents;
	}
	// If we hit CONTENTS_NONE then resume to test against frame's solid entities.
    for ( int32_t i = 0; i < cl.numSolidEntities; i++ ) {
        // Clip against all brush entity models.
        centity_t *ent = cl.solidEntities[ i ];

        //if ( ent->current.solid != BOUNDS_BRUSHMODEL ) { // special value for bmodel
        //    continue;
        //}

        //// Get model.
        //mmodel_t *cmodel = cl.model_clip[ ent->current.modelindex ];
        //if ( !cmodel ) {
        //    continue;
        //}
        // BSP Brush Model Entity:
        mnode_t *headNode = CL_HullForEntity( ent );
        //mnode_t *headNode = nullptr;
        //if ( ent->current.solid == BOUNDS_BRUSHMODEL ) {
        //    // special value for bmodel
        //    mmodel_t *cmodel = cl.model_clip[ ent->current.modelindex ];
        //    if ( !cmodel ) {
        //        continue;
        //    }
        //    headNode = cmodel->headnode;
        //    // Regular Entity, generate a temporary BSP Brush Box based on its mins/maxs:
        //} else {
        //    if ( ent->current.solid == SOLID_BOUNDS_OCTAGON ) {
        //        //headNode = CM_HeadnodeForOctagon( &cl.collisionModel, ent->mins, ent->maxs, ent->current.hullContents );
        //        vec3_t _mins;
        //        vec3_t _maxs;
        //        MSG_UnpackSolidUint32( static_cast<cm_solid_t>( ent->current.bounds ), _mins, _maxs );
        //        headNode = CM_HeadnodeForOctagon( &cl.collisionModel, _mins, _maxs, ent->current.hullContents );
        //    } else {
        //        headNode = CM_HeadnodeForBox( &cl.collisionModel, ent->mins, ent->maxs, ent->current.hullContents );
        //    }
        //    //headNode = CM_HeadnodeForBox( &cl.collisionModel, ent->mins, ent->maxs, ent->current.hullContents );
        //}

        // Might intersect, so do an exact clip.
        contents |= CM_TransformedPointContents( &cl.collisionModel, &point.x, headNode, &ent->current.origin.x, &ent->current.angles.x );
    }

    // Et voila.
    return contents;
}



