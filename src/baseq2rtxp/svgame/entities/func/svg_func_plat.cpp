/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_plat'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_trigger.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_plat.h"

#include "sharedgame/sg_entity_effects.h"
#include "sharedgame/sg_means_of_death.h"



/**
*
*
*
*   Save Descriptor Field Setup: svg_func_plat_trigger
*
*
*
**/
/**
*   @brief  Save descriptor array definition for all the members of svg_monster_testdummy_t.
**/
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_func_plat_trigger_t )
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_func_plat_trigger_t, platformEntityNumber, SD_FIELD_TYPE_INT32 ),
SAVE_DESCRIPTOR_FIELDS_END();

//! Implement the methods for saving this edict type's save descriptor fields.
SVG_SAVE_DESCRIPTOR_FIELDS_DEFINE_IMPLEMENTATION( svg_func_plat_trigger_t, svg_base_edict_t );


/********************************************************************
*
*
*
*   func_plat_trigger
*
*   Spawned in-game, at bottom and/or top depending on spawnflags set.
* 
* 
* 
*********************************************************************/
/**
*   Reconstructs the object, optionally retaining the entityDictionary.
**/
void svg_func_plat_trigger_t::Reset( const bool retainDictionary ) {
	Super::Reset( retainDictionary );
    // Reset.
    platformEntityNumber = -1;
}

/**
*   @brief  Save the entity into a file using game_write_context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_func_plat_trigger_t::Save( struct game_write_context_t *ctx ) {
	Super::Save( ctx );
    // Save all the members of this entity type.
    ctx->write_fields( svg_func_plat_trigger_t::saveDescriptorFields, this );
}
/**
*   @brief  Restore the entity from a loadgame read context.
*   @note   Make sure to call the base parent class' Restore() function.
**/
void svg_func_plat_trigger_t::Restore( struct game_read_context_t *ctx ) {
	Super::Restore( ctx );
    // Save all the members of this entity type.
    ctx->read_fields( svg_func_plat_trigger_t::saveDescriptorFields, this );
}

