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
#include "m_player.h"



static  edict_t *current_player;
static  gclient_t *current_client;

static  vec3_t  forward, right, up;

//float   xyspeed;
//float   bobmove;
//int64_t bobcycle, bobcycle_run;       // odd cycles are right foot going forward
//float   bobfracsin;     // sin(bobfrac*M_PI)


inline bool SkipViewModifiers( ) {
	//if ( g_skipViewModifiers->integer && sv_cheats->integer ) {
	//	return true;
	//}
	//// don't do bobbing, etc on grapple
	//if ( current_client->ctf_grapple &&
	//	 current_client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY ) {
	//	return true;
	//}
	// spectator mode
	if ( current_client->resp.spectator ) { //|| ( G_TeamplayEnabled( ) && current_client->resp.ctf_team == CTF_NOTEAM ) ) {
		return true;
	}
	return false;
}

/*
===============
SV_CalcRoll

===============
*/
float SV_CalcRoll( vec3_t angles, vec3_t velocity ) {
	float   sign;
	float   side;
	float   value;

	side = DotProduct( velocity, right );
	sign = side < 0 ? -1 : 1;
	side = fabsf( side );

	value = sv_rollangle->value;

	if ( side < sv_rollspeed->value )
		side = side * value / sv_rollspeed->value;
	else
		side = value;

	return side * sign;

}


/*
===============
P_DamageFeedback

Handles color blends and view kicks
===============
*/
void P_DamageFeedback( edict_t *player ) {
	float   side;
	int32_t   r, l;
	constexpr Vector3 armor_color = { 1.0, 1.0, 1.0 };
	constexpr Vector3 power_color = { 0.0, 1.0, 0.0 };
	constexpr Vector3 blood_color = { 1.0, 0.0, 0.0 };

	gclient_t *client = player->client;

	// flash the backgrounds behind the status numbers
	int16_t want_flashes = 0;

	if ( client->frameDamage.blood ) {
		want_flashes |= 1;
	}
	if ( client->frameDamage.armor && !( player->flags & FL_GODMODE ) ) {
		want_flashes |= 2;
	}

	if ( want_flashes ) {
		client->flash_time = level.time + 100_ms;
		client->ps.stats[ STAT_FLASHES ] = want_flashes;
	} else if ( client->flash_time < level.time ){
		client->ps.stats[ STAT_FLASHES ] = 0;
	}

	// Total points of damage shot at the player this frame.
	float count = (float)( client->frameDamage.blood + client->frameDamage.armor );
	if ( count == 0 ) {
		// Didn't take any damage.
		return;
	}

	// start a pain animation if still in the player model
	if ( client->anim_priority < ANIM_PAIN && player->s.modelindex == MODELINDEX_PLAYER ) {
		static int      i;

		client->anim_priority = ANIM_PAIN;
		if ( client->ps.pmove.pm_flags & PMF_DUCKED ) {
			player->s.frame = FRAME_crpain1 - 1;
			client->anim_end = FRAME_crpain4;
		} else {
			i = ( i + 1 ) % 3;
			switch ( i ) {
				case 0:
					player->s.frame = FRAME_pain101 - 1;
					client->anim_end = FRAME_pain104;
					break;
				case 1:
					player->s.frame = FRAME_pain201 - 1;
					client->anim_end = FRAME_pain204;
					break;
				case 2:
					player->s.frame = FRAME_pain301 - 1;
					client->anim_end = FRAME_pain304;
					break;
			}
		}

		client->anim_time = 0_ms; // WID: 40hz:
	}

	const float realcount = count;
	//if ( count < 10 )
	//	count = 10; // always make a visible effect
	// if we took health damage, do a minimum clamp
	if ( client->frameDamage.blood ) {
		// Always make a visible effect.
		if ( count < 10 ) {
			count = 10;
		}
	} else {
		// Don't go too deep.
		if ( count > 2 ) {
			count = 2;
		}
	}

	// play an apropriate pain sound
	if ( ( level.time > player->pain_debounce_time ) && !( player->flags & FL_GODMODE ) ) {
		// WID: We only got one sound for each pain level, sadly hehe.
		r = 1; // r = irandom( 0, 2 ) + 1; // r = 1 + ( Q_rand( ) & 1 );

		player->pain_debounce_time = level.time + 0.7_sec;
		if ( player->health < 25 ) {
			l = 25;
		} else if ( player->health < 50 ) {
			l = 50;
		} else if ( player->health < 75 ) {
			l = 75;
		} else {
			l = 100;
		}
		//gi.sound( player, CHAN_VOICE, gi.soundindex( va( "*pain%i_%i.wav", l, r ) ), 1, ATTN_NORM, 0 );
		gi.sound( player, CHAN_VOICE, gi.soundindex( va( "player/pain%i_0%i.wav", l, r ) ), 1, ATTN_NORM, 0 );
		// Paril: pain noises alert monsters
		P_PlayerNoise( player, player->s.origin, PNOISE_SELF );
	}

	// the total alpha of the blend is always proportional to count
	if ( client->damage_alpha < 0 ) {
		client->damage_alpha = 0;
	}
	//client->damage_alpha += count * 0.01f;
	//if ( client->damage_alpha < 0.2f )
	//	client->damage_alpha = 0.2f;
	//if ( client->damage_alpha > 0.6f )
	//	client->damage_alpha = 0.6f;    // don't go too saturated
	// [Paril-KEX] tweak the values to rely less on this
	// and more on damage indicators
	if ( client->frameDamage.blood || ( client->damage_alpha + count * 0.06f ) < 0.15f ) {
		client->damage_alpha += count * 0.06f;

		if ( client->damage_alpha < 0.06f )
			client->damage_alpha = 0.06f;
		if ( client->damage_alpha > 0.4f )
			client->damage_alpha = 0.4f; // don't go too saturated
	}
	// the color of the blend will vary based on how much was absorbed
	// by different armors
	Vector3 blendColor = {};
	if ( client->frameDamage.armor ) {
		blendColor += power_color * ( client->frameDamage.armor / realcount );
	}
	if ( client->frameDamage.blood ) {
		blendColor += blood_color * ( client->frameDamage.blood / realcount );
	}
	Vector3 normalizedBlendColor = QM_Vector3Normalize( blendColor );
	VectorCopy( normalizedBlendColor, client->damage_blend );


	//
	// calculate view angle kicks
	//
	float kick = (float)abs( client->frameDamage.knockBack );
	if ( kick && player->health > 0 ) { // kick of 0 means no view adjust at all
		kick = kick * 100 / player->health;

		if ( kick < count * 0.5f ) {
			kick = count * 0.5f;
		}
		if ( kick > 50 ) {
			kick = 50;
		}

		VectorSubtract( client->frameDamage.from, player->s.origin, blendColor );
		blendColor = QM_Vector3Normalize( blendColor );

		side = QM_Vector3DotProduct( blendColor, right );
		client->viewMove.damageRoll = kick * side * 0.3f;

		side = -QM_Vector3DotProduct( blendColor, forward );
		client->viewMove.damagePitch = kick * side * 0.3f;

		client->viewMove.damageTime = level.time + DAMAGE_TIME( );
	}

	//// [Paril-KEX] send view indicators
	if ( client->frameDamage.num_damage_indicators ) {
		gi.WriteUint8( svc_damage );
		gi.WriteUint8( client->frameDamage.num_damage_indicators );

		for ( uint8_t i = 0; i < client->frameDamage.num_damage_indicators; i++ ) {
			auto &indicator = client->frameDamage.damage_indicators[ i ];

			// WID: TODO: This... resolve this shit!
			#undef clamp
			// encode total damage into 5 bits
			uint8_t encoded = std::clamp( ( indicator.health + indicator.armor ) / 2, 1, 0x1F );

			// Encode types in the latter 3 bits.
			if ( indicator.health )
				encoded |= BIT( 5 );
			if ( indicator.armor )
				encoded |= BIT( 6 );
	//		if ( indicator.power )
	//			encoded |= 0x80;

			gi.WriteUint8( encoded );
			Vector3 damageDir = player->s.origin;
			damageDir -= indicator.from;
			const Vector3 normalizedDir = QM_Vector3Normalize( damageDir );
			gi.WriteDir8( &normalizedDir.x );
		}

		// Cast damage indicator to player entity.
		gi.unicast( player, false );
	}

	//
	// clear totals
	//
	client->frameDamage.blood = 0;
	client->frameDamage.armor = 0;
	client->frameDamage.knockBack = 0;
}




