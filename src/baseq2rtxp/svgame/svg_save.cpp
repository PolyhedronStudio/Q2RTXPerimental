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
// Game related types.
#include "svgame/svg_local.h"
#include "svgame/svg_clients.h"
#include "svgame/svg_edict_pool.h"
#include "sharedgame/sg_usetarget_hints.h"
// Save related types.
#include "svg_save.h"

// Function Pointer Needs:
#include "svgame/player/svg_player_client.h"

#if USE_ZLIB
#include <zlib.h>
#else
#define gzopen(name, mode)          fopen(name, mode)
#define gzclose(file)               fclose(file)
#define gzwrite(file, buf, len)     fwrite(buf, 1, len, file)
#define gzread(file, buf, len)      fread(buf, 1, len, file)
#define gzbuffer(file, size)        (void)0
#define gzFile                      FILE *
#endif

typedef struct {
    fieldtype_t type;
#if USE_DEBUG
	// WID: C++20: Added const.
    const char *name;
#endif
    unsigned ofs;
    unsigned size;
} save_field_t;

#if USE_DEBUG
#define _FA(type, name, size) { type, #name, _OFS(name), size }
#define _DA(type, name, size) { type, #name, _OFS(name), size }
#else
#define _FA(type, name, size) { type, _OFS(name), size }
#define _DA(type, name, size) { type, _OFS(name), size }
#endif
#define _F(type, name) _FA(type, name, 1)
#define ZSTR(name, size) _FA(F_ZSTRING, name, size)
#define BYTE_ARRAY(name, size) _FA(F_BYTE, name, size)
#define BYTE(name) BYTE_ARRAY(name, 1)
#define SHORT_ARRAY(name, size) _FA(F_SHORT, name, size)
#define SHORT(name) SHORT_ARRAY(name, 1)
#define INT32_ARRAY(name, size) _FA(F_INT, name, size)
#define INT32(name) INT32_ARRAY(name, 1)
#define BOOL_ARRAY(name, size) _FA(F_BOOL, name, size)
#define BOOL(name) BOOL_ARRAY(name, 1)
#define FLOAT_ARRAY(name, size) _FA(F_FLOAT, name, size)
#define FLOAT(name) FLOAT_ARRAY(name, 1)
#define DOUBLE_ARRAY(name, size) _DA(F_DOUBLE, name, size)
#define DOUBLE(name) DOUBLE_ARRAY(name, 1)

#define LQSTR(name) _F(F_LEVEL_QSTRING, name)
#define GQSTR(name) _F(F_GAME_QSTRING, name)
#define CHARPTR(name) _F(F_LSTRING, name)

#define LEVEL_QTAG_MEMORY(name) _F(F_LEVEL_QTAG_MEMORY, name)
#define GAME_QTAG_MEMORY(name) _F(F_GAME_QTAG_MEMORY, name)

#define VEC3(name) _F(F_VECTOR3, name)
#define VEC4(name) _F(F_VECTOR4, name)
#define ITEM(name) _F(F_ITEM, name)

#define ENTITY(name) _F(F_EDICT, name)
#define POINTER(name, type) _FA(F_POINTER, name, type)
#define FTIME(name) _F(F_FRAMETIME, name)
#define I64_ARRAY(name, size) _FA(F_INT64, name, size)
#define INT64(name) I64_ARRAY(name, 1)



/***
*
*
*
*
*   Save Field Arrays:
*
*
*
*
***/
/**
*   svg_client_t:
**/
static const save_field_t clientfields[] = {
#define _OFS FOFS_GCLIENT
    INT32( ps.pmove.pm_type ),
    SHORT( ps.pmove.pm_flags ),
    SHORT( ps.pmove.pm_time ),
    SHORT( ps.pmove.gravity ),
    VEC3( ps.pmove.origin ),
    VEC3( ps.pmove.delta_angles ),
    VEC3( ps.pmove.velocity ),
    BYTE( ps.pmove.viewheight ),

    VEC3( ps.viewangles ),
    VEC3( ps.viewoffset ),
    VEC3( ps.kick_angles ),

    //VEC3( ps.gunangles ),
    //VEC3( ps.gunoffset ),
    INT32( ps.gun.modelIndex ),
    INT32( ps.gun.animationID ),

    //FLOAT_ARRAY( ps.damage_blend, 4 ),
    FLOAT_ARRAY( ps.screen_blend, 4 ),
    FLOAT( ps.fov ),
    INT32( ps.rdflags ),
    INT32( ps.bobCycle ),
    INT32_ARRAY( ps.stats, MAX_STATS ),

    ZSTR( pers.userinfo, MAX_INFO_STRING ),
    ZSTR( pers.netname, 16 ),
    INT32( pers.hand ),

    BOOL( pers.connected ),
    BOOL( pers.spawned ),

    INT32( pers.health ),
    INT32( pers.max_health ),
    INT32( pers.savedFlags ),

    INT32( pers.selected_item ),
    INT32_ARRAY( pers.inventory, MAX_ITEMS ),

    ITEM( pers.weapon ),
    ITEM( pers.lastweapon ),
    INT32_ARRAY( pers.weapon_clip_ammo, MAX_ITEMS ),

    INT32( pers.ammoCapacities.pistol ),
    INT32( pers.ammoCapacities.rifle ),
    INT32( pers.ammoCapacities.smg ),
    INT32( pers.ammoCapacities.sniper ),
    INT32( pers.ammoCapacities.shotgun ),

    INT32( pers.score ),

    BOOL( pers.spectator ),

    BOOL( showscores ),
    BOOL( showinventory ),
    BOOL( showhelp ),
    BOOL( showhelpicon ),

    //INT32( buttons ),
    //INT32( oldbuttons ),
    //INT32( latched_buttons ),

    INT32( ammo_index ),

    ITEM( newweapon ),

    BOOL( weapon_thunk ),

    BOOL( grenade_blew_up ),
    INT64( grenade_time ),
    INT64( grenade_finished_time ),

    INT32( frameDamage.armor ),
    INT32( frameDamage.blood ),
    INT32( frameDamage.knockBack ),
    VEC3( frameDamage.from ),

    FLOAT( killer_yaw ),

    INT32( weaponState.mode ),
    INT32( weaponState.canChangeMode ),
    INT32( weaponState.aimState.isAiming ),
    INT32( weaponState.animation.currentFrame ),
    INT32( weaponState.animation.startFrame ),
    INT32( weaponState.animation.endFrame ),
    INT64( weaponState.timers.lastEmptyWeaponClick ),
    INT64( weaponState.timers.lastPrimaryFire ),
    INT64( weaponState.timers.lastAimedFire ),
    INT64( weaponState.timers.lastDrawn ),
    INT64( weaponState.timers.lastHolster ),

    VEC3( weaponKicks.offsetAngles ),
    VEC3( weaponKicks.offsetOrigin ),

    VEC3( viewMove.viewAngles ), VEC3( viewMove.viewForward ),
    INT64( viewMove.damageTime ),
    INT64( viewMove.fallTime ),
    INT64( viewMove.quakeTime ),
    FLOAT( viewMove.damageRoll ), FLOAT( viewMove.damagePitch ),
    FLOAT( viewMove.fallValue ),

    FLOAT( damage_alpha ),
    FLOAT( bonus_alpha ),
    VEC3( damage_blend ),
    INT64( bobCycle ),
    INT64( oldBobCycle ),
    DOUBLE( bobFracSin ),
    INT64( last_stair_step_frame ),
    VEC3( last_ladder_pos ),
    INT64( last_ladder_sound ),

    VEC3( oldviewangles ),
    VEC3( oldvelocity ),
    ENTITY( oldgroundentity ),
    INT32( old_waterlevel ),
    INT64( next_drown_time ),

    // WID: TODO: Animation Mixer States.

    INT64( pickup_msg_time ), // WID: 64-bit-frame

    // WID: C++20: Replaced {0}
    {}
#undef _OFS
};

