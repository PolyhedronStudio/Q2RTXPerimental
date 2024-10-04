/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_door'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_door.h"



/**
*   For readability's sake:
**/
static constexpr int32_t DOOR_STATE_OPENED = PUSHMOVE_STATE_TOP;
static constexpr int32_t DOOR_STATE_CLOSED = PUSHMOVE_STATE_BOTTOM;
static constexpr int32_t DOOR_STATE_MOVING_TO_OPENED_STATE = PUSHMOVE_STATE_MOVING_UP;
static constexpr int32_t DOOR_STATE_MOVING_TO_CLOSED_STATE = PUSHMOVE_STATE_MOVING_DOWN;

/*
======================================================================

DOORS

  spawn a trigger surrounding the entire team unless it is
  already targeted by another

======================================================================
*/

/*QUAKED func_door (0 .5 .8) ? START_OPEN x CRUSHER NOMONSTER ANIMATED TOGGLE ANIMATED_FAST
TOGGLE      Wait in both the start and end states for a trigger event.
START_OPEN  The door to moves to its destination when spawned, and operate in reverse.  
            It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
NOMONSTER   Monsters will not trigger this door.

"message"   Is printed when the door is touched if it is a trigger door and it hasn't been fired yet
"angle"     Determines the opening direction
"targetname" If set, no touch field will be spawned and a remote button or trigger field activates the door.
"health"    If set, door must be shot open
"speed"     Movement speed (100 default)
"wait"      Wait before returning (3 default, -1 = never return)
"lip"       Lip remaining at end of move (8 default)
"dmg"       Damage to inflict when blocked (2 default)
"sounds"
1)  silent
2)  light
3)  medium
4)  heavy
*/

/**
*	@brief  Open/Close the door's area portals.
**/
void door_use_areaportals( edict_t *self, const bool open ) {
    edict_t *t = NULL;

    if ( !self->targetNames.target )
        return;

    while ( ( t = SVG_Find( t, FOFS( targetname ), self->targetNames.target ) ) ) {
        //if (Q_stricmp(t->classname, "func_areaportal") == 0) {
        if ( t->s.entityType == ET_AREA_PORTAL ) {
            gi.SetAreaPortalState( t->style, open );

            //self->s.event = ( open ? EV_AREAPORTAL_OPEN : EV_AREAPORTAL_CLOSE );
            //self->s.event[ 1 ] = t->style;
            //self->s.event[ 2 ] = open;
        }
    }
}

/**
*   @brief  Fire use target lua function implementation if existant.
**/
void door_lua_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t &useType, const int32_t useValue ) {
    // Need the luaName.
    if ( self->luaProperties.luaName ) {
        // Generate function 'callback' name.
        const std::string luaFunctionName = std::string( self->luaProperties.luaName ) + "_Use";
        // Call if it exists.
        if ( LUA_HasFunction( SVG_Lua_GetMapLuaState(), luaFunctionName ) ) {
            LUA_CallFunction( SVG_Lua_GetMapLuaState(), luaFunctionName, 1, LUA_CALLFUNCTION_VERBOSE_MISSING,
                /*[lua args]:*/ self, other, activator, useType, useValue );
        }
    }
}


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
*	@brief
**/
void door_close_move( edict_t *self );

/**
*	@brief
**/
void door_open_move_done( edict_t *self ) {
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sound_end )
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_end, 1, ATTN_STATIC, 0 );
        self->s.sound = 0;
    }
    // Apply state.
    self->pushMoveInfo.state = DOOR_STATE_OPENED;

    // Dispatch a lua signal.
    SVG_Lua_SignalOut( SVG_Lua_GetMapLuaState(), self, self->activator, "OnOpened" );

    // If it is a toggle door, don't set any next think to 'go down' again.
    if ( self->spawnflags & DOOR_SPAWNFLAG_TOGGLE ) {
        return;
    }

    if ( self->pushMoveInfo.wait >= 0 ) {
        self->think = door_close_move;
        self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
    }
}

/**
*	@brief
**/
void door_close_move_done( edict_t *self ) {
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sound_end )
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_end, 1, ATTN_STATIC, 0 );
        self->s.sound = 0;
    }
    self->pushMoveInfo.state = DOOR_STATE_CLOSED;
    door_use_areaportals( self, false );

    // Dispatch a lua signal.
    SVG_Lua_SignalOut( SVG_Lua_GetMapLuaState(), self, self->activator, "OnClosed" );
}



