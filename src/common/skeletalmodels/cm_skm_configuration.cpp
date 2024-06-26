/********************************************************************
*
*
*	Skeletal Model Configuration(.skc) File Load and Parser.
*
*	Used to load up the specified actions, blends, and events.
*
********************************************************************/
#include "shared/shared.h"
#include "shared/util_list.h"

#include "refresh/shared_types.h"

#include "common/cmd.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/files.h"
#include "common/math.h"
#include "common/skeletalmodels/cm_skm_configuration.h"

#include "system/hunk.h"

// Jasmine json parser.
#define JSMN_STATIC
#include "shared/jsmn.h"


//!
//! Shamelessly taken from refresh/models.h
//! 
#ifndef MOD_Malloc
#define MOD_Malloc(size)    Hunk_TryAlloc(&model->hunk, size)
#endif
#ifndef CHECK
#define CHECK(x)    if (!(x)) { ret = Q_ERR(ENOMEM); goto fail; }
#endif



//! Uncomment to enable failure to load developer output.
#define _DEBUG_PRINT_SKM_CONFIGURATION_LOAD_FAILURE

//! Uncomment to enable debug developer output for failing to parse the configuration file.
#define _DEBUG_PRINT_SKM_CONFIGURATION_PARSE_FAILURE

//! Uncomment to enable debug developer output of the (depth indented-) bone tree structure.
#define _DEBUG_PRINT_SKM_BONE_TREE_STRUCTURE

/**
*
*
*	Skeletal Model Configuration Loading:
*
*
**/
/**
*	@brief	Will load up the skeletal model configuration file, usually, 'tris.skc'.
*			It does so by allocating the outputBuffer which needs to be Z_Freed by hand.
*	@return	True on success, false on failure.
**/
char *CM_SKM_LoadConfigurationFile( model_t *model, const char *configurationFilePath, int32_t *loadResult ) {
    char *fileBuffer = nullptr;

    // Ensure the file is existent.
    if ( !FS_FileExistsEx( configurationFilePath, FS_TYPE_ANY ) ) {
        *loadResult = false;
        return nullptr;
    }
    // Load the json filename path.
    FS_LoadFile( configurationFilePath, (void **)&fileBuffer );

    // Error out if we can't find it.
    if ( !fileBuffer ) {
        #ifdef _DEBUG_PRINT_SKM_CONFIGURATION_LOAD_FAILURE
        Com_LPrintf( PRINT_DEVELOPER, "%s: Couldn't read %s\n", __func__, configurationFilePath );
        #endif
        *loadResult = false;
        return fileBuffer;
    }

    // All is well, we got our buffer.
    *loadResult = true;

    return fileBuffer;
}



/**
*
*
*	JSMN JSON Utility Functions: Token Type Tests (TODO: Move elsewhere.)
*
*
**/
/**
*   @return True if the token is a 'primitive' type.
**/
static inline const int32_t json_token_type_is_primitive( jsmntok_t *token ) {
    return ( token != nullptr ? token->type == JSMN_PRIMITIVE ? true : false : -1 );
}
/**
*   @return True if the token is an 'object' type.
**/
static inline const int32_t json_token_type_is_object( jsmntok_t *token ) {
    return ( token != nullptr ? token->type == JSMN_OBJECT ? true : false : -1 );
}
/**
*   @return True if the token is an 'array' type.
**/
static inline const int32_t json_token_type_is_array( jsmntok_t *token ) {
    return ( token != nullptr ? token->type == JSMN_ARRAY ? true : false : -1 );
}
/**
*   @return True if the token is a 'string' type.
**/
static inline const int32_t json_token_type_is_string( jsmntok_t *token ) {
    return ( token != nullptr ? token->type == JSMN_STRING ? true : false : -1 );
}