/**
*   svg_edict_t:
**/
static const save_field_t entityfields[] = {
#define _OFS FOFS_GENTITY
    /**
    *   Server Edict Entity State Data:
    **/
    INT32( s.number ),
    //SHORT( s.client ),
    INT32( s.entityType ),

    VEC3( s.origin ),
    VEC3( s.angles ),
    VEC3( s.old_origin ),

    INT32( s.solid ),
    INT32( s.clipmask ),
    INT32( s.clipmask ),
    INT32( s.hullContents ),
    INT32( s.ownerNumber ),

    INT32( s.modelindex ),
    INT32( s.modelindex2 ),
    INT32( s.modelindex3 ),
    INT32( s.modelindex4 ),

    INT32( s.skinnum ),
    INT32( s.effects ),
    INT32( s.renderfx ),

    INT32( s.frame ),
    INT32( s.old_frame ),

    INT32( s.sound ),
    INT32( s.event ),

    // TODO: Do we really need to save this? Perhaps.
    // For spotlights.
    VEC3( s.spotlight.rgb ),
    FLOAT( s.spotlight.intensity ),
    FLOAT( s.spotlight.angle_width ),
    FLOAT( s.spotlight.angle_falloff ),

    /**
    *   Server Edict Data:
    **/
    INT32( svflags ),
    VEC3( mins ),
    VEC3( maxs ),
    VEC3( absmin ),
    VEC3( absmax ),
    VEC3( size ),
    INT32( solid ),
    INT32( clipmask ),
    INT32( hullContents ),
    ENTITY( owner ),

    /**
    *   Start of Game Edict data:
    **/
    INT32( spawn_count ),
    INT64( freetime ),
    INT64( timestamp ),

    LQSTR( classname ),
    CHARPTR( model ),
    FLOAT( angle ),

    INT32( spawnflags ),
    INT32( flags ),

    /**
    *   Health/Body Status Conditions:
    **/
    INT32( health ),
    INT32( max_health ),
    INT32( gib_health ),
    INT32( lifeStatus ),
    INT32( takedamage ),

    /**
    *   UseTarget Properties and State:
    **/
    INT32( useTarget.flags ),
    INT32( useTarget.state ),

    /**
    *   Target Name Fields:
    **/
    LQSTR( targetname ),
    LQSTR( targetNames.target ),
    LQSTR( targetNames.kill ),
    LQSTR( targetNames.team ),
    LQSTR( targetNames.path ),
    LQSTR( targetNames.death ),
    LQSTR( targetNames.movewith ),

    /**
    *   Target Entities:
    **/
    ENTITY( targetEntities.target ),
    ENTITY( targetEntities.movewith ),

    /**
    *   Lua Properties:
    **/
    LQSTR( luaProperties.luaName ),

    /**
    *   "Delay" entities:
    **/
    ENTITY( delayed.useTarget.creatorEntity ),
    INT32( delayed.useTarget.useType ),
    INT32( delayed.useTarget.useValue ),
    ENTITY( delayed.signalOut.creatorEntity ),
    ZSTR( delayed.signalOut.name, 256 ),
    // WID: TODO: We can't save these with a system like these, can we?
    // WID: We can I guess, but it requires a specified save type for signal argument array indices.
    //SIGNALARGUMENTS( delayed.signalOut.arguments ),

    /**
    *   Physics Related:
    **/
    VEC3( moveWith.absoluteOrigin ),
    VEC3( moveWith.originOffset ),
    VEC3( moveWith.relativeDeltaOffset ),
    VEC3( moveWith.spawnDeltaAngles ),
    VEC3( moveWith.spawnParentAttachAngles ),
    VEC3( moveWith.totalVelocity ),
    ENTITY( moveWith.parentMoveEntity ),
    ENTITY( moveWith.moveNextEntity),

    INT32( movetype ),
    VEC3( velocity ),
    VEC3( avelocity ),
    INT32( viewheight ),

    // WID: Are these actually needed? Would they not be recalculated the first frame around?
    // WID: TODO: mm_ground_info_t
    // WID: TODO: mm_liquid_info_t
    INT32( mass ),
    FLOAT( gravity ),

    /**
    *   Pushers(MOVETYPE_PUSH/MOVETYPE_STOP) Physics:
    **/
    // Start/End Data:
    VEC3( pushMoveInfo.startOrigin ),
    VEC3( pushMoveInfo.startAngles ),
    VEC3( pushMoveInfo.endOrigin ),
    VEC3( pushMoveInfo.endAngles ),
    INT32( pushMoveInfo.startFrame ),
    INT32( pushMoveInfo.endFrame ),
    // Dynamic State Data
    INT32( pushMoveInfo.state ),
    VEC3( pushMoveInfo.dir ),
    VEC3( pushMoveInfo.dest ),
    BOOL( pushMoveInfo.in_motion ),
    FLOAT( pushMoveInfo.current_speed ),
    FLOAT( pushMoveInfo.move_speed ),
    FLOAT( pushMoveInfo.next_speed ),
    FLOAT( pushMoveInfo.remaining_distance ),
    FLOAT( pushMoveInfo.decel_distance ),
    //  Acceleration Data.
    FLOAT( pushMoveInfo.accel ),
    FLOAT( pushMoveInfo.speed ),
    FLOAT( pushMoveInfo.decel ),
    FLOAT( pushMoveInfo.distance ),
    FLOAT( pushMoveInfo.wait ),
    // Curve.
    VEC3( pushMoveInfo.curve.referenceOrigin ),
    //INT64( pushMoveInfo.curve.countPositions ),
    // WID: TODO: This is problematic with this save system, size has to be dynamic in the future.
    //FLOAT_ARRAY( pushMoveInfo.curve.positions, 1024 ),
    LEVEL_QTAG_MEMORY( pushMoveInfo.curve.positions ),
    INT64( pushMoveInfo.curve.frame ),
    INT64( pushMoveInfo.curve.subFrame ),
    INT64( pushMoveInfo.curve.numberSubFrames ),
    INT64( pushMoveInfo.curve.numberFramesDone ),
    // LockState
    BOOL( pushMoveInfo.lockState.isLocked ),
    INT32( pushMoveInfo.lockState.lockedSound ),
    INT32( pushMoveInfo.lockState.lockingSound ),
    INT32( pushMoveInfo.lockState.unlockingSound ),
    // Sounds
    INT32( pushMoveInfo.sounds.start ),
    INT32( pushMoveInfo.sounds.middle ),
    INT32( pushMoveInfo.sounds.end ),
    // Callback
    POINTER( pushMoveInfo.endMoveCallback, P_pusher_moveinfo_endmovecallback ),
    // Movewith
    VEC3( pushMoveInfo.lastVelocity ),
    // WID: Are these actually needed? Would they not be recalculated the first frame around?
    // WID: TODO: PushmoveInfo
    FLOAT( speed ),
    FLOAT( accel ),
    FLOAT( decel ),

    VEC3( movedir ),
    VEC3( pos1 ),
    VEC3( angles1 ),
    VEC3( pos2 ),
    VEC3( angles2 ),
    VEC3( lastOrigin ),
    VEC3( lastAngles ),
    ENTITY( movetarget ),

    /**
    *   NextThink AND Entity Callbacks:
    **/
    INT64( nextthink ),

    POINTER( postspawn, P_postspawn ),
    POINTER( prethink, P_prethink ),
    POINTER( think, P_think ),
    POINTER( postthink, P_postthink ),
    POINTER( blocked, P_blocked ),
    POINTER( touch, P_touch ),
    POINTER( use, P_use ),
    POINTER( pain, P_pain ),
    POINTER( onsignalin, P_onsignalin ),
    POINTER( die, P_die ),

    /**
    *   Entity Pointers:
    **/
    ENTITY( enemy ),
    ENTITY( oldenemy ),
    ENTITY( goalentity ),
    ENTITY( chain ),
    ENTITY( teamchain ),
    ENTITY( teammaster ),
    ENTITY( activator ),
    ENTITY( other ),

    /**
    *   Light Data:
    **/
    INT32( style ),
    CHARPTR( customLightStyle ),

    /**
    *   Item Data:
    **/
    ITEM( item ),

    /**
    *   Monster Data:
    **/
    FLOAT( yaw_speed ),
    FLOAT( ideal_yaw ),

    /**
    *   Player Noise/Trail:
    **/
    ENTITY( mynoise ),
    ENTITY( mynoise2 ),

    INT32( noise_index ),
    INT32( noise_index2 ),

    /**
    *   Sound Data:
    **/
    FLOAT( volume ),
    FLOAT( attenuation ),
    INT64( last_sound_time ),

    /**
    *   Trigger(s) Data:
    **/
    CHARPTR( message ),
    FLOAT( wait ),
    FLOAT( delay ),

    // WID: TODO: Fix this, wtf at the name of plain 'random'.
    #undef random
    FLOAT( random ),

    /**
    *   Timers Data:
    **/
    INT64( air_finished_time ),
    INT64( damage_debounce_time ),
    INT64( fly_sound_debounce_time ),
    INT64( last_move_time ),
    INT64( touch_debounce_time ),
    INT64( pain_debounce_time ),
    INT64( show_hostile_time ),
    INT64( trail_time ),

    /**
    *   Various Data:
    **/
    INT32( meansOfDeath ),
    CHARPTR( map ),

    INT32( dmg ),
    INT32( radius_dmg ),
    FLOAT( dmg_radius ),
    FLOAT( light ),
    INT32( sounds ),
    INT32( count ),

    /**
    *   Only used for g_turret.cpp - WID: Remove?:
    **/
    VEC3( move_origin ),
    VEC3( move_angles ),

    // WID: TODO: Monster Reimplement.
    //POINTER( monsterinfo.currentmove, P_monsterinfo_currentmove ),
    //POINTER( monsterinfo.nextmove, P_monsterinfo_nextmove ),
    //INT32( monsterinfo.aiflags ),
    //INT64( monsterinfo.nextframe ), // WID: 64-bit-frame
    //FLOAT( monsterinfo.scale ),

    //POINTER( monsterinfo.stand, P_monsterinfo_stand ),
    //POINTER( monsterinfo.idle, P_monsterinfo_idle ),
    //POINTER( monsterinfo.search, P_monsterinfo_search ),
    //POINTER( monsterinfo.walk, P_monsterinfo_walk ),
    //POINTER( monsterinfo.run, P_monsterinfo_run ),
    //POINTER( monsterinfo.dodge, P_monsterinfo_dodge ),
    //POINTER( monsterinfo.attack, P_monsterinfo_attack ),
    //POINTER( monsterinfo.melee, P_monsterinfo_melee ),
    //POINTER( monsterinfo.sight, P_monsterinfo_sight ),
    //POINTER( monsterinfo.checkattack, P_monsterinfo_checkattack ),

    //INT64( monsterinfo.next_move_time ),

    //INT64( monsterinfo.pause_time ),// WID: 64-bit-frame FRAMETIMEmonsterinfo.pause_time),
    //INT64( monsterinfo.attack_finished ),// WID: 64-bit-frame FRAMETIMEmonsterinfo.attack_finished),
    //INT64( monsterinfo.fire_wait ),

    //VEC3( monsterinfo.saved_goal ),
    //INT64( monsterinfo.search_time ),// WID: 64-bit-frame FRAMETIMEmonsterinfo.search_time),
    //INT64( monsterinfo.trail_time ),// WID: 64-bit-frame FRAMETIMEmonsterinfo.trail_time),
    //VEC3( monsterinfo.last_sighting ),
    //INT32( monsterinfo.attack_state ),
    //INT32( monsterinfo.lefty ),
    //INT64( monsterinfo.idle_time ),// WID: 64-bit-frame FRAMETIMEmonsterinfo.idle_time),
    //INT32( monsterinfo.linkcount ),

	// WID: C++20: Replaced {0}
    {}
#undef _OFS
};

