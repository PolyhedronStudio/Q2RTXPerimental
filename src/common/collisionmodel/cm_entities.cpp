/**
*
*
*   Collision Model: Entity key/value pairs.
*
*
**/
#include "shared/shared.h"
#include "common/bsp.h"
#include "common/cmd.h"
#include "common/collisionmodel.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/files.h"
#include "common/math.h"
#include "common/sizebuf.h"
#include "common/zone.h"
#include "system/hunk.h"



//! Used to check whether CM_EntityValue was able/unable to find a matching key in the cm_entity_t.
static cm_entity_t cm_null_entity = { /*.nullable_string = nullptr*/ };

/**
*   @brief  Parse and set the appropriate pair value flags, as well as the value itself for the types
*           it can be represented by.
**/
static cm_entity_parsed_type_t CM_ParseKeyValueType( cm_entity_t *pair ) {
    // Bitflags of what types were successfully parsed and represented by the pair its value.
    cm_entity_parsed_type_t parsed_types = static_cast<cm_entity_parsed_type_t>( 0 );

    // Test for and parse the integer.
    if ( COM_IsInt( pair->nullable_string ) ) {
        // Succesfully parsed integer value, 
        if ( ( sscanf( pair->nullable_string, "%d", &pair->integer ) == 1 ) ) {
            pair->parsed_type = static_cast<cm_entity_parsed_type_t>( pair->parsed_type | cm_entity_parsed_type_t::ENTITY_INTEGER );
        } else {
            //Com_Error( ERR_DROP, "%s: EOF without closing brace", __func__ );
        }
    }
    //// Test for and parse the float.
    //if ( COM_IsFloat( pair->nullable_string ) ) {
    //    // Succesfully parsed float value, 
    //    if ( ( sscanf( pair->nullable_string, "%f", &pair->value ) == 1 ) ) {
    //        pair->parsed_type = static_cast<cm_entity_parsed_type_t>( pair->parsed_type | cm_entity_parsed_type_t::ENTITY_FLOAT );
    //    } else {
    //        Com_Error( ERR_DROP, "%s: EOF without closing brace", __func__ );
    //    }
    //    //pair->value = atof( pair->nullable_string );
    //    //pair->parsed_type = static_cast<cm_entity_parsed_type_t>( pair->parsed_type | cm_entity_parsed_type_t::ENTITY_FLOAT );
    //}
    
    // Test and for and parse the types: float, Vector2, Vector3, Vector4.
    // 
    // Perform a full sscanf on a Vector 4. The return value will know what vector types are contestant
    // for having their value set, and being flagged for valid parsing.
    Vector4 parsedVector = {};
    const int32_t components = sscanf( pair->nullable_string, "%f %f %f %f",
        &parsedVector.x, &parsedVector.y, &parsedVector.z, &parsedVector.w );

    // We need at least 2 components.
    if ( components >= 1 ) {
        for ( int32_t i = 1; i <= components; i++ ) {
            switch ( i ) {
            case 1:
                //if ( parsedVector[ 0 ] < 0 ) {
                //    Com_LPrintf( PRINT_DEVELOPER, "Found a float that is negative %f\n", parsedVector[ 0 ] );
                //}
                pair->value = /*pair->vec2[ 0 ] = pair->vec3[ 0 ] = pair->vec4[ 0 ] = */parsedVector[ 0 ];
                pair->integer = static_cast<int32_t>( pair->value );
                pair->parsed_type = static_cast<cm_entity_parsed_type_t>( pair->parsed_type | cm_entity_parsed_type_t::ENTITY_FLOAT );
                pair->parsed_type = static_cast<cm_entity_parsed_type_t>( pair->parsed_type | cm_entity_parsed_type_t::ENTITY_INTEGER );
                break;
            case 2:
                pair->vec2[ 0 ] = pair->vec3[ 0 ] = pair->vec4[ 0 ] = parsedVector[ 0 ];
                pair->vec2[ 1 ] = pair->vec3[ 1 ] = pair->vec4[ 1 ] = parsedVector[ 1 ];
                pair->parsed_type = static_cast<cm_entity_parsed_type_t>( pair->parsed_type | cm_entity_parsed_type_t::ENTITY_VECTOR2 );
                break;
            case 3:
                pair->vec2[ 0 ] = pair->vec3[ 0 ] = pair->vec4[ 0 ] = parsedVector[ 0 ];
                pair->vec2[ 1 ] = pair->vec3[ 1 ] = pair->vec4[ 1 ] = parsedVector[ 1 ];
                pair->vec3[ 2 ] = pair->vec4[ 2 ] = parsedVector[ 2 ];
                pair->parsed_type = static_cast<cm_entity_parsed_type_t>( pair->parsed_type | cm_entity_parsed_type_t::ENTITY_VECTOR2
                    | cm_entity_parsed_type_t::ENTITY_VECTOR3 );
                break;
            case 4:
                pair->vec2[ 0 ] = pair->vec3[ 0 ] = pair->vec4[ 0 ] = parsedVector[ 0 ];
                pair->vec2[ 1 ] = pair->vec3[ 1 ] = pair->vec4[ 1 ] = parsedVector[ 1 ];
                pair->vec3[ 2 ] = pair->vec4[ 2 ] = parsedVector[ 2 ];
                pair->vec4[ 3 ] = parsedVector[ 3 ];
                pair->parsed_type = static_cast<cm_entity_parsed_type_t>( pair->parsed_type | cm_entity_parsed_type_t::ENTITY_VECTOR2
                    | cm_entity_parsed_type_t::ENTITY_VECTOR3 | cm_entity_parsed_type_t::ENTITY_VECTOR4 );
                break;
            }
        }
    // Otherwise, make sure its value is zerod out.
    } else {
        // Since it is shared memory using a union, ensure that pair->value is emplaced
        // instead of zero-ed out.
        Vector4Set( pair->vec4, 0, 0, 0, 0 );
    }

    // We're done, return the parsed type flag(s).
    return pair->parsed_type;
}