/**
*
*
*	JSMN JSON Utility Functions: Value Acquisition/Comparison/Conversion (TODO: Move elsewhere.) 
*
*
**/
/**
*   @return True if the token is JSMN_STRING and the token string value matches that of *s.
*   @brief  When 'enforce_string_type' is set to false, it won't require the token to be a JSMN_STRING type.
**/
static const int32_t json_token_value_strcmp( const char *jsonBuffer, jsmntok_t *token, const char *str, const bool enforce_string_type = true ) {
    // Need a valid token ptr.
    if ( token == nullptr ) {
        // Failure.
        return -1;
    }
    // Ensure the token type is a string, that its length as well as its contents matches that of *s.
    if ( ( enforce_string_type ? json_token_type_is_string( token ) == 1 : true )
        && (int)strlen( str ) == token->end - token->start
        && strncmp( jsonBuffer + token->start, str, token->end - token->start ) == 0 ) {
        return 1;
    }
    // Failure.
    return -1;
}

/**
*   @brief  Will copy the JSON token its string value into the designated buffer.
*   @note   When it is an actual string token type, the first and last double quote(") are removed.
*   @return The amount of characters copied over. (This is always < outputBufferLength)
**/
static const int32_t json_token_value_to_buffer( const char *jsonBuffer, jsmntok_t *token, char *outputBuffer, const uint32_t outputBufferLength ) {
    // Token string/value size.
    const int32_t tokenValueSize = constclamp( token->end - token->start, 0, outputBufferLength );

    //// Parse field value into buffer.
    //Q_snprintf( fieldValue, tokenValueSize, jsonBuffer + token.start );
    return Q_scnprintf( outputBuffer, tokenValueSize, "%s\0", jsonBuffer + token->start );
}

/**
*
*
*	JSMN JSON Utility Functions: Primitive Type Tests (TODO: Move elsewhere.)
*
*
**/
/**
*   @return The primitive type of the json token.
**/
typedef enum json_primitive_type_s {
    //! For 'true' and 'false'.
    JSON_PRIMITIVE_TYPE_BOOLEAN,
    //! For js 'null'.
    JSON_PRIMITIVE_TYPE_NULL,
    //! For (signed)-integers and floats. 
    JSON_PRIMITIVE_TYPE_NUMERIC,
    
    //! For unknown primitive types.
    JSON_PRIMITIVE_TYPE_UNKNOWN,
    //! For when the token is not even a primitive.
    JSON_PRIMITIVE_IS_NO_PRIMITIVE,
    //! FOr when the token is a (nullptr).
    JSON_PRIMITIVE_TOKEN_IS_NULLPTR
} json_primitive_type_t;
/**
*   @return The actual type of the primitive token. On failure, either type unknown or 'is no primitive'.
**/
static inline const json_primitive_type_t json_token_primitive_type( const char *jsonBuffer, jsmntok_t *token ) {
    // Failure if token is a nullptr.
    if ( !token ) {
        return JSON_PRIMITIVE_TOKEN_IS_NULLPTR;
    }
    // Failure if token is not an actual primitive.
    if ( !json_token_type_is_primitive( token ) ) {
        return JSON_PRIMITIVE_IS_NO_PRIMITIVE;
    }
    // See whether it is numeric.
    const char *firstChar = jsonBuffer + token->start;
    if ( Q_isnumeric( *firstChar ) ) {
        return JSON_PRIMITIVE_TYPE_NUMERIC;
    }
    // See whether it is a 'boolean' true/false.
    if ( json_token_value_strcmp( jsonBuffer, token, "true", false ) || json_token_value_strcmp( jsonBuffer, token, "false", false ) ) {
        return JSON_PRIMITIVE_TYPE_BOOLEAN;
    }
    // See whether it is a 'null'
    if ( json_token_value_strcmp( jsonBuffer, token, "null", false ) ) {
        return JSON_PRIMITIVE_TYPE_NULL;
    }
    // Type unknown.
    return JSON_PRIMITIVE_TYPE_UNKNOWN;
}



