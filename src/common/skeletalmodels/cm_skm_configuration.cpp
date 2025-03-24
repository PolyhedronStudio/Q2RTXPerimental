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
#include "common/skeletalmodels/cm_skm.h"
#include "common/skeletalmodels/cm_skm_configuration.h"

#include "system/hunk.h"

// Include nlohman::json library for easy parsing.
#include <nlohmann/json.hpp>


//!
//! Shamelessly taken from refresh/models.h
//! 
#ifndef MOD_Malloc
#define MOD_Malloc(size)    Hunk_TryAlloc(&model->hunk, size)
#endif
#ifndef CHECK
#define CHECK(x)    if (!(x)) { ret = Q_ERR(ENOMEM); goto fail; }
#endif



//! Uncomment to disable failure to load developer output.
#define _DEBUG_PRINT_SKM_CONFIGURATION_LOAD_FAILURE

//! Uncomment to disable debug developer output for failing to parse the configuration file.
#define _DEBUG_PRINT_SKM_CONFIGURATION_PARSE_FAILURE

//! Uncomment to enable debug developer output of the (depth indented-) bone tree structure.
//#define _DEBUG_PRINT_SKM_BONE_TREE_STRUCTURE



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
char *SKM_LoadConfigurationFile( model_t *model, const char *configurationFilePath, int32_t *loadResult ) {
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
        return nullptr;
    }

    // All is well, we got our buffer.
    *loadResult = true;

    return fileBuffer;
}



/**
*
*
*	Skeletal Model Configuration Parsing:
*
*
*
**/
/**
*   @brief  For debugging reasons, iterates the bone its child nodes.
**/
#ifdef _DEBUG_PRINT_SKM_BONE_TREE_STRUCTURE
    static void SKM_Debug_PrintNode( const skm_bone_node_t *boneNode, const int32_t treeDepth ) {
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
                skm_bone_node_t *childNode = boneNode->childBones[ i ];
                if ( childNode != nullptr ) {
                    SKM_Debug_PrintNode( childNode, treeDepth + 1 );
                }
            }
        }
    }
#endif

/**
*   @brief  Will iterate over the IQM joints and arrange a more user friendly arranged 
*           data copy for the SKM config.
**/
static const int32_t SKM_GenerateBonesArray( const model_t *model, const char *configurationFilePath ) {
    // Sanity checks are skipped since this is only called by SKM_ParseConfigurationBuffer.


    // IQM Data Pointer.
    skm_model_t *skmData = model->skmData;
    // SKM Config Pointer.
    skm_config_t *skmConfig = model->skmConfig;
    

    // Pointer to joint names buffer.
    char *str = skmData->jointNames;
    // Store the number of bones.
    skmConfig->numberOfBones = skmData->num_joints;

    // Iterate the IQM joints data.
    for ( uint32_t joint_idx = 0; joint_idx < skmData->num_joints; joint_idx++ ) {
        // The matching bone node.
        skm_bone_node_t *boneNode = &skmConfig->boneArray[ joint_idx ];

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
        boneNode->parentNumber = skmData->jointParents[ joint_idx ];

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
*   @brief  Iterates over the SKM bones array, if the array node its parent number matches to
*           that of boneNode, add it to the boneNode's childBones array and recursively call
*           this function again to iterate over the array node.
**/
static void SKM_CollectBoneTreeNodeChildren( model_t *model, skm_bone_node_t *boneNode ) {
    // IQM Data Pointer.
    skm_model_t *iqmData = model->skmData;
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
        skm_bone_node_t *arrayNode = &skmConfig->boneArray[ j ];

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
            SKM_CollectBoneTreeNodeChildren( model, arrayNode );
        }
    }
}

