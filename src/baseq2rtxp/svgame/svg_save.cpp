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
#include "svg_local.h"
// Save related types.
#include "svg_save.h"

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
#define SZ(name, size) _FA(F_ZSTRING, name, size)
#define BA(name, size) _FA(F_BYTE, name, size)
#define B(name) BA(name, 1)
#define SA(name, size) _FA(F_SHORT, name, size)
#define S(name) SA(name, 1)
#define IA(name, size) _FA(F_INT, name, size)
#define I(name) IA(name, 1)
#define OA(name, size) _FA(F_BOOL, name, size)
#define O(name) OA(name, 1)
#define FA(name, size) _FA(F_FLOAT, name, size)
#define F(name) FA(name, 1)
#define DA(name, size) _DA(F_DOUBLE, name, size)
#define D(name) DA(name, 1)
#define L(name) _F(F_LSTRING, name)
#define V(name) _F(F_VECTOR, name)
#define T(name) _F(F_ITEM, name)
#define E(name) _F(F_EDICT, name)
#define P(name, type) _FA(F_POINTER, name, type)
#define FT(name) _F(F_FRAMETIME, name)
#define I64A(name, size) _FA(F_INT64, name, size)
#define I64(name) IA(name, 1)

static const save_field_t entityfields[] = {
#define _OFS FOFS
    // [entity_state_s]:
    I( s.number ),
    //S( s.client ),
    I( s.entityType ),

    V( s.origin ),
    V( s.angles ),
    V( s.old_origin ),

    I( s.solid ),
    I( s.clipmask ),
    I( s.clipmask ),
    I( s.hullContents ),
    I( s.ownerNumber ),

    I( s.modelindex ),
    I( s.modelindex2 ),
    I( s.modelindex3 ),
    I( s.modelindex4 ),

    I( s.skinnum ),
    I( s.effects ),
    I( s.renderfx ),

    I( s.frame ),
    I( s.old_frame ),

    I( s.sound ),
    I( s.event ),

    // TODO: Do we really need to save this? Perhaps.
    // For spotlights.
    V( s.spotlight.rgb ),
    F( s.spotlight.intensity ),
    F( s.spotlight.angle_width ),
    F( s.spotlight.angle_falloff ),

    // [...]
    I( svflags ),
    V( mins ),
    V( maxs ),
    V( absmin ),
    V( absmax ),
    V( size ),
    I( solid ),
    I( clipmask ),
    I( hullContents ),
    E( owner ),

    I( spawn_count ),
    I( movetype ),
    I( flags ),

    L( model ),
    I64( freetime ), // WID: 64-bit-frame

    L( message ),
    L( classname ),
    I( spawnflags ),

    I64( timestamp ), // WID: 64-bit-frame FT(timestamp),

    F( angle ),

    L( targetname ),
    L( targetNames.target ),
    L( targetNames.kill ),
    L( targetNames.team ),
    L( targetNames.path ),
    L( targetNames.death ),
    L( targetNames.combat ),
    L( targetNames.movewith ),

    E( targetEntities.target ),
    E( targetEntities.movewith ),

    F( speed ),
    F( accel ),
    F( decel ),
    V( movedir ),
    V( pos1 ),
    V( pos2 ),

    V( velocity ),
    V( avelocity ),
    I( mass ),
    I64( air_finished_time ), // WID: 64-bit-frame FT(air_finished_time), FT(air_finished_time),
    F( gravity ),

    E( goalentity ),
    E( movetarget ),
    F( yaw_speed ),
    F( ideal_yaw ),

    I64( nextthink ),
    P( postspawn, P_postspawn ),
    P( prethink, P_prethink ),
    P( think, P_think ),
    P( postthink, P_postthink ),
    P( blocked, P_blocked ),
    P( touch, P_touch ),
    P( use, P_use ),
    P( pain, P_pain ),
    P( die, P_die ),

    I64( touch_debounce_time ),		// WID: 64-bit-frame FT(touch_debounce_time),
    I64( pain_debounce_time ),		// WID: 64-bit-frame FT(pain_debounce_time),
    I64( damage_debounce_time ),	// WID: 64-bit-frame FT(damage_debounce_time),
    I64( fly_sound_debounce_time ),	// WID: 64-bit-frame FT(fly_sound_debounce_time),
    I64( last_move_time ),			// WID: 64-bit-frame FT(last_move_time),

    I( health ),
    I( max_health ),
    I( gib_health ),
    I( deadflag ),
    I64( show_hostile ),

    L( map ),

    I( viewheight ),
    I( takedamage ),
    I( dmg ),
    I( radius_dmg ),
    F( dmg_radius ),
    F( light ),
    I( sounds ),
    I( count ),

    E( chain ),
    E( enemy ),
    E( oldenemy ),
    E( activator ),
    E( groundInfo.entity ),
    I( groundInfo.entityLinkCount ),
    E( teamchain ),
    E( teammaster ),

    E( mynoise ),
    E( mynoise2 ),

    I( noise_index ),
    I( noise_index2 ),
    F( volume ),
    F( attenuation ),

    F( wait ),
    F( delay ),
    F( random ),

    I64( last_sound_time ), // WID: 64-bit-frame  FT(last_sound_time),

    I( liquidInfo.type ),
    I( liquidInfo.level ),

    V( move_origin ),
    V( move_angles ),

    I( style ),
    L( customLightStyle ),

    T( item ),

    V( pushMoveInfo.start_origin ),
    V( pushMoveInfo.start_angles ),
    V( pushMoveInfo.end_origin ),
    V( pushMoveInfo.end_angles ),

    I( pushMoveInfo.sound_start ),
    I( pushMoveInfo.sound_middle ),
    I( pushMoveInfo.sound_end ),

    F( pushMoveInfo.accel ),
    F( pushMoveInfo.speed ),
    F( pushMoveInfo.decel ),
    F( pushMoveInfo.distance ),

    F( pushMoveInfo.wait ),

    I( pushMoveInfo.state ),
    V( pushMoveInfo.dir ),
    F( pushMoveInfo.current_speed ),
    F( pushMoveInfo.move_speed ),
    F( pushMoveInfo.next_speed ),
    F( pushMoveInfo.remaining_distance ),
    F( pushMoveInfo.decel_distance ),
    P( pushMoveInfo.endfunc, P_pusher_moveinfo_endfunc ),

    // WID: TODO: Monster Reimplement.
    //P( monsterinfo.currentmove, P_monsterinfo_currentmove ),
    //P( monsterinfo.nextmove, P_monsterinfo_nextmove ),
    //I( monsterinfo.aiflags ),
    //I64( monsterinfo.nextframe ), // WID: 64-bit-frame
    //F( monsterinfo.scale ),

    //P( monsterinfo.stand, P_monsterinfo_stand ),
    //P( monsterinfo.idle, P_monsterinfo_idle ),
    //P( monsterinfo.search, P_monsterinfo_search ),
    //P( monsterinfo.walk, P_monsterinfo_walk ),
    //P( monsterinfo.run, P_monsterinfo_run ),
    //P( monsterinfo.dodge, P_monsterinfo_dodge ),
    //P( monsterinfo.attack, P_monsterinfo_attack ),
    //P( monsterinfo.melee, P_monsterinfo_melee ),
    //P( monsterinfo.sight, P_monsterinfo_sight ),
    //P( monsterinfo.checkattack, P_monsterinfo_checkattack ),

    //I64( monsterinfo.next_move_time ),

    //I64( monsterinfo.pause_time ),// WID: 64-bit-frame FT(monsterinfo.pause_time),
    //I64( monsterinfo.attack_finished ),// WID: 64-bit-frame FT(monsterinfo.attack_finished),
    //I64( monsterinfo.fire_wait ),

    //V( monsterinfo.saved_goal ),
    //I64( monsterinfo.search_time ),// WID: 64-bit-frame FT(monsterinfo.search_time),
    //I64( monsterinfo.trail_time ),// WID: 64-bit-frame FT(monsterinfo.trail_time),
    //V( monsterinfo.last_sighting ),
    //I( monsterinfo.attack_state ),
    //I( monsterinfo.lefty ),
    //I64( monsterinfo.idle_time ),// WID: 64-bit-frame FT(monsterinfo.idle_time),
    //I( monsterinfo.linkcount ),

	// WID: C++20: Replaced {0}
    {}
#undef _OFS
};

