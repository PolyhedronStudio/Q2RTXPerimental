/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "g_local.h"


/**
*	@brief	Basic Trigger initialization mechanism.
**/
void InitTrigger( edict_t *self ) {
	if ( !VectorEmpty( self->s.angles ) ) {
		G_SetMovedir( self->s.angles, self->movedir );
	}

	self->solid = SOLID_TRIGGER;
	self->movetype = MOVETYPE_NONE;
	if ( self->model ) {
		gi.setmodel( self, self->model );
	}
	self->svflags = SVF_NOCLIENT;
}


/***
*
*
*	trigger_multiple
*
*
***/
static constexpr int32_t SPAWNFLAG_TRIGGER_MULTIPLE_MONSTER = 1;
static constexpr int32_t SPAWNFLAG_TRIGGER_MULTIPLE_NOT_PLAYER = 2;
static constexpr int32_t SPAWNFLAG_TRIGGER_MULTIPLE_TRIGGERED = 4;
static constexpr int32_t SPAWNFLAG_TRIGGER_MULTIPLE_CLIPPED = 32;
/**
*	@brief	The wait time has passed, so set back up for another activation
**/
void multi_wait( edict_t *ent ) {
	ent->nextthink = 0_ms;
}


/**
*	@brief	The trigger was just activated
*			ent->activator should be set to the activator so it can be held through a delay
*			so wait for the delay time before firing
**/
void multi_trigger( edict_t *ent ) {
	if ( ent->nextthink )
		return;     // already been triggered

	G_UseTargets( ent, ent->activator );

	if ( ent->wait > 0 ) {
		ent->think = multi_wait;
		ent->nextthink = level.time + sg_time_t::from_sec( ent->wait );
	} else {
		// we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		ent->touch = NULL;
		ent->nextthink = level.time + 10_hz;
		ent->think = G_FreeEdict;
	}
}

/**
*	@brief	
**/
void Use_Multi( edict_t *ent, edict_t *other, edict_t *activator ) {
	ent->activator = activator;
	multi_trigger( ent );
}

/**
*	@brief
**/
void Touch_Multi( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
	if ( other->client ) {
		if ( self->spawnflags & SPAWNFLAG_TRIGGER_MULTIPLE_NOT_PLAYER ) {
			return;
		}
	} else if ( other->svflags & SVF_MONSTER ) {
		if ( !( self->spawnflags & SPAWNFLAG_TRIGGER_MULTIPLE_MONSTER ) ) {
			return;
		}
	} else {
		return;
	}

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_MULTIPLE_CLIPPED ) {
		trace_t clip = gi.clip( self, other->s.origin, other->mins, other->maxs, other->s.origin, G_GetClipMask( other ) );

		if ( clip.fraction == 1.0f ) {
			return;
		}
	}

	if ( !VectorEmpty( self->movedir ) ) {
		vec3_t  forward;

		AngleVectors( other->s.angles, forward, NULL, NULL );
		if ( DotProduct( forward, self->movedir ) < 0 ) {
			return;
		}
	}

	self->activator = other;
	multi_trigger( self );
}

/**
*	@brief
**/
void trigger_enable( edict_t *self, edict_t *other, edict_t *activator ) {
	self->solid = SOLID_TRIGGER;
	self->use = Use_Multi;
	gi.linkentity( self );
}

