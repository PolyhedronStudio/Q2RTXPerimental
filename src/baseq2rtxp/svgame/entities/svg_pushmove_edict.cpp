/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_door'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_door.h"
#include "svgame/entities/func/svg_func_door_rotating.h"



/**
*
*
*
*   PushMove - EndMove CallBacks:
*
*
*
**/
/**
*	@brief  Stub.
**/
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_pushmove_edict_t, onCloseEndMove )( svg_pushmove_edict_t *self ) -> void { }
/**
*	@brief  Stub.
**/
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_pushmove_edict_t, onOpenEndMove )( svg_pushmove_edict_t *self ) -> void { }


/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_MoveBegin )( svg_pushmove_edict_t *ent ) -> void {
    if ( ( ent->pushMoveInfo.speed * gi.frame_time_s ) >= ent->pushMoveInfo.remaining_distance ) {
        SVG_PushMove_MoveFinal( ent );
        return;
    }
    // Recalculate velocity into current direction.
    ent->velocity = ent->pushMoveInfo.dir * ent->pushMoveInfo.speed;

    //if ( ent->targetEntities.movewith ) {
    //    VectorAdd( ent->targetEntities.movewith->velocity, ent->velocity, ent->velocity );
    //    ent->pushMoveInfo.remaining_distance -= ent->pushMoveInfo.speed * gi.frame_time_s;
    //    ent->nextthink = level.time + FRAME_TIME_S;
    //    ent->think = SVG_PushMove_MoveBegin;
    //} else {
    const float frames = floor( ( ent->pushMoveInfo.remaining_distance / ent->pushMoveInfo.speed ) / gi.frame_time_s );
    ent->pushMoveInfo.remaining_distance -= frames * ent->pushMoveInfo.speed * gi.frame_time_s;
    ent->nextthink = level.time + ( FRAME_TIME_S * frames );
    ent->SetThinkCallback( SVG_PushMove_MoveFinal );
    //}

    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_MoveDone )( svg_pushmove_edict_t *ent ) -> void {
    // WID: MoveWith: Clear last velocity also.
    ent->velocity = {};
    // Fire the endMoveCallback.
    if ( ent->pushMoveInfo.endMoveCallback ) {
        ent->pushMoveInfo.endMoveCallback( ent );
    }

    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}
/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_MoveFinal )( svg_pushmove_edict_t *ent ) -> void {
    if ( ent->pushMoveInfo.remaining_distance == 0 ) {
        SVG_PushMove_MoveDone( ent );
        return;
    }

    // [Paril-KEX] use exact remaining distance
    ent->velocity = ( ent->pushMoveInfo.dest - ent->s.origin ) * ( 1.f / gi.frame_time_s );
    //if ( ent->targetEntities.movewith ) {
    //    VectorAdd( ent->targetEntities.movewith->velocity, ent->velocity, ent->velocity );
    //}

    ent->SetThinkCallback( &svg_pushmove_edict_t::onThink_MoveDone );
    ent->nextthink = level.time + FRAME_TIME_S;
    //if ( ent->targetEntities.movewith_next && ( ent->targetEntities.movewith_next->targetEntities.movewith == ent ) ) {
    //    SVG_MoveWith_SetChildEntityMovement( ent );
    //}
}