static const save_field_t levelfields[] = {
#define _OFS LLOFS
	I64( framenum ),
	I64( time ), // WID: 64-bit-frame

	SZ( level_name, MAX_QPATH ),
	SZ( mapname, MAX_QPATH ),
	SZ( nextmap, MAX_QPATH ),

	I64( intermission_framenum ),

	L( changemap ),
	I64( exitintermission ),
	V( intermission_origin ),
	V( intermission_angle ),

	E( sight_client ),

	E( sight_entity ),
	I64( sight_entity_framenum ), // WID: 64-bit-frame
	E( sound_entity ),
	I64( sound_entity_framenum ), // WID: 64-bit-frame
	E( sound2_entity ),
	I64( sound2_entity_framenum ),// WID: 64-bit-frame

	I( pic_health ),

	I( total_secrets ),
	I( found_secrets ),

	I( total_goals ),
	I( found_goals ),

	I( total_monsters ),
	I( killed_monsters ),

    E( current_entity ),

	I( body_que ),

	// WID: C++20: Replaced {0}
	{}
#undef _OFS
};

static const save_field_t clientfields[] = {
#define _OFS CLOFS
	I( ps.pmove.pm_type ),
    S( ps.pmove.pm_flags ),
    S( ps.pmove.pm_time ),
    S( ps.pmove.gravity ),
	V( ps.pmove.origin ),
    V( ps.pmove.delta_angles ),
	V( ps.pmove.velocity ),
    B( ps.pmove.viewheight ),

	V( ps.viewangles ),
	V( ps.viewoffset ),
	V( ps.kick_angles ),

	//V( ps.gunangles ),
	//V( ps.gunoffset ),
	I( ps.gun.modelIndex ),
	I( ps.gun.animationID ),

    //FA( ps.damage_blend, 4 ),
	FA( ps.screen_blend, 4 ),
	F( ps.fov ),
	I( ps.rdflags ),
    I( ps.bobCycle ),
	IA( ps.stats, MAX_STATS ),

	SZ( pers.userinfo, MAX_INFO_STRING ),
	SZ( pers.netname, 16 ),
	I( pers.hand ),

    O( pers.connected ),
    O( pers.spawned ),

	I( pers.health ),
	I( pers.max_health ),
	I( pers.savedFlags ),

	I( pers.selected_item ),
	IA( pers.inventory, MAX_ITEMS ),
    
    T( pers.weapon ),
    T( pers.lastweapon ),
    IA( pers.weapon_clip_ammo, MAX_ITEMS ),

	I( pers.ammoCapacities.pistol ),
	I( pers.ammoCapacities.rifle ),
	I( pers.ammoCapacities.smg ),
	I( pers.ammoCapacities.sniper ),
	I( pers.ammoCapacities.shotgun ),

	I( pers.score ),

	O( pers.spectator ),

	O( showscores ),
	O( showinventory ),
	O( showhelp ),
	O( showhelpicon ),

    //I( buttons ),
    //I( oldbuttons ),
    //I( latched_buttons ),

	I( ammo_index ),

	T( newweapon ),

    O( weapon_thunk ),

    O( grenade_blew_up ),
    I64( grenade_time ),
    I64( grenade_finished_time ),

	I( frameDamage.armor ),
	I( frameDamage.blood ),
	I( frameDamage.knockBack ),
	V( frameDamage.from ),

	F( killer_yaw ),

	I( weaponState.mode ),
    I( weaponState.canChangeMode ),
    I( weaponState.aimState.isAiming ),
    I( weaponState.animation.currentFrame ),
    I( weaponState.animation.startFrame ),
    I( weaponState.animation.endFrame ),
    I64( weaponState.timers.lastEmptyWeaponClick ),
    I64( weaponState.timers.lastPrimaryFire ),
    I64( weaponState.timers.lastAimedFire ),
    I64( weaponState.timers.lastDrawn ),
    I64( weaponState.timers.lastHolster ),

    V( weaponKicks.offsetAngles ),
    V( weaponKicks.offsetOrigin ),

    V( viewMove.viewAngles ), V( viewMove.viewForward ),   
    I64( viewMove.damageTime ),
    I64( viewMove.fallTime ),
    I64( viewMove.quakeTime ),
	F( viewMove.damageRoll ), F( viewMove.damagePitch ),
	F( viewMove.fallValue ),

	F( damage_alpha ),
	F( bonus_alpha ),
	V( damage_blend ),
	I64( bobCycle ),
    I64( oldBobCycle ),
    D( bobFracSin ),
    I64( last_stair_step_frame ),
    V( last_ladder_pos ),
    I64( last_ladder_sound ),

	V( oldviewangles ),
	V( oldvelocity ),
    E( oldgroundentity ),
    I( old_waterlevel ),
	I64( next_drown_time ),

    I64( pickup_msg_time ), // WID: 64-bit-frame

    // WID: C++20: Replaced {0}
	{}
#undef _OFS
};

