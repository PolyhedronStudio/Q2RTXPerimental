/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_plat'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_entity_events.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"

#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_plat.h"
#include "svgame/entities/svg_entities_pushermove.h"

#include "sharedgame/sg_entity_flags.h"
#include "sharedgame/sg_means_of_death.h"


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
*	@brief	Save descriptor array definition for all the members of svg_func_plat_trigger_t.
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
*	@brief	Reconstructs the object, optionally retaining the entityDictionary.
*	@param	retainDictionary	If true, the entityDictionary is kept between resets.
**/
void svg_func_plat_trigger_t::Reset( const bool retainDictionary ) {
	// Always reset base class first.
	Super::Reset( retainDictionary );

	// Reset all members owned by this type.
	platformEntityNumber = -1;
}

/**
*	@brief	Save the entity into a file using game_write_context.
*	@note	Make sure to call the base parent class' Save() function.
*	@param	ctx	Save context used to serialize the entity.
**/
void svg_func_plat_trigger_t::Save( struct game_write_context_t *ctx ) {
	// Save base class state first.
	Super::Save( ctx );

	// Save all members for this entity type.
	ctx->write_fields( svg_func_plat_trigger_t::saveDescriptorFields, this );
}
/**
*	@brief	Restore the entity from a loadgame read context.
*	@note	Make sure to call the base parent class' Restore() function.
*	@param	ctx	Load context used to deserialize the entity.
**/
void svg_func_plat_trigger_t::Restore( struct game_read_context_t *ctx ) {
	// Restore base class state first.
	Super::Restore( ctx );

	// Restore all members for this entity type.
	ctx->read_fields( svg_func_plat_trigger_t::saveDescriptorFields, this );
}



/**
*
*
*
*   Sound Handling:
*
*
*
**/
/**
*	@brief	Start the sound playback for the platform.
**/
void svg_func_plat_t::StartSoundPlayback() {
	// Do not play team sounds for slaves; only the master owns audio state.
	if ( !( flags & FL_TEAMSLAVE ) ) {
		// Play one-shot "start moving" sound if present.
		if ( pushMoveInfo.sounds.start ) {
			//SVG_TempEventEntity_GeneralSoundEx( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.sounds.start, ATTN_STATIC );
			SVG_EntityEvent_GeneralSoundEx( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.sounds.start, ATTN_STATIC );
		}

		// Set looping movement sound for clients.
		s.sound = pushMoveInfo.sounds.middle;
	}
}
/**
*	@brief	End the sound playback for the platform.
**/
void svg_func_plat_t::EndSoundPlayback() {
	// Do not play team sounds for slaves; only the master owns audio state.
	if ( !( flags & FL_TEAMSLAVE ) ) {
		// Play one-shot "stop moving" sound if present.
		if ( pushMoveInfo.sounds.end ) {
			//SVG_TempEventEntity_GeneralSoundEx( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.sounds.end, ATTN_STATIC );
			SVG_EntityEvent_GeneralSoundEx( this, CHAN_NO_PHS_ADD + CHAN_VOICE, pushMoveInfo.sounds.end, ATTN_STATIC );
		}

		// Clear looping movement sound.
		s.sound = 0;
	}
}