/**
*
*
*	Skeletal Model Configuration Parsing:
*
*
*
**/
///**
//*   @brief  Will parse the "Animations" JSON object.
//**/
//static int32_t CM_SKM_ParseJSON_ActionsObject( const char *fileBuffer, jsmntok_t *tokens, const int32_t numTokens, const int32_t tokenID ) {
//    std::string actionsRootTokenType = ( tokens[ tokenID ].type == JSMN_ARRAY ? "ARRAY" : "DIFF" );
//    // Debug: 
//    Com_LPrintf( PRINT_DEVELOPER, "%s: ------- Actions Object[tokenType=%s numTokens=%i, tokenID=%i][\n", __func__, actionsRootTokenType.c_str(), numTokens, tokenID );
//
//    // We skip sanity checks since we're only calling this function from CM_SKM_ParseConfigurationBuffer.
//    // Begin iterating at tokenID 1, since tokenID 0 is our 'Root' JSON Object.
//    int32_t subTokenID = tokenID;
//    for ( subTokenID; subTokenID < tokenID + numTokens; subTokenID++ ) {
//        // The actual previous token.
//        const uint32_t previousTokenID = subTokenID - 1;
//        jsmntok_t *previousToken = &tokens[ previousTokenID ];
//        // The current token.
//        jsmntok_t *currentToken = &tokens[ subTokenID ];
//        // The next token.
//        const uint32_t nextTokenID = subTokenID + 1;
//        jsmntok_t *nextToken = ( nextTokenID < ( numTokens - 1 ) ? &tokens[ nextTokenID ] : nullptr );
//
//        // Start 
//        std::string tokenTypeStr = "UNKNOWN";
//        if ( currentToken->type == JSMN_ARRAY ) {
//            tokenTypeStr = "Array";
//        } else if ( currentToken->type == JSMN_OBJECT ) {
//            tokenTypeStr = "Object";
//        } else if ( currentToken->type == JSMN_STRING ) {
//            tokenTypeStr = "String";
//        } else if ( currentToken->type == JSMN_PRIMITIVE ) {
//            tokenTypeStr = "Primitive";
//        }
//        Com_LPrintf( PRINT_DEVELOPER, "[tokenType=%s tokenID=%i]\n", tokenTypeStr.c_str(), subTokenID );
//    }
//    Com_LPrintf( PRINT_DEVELOPER, "]----------------------------------\n" );
//
//    return tokenID + numTokens;
//}
///**
//*   @brief  Will parse the "RootMotion" JSON object.
//**/
//static int32_t CM_SKM_ParseJSON_RootMotionObject( const char *fileBuffer, jsmntok_t *tokens, const int32_t numTokens, const int32_t tokenID ) {
//
//    return 0;
//}

/**
*   @brief  Will iterate over the IQM joints and arrange a more user friendly arranged 
*           data copy for the SKM config.
**/
static const int32_t CM_SKM_CollectAndFillBonesArray( const model_t *model, const char *configurationFilePath ) {
    // Sanity checks are skipped since this is only called by CM_SKM_ParseConfigurationBuffer.


    // IQM Data Pointer.
    iqm_model_t *iqmData = model->iqmData;
    // SKM Config Pointer.
    skm_config_t *skmConfig = model->skmConfig;
    

    // Pointer to joint names buffer.
    char *str = iqmData->jointNames;
    // Store the number of bones.
    skmConfig->numberOfBones = iqmData->num_joints;

    // Iterate the IQM joints data.
    for ( uint32_t joint_idx = 0; joint_idx < iqmData->num_joints; joint_idx++ ) {
        // The matching bone node.
        skm_config_bone_node_t *boneNode = &skmConfig->boneArray[ joint_idx ];

        // Copy over the name.
        char *name = str;//(const char *)iqmData->jointNames[ joint_idx ];// +header->ofs_text + joint->name;
        size_t len = strlen( name ) + 1;
        // If the name is empty however..
        if ( !name || (name && *name == '\0' ) ) {
            continue;
        }
        if ( len >= MAX_QPATH ) { len = MAX_QPATH - 1; }
        memcpy( boneNode->name, name, len );
        // Move to next bone name.
        str += len;

        // Store the Bone Number and Parent Bone Number.
        boneNode->number = joint_idx;
        boneNode->parentNumber = iqmData->jointParents[ joint_idx ];

        // Set pointer to the parent bone. (Unless we're the root node.)
        //boneNode->parentBone = ( joint_idx == 0 ? nullptr : &skmConfig->boneArray[ boneNode->parentNumber ] );
        boneNode->parentBone = ( boneNode->parentNumber == -1 ? nullptr : &skmConfig->boneArray[ boneNode->parentNumber ] );
        // If we are the root node, setup the boneTree node pointer.
        if ( boneNode->parentNumber == -1 ) {
            skmConfig->boneTree = boneNode;
        }
    
        // Increment the number of child nodes of the node matching parentNumber.
        // This is done here so we already know how much space to allocate when creating the bone tree.
        if ( boneNode->parentNumber == -1 ) {
            skmConfig->boneTree->numberOfChildNodes++;
        } else {
            skmConfig->boneArray[ boneNode->parentNumber ].numberOfChildNodes++;
        }
    }

    return 1;
}

