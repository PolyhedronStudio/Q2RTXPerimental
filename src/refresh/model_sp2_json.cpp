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
    dsp2frame_t *src_frame = nullptr;

    char buffer[ SPJ_MAX_FRAMENAME ] = {};
    int32_t i, ret = Q_ERR_SUCCESS;

    const char *jsonBuffer = (const char *)rawdata;

    //if ( length < sizeof( header ) )
    //    return Q_ERR_FILE_TOO_SMALL;
    if ( length < 2 || !rawdata ) { // {} == 2.. Still useless so.
		return Q_ERR_FILE_TOO_SMALL;
    }

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

    // Try and read the sprite its properties.
    try {
        // Get the version number.
        if ( json.contains( "version" ) ) {
			header.version = json[ "version" ].get< int32_t >();
            if ( header.version != SPJ_VERSION ) {
                ret = Q_ERR_INVALID_FORMAT;
            }
        }
		// Get the frame count.
        if ( json.contains( "frameCount" ) ) {
            header.numframes = json[ "frameCount" ].get< int32_t >();
        } else {
            // Notify about the situation.
            ret = Q_ERR_TOO_FEW;
        }
        
        // Get the frames array.
        if ( json.contains( "frames" ) && json.at( "frames" ).is_array() ) {
            // Get frames array.
			auto framesArray = json.at( "frames" );
            // Iterate over array of frame objects.
			for ( const auto &frame : framesArray ) {
				// Get the frame its name.
				if ( frame.contains( "name" ) ) {
                    // The frame to push back to src frames.
					dsp2frame_t pendingFrame = {};

                    // Get the frame its name.
					const std::string frameName = frame[ "name" ].get< const std::string >();
					// Get the frame its width.
                    pendingFrame.width = frame[ "width" ].get< uint32_t >();
					// Get the frame its height.
                    pendingFrame.height = frame[ "height" ].get< uint32_t >();
					// Get the frame its origin x.
                    pendingFrame.origin_x = frame[ "origin_x" ].get< uint32_t >();
					// Get the frame its origin y.
					pendingFrame.origin_y = frame[ "origin_y" ].get< uint32_t >();
					

                    // Copy the frame name into the buffer.
					Q_strlcpy( pendingFrame.name, frameName.c_str(), sizeof( pendingFrame.name ) );

					// Add the frame to the list.
                    src_frames.push_back( pendingFrame );
				}
			}
        }
    }
    // Catch any json parsing errors.
    catch ( const nlohmann::json::exception &e ) {
        #ifdef _DEBUG_SPJ_PRINT_FAILURES
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

    //if ( header.ident != SP2_IDENT )
    //    return Q_ERR_UNKNOWN_FORMAT;
    //if ( header.version != SP2_VERSION )
    //    return Q_ERR_UNKNOWN_FORMAT;
    header.ident = SP2_IDENT;
    if ( header.numframes < 1 ) {
        // empty models draw nothing
        model->type = model_s::MOD_EMPTY;
        return Q_ERR_TOO_FEW;
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
        src_frame = &src_frames[ i ];
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