/**
*
* 
*   CallBack Member Functions:
* 
* 
**/
/**
*   @brief  Spawn.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_plat_trigger_t, onSpawn )( svg_func_plat_trigger_t *self ) -> void {
    //Super::onSpawn( self );
    self->SetTouchCallback( &svg_func_plat_trigger_t::onTouch );
	self->SetPostSpawnCallback( &svg_func_plat_trigger_t::onPostSpawn );
    self->movetype = MOVETYPE_NONE;
    self->solid = SOLID_TRIGGER;
    self->s.entityType = ET_PUSH_TRIGGER;
	self->svFlags |= SVF_NOCLIENT; // Don't send to clients.
    gi.linkentity( self );
}
/**
*   @brief  PostSpawn.
**/
DEFINE_MEMBER_CALLBACK_POSTSPAWN( svg_func_plat_trigger_t, onPostSpawn )( svg_func_plat_trigger_t *self ) -> void {
    // Get the platform owner entity.
    svg_base_edict_t *foundEntity = g_edict_pool.EdictForNumber( self->platformEntityNumber );

    // Make sure we have a valid platform entity.
    if ( !SVG_Entity_IsActive( foundEntity ) || !foundEntity->GetTypeInfo()->IsSubClassType<svg_func_plat_t>() ) {
        gi.dprintf( "%s: Invalid platform entity.\n", __func__ );
        return;
    }
    // Setup touch callback.
    self->SetTouchCallback( &svg_func_plat_trigger_t::onTouch );

    // Acquire the platform entity.
    svg_func_plat_t *platformEntity = static_cast<svg_func_plat_t *>( foundEntity );

    // Calculate trigger mins/maxs.
    Vector3 triggerMins = platformEntity->mins + Vector3{ 25.f, 25.f, 0.f };
    Vector3 triggerMaxs = platformEntity->maxs + Vector3{ -25.f, -25.f, 8.f };
	// Convert to BBox3.
	BBox3 triggerBox = QM_BBox3FromMinsMaxs( triggerMins, triggerMaxs );
    
	// If high trigger, adjust the z position.
    Vector3 triggerOrigin = platformEntity->pos2;
    if ( self->spawnflags & svg_func_plat_t::SPAWNFLAG_HIGH_TRIGGER ) {
        triggerOrigin = platformEntity->pos1;
	}
    // Set the trigger's origin.
    triggerOrigin += Vector3{ 0.f, 0.f, platformEntity->lip };
    VectorCopy( triggerOrigin, self->s.origin );

    // Calculate the final bounds for this trigger entity.
    BBox3 finalBounds = QM_BBox3FromCenterSize( QM_BBox3Size( triggerBox ), QM_Vector3Zero() );
	// Set the trigger's mins and maxs.
	VectorCopy( finalBounds.mins, self->mins );
	VectorCopy( finalBounds.maxs, self->maxs );

    gi.linkentity( self );
}
/**
*   @brief  Touched.
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_func_plat_trigger_t, onTouch )( svg_func_plat_trigger_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
	// Has to be a client.
    if ( !SVG_Entity_IsClient( other, true /*healthCheck*/) ) {
        return;
    }
    // Get platform entity.
    svg_base_edict_t *platPtr = g_edict_pool.EdictForNumber( self->platformEntityNumber );

    // Make sure we have a valid platform entity.
    if ( !SVG_Entity_IsActive( platPtr ) || !platPtr->GetTypeInfo()->IsSubClassType<svg_func_plat_t>() ) {
        // If we have a platform entity, but it's not valid, print a debug message.
        if ( platPtr != nullptr ) {
			gi.dprintf( "%s: Platform entity %i is not valid.\n", __func__, self->platformEntityNumber );
        // If we don't have a platform entity, print a debug message.
        } else {
            gi.dprintf( "%s: Invalid platform entity.\n", __func__ );
		}
        return;
    }

	// Acquire the platform entity.
    svg_func_plat_t *platformEntity = static_cast<svg_func_plat_t *>( platPtr );
    
	// If we are within the debounce time, return.
	if ( platformEntity->touch_debounce_time > level.time ) {
        gi.dprintf( "%s: Platform (#%i, \"%s\") touch debounce time not expired.\n", __func__, platformEntity->s.number, (const char *)platformEntity->classname.ptr );
        return;
	}
    // If we are a toggle platform instead, we return.
    if ( ( platformEntity->spawnflags & svg_func_plat_t::SPAWNFLAG_TOGGLE ) ) {
        gi.dprintf( "%s: Platform (#%i, \"%s\") is toggleable only.\n", __func__, platformEntity->s.number, (const char*)platformEntity->classname.ptr );
        return;
    }

	// Go up if we'r at the bottom.
    if ( platformEntity->pushMoveInfo.state == PUSHMOVE_STATE_BOTTOM ) {
        platformEntity->BeginUpMove( );
    // Go down if we're at the top.
    } else if ( platformEntity->pushMoveInfo.state == PUSHMOVE_STATE_TOP ) {
        #if 0
            #if 1
            platformEntity->onThink_Idle( platformEntity );
            gi.dprintf( "%s: onThink_Idle.\n", __func__, platformEntity->classname.ptr );
            #else
            //platformEntity->nextthink = level.time + 1_sec; // the player is still on the plat, so delay going down
            #endif
        #else
            platformEntity->BeginDownMove();
        #endif
    }
}


/*QUAKED func_plat (0 .5 .8) ? PLAT_LOW_TRIGGER
speed   default 150

Plats are always drawn in the extended position, so they will light correctly.

If the plat is the target of another trigger or button, it will start out disabled in the extended position until it is trigger, when it will lower and become a normal plat.

"speed" overrides default 20.
"accel" overrides default 5
"decel" overrides default 5
"lip"   overrides default 8 pixel lip

If the "height" key is set, that will determine the amount the plat moves, instead of being implicitly determoveinfoned by the model's height.

Set "sounds" to one of the following:
1) base fast
2) chain slow
*/

/*
=========================================================

  PLATS

  movement options:

  linear
  smooth start, hard stop
  smooth start, smooth stop

  start
  end
  acceleration
  speed
  deceleration
  begin sound
  end sound
  target fired when reaching end
  wait at end

  object characteristics that use move segments
  ---------------------------------------------
  movetype_push, or movetype_stop
  action when touched
  action when blocked
  action when used
    disabled?
  auto trigger spawning


=========================================================
*/