/**
*   @brief  For debugging reasons, iterates the bone its child nodes.
**/
#ifdef _DEBUG_PRINT_SKM_BONE_TREE_STRUCTURE
static void CM_SKM_Debug_PrintNode( const skm_config_bone_node_t *boneNode, const int32_t treeDepth ) {
    if ( !boneNode ) {
        return;
    }

    // Generate the amount of spaces to indent based on treeDepth.
    std::string depthComponent = "    ";
    std::string depthString = "";
    for ( int32_t i = 0; i < treeDepth; i++ ) {
        depthString += depthComponent;
    }

    // Generate the actual string we want to print.
    Com_LPrintf( PRINT_DEVELOPER, " %s [Bone=\"%s\", Number=%i, parentNumber=%i, numberOfChildNodes=%i]%s\n",
        depthString.c_str(), boneNode->name, boneNode->number, boneNode->parentNumber, boneNode->numberOfChildNodes, ( boneNode->numberOfChildNodes > 0 ? " ->:" : "" ) );

    if ( boneNode->numberOfChildNodes ) {
        for ( int32_t i = 0; i < boneNode->numberOfChildNodes; i++ ) {
            skm_config_bone_node_t *childNode = boneNode->childBones[ i ];
            if ( childNode != nullptr ) {
                CM_SKM_Debug_PrintNode( childNode, treeDepth + 1 );
            }
        }
    }
}
#endif

/**
*   @brief  Iterates over the SKM bones array, if the array node its parent number matches to
*           that of boneNode, add it to the boneNode's childBones array and recursively call
*           this function again to iterate over the array node.
**/
static void CM_SKM_CollectBoneChildren( model_t *model, skm_config_bone_node_t *boneNode ) {
    // IQM Data Pointer.
    iqm_model_t *iqmData = model->iqmData;
    // SKM Config Pointer.
    skm_config_t *skmConfig = model->skmConfig;

    // Need a valid pointer to work with, and it could be a nullptr.
    if ( !boneNode ) {
        return;
    }

    // Iterate over the amount of child bones it has.
    int32_t childrenAdded = 0;
    for ( int32_t j = 0; j < skmConfig->numberOfBones; j++ ) {
        // Get array bone node.
        skm_config_bone_node_t *arrayNode = &skmConfig->boneArray[ j ];

        // If its a valid number and a matching parent number.
        if ( boneNode->number >= 0 && arrayNode && arrayNode->parentNumber == boneNode->number ) {
            // Store the pointer to the node as a childBone.
            boneNode->childBones[ childrenAdded ] = arrayNode;
            // Increment children added for the current boneNode.
            childrenAdded++;

            // Skip it if it has zero child bones.
            if ( !arrayNode || arrayNode->name[ 0 ] == '\0' || arrayNode->numberOfChildNodes == 0 ) {
                continue;
            }

            // Recurse into the arrayNode to collect its children nodes.
            CM_SKM_CollectBoneChildren( model, arrayNode );
        }
    }
}
/**
*   @brief  Iterates over the SKM config bones array in order to generate the boneTree.
**/
static const int32_t CM_SKM_CreateBoneTree( model_t *model, const char *configurationFilePath ) {
    // For CHECK( )
    int ret = 0;
    // Sanity checks are skipped since this is only called by CM_SKM_ParseConfigurationBuffer.


    // IQM Data Pointer.
    iqm_model_t *iqmData = model->iqmData;
    // SKM Config Pointer.
    skm_config_t *skmConfig = model->skmConfig;


    // By now we've collected the number of child nodes for each SKM bone in the array.
    // Get pointer to the root node.
    skm_config_bone_node_t *boneTreeRootNode = skmConfig->boneTree;
    
    // Declared here since, label usage...
    int32_t numberOfBonesFilled = 0;

    // Iterate along the bones, allocate space for child bones..
    for ( int32_t i = 0; i < skmConfig->numberOfBones; i++ ) {
        // Acquire pointer to the bone node.
        skm_config_bone_node_t *boneNode = &skmConfig->boneArray[ i ];

        // Skip it if it has zero child bones.
        if ( !boneNode || boneNode->name[ 0 ] == '\0' || boneNode->numberOfChildNodes == 0 ) {
            continue;
        }

        // Allocate space for the child bones.
        CHECK( boneNode->childBones = (skm_config_bone_node_t **)( MOD_Malloc( sizeof( skm_config_bone_node_t* ) * boneNode->numberOfChildNodes ) ) );
        // Ensure it is zeroed out.
        memset( boneNode->childBones, 0, sizeof( skm_config_bone_node_t*) * boneNode->numberOfChildNodes );
    }

    // Iterate along the bones once more now that memory is allocated.
    // Collect and fill the matching childBones array for each bone recursively.
    for ( int32_t i = 0; i < skmConfig->numberOfBones; i++ ) {
        // Acquire pointer to the bone node.
        skm_config_bone_node_t *boneNode = &skmConfig->boneArray[ i ];

        // Skip it if it has zero child bones.
        if ( !boneNode || boneNode->name[0] == '\0' || boneNode->numberOfChildNodes == 0 ) {
            continue;
        }

        // Generate the bone tree.
        CM_SKM_CollectBoneChildren( model, boneNode );
    }

#ifdef _DEBUG_PRINT_SKM_BONE_TREE_STRUCTURE
    // Debug print.
    Com_LPrintf( PRINT_DEVELOPER, " ------------------------- BoneTree: \"%s\" ----------------------- \n", configurationFilePath );
    CM_SKM_Debug_PrintNode( boneTreeRootNode, 0 );
    // Debug print.
    Com_LPrintf( PRINT_DEVELOPER, " ------------------------------------------------------------------ \n" );
#endif

    return 1;

fail:
    return 0;
}