/**
*   game:
**/
static const save_field_t gamefields[] = {
#define _OFS FOFS_GAME_LOCALS
    INT32(maxclients),
    INT32(maxentities),
    INT32(gamemode),

    INT32(serverflags),

    INT32(num_items),

    BOOL(autosaved),

    INT32( num_movewithEntityStates ),

	// WID: C++20: Replaced {0}
    {}
#undef _OFS
};

/**
*   level:
**/
static const save_field_t levelfields[] = {
#define _OFS FOFS_LEVEL_LOCALS
    INT64( frameNumber ),
    INT64( time ), // WID: 64-bit-frame

    ZSTR( level_name, MAX_QPATH ),
    ZSTR( mapname, MAX_QPATH ),
    ZSTR( nextmap, MAX_QPATH ),

    INT64( intermissionFrameNumber ),

    CHARPTR( changemap ),
    INT64( exitintermission ),
    VEC3( intermission_origin ),
    VEC3( intermission_angle ),

    ENTITY( sight_client ),

    ENTITY( sight_entity ),
    INT64( sight_entity_framenum ), // WID: 64-bit-frame
    ENTITY( sound_entity ),
    INT64( sound_entity_framenum ), // WID: 64-bit-frame
    ENTITY( sound2_entity ),
    INT64( sound2_entity_framenum ),// WID: 64-bit-frame

    INT32( pic_health ),

    INT32( total_secrets ),
    INT32( found_secrets ),

    INT32( total_goals ),
    INT32( found_goals ),

    ENTITY( current_entity ),

    INT32( body_que ),

    // WID: C++20: Replaced {0}
    {}
#undef _OFS
};



