/********************************************************************
*
*
*	ClientGame: 'ET_BEAM' Packet Entities.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_temp_entities.h"

/**
*	@brief	Will setup the refresh entity for the ET_BEAM centity with the nextState.
**/
void CLG_PacketEntity_AddBeam( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *nextState ) {
    refreshEntity->frame = nextState->frame;
    // Setup the old frame.
    refreshEntity->oldframe = packetEntity->prev.frame;
    // Backlerp.
    refreshEntity->backlerp = 1.0f - clgi.client->lerpfrac;

    // Set model.
    refreshEntity->model = 0;
    // The four beam colors are encoded in 32 bits of skinnum.
    refreshEntity->alpha = 0.30f;
    refreshEntity->skinnum = ( nextState->skinnum >> ( ( irandom( 4 ) ) * 8 ) ) & 0xff;

    refreshEntity->flags = RF_BEAM | nextState->renderfx;
    //if ( refreshEntity->model == precache.models.laser ) {
    //    refreshEntity->flags |= RF_NOSHADOW;
    //}

    // Interpolate start and end points for beams.
    Vector3 cent_origin = QM_Vector3Lerp( packetEntity->prev.origin, packetEntity->current.origin, clgi.client->lerpfrac );
    VectorCopy( cent_origin, refreshEntity->origin );
    Vector3 cent_old_origin = QM_Vector3Lerp( packetEntity->prev.old_origin, packetEntity->current.old_origin, clgi.client->lerpfrac );
    VectorCopy( cent_old_origin, refreshEntity->oldorigin );

    // Reguler entity angle interpolation:
    LerpAngles( packetEntity->prev.angles, packetEntity->current.angles, clgi.client->lerpfrac, refreshEntity->angles );

    // Add entity to refresh list
    clgi.V_AddEntity( refreshEntity );

    // skip:
    VectorCopy( refreshEntity->origin, packetEntity->lerpOrigin );
}