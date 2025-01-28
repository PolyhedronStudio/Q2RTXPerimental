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


//
// monster weapons
//

//FIXME mosnters should call these with a totally accurate direction
// and we can mess it up based on skill.  Spread should be for normal
// and we can tighten or loosen based on skill.  We could muck with
// the damages too, but I'm not sure that's such a good idea.
void monster_fire_bullet( edict_t *self, vec3_t start, vec3_t dir, int damage, int kick, int hspread, int vspread, int flashtype ) {
	fire_bullet( self, start, dir, damage, kick, hspread, vspread, MOD_UNKNOWN );

	gi.WriteUint8( svc_muzzleflash2 );
	gi.WriteInt16( self - g_edicts );
	gi.WriteUint8( flashtype );
	gi.multicast( start, MULTICAST_PHS, false );
}

void monster_fire_shotgun( edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int flashtype ) {
	fire_shotgun( self, start, aimdir, damage, kick, hspread, vspread, count, MOD_UNKNOWN );

	gi.WriteUint8( svc_muzzleflash2 );
	gi.WriteInt16( self - g_edicts );
	gi.WriteUint8( flashtype );
	gi.multicast( start, MULTICAST_PHS, false );
}

void monster_fire_blaster( edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype, int effect ) {
	fire_blaster( self, start, dir, damage, speed, effect, false );

	gi.WriteUint8( svc_muzzleflash2 );
	gi.WriteInt16( self - g_edicts );
	gi.WriteUint8( flashtype );
	gi.multicast( start, MULTICAST_PHS, false );
}

void monster_fire_grenade(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int flashtype)
{
    fire_grenade(self, start, aimdir, damage, speed, sg_time_t::from_sec( 2.5f ), damage + 40);

	gi.WriteUint8( svc_muzzleflash2 );
	gi.WriteInt16( self - g_edicts );
	gi.WriteUint8( flashtype );
	gi.multicast( start, MULTICAST_PHS, false );
}

void monster_fire_rocket( edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype ) {
	fire_rocket( self, start, dir, damage, speed, damage + 20, damage );

	gi.WriteUint8( svc_muzzleflash2 );
	gi.WriteInt16( self - g_edicts );
	gi.WriteUint8( flashtype );
	gi.multicast( start, MULTICAST_PHS, false );
}

void monster_fire_railgun( edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int flashtype ) {
	contents_t contents = gi.pointcontents( start );
	if ( contents & (const contents_t)MASK_SOLID )
		return;

	fire_rail( self, start, aimdir, damage, kick );

	gi.WriteUint8( svc_muzzleflash2 );
	gi.WriteInt16( self - g_edicts );
	gi.WriteUint8( flashtype );
	gi.multicast( start, MULTICAST_PHS, false );
}

void monster_fire_bfg( edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int kick, float damage_radius, int flashtype ) {
	fire_bfg( self, start, aimdir, damage, speed, damage_radius );

	gi.WriteUint8( svc_muzzleflash2 );
	gi.WriteInt16( self - g_edicts );
	gi.WriteUint8( flashtype );
	gi.multicast( start, MULTICAST_PHS, false );
}



//
// Monster utility functions
//

void M_FliesOff( edict_t *self ) {
	self->s.effects &= ~EF_FLIES;
	self->s.sound = 0;
}

void M_FliesOn(edict_t *self)
{
    if (self->liquidlevel)
        return;
    self->s.effects |= EF_FLIES;
    self->s.sound = gi.soundindex("infantry/inflies1.wav");
    self->think = M_FliesOff;
    self->nextthink = level.time + 60_sec;
}

void M_FlyCheck( edict_t *self ) {
	if ( self->liquidlevel )
		return;

	if ( random( ) > 0.5f )
		return;

    self->think = M_FliesOn;
    self->nextthink = level.time + random_time(5_sec, 10_sec );// + ( 5 + 10 * random( ) ) * BASE_FRAMERATE;
}

void AttackFinished(edict_t *self, sg_time_t time)
{
    self->monsterinfo.attack_finished = level.time + time;
}


