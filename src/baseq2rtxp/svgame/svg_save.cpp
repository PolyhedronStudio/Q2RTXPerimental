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
// Save related types.
#include "svgame/svg_save.h"

#include "svgame/svg_clients.h"
#include "svgame/svg_edict_pool.h"

#include "svgame/entities/svg_player_edict.h"


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

    INT32( clientNum ),
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
SAVE_DESCRIPTOR_FIELDS_BEGIN( svg_level_locals_t )
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, frameNumber, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, time, SD_FIELD_TYPE_FRAMETIME ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_SIZE( svg_level_locals_t, level_name, SD_FIELD_TYPE_ZSTRING, MAX_QPATH ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_SIZE( svg_level_locals_t, mapname, SD_FIELD_TYPE_ZSTRING, MAX_QPATH ),
    SAVE_DESCRIPTOR_DEFINE_FIELD_SIZE( svg_level_locals_t, nextmap, SD_FIELD_TYPE_ZSTRING, MAX_QPATH ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, intermissionFrameNumber, SD_FIELD_TYPE_INT64 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, changemap, SD_FIELD_TYPE_LSTRING ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, exitintermission, SD_FIELD_TYPE_FRAMETIME ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, intermission_origin, SD_FIELD_TYPE_VECTOR3 ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, intermission_angle, SD_FIELD_TYPE_VECTOR3 ),
    // Set in-game during the frame, invalid at load/save time.
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sight_client, SD_FIELD_TYPE_EDICT /*SD_FIELD_TYPE_CLIENT*/ ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sight_entity, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sight_entity_framenum, SD_FIELD_TYPE_FRAMETIME ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sound_entity, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sound_entity_framenum, SD_FIELD_TYPE_FRAMETIME ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sound2_entity, SD_FIELD_TYPE_EDICT ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, sound2_entity_framenum, SD_FIELD_TYPE_FRAMETIME ),
    SAVE_DESCRIPTOR_DEFINE_FIELD( svg_level_locals_t, body_que, SD_FIELD_TYPE_INT32 ),
SAVE_DESCRIPTOR_FIELDS_END();



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

//static void write_pointer(gzFile f, void *p, ptr_type_t type, const save_field_t *saveField ) {
//    const save_ptr_t *ptr;
//    int i;
//
//    if (!p) {
//        write_int(f, -1);
//        return;
//    }
//
//    for (i = 0, ptr = save_ptrs; i < num_save_ptrs; i++, ptr++) {
//        if (ptr->type == type && ptr->ptr == p) {
//            write_int(f, i);
//            return;
//        }
//    }
//
//    gzclose(f);
//    #if USE_DEBUG
//    gi.error( "%s: unknown pointer for '%s': %p", __func__, saveField->name, p );
//    #else
//    gi.error("%s: unknown pointer: %p", __func__, p);
//    #endif
//}

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
        write_int( f, g_edict_pool.NumberForEdict( ( *(svg_base_edict_t **)p ) ) );
        //write_index(f, *(void **)p, sizeof(svg_base_edict_t), g_edicts, MAX_EDICTS - 1);
        break;
    case F_CLIENT:
        write_int( f, ( *(svg_client_t **)p )->clientNum );
        //write_index(f, *(void **)p, sizeof(svg_client_t), game.clients, game.maxclients - 1);
        break;
    case F_ITEM:
        write_index(f, *(void **)p, sizeof(gitem_t), itemlist, game.num_items - 1);
        break;

    case F_POINTER:
		// WID: C++20: Added cast.
        //write_pointer(f, *(void **)p, (ptr_type_t)field->size, field );
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


