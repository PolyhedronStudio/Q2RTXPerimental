/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_door_rotating'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_trigger.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_door.h"
#include "svgame/entities/func/svg_func_door_rotating.h"

#include "sharedgame/sg_entity_flags.h"


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
void SVG_Util_SetMoveDir( Vector3 &angles, Vector3 &movedir );
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_door_rotating_t, onSpawn )( svg_func_door_rotating_t *self ) -> void {
    //vec3_t  abs_movedir;
    svg_pushmove_edict_t::onSpawn( self );

    if ( self->sounds != 1 ) {
        self->SetupDefaultSounds();
    }

    // In case we ever go Q2 Rerelease movers I guess.
    #if 0
    const bool isBothDirections = SVG_HasSpawnFlags( self, svg_func_door_rotating_t::SPAWNFLAG_BOTH_DIRECTIONS );
    if ( isBothDirections ) {
        SVG_Util_SetMoveDir( self->s.angles, self->pushMoveInfo.dir);
    }
    #endif

    // Clear the angles.
    VectorClear( self->s.angles );

    // Set the axis of rotation.
    self->movedir = QM_Vector3Zero();
    if ( self->spawnflags & svg_func_door_rotating_t::SPAWNFLAG_X_AXIS ) {
        self->movedir[ 2 ] = 1.0f;
    } else if ( self->spawnflags & svg_func_door_rotating_t::SPAWNFLAG_Y_AXIS ) {
        self->movedir[ 0 ] = 1.0f;
    } else {// Z_AXIS
        self->movedir[ 1 ] = 1.0f;
    }

    // Check for reverse rotation.
    if ( SVG_HasSpawnFlags( self, svg_func_door_rotating_t::SPAWNFLAG_REVERSE ) ) {
        self->movedir = QM_Vector3Negate( self->movedir );
    }

    // Default distance to 90 degrees if not set.
    if ( !self->distance ) {
        gi.dprintf( "%s at %s with no distance set\n", (const char *)self->classname, vtos( self->s.origin ) );
        self->distance = 90;
    }

    // Determine move angles.
    const float distance = self->distance; // ( SVG_HasSpawnFlags( self, svg_func_door_rotating_t::SPAWNFLAG_REVERSE ) ? -st.distance : st.distance );
    self->angles1 = self->s.angles;
    VectorMA( self->s.angles, distance, self->movedir, self->angles2 );
    self->pushMoveInfo.distance = distance;

    //SVG_Util_SetMoveDir( self->s.angles, self->movedir );
    self->movetype = MOVETYPE_PUSH;
    self->solid = SOLID_BSP;
    self->s.renderfx |= RF_BRUSHTEXTURE_SET_FRAME_INDEX;
    self->s.entityType = ET_PUSHER;
    self->svFlags |= SVF_DOOR;
    // BSP Model, or otherwise, specified external model.
    gi.setmodel( self, self->model );

    // PushMove defaults:
    if ( !self->speed ) {
        self->speed = 100;
    }
    if ( !self->accel ) {
        self->accel = self->speed;
    }
    if ( !self->decel ) {
        self->decel = self->speed;
    }
    if ( !self->lip ) {
        self->lip = 8;
    }
    // Trigger defaults:
    if ( !self->wait ) {
        self->wait = 3;
    }
    if ( !self->dmg ) {
        self->dmg = 2;
    }

    // Callbacks.
    self->SetPostSpawnCallback( &svg_func_door_rotating_t::onPostSpawn );
    self->SetBlockedCallback( &svg_func_door_rotating_t::onBlocked );
    self->SetTouchCallback( &svg_func_door_rotating_t::onTouch );
    self->SetUseCallback( &svg_func_door_rotating_t::onUse );
    self->SetPainCallback( &svg_func_door_rotating_t::onPain );
    self->SetOnSignalInCallback( &svg_func_door_rotating_t::onSignalIn );

    // Calculate absolute move distance to get from pos1 to pos2.
    self->pos1 = self->s.origin;
    self->pos2 = self->s.origin;

    // if it starts open, switch the positions
    if ( SVG_HasSpawnFlags( self, svg_func_door_rotating_t::SPAWNFLAG_START_OPEN ) ) {
        VectorCopy( self->angles2, self->s.angles );
        //self->angles2 = self->angles1;
        //self->angles1 = -Vector3(self->s.angles);
        //self->movedir = QM_Vector3Negate( self->movedir );
        self->pushMoveInfo.state = svg_func_door_rotating_t::DOOR_STATE_OPENED;
    } else {
        // Initial closed state.
        self->pushMoveInfo.state = svg_func_door_rotating_t::DOOR_STATE_CLOSED;
    }

    // Used for condition checking, if we got a damage activating door we don't want to have it support pressing.
    const bool damageActivates = SVG_HasSpawnFlags( self, svg_func_door_rotating_t::SPAWNFLAG_DAMAGE_ACTIVATES );
    // Health trigger based door:
    if ( damageActivates ) {
        #if 0
        // Spawn open, does not imply death, to do so, spawn open, set max_health for revival, and health to 0 for death.
        if ( self->max_health > 0 && self->health <= 0 ) {
            // Don't allow damaging.
            self->takedamage = DAMAGE_NO;
            self->lifeStatus = LIFESTATUS_DEAD;
            // Die callback.
            self->SetDieCallback( &svg_func_door_rotating_t::onDie );
            self->SetPainCallback( &svg_func_door_rotating_t::onPain );
        } else {
            // Set max health, in case it wasn't set.also used to reinitialize the door to revive.
            if ( !self->health ) {
                self->health = self->max_health;
            }
            // Let it take damage.
            self->takedamage = DAMAGE_YES;
            self->lifeStatus = LIFESTATUS_ALIVE;
            // Die callback.
            self->SetDieCallback( &svg_func_door_rotating_t::onDie );
            self->SetPainCallback( &svg_func_door_rotating_t::onPain );
        }

        // Apply next think time and method.
        self->nextthink = level.time + FRAME_TIME_S;
        self->SetThinkCallback( &svg_func_door_rotating_t::SVG_PushMove_Think_CalculateMoveSpeed );
        #else
        // Set max health, also used to reinitialize the door to revive.
        self->max_health = self->health;
        // Let it take damage.
        self->takedamage = DAMAGE_YES;
        // Die callback.
        self->SetDieCallback( &svg_func_door_rotating_t::onDie );
        self->SetPainCallback( &svg_func_door_rotating_t::onPain );
        // Apply next think time and method.
        self->nextthink = level.time + FRAME_TIME_S;
        self->SetThinkCallback( &svg_func_door_rotating_t::SVG_PushMove_Think_CalculateMoveSpeed );
        #endif
    // Touch based door:svg_func_door_rotating_t::SPAWNFLAG_DAMAGE_ACTIVATES
    } else if ( SVG_HasSpawnFlags( self, svg_func_door_rotating_t::SPAWNFLAG_TOUCH_AREA_TRIGGERED ) ) {
        // Set its next think to create the trigger area.
        self->nextthink = level.time + FRAME_TIME_S;
        self->SetThinkCallback( &DoorTrigger_SpawnThink );
        self->SetUseCallback( &svg_func_door_rotating_t::onUse );
    } else {
        // Apply next think time and method.
        self->nextthink = level.time + FRAME_TIME_S;
        self->SetThinkCallback( &svg_func_door_rotating_t::SVG_PushMove_Think_CalculateMoveSpeed );

        // This door is only toggled, never untoggled, by each (+usetarget) interaction.
        if ( SVG_HasSpawnFlags( self, SPAWNFLAG_USETARGET_PRESSABLE ) ) {
            self->useTarget.flags = ENTITY_USETARGET_FLAG_PRESS;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            self->SetTouchCallback( nullptr );
            self->SetUseCallback( &svg_func_door_rotating_t::onUse );
            // This door is dispatches untoggle/toggle callbacks by each (+usetarget) interaction, based on its usetarget state.
        } else if ( SVG_HasSpawnFlags( self, SPAWNFLAG_USETARGET_TOGGLEABLE ) ) {
            self->useTarget.flags = ENTITY_USETARGET_FLAG_TOGGLE;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            self->SetTouchCallback( nullptr );
            self->SetUseCallback( &svg_func_door_rotating_t::onUse );
        } else if ( SVG_HasSpawnFlags( self, SPAWNFLAG_USETARGET_HOLDABLE ) ) {
            self->useTarget.flags = ENTITY_USETARGET_FLAG_CONTINUOUS;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            self->SetTouchCallback( nullptr );
            self->SetUseCallback( &svg_func_door_rotating_t::onUse );
        }

        // Is usetargetting disabled by default?
        if ( SVG_HasSpawnFlags( self, SPAWNFLAG_USETARGET_DISABLED ) ) {
            self->useTarget.flags |= ENTITY_USETARGET_FLAG_DISABLED;
        }
    }

    // Copy the calculated info into the pushMoveInfo state struct.
    self->pushMoveInfo.speed = self->speed;
    self->pushMoveInfo.accel = self->accel;
    self->pushMoveInfo.decel = self->decel;
    self->pushMoveInfo.wait = self->wait;
    
    #if 1
    self->pushMoveInfo.startOrigin = self->s.origin;
    self->pushMoveInfo.startAngles = self->angles1;
    self->pushMoveInfo.endOrigin = self->s.origin;
    self->pushMoveInfo.endAngles = self->angles2;
    #else
    // For PRESSED: pos1 = start, pos2 = end.
    if ( SVG_HasSpawnFlags( self, svg_func_door_rotating_t::SPAWNFLAG_START_OPEN ) ) {
        self->pushMoveInfo.state = DOOR_STATE_OPENED;
        self->pushMoveInfo.startOrigin = self->s.origin;
        self->pushMoveInfo.startAngles = self->angles1;
        self->pushMoveInfo.endOrigin = self->s.origin;
        self->pushMoveInfo.endAngles = self->angles2;
    // For UNPRESSED: pos1 = start, pos2 = end.
    } else {
        self->pushMoveInfo.startOrigin = self->s.origin;
        self->pushMoveInfo.startAngles = self->angles1;
        self->pushMoveInfo.endOrigin = self->s.origin;
        self->pushMoveInfo.endAngles = self->angles2;
    }
    #endif
    // Animated doors:
    if ( SVG_HasSpawnFlags( self, svg_func_door_rotating_t::SPAWNFLAG_ANIMATED ) ) {
        self->s.entityFlags |= EF_ANIM_ALL;
    }
    if ( SVG_HasSpawnFlags( self, svg_func_door_rotating_t::SPAWNFLAG_ANIMATED_FAST ) ) {
        self->s.entityFlags |= EF_ANIM_ALLFAST;
    }
    
    // WID: TODO: Necessary? Default frame?
    // WID: Perhaps better to allow as spawn key value field.
    self->s.frame = 0;

    // Locked or not locked?
    if ( SVG_HasSpawnFlags( self, svg_func_door_rotating_t::SPAWNFLAG_START_LOCKED ) ) {
        self->SetLockState( true );
    }

    // To simplify logic elsewhere, make non-teamed doors into a team of one
    if ( !self->targetNames.team ) {
        self->teammaster = self;
    }

    // Link it in.
    gi.linkentity( self );
}