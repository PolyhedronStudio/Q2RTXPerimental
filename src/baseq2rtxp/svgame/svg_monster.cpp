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
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "sharedgame/sg_entity_flags.h"
#include "sharedgame/sg_means_of_death.h"
#include "sharedgame/sg_tempentity_events.h"

/**
*   @brief  Marks the edict as free
**/
DECLARE_GLOBAL_CALLBACK_THINK( M_droptofloor );

#if 0
/**
*	@brief	
**/
void monster_fire_bullet( svg_base_edict_t *self, vec3_t start, vec3_t dir, const float damage, const float kick, const float hspread, const float vspread, int flashtype ) {
	fire_bullet( self, start, dir, damage, kick, hspread, vspread, MEANS_OF_DEATH_UNKNOWN );

	gi.WriteUint8( svc_muzzleflash2 );
	gi.WriteInt16( self - g_edicts );
	gi.WriteUint8( flashtype );
	gi.multicast( start, MULTICAST_PHS, false );
}
/**
*	@brief
**/
void monster_fire_shotgun( svg_base_edict_t *self, vec3_t start, vec3_t aimdir, const float damage, const float kick, const float hspread, const float vspread, int count, int flashtype ) {
	fire_shotgun( self, start, aimdir, damage, kick, hspread, vspread, count, MEANS_OF_DEATH_UNKNOWN );

	gi.WriteUint8( svc_muzzleflash2 );
	gi.WriteInt16( self - g_edicts );
	gi.WriteUint8( flashtype );
	gi.multicast( start, MULTICAST_PHS, false );
}
#endif

/**
*	@brief
**/
void M_CheckGround( svg_base_edict_t *ent, const cm_contents_t mask ) {
	vec3_t      point;
	svg_trace_t     trace;

	// Swimming and flying monsters don't check for ground.
	if ( ent->flags & ( FL_SWIM | FL_FLY ) ) {
		return;
	}

	// Too high of a velocity, we're moving upwards rapidly.
	if ( ent->velocity[ 2 ] > 100 ) {
		ent->groundInfo.entityNumber = ENTITYNUM_NONE;
		return;
	}

	// If the hull point one-quarter unit down is solid the entity is on ground.
	point[ 0 ] = ent->currentOrigin[ 0 ];
	point[ 1 ] = ent->currentOrigin[ 1 ];
	point[ 2 ] = ent->currentOrigin[ 2 ] - 0.25f;

	trace = SVG_Trace( ent->currentOrigin, ent->mins, ent->maxs, point, ent, mask );

	// check steepness
	if ( trace.plane.normal[ 2 ] < 0.7f && !trace.startsolid ) {
		ent->groundInfo.entityNumber = ENTITYNUM_NONE;
		return;
	}

//  ent->groundentity = trace.ent;
//  ent->groundentity_linkcount = trace.ent->linkCount;
//  if (!trace.startsolid && !trace.allsolid)
//      VectorCopy (trace.endpos, ent->s.origin);
	if ( !trace.startsolid && !trace.allsolid ) {
		//VectorCopy( trace.endpos, ent->s.origin );
		SVG_Util_SetEntityOrigin( ent, trace.endpos );
		ent->groundInfo.entityNumber = trace.ent->s.number;
		ent->groundInfo.entityLinkCount = trace.ent->linkCount;
		ent->velocity[ 2 ] = 0;
	}
}

/**
*	@brief
**/
void M_CatagorizePosition( svg_base_edict_t *ent, const Vector3 &in_point, cm_liquid_level_t &liquidlevel, cm_contents_t &liquidtype ) {
	//
	// get liquidlevel
	//
	Vector3 point = in_point + Vector3{ 0.f, 0.f, ent->mins[ 2 ] + 1 };
	cm_contents_t cont = gi.pointcontents( &point );

	if ( !( cont & CM_CONTENTMASK_LIQUID ) ) {
		liquidlevel = cm_liquid_level_t::LIQUID_NONE;
		liquidtype = CONTENTS_NONE;
		return;
	}

	liquidtype = cont;
	liquidlevel = cm_liquid_level_t::LIQUID_FEET;
	point.z += 26;
	cont = gi.pointcontents( &point );
	if ( !( cont & CM_CONTENTMASK_LIQUID ) ) {
		return;
	}

	liquidlevel = cm_liquid_level_t::LIQUID_WAIST;
	point[ 2 ] += 22;
	cont = gi.pointcontents( &point );
	if ( cont & CM_CONTENTMASK_LIQUID ) {
		liquidlevel = cm_liquid_level_t::LIQUID_UNDER;
	}
}

