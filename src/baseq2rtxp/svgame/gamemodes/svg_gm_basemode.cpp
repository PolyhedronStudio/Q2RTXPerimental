/********************************************************************
*
*
*	SharedGame: GameMode Related Stuff.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/player/svg_player_client.h"
#include "svgame/player/svg_player_hud.h"

#include "svgame/svg_lua_api.h"

#include "sharedgame/sg_gamemode.h"
#include "svgame/svg_gamemode.h"
#include "svgame/gamemodes/svg_gm_basemode.h"

#include "svgame/entities/target/svg_target_changelevel.h"
#include "svgame/entities/svg_player_edict.h"



/**
*
*
*
*	General GameMode Functionality:
*
*
*
**/
/**
*	@brief	Called when an entity has been killed.
**/
void svg_gamemode_t::EntityKilled( svg_base_edict_t *targ, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) {
    if ( targ->health < -999 )
        targ->health = -999;

    targ->enemy = attacker;

    if ( targ->movetype == MOVETYPE_PUSH || targ->movetype == MOVETYPE_STOP || targ->movetype == MOVETYPE_NONE ) {
        // doors, triggers, etc
        if ( targ->HasDieCallback() ) {
            targ->DispatchDieCallback( inflictor, attacker, damage, point );
        }
        return;
    }

    if ( ( targ->svFlags & SVF_MONSTER ) && ( targ->lifeStatus != LIFESTATUS_DEAD ) ) {
        targ->SetTouchCallback( nullptr );//targ->touch = NULL;
        // WID: TODO: Actually trigger death.
        //monster_death_use(targ);
        //When a monster dies, it fires all of its targets with the current
        //enemy as activator.
        //void monster_death_use( svg_base_edict_t * self ) {
        //    self->flags &= ~( FL_FLY | FL_SWIM );
        //    self->monsterinfo.aiflags &= ( AI_HIGH_TICK_RATE | AI_GOOD_GUY );

        //    if ( self->item ) {
        //        Drop_Item( self, self->item );
        //        self->item = NULL;
        //    }

        //    if ( self->targetNames.death )
        //        self->targetNames.target = self->targetNames.death;

        //    if ( !self->targetNames.target )
        //        return;

        //    SVG_UseTargets( self, self->enemy );
        //}
    }
    if ( targ->HasDieCallback() ) {
        targ->DispatchDieCallback( inflictor, attacker, damage, point );
    }
}

