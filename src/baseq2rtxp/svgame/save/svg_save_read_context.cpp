/********************************************************************
*
*
*	Save Game Mechanism:
*
*	A read context wrapper for conveniently passing around as to
*	that we can read the game data to/from disk anywhere from
*	anywhere within the code.
*
*
********************************************************************/
// Game related types.
#include "svgame/svg_local.h"
// Save related types.
#include "svgame/svg_save.h"
#include "svgame/svg_game_items.h"

/**
*   @brief Creates a game read context with the specified file and version.
*   @param f The gzFile object representing the file to read from.
*   @param version The version of the game data being read.
*   @return A game_read_context_t object initialized with the provided file and version.
**/
game_read_context_t game_read_context_t::make_read_context( gzFile f ) {
    game_read_context_t ctx;
    ctx.f = f;
    return ctx;
}

/**
*   @brief  Read data from the file.
**/
void game_read_context_t::read_data( void *buf, size_t len ) {
    if ( gzread( f, buf, len ) != len ) {
        gzclose( f );
        gi.error( "%s: couldn't read %zu bytes", __func__, len );
    }
}

/**
*   @brief  Reads a 16-bit short integer from a gzFile, converts it to little-endian format, and returns it as an int.
*   @param  f The gzFile from which the 16-bit short integer is read.
*   @return  The 16-bit short integer, converted to little-endian format, as an int.
**/
const int16_t game_read_context_t::read_short() {
    int16_t v;

    read_data( &v, sizeof( v ) );
    v = LittleShort( v );

    return v;
}

/**
*   @brief Reads a 32-bit integer from a gzFile, converting it to little-endian format.
*   @param f The gzFile from which the integer is read.
*   @return The 32-bit integer read from the file, converted to little-endian format.
**/
const int32_t game_read_context_t::read_int32() {
    int32_t v;

    read_data( &v, sizeof( v ) );
    v = LittleLong( v );

    return v;
}
/**
*   @brief  Reads a 64-bit integer from a gzFile, converting it to little-endian format.
*   @return The 64-bit integer read from the file, converted to little-endian format.
**/
const int64_t game_read_context_t::read_int64() {
    int64_t v;

    read_data( &v, sizeof( v ) );
    v = LittleLongLong( v );

    return v;
}
/**
*   @brief  Reads a floating-point value from the game context, converting it to little-endian format if necessary.
*   @return The floating-point value read from the game context.
**/
const float game_read_context_t::read_float() {
    float v;

    read_data( &v, sizeof( v ) );
    v = LittleFloat( v );

    return v;
}
/**
*   @brief  Reads a double-precision floating-point value from the game context, converting it from little-endian format if necessary.
*   @return The double-precision floating-point value read from the context.
**/
const double game_read_context_t::read_double() {
    double v;

    read_data( &v, sizeof( v ) );
    v = LittleDouble( v );

    return v;
}
/**
*   @brief  Reads a string from the game context, allocating memory for it and ensuring it is null-terminated.
*   @return A pointer to the null-terminated string read from the context, or NULL if the length is -1. If an invalid length is encountered, the function terminates with an error.
**/
char *game_read_context_t::read_string() {
    int32_t len;
    char *s;

    len = read_int32();
    if ( len == -1 ) {
        return NULL;
    }

    if ( len < 0 || len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length", __func__ );
    }

    // WID: C++20: Added cast.
    s = (char *)gi.TagMallocz( len + 1, TAG_SVGAME_LEVEL );
    read_data( s, len );
    s[ len ] = 0;

    return s;
}

/**
*   @brief Reads a level tag string from the read game context.
*   @return An instance of `svg_level_qstring_t` containing the level tag string, or `nullptr` if the length is -1.
**/
const svg_level_qstring_t game_read_context_t::read_level_qstring() {
    int32_t len;

    len = read_int32();
    if ( len == -1 ) {
        return nullptr;
    }

    if ( len < 0 || len >= UINT16_MAX ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }

    // Allocate level tag string space.
    // <Q2RTXP>: WID: We use len - 1 since the saved length already includes the null terminator,
    // and using new_for_size will allocate an extra byte for the null terminator.
    svg_level_qstring_t lstring = svg_level_qstring_t::new_for_size( len - 1);
    // Delete temporary buffer.
    read_data( lstring.ptr, lstring.size() );
    return lstring;
}
/**
*   @brief Reads a game tag string from the read game context.
*   @return An instance of `svg_game_qstring_t` containing the game tag string, or `nullptr` if the length is -1.
**/
const svg_game_qstring_t game_read_context_t::read_game_qstring() {
    int32_t len;

    len = read_int32();
    if ( len == -1 ) {
        return nullptr;
    }

    if ( len < 0 || len >= UINT16_MAX ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }

    // Allocate level tag string space.
	// <Q2RTXP>: WID: We use len - 1 since the saved length already includes the null terminator,
	// and using new_for_size will allocate an extra byte for the null terminator.
    svg_game_qstring_t gstring = svg_game_qstring_t::new_for_size( len - 1 );
    // Delete temporary buffer.
    read_data( gstring.ptr, gstring.size() );
    return gstring;
}

