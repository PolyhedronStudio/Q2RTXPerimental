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



/**
*   For readability's sake:
**/
static constexpr int32_t DOOR_STATE_OPENED = PUSHMOVE_STATE_TOP;
static constexpr int32_t DOOR_STATE_CLOSED = PUSHMOVE_STATE_BOTTOM;
static constexpr int32_t DOOR_STATE_MOVING_TO_OPENED_STATE = PUSHMOVE_STATE_MOVING_UP;
static constexpr int32_t DOOR_STATE_MOVING_TO_CLOSED_STATE = PUSHMOVE_STATE_MOVING_DOWN;



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
extern void door_postspawn( edict_t *self );

/**
*	@brief
**/
void SP_func_door_rotating( edict_t *ent ) {
    //vec3_t  abs_movedir;

    if ( ent->sounds != 1 ) {
        ent->pushMoveInfo.sound_start = gi.soundindex( "doors/door_start_01.wav" );
        ent->pushMoveInfo.sound_middle = gi.soundindex( "doors/door_mid_01.wav" );
        ent->pushMoveInfo.sound_end = gi.soundindex( "doors/door_end_01.wav" );

        ent->pushMoveInfo.lockState.lockedSound = gi.soundindex( "misc/door_locked.wav" );
        ent->pushMoveInfo.lockState.lockingSound = gi.soundindex( "misc/door_locking.wav" );
        ent->pushMoveInfo.lockState.unlockingSound = gi.soundindex( "misc/door_unlocking.wav" );
    }

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

    // check for reverse rotation
    if ( ent->spawnflags & DOOR_SPAWNFLAG_REVERSE ) {
        ent->movedir = QM_Vector3Negate( ent->movedir );
    }

    // Default distance to 90 degrees if not set.
    if ( !st.distance ) {
        gi.dprintf( "%s at %s with no distance set\n", ent->classname, vtos( ent->s.origin ) );
        st.distance = 90;
    }

    // Determine move angles.
    ent->angles1 = ent->s.angles;
    VectorMA( ent->s.angles, st.distance, ent->movedir, ent->angles2 );
    ent->pushMoveInfo.distance = st.distance;

    //SVG_SetMoveDir( ent->s.angles, ent->movedir );
    ent->movetype = MOVETYPE_PUSH;
    ent->solid = SOLID_BSP;
    ent->s.entityType = ET_PUSHER;
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
    ent->postspawn = door_postspawn;
    ent->blocked = door_blocked;
    ent->touch = door_touch;
    ent->use = door_use;
    ent->onsignalin = door_onsignalin;

    // Calculate absolute move distance to get from pos1 to pos2.
    ent->pos1 = ent->s.origin;
    ent->pos2 = ent->s.origin;

    // if it starts open, switch the positions
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_START_OPEN ) ) {
        VectorCopy( ent->angles2, ent->s.angles );
        ent->angles2 = ent->angles1;
        ent->angles1 = ent->s.angles;
        ent->movedir = QM_Vector3Negate( ent->movedir );
        ent->pushMoveInfo.state = DOOR_STATE_OPENED;
    } else {
        // Initial closed state.
        ent->pushMoveInfo.state = DOOR_STATE_CLOSED;
    }

    // Used for condition checking, if we got a damage activating door we don't want to have it support pressing.
    const bool damageActivates = SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_DAMAGE_ACTIVATES );
    // Health trigger based door:
    if ( damageActivates ) {
        // Set max health, also used to reinitialize the door to revive.
        ent->max_health = ent->health;
        // Let it take damage.
        ent->takedamage = DAMAGE_YES;
        // Die callback.
        ent->die = door_killed;
        // Apply next think time and method.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = Think_CalcMoveSpeed;
    // Touch based door:DOOR_SPAWNFLAG_DAMAGE_ACTIVATES
    } else if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_TOUCH_AREA_TRIGGERED ) ) {
        // Set its next think to create the trigger area.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = Think_SpawnDoorTrigger;
    } else {
        // Apply next think time and method.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = Think_CalcMoveSpeed;

        // This door is only toggled, never untoggled, by each (+usetarget) interaction.
        if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_PRESSABLE ) ) {
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_PRESS;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            ent->touch = nullptr;
            ent->use = door_use;
            // This door is dispatches untoggle/toggle callbacks by each (+usetarget) interaction, based on its usetarget state.
        } else if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_TOGGLEABLE ) ) {
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_TOGGLE;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            ent->touch = nullptr;
            ent->use = door_use;
        } else if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_HOLDABLE ) ) {
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_CONTINUOUS;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            ent->touch = nullptr;
            ent->use = door_use;
        }

        // Is usetargetting disabled by default?
        if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_DISABLED ) ) {
            ent->useTarget.flags = (entity_usetarget_flags_t)( ent->useTarget.flags | ENTITY_USETARGET_FLAG_DISABLED );
        }
    }

    // Copy the calculated info into the pushMoveInfo state struct.
    ent->pushMoveInfo.speed = ent->speed;
    ent->pushMoveInfo.accel = ent->accel;
    ent->pushMoveInfo.decel = ent->decel;
    ent->pushMoveInfo.wait = ent->wait;
    
    #if 1
    ent->pushMoveInfo.start_origin = ent->s.origin;
    ent->pushMoveInfo.start_angles = ent->angles1;
    ent->pushMoveInfo.end_origin = ent->s.origin;
    ent->pushMoveInfo.end_angles = ent->angles2;
    #else
    // For PRESSED: pos1 = start, pos2 = end.
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_START_OPEN ) ) {
        ent->pushMoveInfo.state = DOOR_STATE_OPENED;
        ent->pushMoveInfo.start_origin = ent->s.origin;
        ent->pushMoveInfo.start_angles = ent->angles1;
        ent->pushMoveInfo.end_origin = ent->s.origin;
        ent->pushMoveInfo.end_angles = ent->angles2;
    // For UNPRESSED: pos1 = start, pos2 = end.
    } else {
        ent->pushMoveInfo.start_origin = ent->s.origin;
        ent->pushMoveInfo.start_angles = ent->angles1;
        ent->pushMoveInfo.end_origin = ent->s.origin;
        ent->pushMoveInfo.end_angles = ent->angles2;
    }
    #endif
    // Animated doors:
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_ANIMATED ) ) {
        ent->s.effects |= EF_ANIM_ALL;
    }
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_ANIMATED_FAST ) ) {
        ent->s.effects |= EF_ANIM_ALLFAST;
    }

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