/*QUAKED trigger_multiple (.5 .5 .5) ? MONSTER NOT_PLAYER TRIGGERED
Variable sized repeatable trigger.  Must be targeted at one or more entities.
If "delay" is set, the trigger waits some time after activating before firing.
"wait" : Seconds between triggerings. (.2 default)
sounds
1)  secret
2)  beep beep
3)  large switch
4)
set "message" to text string
*/
void SP_trigger_multiple( edict_t *ent ) {
	if ( ent->sounds == 1 )
		ent->noise_index = gi.soundindex( "misc/secret.wav" );
	else if ( ent->sounds == 2 )
		ent->noise_index = gi.soundindex( "misc/talk.wav" );
	else if ( ent->sounds == 3 )
		ent->noise_index = gi.soundindex( "misc/trigger1.wav" );

	if ( !ent->wait )
		ent->wait = 0.2f;

	// WID: Initialize triggers properly.
	InitTrigger( ent );

	ent->touch = Touch_Multi;
	ent->movetype = MOVETYPE_NONE;
	ent->svflags |= SVF_NOCLIENT;

	if ( ent->spawnflags & SPAWNFLAG_TRIGGER_MULTIPLE_TRIGGERED ) {
		ent->solid = SOLID_NOT;
		ent->use = trigger_enable;
	} else {
		ent->solid = SOLID_TRIGGER;
		ent->use = Use_Multi;
	}

	if ( !VectorEmpty( ent->s.angles ) )
		G_SetMovedir( ent->s.angles, ent->movedir );

	gi.linkentity( ent );

	if ( ent->spawnflags & SPAWNFLAG_TRIGGER_MULTIPLE_CLIPPED ) {
		ent->svflags |= SVF_HULL;
	}
}



/***
*
*
*	trigger_once
*
*
***/
static constexpr int32_t SPAWNFLAG_TRIGGER_ONCE_TRIGGERED = 4;

/*QUAKED trigger_once (.5 .5 .5) ? x x TRIGGERED
Triggers once, then removes itself.
You must set the key "target" to the name of another object in the level that has a matching "targetname".

If TRIGGERED, this trigger must be triggered before it is live.

sounds
 1) secret
 2) beep beep
 3) large switch
 4)

"message"   string to be displayed when triggered
*/
void SP_trigger_once( edict_t *ent ) {
	// make old maps work because I messed up on flag assignments here
	// triggered was on bit 1 when it should have been on bit 4
	if ( ent->spawnflags & 1 ) {
		vec3_t  v;

		VectorMA( ent->mins, 0.5f, ent->size, v );
		ent->spawnflags &= ~1;
		ent->spawnflags |= SPAWNFLAG_TRIGGER_ONCE_TRIGGERED;
		gi.dprintf( "fixed TRIGGERED flag on %s at %s\n", ent->classname, vtos( v ) );
	}

	ent->wait = -1;
	SP_trigger_multiple( ent );
}



/***
*
*
*	trigger_relay
*
*
***/
/**
*	@brief
**/
void trigger_relay_use( edict_t *self, edict_t *other, edict_t *activator ) {
	G_UseTargets( self, activator );
}

/*QUAKED trigger_relay (.5 .5 .5) (-8 -8 -8) (8 8 8)
This fixed size trigger cannot be touched, it can only be fired by other events.
*/
void SP_trigger_relay( edict_t *self ) {
	self->use = trigger_relay_use;
}



/***
*
*
*	trigger_key
*
*
***/
/**
*	@brief
**/
void trigger_key_use( edict_t *self, edict_t *other, edict_t *activator ) {
	int         index;

	if ( !self->item ) {
		return;
	}
	if ( !activator->client ) {
		return;
	}

	index = ITEM_INDEX( self->item );
	if ( !activator->client->pers.inventory[ index ] ) {
		if ( level.time < self->touch_debounce_time ) {
			return;
		}
		self->touch_debounce_time = level.time + 5_sec;
		gi.centerprintf( activator, "You need the %s", self->item->pickup_name );
		gi.sound( activator, CHAN_AUTO, gi.soundindex( "misc/keytry.wav" ), 1, ATTN_NORM, 0 );
		return;
	}

	gi.sound( activator, CHAN_AUTO, gi.soundindex( "misc/keyuse.wav" ), 1, ATTN_NORM, 0 );
	if ( coop->value ) {
		int     player;
		edict_t *ent;

		if ( strcmp( self->item->classname, "key_power_cube" ) == 0 ) {
			int cube;

			for ( cube = 0; cube < 8; cube++ ) {
				if ( activator->client->pers.power_cubes & ( 1 << cube ) ) {
					break;
				}
			}
			for ( player = 1; player <= game.maxclients; player++ ) {
				ent = &g_edicts[ player ];
				if ( !ent->inuse ) {
					continue;
				}
				if ( !ent->client ) {
					continue;
				}
				if ( ent->client->pers.power_cubes & ( 1 << cube ) ) {
					ent->client->pers.inventory[ index ]--;
					ent->client->pers.power_cubes &= ~( 1 << cube );
				}
			}
		} else {
			for ( player = 1; player <= game.maxclients; player++ ) {
				ent = &g_edicts[ player ];
				if ( !ent->inuse ) {
					continue;
				}
				if ( !ent->client ) {
					continue;
				}
				ent->client->pers.inventory[ index ] = 0;
			}
		}
	} else {
		activator->client->pers.inventory[ index ]--;
	}

	G_UseTargets( self, activator );

	self->use = NULL;
}