/*
===============
SV_CalcViewOffset

Auto pitching on slopes?

  fall from 128: 400 = 160000
  fall from 256: 580 = 336400
  fall from 384: 720 = 518400
  fall from 512: 800 = 640000
  fall from 640: 960 =

  damage = deltavelocity*deltavelocity  * 0.0001

===============
*/
void SV_CalcViewOffset( edict_t *ent ) {
	//float *angles;
	//float       bob;
	float       ratio;
	//float       delta;

	Vector3 viewOriginOffset = QM_Vector3Zero();
	Vector3 viewAnglesOffset = ent->client->weaponKicks.offsetAngles;//ent->client->ps.kick_angles;

	// If dead, fix the angle and don't add any kicks
	if ( ent->deadflag ) {
		// Clear out weapon kick angles.
		VectorClear( ent->client->ps.kick_angles );

		// Apply death view angles.
		ent->client->ps.viewangles[ ROLL ] = 40;
		ent->client->ps.viewangles[ PITCH ] = -15;
		ent->client->ps.viewangles[ YAW ] = ent->client->killer_yaw;
	
	/**
	*	Add view angle kicks for: Damage, Falling, Velocity and EarthQuakes.
	**/
	} else {
		// First apply weapon angle kicks.
		VectorCopy( ent->client->weaponKicks.offsetAngles, viewAnglesOffset );

		// Add angles based on the damage impackt kick.
		if ( ent->client->viewMove.damageTime > level.time ) {
			// [Paril-KEX] 100ms of slack is added to account for
			// visual difference in higher tickrates
			sg_time_t diff = ent->client->viewMove.damageTime - level.time;

			// slack time remaining
			if ( DAMAGE_TIME_SLACK( ) ) {
				if ( diff > DAMAGE_TIME( ) - DAMAGE_TIME_SLACK( ) )
					ratio = ( DAMAGE_TIME( ) - diff ).seconds( ) / DAMAGE_TIME_SLACK( ).seconds( );
				else
					ratio = diff.seconds( ) / ( DAMAGE_TIME( ) - DAMAGE_TIME_SLACK( ) ).seconds( );
			} else
				ratio = diff.seconds( ) / ( DAMAGE_TIME( ) - DAMAGE_TIME_SLACK( ) ).seconds( );

			viewAnglesOffset[ PITCH ] += ratio * ent->client->viewMove.damagePitch;
			viewAnglesOffset[ ROLL ] += ratio * ent->client->viewMove.damageRoll;
		}

		// Add pitch angles based on the fall impact kick.
		if ( ent->client->viewMove.fallTime > level.time ) {
			// [Paril-KEX] 100ms of slack is added to account for
			// visual difference in higher tickrates
			sg_time_t diff = ent->client->viewMove.fallTime - level.time;

			// slack time remaining
			if ( DAMAGE_TIME_SLACK( ) ) {
				if ( diff > FALL_TIME( ) - DAMAGE_TIME_SLACK( ) )
					ratio = ( FALL_TIME( ) - diff ).seconds( ) / DAMAGE_TIME_SLACK( ).seconds( );
				else
					ratio = diff.seconds( ) / ( FALL_TIME( ) - DAMAGE_TIME_SLACK( ) ).seconds( );
			} else
				ratio = diff.seconds( ) / ( FALL_TIME( ) - DAMAGE_TIME_SLACK( ) ).seconds( );
			viewAnglesOffset[ PITCH ] += ratio * ent->client->viewMove.fallValue;
		}

		// WID: Moved this to CLGame!
		// Add angles based on the entity's velocity.
		if ( /*!ent->client->pers.bob_skip &&*/ !SkipViewModifiers( ) ) {
			////delta = ent->velocity.dot( forward );
			//delta = DotProduct( ent->velocity, ( forward ) );
			//angles[ PITCH ] += delta * run_pitch->value;

			////delta = ent->velocity.dot( right );
			//delta = DotProduct( ent->velocity, ( right ) );
			//angles[ ROLL ] += delta * run_roll->value;

			//// Add angles based on bob fractional sin value.
			//delta = bobfracsin * bob_pitch->value * xyspeed;
			//if ( ( ent->client->ps.pmove.pm_flags & PMF_DUCKED ) && ent->groundentity )
			//	delta *= 6; // crouching
			//delta = min( delta, 1.2f );
			//angles[ PITCH ] += delta;
			//delta = bobfracsin * bob_roll->value * xyspeed;
			//if ( ( ent->client->ps.pmove.pm_flags & PMF_DUCKED ) && ent->groundentity )
			//	delta *= 6; // crouching
			//delta = min( delta, 1.2f );
			//if ( bobcycle & 1 )
			//	delta = -delta;
			//angles[ ROLL ] += delta;
		}

		// Add earthquake view offset angles
		if ( ent->client->viewMove.quakeTime > level.time ) {
			float factor = min( 1.0f, ( ent->client->viewMove.quakeTime.seconds( ) / level.time.seconds( ) ) * 0.25f );

			viewAnglesOffset.x += crandom_open( ) * factor;
			viewAnglesOffset.y += crandom_open( ) * factor;
			viewAnglesOffset.z += crandom_open( ) * factor;
		}
	}
	// [Paril-KEX] Clamp view offset angles within a pleasant realistic range.
	for ( int32_t i = 0; i < 3; i++ ) {
		viewAnglesOffset[ i ] = std::clamp( viewAnglesOffset[ i ], -31.f, 31.f );
	}

	/**
	*	View Offset.
	**/
	// Base origin for view offset.
	VectorClear( viewOriginOffset );

	// Add fall height landing impact to the final view originOffset.
	if ( ent->client->viewMove.fallTime > level.time ) {
		// [Paril-KEX] 100ms of slack is added to account for
		// visual difference in higher tickrates
		sg_time_t diff = ent->client->viewMove.fallTime - level.time;

		// Slack time remaining
		if ( DAMAGE_TIME_SLACK( ) ) {
			if ( diff > FALL_TIME( ) - DAMAGE_TIME_SLACK( ) ) {
				ratio = ( FALL_TIME( ) - diff ).seconds( ) / DAMAGE_TIME_SLACK( ).seconds( );
			} else {
				ratio = diff.seconds() / ( FALL_TIME() - DAMAGE_TIME_SLACK() ).seconds();
			}
		} else {
			ratio = diff.seconds() / ( FALL_TIME() - DAMAGE_TIME_SLACK() ).seconds();
		}
		viewOriginOffset.z -= ratio * ent->client->viewMove.fallValue * 0.4f;
	}

	// WID: Moved this to CLGame!
	// Add bob height to the viewOffset.
	//bob = bobfracsin * xyspeed * bob_up->value;
	//if ( bob > 6 )
	//	bob = 6;
	////gi.DebugGraph (bob *2, 255);
	//viewOffset[ 2 ] += bob;

	// Add weapon kicks offset into the final viewOffset.
	viewOriginOffset += ent->client->weaponKicks.offsetOrigin;

	// Clamp the viewOffset values to remain within the player bounding box.
	viewOriginOffset[ 0 ] = std::clamp( viewOriginOffset[ 0 ], -14.f, 14.f );
	viewOriginOffset[ 1 ] = std::clamp( viewOriginOffset[ 1 ], -14.f, 14.f );
	viewOriginOffset[ 2 ] = std::clamp( viewOriginOffset[ 2 ], -22.f, 30.f );

	// Store the viewOriginOffset and viewAnglesOffset(as KickAngles in the client's playerState.
	VectorCopy( viewOriginOffset, ent->client->ps.viewoffset );
	VectorCopy( viewAnglesOffset, ent->client->ps.kick_angles );
}