/**
*   @brief  Returns true if the inflictor can directly damage the target.  Used for
*           explosions and melee attacks.
**/
const bool svg_gamemode_t::CanDamageEntityDirectly( svg_base_edict_t *targ, svg_base_edict_t *inflictor ) {
    vec3_t  dest;
    svg_trace_t trace;

    // bmodels need special checking because their origin is 0,0,0
    if ( targ->movetype == MOVETYPE_PUSH ) {
        VectorAdd( targ->absMin, targ->absMax, dest );
        VectorScale( dest, 0.5f, dest );
        trace = SVG_Trace( inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, CM_CONTENTMASK_SOLID );
        if ( trace.fraction == 1.0f )
            return true;
        if ( trace.ent == targ )
            return true;
        return false;
    }

    trace = SVG_Trace( inflictor->s.origin, vec3_origin, vec3_origin, targ->s.origin, inflictor, CM_CONTENTMASK_SOLID );
    if ( trace.fraction == 1.0f )
        return true;

    VectorCopy( targ->s.origin, dest );
    dest[ 0 ] += 15.0f;
    dest[ 1 ] += 15.0f;
    trace = SVG_Trace( inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, CM_CONTENTMASK_SOLID );
    if ( trace.fraction == 1.0f )
        return true;

    VectorCopy( targ->s.origin, dest );
    dest[ 0 ] += 15.0f;
    dest[ 1 ] -= 15.0f;
    trace = SVG_Trace( inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, CM_CONTENTMASK_SOLID );
    if ( trace.fraction == 1.0f )
        return true;

    VectorCopy( targ->s.origin, dest );
    dest[ 0 ] -= 15.0f;
    dest[ 1 ] += 15.0f;
    trace = SVG_Trace( inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, CM_CONTENTMASK_SOLID );
    if ( trace.fraction == 1.0f )
        return true;

    VectorCopy( targ->s.origin, dest );
    dest[ 0 ] -= 15.0f;
    dest[ 1 ] -= 15.0f;
    trace = SVG_Trace( inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, CM_CONTENTMASK_SOLID );
    if ( trace.fraction == 1.0f )
        return true;


    return false;
}
/**
*	@brief	Sets the spawn origin and angles to that matching the found spawn point.
**/
svg_base_edict_t *svg_gamemode_t::SelectSpawnPoint( svg_player_edict_t *ent, Vector3 &origin, Vector3 &angles ) {
    svg_base_edict_t *spot = nullptr;

    // Find a single player start spot since the game modes found none.
    if ( !spot ) {
        // Iterate for info_player_start that matches the game.spawnpoint targetname to spawn at..
        while ( ( spot = SVG_Entities_Find( spot, q_offsetof( svg_base_edict_t, classname ), "info_player_start" ) ) != NULL ) {
            // Break out if the string data is invalid.
            if ( game.spawnpoint[ 0 ] == '\0' && !( const char * )spot->targetname )
                break;
            if ( game.spawnpoint[ 0 ] == '\0' || !( const char * )spot->targetname )
                continue;

            // Break out in case we found the game.spawnpoint matching info_player_start 'targetname'.
            if ( Q_stricmp( game.spawnpoint, (const char *)spot->targetname ) == 0 )
                break;
        }

        // Couldn't find any designated spawn points, pick one with just a matching classname instead.
        if ( !spot ) {
            if ( game.spawnpoint[ 0 ] == '\0' ) {
                // there wasn't a spawnpoint without a target, so use any
                spot = SVG_Entities_Find( spot, q_offsetof( svg_base_edict_t, classname ), "info_player_start" );
            }
            if ( !spot )
                gi.error( "Couldn't find spawn point %s", game.spawnpoint );
        }
    }

	// If we found a spot, set the origin and angles.
	if ( spot ) {
		origin = spot->s.origin;
		angles = spot->s.angles;
	}

	return spot;
}
/**
*	@brief	Defines the behavior for the game mode when the level has to exit.
**/
void svg_gamemode_t::ExitLevel() {
	// Notify Lua.
	SVG_Lua_CallBack_ExitMap();

	// Generate the command string to change the map.
	std::string newMapCommand = "gamemap \"" + (std::string)(level.intermissionState.changemap) + "\"";
	//Q_snprintf( command, sizeof( command ), "gamemap \"%s\"\n", (const char *)level.changemap );
	// Add it to the command string queue.
	gi.AddCommandString( newMapCommand.c_str() );

	// Unset the next level changemap.
	level.intermissionState.changemap = nullptr;
	// Unset the exiting intermission state.
	level.intermissionState.shouldExit = 0;
	// Unset the engaged intermission frame number state.
	level.intermissionState.engagedFrameNumber = 0;

	// Clear some things before going to next level.
	for ( int32_t i = 0; i < maxclients->value; i++ ) {
		// Get client entity and ensure it is valid.
		svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i + 1 );
		if ( !ent || !ent->inUse ) {
			continue;
		}
		/**
		*	Ensure the client entity health and armor never exceeds the max. This can happen 
		*	if the player has a powerup that increases their health, but we want to reset it 
		*	back to normal for the next level.
		*
		*	Reset health and armor to max if they are above max, but don't change them if they 
		*	are below max. This allows the player to keep any health or armor they have, but 
		*	also ensures that they don't start the next level with more than the maximum allowed.
		**/
		ent->health	= std::min( ent->health, ent->client->pers.max_health );
		ent->armor	= std::min( ent->armor, ent->client->pers.max_armor );
	}
}

/**
*   @brief  Returns the created target changelevel
**/
svg_base_edict_t *svg_gamemode_t::CreateTargetChangeLevel( const char *map ) {
	svg_base_edict_t *ent = g_edict_pool.AllocateNextFreeEdict<svg_target_changelevel_t>( "target_changelevel" );
	//ent->classname = svg_level_qstring_t::from_char_str( "target_changelevel" );
	if ( strcmp( map, level.nextmap ) != 0 ) {
		Q_strlcpy( level.nextmap, map, sizeof( level.nextmap ) );
	}
	ent->map = svg_level_qstring_t::from_char_str( level.nextmap );
	return ent;
}