/**
*   @brief  Thinking.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_func_plat_t, onThink )( svg_func_plat_t *self ) -> void { }
DEFINE_MEMBER_CALLBACK_THINK( svg_func_plat_t, onThink_Idle )( svg_func_plat_t *self ) -> void {
    // Reset to pusher so we can try moving again this frame.
    if ( self->spawnflags & SPAWNFLAG_BLOCK_STOPS ) {
        self->movetype = MOVETYPE_PUSH;
    }

    self->SetThinkCallback( &svg_func_plat_t::onThink_Idle );
    self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*   @brief  PushMoveInfo EndMove Callback for when the platform hits the top.
**/
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_plat_t, onPlatHitTop )( svg_func_plat_t *self ) -> void {
    // If not a team slave, play the end sound and reset the sound state.
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sounds.end ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, 1, ATTN_STATIC, 0 );
        }
        self->s.sound = 0;
    }
    // Set the state to top.
    self->pushMoveInfo.state = PUSHMOVE_STATE_TOP;

    // Set the debounce time to prevent immmediate re-triggering.
    if ( !( self->spawnflags & svg_func_plat_t::SPAWNFLAG_TOGGLE ) ) {
        self->touch_debounce_time = level.time + QMTime::FromSeconds( self->wait );
    }
    #if 0
    ent->think = plat_go_down;
    ent->nextthink = level.time + 3_sec;
    #else
    self->onThink_Idle( self );
    #endif

    // Get reference to sol lua state view.
    sol::state_view &solStateView = SVG_Lua_GetSolStateView();
    // Create temporary objects encapsulating access to svg_base_edict_t's.
    auto leSelf = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( self ) );
    auto leOther = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( self->other ) );
    auto leActivator = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( self->activator ) );
    // Call into function.
    bool callReturnValue = false;
    bool calledFunction = LUA_CallLuaNameEntityFunction( self, "OnPlatformHitTop",
        solStateView,
        callReturnValue,
        leSelf, leOther, leActivator, ENTITY_USETARGET_TYPE_ON, 1
    );
}

/**
*   @brief  PushMoveInfo EndMove Callback for when the platform hits the bottom.
**/
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_plat_t, onPlatHitBottom )( svg_func_plat_t *self ) -> void {
	// If not a team slave, play the end sound and reset the sound state.
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sounds.end ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, 1, ATTN_STATIC, 0 );
        }
        self->s.sound = 0;
    }
	// Set the state to bottom.
    self->pushMoveInfo.state = PUSHMOVE_STATE_BOTTOM;

    // Set the debounce time to prevent immmediate re-triggering.
    if ( !( self->spawnflags & svg_func_plat_t::SPAWNFLAG_TOGGLE ) ) {
        self->touch_debounce_time = level.time + QMTime::FromSeconds( self->wait );
    }

    // Engage into idle thinking.
    self->onThink_Idle( self );

    // Get reference to sol lua state view.
    sol::state_view &solStateView = SVG_Lua_GetSolStateView();
    // Create temporary objects encapsulating access to svg_base_edict_t's.
    auto leSelf = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( self ) );
    auto leOther = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( self->other ) );
    auto leActivator = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( self->activator ) );
    // Call into function.
    bool callReturnValue = false;
    bool calledFunction = LUA_CallLuaNameEntityFunction( self, "OnPlatformHitBottom",
        solStateView,
        callReturnValue, LUA_CALLFUNCTION_VERBOSE_MISSING,
        leSelf, leOther, leActivator, ENTITY_USETARGET_TYPE_OFF, 0
    );
}