/**
*   @brief  Iterates over the SKM config bones array in order to generate the boneTree.
**/
static const int32_t SKM_CreateBonesTree( model_t *model, const char *configurationFilePath ) {
    // For CHECK( )
    int ret = 0;
    // Sanity checks are skipped since this is only called by SKM_ParseConfigurationBuffer.


    // IQM Data Pointer.
    skm_model_t *iqmData = model->skmData;
    // SKM Config Pointer.
    skm_config_t *skmConfig = model->skmConfig;


    // By now we've collected the number of child nodes for each SKM bone in the array.
    // Get pointer to the root node.
    skm_bone_node_t *boneTreeRootNode = skmConfig->boneTree;
    
    // Declared here since, label usage...
    int32_t numberOfBonesFilled = 0;

    // Iterate along the bones, allocate space for child bones..
    for ( int32_t i = 0; i < skmConfig->numberOfBones; i++ ) {
        // Acquire pointer to the bone node.
        skm_bone_node_t *boneNode = &skmConfig->boneArray[ i ];

        // Skip it if it has zero child bones.
        if ( !boneNode || boneNode->name[ 0 ] == '\0' || boneNode->numberOfChildNodes == 0 ) {
            continue;
        }

        // Allocate space for the child bones.
        CHECK( boneNode->childBones = (skm_bone_node_t **)( MOD_Malloc( sizeof( skm_bone_node_t* ) * boneNode->numberOfChildNodes ) ) );
        // Ensure it is zeroed out.
        memset( boneNode->childBones, 0, sizeof( skm_bone_node_t*) * boneNode->numberOfChildNodes );
    }

    // Iterate along the bones once more now that memory is allocated.
    // Collect and fill the matching childBones array for each bone recursively.
    for ( int32_t i = 0; i < skmConfig->numberOfBones; i++ ) {
        // Acquire pointer to the bone node.
        skm_bone_node_t *boneNode = &skmConfig->boneArray[ i ];

        // Skip it if it has zero child bones.
        if ( !boneNode || boneNode->name[0] == '\0' || boneNode->numberOfChildNodes == 0 ) {
            continue;
        }

        // Generate the bone tree.
        SKM_CollectBoneTreeNodeChildren( model, boneNode );
    }

#ifdef _DEBUG_PRINT_SKM_BONE_TREE_STRUCTURE
    // Debug print.
    Com_LPrintf( PRINT_DEVELOPER, " ------------------------- BoneTree: \"%s\" ----------------------- \n", configurationFilePath );
    SKM_Debug_PrintNode( boneTreeRootNode, 0 );
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
const int32_t SKM_ParseConfigurationBuffer( model_t *model, const char *configurationFilePath, char *jsonBuffer ) {
    // Used for the CHECK( ) macro.
    int ret = 0;

    int32_t motionBoneNumber = -1;

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
    if ( !model->skmData ) {
        #ifdef _DEBUG_PRINT_SKM_CONFIGURATION_PARSE_FAILURE
            Com_LPrintf( PRINT_DEVELOPER, "%s: model->iqmData == nullptr! while trying to parse '%s', \n", __func__, configurationFilePath );
        #endif
        return false;
    }
    // Ensure the input buffer is valid.
    if ( !jsonBuffer ) {
        #ifdef _DEBUG_PRINT_SKM_CONFIGURATION_PARSE_FAILURE
            Com_LPrintf( PRINT_DEVELOPER, "%s: char **jsonBuffer == nullptr! while trying to parse '%s'.\n", __func__, configurationFilePath );
        #endif
        return false;
    }
    
    // Parse JSON using nlohmann::json.
    nlohmann::json json;
    try {
        json = nlohmann::json::parse( jsonBuffer );
    }
    // Catch parsing errors if any.
    catch ( const nlohmann::json::parse_error &e ) {
        // Output parsing error.
        #ifdef _DEBUG_PRINT_SKM_CONFIGURATION_PARSE_FAILURE
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(%s)\n", __func__, configurationFilePath, e.what());
        #endif
        // Return as we failed to parse the json.
        return false;
    }

	// Try and read the skm configuration properties.
    try {
        /**
        *   All went well so far, so now we can start allocating things.
        **/
        int32_t boneArrayResult = 0;
        int32_t boneTreeResult = 0;

        // First of all, allocate our config object in general.
        CHECK( model->skmConfig = static_cast<skm_config_t *>( MOD_Malloc( sizeof( skm_config_t ) ) ) );
        // Collect and fill the bones array.
        boneArrayResult = SKM_GenerateBonesArray( model, configurationFilePath );
        // Build a proper boneTree based on the bone array data.
        boneTreeResult = SKM_CreateBonesTree( model, configurationFilePath );

        // Debug print.
        Com_LPrintf( PRINT_DEVELOPER, " ------------------------- JSON SKC Parsing: \"%s\" ----------------------- \n", configurationFilePath );

        // Ensure it is nullptr.
        model->skmConfig->rootBones.motion = nullptr;
        if ( json.contains( "rootmotionbone" ) ) {
            // Get root motion bone name.
            const std::string rootMotionBoneName = json["rootmotionbone"].get< const std::string >();
            // Ensure the key its value is not empty.
            if ( !rootMotionBoneName.empty() ) {
                // Get the bone node that has the matching name.
                skm_bone_node_t *rootMotionBoneNode = SKM_GetBoneByName( model, rootMotionBoneName.c_str() );
                // Assign bone node pointer if we found a matching bone.
                if ( rootMotionBoneNode ) {
                    model->skmConfig->rootBones.motion = rootMotionBoneNode;
                    // Debug print.
                    Com_LPrintf( PRINT_DEVELOPER, " \"%s\" - RootMotionBone Set To (#%i, \"%s\")\n", configurationFilePath, rootMotionBoneNode->number, rootMotionBoneName.c_str() );
                }
            }
        }

        // Ensure it is nullptr.
        model->skmConfig->rootBones.head = nullptr;
        if ( json.contains( "headbone" ) ) {
            // Get head bone name.
            const std::string headBoneName = json[ "headbone" ].get< const std::string >();
            // Ensure the key its value is not empty.
            if ( !headBoneName.empty() ) {
                // Get the bone node that has the matching name.
                skm_bone_node_t *headBoneNode = SKM_GetBoneByName( model, headBoneName.c_str() );
                // Assign bone node pointer if we found a matching bone.
                if ( headBoneNode ) {
                    model->skmConfig->rootBones.head = headBoneNode;
                    // Debug print.
                    Com_LPrintf( PRINT_DEVELOPER, " \"%s\" - headBone Set To (#%i, \"%s\")\n", configurationFilePath, headBoneNode->number, headBoneName.c_str() );
                }
            }
        }

        // Ensure it is nullptr.
        model->skmConfig->rootBones.torso = nullptr;
        if ( json.contains( "torsobone" ) ) {
            // Get bone name.
            const std::string torsoBoneName = json[ "torsobone" ].get< const std::string >();
            // Ensure the key its value is not empty.
            if ( !torsoBoneName.empty() ) {
                // Get the bone node that has the matching name.
                skm_bone_node_t *torsoBoneNode = SKM_GetBoneByName( model, torsoBoneName.c_str() );
                // Assign bone node pointer if we found a matching bone.
                if ( torsoBoneNode ) {
                    model->skmConfig->rootBones.torso = torsoBoneNode;
                    // Debug print.
                    Com_LPrintf( PRINT_DEVELOPER, " \"%s\" - torsoBone Set To (#%i, \"%s\")\n", configurationFilePath, torsoBoneNode->number, torsoBoneName.c_str() );
                }
            }
        }

        // Ensure it is nullptr.
        model->skmConfig->rootBones.hip = nullptr;
        if ( json.contains( "hipbone" ) ) {
            // Get bone name.
            const std::string hipsBoneName = json[ "hipbone" ].get< const std::string >();
			// Ensure the key its value is not empty.
            if ( !hipsBoneName.empty() ) {
                // Get the bone node that has the matching name.
                skm_bone_node_t *hipsBoneNode = SKM_GetBoneByName( model, hipsBoneName.c_str() );
                // Assign bone node pointer if we found a matching bone.
                if ( hipsBoneNode ) {
                    model->skmConfig->rootBones.hip = hipsBoneNode;
                    // Debug print.
                    Com_LPrintf( PRINT_DEVELOPER, " \"%s\" - hipsBone Set To (#%i, \"%s\")\n", configurationFilePath, hipsBoneNode->number, hipsBoneName.c_str() );
                }
            }
        }
    }
    // Catch any json parsing errors.
    catch ( const nlohmann::json::exception &e ) {
        // Output parsing error.
        #ifdef _DEBUG_PRINT_SKM_CONFIGURATION_PARSE_FAILURE
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(%s)\n", __func__, configurationFilePath, e.what() );
        #endif
        // Return 0 as we failed to parse the json.
        return false;
    }

    // Debug print.
    #ifdef _DEBUG_PRINT_SKM_ROOTMOTION_DATA
        Com_LPrintf( PRINT_DEVELOPER, " -------------------- RootMotion Bone Tracking: \"%s\" --------------------- \n", configurationFilePath );
    #endif

    // Determine the root motion bone number.
    motionBoneNumber = ( model->skmConfig && model->skmConfig->rootBones.motion ? model->skmConfig->rootBones.motion->number : -1 );
    // If the rootmotion bone number == -1, it means we don't want to generate rootmotion data.
    if ( motionBoneNumber >= 0 ) {
        SKM_GenerateRootMotionSet( model, motionBoneNumber, 0 );
    }

    // Debug print.
    #ifdef _DEBUG_PRINT_SKM_ROOTMOTION_DATA
        Com_LPrintf( PRINT_DEVELOPER, " -------------------------------------------------------------------------- \n", configurationFilePath );
    #endif

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