void M_CheckGround( edict_t *ent, const contents_t mask ) {
	vec3_t      point;
	trace_t     trace;

	// Swimming and flying monsters don't check for ground.
	if ( ent->flags & ( FL_SWIM | FL_FLY ) ) {
		return;
	}

	// Too high of a velocity, we're moving upwards rapidly.
	if ( ent->velocity[ 2 ] > 100 ) {
		ent->groundentity = nullptr;
		return;
	}

	// If the hull point one-quarter unit down is solid the entity is on ground.
	point[ 0 ] = ent->s.origin[ 0 ];
	point[ 1 ] = ent->s.origin[ 1 ];
	point[ 2 ] = ent->s.origin[ 2 ] - 0.25f;

	trace = gi.trace( ent->s.origin, ent->mins, ent->maxs, point, ent, mask );

	// check steepness
	if ( trace.plane.normal[ 2 ] < 0.7f && !trace.startsolid ) {
		ent->groundentity = nullptr;
		return;
	}

//  ent->groundentity = trace.ent;
//  ent->groundentity_linkcount = trace.ent->linkcount;
//  if (!trace.startsolid && !trace.allsolid)
//      VectorCopy (trace.endpos, ent->s.origin);
	if ( !trace.startsolid && !trace.allsolid ) {
		VectorCopy( trace.endpos, ent->s.origin );
		ent->groundentity = trace.ent;
		ent->groundentity_linkcount = trace.ent->linkcount;
		ent->velocity[ 2 ] = 0;
	}
}


void M_CatagorizePosition( edict_t *ent, const Vector3 &in_point, liquid_level_t &liquidlevel, contents_t &liquidtype ) {
	//
	// get liquidlevel
	//
	Vector3 point = in_point + Vector3{ 0.f, 0.f, ent->mins[ 2 ] + 1 };
	contents_t cont = gi.pointcontents( &point.x );

	if ( !( cont & MASK_WATER ) ) {
		liquidlevel = liquid_level_t::LIQUID_NONE;
		liquidtype = CONTENTS_NONE;
		return;
	}

	liquidtype = cont;
	liquidlevel = liquid_level_t::LIQUID_FEET;
	point.z += 26;
	cont = gi.pointcontents( &point.x );
	if ( !( cont & MASK_WATER ) ) {
		return;
	}

	liquidlevel = liquid_level_t::LIQUID_WAIST;
	point[ 2 ] += 22;
	cont = gi.pointcontents( &point.x );
	if ( cont & MASK_WATER ) {
		liquidlevel = liquid_level_t::LIQUID_UNDER;
	}
}