/**
*	@brief	Parses the buffer, allocates specified memory in the model_t struct and fills it up with the results.
*	@return	True on success, false on failure.
**/
const int32_t CM_SKM_ParseConfigurationBuffer( model_t *model, const char *configurationFilePath, char *fileBuffer ) {
    // Used for the CHECK( ) macro.
    int ret = 0;

    // WID: TODO: REMOVE AFTER FINISHING THIS.
    //
    // This sits here for debugging purposes only, so we can place breakpoints without
    // them triggering for each model it attemps to load a config for.
    if ( strcmp( configurationFilePath, "models/characters/mixadummy/tris.skc" ) ) {
        return true;
    }

    /**
    *   Sanity Checks:
    **/
    // Ensure the model ptr is valid.
    if ( !model ) {
        #ifdef _DEBUG_PRINT_SKM_CONFIGURATION_PARSE_FAILURE
        Com_LPrintf( PRINT_DEVELOPER, "%s: model_t *model == nullptr! while trying to parse '%s', \n", __func__, configurationFilePath );
        #endif
        return false;
    }
    // Ensure that there is valid iqmData to work with.
    if ( !model->iqmData ) {
        #ifdef _DEBUG_PRINT_SKM_CONFIGURATION_PARSE_FAILURE
        Com_LPrintf( PRINT_DEVELOPER, "%s: model->iqmData == nullptr! while trying to parse '%s', \n", __func__, configurationFilePath );
        #endif
        return false;
    }
    // Ensure the input buffer is valid.
    if ( !fileBuffer ) {
        #ifdef _DEBUG_PRINT_SKM_CONFIGURATION_PARSE_FAILURE
        Com_LPrintf( PRINT_DEVELOPER, "%s: char **fileBuffer == nullptr! while trying to parse '%s'.\n", __func__, configurationFilePath );
        #endif
        return false;
    }

    
    /**
    *   JSON Buffer Tokenizing.
    **/
    // Initialize JSON parser.
    jsmn_parser parser;
    jsmn_init( &parser );

    // Parse tokens.
    constexpr int32_t MaxJSONTokens = 1024;
    jsmntok_t tokens[ MaxJSONTokens ];
    int32_t numTokensParsed = jsmn_parse(
        &parser, fileBuffer, strlen( fileBuffer ), tokens, MaxJSONTokens//( sizeof( tokens ) / sizeof( tokens[ 0 ] )
    );
// WID: TODO: Dynamically allocating somehow fails.
#if 0
    // Analyze parse to determine how much space to allocate for all tokens.
    jsmntok_t *tokens = nullptr;
    int32_t numTokensParsed = jsmn_parse(
        &parser, fileBuffer, strlen( fileBuffer ), tokens, 4096 / sizeof( jsmntok_t ) );//( sizeof( tokens ) / sizeof( tokens[ 0 ] )
    );