static const save_field_t gamefields[] = {
#define _OFS GLOFS
    I(maxclients),
    I(maxentities),
    I(gamemode),

    I(serverflags),

    I(num_items),

    O(autosaved),

	// WID: C++20: Replaced {0}
    {}
#undef _OFS
};

//=========================================================

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

static void write_vector(gzFile f, vec_t *v)
{
    write_float(f, v[0]);
    write_float(f, v[1]);
    write_float(f, v[2]);
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
    case F_VECTOR:
        write_vector(f, (vec_t *)p);
        break;
    case F_ZSTRING:
        write_string(f, (char *)p);
        break;
    case F_LSTRING:
        write_string(f, *(char **)p);
        break;

    case F_EDICT:
        write_index(f, *(void **)p, sizeof(edict_t), g_edicts, MAX_EDICTS - 1);
        break;
    case F_CLIENT:
        write_index(f, *(void **)p, sizeof(gclient_t), game.clients, game.maxclients - 1);
        break;
    case F_ITEM:
        write_index(f, *(void **)p, sizeof(gitem_t), itemlist, game.num_items - 1);
        break;

    case F_POINTER:
		// WID: C++20: Added cast.
        write_pointer(f, *(void **)p, (ptr_type_t)field->size, field );
        break;

    case F_FRAMETIME:
		// WID: We got sg_time_t, so we need int64 saving for frametime.
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
        gi.error("%s: unknown field type", __func__);
    }
}