/***
*
*
*
*
*   Write:
*
*
*
*
***/
static void write_data(void *buf, size_t len, gzFile f)
{
    if (gzwrite(f, buf, len) != len) {
        gzclose(f);
        gi.error("%s: couldn't write %zu bytes", __func__, len);
    }
}

static void write_short(gzFile f, int16_t v)
{
    v = LittleShort(v);
    write_data(&v, sizeof(v), f);
}

static void write_int(gzFile f, int32_t v)
{
    v = LittleLong(v);
    write_data(&v, sizeof(v), f);
}

static void write_int64(gzFile f, int64_t v) {
	v = LittleLongLong( v );
	write_data( &v, sizeof( v ), f );
}

static void write_float(gzFile f, float v)
{
    v = LittleFloat(v);
    write_data(&v, sizeof(v), f);
}

static void write_double( gzFile f, double v ) {
    v = LittleDouble( v );
    write_data( &v, sizeof( v ), f );
}

static void write_string(gzFile f, char *s)
{
    size_t len;

    if (!s) {
        write_int(f, -1);
        return;
    }

    len = strlen(s);
    if (len >= 65536) {
        gzclose(f);
        gi.error("%s: bad length", __func__);
    }
    write_int(f, len);
    write_data(s, len, f);
}

static void write_level_qstring( gzFile f, svg_level_qstring_t *qstr ) {
    if ( !qstr || !qstr->ptr || qstr->count <= 0 ) {
        write_int( f, -1 );
        return;
    }

    const size_t len = qstr->count;
    if ( len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }
    
    write_int( f, len );
    write_data( qstr->ptr, len * sizeof( char ), f);
    return;
}

static void write_game_qstring( gzFile f, svg_game_qstring_t *qstr ) {
    if ( !qstr || !qstr->ptr || qstr->count <= 0 ) {
        write_int( f, -1 );
        return;
    }

    const size_t len = qstr->count;
    if ( len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }

    write_int( f, len );
    write_data( qstr->ptr, len * sizeof( char ), f );
    return;
}

