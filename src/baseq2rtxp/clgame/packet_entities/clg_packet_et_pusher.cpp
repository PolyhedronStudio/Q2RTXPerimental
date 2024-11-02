/********************************************************************
*
*
*	ClientGame: 'ET_PUSHER' Packet Entities.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_temp_entities.h"



/**
*	@brief	Will setup the refresh entity for the ET_PUSHER centity with the newState.
**/
void CLG_PacketEntity_AddPusher( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    // Brush models can auto animate their frames.
    int64_t autoanim = 2 * clgi.client->time / 1000;

    if ( newState->effects & EF_ANIM01 ) {
        refreshEntity->frame = autoanim & 1;
    } else if ( newState->effects & EF_ANIM23 ) {
        refreshEntity->frame = 2 + ( autoanim & 1 );
    } else if ( newState->effects & EF_ANIM_ALL ) {
        refreshEntity->frame = autoanim;
    } else if ( newState->effects & EF_ANIM_ALLFAST ) {
        refreshEntity->frame = clgi.client->time / BASE_FRAMETIME; // WID: 40hz: Adjusted. clgi.client->time / 100;
    } else if ( newState->effects & EF_ANIM_CYCLE2_2HZ ) {
        refreshEntity->frame = newState->frame + ( autoanim & 1 );
    }

    // Setup the old frame.
    refreshEntity->oldframe = refreshEntity->frame;// packetEntity->prev.frame;
    // Backlerp.
    refreshEntity->backlerp = 1.0f -clgi.client->lerpfrac;

    // Set skin and model.
    //refreshEntity->skinnum = newState->skinnum;
    //refreshEntity->skin = 0;
    refreshEntity->model = clgi.client->model_draw[ newState->modelindex ];
    // Render effects.
    refreshEntity->flags = newState->renderfx;

    // Lerp Origin:
    Vector3 cent_origin = QM_Vector3Lerp( packetEntity->prev.origin, packetEntity->current.origin, clgi.client->lerpfrac );
    VectorCopy( cent_origin, refreshEntity->origin );
    VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );

    // Lerp Angles:
    LerpAngles( packetEntity->prev.angles, packetEntity->current.angles, clgi.client->lerpfrac, refreshEntity->angles );

    // Add entity to refresh list
    clgi.V_AddEntity( refreshEntity );

    // skip:
    VectorCopy( refreshEntity->origin, packetEntity->lerp_origin );
}