/*QUAKED trigger_key (.5 .5 .5) (-8 -8 -8) (8 8 8)
A relay trigger that only fires it's targets if player has the proper key.
Use "item" to specify the required key, for example "key_data_cd"
*/
void SP_trigger_key( edict_t *self ) {
	if ( !st.item ) {
		gi.dprintf( "no key item for trigger_key at %s\n", vtos( self->s.origin ) );
		return;
	}
	self->item = FindItemByClassname( st.item );

	if ( !self->item ) {
		gi.dprintf( "item %s not found for trigger_key at %s\n", st.item, vtos( self->s.origin ) );
		return;
	}

	if ( !self->target ) {
		gi.dprintf( "%s at %s has no target\n", self->classname, vtos( self->s.origin ) );
		return;
	}

	gi.soundindex( "misc/keytry.wav" );
	gi.soundindex( "misc/keyuse.wav" );

	self->use = trigger_key_use;
}



/***
*
*
*	trigger_counter
*
*
***/
static constexpr int32_t SPAWNPFLAG_TRIGGER_COUNTER_NO_MESSAGE = 1;

/**
*	@brief
**/
void trigger_counter_use( edict_t *self, edict_t *other, edict_t *activator ) {
	if ( self->count == 0 ) {
		return;
	}

	self->count--;

	if ( self->count ) {
		if ( !( self->spawnflags & SPAWNPFLAG_TRIGGER_COUNTER_NO_MESSAGE ) ) {
			gi.centerprintf( activator, "%i more to go...", self->count );
			gi.sound( activator, CHAN_AUTO, gi.soundindex( "misc/talk1.wav" ), 1, ATTN_NORM, 0 );
		}
		return;
	}

	if ( !( self->spawnflags & SPAWNPFLAG_TRIGGER_COUNTER_NO_MESSAGE ) ) {
		gi.centerprintf( activator, "Sequence completed!" );
		gi.sound( activator, CHAN_AUTO, gi.soundindex( "misc/talk1.wav" ), 1, ATTN_NORM, 0 );
	}
	self->activator = activator;
	multi_trigger( self );
}

/*QUAKED trigger_counter (.5 .5 .5) ? nomessage
Acts as an intermediary for an action that takes multiple inputs.

If nomessage is not set, t will print "1 more.. " etc when triggered and "sequence complete" when finished.

After the counter has been triggered "count" times (default 2), it will fire all of it's targets and remove itself.
*/
void SP_trigger_counter( edict_t *self ) {
	self->wait = -1;
	if ( !self->count ) {
		self->count = 2;
	}

	self->use = trigger_counter_use;
}



/***
*
*
*	trigger_always
*
*
***/
/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
This trigger will always fire.  It is activated by the world.
*/
void SP_trigger_always( edict_t *ent ) {
	// we must have some delay to make sure our use targets are present
	if ( !ent->delay ) {
		ent->delay = 0.2f;
	}
	G_UseTargets( ent, ent );
}



/***
*
*
*	trigger_push
*
*
***/
static constexpr int32_t SPAWNFLAG_TRIGGER_PUSH_PUSH_ONCE	= 1;
static constexpr int32_t SPAWNFLAG_TRIGGER_PUSH_CLIPPED		= 32;

static int windsound = 0;

