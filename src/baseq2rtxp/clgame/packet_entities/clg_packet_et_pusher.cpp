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

#include "sharedgame/sg_entity_flags.h"



/**
*   @brief  Adds trail entityFlags to the entity.
**/
static void CLG_PacketEntity_AddTrailEffects( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState, const uint32_t entityFlags ) {
    // If no rotation flag is set, add specified trail flags.
    // WID: Why not? Let's just do this.
    //if ( entityFlags & ~EF_ROTATE ) {
    if ( entityFlags & EF_GIB ) {
        CLG_FX_DiminishingTrail( packetEntity->lerp_origin, refreshEntity->origin, packetEntity, entityFlags );
    }
    //}
}

/**
*	@brief	Will setup the refresh entity for the ET_PUSHER centity with the newState.
**/
void CLG_PacketEntity_AddPusher( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    // Brush models can auto animate their frames.
    int64_t autoanim = 2 * clgi.client->time / 1000;

    if ( newState->entityFlags & EF_ANIM01 ) {
        refreshEntity->frame = autoanim & 1;
        refreshEntity->oldframe = packetEntity->prev.frame;

        //clgi.Print( PRINT_DEVELOPER, "%s: EF_ANIM01 refreshEntity->frame(%d), refreshEntity->oldframe(%d)\n",
        //    __func__, refreshEntity->frame, refreshEntity->oldframe );
    } else if ( newState->entityFlags & EF_ANIM23 ) {
        refreshEntity->frame = 2 + ( autoanim & 1 );
        refreshEntity->oldframe = packetEntity->prev.frame;
        //clgi.Print( PRINT_DEVELOPER, "%s: EF_ANIM23 refreshEntity->frame(%d), refreshEntity->oldframe(%d)\n",
        //    __func__, refreshEntity->frame, refreshEntity->oldframe );
    } else if ( newState->entityFlags & EF_ANIM_ALL ) {
        refreshEntity->frame = autoanim;
        refreshEntity->oldframe = packetEntity->prev.frame;
    } else if ( newState->entityFlags & EF_ANIM_ALLFAST ) {
        refreshEntity->frame = clgi.client->time / BASE_FRAMETIME; // WID: 40hz: Adjusted. clgi.client->time / 100;
        refreshEntity->oldframe = packetEntity->prev.frame;
    } else if ( newState->entityFlags & EF_ANIM_CYCLE2_2HZ ) {
        refreshEntity->frame = newState->frame + ( autoanim & 1 );
        refreshEntity->oldframe = packetEntity->prev.frame;
    // For hard set animated texture chain frames.
    } else {
        refreshEntity->frame = newState->frame;
        refreshEntity->oldframe = packetEntity->prev.frame;
    }
    // Backlerp.
    refreshEntity->backlerp = 1.0f - clgi.client->lerpfrac;

    // Set skin and model.
    //refreshEntity->skinnum = newState->skinnum;
    //refreshEntity->skin = 0;
    refreshEntity->model = clgi.client->model_draw[ newState->modelindex ];
    // Render entityFlags.
    refreshEntity->flags = newState->renderfx;

    // Lerp Origin:
    Vector3 cent_origin = QM_Vector3Lerp( packetEntity->prev.origin, packetEntity->current.origin, clgi.client->lerpfrac );
    VectorCopy( cent_origin, refreshEntity->origin );
    VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );

    // Lerp Angles:
    LerpAngles( packetEntity->prev.angles, packetEntity->current.angles, clgi.client->lerpfrac, refreshEntity->angles );

    // Add automatic particle trails
    CLG_PacketEntity_AddTrailEffects( packetEntity, refreshEntity, newState, newState->entityFlags );

    // Add entity to refresh list
    clgi.V_AddEntity( refreshEntity );

    #if 0
    // Render entityFlags.
    if ( newState->entityFlags & EF_COLOR_SHELL ) {
        refreshEntity->flags |= RF_SHELL_GREEN | RF_TRANSLUCENT;
        refreshEntity->alpha = 0.3;
        refreshEntity->scale = 1.03125;
        clgi.V_AddEntity( refreshEntity );
    }
    #endif
    // skip:
    VectorCopy( refreshEntity->origin, packetEntity->lerp_origin );
}