/*
==============
SV_CalcGunOffset
==============
*/
// WID: Moved to CLGame.
//void SV_CalcGunOffset( edict_t *ent ) {
//	int     i;
//	float   delta;
//
//	// Gun angles from bobbing
//	ent->client->ps.gunangles[ ROLL ] = xyspeed * bobfracsin * 0.005f;
//	ent->client->ps.gunangles[ YAW ] = xyspeed * bobfracsin * 0.01f;
//	if ( bobcycle & 1 ) {
//		ent->client->ps.gunangles[ ROLL ] = -ent->client->ps.gunangles[ ROLL ];
//		ent->client->ps.gunangles[ YAW ] = -ent->client->ps.gunangles[ YAW ];
//	}
//
//	ent->client->ps.gunangles[ PITCH ] = xyspeed * bobfracsin * 0.005f;
//
//	// Gun angles from delta movement
//	for ( i = 0; i < 3; i++ ) {
//		delta = ent->client->oldviewangles[ i ] - ent->client->ps.viewangles[ i ];
//		if ( delta > 180 )
//			delta -= 360;
//		if ( delta < -180 )
//			delta += 360;
//		clamp( delta, -45, 45 );
//		if ( i == YAW )
//			ent->client->ps.gunangles[ ROLL ] += 0.1f * delta;
//		ent->client->ps.gunangles[ i ] += 0.2f * delta;
//	}
//
//	// gun height
//	VectorClear( ent->client->ps.gunoffset );
////  ent->ps->gunorigin[2] += bob;
//
//	// gun_x / gun_y / gun_z are development tools
//	for ( i = 0; i < 3; i++ ) {
//		ent->client->ps.gunoffset[ i ] += forward[ i ] * ( gun_y->value );
//		ent->client->ps.gunoffset[ i ] += right[ i ] * gun_x->value;
//		ent->client->ps.gunoffset[ i ] += up[ i ] * ( -gun_z->value );
//	}
//}