/**
*	@brief
**/
void trigger_push_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
	
	if ( self->spawnflags & SPAWNFLAG_TRIGGER_PUSH_CLIPPED ) {
		trace_t clip = gi.clip( self, other->s.origin, other->mins, other->maxs, other->s.origin, G_GetClipMask( other ) );

		if ( clip.fraction == 1.0f ) {
			return;
		}
	}

	if ( strcmp( other->classname, "grenade" ) == 0 ) {
		VectorScale( self->movedir, self->speed * 10, other->velocity );
	} else if ( other->health > 0 ) {
		VectorScale( self->movedir, self->speed * 10, other->velocity );

		if ( other->client ) {
			// don't take falling damage immediately from this
			VectorCopy( other->velocity, other->client->oldvelocity );
			other->client->oldgroundentity = other->groundentity;

			if ( other->fly_sound_debounce_time < level.time ) {
				other->fly_sound_debounce_time = level.time + 1.5_sec;
				gi.sound( other, CHAN_AUTO, windsound, 1, ATTN_NORM, 0 );
			}
		}
	}
	if ( self->spawnflags & SPAWNFLAG_TRIGGER_PUSH_PUSH_ONCE ) {
		G_FreeEdict( self );
	}
}


/*QUAKED trigger_push (.5 .5 .5) ? PUSH_ONCE
Pushes the player
"speed"     defaults to 1000
*/
void SP_trigger_push( edict_t *self ) {
	// WID: Initialize triggers properly.
	InitTrigger( self );

	if ( !windsound ) {
		windsound = gi.soundindex( "misc/windfly.wav" );
	}
	self->touch = trigger_push_touch;
	if ( !self->speed ) {
		self->speed = 1000;
	}
	gi.linkentity( self );

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_PUSH_CLIPPED ) {
		self->svflags |= SVF_HULL;
	}
}



/***
*
*
*	trigger_hurt
*
*
***/
static constexpr int32_t SPAWNFLAG_TRIGGER_HURT_START_OFF = 1;
static constexpr int32_t SPAWNFLAG_TRIGGER_HURT_TOGGLE = 2;
static constexpr int32_t SPAWNFLAG_TRIGGER_HURT_SILENT = 4;
static constexpr int32_t SPAWNFLAG_TRIGGER_HURT_NO_PROTECTION = 8;
static constexpr int32_t SPAWNFLAG_TRIGGER_HURT_SLOW_HURT = 16;
static constexpr int32_t SPAWNFLAG_TRIGGER_HURT_CLIPPED = 32;

/**
*	@brief
**/
void hurt_use( edict_t *self, edict_t *other, edict_t *activator ) {
	if ( self->solid == SOLID_NOT ) {
		self->solid = SOLID_TRIGGER;
	} else {
		self->solid = SOLID_NOT;
	}
	gi.linkentity( self );

	if ( !( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_TOGGLE ) ) {
		self->use = NULL;
	}
}

/**
*	@brief	
**/
void hurt_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
	int     dflags;

	if ( !other->takedamage ) {
		return;
	}

	if ( self->timestamp > level.time ) {
		return;
	}

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_CLIPPED ) {
		trace_t clip = gi.clip( self, other->s.origin, other->mins, other->maxs, other->s.origin, G_GetClipMask( other ) );

		if ( clip.fraction == 1.0f ) {
			return;
		}
	}

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_SLOW_HURT ) {
		self->timestamp = level.time + 1_sec;
	} else {
		self->timestamp = level.time + 10_hz;
	}

	if ( !( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_SILENT ) ) {
		if ( self->fly_sound_debounce_time < level.time ) {
			gi.sound( other, CHAN_AUTO, self->noise_index, 1, ATTN_NORM, 0 );
			self->fly_sound_debounce_time = level.time + 1_sec;
		}
	}

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_NO_PROTECTION )
		dflags = DAMAGE_NO_PROTECTION;
	else
		dflags = 0;
	T_Damage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, self->dmg, dflags, MOD_TRIGGER_HURT );
}