/**
*	@brief	Apply world effects to monster entity.
**/
void M_WorldEffects( svg_base_edict_t *ent ) {
	int     dmg;

    if (ent->health > 0) {
        if (!(ent->flags & FL_SWIM)) {
            if (ent->liquidInfo.level < 3) {
                ent->air_finished_time = level.time + 12_sec;
            } else if (ent->air_finished_time < level.time) {
                // drown!
                if (ent->pain_debounce_time < level.time ) {
					dmg = 2 + (int)( 2 * floorf( ( level.time - ent->air_finished_time ).Seconds( ) ) );
                    if (dmg > 15)
                        dmg = 15;
                    SVG_DamageEntity(ent, world, world, vec3_origin, ent->s.origin, vec3_origin, dmg, 0, DAMAGE_NO_ARMOR, MEANS_OF_DEATH_WATER );
                    ent->pain_debounce_time = level.time + 1_sec;
                }
            }
        } else {
            if (ent->liquidInfo.level > 0) {
                ent->air_finished_time = level.time + 9_sec;
            } else if (ent->air_finished_time < level.time ) {
                // suffocate!
                if (ent->pain_debounce_time < level.time ) {
					dmg = 2 + (int)( 2 * floorf( ( level.time - ent->air_finished_time ).Seconds( ) ) );
                    if (dmg > 15)
                        dmg = 15;
                    SVG_DamageEntity(ent, world, world, vec3_origin, ent->s.origin, vec3_origin, dmg, 0, DAMAGE_NO_ARMOR, MEANS_OF_DEATH_WATER );
                    ent->pain_debounce_time = level.time + 1_sec;
                }
            }
        }
    }

	if ( ent->liquidInfo.level == 0 ) {
		if ( ent->flags & FL_INWATER ) {
			gi.sound( ent, CHAN_BODY, gi.soundindex( "player/water_feet_out01.wav" ), 1, ATTN_NORM, 0 );
			ent->flags = static_cast<entity_flags_t>( ent->flags & ~FL_INWATER );
		}
		return;
	}

    if ((ent->liquidInfo.type & CONTENTS_LAVA) && !(ent->flags & FL_IMMUNE_LAVA)) {
        if (ent->damage_debounce_time < level.time ) {
            ent->damage_debounce_time = level.time + 0.2_sec;
            SVG_DamageEntity(ent, world, world, vec3_origin, ent->s.origin, vec3_origin, 10 * ent->liquidInfo.level, 0, DAMAGE_NONE, MEANS_OF_DEATH_LAVA );
        }
    }
    if ((ent->liquidInfo.type & CONTENTS_SLIME) && !(ent->flags & FL_IMMUNE_SLIME)) {
        if (ent->damage_debounce_time < level.time ) {
            ent->damage_debounce_time = level.time + 1_sec;
            SVG_DamageEntity(ent, world, world, vec3_origin, ent->s.origin, vec3_origin, 4 * ent->liquidInfo.level, 0, DAMAGE_NONE, MEANS_OF_DEATH_SLIME );
        }
    }

	if ( !( ent->flags & FL_INWATER ) ) {
		if ( !( ent->svFlags & SVF_DEADENTITY ) ) {
			if ( ent->liquidInfo.type & CONTENTS_LAVA ) {
				//if ( random() <= 0.5f )
				//	gi.sound( ent, CHAN_BODY, gi.soundindex( "player/lava1.wav" ), 1, ATTN_NORM, 0 );
				//else
				//	gi.sound( ent, CHAN_BODY, gi.soundindex( "player/lava2.wav" ), 1, ATTN_NORM, 0 );
				const std::string burn_sfx_path = SG_RandomResourcePath( "player/burn", "wav", 0, 2 );
				gi.sound( ent, CHAN_BODY, gi.soundindex( burn_sfx_path.c_str() ), 1, ATTN_NORM, 0 );
			} else if ( ent->liquidInfo.type & CONTENTS_SLIME ) {
				gi.sound( ent, CHAN_BODY, gi.soundindex( "player/water_feet_in01.wav" ), 1, ATTN_NORM, 0 );
			} else if ( ent->liquidInfo.type & CONTENTS_WATER ) {
				gi.sound( ent, CHAN_BODY, gi.soundindex( "player/water_feet_in01.wav" ), 1, ATTN_NORM, 0 );
			}
		}

		ent->flags = static_cast<entity_flags_t>( ent->flags | FL_INWATER );

        ent->damage_debounce_time = 0_ms;
    }
}

/**
*	@brief	
**/
DEFINE_GLOBAL_CALLBACK_THINK( M_droptofloor )( svg_base_edict_t *ent ) -> void {
	vec3_t      end;
	svg_trace_t     trace;

	cm_contents_t mask = SVG_GetClipMask( ent );

	ent->s.origin[ 2 ] += 1;
	VectorCopy( ent->s.origin, end );
	end[ 2 ] -= 256;

	trace = SVG_Trace( ent->s.origin, ent->mins, ent->maxs, end, ent, mask );

	if ( trace.fraction == 1 || trace.allsolid || ( trace.startsolid ) ) {
		return;
	}

	VectorCopy( trace.endpos, ent->s.origin );

	gi.linkentity( ent );
	M_CheckGround( ent, mask );
	M_CatagorizePosition( ent, ent->s.origin, ent->liquidInfo.level, ent->liquidInfo.type );
}

void M_SetEffects( svg_base_edict_t *ent ) {
	ent->s.entityFlags &= ~( EF_COLOR_SHELL );
	ent->s.renderfx &= ~( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE );

	// WID: TODO: Monster Reimplement.
	//if ( ent->monsterinfo.aiflags & AI_RESURRECTING ) {
	//	ent->s.entityFlags |= EF_COLOR_SHELL;
	//	ent->s.renderfx |= RF_SHELL_RED;
	//}

	if ( ent->health <= 0 )
		return;
}