/**
*
* 
* 
* 
*   CallBack Member Functions:
* 
* 
* 
* 
**/
/**
*	@brief	Spawn.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_plat_trigger_t, onSpawn )( svg_func_plat_trigger_t *self ) -> void {
	// Always spawn Super class first.
	Super::onSpawn( self );

	// Initialize core trigger defaults (SOLID_TRIGGER, MOVETYPE_NONE, SVF_NOCLIENT, model if any).
	SVG_Util_InitTrigger( self );

	// Configure trigger callbacks.
    self->SetTouchCallback( &svg_func_plat_trigger_t::onTouch );
	self->SetPostSpawnCallback( &svg_func_plat_trigger_t::onPostSpawn );

	// Tag as a pusher/teleport style trigger.
	self->s.entityType = ET_PUSH_TRIGGER;

	BBox3 triggerAbsBounds = QM_BBox3ExpandVector3( { self->absMin, self->absMax }, { 25., 25., 0. } );
	triggerAbsBounds = QM_BBox3ExpandVector3( triggerAbsBounds, { -25., -25., 0. } );
	triggerAbsBounds = QM_BBox3ExpandVector3( triggerAbsBounds, { 25., 25., 8. } );

	// Update mins
	self->mins = triggerAbsBounds.mins;
	self->maxs = triggerAbsBounds.maxs;

	// Publish updated state to server collision world.
    gi.linkentity( self );
}
/**
*	@brief	PostSpawn.
**/
DEFINE_MEMBER_CALLBACK_POSTSPAWN( svg_func_plat_trigger_t, onPostSpawn )( svg_func_plat_trigger_t *self ) -> void {
    // Get platform owner entity.
    svg_base_edict_t *foundEntity = g_edict_pool.EdictForNumber( self->platformEntityNumber );

    // Validate platform entity.
    if ( !SVG_Entity_IsActive( foundEntity ) || !foundEntity->GetTypeInfo()->IsSubClassType<svg_func_plat_t>() ) {
        gi.dprintf( "%s: Invalid platform entity.\n", __func__ );
        return;
    }

    // Re-assert touch callback in case spawn logic replaced it.
    self->SetTouchCallback( &svg_func_plat_trigger_t::onTouch );

    // Acquire platform entity.
    svg_func_plat_t *platformEntity = static_cast<svg_func_plat_t *>( foundEntity );

	// Compute trigger bounds in world space. Include teamchain members.
	BBox3 triggerAbsBounds = { platformEntity->absMin, platformEntity->absMax };
	for ( svg_base_edict_t *chain = platformEntity->teamchain; chain; chain = chain->teamchain ) {
		AddPointToBounds( &chain->absMin.x, &triggerAbsBounds.mins.x, &triggerAbsBounds.maxs.x );
		AddPointToBounds( &chain->absMax.x, &triggerAbsBounds.mins.x, &triggerAbsBounds.maxs.x );
	}

	// Expand similarly to other pusher triggers.
	triggerAbsBounds = QM_BBox3ExpandVector3( triggerAbsBounds, { 25.f, 25.f, 0.f } );
	//triggerAbsBounds = QM_BBox3ExpandVector3( triggerAbsBounds, { -25.f, -25.f, 8.f } );

	// Select which end (top/bottom) the trigger is attached to.
	Vector3 triggerOrigin = platformEntity->pos2;
    if ( self->spawnflags & svg_func_plat_t::SPAWNFLAG_HIGH_TRIGGER ) {
        triggerOrigin = platformEntity->pos1;
	}

	// Add lip offset so the trigger sits at the pusher deck.
	triggerOrigin += Vector3{ 0.f, 0.f, platformEntity->lip };
	SVG_Util_SetEntityOrigin( self, triggerOrigin, true );

	// Build final bbox centered at origin (mins/maxs symmetric about 0).
	Vector3 triggerSize = QM_BBox3Size( triggerAbsBounds );
	// Ensure we always get a sensible trigger thickness; helps for very flat plats.
	if ( triggerSize[ 2 ] < 16.f ) {
		triggerSize[ 2 ] = 16.f;
	}
	BBox3 finalBounds = QM_BBox3FromCenterSize( triggerSize, QM_Vector3Zero() );
	// Apply final bbox to entity.
	self->mins = finalBounds.mins;
	self->maxs = finalBounds.maxs;

	// Publish updated bbox/origin to collision world.
    gi.linkentity( self );
}
/**
*	@brief	Touched.
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_func_plat_trigger_t, onTouch )( svg_func_plat_trigger_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
	
	gi.dprintf( "%s: Trigger touched by entity %i.\n", __func__, other->s.number );
	// Only clients can activate plat triggers.
	if ( !SVG_Entity_IsClient( other, true /*healthCheck*/) ) {
		return;
	}

	// Get platform entity by stored entity number.
	svg_base_edict_t *platPtr = g_edict_pool.EdictForNumber( self->platformEntityNumber );

	// Validate platform entity.
	if ( !SVG_Entity_IsActive( platPtr ) || !platPtr->GetTypeInfo()->IsSubClassType<svg_func_plat_t>() ) {
		gi.dprintf( "%s: Invalid platform entity %i.\n", __func__, self->platformEntityNumber );
		return;
	}

	// Acquire platform entity.
	svg_func_plat_t *platformEntity = static_cast<svg_func_plat_t *>( platPtr );

	// Prevent immediate re-triggering due to overlap/latching.
	if ( platformEntity->touch_debounce_time > level.time ) {
		return;
	}

	// Toggle platforms are use-only.
	if ( ( platformEntity->spawnflags & svg_func_plat_t::SPAWNFLAG_TOGGLE ) ) {
		return;
	}

	// Go up if at bottom, go down if at top.
	if ( platformEntity->pushMoveInfo.state == PUSHMOVE_STATE_BOTTOM ) {
		platformEntity->BeginUpMove( );
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

/**
*	@brief	Thinking.
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_func_plat_t, onThink )( svg_func_plat_t *self ) -> void { }
DEFINE_MEMBER_CALLBACK_THINK( svg_func_plat_t, onThink_SpawnInsideTrigger )( svg_func_plat_t *self ) -> void {
	// Spawn the trigger touch area entities.
	if ( self->spawnflags & svg_func_plat_t::SPAWNFLAG_HIGH_TRIGGER ) {
		self->SpawnInsideTrigger( true );  // the "top" trigger
	}
	if ( self->spawnflags & svg_func_plat_t::SPAWNFLAG_LOW_TRIGGER ) {
		self->SpawnInsideTrigger( false ); // the "bottom" trigger
	}

	// Engage idle thinking.
	self->onThink_Idle( self );
}
DEFINE_MEMBER_CALLBACK_THINK( svg_func_plat_t, onThink_Idle )( svg_func_plat_t *self ) -> void {
	// Reset to pusher so we can try moving again this frame.
	if ( self->spawnflags & SPAWNFLAG_BLOCK_STOPS ) {
		self->movetype = MOVETYPE_PUSH;
	}

	// Keep idling; a simple "delay before moving again" mechanism.
	self->SetThinkCallback( &svg_func_plat_t::onThink_Idle );
	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*	@brief	PushMoveInfo EndMove Callback for when the platform hits the top.
**/
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_plat_t, onPlatHitTop )( svg_func_plat_t *self ) -> void {
	// Stop sound playback.
	self->EndSoundPlayback();

	// Mark final position state.
	self->pushMoveInfo.state = PUSHMOVE_STATE_TOP;

	// Prevent immediate re-triggering if not toggle.
	if ( !( self->spawnflags & svg_func_plat_t::SPAWNFLAG_TOGGLE ) ) {
		self->touch_debounce_time = level.time + QMTime::FromSeconds( self->wait );
	}

	// Engage idle thinking; replaces classic "go down after delay" think func.
	#if 0
	ent->think = plat_go_down;
	ent->nextthink = level.time + 3_sec;
	#else
	self->onThink_Idle( self );
	#endif

	// Call into Lua hook (if available).
	sol::state_view &solStateView = SVG_Lua_GetSolStateView();
	auto leSelf = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( self ) );
	auto leOther = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( self->other ) );
	auto leActivator = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( self->activator ) );

	bool callReturnValue = false;
	bool calledFunction = LUA_CallLuaNameEntityFunction( self, "OnPlatformHitTop",
		solStateView,
		callReturnValue,
		leSelf, leOther, leActivator, ENTITY_USETARGET_TYPE_ON, 1
	);
}