/**
*   @brief  Write level qtag memory block to disk.
**/
template<typename T, int32_t tag = TAG_SVGAME_LEVEL>
static void write_level_qtag_memory( gzFile f, sg_qtag_memory_t<T, tag> *qtagMemory ) {
    if ( !qtagMemory || !qtagMemory->ptr || qtagMemory->count <= 0 ) {
        write_int( f, -1 );
        return;
    }

    const size_t len = qtagMemory->count;
    if ( len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }
    write_int( f, len );
    write_data( qtagMemory->ptr, len * sizeof( T ), f );
    return;
}
/**
*   @brief  Write game qtag memory block to disk.
**/
template<typename T, int32_t tag = TAG_SVGAME>
static void write_game_qtag_memory( gzFile f, sg_qtag_memory_t<T, tag> *qtagMemory ) {
    if ( !qtagMemory || !qtagMemory->ptr || qtagMemory->count <= 0 ) {
        write_int( f, -1 );
        return;
    }

    const size_t len = qtagMemory->count;
    if ( len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }
    write_int( f, len );
    write_data( qtagMemory->ptr, len * sizeof( T ), f );
    return;
}

static void write_vector3(gzFile f, vec_t *v)
{
    write_float(f, v[0]);
    write_float(f, v[1]);
    write_float(f, v[2]);
}

static void write_vector4( gzFile f, vec_t *v ) {
    write_float( f, v[ 0 ] );
    write_float( f, v[ 1 ] );
    write_float( f, v[ 2 ] );
    write_float( f, v[ 3 ] );
}

static void write_index(gzFile f, void *p, size_t size, void *start, int max_index)
{
    uintptr_t diff;

    if (!p) {
        write_int(f, -1);
        return;
    }

    diff = (uintptr_t)p - (uintptr_t)start;
    if (diff > max_index * size) {
        gzclose(f);
        gi.error("%s: pointer out of range: %p", __func__, p);
    }
    if (diff % size) {
        gzclose(f);
        gi.error("%s: misaligned pointer: %p", __func__, p);
    }
    write_int(f, (int)(diff / size));
}

static void write_pointer(gzFile f, void *p, ptr_type_t type, const save_field_t *saveField ) {
    const save_ptr_t *ptr;
    int i;

    if (!p) {
        write_int(f, -1);
        return;
    }

    for (i = 0, ptr = save_ptrs; i < num_save_ptrs; i++, ptr++) {
        if (ptr->type == type && ptr->ptr == p) {
            write_int(f, i);
            return;
        }
    }

    gzclose(f);
    #if USE_DEBUG
    gi.error( "%s: unknown pointer for '%s': %p", __func__, saveField->name, p );
    #else
    gi.error("%s: unknown pointer: %p", __func__, p);
    #endif
}

static void write_field(gzFile f, const save_field_t *field, void *base)
{
    void *p = (byte *)base + field->ofs;
    int i;

    switch (field->type) {
    case F_BYTE:
        write_data(p, field->size, f);
        break;
    case F_SHORT:
        for (i = 0; i < field->size; i++) {
            write_short(f, ((short *)p)[i]);
        }
        break;
    case F_INT:
        for (i = 0; i < field->size; i++) {
            write_int(f, ((int *)p)[i]);
        }
        break;
    case F_BOOL:
        for (i = 0; i < field->size; i++) {
            write_int(f, ((bool *)p)[i]);
        }
        break;
    case F_FLOAT:
        for (i = 0; i < field->size; i++) {
            write_float(f, ((float *)p)[i]);
        }
        break;
    case F_DOUBLE:
        for ( i = 0; i < field->size; i++ ) {
            write_double( f, ( (double *)p )[ i ] );
        }
        break;
    case F_VECTOR3:
        write_vector3(f, (vec_t *)p);
        break;
    case F_VECTOR4:
        write_vector4( f, (vec_t *)p );
        break;
    case F_ZSTRING:
        write_string(f, (char *)p);
        break;
    case F_LSTRING:
        write_string(f, *(char **)p);
        break;
    case F_LEVEL_QSTRING:
        write_level_qstring( f, (svg_level_qstring_t*)p );
        break;
    case F_GAME_QSTRING:
        write_game_qstring( f, (svg_game_qstring_t *)p );
        break;
    case F_LEVEL_QTAG_MEMORY:
        write_level_qtag_memory<float>( f, ( ( sg_qtag_memory_t<float, TAG_SVGAME_LEVEL> * )p ) );
        break;
    case F_GAME_QTAG_MEMORY:
        write_game_qtag_memory<float>( f, ( ( sg_qtag_memory_t<float, TAG_SVGAME> * )p ) );
        break;
    case F_EDICT:
        write_index(f, *(void **)p, sizeof(svg_edict_t), g_edicts, MAX_EDICTS - 1);
        break;
    case F_CLIENT:
        write_index(f, *(void **)p, sizeof(svg_client_t), game.clients, game.maxclients - 1);
        break;
    case F_ITEM:
        write_index(f, *(void **)p, sizeof(gitem_t), itemlist, game.num_items - 1);
        break;

    case F_POINTER:
		// WID: C++20: Added cast.
        write_pointer(f, *(void **)p, (ptr_type_t)field->size, field );
        break;

    case F_FRAMETIME:
		// WID: We got QMTime, so we need int64 saving for frametime.
        for (i = 0; i < field->size; i++) {
            write_int64(f, ((int64_t *)p)[i]);
        }
        break;
	case F_INT64:
		for ( i = 0; i < field->size; i++ ) {
			write_int64( f, ( (int64_t *)p )[ i ] );
		}
		break;

    default:
        #if !defined(_USE_DEBUG)
        gi.error( "%s: unknown field type(%d)", __func__, field->type );
        #else
        gi.error( "%s: unknown field type(%d), name(%s)", __func__, field->type, field->name );
        #endif
    }
}

static void write_fields(gzFile f, const save_field_t *fields, void *base)
{
    const save_field_t *field;

    for (field = fields; field->type; field++) {
        write_field(f, field, base);
    }
}