#endif
    // If lesser than 0 we failed to parse the json properly.
    if ( numTokensParsed < 0 ) {
        if ( numTokensParsed == JSMN_ERROR_INVAL ) {
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(JSMN_ERROR_INVAL), bad token, JSON string is corrupted\n", __func__, configurationFilePath );
        } else if ( numTokensParsed == JSMN_ERROR_NOMEM ) {
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(JSMN_ERROR_INVAL), not enough tokens, JSON string is too large\n", __func__, configurationFilePath );
        } else if ( numTokensParsed == JSMN_ERROR_PART ) {
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(JSMN_ERROR_PART),  JSON string is too short, expecting more JSON data\n", __func__, configurationFilePath );
        } else {
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(unknown)\n", __func__, configurationFilePath );
        }
        return false;
    }
// WID: TODO: Dynamically allocating somehow fails.
#if 0
// Allocate enough space for parsing JSON tokens.
tokens = static_cast<jsmntok_t *> ( Z_Malloc( sizeof( jsmntok_t ) * 4096 ) );
// Parse JSON into tokens.
numTokensParsed = jsmn_parse(
    &parser, fileBuffer, strlen( fileBuffer ), tokens, numTokensParsed );
#endif
    // Error out if the top level element is not an object.
    if ( numTokensParsed < 1 || tokens[ 0 ].type != JSMN_OBJECT ) {
        Com_LPrintf( PRINT_DEVELOPER, "%s: Expected a json Object at the root of file '%s'!\n", __func__, configurationFilePath );
        return false;
    }

    /**
    *   All went well so far, so now we can start allocating things.
    **/
    int32_t boneArrayResult = 0;
    int32_t boneTreeResult = 0;
    // First of all, allocate our config object in general.
    CHECK( model->skmConfig = static_cast<skm_config_t*>( MOD_Malloc( sizeof( skm_config_t ) ) ) );
    // Collect and fill the bones array.
    boneArrayResult = CM_SKM_CollectAndFillBonesArray( model, configurationFilePath );
    // Build a proper boneTree based on the bone array data.
    boneTreeResult = CM_SKM_CreateBoneTree( model, configurationFilePath );

    // Debug print.
    Com_LPrintf( PRINT_DEVELOPER, " ------------------------- JSON SKC Parsing: \"%s\" ----------------------- \n", configurationFilePath );

    /**
    *   JSON Token Iterating:
    **/
    // Begin iterating at tokenID 1, since tokenID 0 is our 'Root' JSON Object.
    for ( int32_t tokenID = 1; tokenID < numTokensParsed; tokenID++ ) {
        // The actual previous token.
        const uint32_t previousTokenID = tokenID - 1;
        jsmntok_t *previousToken = &tokens[ previousTokenID ];
        // The current token.
        jsmntok_t *currentToken = &tokens[ tokenID ];
        // The next token.
        const uint32_t nextTokenID = tokenID + 1;
        jsmntok_t *nextToken = ( nextTokenID < ( numTokensParsed - 1 ) ? &tokens[ nextTokenID ] : nullptr );

        //
        // RootMotion Bone:
        //
        if ( json_token_value_strcmp( fileBuffer, previousToken, "rootmotionbone" ) == 1 ) {
            // Make sure it is an object token. (Using 1 to prevent compiler warning C4805)...)
            if ( json_token_type_is_string( currentToken ) == 1 /*true*/ ) {
                char rootMotionBoneName[ MAX_QPATH ];
                if ( json_token_value_to_buffer( fileBuffer, currentToken, rootMotionBoneName, MAX_QPATH ) ) {
                    Com_LPrintf( PRINT_DEVELOPER, " \"%s\" - RootMotionBone(\"%s\")\n", configurationFilePath, rootMotionBoneName );
                }
                continue;
            }
        }
        //
        // (SKM_BODY_LOWER) Hip Bone:
        //
        if ( json_token_value_strcmp( fileBuffer, previousToken, "hipbone" ) == 1 ) {
            // Make sure it is an object token. (Using 1 to prevent compiler warning C4805)...)
            if ( json_token_type_is_string( currentToken ) == 1 /*true*/ ) {
                char hipBoneName[ MAX_QPATH ];
                if ( json_token_value_to_buffer( fileBuffer, currentToken, hipBoneName, MAX_QPATH ) ) {
                    Com_LPrintf( PRINT_DEVELOPER, " \"%s\" - hipBone(\"%s\")\n", configurationFilePath, hipBoneName );
                }
                continue;
            }
        }
        //
        // (SKM_BODY_UPPER) Torso Bone:
        //
        if ( json_token_value_strcmp( fileBuffer, previousToken, "torsobone" ) == 1 ) {
            // Make sure it is an object token. (Using 1 to prevent compiler warning C4805)...)
            if ( json_token_type_is_string( currentToken ) == 1 /*true*/ ) {
                char torsoBoneName[ MAX_QPATH ];
                if ( json_token_value_to_buffer( fileBuffer, currentToken, torsoBoneName, MAX_QPATH ) ) {
                    Com_LPrintf( PRINT_DEVELOPER, " \"%s\" - torsoBoneName(\"%s\")\n", configurationFilePath, torsoBoneName );
                }
                continue;
            }
        }
        //
        // (SKM_BODY_HEAD) Head Bone:
        //
        if ( json_token_value_strcmp( fileBuffer, previousToken, "headbone" ) == 1 ) {
            // Make sure it is an object token. (Using 1 to prevent compiler warning C4805)...)
            if ( json_token_type_is_string( currentToken ) == 1 /*true*/ ) {
                char headBoneName[ MAX_QPATH ];
                if ( json_token_value_to_buffer( fileBuffer, currentToken, headBoneName, MAX_QPATH ) ) {
                    Com_LPrintf( PRINT_DEVELOPER, " \"%s\" - headBoneName(\"%s\")\n", configurationFilePath, headBoneName );
                }
                continue;
            }
        }
        ////
        ////  RootMotion Configuration Object:
        ////
        //else if ( nextToken && json_token_value_strcmp( fileBuffer, currentToken, "rootmotion" ) == 1 ) {
        //    // Make sure it is an object token. (Using 1 to prevent compiler warning C4805)...)
        //    if ( json_token_type_is_object( nextToken ) == 1 /*true*/ ) {
        //        // Proceed into parsing the JSON for the RootMotion object.
        //        //CM_SKM_ParseJSON_RootMotionObject( fileBuffer, tokens, numTokensParsed, tokenID + 1 );
        //    }
        //    //// Value
        //    //char fieldValue[ MAX_QPATH ] = { };
        //    //// Fetch field value string size.
        //    //const int32_t size = tokens[ tokenID + 1 ].end - tokens[ tokenID + 1 ].start;// constclamp( , 0, MAX_QPATH );
        //    //// Parse field value into buffer.
        //    //Q_snprintf( fieldValue, size + 1, jsonBuffer + tokens[ tokenID + 1 ].start );

        //    //// Copy it over into our material kind string buffer.
        //    //memset( material->physical.kind, 0, sizeof( material->physical.kind )/*MAX_QPATH*/ );
        //    //Q_strlcpy( material->physical.kind, fieldValue, sizeof( material->physical.kind )/*MAX_QPATH*/ );//jsonBuffer + tokens[tokenID].start, size);

        //}
    }
// WID: TODO: Dynamically allocating somehow fails.
#if 0
    // Free tokens.
    Z_Free( tokens );
#endif
    // Debug print.
    Com_LPrintf( PRINT_DEVELOPER, " -------------------------------------------------------------------------- \n", configurationFilePath );
    // Success.
    return true;

// Used by the CHECK( ) macro.
fail:
    // WID: TODO: Dynamically allocating somehow fails.
    #if 0
        // Free tokens.
    Z_Free( tokens );
    #endif
    return false;
}

