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

#include "sharedgame/sg_entity_effects.h"



/**
*	@brief	Will setup the refresh entity for the ET_GIB centity with the newState.
**/
void CLG_PacketEntity_AddGib( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    // Apply frame.
    refreshEntity->frame = newState->frame;
    // Setup the old frame.
    refreshEntity->oldframe = packetEntity->prev.frame;
    // Backlerp.
    refreshEntity->backlerp = 1.0f - clgi.client->lerpfrac;

    // Set model.
    refreshEntity->skinnum = newState->skinnum;
    refreshEntity->skin = 0;
    refreshEntity->model = clgi.client->model_draw[ newState->modelindex ];
    // Render effects.
    refreshEntity->flags |= newState->renderfx;

    // Lerp Origin:
    Vector3 cent_origin = QM_Vector3Lerp( packetEntity->prev.origin, packetEntity->current.origin, clgi.client->lerpfrac );
    VectorCopy( cent_origin, refreshEntity->origin );
    VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );

    // For General Rotate: (Some bonus items auto-rotate.)
    if ( newState->effects & EF_ROTATE ) {
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
    if ( newState->effects & ~EF_ROTATE ) {
        //if ( newState->effects & EF_GIB ) {
        CLG_DiminishingTrail( packetEntity->lerp_origin, refreshEntity->origin, packetEntity, newState->effects | EF_GIB );
    }
    
    // Add refresh entity to scene.
    clgi.V_AddEntity( refreshEntity );
     
    // skip:
    VectorCopy( refreshEntity->origin, packetEntity->lerp_origin );
}