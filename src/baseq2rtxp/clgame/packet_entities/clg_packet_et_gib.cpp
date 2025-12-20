/********************************************************************
*
*
*	ClientGame: 'ET_GIB' Packet Entities.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_temp_entities.h"

#include "sharedgame/sg_entity_flags.h"



/**
*	@brief	Will setup the refresh entity for the ET_GIB centity with the nextState.
**/
void CLG_PacketEntity_AddGib( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *nextState ) {
    // Apply frame.
    refreshEntity->frame = nextState->frame;
    // Setup the old frame.
    refreshEntity->oldframe = packetEntity->prev.frame;
    // Backlerp.
    refreshEntity->backlerp = 1.0f - clgi.client->lerpfrac;

    // Set model.
    refreshEntity->skinnum = nextState->skinnum;
    refreshEntity->skin = 0;
    refreshEntity->model = clgi.client->model_draw[ nextState->modelindex ];
    // Render entityFlags.
    refreshEntity->flags |= nextState->renderfx;

    // Lerp Origin:
    Vector3 cent_origin = QM_Vector3Lerp( packetEntity->prev.origin, packetEntity->current.origin, clgi.client->lerpfrac );
    VectorCopy( cent_origin, refreshEntity->origin );
    VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );

    // For General Rotate: (Some bonus items auto-rotate.)
    if ( nextState->entityFlags & EF_ROTATE ) {
        // Bonus items rotate at a fixed rate.
        const float autorotate = QM_AngleMod( clgi.client->time * BASE_FRAMETIME_1000 );//AngleMod(clgi.client->time * 0.1f); // WID: 40hz: Adjusted.

        refreshEntity->angles[ 0 ] = 0;
        refreshEntity->angles[ 1 ] = autorotate;
        refreshEntity->angles[ 2 ] = 0;
        // Reguler entity angle interpolation:
    } else {
        LerpAngles( packetEntity->prev.angles, packetEntity->current.angles, clgi.client->lerpfrac, refreshEntity->angles );
    }

    // If no rotation flag is set, add specified trail flags.
    // WID: Why not? Let's just do this.
    if ( nextState->entityFlags & ~EF_ROTATE ) {
        //if ( nextState->entityFlags & EF_GIB ) {
        CLG_FX_DiminishingTrail( packetEntity->lerp_origin, refreshEntity->origin, packetEntity, nextState->entityFlags | EF_GIB );
    }
    
    // Add refresh entity to scene.
    clgi.V_AddEntity( refreshEntity );
     
    // skip:
    VectorCopy( refreshEntity->origin, packetEntity->lerp_origin );
}