//void *read_pointer( save_ptr_t type ) {
//    int index;
//    const save_ptr_t *ptr;
//
//    index = read_int();
//    if ( index == -1 ) {
//        return NULL;
//    }
//
//    if ( index < 0 || index >= num_save_ptrs ) {
//        gzclose( f );
//        gi.error( "%s: bad index", __func__ );
//    }
//
//    ptr = &save_ptrs[ index ];
//    if ( ptr->type != type ) {
//        gzclose( f );
//        gi.error( "%s: type mismatch", __func__ );
//    }
//
//    return ptr->ptr;
//}
/**
*   @brief  Reads a field from the read game context.
*   @param  field The field to read.
*   @param  base The base address of the structure to read from.
**/
static void read_field(game_read_context_t* ctx, const save_field_t *field, void *base)
{
    void *p = (byte *)base + field->ofs;
    int i;

    switch (field->type) {
    case F_BYTE:
        ctx->read_data(p, field->size );
        break;
    case F_SHORT:
        for (i = 0; i < field->size; i++) {
            ((short *)p)[i] = ctx->read_short();
        }
        break;
    case F_INT:
        for (i = 0; i < field->size; i++) {
            ((int *)p)[i] = ctx->read_int32();
        }
        break;
    case F_BOOL:
        for (i = 0; i < field->size; i++) {
            ((bool *)p)[i] = ctx->read_int32();
        }
        break;
    case F_FLOAT:
        for (i = 0; i < field->size; i++) {
            ((float *)p)[i] = ctx->read_float();
        }
        break;
    case F_DOUBLE:
        for ( i = 0; i < field->size; i++ ) {
            ( (double *)p )[ i ] = ctx->read_double();
        }
        break;
    case F_VECTOR3:
        ctx->read_vector3((vec_t *)p);
        break;
    case F_VECTOR4:
        ctx->read_vector4( (vec_t *)p );
        break;
    case F_LSTRING:
        *(char **)p = ctx->read_string();
        break;
    case F_LEVEL_QSTRING:
        ( *(svg_level_qstring_t *)p ) = ctx->read_level_qstring( );
        break;
    case F_GAME_QSTRING:
        ( *(svg_game_qstring_t *)p ) = ctx->read_game_qstring( );
        break;
    case F_LEVEL_QTAG_MEMORY:
        ctx->read_level_qtag_memory<float>( ( ( sg_qtag_memory_t<float, TAG_SVGAME_LEVEL> * )p ) );
        break;
    case F_GAME_QTAG_MEMORY:
        ctx->read_game_qtag_memory<float>( ( ( sg_qtag_memory_t<float, TAG_SVGAME> * )p ) );
        break;
    case F_ZSTRING:
        ctx->read_zstring((char *)p, field->size);
        break;
    case F_EDICT:
		// WID: C++20: Added cast.
        ( *(svg_base_edict_t **)p ) = g_edict_pool.EdictForNumber( ctx->read_int32( ) );
		//*(svg_base_edict_t **)p = (svg_base_edict_t*)read_index(ctx->f, sizeof(svg_base_edict_t), g_edicts, game.maxentities - 1);
        break;
    case F_CLIENT:
		// WID: C++20: Added cast.
		//*(svg_client_t **)p = (svg_client_t*)read_index(ctx->f, sizeof(svg_client_t), game.clients, game.maxclients - 1);
        ( *(svg_client_t **)p )->clientNum = ctx->read_int32( );
        break;
    case F_ITEM:
		// WID: C++20: Added cast.
        *(gitem_t **)p = (gitem_t*)ctx->read_index( sizeof(gitem_t), itemlist, game.num_items - 1);
        break;

    case F_POINTER:
		// WID: C++20: Added cast.
        //*(void **)p = read_pointer(ctx, (svg_save_descriptor_funcptr_type_t)field->flags);
        break;

    case F_FRAMETIME:
		// WID: We got QMTime, so we need int64 saving for frametime.
		for (i = 0; i < field->size; i++) {
	        ((int64_t *)p)[i] = ctx->read_int64( );
        }
        break;
	case F_INT64:
		for ( i = 0; i < field->size; i++ ) {
			( (int64_t *)p )[ i ] = ctx->read_int64( );
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



/**
*   @description    WriteGame
*
*   This will be called whenever the game goes to a new level,
*   and when the user explicitly saves the game.
*
*   Game information include cross level data, like multi level
*   triggers, help computer info, and all client states.
*
*   A single player death will automatically restore from the
*   last save position.
**/
void SVG_WriteGame( const char *filename, qboolean autosave ) {
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

/**
*   @brief  Reads a game save file and initializes the game state.
*   @param  filename The name of the save file to read.
*   @description    This function reads a save file and initializes the game state.
*                   It allocates memory for the game state and clients, and reads the game data from the file.
*                   It also checks the version of the save file and ensures that the game state is valid.
**/
void SVG_ReadGame( const char *filename ) {
    gzFile	f;
    int     i;

    //if ( g_edict_pool.edicts != nullptr || g_edict_pool.max_edicts != game.maxentities ) {
    //    SVG_EdictPool_Release( &g_edict_pool );
    //}
    gi.FreeTags(TAG_SVGAME);
    game = {};

    // ReInitialize the game local's movewith array.
    game.moveWithEntities = static_cast<game_locals_t::game_locals_movewith_t *>( gi.TagMalloc( sizeof( game_locals_t::game_locals_movewith_t ) * MAX_EDICTS, TAG_SVGAME ) );
    memset( game.moveWithEntities, 0, sizeof( game_locals_t::game_locals_movewith_t ) * MAX_EDICTS );

	// Open the file.
    f = gzopen(filename, "rb");
    if ( !f ) {
        gi.error( "Couldn't open %s", filename );
    }
	// Set the buffer size to 64k.
    gzbuffer(f, 65536);
    // Create a savegame read context.
    game_read_context_t ctx = game_read_context_t::make_read_context( f );

	// Read the magic number.
    i = ctx.read_int32();
    if (i != SAVE_MAGIC1) {
        gzclose(f);
        gi.error("Not a Q2RTXPerimental save game");
    }
	// Read the version number.
    i = ctx.read_int32();
    if ((i != SAVE_VERSION)  && (i != 2)) {
        // Version 2 was written by Q2RTX 1.5.0, and the savegame code was crafted such to allow reading it
        gzclose(f);
        gi.error("Savegame from different version (got %d, expected %d)", i, SAVE_VERSION);
    }

    // Read in svg_game_locals_t fields.
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
    // Release all edicts.
    g_edicts = SVG_EdictPool_Release( &g_edict_pool );
    // Initialize the edicts array pointing to the memory allocated within the pool.
    g_edicts = SVG_EdictPool_Allocate( &g_edict_pool, game.maxentities );
	g_edict_pool.max_edicts = game.maxentities;

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
    gzFile f = gzopen(filename, "wb");
    if ( !f ) {
        gi.error( "Couldn't open %s", filename );
    }

    // Create a savegame write context.
    game_write_context_t ctx = game_write_context_t::make_write_context( f );

    ctx.write_int32( SAVE_MAGIC2 );
    ctx.write_int32( SAVE_VERSION );

    // Write out all the entities.
    for ( int32_t i = 0; i < globals.edictPool->num_edicts; i++) {
        svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i );
        //ent = &g_edicts[i];
        if ( !ent || !ent->inuse ) {
            continue;
        }
        // Entity number.
        ctx.write_int32( i );
        // Entity classname.
        ctx.write_level_qstring( &ent->classname );
        // Rest of entity fields.
        ent->Save( &ctx );
        //write_fields(f, entityfields, ent);
    }

    // End of level data.
    ctx.write_int32( -1 );

    // Write out svg_level_locals_t. We do this after writing out the entities.
	// This is so that while loading the level, we initialize entities first
    // which in turn, the levelfields can optionally be pointing at.
	ctx.write_fields( svg_level_locals_t::saveDescriptorFields, &level );

	// Possibly throw an error if the file couldn't be written.
    if ( gzclose( f ) ) {
        gi.error( "Couldn't write %s", filename );
    }
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
    svg_base_edict_t *ent;

	// For recovering the level cm_entity_t's.
    static const cm_entity_t *cm_entities[ MAX_EDICTS ] = {};
	for ( int32_t i = 0; i < game.maxentities; i++ ) {
		cm_entities[ i ] = g_edict_pool.edicts[ i ]->entityDictionary;
	}

    // free any dynamic memory allocated by loading the level
    // base state.
    //
	// WID: Keep in mind that freeing entities after this, would invalidate their sg_qtag_memory_t blocks.
    gi.FreeTags(TAG_SVGAME_LEVEL);
    
    gzFile f = gzopen(filename, "rb");
    if ( !f ) {
        gi.error( "Couldn't open %s", filename );
    }
    gzbuffer(f, 65536);

    // Create read context.
    game_read_context_t ctx = game_read_context_t::make_read_context( f );

    // Wipe all the entities back to 'baseline'.
    for ( int32_t i = 0; i < game.maxentities; i++ ) {
        #if 0
            // Store original number.
            const int32_t number = g_edicts[ i ]->s.number;
            // Zero out.
            *g_edicts[ i ] = {}; //memset( g_edicts, 0, game.maxentities * sizeof( g_edicts[ 0 ] ) );
            // Retain the entity's original number.
            g_edicts[ i ]->s.number = number;
        #else
            //// Reset the entity to base state.
            //if ( g_edicts[ i ] ) {
            //    // Reset entity.
            //    g_edicts[ i ]->Reset( g_edicts[ i ]->entityDictionary );
            //} else {
                const int32_t entityNumber = g_edicts[ i ]->s.number;
                ////*g_edicts[ i ] = { };
                if ( i >= 1 && i < game.maxclients + 1 ) {
                    g_edict_pool.edicts[ i ] = new svg_player_edict_t();
                } else {
                    g_edict_pool.edicts[ i ] = new svg_base_edict_t( cm_entities[ i ] );
                }
                //// Set the number to the current index.
                g_edict_pool.edicts[ i ]->s.number = entityNumber;
            //}
        #endif
    }

    // Default num_edicts.
    g_edict_pool.num_edicts = maxclients->value + 1;

    int32_t i = ctx.read_int32();
    if (i != SAVE_MAGIC2) {
        gzclose(f);
        gi.error("Not a Q2RTXPerimental save game");
    }

    i = ctx.read_int32();
    if ((i != SAVE_VERSION) && (i != 2)) {
        // Version 2 was written by Q2RTX 1.5.0, and the savegame code was crafted such to allow reading it
        gzclose(f);
        gi.error("Savegame from different version (got %d, expected %d)", i, SAVE_VERSION);
    }

    // load all the entities
    while (1) {
        // Read in entity number.
        entnum = ctx.read_int32( );
        if ( entnum == -1 )
            break;
        if ( entnum < 0 || entnum >= game.maxentities ) {
            gzclose( f );
            gi.error( "%s: bad entity number", __func__ );
        }
        if ( entnum >= g_edict_pool.num_edicts ) {
            g_edict_pool.num_edicts = entnum + 1;
        }
        // Inquire for the classname.
		svg_level_qstring_t classname;
        classname = ctx.read_level_qstring();

        // Acquire the typeinfo.
        // TypeInfo for this entity.
        EdictTypeInfo *typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( classname.ptr );
        if ( !typeInfo ) {
            classname = "svg_base_edict_t";
            typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( classname.ptr );
        }

        // For restoring the cm_entity_t.
        const cm_entity_t *cm_entity = cm_entities[ entnum ];//g_edict_pool.edicts[ entnum ]->entityDictionary;
        // Worldspawn:
        svg_base_edict_t *ent = g_edict_pool.edicts[ entnum ] = typeInfo->allocateEdictInstanceCallback( nullptr );
        ent->classname = classname;
        // It was the original entity instanced at the map load time.
		// Restore the cm_entity_t.
        if ( ent->spawn_count == 0 ) {
            ent->entityDictionary = cm_entity;
        }
        // Restore the entity.

        ent->Restore( &ctx );

        ent->inuse = true;
        ent->s.number = entnum;

        // let the server rebuild world links for this ent
        memset(&ent->area, 0, sizeof(ent->area));
        gi.linkentity(ent);
    }

	// We load these right after the entities, so that we can
	// optionally point to them from the svg_level_locals_t fields.
    //read_fields( &ctx, levelfields, &level );
	ctx.read_fields( svg_level_locals_t::saveDescriptorFields, &level );

    gzclose(f);

    // Mark all clients as unconnected
    for ( i = 0; i < maxclients->value; i++ ) {
        //ent = &g_edicts[ i + 1 ];
        ent = g_edict_pool.EdictForNumber( i + 1 );
        ent->client = &game.clients[ i ];
        ent->client->pers.connected = false;
        ent->client->pers.spawned = false;
    }

    // do any load time things at this point
    for (i = 0 ; i < g_edict_pool.num_edicts ; i++) {
        ent = g_edict_pool.EdictForNumber( i ); //&g_edicts[i];

        if ( !ent || !ent->inuse ) {
            continue;
        }

        // fire any cross-level triggers
        if (ent->classname)
            if (strcmp( (const char *)ent->classname, "target_crosslevel_target") == 0)
                ent->nextthink = level.time + QMTime::FromSeconds( ent->delay );
        #if 0
        if (ent->think == func_clock_think || ent->use == func_clock_use) {
            char *msg = ent->message;
			// WID: C++20: Added cast.
            ent->message = (char*)gi.TagMallocz(CLOCK_MESSAGE_SIZE, TAG_SVGAME_LEVEL);
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