void M_WorldEffects( edict_t *ent ) {
	int     dmg;

    if (ent->health > 0) {
        if (!(ent->flags & FL_SWIM)) {
            if (ent->liquidlevel < 3) {
                ent->air_finished_time = level.time + 12_sec;
            } else if (ent->air_finished_time < level.time) {
                // drown!
                if (ent->pain_debounce_time < level.time ) {
					dmg = 2 + (int)( 2 * floorf( ( level.time - ent->air_finished_time ).seconds( ) ) );
                    if (dmg > 15)
                        dmg = 15;
                    SVG_TriggerDamage(ent, world, world, vec3_origin, ent->s.origin, vec3_origin, dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
                    ent->pain_debounce_time = level.time + 1_sec;
                }
            }
        } else {
            if (ent->liquidlevel > 0) {
                ent->air_finished_time = level.time + 9_sec;
            } else if (ent->air_finished_time < level.time ) {
                // suffocate!
                if (ent->pain_debounce_time < level.time ) {
					dmg = 2 + (int)( 2 * floorf( ( level.time - ent->air_finished_time ).seconds( ) ) );
                    if (dmg > 15)
                        dmg = 15;
                    SVG_TriggerDamage(ent, world, world, vec3_origin, ent->s.origin, vec3_origin, dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
                    ent->pain_debounce_time = level.time + 1_sec;
                }
            }
        }
    }

	if ( ent->liquidlevel == 0 ) {
		if ( ent->flags & FL_INWATER ) {
			gi.sound( ent, CHAN_BODY, gi.soundindex( "player/watr_out.wav" ), 1, ATTN_NORM, 0 );
			ent->flags = static_cast<entity_flags_t>( ent->flags & ~FL_INWATER );
		}
		return;
	}

    if ((ent->liquidtype & CONTENTS_LAVA) && !(ent->flags & FL_IMMUNE_LAVA)) {
        if (ent->damage_debounce_time < level.time ) {
            ent->damage_debounce_time = level.time + 0.2_sec;
            SVG_TriggerDamage(ent, world, world, vec3_origin, ent->s.origin, vec3_origin, 10 * ent->liquidlevel, 0, 0, MOD_LAVA);
        }
    }
    if ((ent->liquidtype & CONTENTS_SLIME) && !(ent->flags & FL_IMMUNE_SLIME)) {
        if (ent->damage_debounce_time < level.time ) {
            ent->damage_debounce_time = level.time + 1_sec;
            SVG_TriggerDamage(ent, world, world, vec3_origin, ent->s.origin, vec3_origin, 4 * ent->liquidlevel, 0, 0, MOD_SLIME);
        }
    }

	if ( !( ent->flags & FL_INWATER ) ) {
		if ( !( ent->svflags & SVF_DEADMONSTER ) ) {
			if ( ent->liquidtype & CONTENTS_LAVA )
				if ( random( ) <= 0.5f )
					gi.sound( ent, CHAN_BODY, gi.soundindex( "player/lava1.wav" ), 1, ATTN_NORM, 0 );
				else
					gi.sound( ent, CHAN_BODY, gi.soundindex( "player/lava2.wav" ), 1, ATTN_NORM, 0 );
			else if ( ent->liquidtype & CONTENTS_SLIME )
				gi.sound( ent, CHAN_BODY, gi.soundindex( "player/watr_in.wav" ), 1, ATTN_NORM, 0 );
			else if ( ent->liquidtype & CONTENTS_WATER )
				gi.sound( ent, CHAN_BODY, gi.soundindex( "player/watr_in.wav" ), 1, ATTN_NORM, 0 );
		}

		ent->flags = static_cast<entity_flags_t>( ent->flags | FL_INWATER );

        ent->damage_debounce_time = 0_ms;
    }
}


void M_droptofloor( edict_t *ent ) {
	vec3_t      end;
	trace_t     trace;

	contents_t mask = SVG_GetClipMask( ent );

	ent->s.origin[ 2 ] += 1;
	VectorCopy( ent->s.origin, end );
	end[ 2 ] -= 256;

	trace = gi.trace( ent->s.origin, ent->mins, ent->maxs, end, ent, mask );

	if ( trace.fraction == 1 || trace.allsolid ) {
		return;
	}

	VectorCopy( trace.endpos, ent->s.origin );

	gi.linkentity( ent );
	M_CheckGround( ent, mask );
	M_CatagorizePosition( ent, ent->s.origin, ent->liquidlevel, ent->liquidtype );
}


void M_SetEffects( edict_t *ent ) {
	ent->s.effects &= ~( EF_COLOR_SHELL | EF_POWERSCREEN );
	ent->s.renderfx &= ~( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE );

	if ( ent->monsterinfo.aiflags & AI_RESURRECTING ) {
		ent->s.effects |= EF_COLOR_SHELL;
		ent->s.renderfx |= RF_SHELL_RED;
	}

	if ( ent->health <= 0 )
		return;

    if (ent->powerarmor_time > level.time ) {
        if (ent->monsterinfo.power_armor_type == POWER_ARMOR_SCREEN) {
            ent->s.effects |= EF_POWERSCREEN;
        } else if (ent->monsterinfo.power_armor_type == POWER_ARMOR_SHIELD) {
            ent->s.effects |= EF_COLOR_SHELL;
            ent->s.renderfx |= RF_SHELL_GREEN;
        }
    }
}

void M_SetAnimation( edict_t *self, mmove_t *move, bool instant ) {
	// [Paril-KEX] free the beams if we switch animations.
	//if ( self->beam ) {
	//	SVG_FreeEdict( self->beam );
	//	self->beam = nullptr;
	//}

	//if ( self->beam2 ) {
	//	SVG_FreeEdict( self->beam2 );
	//	self->beam2 = nullptr;
	//}

	// instant switches will cause active_move to change on the next frame
	if ( instant ) {
		self->monsterinfo.currentmove = move;
		self->monsterinfo.nextmove = nullptr;
		return;
	}

	// these wait until the frame is ready to be finished
	self->monsterinfo.nextmove = move;
}

void M_MoveFrame(edict_t *self)
{
	mmove_t *move = self->monsterinfo.currentmove;

	// [Paril-KEX] high tick rate adjustments;
	// monsters still only step frames and run thinkfunc's at
	// 10hz, but will run aifuncs at full speed with
	// distance spread over 10hz
	self->nextthink = level.time + FRAME_TIME_S;

	// time to run next 10hz move yet?
	bool run_frame = self->monsterinfo.next_move_time <= level.time;

	// we asked nicely to switch frames when the timer ran up
	if ( run_frame && self->monsterinfo.nextmove && self->monsterinfo.currentmove != self->monsterinfo.nextmove ) {
		M_SetAnimation( self, self->monsterinfo.nextmove, true );
		move = self->monsterinfo.currentmove;
	}

	if ( !move ) {
		return;
	}

	// no, but maybe we were explicitly forced into another move (pain,
	// death, etc)
	if ( !run_frame ) {
		run_frame = ( self->s.frame < move->firstframe || self->s.frame > move->lastframe );
	}

	if ( run_frame ) {
		// [Paril-KEX] allow next_move and nextframe to work properly after an endfunc
		bool explicit_frame = false;
		if ( ( self->monsterinfo.nextframe ) && ( self->monsterinfo.nextframe >= move->firstframe ) 
			 && ( self->monsterinfo.nextframe <= move->lastframe ) ) {

			self->s.frame = self->monsterinfo.nextframe;
			self->monsterinfo.nextframe = 0;

		} else {
			if ( self->s.frame == move->lastframe ) {
				if ( move->endfunc ) {
					move->endfunc( self );

					if ( self->monsterinfo.nextmove ) {
						M_SetAnimation( self, self->monsterinfo.nextmove, true );

						if ( self->monsterinfo.nextframe ) {
							self->s.frame = self->monsterinfo.nextframe;
							self->monsterinfo.nextframe = 0;
							explicit_frame = true;
						}
					}

					// regrab move, endfunc is very likely to change it
					move = self->monsterinfo.currentmove;

					// check for death
					if ( self->svflags & SVF_DEADMONSTER ) {
						return;
					}
				}
			}

			if ( self->s.frame < move->firstframe || self->s.frame > move->lastframe ) {
				self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
				self->s.frame = move->firstframe;
			} else if ( !explicit_frame ) {
				if ( !( self->monsterinfo.aiflags & AI_HOLD_FRAME ) ) {
					self->s.frame++;
					if ( self->s.frame > move->lastframe ) {
						self->s.frame = move->firstframe;
					}
				}
			}
		}

		if ( self->monsterinfo.aiflags & AI_HIGH_TICK_RATE )
			self->monsterinfo.next_move_time = level.time;
		else
			self->monsterinfo.next_move_time = level.time + 10_hz;

		if ( ( self->monsterinfo.nextframe ) && !( ( self->monsterinfo.nextframe >= move->firstframe ) &&
			 ( self->monsterinfo.nextframe <= move->lastframe ) ) ) {
			self->monsterinfo.nextframe = 0;
		}
	}

	// NB: frame thinkfunc can be called on the same frame
	// as the animation changing
	int32_t index = self->s.frame - move->firstframe;
	if ( move->frame[ index ].aifunc ) {
		if ( !( self->monsterinfo.aiflags & AI_HOLD_FRAME ) ) {
			float dist = move->frame[ index ].dist * self->monsterinfo.scale;
			dist /= gi.tick_rate / 10;
			move->frame[ index ].aifunc( self, dist );
		} else
			move->frame[ index ].aifunc( self, 0 );
	}

	if ( run_frame && move->frame[ index ].thinkfunc )
		move->frame[ index ].thinkfunc( self );

	if ( move->frame[ index ].lerp_frame != -1 ) {
		self->s.renderfx |= RF_OLD_FRAME_LERP;
		self->s.old_frame = move->frame[ index ].lerp_frame;
	}

	//index = self->s.frame - move->firstframe;
    //if (move->frame[index].aifunc) {
    //    if (!(self->monsterinfo.aiflags & AI_HOLD_FRAME))
    //        move->frame[index].aifunc(self, move->frame[index].dist * self->monsterinfo.scale);
    //    else
    //        move->frame[index].aifunc(self, 0);
    //}

    //if (move->frame[index].thinkfunc)
    //    move->frame[index].thinkfunc(self);
}


void monster_think( edict_t *self ) {
	// Ensure to remove RF_STAIR_STEP and RF_OLD_FRAME_LERP.
	self->s.renderfx &= ~( RF_STAIR_STEP | RF_OLD_FRAME_LERP );

	M_MoveFrame( self );
	if ( self->linkcount != self->monsterinfo.linkcount ) {
		self->monsterinfo.linkcount = self->linkcount;
		M_CheckGround( self, SVG_GetClipMask( self ) );
	}
	M_CatagorizePosition( self, self->s.origin, self->liquidlevel, self->liquidtype );
	M_WorldEffects( self );
	M_SetEffects( self );
}


/*
================
monster_use

Using a monster makes it angry at the current activator
================
*/
void monster_use( edict_t *self, edict_t *other, edict_t *activator ) {
	if ( self->enemy )
		return;
	if ( self->health <= 0 )
		return;
	if ( !activator )
		return;
	if ( activator->flags & FL_NOTARGET )
		return;
	if ( !( activator->client ) && !( activator->monsterinfo.aiflags & AI_GOOD_GUY ) )
		return;

// delay reaction so if the monster is teleported, its sound is still heard
	self->enemy = activator;
	FoundTarget( self );
}


void monster_start_go( edict_t *self );


void monster_triggered_spawn( edict_t *self ) {
	self->s.origin[ 2 ] += 1;
	SVG_Util_KillBox( self, false );

    self->solid = SOLID_BOUNDS_BOX;
    self->movetype = MOVETYPE_STEP;
    self->svflags &= ~SVF_NOCLIENT;
    self->air_finished_time = level.time + 12_sec;
    gi.linkentity(self);

	monster_start_go( self );

	if ( self->enemy && !( self->spawnflags & 1 ) && !( self->enemy->flags & FL_NOTARGET ) ) {
		FoundTarget( self );
	} else {
		self->enemy = NULL;
	}
}

void monster_triggered_spawn_use(edict_t *self, edict_t *other, edict_t *activator)
{
    // we have a one frame delay here so we don't telefrag the guy who activated us
    self->think = monster_triggered_spawn;
	self->nextthink = level.time + FRAME_TIME_S;
    if (activator->client)
        self->enemy = activator;
    self->use = monster_use;
}

void monster_triggered_start(edict_t *self)
{
    self->solid = SOLID_NOT;
    self->movetype = MOVETYPE_NONE;
    self->svflags |= SVF_NOCLIENT;
    self->nextthink = 0_ms;
    self->use = monster_triggered_spawn_use;
}


/*
================
monster_death_use

When a monster dies, it fires all of its targets with the current
enemy as activator.
================
*/
void monster_death_use( edict_t *self ) {
	self->flags = static_cast<entity_flags_t>( self->flags & ~( FL_FLY |  FL_SWIM ) );
	self->monsterinfo.aiflags &= AI_GOOD_GUY;

	if ( self->item ) {
		Drop_Item( self, self->item );
		self->item = NULL;
	}

	if ( self->targetNames.death )
		self->targetNames.target = self->targetNames.death;

	if ( !self->targetNames.target )
		return;

	SVG_UseTargets( self, self->enemy );
}


//============================================================================

bool monster_start( edict_t *self ) {
	if ( deathmatch->value ) {
		SVG_FreeEdict( self );
		return false;
	}

	if ( ( self->spawnflags & 4 ) && !( self->monsterinfo.aiflags & AI_GOOD_GUY ) ) {
		self->spawnflags &= ~4;
		self->spawnflags |= 1;
//      gi.dprintf("fixed spawnflags on %s at %s\n", self->classname, vtos(self->s.origin));
	}

	if ( !( self->monsterinfo.aiflags & AI_GOOD_GUY ) )
		level.total_monsters++;

	self->nextthink = level.time + FRAME_TIME_S;
    self->svflags |= SVF_MONSTER;
    self->s.renderfx |= RF_FRAMELERP;
    self->takedamage = DAMAGE_AIM;
    self->air_finished_time = level.time + 12_sec;
    self->use = monster_use;
    self->max_health = self->health;
    self->clipmask = MASK_MONSTERSOLID;

	self->s.skinnum = 0;
	self->lifeStatus = LIFESTATUS_ALIVE;
	self->svflags &= ~SVF_DEADMONSTER;

	if ( !self->monsterinfo.checkattack )
		self->monsterinfo.checkattack = M_CheckAttack;
	VectorCopy( self->s.origin, self->s.old_origin );

	if ( st.item ) {
		self->item = SVG_FindItemByClassname( st.item );
		if ( !self->item )
			gi.dprintf( "%s at %s has bad item: %s\n", self->classname, vtos( self->s.origin ), st.item );
	}

	// randomize what frame they start on
	if ( self->monsterinfo.currentmove )
		self->s.frame = self->monsterinfo.currentmove->firstframe + ( Q_rand( ) % ( self->monsterinfo.currentmove->lastframe - self->monsterinfo.currentmove->firstframe + 1 ) );

	return true;
}

void monster_start_go( edict_t *self ) {
	vec3_t  v;

	if ( self->health <= 0 )
		return;

	// check for target to combat_point and change to targetNames.combat
	if ( self->targetNames.target ) {
		bool        notcombat;
		bool        fixup;
		edict_t *target;

		target = NULL;
		notcombat = false;
		fixup = false;
		while ( ( target = SVG_Find( target, FOFS_GENTITY( targetname ), self->targetNames.target ) ) != NULL ) {
			if ( strcmp( target->classname, "point_combat" ) == 0 ) {
				self->targetNames.combat = self->targetNames.target;
				fixup = true;
			} else {
				notcombat = true;
			}
		}
		if ( notcombat && self->targetNames.combat )
			gi.dprintf( "%s at %s has target with mixed types\n", self->classname, vtos( self->s.origin ) );
		if ( fixup )
			self->targetNames.target = NULL;
	}

	// validate targetNames.combat
	if ( self->targetNames.combat ) {
		edict_t *target;

		target = NULL;
		while ( ( target = SVG_Find( target, FOFS_GENTITY( targetname ), self->targetNames.combat ) ) != NULL ) {
			if ( strcmp( target->classname, "point_combat" ) != 0 ) {
				gi.dprintf( "%s at %s has a bad targetNames.combat %s : %s at %s\n",
						   self->classname, vtos( self->s.origin ),
						   self->targetNames.combat, target->classname, vtos( target->s.origin ) );
			}
		}
	}

    if (self->targetNames.target) {
        self->goalentity = self->movetarget = SVG_PickTarget(self->targetNames.target);
        if (!self->movetarget) {
            gi.dprintf("%s can't find target %s at %s\n", self->classname, self->targetNames.target, vtos(self->s.origin));
            self->targetNames.target = NULL;
            self->monsterinfo.pause_time = HOLD_FOREVER;
            self->monsterinfo.stand(self);
        } else if (strcmp(self->movetarget->classname, "path_corner") == 0) {
            VectorSubtract(self->goalentity->s.origin, self->s.origin, v);
            self->ideal_yaw = self->s.angles[YAW] = QM_Vector3ToYaw(v);
            self->monsterinfo.walk(self);
            self->targetNames.target = NULL;
        } else {
            self->goalentity = self->movetarget = NULL;
            self->monsterinfo.pause_time = HOLD_FOREVER;
            self->monsterinfo.stand(self);
        }
    } else {
        self->monsterinfo.pause_time = HOLD_FOREVER;
        self->monsterinfo.stand(self);
    }

    self->think = monster_think;
	self->nextthink = level.time + FRAME_TIME_S;
}


void walkmonster_start_go(edict_t *self)
{
    if (!(self->spawnflags & 2) && level.time < 1_sec) {
        M_droptofloor(self);

		if ( self->groundentity )
			if ( !M_walkmove( self, 0, 0 ) )
				gi.dprintf( "%s in solid at %s\n", self->classname, vtos( self->s.origin ) );
	}

	if ( !self->yaw_speed )
		self->yaw_speed = 20;
	self->viewheight = 25;

	monster_start_go( self );

	if ( self->spawnflags & 2 )
		monster_triggered_start( self );
}

void walkmonster_start( edict_t *self ) {
	self->think = walkmonster_start_go;
	monster_start( self );
}


void flymonster_start_go( edict_t *self ) {
	if ( !M_walkmove( self, 0, 0 ) )
		gi.dprintf( "%s in solid at %s\n", self->classname, vtos( self->s.origin ) );

	if ( !self->yaw_speed )
		self->yaw_speed = 10;
	self->viewheight = 25;

	monster_start_go( self );

	if ( self->spawnflags & 2 )
		monster_triggered_start( self );
}


void flymonster_start( edict_t *self ) {
	self->flags = static_cast<entity_flags_t>( self->flags | FL_FLY );
	self->think = flymonster_start_go;
	monster_start( self );
}


void swimmonster_start_go( edict_t *self ) {
	if ( !self->yaw_speed )
		self->yaw_speed = 10;
	self->viewheight = 10;

	monster_start_go( self );

	if ( self->spawnflags & 2 )
		monster_triggered_start( self );
}

void swimmonster_start( edict_t *self ) {
	self->flags = static_cast<entity_flags_t>( self->flags | FL_SWIM );
	self->think = swimmonster_start_go;
	monster_start( self );
}