/**
*   @brief  Blocked.
**/
DEFINE_MEMBER_CALLBACK_BLOCKED( svg_func_plat_t, onBlocked )( svg_func_plat_t *self, svg_base_edict_t *other ) -> void {
    if ( self->spawnflags & SPAWNFLAG_PAIN_ON_TOUCH ) {
        // This only does damage if the plat is set to do so.
        if ( !( other->svFlags & SVF_MONSTER ) && ( !other->client ) ) {
            const bool knockBack = true;
            // give it a chance to go away on it's own terms (like gibs)
            SVG_DamageEntity( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, knockBack, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
            // if it's still there, nuke it
            if ( other && other->inUse && other->solid ) { // PGM)
                SVG_Misc_BecomeExplosion( other, 1 );
            }
            return;
        }

        // PGM
        //  gib dead things
        if ( other->health < 1 ) {
            SVG_DamageEntity( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
        }
        // PGM

        const bool knockBack = false;
        SVG_DamageEntity( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );

        // [Paril-KEX] killed the thing, so don't switch directions
        //if ( !other->inUse || other->solid == SOLID_NOT ) {
        // WID: Seems more appropriate since solid_not can still be inUse and alive but whatever.
        if ( !other->inUse || ( other->inUse && other->solid == SOLID_NOT ) ) {
            return;
        }
    }

	// If the platform is a blocking platform, we don't change direction.
	// Instead we become a MOVE_STOP platform.
    if ( self->spawnflags & SPAWNFLAG_BLOCK_STOPS ) {
        self->movetype = MOVETYPE_STOP;
        return;
    }

	// If the platform is moving up or at the top, we begin a down move.
    if ( self->pushMoveInfo.state == PUSHMOVE_STATE_TOP || self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP ) {
        self->BeginDownMove();
	// If the platform is moving down or at the bottom, we begin an up move.
    } else {
        self->BeginUpMove();
    }
}

/**
*   @brief  Used.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_func_plat_t, onUse )( svg_func_plat_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    // WID: <Q2RTXP> For func_button support.
    //if ( ( other && !strcmp( other->classname, "func_button" ) ) ) {
    if ( self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP || self->pushMoveInfo.state == PUSHMOVE_STATE_TOP ) {
        self->BeginDownMove( );
    } else if ( self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_DOWN || self->pushMoveInfo.state == PUSHMOVE_STATE_BOTTOM ) {
        self->BeginUpMove( );
    }
    //    return;     // already down
    //}
    // WID: </Q2RTXP>

    #if 0
    if ( ent->think ) {
        return;
    }
    plat_go_down( ent );
    #endif
}


/**
*   @brief  
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_plat_t, onSpawn )( svg_func_plat_t *self ) -> void {
    Super::onSpawn( self );

    VectorClear( self->s.angles );
    self->solid = SOLID_BSP;
    // Blocking STOPS the platform from moving further:
    //if ( self->spawnflags & svg_func_plat_t::SPAWNFLAG_BLOCK_STOPS ) {
    //    self->movetype = MOVETYPE_STOP;
    // Blocking PUSHES the entities on the platform.
    //} else {
        self->movetype = MOVETYPE_PUSH;
    //}
    self->s.entityType = ET_PUSHER;
    gi.setmodel( self, self->model );

    // Set audio.
    self->pushMoveInfo.sounds.start = gi.soundindex( "pushers/plat_start_01.wav" );
    self->pushMoveInfo.sounds.middle = gi.soundindex( "pushers/plat_mid_01.wav" );
    self->pushMoveInfo.sounds.end = gi.soundindex( "pushers/plat_end_01.wav" );

	// SpawnKey Defaults:
    if ( !self->speed ) {
        self->speed = 20.f;
    } else {
        self->speed *= 0.1f;
    }
    if ( !self->accel ) {
        self->accel = 5.f;
    } else {
        self->accel *= 0.1f;
    }
    if ( !self->decel ) {
        self->decel = 5.f;
    } else {
        self->decel *= 0.1f;
    }
    if ( !self->dmg ) {
        self->dmg = 2;
    }
    if ( !self->lip ) {
        self->lip = 8;
    }

    // Pos1 = top position, determine pos2 which = bottom position.
    VectorCopy( self->s.origin, self->pos1 );
    VectorCopy( self->s.origin, self->pos2 );
    // Height was already set.
    if ( self->height ) {
        self->pos2[ 2 ] -= self->height;
    // Determine height ourselves.
    } else {
        self->pos2[ 2 ] -= ( self->height = ( self->maxs[ 2 ] - self->mins[ 2 ] ) - self->lip );
    }

    // WID: TODO: For Lua stuff we dun need this.
    self->pushMoveInfo.state = PUSHMOVE_STATE_TOP;
    // WID: TODO: Add spawnflags for this stuff.
    if ( self->spawnflags & svg_func_plat_t::SPAWNFLAG_START_BOTTOM ) {
        VectorCopy( self->pos2, self->s.origin );
        gi.linkentity( self );
        self->pushMoveInfo.state = PUSHMOVE_STATE_BOTTOM;
    }

    // Spawn the trigger touch area entities.
    if ( self->spawnflags & svg_func_plat_t::SPAWNFLAG_HIGH_TRIGGER ) {
        self->SpawnInsideTrigger( true );  // the "top" trigger
    }
    if ( self->spawnflags & svg_func_plat_t::SPAWNFLAG_LOW_TRIGGER ) {
        self->SpawnInsideTrigger( false ); // the "bottom" trigger
    }

    // Animated doors:
    if ( SVG_HasSpawnFlags( self, svg_func_plat_t::SPAWNFLAG_ANIMATED ) ) {
        self->s.effects |= EF_ANIM_ALL;
    }
    if ( SVG_HasSpawnFlags( self, svg_func_plat_t::SPAWNFLAG_ANIMATED_FAST ) ) {
        self->s.effects |= EF_ANIM_ALLFAST;
    }

    #if 0 
    if ( self->targetname ) {
        self->pushMoveInfo.state = PUSHMOVE_STATE_MOVING_UP;
    } else {
        VectorCopy( self->pos2, self->s.origin );
        gi.linkentity( ent );
        self->pushMoveInfo.state = PUSHMOVE_STATE_BOTTOM;
    }
    #endif

    // Setup callbacks.
    self->SetBlockedCallback( &svg_func_plat_t::onBlocked /*plat_blocked*/ );
    self->SetUseCallback( &svg_func_plat_t::onUse /*Use_Plat*/ );

    // Setup PushMove Info.
    self->pushMoveInfo.speed = self->speed;
    self->pushMoveInfo.accel = self->accel;
    self->pushMoveInfo.decel = self->decel;
    self->pushMoveInfo.wait = self->wait;
    VectorCopy( self->pos1, self->pushMoveInfo.startOrigin );
    VectorCopy( self->s.angles, self->pushMoveInfo.startAngles );
    VectorCopy( self->pos2, self->pushMoveInfo.endOrigin );
    VectorCopy( self->s.angles, self->pushMoveInfo.endAngles );
}