/**
*
*
*
*   Close/Open Initiators:
*
*
*
**/
/**
*	@brief
**/
void door_close_move( edict_t *self ) {
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sound_start )
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
        self->s.sound = self->pushMoveInfo.sound_middle;
    }
    if ( self->max_health ) {
        self->takedamage = DAMAGE_YES;
        self->health = self->max_health;
    }

    self->pushMoveInfo.state = DOOR_STATE_MOVING_TO_CLOSED_STATE;
    if ( strcmp( self->classname, "func_door" ) == 0 )
        SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.start_origin, door_close_move_done );
    else if ( strcmp( self->classname, "func_door_rotating" ) == 0 )
        SVG_PushMove_AngleMoveCalculate( self, door_close_move_done );

    // Dispatch a lua signal.
    SVG_Lua_SignalOut( SVG_Lua_GetMapLuaState(), self, self->activator, "OnClose" );
}

/**
*	@brief
**/
void door_open_move( edict_t *self/*, edict_t *activator */) {
    if ( self->pushMoveInfo.state == DOOR_STATE_MOVING_TO_OPENED_STATE )
        return;     // already going up

    if ( self->pushMoveInfo.state == DOOR_STATE_OPENED ) {
        // reset top wait time
        if ( self->pushMoveInfo.wait >= 0 )
            self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
        return;
    }

    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sound_start )
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
        self->s.sound = self->pushMoveInfo.sound_middle;
    }
    self->pushMoveInfo.state = DOOR_STATE_MOVING_TO_OPENED_STATE;
    if ( strcmp( self->classname, "func_door" ) == 0 )
        SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.end_origin, door_open_move_done );
    else if ( strcmp( self->classname, "func_door_rotating" ) == 0 )
        SVG_PushMove_AngleMoveCalculate( self, door_open_move_done );

    SVG_UseTargets( self, self->activator );
    door_use_areaportals( self, true );

    // Dispatch a lua signal.
    SVG_Lua_SignalOut( SVG_Lua_GetMapLuaState(), self, self->activator, "OnOpen" );
}

/**
*	@brief
**/
void door_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    edict_t *ent;

    if ( self->flags & FL_TEAMSLAVE )
        return;

    if ( self->spawnflags & DOOR_SPAWNFLAG_TOGGLE ) {
        if ( self->pushMoveInfo.state == DOOR_STATE_MOVING_TO_OPENED_STATE || self->pushMoveInfo.state == DOOR_STATE_OPENED ) {
            // trigger all paired doors
            for ( ent = self; ent; ent = ent->teamchain ) {
                ent->message = NULL;
                ent->touch = NULL;
                ent->activator = activator; // WID: We need to assign it right?
                door_close_move( ent );
            }
            return;
        }
    }

    // trigger all paired doors
    for ( ent = self; ent; ent = ent->teamchain ) {
        ent->message = NULL;
        ent->touch = NULL;
        ent->activator = activator; // WID: We need to assign it right?
        door_open_move( ent/*, activator */);
    }

    // Call upon Lua OnUse.
    door_lua_use( self, other, activator, useType, useValue );
}

/**
*	@brief
**/
void door_blocked( edict_t *self, edict_t *other ) {
    edict_t *ent;

    if ( !( other->svflags & SVF_MONSTER ) && ( !other->client ) ) {
        // give it a chance to go away on it's own terms (like gibs)
        T_Damage( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, 0, MEANS_OF_DEATH_CRUSHED );
        // if it's still there, nuke it
        if ( other )
            BecomeExplosion1( other );
        return;
    }

    T_Damage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MEANS_OF_DEATH_CRUSHED );

    if ( self->spawnflags & DOOR_SPAWNFLAG_CRUSHER ) {
        return;
    }

    // if a door has a negative wait, it would never come back if blocked,
    // so let it just squash the object to death real fast
    if ( self->pushMoveInfo.wait >= 0 ) {
        if ( self->pushMoveInfo.state == DOOR_STATE_MOVING_TO_CLOSED_STATE ) {
            for ( ent = self->teammaster; ent; ent = ent->teamchain )
                door_open_move( ent/*, ent->activator */);
        } else {
            for ( ent = self->teammaster; ent; ent = ent->teamchain )
                door_close_move( ent );
        }
    }
}

/**
*	@brief
**/
void door_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    edict_t *ent;

    for ( ent = self->teammaster; ent; ent = ent->teamchain ) {
        ent->health = ent->max_health;
        ent->takedamage = DAMAGE_NO;
    }
    door_use( self->teammaster, attacker, attacker, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, 0 );
}

/**
*	@brief
**/
void door_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
    if ( !other->client )
        return;

    if ( level.time < self->touch_debounce_time )
        return;
    self->touch_debounce_time = level.time + 5_sec;

    gi.centerprintf( other, "%s", self->message );
    gi.sound( other, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), 1, ATTN_NORM, 0 );
}