/***
* 
* 
* 
* 
*   Reading:
* 
* 
* 
* 
***/
typedef struct game_read_context_s {
	gzFile f;
    //bool frametime_is_float;
    const save_ptr_t* save_ptrs;
    int num_save_ptrs;
} game_read_context_t;

static void read_data(void *buf, size_t len, gzFile f)
{
    if (gzread(f, buf, len) != len) {
        gzclose(f);
        gi.error("%s: couldn't read %zu bytes", __func__, len);
    }
}

static int read_short(gzFile f)
{
    int16_t v;

    read_data(&v, sizeof(v), f);
    v = LittleShort(v);

    return v;
}

static int read_int(gzFile f)
{
    int32_t v;

    read_data(&v, sizeof(v), f);
    v = LittleLong(v);

    return v;
}

static float read_int64(gzFile f) {
	int64_t v;

	read_data( &v, sizeof( v ), f );
	v = LittleLongLong( v );

	return v;
}

static float read_float(gzFile f)
{
    float v;

    read_data(&v, sizeof(v), f);
    v = LittleFloat(v);

    return v;
}

static double read_double( gzFile f ) {
    double v;

    read_data( &v, sizeof( v ), f );
    v = LittleDouble( v );

    return v;
}

static char *read_string(gzFile f)
{
    int len;
    char *s;

    len = read_int(f);
    if (len == -1) {
        return NULL;
    }

    if (len < 0 || len >= 65536) {
        gzclose(f);
        gi.error("%s: bad length", __func__);
    }

	// WID: C++20: Added cast.
    s = (char*)gi.TagMalloc(len + 1, TAG_SVGAME_LEVEL);
    read_data(s, len, f);
    s[len] = 0;

    return s;
}

static const svg_level_qstring_t read_level_qstring( gzFile f ) {
    int len;

    len = read_int( f );
    if ( len == -1 ) {
        return nullptr;
    }

    if ( len < 0 || len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }

    // Allocate level tag string space.
    svg_level_qstring_t lstring = svg_level_qstring_t::new_for_size( len );
    // Delete temporary buffer.
    read_data( lstring.ptr, len, f );

    return lstring;
}

static const svg_game_qstring_t read_game_qstring( gzFile f ) {
    int len;

    len = read_int( f );
    if ( len == -1 ) {
        return nullptr;
    }

    if ( len < 0 || len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }

    // Allocate level tag string space.
    svg_game_qstring_t gstring = svg_game_qstring_t::new_for_size( len );
    // Delete temporary buffer.
    read_data( gstring.ptr, len, f );
    return gstring;
}

/**
*   @brief 
**/
template<typename T>
static sg_qtag_memory_t<T, TAG_SVGAME_LEVEL> *read_level_qtag_memory( gzFile f, sg_qtag_memory_t<T, TAG_SVGAME_LEVEL> *p ) {
    int len;

    len = read_int( f );
    if ( len == -1 ) {
        return allocate_qtag_memory<T, TAG_SVGAME_LEVEL>( p, 0 );
    }

    if ( len < 0 || len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }

    // Allocate level tag string space.
    allocate_qtag_memory<T, TAG_SVGAME_LEVEL>( p, len );
    // Delete temporary buffer.
    read_data( p->ptr, /*len*/p->size(), f );
    return p;
}
/**
*   @brief
**/
template<typename T>
static sg_qtag_memory_t<T, TAG_SVGAME> *read_game_qtag_memory( gzFile f, sg_qtag_memory_t<T, TAG_SVGAME> *p ) {
    int len;

    len = read_int( f );
    if ( len == -1 ) {
        return allocate_qtag_memory<T, TAG_SVGAME>( p, 0 );
    }

    if ( len < 0 || len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }

    // Allocate level tag string space.
    allocate_qtag_memory<T, TAG_SVGAME>( p, len );
    // Delete temporary buffer.
    read_data( p->ptr, /*len*/p->size(), f);
    return p;
}

static void read_zstring(gzFile f, char *s, size_t size)
{
    int len;

    len = read_int(f);
    if (len < 0 || len >= size) {
        gzclose(f);
        gi.error("%s: bad length", __func__);
    }

    read_data(s, len, f);
    s[len] = 0;
}

static void read_vector3(gzFile f, vec_t *v)
{
    v[0] = static_cast<vec_t>( read_float(f) );
    v[1] = static_cast<vec_t>( read_float( f ) );
    v[2] = static_cast<vec_t>( read_float( f ) );
}
static void read_vector4( gzFile f, vec_t *v ) {
    v[ 0 ] = static_cast<vec_t>( read_float( f ) );
    v[ 1 ] = static_cast<vec_t>( read_float( f ) );
    v[ 2 ] = static_cast<vec_t>( read_float( f ) );
    v[ 3 ] = static_cast<vec_t>( read_float( f ) );
}

static void *read_index(gzFile f, size_t size, void *start, int max_index)
{
    int index;
    byte *p;

    index = read_int(f);
    if (index == -1) {
        return NULL;
    }

    if (index < 0 || index > max_index) {
        gzclose(f);
        gi.error("%s: bad index", __func__);
    }

    p = (byte *)start + index * size;
    return p;
}

static void *read_pointer(game_read_context_t* ctx, ptr_type_t type)
{
    int index;
    const save_ptr_t *ptr;

    index = read_int(ctx->f);
    if (index == -1) {
        return NULL;
    }

    if (index < 0 || index >= ctx->num_save_ptrs) {
        gzclose(ctx->f);
        gi.error("%s: bad index", __func__);
    }

    ptr = &ctx->save_ptrs[index];
    if (ptr->type != type) {
        gzclose(ctx->f);
        gi.error("%s: type mismatch", __func__);
    }

    return ptr->ptr;
}