/**
*	@brief  Spawns a trigger inside the plat, at
*           PLAT_LOW_TRIGGER and/or PLAT_HIGH_TRIGGER
*           when their spawnflags are set.
**/
void svg_func_plat_t::SpawnInsideTrigger( const bool isTop ) {
    // Spawn a func_plat_trigger.
    EdictTypeInfo *typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( "func_plat_trigger" );

    // Allocate it using the found typeInfo.
    svg_func_plat_trigger_t *triggerEdictInstance = static_cast<svg_func_plat_trigger_t *>( typeInfo->allocateEdictInstanceCallback( nullptr ) );

    // Emplace the spawned edict in the next avaible edict slot.
    g_edict_pool.EmplaceNextFreeEdict( triggerEdictInstance );
    // Spawn.
    triggerEdictInstance->SetSpawnCallback( &svg_func_plat_trigger_t::onSpawn /*onSpawn*/ );
    triggerEdictInstance->SetPostSpawnCallback( &svg_func_plat_trigger_t::onPostSpawn /*onSpawn*/ );
    // Assign before spawn so we can refer to the platform entity that 'owns' this trigger.
    triggerEdictInstance->platformEntityNumber = s.number;
    triggerEdictInstance->spawnflags |= ( isTop ? svg_func_plat_t::SPAWNFLAG_HIGH_TRIGGER : svg_func_plat_t::SPAWNFLAG_LOW_TRIGGER );
    triggerEdictInstance->DispatchSpawnCallback();
    if ( isTop ) {
        triggerEdictInstance->spawnflags |= svg_func_plat_t::SPAWNFLAG_HIGH_TRIGGER;
    } else {
        triggerEdictInstance->spawnflags |= svg_func_plat_t::SPAWNFLAG_LOW_TRIGGER;
    }
}

/**
*   @brief  Engage the platform to go up.
**/
void svg_func_plat_t::BeginUpMove() {
    // Play the start sound if not a team slave, as well as apply the middle movement sound to entity state.
    if ( !( flags & FL_TEAMSLAVE ) ) {
        if ( pushMoveInfo.sounds.start ) {
            gi.sound( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.sounds.start, 1, ATTN_STATIC, 0 );
        }
        s.sound = pushMoveInfo.sounds.middle;
    }
    // Set the state to moving down.
    pushMoveInfo.state = PUSHMOVE_STATE_MOVING_UP;
    // Engage the PushMover action.
    CalculateDirectionalMove( pushMoveInfo.startOrigin, reinterpret_cast<svg_pushmove_endcallback>( &this->onPlatHitTop ) );
    // Debug.
    gi.dprintf( "%s: Platform %s going up.\n", __func__, (const char *)classname.ptr );
}
/**
*   @brief  Engage the platform to go down.
**/
void svg_func_plat_t::BeginDownMove() {
    // Play the start sound if not a team slave, as well as apply the middle movement sound to entity state.
    if ( !( flags & FL_TEAMSLAVE ) ) {
        if ( pushMoveInfo.sounds.start ) {
            gi.sound( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.sounds.start, 1, ATTN_STATIC, 0 );
        }
        s.sound = pushMoveInfo.sounds.middle;
    }
    // Set the state to moving down.
    pushMoveInfo.state = PUSHMOVE_STATE_MOVING_DOWN;
    // Engage the PushMover action.
    CalculateDirectionalMove( pushMoveInfo.endOrigin, reinterpret_cast<svg_pushmove_endcallback>( &this->onPlatHitBottom ) );
    // Debug.
    gi.dprintf( "%s: Platform %s going down.\n", __func__, (const char *)classname.ptr );
}
