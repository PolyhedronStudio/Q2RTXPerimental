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
#include "svgame/player/svg_player_client.h"
#include "svgame/player/svg_player_hud.h"
#include "svgame/player/svg_player_trail.h"
#include "svgame/player/svg_player_view.h"

#include "sharedgame/sg_cmd_messages.h"
#include "sharedgame/sg_entity_effects.h"
#include "sharedgame/sg_means_of_death.h"

#include "sharedgame/sg_gamemode.h"
#include "svgame/svg_gamemode.h"
#include "svgame/gamemodes/svg_gm_basemode.h"

#include "svgame/entities/svg_player_edict.h"


static  svg_base_edict_t *current_player;
static  svg_client_t *current_client;

static  vec3_t  forward, right, up;

//float   xyspeed;
//float   bobmove;
//int64_t bobcycle, bobcycle_run;       // odd cycles are right foot going forward
//float   bobfracsin;     // sin(bobfrac*M_PI)

/**
*   @brief  Checks for player state generated events(usually by PMove) and processed them for execution.
**/
void SVG_CheckClientPlayerstateEvents( const svg_base_edict_t *ent, player_state_t *ops, player_state_t *ps );

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
	if ( current_client->resp.spectator ) { //|| ( SVG_TeamplayEnabled( ) && current_client->resp.ctf_team == CTF_NOTEAM ) ) {
		return true;
	}
	return false;
}

/*
===============
SV_CalcRoll

===============
*/
const double SV_CalcRoll( const Vector3 &angles, const Vector3 &velocity ) {
	float   sign;
	float   side;
	float   value;

	side = QM_Vector3DotProduct( velocity, right );
	sign = side < 0 ? -1 : 1;
	side = fabsf( side );

	value = sv_rollangle->value;

	if ( side < sv_rollspeed->value )
		side = side * value / sv_rollspeed->value;
	else
		side = value;

	return side * sign;

}


