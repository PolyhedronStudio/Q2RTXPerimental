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

/**
*   @brief  Clips the trace to all entities currently in-frame.
**/
static void CL_ClipMoveToEntities( trace_t *tr, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const centity_t *passEntity, const contents_t contentmask ) {
    trace_t     trace = {};

    mnode_t *headnode = nullptr;
    mmodel_t *cmodel = nullptr;

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
        if ( ent->current.number <= cl.maxclients && !( contentmask & CONTENTS_PLAYER ) ) {
            continue;
        }


        // Prevent tracing against passEntity.
        if ( passEntity != nullptr && ent != nullptr && ( ent->current.number == passEntity->current.number ) ) {
            continue;
        }

        // Don't clip if we're owner of said entity.
        if ( passEntity ) {
            if ( ent->current.ownerNumber == passEntity->current.number ) {
                continue;    // Don't clip against own missiles.
            }
            if ( passEntity->current.ownerNumber == ent->current.number ) {
                continue;    // Don't clip against owner.
            }
        }

        // No need to continue if we're in all-solid.
        if ( tr->allsolid ) {
            return;
        }

        //if ( !( contentmask & CONTENTS_DEADMONSTER )
        //    && ( ent->svflags & SVF_DEADMONSTER ) ) {
        //    continue;
        //}
        //if ( !( contentmask & CONTENTS_PROJECTILE )
        //    && ( ent->svflags & SVF_PROJECTILE ) ) {
        //    continue;
        //}
        //if ( !( contentmask & CONTENTS_PLAYER ) {
        //    && ( ent->svflags & SVF_PLAYER ) )
        //    continue;
        //}

        // BSP Brush Model Entity:
        if ( ent->current.solid == PACKED_BSP ) {
            // special value for bmodel
            cmodel = cl.model_clip[ ent->current.modelindex ];
            if ( !cmodel ) {
                continue;
            }
            headnode = cmodel->headnode;
            // Regular Entity, generate a temporary BSP Brush Box based on its mins/maxs:
        } else {
            headnode = CM_HeadnodeForBox( &cl.collisionModel, ent->mins, ent->maxs, ent->current.hullContents );
        }

        // Perform the BSP box sweep.
        CM_TransformedBoxTrace( &cl.collisionModel, &trace, start, end,
            mins, maxs, headnode, contentmask,
            ent->current.origin, ent->current.angles );

        // Determine clipped entity trace result.
        CM_ClipEntity( &cl.collisionModel, tr, &trace, (struct edict_s *)ent );
    }
}

/**
*   @brief  Substituting the below 'CL_PM_Trace' implementation:
**/
const trace_t q_gameabi CL_Trace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const centity_t *passEntity, const contents_t contentmask ) {
    // Trace results.
    trace_t trace = {};

    // Clip against world.
    if ( cl.collisionModel.cache ) {
        CM_BoxTrace( &cl.collisionModel, &trace, start, end, mins, maxs, cl.collisionModel.cache->nodes, contentmask );
        trace.ent = (struct edict_s *)cl_entities;
        if ( trace.fraction == 0 ) {
            return trace; // Blocked by world.
        }

        // Clip to all other solid entities.
        CL_ClipMoveToEntities( &trace, start, mins, maxs, end, passEntity, contentmask );
    }

    // Return trace.
    return trace;
}

/**
*   @brief  Will perform a clipping trace to the specified entity.
*           If clipEntity == nullptr, it'll perform a clipping trace against the World.
**/
const trace_t q_gameabi CL_Clip( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const centity_t *clipEntity, const contents_t contentmask ) {
    // Trace results.
    trace_t trace = {};

    // Allow for point tracing in case mins or maxs is null.
    if ( !mins ) {
        mins = vec3_origin;
    }
    if ( !maxs ) {
        maxs = vec3_origin;
    }

    // Clip against World:
    if ( cl.collisionModel.cache ) {
        if ( clipEntity == nullptr || clipEntity == cl_entities ) {
            CM_BoxTrace( &cl.collisionModel, &trace, start, end, mins, maxs, cl.collisionModel.cache->nodes, contentmask );
            // Clip against Client Entity:
        } else {
            mnode_t *headNode = nullptr;

            // BSP Brush Model Entity:
            if ( clipEntity->current.solid == PACKED_BSP ) {
                // special value for bmodel
                mmodel_t *cmodel = cl.model_clip[ clipEntity->current.modelindex ];
                if ( !cmodel ) {
                    return trace;
                }
                headNode = cmodel->headnode;
                // Regular Entity, generate a temporary BSP Brush Box based on its mins/maxs:
            } else {
                headNode = CM_HeadnodeForBox( &cl.collisionModel, clipEntity->mins, clipEntity->maxs, clipEntity->current.hullContents );
            }

            // Perform clip.
            trace_t tr;
            CM_TransformedBoxTrace( &cl.collisionModel, &tr, start, end, mins, maxs, headNode, contentmask,
                clipEntity->current.origin, clipEntity->current.angles );

            // Determine clipped entity trace result.
            CM_ClipEntity( &cl.collisionModel, &trace, &tr, (struct edict_s *)clipEntity );
        }
    }
    return trace;
}

/**
*   @brief  Player Move specific 'PointContents' implementation:
**/
const contents_t q_gameabi CL_PointContents( const vec3_t point ) {
    // Perform point contents against world.
    contents_t contents = static_cast<contents_t>( CM_PointContents( &cl.collisionModel, point, cl.collisionModel.cache->nodes ) );

    for ( int32_t i = 0; i < cl.numSolidEntities; i++ ) {
        // Clip against all brush entity models.
        centity_t *ent = cl.solidEntities[ i ];

        //if ( ent->current.solid != PACKED_BSP ) { // special value for bmodel
        //    continue;
        //}

        //// Get model.
        //mmodel_t *cmodel = cl.model_clip[ ent->current.modelindex ];
        //if ( !cmodel ) {
        //    continue;
        //}
        // BSP Brush Model Entity:
        mnode_t *headNode = nullptr;
        if ( ent->current.solid == PACKED_BSP ) {
            // special value for bmodel
            mmodel_t *cmodel = cl.model_clip[ ent->current.modelindex ];
            if ( !cmodel ) {
                continue;
            }
            headNode = cmodel->headnode;
            // Regular Entity, generate a temporary BSP Brush Box based on its mins/maxs:
        } else {
            headNode = CM_HeadnodeForBox( &cl.collisionModel, ent->mins, ent->maxs, ent->current.hullContents );
        }

        // Might intersect, so do an exact clip.
        contents = static_cast<contents_t>( contents | CM_TransformedPointContents( &cl.collisionModel, point, headNode, ent->current.origin, ent->current.angles ) );
    }

    // Et voila.
    return contents;
}