static void write_fields(gzFile f, const save_field_t *fields, void *base)
{
    const save_field_t *field;

    for (field = fields; field->type; field++) {
        write_field(f, field, base);
    }
}

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

static void read_vector(gzFile f, vec_t *v)
{
    v[0] = static_cast<vec_t>( read_float(f) );
    v[1] = static_cast<vec_t>( read_float( f ) );
    v[2] = static_cast<vec_t>( read_float( f ) );
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
    case F_VECTOR:
        read_vector(ctx->f, (vec_t *)p);
        break;
    case F_LSTRING:
        *(char **)p = read_string(ctx->f);
        break;
    case F_ZSTRING:
        read_zstring(ctx->f, (char *)p, field->size);
        break;

    case F_EDICT:
		// WID: C++20: Added cast.
		*(edict_t **)p = (edict_t*)read_index(ctx->f, sizeof(edict_t), g_edicts, game.maxentities - 1);
        break;
    case F_CLIENT:
		// WID: C++20: Added cast.
		*(gclient_t **)p = (gclient_t*)read_index(ctx->f, sizeof(gclient_t), game.clients, game.maxclients - 1);
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
		// WID: We got sg_time_t, so we need int64 saving for frametime.
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
        gi.error("%s: unknown field type", __func__);
    }
}