/**
*   @brief  Parses the key:value pair string tokens. It'll break out in case of a closing bracket '}',
*           and error out in case of an unexpected token or EOF entityString.
*
*           If none of the above occures it proceeds to setting up the pair its key:string. Its member
*           'nullable_string' will remain nullptr in case of a non valid entry, otherwise it'll point
*           to the null terminated string data itself.
**/
static cm_entity_t *CM_ParseKeyValuePair( const char **entityString ) {
    // Pointer of the first key/value pair, and for the next, and next, and.. so on.
    cm_entity_t *entity = nullptr;

    // Pointer to the current token being processed.
    const char *com_token = nullptr;

    while ( 1 ) {
        // Next token. Check for a closing brace before proceeding ahead with parsing the key:pair sets
        com_token = COM_Parse( entityString );
        // Succeeded, break out.
        if ( com_token[ 0 ] == '}' ) {
            break;
        }

        // Mew pair.
        cm_entity_t *pair = static_cast<cm_entity_t *>( Z_TagMallocz( sizeof( cm_entity_t ), TAG_CMODEL ) );

        // Key:
        const char *key = com_token;
        if ( key[ 0 ] == '}' ) {
            break;
        }
        if ( !*entityString ) {
            Com_Error( ERR_DROP, "%s: EOF without closing brace", __func__ );
        }
        // Value:
        const char *value = COM_Parse( entityString );
        if ( !*entityString ) {
            Com_Error( ERR_DROP, "%s: EOF without closing brace", __func__ );
        }
        if ( value[ 0 ] == '}' ) {
            Com_Error( ERR_DROP, "%s: closing brace without data", __func__ );
        }
        // No parsing error occured, proceed and setup the key and string value representation.
        Q_strlcpy( pair->key, key, MAX_KEY );
        Q_strlcpy( pair->string, value, MAX_VALUE );

        // Set nullable string, which remains nullptr if we had no valid value.
        if ( Q_strnlen( pair->string, MAX_VALUE ) ) {
            pair->parsed_type = static_cast<cm_entity_parsed_type_t>( pair->parsed_type | ENTITY_STRING );
            pair->nullable_string = pair->string;
        }

        // Last but not least, determine and set flags for what types representing this key:value pair was parsed for.
        CM_ParseKeyValueType( pair );

        // Assign it as the next key/field.
        pair->next = entity;
        entity = pair;
    }

    return entity;
}

/**
*   @brief  Parse the bsp entitystring into a list of cm_entity_t entities, each listing
*           their next key:value pair sets.
**/
static const std::list<cm_entity_t *> CM_EntityDictionariesFromString( cm_t *cm ) {
    // The actual list containing pointers to memory allocated key:value pairs.
    std::list<cm_entity_t *> entities;

    // The whole entity string.
    const char *entityString = cm->entitystring;

    // Token.
    const char *com_token = nullptr;

    while ( 1 ) {
        // Parse until EOF entity string.
        com_token = COM_Parse( &entityString );
        if ( !entityString ) {
            // Break out when done.
            break;
        }
        // Since we are just starting, or came from a closing brace, the next token is expected
        // to be another opening brace '{'.
        if ( com_token[ 0 ] == '{' ) {
            // Start parsing the entities list of key:value pairs.
            cm_entity_t *entity = CM_ParseKeyValuePair( &entityString );

            // No more entities left to parse and/or ran into trouble.
            if ( !entity ) {
                // So we break out.
                break;
            }

            // Push back parsed entity result on the entity list.
            entities.push_back( entity );
            // We error out if we got something other than an opening brace '{'.
        } else {
            Com_Error( ERR_DROP, "BSP_EntityDictionariesFromString: found %s when expecting {", com_token );
        }
    }

    // Reverse.
    //entities.reverse();

    // Return.
    return entities;
}

