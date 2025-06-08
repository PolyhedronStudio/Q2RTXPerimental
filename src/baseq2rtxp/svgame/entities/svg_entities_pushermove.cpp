/********************************************************************
*
*
*	ServerGame: Support Routines for implementing brush entity
*				movement.
*
*				(Changes in origin using velocity.)
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/svg_base_edict.h"
#include "svgame/entities/svg_pushmove_edict.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"



/**
*
*
*
*   Lock/UnLock:
*
*
*
**/
/**
*   @brief  Set button lock state.
*   @param  isLocked
*           If true: Locks the button, thereby disabling it from being able to change state.
*           If false: UnLocks the button, thereby enabling it from being able to change state.
**/
void svg_pushmove_edict_t::SetLockState( const bool lockButton ) {
    if ( lockButton ) {
        // Last but not least, lock its state.
        pushMoveInfo.lockState.isLocked = true;
    } else {
        // Last but not least, unlock its state.
        pushMoveInfo.lockState.isLocked = false;
    }
}



/**
*
*
*
*   MoveWith Entities:
*
*
**/
/**
*   @brief  Reposition 'moveWith' entities to their parent entity its made movement.
**/
void SVG_PushMove_UpdateMoveWithEntities() {
    //
    // Readjust specific mover entities if necessary.
    //
    for ( int32_t i = 0; i < game.num_movewithEntityStates; i++ ) {
        // Parent mover.
        svg_base_edict_t *parentMover = nullptr;
        if ( game.moveWithEntities[ i ].parentNumber > 0 && game.moveWithEntities[ i ].parentNumber < MAX_EDICTS ) {
            parentMover = g_edict_pool.EdictForNumber( game.moveWithEntities[ i ].parentNumber );//&g_edicts[ game.moveWithEntities[ i ].parentNumber ];
        }

        if ( parentMover && parentMover->inuse && ( parentMover->movetype == MOVETYPE_PUSH || parentMover->movetype == MOVETYPE_STOP ) ) {
            Vector3 parentAnglesDelta = parentMover->s.angles - parentMover->lastAngles;
            Vector3 parentOriginDelta = parentMover->s.origin - parentMover->lastOrigin;

            Vector3 parentVForward, parentVRight, parentVUp;
            QM_AngleVectors( parentAnglesDelta, &parentVForward, &parentVRight, &parentVUp );
            //parentVRight = QM_Vector3Negate( parentVRight );

            //        Vector3 delta_angles, forward, right, up;
            //        VectorSubtract( moveWithEntity->s.angles, ent->moveWith.originAnglesOffset, delta_angles );
            //        AngleVectors( delta_angles, forward, right, up );
            //        VectorNegate( right, right );

            // Iterate to find the child movers to adjust to parent.
            for ( int32_t j = 0; j < game.num_movewithEntityStates; j++ ) {
                // Child mover.
                svg_base_edict_t *childMover = nullptr;
                if ( game.moveWithEntities[ j ].parentNumber == parentMover->s.number &&
                    game.moveWithEntities[ j ].childNumber > 0 && game.moveWithEntities[ j ].childNumber < MAX_EDICTS ) {
                    //childMover = &g_edicts[ game.moveWithEntities[ j ].childNumber ];
                    childMover = g_edict_pool.EdictForNumber( game.moveWithEntities[ j ].childNumber );
                }
                if ( childMover && childMover->inuse 
                    && ( 
                        //// Allow Triggers, 
                        //childMover->solid == SOLID_TRIGGER ||
                        // And Pushers.
                        childMover->movetype == MOVETYPE_PUSH || childMover->movetype == MOVETYPE_STOP ) ) {
                    SVG_MoveWith_AdjustToParent(
                        parentOriginDelta, parentAnglesDelta,
                        parentVUp, parentVRight, parentVForward,
                        parentMover, childMover
                    );
                }
            }
        }
    }
}