/**
*	@brief
**/
void door_postspawn( edict_t *self ) {
    //if ( self->spawnflags & DOOR_START_OPEN ) {
    //    //SVG_UseTargets( self, self );
    //    door_use_areaportals( self, true );
    //    //self->pushMoveInfo.state = DOOR_STATE_OPENED;
    //}
}

/**
*	@brief
**/
void SP_func_door( edict_t *ent ) {
    vec3_t  abs_movedir;

    if ( ent->sounds != 1 ) {
        ent->pushMoveInfo.sound_start = gi.soundindex( "doors/door_start_01.wav" );
        ent->pushMoveInfo.sound_middle = gi.soundindex( "doors/door_mid_01.wav" );
        ent->pushMoveInfo.sound_end = gi.soundindex( "doors/door_end_01.wav" );
    }

    SVG_SetMoveDir( ent->s.angles, ent->movedir );
    ent->movetype = MOVETYPE_PUSH;
    ent->solid = SOLID_BSP;
    ent->s.entityType = ET_PUSHER;
    // BSP Model, or otherwise, specified external model.
    gi.setmodel( ent, ent->model );

    // WID: TODO: We don't want this...
    #if 0
    if ( deathmatch->value ) {
        ent->speed *= 2;
    }
    #endif
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
    ent->use = door_use;

    // Calculate absolute move distance to get from pos1 to pos2.
    ent->pos1 = ent->s.origin;
    abs_movedir[ 0 ] = fabsf( ent->movedir[ 0 ] );
    abs_movedir[ 1 ] = fabsf( ent->movedir[ 1 ] );
    abs_movedir[ 2 ] = fabsf( ent->movedir[ 2 ] );
    ent->pushMoveInfo.distance = abs_movedir[ 0 ] * ent->size[ 0 ] + abs_movedir[ 1 ] * ent->size[ 1 ] + abs_movedir[ 2 ] * ent->size[ 2 ] - st.lip;
    // Translate the determined move distance into the move direction to get pos2, our move end origin.
    ent->pos2 = QM_Vector3MultiplyAdd( ent->pos1, ent->pushMoveInfo.distance, ent->movedir );

    // if it starts open, switch the positions
    //if ( ent->spawnflags & DOOR_SPAWNFLAG_START_OPEN ) {
    //    VectorCopy( ent->pos2, ent->s.origin );
    //    VectorCopy( ent->pos1, ent->pos2 );
    //    VectorCopy( ent->s.origin, ent->pos1 );
    //}

    // Initial closed state.
    ent->pushMoveInfo.state = DOOR_STATE_CLOSED;

    if ( ent->health ) {
        ent->takedamage = DAMAGE_YES;
        ent->die = door_killed;
        ent->max_health = ent->health;
    } else if ( ent->targetname && ent->message ) {
        gi.soundindex( "hud/chat01.wav" );
        ent->touch = door_touch;
    }

    // Copy the calculated info into the pushMoveInfo state struct.
    ent->pushMoveInfo.speed = ent->speed;
    ent->pushMoveInfo.accel = ent->accel;
    ent->pushMoveInfo.decel = ent->decel;
    ent->pushMoveInfo.wait = ent->wait;
    // For PRESSED: pos1 = start, pos2 = end.
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_START_OPEN ) ) {
        ent->pushMoveInfo.state = DOOR_STATE_OPENED;
        ent->pushMoveInfo.start_origin = ent->pos2;
        ent->pushMoveInfo.start_angles = ent->s.angles;
        ent->pushMoveInfo.end_origin = ent->pos1;
        ent->pushMoveInfo.end_angles = ent->s.angles;
    // For UNPRESSED: pos1 = start, pos2 = end.
    } else {
        ent->pushMoveInfo.start_origin = ent->pos1;
        ent->pushMoveInfo.start_angles = ent->s.angles;
        ent->pushMoveInfo.end_origin = ent->pos2;
        ent->pushMoveInfo.end_angles = ent->s.angles;
    }

    // Animated doors:
    if ( ent->spawnflags & DOOR_SPAWNFLAG_ANIMATED ) {
        ent->s.effects |= EF_ANIM_ALL;
    }
    if ( ent->spawnflags & DOOR_SPAWNFLAG_ANIMATED_FAST ) {
        ent->s.effects |= EF_ANIM_ALLFAST;
    }

    // To simplify logic elsewhere, make non-teamed doors into a team of one
    if ( !ent->targetNames.team ) {
        ent->teammaster = ent;
    }

    // Link it in.
    gi.linkentity( ent );

    // Apply next think time and method.
    ent->nextthink = level.time + FRAME_TIME_S;
    if ( ent->health || ent->targetname ) {
        ent->think = Think_CalcMoveSpeed;
    } else {
        ent->think = Think_SpawnDoorTrigger;
    }
}