/**
*   @brief  Parses the collision model its entity string into collision model entities
*           that contain a list of key/value pairs containing the key's value as well as
*           describing the types it got validly parsed for.
**/
void CM_ParseEntityString( cm_t *cm ) {
    // For the 'collision model', or rather the BSP world, entities are nothing but
    // a list each containing their own separate list of key:value sets.
    //
    // We'll parse the BSP entity string into a list of cm_entity_t key:value sets per entity.
    typedef std::list<cm_entity_t *> cm_entity_list_t;
    cm_entity_list_t entities = CM_EntityDictionariesFromString( cm );

    // Actually allocate the BSP entities array and fill it with the entities from our generated list.
    cm->numentities = entities.size();
    cm->entities = static_cast<const cm_entity_t **>( Z_TagMallocz( sizeof( cm_entity_t * ) * cm->numentities, TAG_CMODEL ) );

    const cm_entity_t **out = cm->entities;

    for ( cm_entity_list_t::const_iterator list = entities.cbegin(); list != entities.cend(); ++list, out++ ) {
        *out = *list;
    }
    //cm->numentities = entities.size();
    //cm->entities = static_cast<const cm_entity_t **>( Z_TagMallocz( sizeof( cm_entity_t * ) * cm->numentities, TAG_CMODEL ) );

    //const cm_entity_t **out = cm->entities;
    //int32_t i = 0;
    //for ( cm_entity_list_t::const_iterator list = entities.cbegin(); list != entities.cend(); ++list, out++ ) {
    //    //cm->entities[i] = *list;
    //    *out = *list;
    //}

    // Clear the list from memory.
    entities.clear();

    // DEBUG OUTPUT.
    #if 1
    int32_t entityID = 0;
    const cm_entity_t *ent = nullptr;
    for ( size_t i = 0; i < cm->numentities; i++ ) {
        ent = cm->entities[ i ];
        Com_LPrintf( PRINT_DEVELOPER, "Entity(#%i) {\n", entityID );
        const cm_entity_t *kv = ent;
        while ( kv ) {
            if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_STRING ) {
                Com_LPrintf( PRINT_DEVELOPER, "\"%s\":\"%s\" parsed for type(string) value=(%s) \n", kv->key, kv->string, kv->nullable_string );
            }
            if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
                Com_LPrintf( PRINT_DEVELOPER, "\"%s\":\"%s\" parsed for type(integer) value=(%d) \n", kv->key, kv->string, kv->integer );
            }
            if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_FLOAT ) {
                Com_LPrintf( PRINT_DEVELOPER, "\"%s\":\"%s\" parsed for type(float) value=(%f) \n", kv->key, kv->string, kv->value );
            }
            if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR2 ) {
                Com_LPrintf( PRINT_DEVELOPER, "\"%s\":\"%s\" parsed for type(Vector2) value=(%f, %f) \n", kv->key, kv->string, kv->vec2[0], kv->vec2[ 1 ] );
            }
            if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR3 ) {
                Com_LPrintf( PRINT_DEVELOPER, "\"%s\":\"%s\" parsed for type(Vector3) value=(%f, %f, %f) \n", kv->key, kv->string, kv->vec3[ 0 ], kv->vec3[ 1 ], kv->vec3[ 2 ] );
            }
            if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR4 ) {
                Com_LPrintf( PRINT_DEVELOPER, "\"%s\":\"%s\" parsed for type(Vector4) value=(%f, %f, %f, %f) \n", kv->key, kv->vec4[ 0 ], kv->vec4[ 1 ], kv->vec4[ 2 ], kv->vec4[ 3 ] );
            }
            kv = kv->next;
        }
        Com_LPrintf( PRINT_DEVELOPER, "}\n" );
        entityID++;
    }
    #endif // #if 0
}

/**
 * @brief
 */
const int32_t CM_EntityNumber( const cm_t *cm, const cm_entity_t *entity ) {
    // This should technically never happen, but prevent crashing if it did.
    if ( !cm || !cm->cache ) {
        return -1;
    }
    // Iterate over number of entities until we find the one that matches to our entity pointer,
    // then return its index.
    for ( int32_t i = 0; i < cm->numentities; i++ ) {
        if ( cm->entities[ i ] == entity ) {
            return i;
        }
    }

    return -1;
}

/**
*   @brief  Used to check whether CM_EntityValue was able/unable to find a matching key in the cm_entity_t.
*   @return Pointer to the collision model system's 'null' entity key/pair.
**/
const cm_entity_t *CM_GetNullEntity( void ) {
    return &cm_null_entity;
}

/**
*   @brief  Looks up the key/value cm_entity_t pair in the list for the cm_entity_t entity.
*   @return If found, a pointer to the key/value pair, otherwise a pointer to the 'cm_null_entity', 
*           which will have a nullptr value set for its 'nullable_string'.
**/
const cm_entity_t *CM_EntityKeyValue( const cm_entity_t *entity, const char *key ) {
    // Can't seek for a key if it is invalid.
    if ( !key ) {
        // Return a pointer to the 'null entity'.
        return CM_GetNullEntity();
    }

    // Iterate over all key/value pairs of this entity dictionary.
    for ( const cm_entity_t *e = entity; e; e = e->next ) {
        // If the key matches the one we sought for, return a pointer to the cm_entity_t key/value pair entry.
        if ( !Q_stricmp( e->key, key ) ) {
            return e;
        }
    }

    // Return a pointer to the 'null entity'.
    return CM_GetNullEntity();
}