static void read_field(game_read_context_t* ctx, const save_field_t *field, void *base)
{
    void *p = (byte *)base + field->ofs;
    int i;

    switch (field->type) {
    case F_BYTE:
        read_data(p, field->size, ctx->f);
        break;
    case F_SHORT:
        for (i = 0; i < field->size; i++) {
            ((short *)p)[i] = read_short(ctx->f);
        }
        break;
    case F_INT:
        for (i = 0; i < field->size; i++) {
            ((int *)p)[i] = read_int(ctx->f);
        }
        break;
    case F_BOOL:
        for (i = 0; i < field->size; i++) {
            ((bool *)p)[i] = read_int(ctx->f);
        }
        break;
    case F_FLOAT:
        for (i = 0; i < field->size; i++) {
            ((float *)p)[i] = read_float(ctx->f);
        }
        break;
    case F_DOUBLE:
        for ( i = 0; i < field->size; i++ ) {
            ( (double *)p )[ i ] = read_double( ctx->f );
        }
        break;
    case F_VECTOR3:
        read_vector3(ctx->f, (vec_t *)p);
        break;
    case F_VECTOR4:
        read_vector4( ctx->f, (vec_t *)p );
        break;
    case F_LSTRING:
        *(char **)p = read_string(ctx->f);
        break;
    case F_LEVEL_QSTRING:
        ( *(svg_level_qstring_t *)p ) = read_level_qstring( ctx->f );
        break;
    case F_GAME_QSTRING:
        ( *(svg_game_qstring_t *)p ) = read_game_qstring( ctx->f );
        break;
    case F_LEVEL_QTAG_MEMORY:
        read_level_qtag_memory<float>( ctx->f, ( ( sg_qtag_memory_t<float, TAG_SVGAME_LEVEL> * )p ) );
        break;
    case F_GAME_QTAG_MEMORY:
        read_game_qtag_memory<float>( ctx->f, ( ( sg_qtag_memory_t<float, TAG_SVGAME> * )p ) );
        break;
    case F_ZSTRING:
        read_zstring(ctx->f, (char *)p, field->size);
        break;
    case F_EDICT:
		// WID: C++20: Added cast.
		*(svg_edict_t **)p = (svg_edict_t*)read_index(ctx->f, sizeof(svg_edict_t), g_edicts, game.maxentities - 1);
        break;
    case F_CLIENT:
		// WID: C++20: Added cast.
		*(svg_client_t **)p = (svg_client_t*)read_index(ctx->f, sizeof(svg_client_t), game.clients, game.maxclients - 1);
        break;
    case F_ITEM:
		// WID: C++20: Added cast.
        *(gitem_t **)p = (gitem_t*)read_index(ctx->f, sizeof(gitem_t), itemlist, game.num_items - 1);
        break;

    case F_POINTER:
		// WID: C++20: Added cast.
        *(void **)p = read_pointer(ctx, (ptr_type_t)field->size);
        break;

    case F_FRAMETIME:
		// WID: We got QMTime, so we need int64 saving for frametime.
		for (i = 0; i < field->size; i++) {
	        ((int64_t *)p)[i] = read_int64(ctx->f);
        }
        break;
	case F_INT64:
		for ( i = 0; i < field->size; i++ ) {
			( (int64_t *)p )[ i ] = read_int64( ctx->f );
		}
		break;

    default:
        #if !defined( _USE_DEBUG )
        gi.error("%s: unknown field type(%d)", __func__, field->type );
        #else
        gi.error( "%s: unknown field type(%d), name(%s)", __func__, field->type, field->name );
        #endif
    }
}

static void read_fields(game_read_context_t* ctx, const save_field_t *fields, void *base)
{
    const save_field_t *field;

    for (field = fields; field->type; field++) {
        read_field(ctx, field, base);
    }
}



/***
*
*
*
*
*   Game Read/Write:
*
*
*
*
***/
#define SAVE_MAGIC1     MakeLittleLong('G','S','V','1')
#define SAVE_MAGIC2     MakeLittleLong('L','S','V','1')
// WID: We got our own version number obviously.
//#define SAVE_VERSION    8
#define SAVE_VERSION	1337

static void check_gzip(int magic)
{
#if !USE_ZLIB
    if ((magic & 0xe0ffffff) == 0x00088b1f)
        gi.error("Savegame is compressed, but no gzip support linked in");
#endif
}

/*
============
WriteGame

This will be called whenever the game goes to a new level,
and when the user explicitly saves the game.

Game information include cross level data, like multi level
triggers, help computer info, and all client states.

A single player death will automatically restore from the
last save position.
============
*/
void SVG_WriteGame(const char *filename, qboolean autosave)
{
    gzFile  f;
    int     i;

    if (!autosave)
        SVG_Player_SaveClientData();

    f = gzopen(filename, "wb");
    if (!f)
        gi.error("Couldn't open %s", filename);

    write_int(f, SAVE_MAGIC1);
    write_int(f, SAVE_VERSION);

    game.autosaved = autosave;
    write_fields(f, gamefields, &game);
    game.autosaved = false;

    for (i = 0; i < game.maxclients; i++) {
        write_fields(f, clientfields, &game.clients[i]);
    }

    if (gzclose(f))
        gi.error("Couldn't write %s", filename);
}

static game_read_context_t make_read_context(gzFile f, int version)
{
    game_read_context_t ctx;
    ctx.f = f;
	ctx.save_ptrs = save_ptrs;
	ctx.num_save_ptrs = num_save_ptrs;
    return ctx;
}