/*
=============
SV_AddBlend
=============
*/
void SV_AddBlend( float r, float g, float b, float a, float *v_blend ) {
	float   a2, a3;

	if ( a <= 0 )
		return;
	a2 = v_blend[ 3 ] + ( 1 - v_blend[ 3 ] ) * a; // new total alpha
	a3 = v_blend[ 3 ] / a2;   // fraction of color from old

	v_blend[ 0 ] = v_blend[ 0 ] * a3 + r * ( 1 - a3 );
	v_blend[ 1 ] = v_blend[ 1 ] * a3 + g * ( 1 - a3 );
	v_blend[ 2 ] = v_blend[ 2 ] * a3 + b * ( 1 - a3 );
	v_blend[ 3 ] = a2;
}


/*
=============
SV_CalcBlend
=============
*/
// [Paril-KEX] convenience functions that returns true
// if the powerup should be 'active' (false to disable,
// will flash at 500ms intervals after 3 sec)
[[nodiscard]] constexpr bool G_PowerUpExpiringRelative( sg_time_t left ) {
	return left.milliseconds( ) > 3000 || ( left.milliseconds( ) % 1000 ) < 500;
}

[[nodiscard]] constexpr bool G_PowerUpExpiring( sg_time_t time ) {
	return G_PowerUpExpiringRelative( time - level.time );
}

void SV_CalcBlend( edict_t *ent ) {
	sg_time_t remaining;

	Vector4Clear( ent->client->ps.screen_blend );

	// add for contents
	vec3_t vieworg = {};
	VectorAdd( ent->s.origin, ent->client->ps.viewoffset, vieworg );
	vieworg[ 2 ] += ent->client->ps.pmove.viewheight;

	int32_t contents = gi.pointcontents( vieworg );
	if ( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) )
		ent->client->ps.rdflags |= RDF_UNDERWATER;
	else
		ent->client->ps.rdflags &= ~RDF_UNDERWATER;
	
	// Prevent it from adding screenblend if we're inside a client entity, by checking
	// if its brush has CONTENTS_PLAYER set in its clipmask.
	if ( /*!( contents & CONTENTS_PLAYER ) 
		&&*/ ( contents & ( CONTENTS_SOLID | CONTENTS_LAVA ) ) ) {
		SG_AddBlend( 1.0f, 0.3f, 0.0f, 0.6f, ent->client->ps.screen_blend );
	} else if ( contents & CONTENTS_SLIME ) {
		SG_AddBlend( 0.0f, 0.1f, 0.05f, 0.6f, ent->client->ps.screen_blend );
	} else if ( contents & CONTENTS_WATER ) {
		SG_AddBlend( 0.5f, 0.3f, 0.2f, 0.4f, ent->client->ps.screen_blend );
	}

	// add for damage
	if ( ent->client->damage_alpha > 0 ) {
		SG_AddBlend( ent->client->damage_blend[ 0 ], ent->client->damage_blend[ 1 ]
			, ent->client->damage_blend[ 2 ], ent->client->damage_alpha, ent->client->ps.screen_blend );
	}

	// [Paril-KEX] drowning visual indicator
	if ( ent->air_finished_time < level.time + 9_sec ) {
		constexpr vec3_t drown_color = { 0.1f, 0.1f, 0.2f };
		constexpr float max_drown_alpha = 0.75f;
		float alpha = ( ent->air_finished_time < level.time ) ? 1 : ( 1.f - ( ( ent->air_finished_time - level.time ).seconds() / 9.0f ) );
		//SV_AddBlend( drown_color[ 0 ], drown_color[ 1 ], drown_color[ 2 ], min( alpha, max_drown_alpha ), ent->client->ps.damage_blend );
		SG_AddBlend( drown_color[ 0 ], drown_color[ 1 ], drown_color[ 2 ], min( alpha, max_drown_alpha ), ent->client->damage_blend );
	}

