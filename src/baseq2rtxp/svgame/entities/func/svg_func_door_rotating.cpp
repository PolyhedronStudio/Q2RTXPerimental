/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_door_rotating'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_door.h"
#include "svgame/entities/func/svg_func_door_rotating.h"



/*QUAKED func_door_rotating (0 .5 .8) ? START_OPEN REVERSE CRUSHER NOMONSTER ANIMATED TOGGLE X_AXIS Y_AXIS
TOGGLE causes the door to wait in both the start and end states for a trigger event.

START_OPEN  the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
NOMONSTER   monsters will not trigger this door

You need to have an origin brush as part of this entity.  The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"distance" is how many degrees the door will be rotated.
"speed" determines how fast the door moves; default value is 100.

REVERSE will cause the door to rotate in the opposite direction.

"message"   is printed when the door is touched if it is a trigger door and it hasn't been fired yet
"angle"     determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"health"    if set, door must be shot open
"speed"     movement speed (100 default)
"wait"      wait before returning (3 default, -1 = never return)
"dmg"       damage to inflict when blocked (2 default)
"sounds"
1)  silent
2)  light
3)  medium
4)  heavy
*/

/**
*	@brief
**/
extern void door_postspawn( svg_base_edict_t *self );

/**
*	@brief
**/
void SVG_Util_SetMoveDir( vec3_t angles, Vector3 &movedir );
void SP_func_door_rotating( svg_base_edict_t *ent ) {
    //vec3_t  abs_movedir;

    if ( ent->sounds != 1 ) {
        ent->pushMoveInfo.sounds.start = gi.soundindex( "doors/door_start_01.wav" );
        ent->pushMoveInfo.sounds.middle = gi.soundindex( "doors/door_mid_01.wav" );
        ent->pushMoveInfo.sounds.end = gi.soundindex( "doors/door_end_01.wav" );

        ent->pushMoveInfo.lockState.lockedSound = gi.soundindex( "misc/door_locked.wav" );
        ent->pushMoveInfo.lockState.lockingSound = gi.soundindex( "misc/door_locking.wav" );
        ent->pushMoveInfo.lockState.unlockingSound = gi.soundindex( "misc/door_unlocking.wav" );
    }

    // In case we ever go Q2 Rerelease movers I guess.
    #if 0
    const bool isBothDirections = SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_BOTH_DIRECTIONS );
    if ( isBothDirections ) {
        SVG_Util_SetMoveDir( ent->s.angles, ent->pushMoveInfo.dir);
    }
    #endif

    // Clear the angles.
    VectorClear( ent->s.angles );

    // Set the axis of rotation.
    ent->movedir = QM_Vector3Zero();
    if ( ent->spawnflags & DOOR_SPAWNFLAG_X_AXIS ) {
        ent->movedir[ 2 ] = 1.0f;
    } else if ( ent->spawnflags & DOOR_SPAWNFLAG_Y_AXIS ) {
        ent->movedir[ 0 ] = 1.0f;
    } else {// Z_AXIS
        ent->movedir[ 1 ] = 1.0f;
    }

    // Check for reverse rotation.
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_REVERSE ) ) {
        ent->movedir = QM_Vector3Negate( ent->movedir );
    }

    // Default distance to 90 degrees if not set.
    if ( !st.distance ) {
        gi.dprintf( "%s at %s with no distance set\n", (const char *)ent->classname, vtos( ent->s.origin ) );
        st.distance = 90;
    }

    // Determine move angles.
    const float distance = st.distance; // ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_REVERSE ) ? -st.distance : st.distance );
    ent->angles1 = ent->s.angles;
    VectorMA( ent->s.angles, distance, ent->movedir, ent->angles2 );
    ent->pushMoveInfo.distance = distance;

    //SVG_Util_SetMoveDir( ent->s.angles, ent->movedir );
    ent->movetype = MOVETYPE_PUSH;
    ent->solid = SOLID_BSP;
    ent->s.renderfx |= RF_BRUSHTEXTURE_SET_FRAME_INDEX;
    ent->s.entityType = ET_PUSHER;
    ent->svflags |= SVF_DOOR;
    // BSP Model, or otherwise, specified external model.
    gi.setmodel( ent, ent->model );

    // PushMove defaults:
    if ( !ent->speed ) {
        ent->speed = 100;
    }
    if ( !ent->accel ) {
        ent->accel = ent->speed;
    }
    if ( !ent->decel ) {
        ent->decel = ent->speed;
    }
    if ( !st.lip ) {
        st.lip = 8;
    }
    // Trigger defaults:
    if ( !ent->wait ) {
        ent->wait = 3;
    }
    if ( !ent->dmg ) {
        ent->dmg = 2;
    }

    // Callbacks.
    ent->SetPostSpawnCallback( door_postspawn );
    ent->SetBlockedCallback( door_blocked );
    ent->SetTouchCallback( door_touch );
    ent->SetUseCallback( door_use );
    //ent->pain = door_pain;
    ent->SetOnSignalInCallback( door_onsignalin );

    // Calculate absolute move distance to get from pos1 to pos2.
    ent->pos1 = ent->s.origin;
    ent->pos2 = ent->s.origin;

    // if it starts open, switch the positions
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_START_OPEN ) ) {
        VectorCopy( ent->angles2, ent->s.angles );
        //ent->angles2 = ent->angles1;
        //ent->angles1 = -Vector3(ent->s.angles);
        //ent->movedir = QM_Vector3Negate( ent->movedir );
        ent->pushMoveInfo.state = DOOR_STATE_OPENED;
    } else {
        // Initial closed state.
        ent->pushMoveInfo.state = DOOR_STATE_CLOSED;
    }

    // Used for condition checking, if we got a damage activating door we don't want to have it support pressing.
    const bool damageActivates = SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_DAMAGE_ACTIVATES );
    // Health trigger based door:
    if ( damageActivates ) {
        // Spawn open, does not imply death, to do so, spawn open, set max_health for revival, and health to 0 for death.
        if ( ent->max_health > 0 && ent->health <= 0 ) {
            // Don't allow damaging.
            ent->takedamage = DAMAGE_NO;
            ent->lifeStatus = LIFESTATUS_DEAD;
            // Die callback.
            ent->SetDieCallback( door_killed );
            ent->SetPainCallback( door_pain );
        } else {
            // Set max health, in case it wasn't set.also used to reinitialize the door to revive.
            if ( !ent->health ) {
                ent->health = ent->max_health;
            }
            // Let it take damage.
            ent->takedamage = DAMAGE_YES;
            ent->lifeStatus = LIFESTATUS_ALIVE;
            // Die callback.
            ent->SetDieCallback( door_killed );
            ent->SetPainCallback( door_pain );
        }

        // Apply next think time and method.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->SetThinkCallback( SVG_PushMove_Think_CalculateMoveSpeed );
    // Touch based door:DOOR_SPAWNFLAG_DAMAGE_ACTIVATES
    } else if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_TOUCH_AREA_TRIGGERED ) ) {
        // Set its next think to create the trigger area.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->SetThinkCallback( Think_SpawnDoorTrigger );
    } else {
        // Apply next think time and method.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->SetThinkCallback( SVG_PushMove_Think_CalculateMoveSpeed );

        // This door is only toggled, never untoggled, by each (+usetarget) interaction.
        if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_PRESSABLE ) ) {
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_PRESS;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            ent->SetTouchCallback( nullptr );
            ent->SetUseCallback( door_use );
            // This door is dispatches untoggle/toggle callbacks by each (+usetarget) interaction, based on its usetarget state.
        } else if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_TOGGLEABLE ) ) {
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_TOGGLE;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            ent->SetTouchCallback( nullptr );
            ent->SetUseCallback( door_use );
        } else if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_HOLDABLE ) ) {
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_CONTINUOUS;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            ent->SetTouchCallback( nullptr );
            ent->SetUseCallback( door_use );
        }

        // Is usetargetting disabled by default?
        if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_DISABLED ) ) {
            ent->useTarget.flags |= ENTITY_USETARGET_FLAG_DISABLED;
        }
    }

    // Copy the calculated info into the pushMoveInfo state struct.
    ent->pushMoveInfo.speed = ent->speed;
    ent->pushMoveInfo.accel = ent->accel;
    ent->pushMoveInfo.decel = ent->decel;
    ent->pushMoveInfo.wait = ent->wait;
    
    #if 1
    ent->pushMoveInfo.startOrigin = ent->s.origin;
    ent->pushMoveInfo.startAngles = ent->angles1;
    ent->pushMoveInfo.endOrigin = ent->s.origin;
    ent->pushMoveInfo.endAngles = ent->angles2;
    #else
    // For PRESSED: pos1 = start, pos2 = end.
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_START_OPEN ) ) {
        ent->pushMoveInfo.state = DOOR_STATE_OPENED;
        ent->pushMoveInfo.startOrigin = ent->s.origin;
        ent->pushMoveInfo.startAngles = ent->angles1;
        ent->pushMoveInfo.endOrigin = ent->s.origin;
        ent->pushMoveInfo.endAngles = ent->angles2;
    // For UNPRESSED: pos1 = start, pos2 = end.
    } else {
        ent->pushMoveInfo.startOrigin = ent->s.origin;
        ent->pushMoveInfo.startAngles = ent->angles1;
        ent->pushMoveInfo.endOrigin = ent->s.origin;
        ent->pushMoveInfo.endAngles = ent->angles2;
    }
    #endif
    // Animated doors:
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_ANIMATED ) ) {
        ent->s.effects |= EF_ANIM_ALL;
    }
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_ANIMATED_FAST ) ) {
        ent->s.effects |= EF_ANIM_ALLFAST;
    }
    
    // WID: TODO: Necessary? Default frame?
    // WID: Perhaps better to allow as spawn key value field.
    ent->s.frame = 0;

    // Locked or not locked?
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_START_LOCKED ) ) {
        ent->pushMoveInfo.lockState.isLocked = true;
    }

    // To simplify logic elsewhere, make non-teamed doors into a team of one
    if ( !ent->targetNames.team ) {
        ent->teammaster = ent;
    }

    // Link it in.
    gi.linkentity( ent );
}