/**
*	@brief	Handles and applies the screen color blends and view kicks
**/
void P_DamageFeedback( svg_base_edict_t *player ) {
	float   side;
	int32_t   r, l;
	constexpr Vector3 armor_color = { 1.0, 1.0, 1.0 };
	constexpr Vector3 power_color = { 0.0, 1.0, 0.0 };
	constexpr Vector3 blood_color = { 1.0, 0.0, 0.0 };

	svg_client_t *client = player->client;

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
	if ( /*client->anim_priority < ANIM_PAIN &&*/ player->s.modelindex == MODELINDEX_PLAYER ) {
		// WID: TODO:
		// ?? SG_PMoveState_AddExternalEvent( ... );
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
		SVG_Player_PlayerNoise( player, player->s.origin, PNOISE_SELF );
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
P_CalculateViewOffset

Auto pitching on slopes?

  fall from 128: 400 = 160000
  fall from 256: 580 = 336400
  fall from 384: 720 = 518400
  fall from 512: 800 = 640000
  fall from 640: 960 =

  damage = deltavelocity*deltavelocity  * 0.0001

===============
*/
void P_CalculateViewOffset( svg_base_edict_t *ent ) {
	//float *angles;
	//float       bob;
	float       ratio;
	//float       delta;

	Vector3 viewOriginOffset = QM_Vector3Zero();
	Vector3 viewAnglesOffset = ent->client->weaponKicks.offsetAngles;//ent->client->ps.kick_angles;

	// If dead, fix the angle and don't add any kicks
	if ( ent->lifeStatus & entity_lifestatus_t::LIFESTATUS_DEAD ) {
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
			QMTime diff = ent->client->viewMove.damageTime - level.time;

			// slack time remaining
			if ( DAMAGE_TIME_SLACK( ) ) {
				if ( diff > DAMAGE_TIME( ) - DAMAGE_TIME_SLACK( ) )
					ratio = ( DAMAGE_TIME( ) - diff ).Seconds( ) / DAMAGE_TIME_SLACK( ).Seconds( );
				else
					ratio = diff.Seconds( ) / ( DAMAGE_TIME( ) - DAMAGE_TIME_SLACK( ) ).Seconds( );
			} else
				ratio = diff.Seconds( ) / ( DAMAGE_TIME( ) - DAMAGE_TIME_SLACK( ) ).Seconds( );

			viewAnglesOffset[ PITCH ] += ratio * ent->client->viewMove.damagePitch;
			viewAnglesOffset[ ROLL ] += ratio * ent->client->viewMove.damageRoll;
		}

		// Add pitch angles based on the fall impact kick.
		if ( ent->client->viewMove.fallTime > level.time ) {
			// [Paril-KEX] 100ms of slack is added to account for
			// visual difference in higher tickrates
			QMTime diff = ent->client->viewMove.fallTime - level.time;

			// slack time remaining
			if ( DAMAGE_TIME_SLACK( ) ) {
				if ( diff > FALL_TIME( ) - DAMAGE_TIME_SLACK( ) )
					ratio = ( FALL_TIME( ) - diff ).Seconds( ) / DAMAGE_TIME_SLACK( ).Seconds( );
				else
					ratio = diff.Seconds( ) / ( FALL_TIME( ) - DAMAGE_TIME_SLACK( ) ).Seconds( );
			} else
				ratio = diff.Seconds( ) / ( FALL_TIME( ) - DAMAGE_TIME_SLACK( ) ).Seconds( );
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
			float factor = std::min( 1.0f, (float)( ent->client->viewMove.quakeTime.Seconds( ) / level.time.Seconds( ) ) * 0.25f );

			viewAnglesOffset.x += crandom_openf( ) * factor;
			viewAnglesOffset.y += crandom_openf( ) * factor;
			viewAnglesOffset.z += crandom_openf( ) * factor;
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
		QMTime diff = ent->client->viewMove.fallTime - level.time;

		// Slack time remaining
		if ( DAMAGE_TIME_SLACK( ) ) {
			if ( diff > FALL_TIME( ) - DAMAGE_TIME_SLACK( ) ) {
				ratio = ( FALL_TIME( ) - diff ).Seconds( ) / DAMAGE_TIME_SLACK( ).Seconds( );
			} else {
				ratio = diff.Seconds() / ( FALL_TIME() - DAMAGE_TIME_SLACK() ).Seconds();
			}
		} else {
			ratio = diff.Seconds() / ( FALL_TIME() - DAMAGE_TIME_SLACK() ).Seconds();
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
SV_CalculateGunOffset
==============
*/
// WID: Moved to CLGame.
//void SV_CalculateGunOffset( svg_base_edict_t *ent ) {
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



/**
*	@brief	Determine the contents of the client's view org as well calculate possible
*			damage influenced screen blends.
**/
void P_CalculateBlend( svg_base_edict_t *ent ) {
	QMTime remaining;

	// Clear player state screen blend.
	Vector4Clear( ent->client->ps.screen_blend );

	// Add for contents specific.
	vec3_t vieworg = {};
	VectorAdd( ent->s.origin, ent->client->ps.viewoffset, vieworg );
	vieworg[ 2 ] += ent->client->ps.pmove.viewheight;

	int32_t contents = gi.pointcontents( vieworg );
	if ( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) {
		ent->client->ps.rdflags |= RDF_UNDERWATER;
	} else {
		ent->client->ps.rdflags &= ~RDF_UNDERWATER;
	}
	
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

	// Add for damage
	if ( ent->client->damage_alpha > 0 ) {
		SG_AddBlend( ent->client->damage_blend[ 0 ], ent->client->damage_blend[ 1 ]
			, ent->client->damage_blend[ 2 ], ent->client->damage_alpha, ent->client->ps.screen_blend );
	}

	// [Paril-KEX] drowning visual indicator
	if ( ent->air_finished_time < level.time + 9_sec ) {
		constexpr vec3_t drown_color = { 0.1f, 0.1f, 0.2f };
		constexpr float max_drown_alpha = 0.75f;
		float alpha = ( ent->air_finished_time < level.time ) ? 1 : ( 1.f - ( ( ent->air_finished_time - level.time ).Seconds() / 9.0f ) );
		//SV_AddBlend( drown_color[ 0 ], drown_color[ 1 ], drown_color[ 2 ], min( alpha, max_drown_alpha ), ent->client->ps.damage_blend );
		SG_AddBlend( drown_color[ 0 ], drown_color[ 1 ], drown_color[ 2 ], std::min( alpha, max_drown_alpha ), ent->client->damage_blend );
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

/**
*	@brief	Check if any world related collisions occure and if so apply necessary effects.
**/
void P_CheckWorldEffects( void ) {
	cm_liquid_level_t liquidlevel, old_waterlevel;

	if ( current_player->movetype == MOVETYPE_NOCLIP ) {
		current_player->air_finished_time = level.time + 12_sec; // don't need air
		return;
	}

	liquidlevel = current_player->liquidInfo.level;
	old_waterlevel = current_client->old_waterlevel;
	current_client->old_waterlevel = liquidlevel;

	//
	// if just entered a water volume, play a sound
	//
	if ( !old_waterlevel && liquidlevel ) {
		// Feet in.
		if ( liquidlevel == cm_liquid_level_t::LIQUID_FEET ) {
			SVG_Player_PlayerNoise( current_player, current_player->s.origin, PNOISE_SELF );
			if ( current_player->liquidInfo.type & CONTENTS_LAVA ) {
				//gi.sound( current_player, CHAN_BODY, gi.soundindex( "player/lava_in.wav" ), 1, ATTN_NORM, 0 );
				gi.sound( current_player, CHAN_BODY, gi.soundindex( "player/burn01.wav" ), 1, ATTN_NORM, 0 );

				// clear damage_debounce, so the pain sound will play immediately
				current_player->damage_debounce_time = level.time - 1_sec;
			} else if ( current_player->liquidInfo.type & CONTENTS_SLIME ) {
				gi.sound( current_player, CHAN_BODY, gi.soundindex( "player/water_feet_in01.wav" ), 1, ATTN_NORM, 0 );
			} else if ( current_player->liquidInfo.type & CONTENTS_WATER ) {
				gi.sound( current_player, CHAN_BODY, gi.soundindex( "player/water_feet_in01.wav" ), 1, ATTN_NORM, 0 );
			}
		} else if ( liquidlevel >= cm_liquid_level_t::LIQUID_WAIST ) {
			SVG_Player_PlayerNoise( current_player, current_player->s.origin, PNOISE_SELF );
			if ( current_player->liquidInfo.type & CONTENTS_LAVA ) {
				gi.sound( current_player, CHAN_BODY, gi.soundindex( "player/burn02.wav" ), 1, ATTN_NORM, 0 );

				// clear damage_debounce, so the pain sound will play immediately
				current_player->damage_debounce_time = level.time - 1_sec;
			} else if ( current_player->liquidInfo.type & CONTENTS_SLIME ) {
				const std::string splash_sfx_path = SG_RandomResourcePath( "player/water_splash_in", "wav", 0, 2 );
				gi.sound( current_player, CHAN_AUTO, gi.soundindex( splash_sfx_path.c_str() ), 1, ATTN_NORM, 0 );
			} else if ( current_player->liquidInfo.type & CONTENTS_WATER ) {
				const std::string splash_sfx_path = SG_RandomResourcePath( "player/water_splash_in", "wav", 0, 2 );
				gi.sound( current_player, CHAN_AUTO, gi.soundindex( splash_sfx_path.c_str() ), 1, ATTN_NORM, 0 );
			}
		}
		current_player->flags |= FL_INWATER;
	}

	// If just completely exited a water volume while only feet in, play a sound.
	if ( !liquidlevel && old_waterlevel == cm_liquid_level_t::LIQUID_FEET ) {
		SVG_Player_PlayerNoise( current_player, current_player->s.origin, PNOISE_SELF );
		gi.sound( current_player, CHAN_AUTO, gi.soundindex( "player/water_feet_out01.wav" ), 1, ATTN_NORM, 0 );
	}
	// If just completely exited a water volume waist or head in, play a sound.
	if ( !liquidlevel && old_waterlevel >= cm_liquid_level_t::LIQUID_WAIST ) {
		SVG_Player_PlayerNoise( current_player, current_player->s.origin, PNOISE_SELF );
		gi.sound( current_player, CHAN_AUTO, gi.soundindex( "player/water_body_out01.wav" ), 1, ATTN_NORM, 0 );
		current_player->flags &= ~FL_INWATER;
	}

	// Check for head just going under water.
	if ( old_waterlevel < cm_liquid_level_t::LIQUID_UNDER && liquidlevel == cm_liquid_level_t::LIQUID_UNDER ) {
		gi.sound( current_player, CHAN_BODY, gi.soundindex( "player/water_head_under01.wav" ), 1, ATTN_NORM, 0 );
	}
	//
	// Check for head just coming out of water.
	if ( old_waterlevel == cm_liquid_level_t::LIQUID_UNDER && liquidlevel != cm_liquid_level_t::LIQUID_UNDER ) {
		if ( current_player->air_finished_time < level.time ) {
			// gasp for air
			gi.sound( current_player, CHAN_VOICE, gi.soundindex( "player/gasp01.wav" ), 1, ATTN_NORM, 0 );
			SVG_Player_PlayerNoise( current_player, current_player->s.origin, PNOISE_SELF );
		} else  if ( current_player->air_finished_time < level.time + 11_sec ) {
			// just break surface
			gi.sound( current_player, CHAN_VOICE, gi.soundindex( "player/gasp02.wav" ), 1, ATTN_NORM, 0 );
		}
	}

	//
	// check for drowning
	//
	if ( liquidlevel == cm_liquid_level_t::LIQUID_UNDER ) {
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

				SVG_DamageEntity( current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, current_player->dmg, 0, DAMAGE_NO_ARMOR, MEANS_OF_DEATH_WATER );
			}
		}
	} else {
		current_player->air_finished_time = level.time + 12_sec;
		current_player->dmg = 2;
	}

	//
	// Check for sizzle damage
	//
	if ( liquidlevel && ( current_player->liquidInfo.type & ( CONTENTS_LAVA | CONTENTS_SLIME ) ) ) {
		if ( current_player->liquidInfo.type & CONTENTS_LAVA ) {
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

			SVG_DamageEntity( current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 3 * liquidlevel, 0, DAMAGE_NONE, MEANS_OF_DEATH_LAVA );
		}

		if ( current_player->liquidInfo.type & CONTENTS_SLIME ) {
			SVG_DamageEntity( current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 1 * liquidlevel, 0, DAMAGE_NONE, MEANS_OF_DEATH_SLIME );
		}
	}
}


/**
*	@brief	Apply possibly applied 'effects'.
**/
void SVG_SetClientEffects( svg_base_edict_t *ent ) {
	ent->s.effects = 0;
	ent->s.renderfx = 0;

	if ( ent->health <= 0 || level.intermissionFrameNumber ) {
		return;
	}

	// show cheaters!!!
	if ( ent->flags & FL_GODMODE ) {
		ent->s.effects |= EF_COLOR_SHELL;
		ent->s.renderfx |= ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE );
	}
}


/**
*	@brief	Determine the client's event.
**/
void SVG_SetClientEvent( svg_base_edict_t *ent ) {
	// We're already occupied by an event.
	if ( ent->s.event ) {
		return;
	}

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
	} else if ( ent->groundInfo.entity && ent->client->ps.xySpeed > 225 ) {
		if ( ( current_client->bobCycle != current_client->oldBobCycle ) ) {
			ent->s.event = EV_FOOTSTEP;
			//gi.dprintf( "%s: EV_FOOTSTEP ent->ground xyspeed > 225\n", __func__ );
		}
	}
}

/**
*	@brief	Determine client sound.
**/
void SVG_SetClientSound( svg_base_edict_t *ent ) {
	// Override sound with the 'fry' sound in case of being in a 'fryer' liquid, lol.
	if ( ent->liquidInfo.level && ( ent->liquidInfo.type & ( CONTENTS_LAVA | CONTENTS_SLIME ) ) ) {
		ent->s.sound = gi.soundindex( "player/burn01.wav" );;
	// Override entity sound with that of the weapon's activeSound.
	} else if ( ent->client->weaponState.activeSound ) {
		ent->s.sound = ent->client->weaponState.activeSound;
	// Default to zeroing out the entity's sound.
	} else {
		ent->s.sound = 0;
	}
}

/**
*	@brief	Will set the client entity's animation for the current frame.
**/
void SVG_P_ProcessAnimations( const svg_base_edict_t *ent );
void SVG_SetClientFrame( svg_base_edict_t *ent ) {
	// Return if not viewing a player model entity.
	if ( ent->s.modelindex != MODELINDEX_PLAYER ) {
		return;     // not in the player model
	}

	// Acquire its client info.
	svg_client_t *client = ent->client;
	if ( !client ) {
		return; // Need a client entity.
	}


	// Finally process the actual player_state animations.
	SVG_P_ProcessAnimations( ent );

	// Get the model resource.
	const model_t *model = gi.GetModelDataForName( ent->model );

	// Get animation mixer.
	sg_skm_animation_mixer_t *animationMixer = &client->animationMixer;
	// Get lower body state.
	sg_skm_animation_state_t *lowerBodyState = &animationMixer->currentBodyStates[ SKM_BODY_LOWER ];
	// Get upper body state.
	sg_skm_animation_state_t *upperBodyState = &animationMixer->currentBodyStates[ SKM_BODY_UPPER ];

	// Get lower event body state.
	sg_skm_animation_state_t *lowerEventState = &animationMixer->eventBodyState[ SKM_BODY_LOWER ];
	// Get upper event body state.
	sg_skm_animation_state_t *upperEventState = &animationMixer->eventBodyState[ SKM_BODY_UPPER ];

	// Default to body states.
	uint8_t lowerBodyAnimationID = lowerBodyState->animationID;
	uint8_t upperBodyAnimationID = upperBodyState->animationID;

	// These are 0 in case of being inactive!
	uint8_t lowerEventAnimationID = 0;
	uint8_t upperEventAnimationID = 0;

	// Only override the actual animationIDs if the event is still actively playing.
	if ( lowerEventState->timeEnd >= level.time ) {
		lowerEventAnimationID = lowerEventState->animationID;
		//gi.bprintf( PRINT_DEVELOPER, "%s: lowerEventState->animationID(%i), lowerEventState->time(%" PRIu64 "), lowerEventState->timEnd(%" PRIu64 ")\n", __func__, lowerEventAnimationID, ( level.time - lowerEventState->timeStart ).Milliseconds(), lowerEventState->timeEnd.Milliseconds() );
	} else {
		//gi.bprintf( PRINT_DEVELOPER, "%s: lowerEventState->animationID(0) lowerEventState->timeStart(%" PRIu64 "), level.time(%" PRIu64 ")\n", __func__, lowerEventState->timeStart.Milliseconds(), level.time.Milliseconds() );
	}
	if ( upperEventState->timeEnd >= level.time ) {
		upperEventAnimationID = upperEventState->animationID;
		//gi.bprintf( PRINT_DEVELOPER, "%s: upperEventState->animationID(%i), upperEventState->time(%" PRIu64 "), upperEventState->timEnd(%" PRIu64 ")\n", __func__, upperEventAnimationID, ( level.time - upperEventState->timeStart ).Milliseconds(), upperEventState->timeEnd.Milliseconds() );
	} else {
		//gi.bprintf( PRINT_DEVELOPER, "%s: lowerEventState->animationID(0) upperEventState->timeStart(%" PRIu64 "), level.time(%" PRIu64 ")\n", __func__, upperEventState->timeStart.Milliseconds(), level.time.Milliseconds() );
	}

	// Encode the body specifics into the frame value.
	ent->s.frame = SG_EncodeAnimationState( 
		( lowerEventAnimationID ? lowerEventAnimationID : lowerBodyAnimationID ),
		( upperEventAnimationID ? upperEventAnimationID : upperBodyAnimationID ),
		0, 
		BASE_FRAMERATE /*lowerBodyState->frameTime*/
	);
}


/**
*   @brief  This will be called once for all clients on each server frame, before running any other entities in the world.
**/
void SVG_Client_BeginServerFrame( svg_base_edict_t *ent ) {
	// Ensure we are dealing with a player entity here.
	if ( !ent->GetTypeInfo()->IsSubClassType<svg_player_edict_t>() ) {
		gi.dprintf( "%s: Not a player entity.\n", __func__ );
		return;
	}

	// <Q2RTXP>: WID: TODO: WARN?
	if ( !ent->client ) {
		return;
	}

	/**
	*   Remove RF_STAIR_STEP if we're in a new frame, not stepping.
	**/
	if ( gi.GetServerFrameNumber() != ent->client->last_stair_step_frame ) {
		ent->s.renderfx &= ~RF_STAIR_STEP;
	}

	// Give game mode control.
	game.mode->BeginServerFrame( static_cast<svg_player_edict_t *>( ent ) );

	/**
	*   UNLATCH ALL LATCHED BUTTONS:
	**/
	ent->client->latched_buttons = BUTTON_NONE;
}

/**
*	@brief	Called for each player at the end of the server frame, and right after spawning.
**/
void SVG_Client_EndServerFrame( svg_base_edict_t *ent ) {
	// Ensure we are dealing with a player entity here.
	if ( !ent->GetTypeInfo()->IsSubClassType<svg_player_edict_t>() ) {
		gi.dprintf( "%s: Not a player entity.\n", __func__ );
		return;
	}
	// <Q2RTXP>: WID: TODO: WARN?
	if ( !ent->client ) {
		return;
	}

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
	current_client->ps.pmove.origin = ent->s.origin;
	current_client->ps.pmove.velocity = ent->velocity;

	// Let game modes handle it from here.
	game.mode->EndServerFrame( static_cast<svg_player_edict_t*>( ent ) );
}