#if 0
	if ( ent->client->bonus_alpha > 0 )
		SG_AddBlend( 0.85f, 0.7f, 0.3f, ent->client->bonus_alpha, ent->client->ps.screen_blend );
#endif
	// Drop the damage value
	ent->client->damage_alpha -= gi.frame_time_s * 0.6f;
	if ( ent->client->damage_alpha < 0 ) {
		ent->client->damage_alpha = 0;
	}

	// Drop the bonus value
	ent->client->bonus_alpha -= gi.frame_time_s;
	if ( ent->client->bonus_alpha < 0 ) {
		ent->client->bonus_alpha = 0;
	}
}

/*
=============
P_WorldEffects
=============
*/
static void P_WorldEffects( void ) {
	liquid_level_t liquidlevel, old_waterlevel;

	if ( current_player->movetype == MOVETYPE_NOCLIP ) {
		current_player->air_finished_time = level.time + 12_sec; // don't need air
		return;
	}

	liquidlevel = current_player->liquidlevel;
	old_waterlevel = current_client->old_waterlevel;
	current_client->old_waterlevel = liquidlevel;

	//
	// if just entered a water volume, play a sound
	//
	if ( !old_waterlevel && liquidlevel ) {
		// Feet in.
		if ( liquidlevel == liquid_level_t::LIQUID_FEET ) {
			P_PlayerNoise( current_player, current_player->s.origin, PNOISE_SELF );
			if ( current_player->liquidtype & CONTENTS_LAVA ) {
				//gi.sound( current_player, CHAN_BODY, gi.soundindex( "player/lava_in.wav" ), 1, ATTN_NORM, 0 );
				gi.sound( current_player, CHAN_BODY, gi.soundindex( "player/burn01.wav" ), 1, ATTN_NORM, 0 );

				// clear damage_debounce, so the pain sound will play immediately
				current_player->damage_debounce_time = level.time - 1_sec;
			} else if ( current_player->liquidtype & CONTENTS_SLIME ) {
				gi.sound( current_player, CHAN_BODY, gi.soundindex( "player/water_feet_in01.wav" ), 1, ATTN_NORM, 0 );
			} else if ( current_player->liquidtype & CONTENTS_WATER ) {
				gi.sound( current_player, CHAN_BODY, gi.soundindex( "player/water_feet_in01.wav" ), 1, ATTN_NORM, 0 );
			}
		} else if ( liquidlevel >= liquid_level_t::LIQUID_WAIST ) {
			P_PlayerNoise( current_player, current_player->s.origin, PNOISE_SELF );
			if ( current_player->liquidtype & CONTENTS_LAVA ) {
				gi.sound( current_player, CHAN_BODY, gi.soundindex( "player/burn02.wav" ), 1, ATTN_NORM, 0 );

				// clear damage_debounce, so the pain sound will play immediately
				current_player->damage_debounce_time = level.time - 1_sec;
			} else if ( current_player->liquidtype & CONTENTS_SLIME ) {
				const std::string splash_sfx_path = SG_RandomResourcePath( "player/water_splash_in", "wav", 0, 2 );
				gi.sound( current_player, CHAN_AUTO, gi.soundindex( splash_sfx_path.c_str() ), 1, ATTN_NORM, 0 );
			} else if ( current_player->liquidtype & CONTENTS_WATER ) {
				const std::string splash_sfx_path = SG_RandomResourcePath( "player/water_splash_in", "wav", 0, 2 );
				gi.sound( current_player, CHAN_AUTO, gi.soundindex( splash_sfx_path.c_str() ), 1, ATTN_NORM, 0 );
			}
		}
		current_player->flags = static_cast<ent_flags_t>( current_player->flags | FL_INWATER );

	}

	// If just completely exited a water volume while only feet in, play a sound.
	if ( !liquidlevel && old_waterlevel == liquid_level_t::LIQUID_FEET ) {
		P_PlayerNoise( current_player, current_player->s.origin, PNOISE_SELF );
		gi.sound( current_player, CHAN_AUTO, gi.soundindex( "player/water_feet_out01.wav" ), 1, ATTN_NORM, 0 );

	}
	// If just completely exited a water volume waist or head in, play a sound.
	if ( !liquidlevel && old_waterlevel >= liquid_level_t::LIQUID_WAIST ) {
		P_PlayerNoise( current_player, current_player->s.origin, PNOISE_SELF );
		gi.sound( current_player, CHAN_AUTO, gi.soundindex( "player/water_body_out01.wav" ), 1, ATTN_NORM, 0 );
		current_player->flags = static_cast<ent_flags_t>( current_player->flags & ~FL_INWATER );
	}

	// Check for head just going under water.
	if ( old_waterlevel < liquid_level_t::LIQUID_UNDER && liquidlevel == liquid_level_t::LIQUID_UNDER ) {
		gi.sound( current_player, CHAN_BODY, gi.soundindex( "player/water_head_under01.wav" ), 1, ATTN_NORM, 0 );
	}
	//
	// Check for head just coming out of water.
	if ( old_waterlevel == liquid_level_t::LIQUID_UNDER && liquidlevel != liquid_level_t::LIQUID_UNDER ) {
		if ( current_player->air_finished_time < level.time ) {
			// gasp for air
			gi.sound( current_player, CHAN_VOICE, gi.soundindex( "player/gasp01.wav" ), 1, ATTN_NORM, 0 );
			P_PlayerNoise( current_player, current_player->s.origin, PNOISE_SELF );
		} else  if ( current_player->air_finished_time < level.time + 11_sec ) {
			// just break surface
			gi.sound( current_player, CHAN_VOICE, gi.soundindex( "player/gasp02.wav" ), 1, ATTN_NORM, 0 );
		}
	}

	//
	// check for drowning
	//
	if ( liquidlevel == liquid_level_t::LIQUID_UNDER ) {
		// if out of air, start drowning
		if ( current_player->air_finished_time < level.time ) {
			// drown!
			if ( current_player->client->next_drown_time < level.time
				&& current_player->health > 0 ) {
				current_player->client->next_drown_time = level.time + 3_sec;

				// take more damage the longer underwater
				current_player->dmg += 15;
				if ( current_player->dmg > 30 )
					current_player->dmg = 30;

				// Play a gurp sound instead of a normal pain sound
				if ( current_player->health <= current_player->dmg ) {
					gi.sound( current_player, CHAN_VOICE, gi.soundindex( "player/drown01.wav" ), 1, ATTN_NORM, 0 );
				} else {
					const qhandle_t gurp_sfx_index = gi.soundindex( SG_RandomResourcePath( "player/gurp", "wav", 0, 2 ).c_str() );
					gi.sound( current_player, CHAN_VOICE, gurp_sfx_index, 1, ATTN_NORM, 0 );
				}

				current_player->pain_debounce_time = level.time;

				T_Damage( current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, current_player->dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER );
			}
		}
	} else {
		current_player->air_finished_time = level.time + 12_sec;
		current_player->dmg = 2;
	}

	//
	// check for sizzle damage
	//
	if ( liquidlevel && ( current_player->liquidtype & ( CONTENTS_LAVA | CONTENTS_SLIME ) ) ) {
		if ( current_player->liquidtype & CONTENTS_LAVA ) {
			if ( current_player->health > 0
				&& current_player->pain_debounce_time <= level.time ) {
				//if ( Q_rand( ) & 1 )
				//	gi.sound( current_player, CHAN_VOICE, gi.soundindex( "player/burn1.wav" ), 1, ATTN_NORM, 0 );
				//else
				//	gi.sound( current_player, CHAN_VOICE, gi.soundindex( "player/burn2.wav" ), 1, ATTN_NORM, 0 );
				const std::string burn_sfx_path = SG_RandomResourcePath( "player/burn", "wav", 0, 2 );
				gi.sound( current_player, CHAN_VOICE, gi.soundindex( burn_sfx_path.c_str() ), 1, ATTN_NORM, 0 );
				current_player->pain_debounce_time = level.time + 1_sec;
			}

			T_Damage( current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 3 * liquidlevel, 0, 0, MOD_LAVA );
		}

		if ( current_player->liquidtype & CONTENTS_SLIME ) {
			T_Damage( current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 1 * liquidlevel, 0, 0, MOD_SLIME );
		}
	}
}