/**
*	@brief	PushMoveInfo EndMove Callback for when the platform hits the bottom.
**/
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_plat_t, onPlatHitBottom )( svg_func_plat_t *self ) -> void {
	// Stop sound playback.
	self->EndSoundPlayback();

	// Mark final position state.
	self->pushMoveInfo.state = PUSHMOVE_STATE_BOTTOM;

	// Prevent immediate re-triggering if not toggle.
	if ( !( self->spawnflags & svg_func_plat_t::SPAWNFLAG_TOGGLE ) ) {
		self->touch_debounce_time = level.time + QMTime::FromSeconds( self->wait );
	}

	// Engage idle thinking.
	self->onThink_Idle( self );

	// Call into Lua hook (if available).
	sol::state_view &solStateView = SVG_Lua_GetSolStateView();
	auto leSelf = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( self ) );
	auto leOther = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( self->other ) );
	auto leActivator = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( self->activator ) );

	bool callReturnValue = false;
	bool calledFunction = LUA_CallLuaNameEntityFunction( self, "OnPlatformHitBottom",
		solStateView,
		callReturnValue, LUA_CALLFUNCTION_VERBOSE_MISSING,
		leSelf, leOther, leActivator, ENTITY_USETARGET_TYPE_OFF, 0
	);
}

