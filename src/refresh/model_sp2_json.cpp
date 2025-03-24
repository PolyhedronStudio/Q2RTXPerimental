/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2008 Andrey Nazarov
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

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

#include "shared/shared.h"
#include "shared/util_list.h"
#include "common/common.h"
#include "common/files.h"
#include "system/hunk.h"
#include "format/sp2.h"
#include "refresh/images.h"
#include "refresh/models.h"
#include "../client/cl_client.h"
#include "gl/gl.h"

// Include nlohman::json library for easy parsing.
#include <nlohmann/json.hpp>

//! Enables material debug output.
//#define _DEBUG_SPJ_SUCCESSFUL 0
#define _DEBUG_SPJ_PRINT_FAILURES 1
//#define _DEBUG_SPJ_PRINT_COULDNOTREAD 1

// Extern "C".
QEXTERN_C_OPEN

// Publicly exposed to refresh modules so, extern "C".
int MOD_LoadSP2Json( model_t *model, const void *rawdata, size_t length, const char *mod_name ) {
    dsp2header_t header = {};
    std::vector<dsp2frame_t> src_frames;
    int32_t src_frame_id = 0;
    mspriteframe_t *dst_frame = nullptr;
    char buffer[ SPJ_MAX_FRAMENAME ] = {};
    int32_t i, ret;

    const char *jsonBuffer = (const char *)rawdata;

    //if ( length < sizeof( header ) )
    //    return Q_ERR_FILE_TOO_SMALL;
    if ( length < 2 || !rawdata ) { // {} == 2.. Still useless so.
		return Q_ERR_FILE_TOO_SMALL;
    }

    #if 1
    // Parse JSON using nlohmann::json.
    nlohmann::json json;
    try {
        json = nlohmann::json::parse( jsonBuffer );
    }
    // Catch parsing errors if any.
    catch ( const nlohmann::json::parse_error &e ) {
        // Output parsing error.
        Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(%s)\n", __func__, mod_name, e.what() );
        // Clear the jsonbuffer buffer.
        // Z_Free( jsonBuffer );
        // Return as we failed to parse the json.
        return Q_ERR_FAILURE;
    }

    // Try and read the eax effect properties.
    try {
        //eax_reverb_properties.flDensity = json.at( "density" ).get< float >();
        //eax_reverb_properties.flDiffusion = json.at( "diffusion" ).get< float >();
        //eax_reverb_properties.flGain = json.at( "gain" ).get< float >();
        //eax_reverb_properties.flGainHF = json.at( "gain_hf" ).get< float >();
        //eax_reverb_properties.flGainLF = json.at( "gain_lf" ).get< float >();
        //eax_reverb_properties.flDecayTime = json.at( "decay_time" ).get< float >();
        //eax_reverb_properties.flDecayHFRatio = json.at( "decay_hf_ratio" ).get< float >();
        //eax_reverb_properties.flDecayLFRatio = json.at( "decay_lf_ratio" ).get< float >();
        //eax_reverb_properties.flReflectionsGain = json.at( "reflections_gain" ).get< float >();
        //eax_reverb_properties.flReflectionsDelay = json.at( "reflections_delay" ).get< float >();
        //if ( json.at( "reflections_pan" ).size() >= 3 ) {
        //    eax_reverb_properties.flReflectionsPan[ 0 ] = json.at( "reflections_pan" )[ 0 ].get< float >();
        //    eax_reverb_properties.flReflectionsPan[ 1 ] = json.at( "reflections_pan" )[ 1 ].get< float >();
        //    eax_reverb_properties.flReflectionsPan[ 2 ] = json.at( "reflections_pan" )[ 2 ].get< float >();
        //}
        //eax_reverb_properties.flLateReverbGain = json.at( "late_reverb_gain" ).get< float >();
        //eax_reverb_properties.flLateReverbDelay = json.at( "late_reverb_delay" ).get< float >();
        //if ( json.at( "late_reverb_pan" ).size() >= 3 ) {
        //    eax_reverb_properties.flLateReverbPan[ 0 ] = json.at( "late_reverb_pan" )[ 0 ].get< float >();
        //    eax_reverb_properties.flLateReverbPan[ 1 ] = json.at( "late_reverb_pan" )[ 1 ].get< float >();
        //    eax_reverb_properties.flLateReverbPan[ 2 ] = json.at( "late_reverb_pan" )[ 2 ].get< float >();
        //}
        //eax_reverb_properties.flEchoTime = json.at( "echo_time" ).get< float >();
        //eax_reverb_properties.flEchoDepth = json.at( "echo_depth" ).get< float >();
        //eax_reverb_properties.flModulationTime = json.at( "modulation_time" ).get< float >();
        //eax_reverb_properties.flModulationDepth = json.at( "modulation_depth" ).get< float >();
        //eax_reverb_properties.flAirAbsorptionGainHF = json.at( "air_absorption_gain_hf" ).get< float >();
        //eax_reverb_properties.flHFReference = json.at( "hf_reference" ).get< float >();
        //eax_reverb_properties.flLFReference = json.at( "lf_reference" ).get< float >();
        //eax_reverb_properties.flRoomRolloffFactor = json.at( "room_rolloff_factor" ).get< float >();
        //eax_reverb_properties.iDecayHFLimit = json.at( "decay_hf_limit" ).get< int32_t >();
    }
    // Catch any json parsing errors.
    catch ( const nlohmann::json::exception &e ) {
        #ifdef _DEBUG_EAX_PRINT_JSON_FAILURES
        // Output parsing error.
        Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(%s)\n", __func__, mod_name, e.what() );
        #endif
        // To prevent silly compiler error: warning C4101: 'e': unreferenced local variable
        if ( !e.what() ) { 
			return Q_ERR_FAILURE;
        }
        // Clear the jsonbuffer buffer.
        //Z_Free( jsonBuffer );
        // Return 0 as we failed to parse the json.
        return Q_ERR_FAILURE;
    }

    // Clear the jsonbuffer buffer.
    //Z_Free( jsonBuffer );

    #else
    // Initialize JSON parser.
    jsmn_parser parser;
    jsmn_init( &parser );

    // Parse JSON into tokens. ( We aren't expecting more than 128 tokens, can be increased if needed though. )
    jsmntok_t tokens[ 4096 ];
    const int32_t numTokensParsed = jsmn_parse( &parser, jsonBuffer, strlen( jsonBuffer ), tokens,
        sizeof( tokens ) / sizeof( tokens[ 0 ] ) );

    // If lesser than 0 we failed to parse the json properly.
    if ( numTokensParsed < 0 ) {
        if ( numTokensParsed == JSMN_ERROR_INVAL ) {
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(JSMN_ERROR_INVAL), bad token, JSON string is corrupted\n", __func__, mod_name );
        } else if ( numTokensParsed == JSMN_ERROR_NOMEM ) {
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(JSMN_ERROR_INVAL), not enough tokens, JSON string is too large\n", __func__, mod_name );
        } else if ( numTokensParsed == JSMN_ERROR_PART ) {
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(JSMN_ERROR_PART),  JSON string is too short, expecting more JSON data\n", __func__, mod_name );
        } else {
            Com_LPrintf( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(unknown)\n", __func__, mod_name );
        }
        return Q_ERR_INVALID_FORMAT;
    }

    // Assume the top-level element is an object.
    if ( numTokensParsed < 1 || tokens[ 0 ].type != JSMN_OBJECT ) {
        Com_LPrintf( PRINT_DEVELOPER, "%s: Expected a json Object at the root of file '%s'!\n", __func__, mod_name );
        return Q_ERR_INVALID_FORMAT;
    }

    // Iterate over json tokens.
    for ( int32_t tokenID = 1; tokenID < numTokensParsed; tokenID++ ) {
        if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "version" ) == 0 ) {
			// Aquire token value of version.
            header.version = json_token_to_int32( jsonBuffer, tokens, tokens[ tokenID + 1 ] );
        } else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "frameCount" ) == 0 ) {
            // Aquire token value of numframes.
            header.numframes = json_token_to_int32( jsonBuffer, tokens, tokens[ tokenID + 1 ] );
			
            // Resize the source frame vector to accommodate enough space.
            src_frames.resize( header.numframes );
        } else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "frames" ) == 0 ) {
            // Skip if not an array.
            if ( tokens[ tokenID + 1 ].type != JSMN_ARRAY ) {
                // TODO: Debug print.
                continue;
            }

            // Its size must've already been set.
			if ( header.numframes == 0 ) {
				Com_LPrintf( PRINT_DEVELOPER, "%s: Expected a frameCount value before 'frames' in file '%s'!\n", __func__, mod_name );
				return Q_ERR_TOO_FEW;
			}

            for ( int32_t j = 1; j < tokens[ tokenID + 1 ].size; j++ ) {
                // Skip if not an array.
                if ( tokens[ tokenID +  j ].type != JSMN_OBJECT ) {
                    // TODO: Debug print.
                    continue;
                }

				// Get the array object token.
                jsmntok_t arrayObjectToken = tokens[ tokenID + 2 + j ];
				// Iterate over the object.
                for ( int32_t objectTokenID = 0; objectTokenID < arrayObjectToken.size; objectTokenID++ ) {
                    // Get the object key token.
                    jsmntok_t objectKeyToken = tokens[ tokenID + 2 + j + objectTokenID + 1 ];
                    // Get the object key string.
                    std::string objectKeyStr = json_token_to_str( jsonBuffer, tokens, objectKeyToken );
                    // Get the object value token.
                    jsmntok_t objectValueToken = tokens[ tokenID + 2 + j + objectTokenID + 2 ];
                    // Ensure it is a primitive.
                    if ( !( objectValueToken.type & JSMN_PRIMITIVE ) && !( objectValueToken.type & JSMN_STRING ) ) {
                        // Token string.
                        std::string tokenStr = json_token_to_str( jsonBuffer, tokens, objectValueToken );
                        // Error print.
                        Com_EPrintf( "%s: Expected a primitive value for key '%s' in file '%s'!\n", __func__, tokenStr.c_str(), mod_name );
                        // Move on.
                        continue;
                    }
                    // Get the object value.
                    std::string objectValueStr = json_token_to_str( jsonBuffer, tokens, objectValueToken );

                    // Detect which key property we are dealing with:
                    if ( objectKeyStr == "name" ) {
                        // Token string.
                        std::string tokenStr = json_token_to_str( jsonBuffer, tokens, objectValueToken );
                        // Error print.
                        Com_DPrintf( "%s: Parsed the primitive value for key '%s' in file '%s'!\n", __func__, tokenStr.c_str(), mod_name );
                    }
                    if ( objectKeyStr == "width" ) {
                        // Token string.
                        std::string tokenStr = json_token_to_str( jsonBuffer, tokens, objectValueToken );
                        // Error print.
                        Com_DPrintf( "%s: Parsed the primitive value for key '%s' in file '%s'!\n", __func__, tokenStr.c_str(), mod_name );
                    }
                    if ( objectKeyStr == "height" ) {
                        // Token string.
                        std::string tokenStr = json_token_to_str( jsonBuffer, tokens, objectValueToken );
                        // Error print.
                        Com_DPrintf( "%s: Parsed the primitive value for key '%s' in file '%s'!\n", __func__, tokenStr.c_str(), mod_name );
                    }
                    if ( objectKeyStr == "origin_x" ) {
                        // Token string.
                        std::string tokenStr = json_token_to_str( jsonBuffer, tokens, objectValueToken );
                        // Error print.
                        Com_DPrintf( "%s: Parsed the primitive value for key '%s' in file '%s'!\n", __func__, tokenStr.c_str(), mod_name );
                    }
                    if ( objectKeyStr == "origin_y" ) {
                        // Token string.
                        std::string tokenStr = json_token_to_str( jsonBuffer, tokens, objectValueToken );
                        // Error print.
                        Com_DPrintf( "%s: Parsed the primitive value for key '%s' in file '%s'!\n", __func__, tokenStr.c_str(), mod_name );
                    }
                        //"name": "sprites/expl0/00.tga",
                        //"width" : 1024,
                        //"height" : 1024,
                        //"origin_x" : 0,
                        //"origin_y" : 0
                }

                // Increase token count.
                tokenID += tokens[ tokenID + 1 ].size + 1;

                // Increment the src_frame_id for use with the next frame.
                src_frame_id++;
            }

            // Increase token count.
            tokenID += tokens[ tokenID + 1 ].size + 1;
        }
    }
    #endif

    //if ( header.ident != SP2_IDENT )
    //    return Q_ERR_UNKNOWN_FORMAT;
    //if ( header.version != SP2_VERSION )
    //    return Q_ERR_UNKNOWN_FORMAT;
    if ( header.numframes < 1 ) {
        // empty models draw nothing
        model->type = model_s::MOD_EMPTY;
        return Q_ERR_SUCCESS;
    }
    if ( header.numframes > SP2_MAX_FRAMES ) {
        model->type = model_s::MOD_EMPTY;
        return Q_ERR_TOO_MANY;
    }
    //if ( sizeof( dsp2header_t ) + sizeof( dsp2frame_t ) * header.numframes > length )
    //    return Q_ERR_BAD_EXTENT;

    Hunk_Begin( &model->hunk, sizeof( mspriteframe_t ) * header.numframes );
    model->type = model_s::MOD_SPRITE;

    CHECK( model->spriteframes = (mspriteframe_t*)MOD_Malloc( sizeof( mspriteframe_t ) * header.numframes ) );
    model->numframes = header.numframes;

	// Allocate space for the sprite frames.
    //src_frame = (dsp2frame_t *)( (byte *)rawdata + sizeof( dsp2header_t ) );
    dst_frame = model->spriteframes;
    for ( i = 0; i < header.numframes; i++ ) {
        dsp2frame_t *src_frame = &src_frames[ i ];
		// Get frame texture rectangle dimensions.
        dst_frame->width = (int32_t)LittleLong( src_frame->width );
        dst_frame->height = (int32_t)LittleLong( src_frame->height );
		// Get frame texture rectangle origin.
        dst_frame->origin_x = (int32_t)LittleLong( src_frame->origin_x );
        dst_frame->origin_y = (int32_t)LittleLong( src_frame->origin_y );

        if ( !Q_memccpy( buffer, src_frame->name, 0, sizeof( buffer ) ) ) {
            Com_WPrintf( "%s has bad frame name\n", model->name );
            dst_frame->image = R_NOTEXTURE;
        } else {
            FS_NormalizePath( buffer );
            dst_frame->image = IMG_Find( buffer, IT_SPRITE, IF_SRGB );
        }

        src_frame++;
        dst_frame++;
    }

    Hunk_End( &model->hunk );

    return Q_ERR_SUCCESS;

fail:
    return ret;
}

// Extern "C".
QEXTERN_C_CLOSE