/*
===============
G_SetClientEffects
===============
*/
void G_SetClientEffects( edict_t *ent ) {
	ent->s.effects = 0;
	ent->s.renderfx = 0;

	if ( ent->health <= 0 || level.intermission_framenum ) {
		return;
	}

	// show cheaters!!!
	if ( ent->flags & FL_GODMODE ) {
		ent->s.effects |= EF_COLOR_SHELL;
		ent->s.renderfx |= ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE );
	}
}


/*
===============
G_SetClientEvent
===============
*/
void G_SetClientEvent( edict_t *ent ) {
	if ( ent->s.event )
		return;

	//if ( ent->groundentity && xyspeed > 225 ) {
	//	if ( (int)( current_client->bobtime + bobmove ) != bobcycle )
	//		ent->s.event = EV_FOOTSTEP;
	//}
	Vector3 ladderDistVec = QM_Vector3Subtract( current_client->last_ladder_pos, ent->s.origin );
	float ladderDistance = QM_Vector3LengthSqr( ladderDistVec );

	if ( ent->client->ps.pmove.pm_flags & PMF_ON_LADDER ) {
		if ( !deathmatch->integer &&
			current_client->last_ladder_sound < level.time &&
			ladderDistance > 48.f ) {
			ent->s.event = EV_FOOTSTEP_LADDER;
			VectorCopy( ent->s.origin, current_client->last_ladder_pos );
			current_client->last_ladder_sound = level.time + LADDER_SOUND_TIME;
		}
	} else if ( ent->groundentity && ent->client->ps.xySpeed > 225 ) {
		if ( ( current_client->bobCycle != current_client->oldBobCycle ) ) {
			ent->s.event = EV_FOOTSTEP;
			//gi.dprintf( "%s: EV_FOOTSTEP ent->ground xyspeed > 225\n", __func__ );
		}
	}
}

