/************************************************************
*
*
*   Save Game Mechanism :
*
*   A write context wrapper for conveniently passing around as to
*   that we can write the game data to / from disk anywhere from
*   anywhere within the code.
*
*
********************************************************************/
// Game related types.
#include "svgame/svg_local.h"
// Save related types.
#include "svgame/svg_save.h"
#include "svgame/svg_game_items.h"



/**
*   @brief  Used for svg_base_edict_t and derived members.
*   @param  f The gzFile object representing the file to write to.
*   @return A game_write_context_t object initialized with the provided file.
**/
game_write_context_t game_write_context_t::make_write_context( gzFile f ) {
    game_write_context_t ctx;
    ctx.f = f;
    return ctx;
}


/**
*
*
*
*   Game Write Context:
*
*
*
**/
void game_write_context_t::write_data( const void *buf, size_t len ) {
    if ( gzwrite( f, buf, len ) != len ) {
        gzclose( f );
        gi.error( "%s: couldn't write %zu bytes", __func__, len );
    }
}

void game_write_context_t::write_int16( int16_t v ) {
    v = LittleShort( v );
    write_data( &v, sizeof( v ) );
}

void game_write_context_t::write_int32( int32_t v ) {
    v = LittleLong( v );
    write_data( &v, sizeof( v ) );
}

void game_write_context_t::write_int64( int64_t v ) {
    v = LittleLongLong( v );
    write_data( &v, sizeof( v ) );
}

void game_write_context_t::write_float( float v ) {
    v = LittleFloat( v );
    write_data( &v, sizeof( v ) );
}

void game_write_context_t::write_double( double v ) {
    v = LittleDouble( v );
    write_data( &v, sizeof( v ) );
}

void game_write_context_t::write_string( const char *s ) {
    size_t len;

    if ( !s ) {
        write_int32( -1 );
        return;
    }

    len = strlen( s );
    if ( len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length", __func__ );
    }
    write_int32( len );
    write_data( s, len );
}

void game_write_context_t::write_level_qstring( svg_level_qstring_t *qstr ) {
    if ( !qstr || !qstr->ptr || qstr->count <= 0 ) {
        write_int32( -1 );
        return;
    }

    const size_t len = qstr->count;
    if ( len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }

    write_int32( len );
    write_data( qstr->ptr, len * sizeof( char ) );
    return;
}

void game_write_context_t::write_game_qstring( svg_game_qstring_t *qstr ) {
    if ( !qstr || !qstr->ptr || qstr->count <= 0 ) {
        write_int32( -1 );
        return;
    }

    const size_t len = qstr->count;
    if ( len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }

    write_int32( len );
    write_data( qstr->ptr, len * sizeof( char ) );
    return;
}

/**
*   @brief  Write level qtag memory block to disk.
**/
template<typename T, int32_t tag>
void game_write_context_t::write_level_qtag_memory( sg_qtag_memory_t<T, tag> *qtagMemory ) {
    if ( !qtagMemory || !qtagMemory->ptr || qtagMemory->count <= 0 ) {
        write_int32( -1 );
        return;
    }

    const size_t len = qtagMemory->count;
    if ( len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }
    write_int32( len );
    write_data( qtagMemory->ptr, len * sizeof( T ) );
    return;
}
/**
*   @brief  Write game qtag memory block to disk.
**/
template<typename T, int32_t tag>
void game_write_context_t::write_game_qtag_memory( sg_qtag_memory_t<T, tag> *qtagMemory ) {
    if ( !qtagMemory || !qtagMemory->ptr || qtagMemory->count <= 0 ) {
        write_int32( -1 );
        return;
    }

    const size_t len = qtagMemory->count;
    if ( len >= 65536 ) {
        gzclose( f );
        gi.error( "%s: bad length(%d)", __func__, len );
    }
    write_int32( len );
    write_data( qtagMemory->ptr, len * sizeof( T ) );
    return;
}