/**
*	@brief	Blocked.
**/
DEFINE_MEMBER_CALLBACK_BLOCKED( svg_func_plat_t, onBlocked )( svg_func_plat_t *self, svg_base_edict_t *other ) -> void {
	if ( self->spawnflags & SPAWNFLAG_PAIN_ON_TOUCH ) {
		// Only apply crushing behavior when configured.
		if ( !( other->svFlags & SVF_MONSTER ) && ( !other->client ) ) {
			const bool knockBack = true;
			SVG_DamageEntity( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, knockBack, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );

			if ( other && other->inUse && other->solid ) { // PGM)
				SVG_Misc_BecomeExplosion( other, 1 );
			}
			return;
		}

		// PGM: gib dead things.
		if ( other->health < 1 ) {
			SVG_DamageEntity( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
		}

		const bool knockBack = false;
		SVG_DamageEntity( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );

		// If we killed/invalidated the entity, do not change direction.
		if ( !other->inUse || ( other->inUse && other->solid == SOLID_NOT ) ) {
			return;
		}
	}

	// If blocking stops is enabled, do not reverse; become MOVETYPE_STOP.
	if ( self->spawnflags & SPAWNFLAG_BLOCK_STOPS ) {
		self->movetype = MOVETYPE_STOP;
		return;
	}

	// Reverse direction depending on current state.
	if ( self->pushMoveInfo.state == PUSHMOVE_STATE_TOP || self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP ) {
		self->BeginDownMove();
	} else {
		self->BeginUpMove();
	}
}

/**
*	@brief	Used.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_func_plat_t, onUse )( svg_func_plat_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
	// Toggle functionality: invert direction depending on current move state.
	if ( self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_UP || self->pushMoveInfo.state == PUSHMOVE_STATE_TOP ) {
		self->BeginDownMove( );
	} else if ( self->pushMoveInfo.state == PUSHMOVE_STATE_MOVING_DOWN || self->pushMoveInfo.state == PUSHMOVE_STATE_BOTTOM ) {
		self->BeginUpMove( );
	}

	#if 0
	if ( ent->think ) {
		return;
	}
	plat_go_down( ent );
	#endif
}


/**
*	@brief	Spawn.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_plat_t, onSpawn )( svg_func_plat_t *self ) -> void {
    // Always spawn Super class.
    Super::onSpawn( self );

    // Plats don't care about orientation for movement.
	SVG_Util_SetEntityAngles( self, {}, true );

    // BSP pusher setup.
    self->solid = SOLID_BSP;
    self->movetype = MOVETYPE_PUSH;
    self->s.entityType = ET_PUSHER;

	// Bind model to establish bounds.
    gi.setmodel( self, self->model );

    // Set audio (hard-coded for now).
    self->pushMoveInfo.sounds.start = gi.soundindex( "pushers/plat_start_01.wav" );
    self->pushMoveInfo.sounds.middle = gi.soundindex( "pushers/plat_mid_01.wav" );
    self->pushMoveInfo.sounds.end = gi.soundindex( "pushers/plat_end_01.wav" );

	// SpawnKey Defaults:
	// Note: values appear to be authored in tenths for this entity (scaled by 0.1f when provided).
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

    // pos1 = top position, pos2 = bottom position.
	self->pos1 = self->currentOrigin; //VectorCopy( self->s.origin, self->pos1 );
	self->pos2 = self->currentOrigin; //VectorCopy( self->s.origin, self->pos2 );

	// Height can be specified, otherwise derive it from model bounds and lip.
    if ( self->height ) {
        self->pos2[ 2 ] -= self->height;
    } else {
        self->pos2[ 2 ] -= ( self->height = ( self->maxs[ 2 ] - self->mins[ 2 ] ) - self->lip );
    }

    // Default to being at the top.
    self->pushMoveInfo.state = PUSHMOVE_STATE_TOP;

    // If start-bottom flag is set, move BSP to pos2 and mark initial state.
    if ( self->spawnflags & svg_func_plat_t::SPAWNFLAG_START_BOTTOM ) {
		SVG_Util_SetEntityOrigin( self, self->pos2, true );
        gi.linkentity( self );
        self->pushMoveInfo.state = PUSHMOVE_STATE_BOTTOM;
    }

    // Spawn trigger volumes used for touch activation after the first
	// frames have passed, so it can use these edicts,

	// Only the team leader spawns a trigger.
	if ( ( self->flags & FL_TEAMSLAVE ) == 0 ) {
		onThink_SpawnInsideTrigger( self );
		self->SetThinkCallback( svg_func_plat_t::onThink_SpawnInsideTrigger );
		//self->nextthink = level.time + 25_ms;
	}

    // Animated doors:
    if ( SVG_HasSpawnFlags( self, svg_func_plat_t::SPAWNFLAG_ANIMATED ) ) {
        self->s.entityFlags |= EF_ANIM_ALL;
    }
    if ( SVG_HasSpawnFlags( self, svg_func_plat_t::SPAWNFLAG_ANIMATED_FAST ) ) {
        self->s.entityFlags |= EF_ANIM_ALLFAST;
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

    // Setup PushMoveInfo.
    self->pushMoveInfo.speed = self->speed;
    self->pushMoveInfo.accel = self->accel;
    self->pushMoveInfo.decel = self->decel;
    self->pushMoveInfo.wait = self->wait;
	self->pushMoveInfo.startOrigin = self->pos1; //VectorCopy( self->pos1, self->pushMoveInfo.startOrigin );
	self->pushMoveInfo.startAngles = self->s.angles; //VectorCopy( self->s.angles, self->pushMoveInfo.startAngles );
	self->pushMoveInfo.endOrigin = self->pos2; //VectorCopy( self->pos2, self->pushMoveInfo.endOrigin );
	self->pushMoveInfo.endAngles = self->s.angles; // VectorCopy( self->s.angles, self->pushMoveInfo.endAngles );
}

/**
*	@brief	Spawns a trigger inside the plat, at
*			PLAT_LOW_TRIGGER and/or PLAT_HIGH_TRIGGER
*			when their spawnflags are set.
**/
void svg_func_plat_t::SpawnInsideTrigger( const bool isTop ) {
	// Only the team leader spawns triggers.
	if ( flags & FL_TEAMSLAVE ) {
		return;
	}

	gi.dprintf( "%s: Spawning %s trigger for plat %s.\n", __func__, ( isTop ? "top" : "bottom" ), ( const char * )classname.ptr );
	/**
	*	Add it to the world.
	**/
	// Find type info for the trigger class by worldspawn classname.
	EdictTypeInfo *typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( "func_plat_trigger" );
	// Allocate trigger instance via the type system.
	svg_func_plat_trigger_t *triggerEdictInstance =	static_cast<svg_func_plat_trigger_t *>( typeInfo->allocateEdictInstanceCallback( nullptr ) );
	// Insert into the global edict pool.
	g_edict_pool.EmplaceNextFreeEdict( triggerEdictInstance );

	// Mark as active; runtime-spawned edicts may not have this set by default.
	triggerEdictInstance->inUse = true;	

	// Ensure the origin is set.
	SVG_Util_SetEntityOrigin( triggerEdictInstance, QM_BBox3Center( { absMin, absMax } ), true );

	// Setup callbacks before spawn.
	triggerEdictInstance->SetSpawnCallback( &svg_func_plat_trigger_t::onSpawn /*onSpawn*/ );
	triggerEdictInstance->SetPostSpawnCallback( &svg_func_plat_trigger_t::onPostSpawn /*onSpawn*/ );

	// Assign owner relationship before spawn so postspawn can compute bounds.
	triggerEdictInstance->platformEntityNumber = s.number;

	// Mark which trigger type we are spawning (top vs bottom).
	triggerEdictInstance->spawnflags |= ( isTop ? svg_func_plat_t::SPAWNFLAG_HIGH_TRIGGER : svg_func_plat_t::SPAWNFLAG_LOW_TRIGGER );

	// Keep the "double OR" (per your instruction).
	// This is defensive: if spawn clears spawnflags, we re-assert the intended mode.	
    if ( isTop ) {
        triggerEdictInstance->spawnflags |= svg_func_plat_t::SPAWNFLAG_HIGH_TRIGGER;
    } else {
        triggerEdictInstance->spawnflags |= svg_func_plat_t::SPAWNFLAG_LOW_TRIGGER;
    }

	//// Spawn the entity into the game.
	triggerEdictInstance->DispatchSpawnCallback();
	triggerEdictInstance->DispatchPostSpawnCallback();
}
	
/**
*	@brief	Engage the platform to go up.
**/
void svg_func_plat_t::BeginUpMove() {
	// Start audio + apply looping movement sound.
	StartSoundPlayback();

	// Set the state to moving up.
	pushMoveInfo.state = PUSHMOVE_STATE_MOVING_UP;

	// Begin movement toward startOrigin; callback when reaching the top.
	CalculateDirectionalMove( pushMoveInfo.startOrigin, reinterpret_cast<svg_pushmove_endcallback>( &this->onPlatHitTop ) );

	// Debug.
	gi.dprintf( "%s: Platform %s going up.\n", __func__, (const char *)classname.ptr );
}
/**
*	@brief	Engage the platform to go down.
**/
void svg_func_plat_t::BeginDownMove() {
	// Start audio + apply looping movement sound.
	StartSoundPlayback();

	// Set the state to moving down.
	pushMoveInfo.state = PUSHMOVE_STATE_MOVING_DOWN;

	// Begin movement toward endOrigin; callback when reaching the bottom.
	CalculateDirectionalMove( pushMoveInfo.endOrigin, reinterpret_cast<svg_pushmove_endcallback>( &this->onPlatHitBottom ) );

	// Debug.
	gi.dprintf( "%s: Platform %s going down.\n", __func__, (const char *)classname.ptr );
}
