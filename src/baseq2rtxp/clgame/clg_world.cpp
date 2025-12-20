/********************************************************************
*
*
*	ClientGame: (Entity/Player State) -Events:
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_precache.h"
#include "clgame/clg_world.h"


/**
*
*
*
*   Helper Functions:
*
*
*
**/
/**
*   @brief  Clips the trace to all entities currently in-frame.
**/
static void ClipTraceMoveToEntities( cm_trace_t *tr, const Vector3 &start, const Vector3 *mins, const Vector3 *maxs, const Vector3 &end, const centity_t *passEntity, const cm_contents_t contentMask ) {
	cm_trace_t trace = cm_trace_t();
        
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	// Iterate all solid entities in the current frame.
    for ( int32_t i = 0; i < game.frameEntities.numSolids; i++ ) {
        // Acquire the entity state.
        centity_t *ent = game.frameEntities.solids[ i ];

        // Can't trace without proper data.
        if ( ent == nullptr ) {
            continue;
        }

        // Prevent tracing against non solids.
        if ( ent->current.solid == SOLID_NOT ) {
            continue;
        }
        // 
        //if ( !( contentMask & CONTENTS_PLAYERCLIP ) ) { // if ( !( contentMask & CONTENTS_PLAYER ) ) {
        //    continue;
        //}
        // Skip dead clients if not specified as a player in contentMask. (Assumed to be CONTENTS_DEADMONSTER)
        if ( ent->current.number <= game.maxclients && !( contentMask & CONTENTS_PLAYER ) ) {
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
        
        //if ( !( contentMask & CONTENTS_DEADMONSTER )
        //    && ( ent->svFlags & SVF_DEADENTITY ) ) {
        //    continue;
        //}
        //if ( !( contentMask & CONTENTS_PROJECTILE )
        //    && ( ent->svFlags & SVF_PROJECTILE ) ) {
        //    continue;
        //}
        //if ( !( contentMask & CONTENTS_PLAYER ) {
        //    && ( ent->svFlags & SVF_PLAYER ) )
        //    continue;
        //}

        // BSP Brush Model Entity:
        mnode_t *headNode = clgi.GetEntityHullNode( ent );

        // Perform the BSP box sweep.
        trace = clgi.CM_TransformedBoxTrace( 
			&start, &end,
            mins, maxs, 
			headNode, contentMask,
            &ent->current.origin, &ent->current.angles 
		);

        // Determine clipped entity trace result.
        //CM_ClipEntity( &cl.collisionModel, tr, &trace, ent->current.number );
        #if 1
		// If we hit something closer, update trace.
        if ( trace.allsolid || trace.fraction < tr->fraction ) {
            trace.entityNumber = ent->current.number;
            *tr = trace;
			// Otherwise, if we started in solid, mark it.
        } else if ( trace.startsolid ) {
            tr->startsolid = true;
        }
		// No need to continue  if we're in an all-solid.
        if ( tr->allsolid ) {
            return;
        }
        #endif
    }
}

/**
*   @brief  Substituting the below 'CL_PM_Trace' implementation:
**/
const cm_trace_t CLG_Trace( const Vector3 &start, const Vector3 *mins, const Vector3 *maxs, const Vector3 &end, const centity_t *passEntity, const cm_contents_t contentMask ) {
    // Initialize to no collision for the initial trace.
	cm_trace_t trace = cm_trace_t();
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	// First Clip to world.
	trace = clgi.CM_BoxTrace( &start, &end, mins, maxs, clgi.GetEntityHullNode( nullptr ), contentMask );

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
    ClipTraceMoveToEntities( &trace, start, mins, maxs, end, passEntity, contentMask );

    // Return trace.
    return trace;
}

/**
*   @brief  Will perform a clipping trace to the specified entity.
*           If clipEntity == nullptr, it'll perform a clipping trace against the World.
**/
const cm_trace_t CLG_Clip( const Vector3 &start, const Vector3 *mins, const Vector3 *maxs, const Vector3 &end, const centity_t *clipEntity, const cm_contents_t contentMask ) {
    // Initialize to no collision for the initial trace.
	cm_trace_t trace = cm_trace_t();
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

    // Clip against World:
    if ( clipEntity == nullptr || clipEntity == clg_entities ) {
        trace = clgi.CM_BoxTrace( &start, &end, mins, maxs, clgi.GetEntityHullNode( nullptr ), contentMask);
        // Clip against clipEntity.
    } else {
		// Get the entity's hull.
        mnode_t *headNode = clgi.GetEntityHullNode( clipEntity );

        // Perform clip.
        if ( headNode != nullptr ) {
            trace = clgi.CM_TransformedBoxTrace( 
				&start, &end, 
				mins, maxs, 
				headNode, contentMask,
                &clipEntity->current.origin, &clipEntity->current.angles
			);

            if ( trace.fraction < 1. ) {
                trace.entityNumber = clipEntity->current.number;
            }
        }
    }

    return trace;
}

/**
*   @brief  Player Move specific 'PointContents' implementation:
**/
const cm_contents_t CLG_PointContents( const Vector3 &point ) {
    // Perform point contents against world.
    cm_contents_t contents = clgi.CM_PointContents( &point, clgi.GetEntityHullNode( nullptr ) );
    // We hit world, so return contents.
    if ( contents != CONTENTS_NONE ) {
        return contents;
    }
    // If we hit CONTENTS_NONE then resume to test against frame's solid entities.
    for ( int32_t i = 0; i < game.frameEntities.numSolids; i++ ) {
        // Clip against all brush entity models.
        centity_t *ent = game.frameEntities.solids[ i ];

        // BSP Brush Model Entity:
        mnode_t *headNode = clgi.GetEntityHullNode( ent );

        // Might intersect, so do an exact clip.
        contents |= clgi.CM_TransformedPointContents( &point, headNode, &ent->current.origin, &ent->current.angles );
    }

    // Et voila.
    return contents;
}



