/**
*
*
*   Collision Model: Materials
*
*
**/
#include "shared/shared.h"
#include "common/bsp.h"
#include "common/collisionmodel.h"
#include "common/common.h"
#include "common/files.h"
#include "common/math.h"
#include "common/zone.h"
#include "system/hunk.h"

// Jasmine json parser.
#define JSMN_STATIC
#include "shared/jsmn.h"

//! Enables material debug output.
//#define _DEBUG_MAT_SUCCESSFUL 0
#define _DEBUG_MAT_PRINT_JSONWAL_FAILURES 1
//#define _DEBUG_MAT_PRINT_JSONWAL_COULDNOTREAD 1

//! Default material to apply properties from instead when no specific material json was found.
//! NOTE: 
cm_material_t cm_default_material = {
    // Appoint to nulltexinfo.
    .materialID = 0,
    .name = "cm_default_material", // "nulltexinfo"
    
    //! Physical material properties:
    .physical {
        //! Default material type.
        //.type = MAT_TYPE_DEFAULT,
        .kind = "default",
        //! Regular default friction.
        .friction = 6.f
    }
};

//! Material for nulltexinfo surfaces(Used for the generated entity hulls.)
cm_material_t cm_nulltexinfo_material = {
    .materialID = 0,
    .name = "cm_nulltexinfo_material",

    .physical = cm_default_material.physical
};

/**
*   @return -1 if non existent. The materialID otherwise.
**/
const int32_t CM_MaterialExists( cm_t *cm, const char *name ) {
    // -1 indicates it does not exist.
    int32_t materialID = -1;

    // Stores the resulting materialID to return.
    for ( int32_t i = 1; i < cm->num_materials + 1; i++ ) {
        if ( strcmp( cm->materials[ i ].name, name ) == 0 ) {
            materialID = i;
            break;
        }
    }

    // Return the material ID.
    return materialID;
}

static const int32_t jsoneq( const char *json, jsmntok_t *tok, const char *s ) {
    // Need a valid token ptr.
    if ( tok == nullptr ) {
        return -1;
    }

    if ( tok->type == JSMN_STRING && (int)strlen( s ) == tok->end - tok->start &&
        strncmp( json + tok->start, s, tok->end - tok->start ) == 0 ) {
        return 1;
    }
    return -1;
}

/**
*   @brief  Tries to find a matching '.wal_json' file to load in order
*           to read specific material properties out of.
* 
*           In case of failure it'll return the index of the 'default' material instead.
**/
const int32_t CM_LoadMaterialFromJSON( cm_t *cm, const char *name, const char *jsonPath ) {
    // Buffer meant to be filled with the file's json data.
    char *jsonBuffer = NULL;

    // Ensure the file is existent.
    if ( !FS_FileExistsEx( jsonPath, FS_TYPE_ANY ) ) {
        return 0;
    }
    // Load the json filename path.
    FS_LoadFile( jsonPath, (void **)&jsonBuffer );

    // Error out if we can't find it.
    if ( !jsonBuffer ) {
        #ifdef _DEBUG_MAT_PRINT_JSONWAL_COULDNOTREAD
        Com_LPrintf( PRINT_DEVELOPER, "%s: Couldn't read %s\n", __func__, jsonPath );
        #endif
        return 0;
    }

    // Initialize JSON parser.
    jsmn_parser parser;
    jsmn_init( &parser );

    // Parse JSON into tokens. ( We aren't expecting more than 128 tokens, can be increased if needed though. )
    jsmntok_t tokens[ 128 ]; 
    const int32_t numTokensParsed = jsmn_parse( &parser, jsonBuffer, strlen( jsonBuffer ), tokens,
        sizeof( tokens ) / sizeof( tokens[ 0 ] ) );

    // If lesser than 0 we failed to parse the json properly.
    if ( numTokensParsed < 0 ) {
        if ( numTokensParsed == JSMN_ERROR_INVAL ) {
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(JSMN_ERROR_INVAL), bad token, JSON string is corrupted\n", __func__, jsonPath );
        } else if ( numTokensParsed == JSMN_ERROR_NOMEM ) {
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(JSMN_ERROR_INVAL), not enough tokens, JSON string is too large\n", __func__, jsonPath );
        } else if ( numTokensParsed == JSMN_ERROR_PART ) {
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(JSMN_ERROR_PART),  JSON string is too short, expecting more JSON data\n", __func__, jsonPath );
        } else {
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(unknown)\n", __func__, jsonPath );
        }
        // Clear the jsonbuffer buffer.
        Z_Free( jsonBuffer );
        return 0;
    }

    // Assume the top-level element is an object.
    if ( numTokensParsed < 1 || tokens[ 0 ].type != JSMN_OBJECT ) {
        Com_LPrintf( PRINT_DEVELOPER, "%s: Expected a json Object at the root of file '%s'!\n", __func__, jsonPath );
        // Clear the jsonbuffer buffer.
        Z_Free( jsonBuffer );
        return 0;
    }

    // Allocate new materialID instance.
    const int32_t materialID = ( cm->num_materials++ ) + 1;

    // Get materialID pointer.
    cm_material_t *material = &cm->materials[ materialID ];
    // Initialize defaults.
    material->materialID = materialID;
    // Copy over the texture as being the material name.
    memset( material->name, 0, MAX_MATERIAL_NAME );
    Q_strlcpy( material->name, name, MAX_MATERIAL_NAME );

    // Copy over defaulting physical properties.
    material->physical = cm_default_material.physical;

    // Iterate over json tokens.
    for ( int32_t tokenID = 1; tokenID < numTokensParsed; tokenID++ ) {
        if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "physical_friction" ) == 1 ) {
            // Value
            char fieldValue[ MAX_QPATH ] = { };
            // Fetch field value string size.
            const int32_t size = constclamp( tokens[ tokenID + 1 ].end - tokens[ tokenID + 1 ].start, 0, MAX_QPATH );
            // Parse field value into buffer.
            Q_snprintf( fieldValue, size, jsonBuffer + tokens[ tokenID + 1 ].start );

            // Try and convert it to a float for our material.
            material->physical.friction = atof( fieldValue );
        } else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "physical_material_kind" ) == 1 ) {
            // Value
            char fieldValue[ MAX_QPATH ] = { };
            // Fetch field value string size.
            const int32_t size = tokens[ tokenID + 1 ].end - tokens[ tokenID + 1 ].start;// constclamp( , 0, MAX_QPATH );
            // Parse field value into buffer.
            Q_snprintf( fieldValue, size + 1, jsonBuffer + tokens[ tokenID + 1 ].start );

            // Copy it over into our material kind string buffer.
            memset( material->physical.kind, 0, sizeof( material->physical.kind )/*MAX_QPATH*/ );
            Q_strlcpy( material->physical.kind, fieldValue, sizeof( material->physical.kind )/*MAX_QPATH*/ );//jsonBuffer + tokens[tokenID].start, size);
        }
    }
    
    #ifdef _DEBUG_MAT_SUCCESSFUL
    // Debug print:
    Com_LPrintf( PRINT_DEVELOPER, "%s: Inserted new material[materialID(#%d), name(\"%s\"), kind(%s), friction(%f)]\n",
        __func__, material->materialID, material->name, material->physical.kind, material->physical.friction );
    #endif

    // Clear the jsonbuffer buffer.
    Z_Free( jsonBuffer );

    // Return incremented num_materials as its ID.
    return materialID;
}