/*
===============
G_SetClientSound
===============
*/
void G_SetClientSound( edict_t *ent ) {
	// WID: C++20: Added const.
	const char *weap;

	if ( ent->client->pers.game_helpchanged != game.helpchanged ) {
		ent->client->pers.game_helpchanged = game.helpchanged;
		ent->client->pers.helpchanged = 1;
	}

	// help beep (no more than ONE time - that's annoying enough)
	if ( ent->client->pers.helpchanged && ent->client->pers.helpchanged <= 1 && !( level.framenum & 63 ) ) {
		ent->client->pers.helpchanged++;
		gi.sound( ent, CHAN_VOICE, gi.soundindex( "misc/pc_up.wav" ), 1, ATTN_STATIC, 0 );
	}


	if ( ent->client->pers.weapon )
		weap = ent->client->pers.weapon->classname;
	else
		weap = "";

	if ( ent->liquidlevel && ( ent->liquidtype & ( CONTENTS_LAVA | CONTENTS_SLIME ) ) )
		ent->s.sound = snd_fry;
	else if ( strcmp( weap, "weapon_railgun" ) == 0 )
		ent->s.sound = gi.soundindex( "weapons/rg_hum.wav" );
	else if ( strcmp( weap, "weapon_bfg" ) == 0 )
		ent->s.sound = gi.soundindex( "weapons/bfg_hum.wav" );
	else if ( ent->client->weaponState.activeSound )
		ent->s.sound = ent->client->weaponState.activeSound;
	else
		ent->s.sound = 0;
}

/**
*	@brief	Will set the client entity's animation for the current frame.
**/
void G_SetClientFrame( edict_t *ent ) {
	// Return if not viewing a player model entity.
	if ( ent->s.modelindex != MODELINDEX_PLAYER ) {
		return;     // not in the player model
	}

	// Acquire its client info.
	gclient_t *client = ent->client;

	// Ducked?
	bool duck = false;
	if ( client->ps.pmove.pm_flags & PMF_DUCKED ) {
		duck = true;
	}

	// Walk/Running ?
	bool run = false;
	if ( client->ps.xySpeed ) {
		run = true;
	}

	// check for stand/duck and stop/go transitions
	if ( duck != client->anim_duck && client->anim_priority < ANIM_DEATH ) {
		goto newanim;
	}
	if ( run != client->anim_run && client->anim_priority == ANIM_BASIC ) {
		goto newanim;
	}
	if ( !ent->groundentity && client->anim_priority <= ANIM_WAVE ) {
		goto newanim;
	}

	// WID: 40hz:
	if ( client->anim_time > level.time ) {
		return;
	} else if ( client->anim_priority == ANIM_REVERSED ) {
		if ( client->anim_time <= level.time ) {
			ent->s.frame--;
			client->anim_time = level.time + 10_hz; // WID: 40hz:
		}
		return;
	} else if ( ent->s.frame < client->anim_end ) {
		// continue an animation
		if ( client->anim_time <= level.time ) {
			ent->s.frame++;
			client->anim_time = level.time + 10_hz; // WID: 40hz:
		}
		return;
	}

	// If death is prioritized, stay death anim.
	if ( client->anim_priority == ANIM_DEATH ) {
		// Stay there.
		return;
	}
	// Jumping.
	if ( client->anim_priority == ANIM_JUMP ) {
		// If we're not on-ground we're already jumping.
		if ( !ent->groundentity ) {
			return;     // stay there
		}
		// I have no clue why this is here.
		ent->client->anim_priority = ANIM_WAVE;

		// WID: 40hz:
		if ( duck ) {
			ent->s.frame = FRAME_jump6;
			ent->client->anim_end = FRAME_jump4;
			ent->client->anim_priority |= ANIM_REVERSED;
		} else {
			ent->s.frame = FRAME_jump3;
			ent->client->anim_end = FRAME_jump6;
		}
		ent->client->anim_time = level.time + 10_hz; // WID: 40hz:
		return;
	}

newanim:
	// return to either a running or standing frame
	client->anim_priority = ANIM_BASIC;
	client->anim_duck = duck;
	client->anim_run = run;
	client->anim_time = level.time + 10_hz; // WID: 40hz:

	if ( !ent->groundentity ) {
		client->anim_priority = ANIM_JUMP;
		if ( ent->s.frame != FRAME_jump2 )
			ent->s.frame = FRAME_jump1;
		client->anim_end = FRAME_jump2;
	} else if ( run ) {
		// running
		if ( duck ) {
			ent->s.frame = FRAME_crwalk1;
			client->anim_end = FRAME_crwalk6;
		} else {
			ent->s.frame = FRAME_run1;
			client->anim_end = FRAME_run6;
		}
	} else {
		// standing
		if ( duck ) {
			ent->s.frame = FRAME_crstnd01;
			client->anim_end = FRAME_crstnd19;
		} else {
			ent->s.frame = FRAME_stand01;
			client->anim_end = FRAME_stand40;
		}
	}
}