/**
*   @brief  Reads a level qtag memory block from the read game context.
**/
template<typename T>
sg_qtag_memory_t<T, TAG_SVGAME_LEVEL> *game_read_context_t::read_level_qtag_memory( sg_qtag_memory_t<T, TAG_SVGAME_LEVEL> *p ) {
    int32_t len;

    len = read_int32();
    if ( len == -1 ) {
        return allocate_qtag_memory<T, TAG_SVGAME_LEVEL>( p, 1 );
    }

    if ( len < 0 || len >= UINT32_MAX ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }

    allocate_qtag_memory<T, TAG_SVGAME_LEVEL>( p, len );
    read_data( p->ptr, /*len*/p->size() );
    return p;
}
/**
*   @brief  Reads a game qtag memory block from the read game context.
**/
template<typename T>
sg_qtag_memory_t<T, TAG_SVGAME> *game_read_context_t::read_game_qtag_memory( sg_qtag_memory_t<T, TAG_SVGAME> *p ) {
    int32_t len;

    len = read_int32();
    if ( len == -1 ) {
        return allocate_qtag_memory<T, TAG_SVGAME>( p, 1 );
    }

    if ( len < 0 || len >= UINT32_MAX ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }

    allocate_qtag_memory<T, TAG_SVGAME>( p, len );
    read_data( p->ptr, /*len*/p->size() );
    return p;
}