static void read_fields(game_read_context_t* ctx, const save_field_t *fields, void *base)
{
    const save_field_t *field;

    for (field = fields; field->type; field++) {
        read_field(ctx, field, base);
    }
}

//=========================================================

#define SAVE_MAGIC1     MakeLittleLong('S','S','V','1')
#define SAVE_MAGIC2     MakeLittleLong('S','A','V','1')
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
void WriteGame(const char *filename, qboolean autosave)
{
    gzFile  f;
    int     i;

    if (!autosave)
        SaveClientData();

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

void ReadGame(const char *filename)
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

	// WID: C++20: Added cast.
    g_edicts = (edict_t*)gi.TagMalloc(game.maxentities * sizeof(g_edicts[0]), TAG_SVGAME);
    globals.edicts = g_edicts;
    globals.max_edicts = game.maxentities;
	
	// WID: C++20: Added cast.
    game.clients = (gclient_t*)gi.TagMalloc(game.maxclients * sizeof(game.clients[0]), TAG_SVGAME);
    for (i = 0; i < game.maxclients; i++) {
        read_fields(&ctx, clientfields, &game.clients[i]);
    }

    gzclose(f);
}

//==========================================================


/*
=================
WriteLevel

=================
*/
void WriteLevel(const char *filename)
{
    int     i;
    edict_t *ent;
    gzFile  f;

    f = gzopen(filename, "wb");
    if (!f)
        gi.error("Couldn't open %s", filename);

    write_int(f, SAVE_MAGIC2);
    write_int(f, SAVE_VERSION);

    // write out level_locals_t
    write_fields(f, levelfields, &level);

    // write out all the entities
    for (i = 0; i < globals.num_edicts; i++) {
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
void ReadLevel(const char *filename)
{
    int     entnum;
    gzFile  f;
    int     i;
    edict_t *ent;

    // free any dynamic memory allocated by loading the level
    // base state
    gi.FreeTags(TAG_SVGAME_LEVEL);

    f = gzopen(filename, "rb");
    if (!f)
        gi.error("Couldn't open %s", filename);

    gzbuffer(f, 65536);

    // wipe all the entities
    memset(g_edicts, 0, game.maxentities * sizeof(g_edicts[0]));
    globals.num_edicts = maxclients->value + 1;

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
        entnum = read_int(f);
        if (entnum == -1)
            break;
        if (entnum < 0 || entnum >= game.maxentities) {
            gzclose(f);
            gi.error("%s: bad entity number", __func__);
        }
        if (entnum >= globals.num_edicts)
            globals.num_edicts = entnum + 1;

        ent = &g_edicts[entnum];
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
    for (i = 0 ; i < globals.num_edicts ; i++) {
        ent = &g_edicts[i];

        if (!ent->inuse)
            continue;

        // fire any cross-level triggers
        if (ent->classname)
            if (strcmp(ent->classname, "target_crosslevel_target") == 0)
                ent->nextthink = level.time + sg_time_t::from_sec( ent->delay );

        if (ent->think == func_clock_think || ent->use == func_clock_use) {
            char *msg = ent->message;
			// WID: C++20: Added cast.
            ent->message = (char*)gi.TagMalloc(CLOCK_MESSAGE_SIZE, TAG_SVGAME_LEVEL);
            if (msg) {
                Q_strlcpy(ent->message, msg, CLOCK_MESSAGE_SIZE);
                gi.TagFree(msg);
            }
        }
    }
}