/*
=================
ClientEndServerFrame

Called for each player at the end of the server frame
and right after spawning
=================
*/
void ClientEndServerFrame( edict_t *ent ) {
	int     i;

	// no player exists yet (load game)
	if ( !ent->client->pers.spawned ) {
		gi.dprintf( "%s: !ent->client->pers.spawned\n", __func__ );
		return;
	}

	current_player = ent;
	current_client = ent->client;

	//
	// If the origin or velocity have changed since ClientThink(),
	// update the pmove values.  This will happen when the client
	// is pushed by a bmodel or kicked by an explosion.
	//
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	//
	for ( i = 0; i < 3; i++ ) {
		current_client->ps.pmove.origin[ i ] = ent->s.origin[ i ];//COORD2SHORT( ent->s.origin[ i ] ); // WID: float-movement
		current_client->ps.pmove.velocity[ i ] = ent->velocity[ i ];//COORD2SHORT( ent->velocity[ i ] ); // WID: float-movement
	}

	//
	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	//
	if ( level.intermission_framenum ) {
		// FIXME: add view drifting here?
		current_client->ps.screen_blend[ 3 ] = 0;
		current_client->ps.fov = 90;
		G_SetStats( ent );
		return;
	}

	// Calculate angle vectors.
	AngleVectors( &ent->client->viewMove.viewAngles.x, forward, right, up );

	// burn from lava, etc
	P_WorldEffects( );

	//
	// set model angles from view angles so other things in
	// the world can tell which direction you are looking
	//
	if ( ent->client->viewMove.viewAngles[ PITCH ] > 180 ) {
		ent->s.angles[ PITCH ] = ( -360 + ent->client->viewMove.viewAngles[ PITCH ] ) / 3;
	} else {
		ent->s.angles[ PITCH ] = ent->client->viewMove.viewAngles[ PITCH ] / 3;
	}
	ent->s.angles[ YAW ] = ent->client->viewMove.viewAngles[ YAW ];
	ent->s.angles[ ROLL ] = 0;
	ent->s.angles[ ROLL ] = SV_CalcRoll( ent->s.angles, ent->velocity ) * 4;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	//xyspeed = sqrtf( ent->velocity[ 0 ] * ent->velocity[ 0 ] + ent->velocity[ 1 ] * ent->velocity[ 1 ] );

	if ( ent->client->ps.xySpeed < 5 ) {
		//bobmove = 0;
		//current_client->bobtime = 0;    // start at beginning of cycle again
	} else if ( ent->groundentity ) {
		// so bobbing only cycles when on ground
		//if ( xyspeed > 210 )
		//	bobmove = 0.25f;
		//else if ( xyspeed > 100 )
		//	bobmove = 0.125f;
		//else
		//	bobmove = 0.0625f;

		//if ( xyspeed > 210 ) {
		//	bobmove = gi.frame_time_ms / 400.f;
		//} else if ( xyspeed > 100 ) {
		//	bobmove = gi.frame_time_ms / 800.f;
		//} else {
		//	bobmove = gi.frame_time_ms / 1600.f;
		//}
	} else {
		//bobmove = 0;
	}

	//float bobtime = ( current_client->bobtime += current_client->ps.bobMove );
	//const float bobtime_run = bobtime;

	if ( ( current_client->ps.pmove.pm_flags & PMF_DUCKED ) && ent->groundentity ) {
		//bobtime *= 4;
	}

	current_client->oldBobCycle = current_client->bobCycle;
	current_client->bobCycle = ( current_client->ps.bobCycle & 128 ) >> 7;
	current_client->bobFracSin = fabs( sin( ( current_client->ps.bobCycle & 127 ) / 127.0 * M_PI ) );

	//bobcycle = (int64_t)bobtime;
	//bobcycle_run = (int64_t)bobtime_run;
	//bobfracsin = fabs( sin( bobtime * M_PI ) );

	gi.dprintf( "SVGame(%s): bobCycle(%i), oldBobCycle(%i), bobFracSin(%f) \n", __func__, current_client->bobCycle, current_client->oldBobCycle, current_client->bobFracSin );
	// apply all the damage taken this frame
	P_DamageFeedback( ent );

	// determine the view offsets
	SV_CalcViewOffset( ent );

	// WID: Moved to CLGame.
	// determine the gun offsets
	//SV_CalcGunOffset( ent );

	// Determine the full screen color blend which must happen after applying viewoffset, 
	// so eye contents can be accurately determined.
	// FIXME: with client prediction, the contents should be determined by the client.
	SV_CalcBlend( ent );

	// chase cam stuff
	if ( ent->client->resp.spectator ) {
		G_SetSpectatorStats( ent );
	} else {
		G_SetStats( ent );
	}
	G_CheckChaseStats( ent );

	G_SetClientEvent( ent );

	G_SetClientEffects( ent );

	G_SetClientSound( ent );

	G_SetClientFrame( ent );

	VectorCopy( ent->velocity, ent->client->oldvelocity );
	VectorCopy( ent->client->ps.viewangles, ent->client->oldviewangles );
	ent->client->oldgroundentity = ent->groundentity;

	// Clear out the weapon kicks.
	ent->client->weaponKicks = {};
	//VectorClear( ent->client->kick_origin );
	//VectorClear( ent->client->kick_angles );

	// if the scoreboard is up, update it
	if ( ent->client->showscores && !( level.framenum & 31 ) ) {
		DeathmatchScoreboardMessage( ent, ent->enemy );
		gi.unicast( ent, false );
	}
}