/*QUAKED trigger_hurt (.5 .5 .5) ? START_OFF TOGGLE SILENT NO_PROTECTION SLOW
Any entity that touches this will be hurt.

It does dmg points of damage each server frame

SILENT          supresses playing the sound
SLOW            changes the damage rate to once per second
NO_PROTECTION   *nothing* stops the damage

"dmg"           default 5 (whole numbers only)

*/
void SP_trigger_hurt( edict_t *self ) {
	// WID: Initialize triggers properly.
	InitTrigger( self );

	self->noise_index = gi.soundindex( "world/electro.wav" );
	self->touch = hurt_touch;

	if ( !self->dmg )
		self->dmg = 5;

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_START_OFF )
		self->solid = SOLID_NOT;
	else
		self->solid = SOLID_TRIGGER;

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_TOGGLE )
		self->use = hurt_use;

	gi.linkentity( self );

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_HURT_CLIPPED ) {
		self->svflags |= SVF_HULL;
	}
}



/***
*
*
*	trigger_gravity
*
*
***/
static constexpr int32_t SPAWNFLAG_TRIGGER_GRAVITY_CLIPPED = 32;

/**
*	@brief	Touch callback in order to change the gravity of 'other', the touching entity.
**/
void trigger_gravity_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
	if ( self->spawnflags & SPAWNFLAG_TRIGGER_GRAVITY_CLIPPED ) {
		trace_t clip = gi.clip( self, other->s.origin, other->mins, other->maxs, other->s.origin, G_GetClipMask( other ) );

		if ( clip.fraction == 1.0f ) {
			return;
		}
	}

	other->gravity = self->gravity;
}

/*QUAKED trigger_gravity (.5 .5 .5) ?
Changes the touching entites gravity to
the value of "gravity".  1.0 is standard
gravity for the level.
*/
void SP_trigger_gravity( edict_t *self ) {
	if ( st.gravity == NULL ) {
		gi.dprintf( "trigger_gravity without gravity set at %s\n", vtos( self->s.origin ) );
		G_FreeEdict( self );
		return;
	}

	InitTrigger( self );

	self->gravity = atof( st.gravity );
	self->touch = trigger_gravity_touch;

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_GRAVITY_CLIPPED ) {
		self->svflags |= SVF_HULL;
	}
}


/***
*
*
*	trigger_monsterjump
*
*
***/
static constexpr int32_t SPAWNFLAG_TRIGGER_MONSTERJUMP_CLIPPED = 32;
/**
*	@brief	
**/
void trigger_monsterjump_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
	if ( other->flags & ( FL_FLY | FL_SWIM ) ) {
		return;
	}
	if ( other->svflags & SVF_DEADMONSTER ) {
		return;
	}
	if ( !( other->svflags & SVF_MONSTER ) ) {
		return;
	}

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_MONSTERJUMP_CLIPPED ) {
		trace_t clip = gi.clip( self, other->s.origin, other->mins, other->maxs, other->s.origin, G_GetClipMask( other ) );

		if ( clip.fraction == 1.0f ) {
			return;
		}
	}

	// set XY even if not on ground, so the jump will clear lips
	other->velocity[ 0 ] = self->movedir[ 0 ] * self->speed;
	other->velocity[ 1 ] = self->movedir[ 1 ] * self->speed;

	if ( !other->groundentity ) {
		return;
	}

	other->groundentity = NULL;
	other->velocity[ 2 ] = self->movedir[ 2 ];
}

/*QUAKED trigger_monsterjump (.5 .5 .5) ?
Walking monsters that touch this will jump in the direction of the trigger's angle
"speed" default to 200, the speed thrown forward
"height" default to 200, the speed thrown upwards
*/
void SP_trigger_monsterjump( edict_t *self ) {
	if ( !self->speed ) {
		self->speed = 200;
	}
	if ( !st.height ) {
		st.height = 200;
	}
	if ( self->s.angles[ YAW ] == 0 ) {
		self->s.angles[ YAW ] = 360;
	}
	InitTrigger( self );
	self->touch = trigger_monsterjump_touch;
	self->movedir[ 2 ] = st.height;

	if ( self->spawnflags & SPAWNFLAG_TRIGGER_MONSTERJUMP_CLIPPED ) {
		self->svflags |= SVF_HULL;
	}
}