/**
*   @brief  Iterate all BSP texinfos and loads in their matching corresponding material file equivelants
*           into the 'collision model' data.
*
*   @return The number of materials loaded for the BSP's texinfos. In case of any errors
*           it'll return 0 instead.
**/
const int32_t CM_LoadMaterials( cm_t *cm ) {
    // We need the BSP to be loaded.
    if ( !cm->cache ) {
        return 0;
    }

    // Get BSP Ptr.
    bsp_t *bsp = cm->cache;

    // Allocate for the collision model, an array the size equal to numtexinfo's with an additional slot for the nulltexinfo..
    const uint32_t material_allocation_count = ( cm->cache->numtexinfo + 1 );
    cm->materials = static_cast<cm_material_t *>( Z_TagMallocz( sizeof( cm->materials[ 0 ] ) * material_allocation_count, TAG_CMODEL ) );    // Memset them.
    memset( cm->materials, 0, sizeof( cm->materials[ 0 ] ) * material_allocation_count);

    // MaterialID(#0) is intended for nulltexinfo and as the 'base defaults' for all other materials.
    const uint32_t default_material_id = cm->num_materials;
    cm->materials[ default_material_id ] = cm_default_material;
    nulltexinfo.c.material = &cm->materials[ default_material_id ];
    nulltexinfo.c.materialID = default_material_id;
    // Increase material count.
    cm->num_materials++;

    // Iterate over the bsp's texinfos, keeping score of its ID for use with materials.
    mtexinfo_t *texinfo = bsp->texinfo;
    // Start off at index 1, since index 0 is used for nulltexinfo.
    for ( uint32_t texInfoID = 1; texInfoID <= bsp->numtexinfo; texInfoID++, texinfo++ ) {
        // Make sure to assign the texinfoID properly.
        texinfo->texInfoID = texInfoID;

        // See if a material matching the texinfo's texture name already exists, if so, acquire its ID.
        int32_t materialID = CM_MaterialExists( cm, texinfo->name );

        // The material is nonexistent.
        if ( materialID == -1 ) {
            // Path to the json file.
            char jsonFilePath[ MAX_OSPATH ] = { };
            // Generate the game folder relative path to the texinfo->name texture '.json_wal' file.
            Q_snprintf( jsonFilePath, MAX_OSPATH, "textures/%s.wal_json", texinfo->name );

            // Attempt to load a material from the '.wal_json' that matches the texture name.
            materialID = CM_LoadMaterialFromJSON( cm, texinfo->name, jsonFilePath );

            // It was non existent, or at least something went wrong loading the json for the material.
            // The materialID will thus be matching to that of the 'default' material type instead.
            if ( materialID == 0 ) {
                // Apply default material to surface.
                texinfo->c.materialID = 0;
                texinfo->c.material = &cm->materials[ 0 ];
                // Iterate to the next.
                continue;
            }
        }

        // Apply the final resulting materialID and pointer to the material into
        // the texinfo's csurface_t.
        texinfo->c.materialID = materialID;
        texinfo->c.material = &cm->materials[ materialID ];
    }

    // Return the total number of materials loaded.
    return cm->num_materials;
}