void SVG_ReadGame(const char *filename)
{
    gzFile	f;
    int     i;

    gi.FreeTags(TAG_SVGAME);

    f = gzopen(filename, "rb");
    if (!f)
        gi.error("Couldn't open %s", filename);

    gzbuffer(f, 65536);

    i = read_int(f);
    if (i != SAVE_MAGIC1) {
        gzclose(f);
        check_gzip(i);
        gi.error("Not a Q2PRO save game");
    }

    i = read_int(f);
    if ((i != SAVE_VERSION)  && (i != 2)) {
        // Version 2 was written by Q2RTX 1.5.0, and the savegame code was crafted such to allow reading it
        gzclose(f);
        gi.error("Savegame from different version (got %d, expected %d)", i, SAVE_VERSION);
    }

    game_read_context_t ctx = make_read_context(f, i);

    read_fields(&ctx, gamefields, &game);

    // should agree with server's version
    if (game.maxclients != (int)maxclients->value) {
        gzclose(f);
        gi.error("Savegame has bad maxclients");
    }
    if (game.maxentities <= game.maxclients || game.maxentities > MAX_EDICTS) {
        gzclose(f);
        gi.error("Savegame has bad maxentities");
    }

    // Clamp maxentities within valid range.
    game.maxentities = QM_ClampUnsigned<uint32_t>( maxentities->integer, (int)maxclients->integer + 1, MAX_EDICTS );
    // Initialize the edicts array pointing to a the memory allocated within the pool.
    g_edicts = SVG_EdictPool_Reallocate( &g_edict_pool, game.maxentities );

	// Initialize a fresh clients array.
    game.clients = SVG_Clients_Reallocate( game.maxclients );
    // Now read in the fields for each client.
    for (i = 0; i < game.maxclients; i++) {
        // Now read in the fields.
        read_fields(&ctx, clientfields, &game.clients[i]);
    }

    gzclose(f);
}



/***
*
*
*
*
*   Level Read/Write:
*
*
*
*
***/
/*
=================
WriteLevel

=================
*/
void SVG_WriteLevel(const char *filename)
{
    int     i;
    svg_edict_t *ent;
    gzFile  f;

    f = gzopen(filename, "wb");
    if (!f)
        gi.error("Couldn't open %s", filename);

    write_int(f, SAVE_MAGIC2);
    write_int(f, SAVE_VERSION);

    // write out level_locals_t
    write_fields(f, levelfields, &level);

    // write out all the entities
    for (i = 0; i < globals.edictPool->num_edicts; i++) {
        ent = &g_edicts[i];
        if (!ent->inuse)
            continue;
        write_int(f, i);
        write_fields(f, entityfields, ent);
    }
    write_int(f, -1);

    if (gzclose(f))
        gi.error("Couldn't write %s", filename);
}


/*
=================
ReadLevel

SpawnEntities will allready have been called on the
level the same way it was when the level was saved.

That is necessary to get the baselines set up identically.

The server will have cleared all of the world links before
calling ReadLevel.

No clients are connected yet.
=================
*/
void SVG_MoveWith_FindParentTargetEntities( void );
void SVG_ReadLevel(const char *filename)
{
    int     entnum;
    gzFile  f;
    int     i;
    svg_edict_t *ent;

    // free any dynamic memory allocated by loading the level
    // base state
    gi.FreeTags(TAG_SVGAME_LEVEL);

    f = gzopen(filename, "rb");
    if (!f)
        gi.error("Couldn't open %s", filename);

    gzbuffer(f, 65536);

    // wipe all the entities
    for ( int32_t i = 0; i < game.maxentities; i++ ) {
        g_edicts[ i ] = {};
    }
    //memset(g_edicts, 0, sizeof(g_edicts[0]));
    //std::fill_n( &g_edicts[ i ], sizeof(svg_edict_t), 0 );

    globals.edictPool->num_edicts = maxclients->value + 1;

    i = read_int(f);
    if (i != SAVE_MAGIC2) {
        gzclose(f);
        check_gzip(i);
        gi.error("Not a Q2PRO save game");
    }

    i = read_int(f);
    if ((i != SAVE_VERSION) && (i != 2)) {
        // Version 2 was written by Q2RTX 1.5.0, and the savegame code was crafted such to allow reading it
        gzclose(f);
        gi.error("Savegame from different version (got %d, expected %d)", i, SAVE_VERSION);
    }

    game_read_context_t ctx = make_read_context(f, i);

    // load the level locals
    read_fields(&ctx, levelfields, &level);

    // load all the entities
    while (1) {
        entnum = read_int( f );
        if ( entnum == -1 )
            break;
        if ( entnum < 0 || entnum >= game.maxentities ) {
            gzclose( f );
            gi.error( "%s: bad entity number", __func__ );
        }
        if ( entnum >= globals.edictPool->num_edicts ) {
            globals.edictPool->num_edicts = entnum + 1;
        }

        ent = g_edicts + entnum;
        read_fields(&ctx, entityfields, ent);
        ent->inuse = true;
        ent->s.number = entnum;

        // let the server rebuild world links for this ent
        memset(&ent->area, 0, sizeof(ent->area));
        gi.linkentity(ent);
    }

    gzclose(f);

    // mark all clients as unconnected
    for ( i = 0; i < maxclients->value; i++ ) {
        ent = &g_edicts[ i + 1 ];
        ent->client = game.clients + i;
        ent->client->pers.connected = false;
        ent->client->pers.spawned = false;
    }

    // do any load time things at this point
    for (i = 0 ; i < globals.edictPool->num_edicts ; i++) {
        ent = &g_edicts[i];

        if (!ent->inuse)
            continue;

        // fire any cross-level triggers
        if (ent->classname)
            if (strcmp( (const char *)ent->classname, "target_crosslevel_target") == 0)
                ent->nextthink = level.time + QMTime::FromSeconds( ent->delay );
        #if 0
        if (ent->think == func_clock_think || ent->use == func_clock_use) {
            char *msg = ent->message;
			// WID: C++20: Added cast.
            ent->message = (char*)gi.TagMalloc(CLOCK_MESSAGE_SIZE, TAG_SVGAME_LEVEL);
            if (msg) {
                Q_strlcpy(ent->message, msg, CLOCK_MESSAGE_SIZE);
                gi.TagFree(msg);
            }
        }
        #endif
    }

    // Connect all movewith entities.
    //SVG_MoveWith_FindParentTargetEntities();
}