/**
*   @brief  Write a vector3 to the file.
**/
void game_write_context_t::write_vector3( const vec_t *v ) {
    write_float( v[ 0 ] );
    write_float( v[ 1 ] );
    write_float( v[ 2 ] );
}
/**
*   @brief  Write a vector4 to the file.
**/
void game_write_context_t::write_vector4( const vec_t *v ) {
    write_float( v[ 0 ] );
    write_float( v[ 1 ] );
    write_float( v[ 2 ] );
    write_float( v[ 3 ] );
}
/**
*   @brief  Write a ranged index to disk. (p must be within start and start + max_index * size).
**/
void game_write_context_t::write_index( const void *p, size_t size, const void *start, int max_index ) {
    uintptr_t diff;

    if ( !p ) {
        write_int32( -1 );
        return;
    }

    diff = (uintptr_t)p - (uintptr_t)start;
    if ( diff > max_index * size ) {
        gzclose( f );
        gi.error( "%s: pointer out of range: %p", __func__, p );
    }
    if ( diff % size ) {
        gzclose( f );
        gi.error( "%s: misaligned pointer: %p", __func__, p );
    }
    write_int32( (int32_t)( diff / size ) );
}
/**
*   @brief  Write a pointer to the file.
**/
void game_write_context_t::write_pointer( void *p, svg_save_funcptr_type_t type, const svg_save_descriptor_field_t *saveField ) {
    // Get the instance of the pointer for said 'type'.
    const svg_save_funcptr_instance_t *saveAbleInstance = svg_save_funcptr_instance_t::GetForPointer( p );
    if ( p && saveAbleInstance && saveAbleInstance->saveAbleType == type ) {
        // Write name.
        //if ( saveAbleInstance->name ) {
        //write_string( saveAbleInstance->name );
        // Write the Type.
        write_int32( saveAbleInstance->saveAbleType );
        // Write the TypeID.
        write_int32( saveAbleInstance->saveAbleTypeID.GetID() );
        return;
    } else if ( p == nullptr && ( saveAbleInstance == nullptr || saveAbleInstance->saveAbleType != type ) ) {
        // Nullptr.
        write_int32( -1 );
        return;
    }

    gzclose( f );
    #if USE_DEBUG
    gi.error( "%s: unknown pointer for '%s': %p", __func__, saveField->name, p );
    #else
    gi.error( "%s: unknown pointer: %p", __func__, p );
    #endif
}
/**
*   @brief  Write a field to the file.
*   @param  descriptorField The field descriptor to write.
*   @param  base The base address of the structure to write to.
**/
void game_write_context_t::write_field( const svg_save_descriptor_field_t *descriptorField, void *base ) {
    void *p = (byte *)base + descriptorField->offset;
    int i;

    switch ( descriptorField->type ) {
    case SD_FIELD_TYPE_INT8:
        write_data( p, descriptorField->size );
        break;
    case SD_FIELD_TYPE_INT16:
        for ( i = 0; i < descriptorField->size; i++ ) {
            write_int16( ( (int16_t *)p )[ i ] );
        }
        break;
    case SD_FIELD_TYPE_INT32:
        for ( i = 0; i < descriptorField->size; i++ ) {
            write_int32( ( (int32_t *)p )[ i ] );
        }
        break;
    case SD_FIELD_TYPE_BOOL:
        for ( i = 0; i < descriptorField->size; i++ ) {
            write_int32( ( (bool *)p )[ i ] );
        }
        break;
    case SD_FIELD_TYPE_FLOAT:
        for ( i = 0; i < descriptorField->size; i++ ) {
            write_float( ( (float *)p )[ i ] );
        }
        break;
    case SD_FIELD_TYPE_DOUBLE:
        for ( i = 0; i < descriptorField->size; i++ ) {
            write_double( ( (double *)p )[ i ] );
        }
        break;
    case SD_FIELD_TYPE_VECTOR3:
        write_vector3( (vec_t *)p );
        break;
    case SD_FIELD_TYPE_VECTOR4:
        write_vector4( (vec_t *)p );
        break;
    case SD_FIELD_TYPE_ZSTRING:
        write_string( (char *)p );
        break;
    case SD_FIELD_TYPE_LSTRING:
        write_string( *(char **)p );
        break;
    case SD_FIELD_TYPE_LEVEL_QSTRING:
        write_level_qstring( (svg_level_qstring_t *)p );
        break;
    case SD_FIELD_TYPE_GAME_QSTRING:
        write_game_qstring( (svg_game_qstring_t *)p );
        break;
    case SD_FIELD_TYPE_LEVEL_QTAG_MEMORY:
        write_level_qtag_memory<float>( ( ( sg_qtag_memory_t<float, TAG_SVGAME_LEVEL> * )p ) );
        break;
    case SD_FIELD_TYPE_GAME_QTAG_MEMORY:
        write_game_qtag_memory<float>( ( ( sg_qtag_memory_t<float, TAG_SVGAME> * )p ) );
        break;
    case SD_FIELD_TYPE_EDICT:
        write_int32( g_edict_pool.NumberForEdict( ( *(svg_base_edict_t **)p ) ) );
        //write_index(f, *(void **)p, sizeof(svg_base_edict_t), g_edicts, MAX_EDICTS - 1);
        break;
    case SD_FIELD_TYPE_CLIENT:
        write_int32( ( *(svg_client_t **)p )->clientNum );
        //write_index(f, *(void **)p, sizeof(svg_client_t), game.clients, game.maxclients - 1);
        break;
    case SD_FIELD_TYPE_ITEM:
        write_index( *(void **)p, sizeof( gitem_t ), itemlist, ( game.num_items - 1 ) );
        break;

    case SD_FIELD_TYPE_FUNCTION:
        // WID: C++20: Added cast.
        write_pointer( *(void **)p, (svg_save_funcptr_type_t)descriptorField->flags, descriptorField );
        break;

    case SD_FIELD_TYPE_FRAMETIME:
        // WID: We got QMTime, so we need int64 saving for frametime.
        for ( i = 0; i < descriptorField->size; i++ ) {
            write_int64( ( (int64_t *)p )[ i ] );
        }
        break;
    case SD_FIELD_TYPE_INT64:
        for ( i = 0; i < descriptorField->size; i++ ) {
            write_int64( ( (int64_t *)p )[ i ] );
        }
        break;

    default:
        //#if !defined(_USE_DEBUG)
        //gi.error( "%s: unknown descriptorField type(%d)", __func__, descriptorField->type );
        //#else
        gi.error( "%s: unknown descriptorField type(%d), name(%s)", __func__, descriptorField->type, descriptorField->name );
        //#endif
    }
}
/**
*   @brief  Write a list of descriptor fields to the file.
*   @param  descriptorFields A pointer to an array of svg_save_descriptor_field_t structures, each describing a field to be written. The array must be null-terminated (field->type must be 0 to indicate the end).
*   @param  base A pointer to the base address where the fields will be written.
**/
void game_write_context_t::write_fields( const svg_save_descriptor_field_t *descriptorFields, void *base ) {
    const svg_save_descriptor_field_t *field;

    for ( field = descriptorFields; field->type; field++ ) {
        write_field( field, base );
    }
}