/**
*   @brief Reads a null-terminated string from the read game context.
**/
void game_read_context_t::read_zstring( char *s, size_t size ) {
    int32_t len;

    len = read_int32();
    if ( len < 0 || len >= size ) {
        gzclose( f );
        gi.error( "%s: bad length(%d) >= size(%d)", __func__, len, size );
    }

    read_data( s, len );
    s[ len ] = 0;
}
/**
*   @brief  Reads a vector3 from the read game context.
**/
void game_read_context_t::read_vector3( vec_t *v ) {
    v[ 0 ] = static_cast<vec_t>( read_float() );
    v[ 1 ] = static_cast<vec_t>( read_float() );
    v[ 2 ] = static_cast<vec_t>( read_float() );
}
/**
*   @brief  Reads a vector4 from the read game context.
**/
void game_read_context_t::read_vector4( vec_t *v ) {
    v[ 0 ] = static_cast<vec_t>( read_float() );
    v[ 1 ] = static_cast<vec_t>( read_float() );
    v[ 2 ] = static_cast<vec_t>( read_float() );
    v[ 3 ] = static_cast<vec_t>( read_float() );
}
/**
*   @brief  Reads a ranged index list from the read game context.
**/
void *game_read_context_t::read_index( size_t size, void *start, int max_index ) {
    int32_t index;
    byte *p;

    index = read_int32();
    if ( index == -1 ) {
        return NULL;
    }

    if ( index < 0 || index > max_index ) {
        gzclose( f );
        gi.error( "%s: bad index", __func__ );
    }

    p = (byte *)start + index * size;
    return p;
}
/**
*   @brief  Reads a callback function pointer from the read game context.
*   @param  type The type of the pointer to read.
*   @return A pointer to the data of the specified type, or NULL if the index is -1.
**/
void *game_read_context_t::read_pointer( svg_save_funcptr_type_t type ) {
    // Verify whether it was a nullptr or not.
    svg_save_funcptr_type_t saveAbleType = static_cast<svg_save_funcptr_type_t>( read_int32() );
    if ( saveAbleType == -1 ) {
        return nullptr;
    }

    //   const char *name = read_string();
       //if ( !name ) {
       //	gzclose( f );
       //	gi.error( "%s: bad name", __func__ );
       //}
    const char *name = "";

    // Read the saveable typeID.
    size_t saveAbleTypeID = read_int32();
    if ( saveAbleType >= 0 && saveAbleTypeID >= 0 ) {
        // Get the instance of the pointer for said 'type'.
        const svg_save_funcptr_instance_t *saveAbleInstance = svg_save_funcptr_instance_t::GetForTypeID( saveAbleTypeID );
        if ( saveAbleInstance ) {
            return saveAbleInstance->ptr;
        }
        return nullptr;
    } else {
        gzclose( f );
        gi.error( "%s: failed finding \"%s\" in the saveAble list.", __func__, name );
        return nullptr;
    }

    return nullptr;
}
/**
*   @brief  Reads a field from the read game context.
*   @param  field The field to read.
*   @param  base The base address of the structure to read from.
**/
void game_read_context_t::read_field( const svg_save_descriptor_field_t *field, void *base ) {
    void *p = (byte *)base + field->offset;
    int i;

    switch ( field->type ) {
    case SD_FIELD_TYPE_INT8:
        read_data( p, field->size );
        break;
    case SD_FIELD_TYPE_INT16:
        for ( i = 0; i < field->size; i++ ) {
            ( (short *)p )[ i ] = read_short();
        }
        break;
    case SD_FIELD_TYPE_INT32:
        for ( i = 0; i < field->size; i++ ) {
            ( (int *)p )[ i ] = read_int32();
        }
        break;
    case SD_FIELD_TYPE_BOOL:
        //for ( i = 0; i < field->size; i++ ) {
        //    ( (bool *)p )[ i ] = read_int32();
        //}
        // <Q2RTXP>: WID: We now store it as a byte, disregarding the old qboolean
        read_data( p, field->size );
        break;
    case SD_FIELD_TYPE_FLOAT:
        for ( i = 0; i < field->size; i++ ) {
            ( (float *)p )[ i ] = read_float();
        }
        break;
    case SD_FIELD_TYPE_DOUBLE:
        for ( i = 0; i < field->size; i++ ) {
            ( (double *)p )[ i ] = read_double();
        }
        break;
    case SD_FIELD_TYPE_VECTOR3:
        read_vector3( (vec_t *)p );
        break;
    case SD_FIELD_TYPE_VECTOR4:
        read_vector4( (vec_t *)p );
        break;
    case SD_FIELD_TYPE_LSTRING:
        *(char **)p = read_string();
        break;
    case SD_FIELD_TYPE_LEVEL_QSTRING:
        ( *(svg_level_qstring_t *)p ) = read_level_qstring();
        break;
    case SD_FIELD_TYPE_GAME_QSTRING:
        ( *(svg_game_qstring_t *)p ) = read_game_qstring();
        break;
    case SD_FIELD_TYPE_LEVEL_QTAG_MEMORY:
        read_level_qtag_memory<float>( ( ( sg_qtag_memory_t<float, TAG_SVGAME_LEVEL> * )p ) );
        break;
    case SD_FIELD_TYPE_GAME_QTAG_MEMORY:
        read_game_qtag_memory<float>( ( ( sg_qtag_memory_t<float, TAG_SVGAME> * )p ) );
        break;
    case SD_FIELD_TYPE_ZSTRING:
        read_zstring( (char *)p, field->size );
        break;
    case SD_FIELD_TYPE_EDICT:
        // WID: C++20: Added cast.
        ( *(svg_base_edict_t **)p ) = g_edict_pool.EdictForNumber( read_int32() );
        //*(svg_base_edict_t **)p = (svg_base_edict_t*)read_index(ctx->f, sizeof(svg_base_edict_t), g_edicts, game.maxentities - 1);
        break;
    case SD_FIELD_TYPE_CLIENT:
        // WID: C++20: Added cast.
        //*(svg_client_t **)p = (svg_client_t*)read_index(ctx->f, sizeof(svg_client_t), game.clients, game.maxclients - 1);
        ( *(svg_client_t **)p )->clientNum = read_int32();
        break;
    case SD_FIELD_TYPE_ITEM:
        // WID: C++20: Added cast.
        *(gitem_t **)p = (gitem_t *)read_index( sizeof( gitem_t ), itemlist, ( game.num_items - 1 ) );
        break;

    case SD_FIELD_TYPE_FUNCTION:
        // WID: C++20: Added cast.
        *(void **)p = read_pointer( (svg_save_funcptr_type_t)field->flags );
        break;

    case SD_FIELD_TYPE_FRAMETIME:
        // WID: We got QMTime, so we need int64 saving for frametime.
        for ( i = 0; i < field->size; i++ ) {
            ( (int64_t *)p )[ i ] = read_int64();
        }
        break;
    case SD_FIELD_TYPE_INT64:
        for ( i = 0; i < field->size; i++ ) {
            ( (int64_t *)p )[ i ] = read_int64();
        }
        break;

    default:
        #if !defined( _USE_DEBUG )
        gi.error( "%s: unknown field type(%d)", __func__, field->type );
        #else
        gi.error( "%s: unknown field type(%d), name(%s)", __func__, field->type, field->name );
        #endif
    }
}
/**
*   @brief Reads and processes a list of descriptor fields for a game context.
*   @param descriptorFields A pointer to an array of svg_save_descriptor_field_t structures, each describing a field to be read. The array must be null-terminated (field->type must be 0 to indicate the end).
*   @param base A pointer to the base address where the fields will be read and processed.
**/
void game_read_context_t::read_fields( const svg_save_descriptor_field_t *descriptorFields, void *base ) {
    const svg_save_descriptor_field_t *field;

    for ( field = descriptorFields; field->type; field++ ) {
        read_field( field, base );
    }
}
