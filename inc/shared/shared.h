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
#pragma once

//
// shared.h -- included first by ALL program modules
//
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/**
*	Include based on whether the unit including is .c or .cpp
**/
#include "shared/stdlibs.h"
//! Include Endianness utilities if not already included by another system header.
#if HAVE_ENDIAN_H
    #include <endian.h>
#endif
//! Include shared platform specifics.
#include "shared/platform.h"
//! Determine and define which endianness we're compiling for.
#if __BYTE_ORDER == __LITTLE_ENDIAN
    #define USE_LITTLE_ENDIAN   1
#elif __BYTE_ORDER == __BIG_ENDIAN
    #define USE_BIG_ENDIAN      1
#endif

//! Include (Quake-)Error Code Definitions:
#include "shared/qerrors.h"
//! Utility to get the count of arrays.
#define q_countof(a) (sizeof(a) / sizeof(a[0]))


/**
*   Typedef certain types based on the translation unit's source language.
**/
#ifdef __cplusplus
    // Byte type remains the same, just included here as well as in the else statement for consistency.
    typedef uint8_t byte;
    // QBoolean support(treat it as an int to remain compatible with the C code parts).
    typedef int32_t qboolean;
    //! Support qtrue for legacy code.
    static constexpr int32_t qtrue = true;
    //! Support qfalse for legacy code.
    static constexpr int32_t qfalse = false;
    // qhandle_t
    typedef int32_t qhandle_t;

	// Include our 'shared_cpp.h' header.
	#include "shared_cpp.h"
#else // __cplusplus
	typedef uint8_t byte;
	typedef enum { qfalse, qtrue } qboolean;    // ABI compat only, don't use: will be int32_t on x86-64 systems.
	typedef int32_t qhandle_t;
	#ifndef NULL
	#define NULL ((void *)0)
	#endif
    // No implementation needed in C translation units.
    #define QENUM_BIT_FLAGS(...)
    #define QEXTERN_C_ENCLOSE(ENCLOSED_CODE) ENCLOSED_CODE
    #define QEXTERN_C_OPEN
    #define QEXTERN_C_CLOSE

#endif //__cplusplus


/**
*   Angle Indices:
**/
#define PITCH               0       // up / down
#define YAW                 1       // left / right
#define ROLL                2       // fall over


/**
*   String Limits:
**/
#define MAX_STRING_CHARS    4096    // max length of a string passed to Cmd_TokenizeString
#define MAX_STRING_TOKENS   256     // max tokens resulting from Cmd_TokenizeString
#define MAX_TOKEN_CHARS     1024    // max length of an individual token
#define MAX_NET_STRING      2048    // max length of a string used in network protocol

#define MAX_QPATH			64      // max length of a quake game pathname
#ifdef WIN32
    // We know for a fact in Win32 this is 260. Including windows.h is messy. So..
    #define MAX_OSPATH      256 // Just default to 256, as the max length of a filesystem pathname.
#else
    #define MAX_OSPATH      PATH_MAX // Unix (one I got, has 4096) max length of a filesystem pathname.
#endif


/**
*   ConfigString type:
**/
//! Maximum length of a configstring.
#define MAX_CS_STRING_LENGTH	96	
//! The actual configstring_t char array type.
typedef char configstring_t[ MAX_CS_STRING_LENGTH ];


/**
*   Per-Level Limits:
**/
//! // Absolute limit
#define MAX_CLIENTS         256     
//! Maximum amount of server entities, must change protocol to increase more.
#define MAX_EDICTS          8192    
//! Maximum amount of client entities.
#define MAX_CLIENT_ENTITIES ( MAX_EDICTS ) /*8192*/

//! Maximum lightstyles.
#define MAX_LIGHTSTYLES     256
//! These are sent over the net as bytes so they cannot be blindly increased
#define MAX_MODELS          8192	// These are sent over the net as bytes
#define MAX_SOUNDS          2048	// 
#define MAX_IMAGES          512
#define MAX_ITEMS           256

// General config strings.
#define MAX_GENERAL         (MAX_CLIENTS * 2)

//! Used for player model index.
#define MAX_MODELS_OLD		256
//! Maximum clients char array name size.
#define MAX_CLIENT_NAME     16

/**
*   Reserved client numbers.
**/
//! Client number for 'none'.
#define CLIENTNUM_NONE        (MAX_CLIENTS - 1)
#define CLIENTNUM_RESERVED    (MAX_CLIENTS - 1)

/**
*   Reserved entity numbers.
**/
//! Entity number for 'none'.
#define ENTITYNUM_NONE      ( -1 )
//! Entity number for 'World'.
#define ENTITYNUM_WORLD     ( 0 )

/**
*   Reserved model handle indices.
**/
//! None
#define MODELINDEX_NONE			( 0 )
//! Player model.
#define MODELINDEX_PLAYER		( MAX_MODELS_OLD - 1 ) // 255
//! Worldspawn world model.
#define MODELINDEX_WORLDSPAWN	( 1 )

/**
*   Math Library:
**/
// Includes the 'old' math library support. Which is pretty much still in use all over the place.
// Strictly speaking, for consistency sake, we should update all code however it is prone to creating
// possible new bugs and takes a lot of time to invest.
#include "shared/math/qm_legacy_math_macros.h"

//! Include our own custom version of raylib1.5 its raymath library.
#ifdef __cplusplus
    #include "shared/math/qm_math_cpp.h"
#else
    #include "shared/math/qm_math_c.h"
#endif // #ifdef __cplusplus

//! Color Index Table:
#include "shared/color_index_table.h"

//! Shared UI:
#include "shared/ui_shared.h"

//! Bit Utilities:
#include "shared/util/util_bits.h"
//! Endian Utilities:
#include "shared/util/util_endian.h"
//! Encode/Decode utilities
#include "shared/util/util_decode.h"
#include "shared/util/util_encode.h"
#include "shared/util/util_halffloat.h"
//! List Utility:
#include "shared/util/util_list.h"
//! String Utilities:
#include "shared/util/util_strings.h"


//! Key/Value Info Strings:
#include "shared/info_strings.h"


/**
*   @brief  Hunk allocation memory block, used for resource allocation.
**/
typedef struct {
    void *base;
    size_t  maxsize;
    size_t  cursize;
    size_t  mapped;
} memhunk_t;


//! KeyButton/KeyButton State:
#include "shared/keys/key_button.h"
//! Key indices/numbers:
#include "shared/keys/key_numbers.h"


//! CommandBuffers/Console/CVars:
#include "shared/cmd/cmd_cvars.h"
#include "shared/cmd/cmd_buffer.h"
#include "shared/cmd/cmd_print.h"


//! Collision: 
//! Maximum World 'Half-Size'. Now 8 times larger(+/- 32768) than the old Q2 Vanilla 'Half-Size': (+/- 4096).
#define CM_MAX_WORLD_HALF_SIZE 32768
//! Maximum World Size, used for calculating various trace distance end point vectors. ( 32768 * 2 == 65536 )
#define CM_MAX_WORLD_SIZE ( CM_MAX_WORLD_HALF_SIZE * 2 )
//! The maximum amount of characters in a texture name.
#define CM_MAX_TEXNAME     32

//! Maximum amount of entity clusters.
#define MAX_ENT_CLUSTERS    16
//! Maximum total entity leafs.
#define MAX_TOTAL_ENT_LEAFS 128

//! Collision(-Model) Shared Subsystem Stuff:
#include "shared/cm/cm_types.h"
#include "shared/cm/cm_entity.h"
#include "shared/cm/cm_plane.h"
#include "shared/cm/cm_surface.h"
#include "shared/cm/cm_material.h"
#include "shared/cm/cm_trace.h"
#include "shared/formats/format_bsp.h"
#include "shared/cm/cm_model.h"

// gi.BoxEdicts() can return a list of either solid or trigger entities
// FIXME: eliminate AREA_ distinction?
typedef enum sector_area_s {
	AREA_SOLID = 1,
	AREA_TRIGGERS = 2
} sector_area_t;


//!	Entity Muzzleflashes/Player Effects:
#include "shared/entities/entity_effects.h"
//!	Entity Render Flags:
#include "shared/entities/entity_renderflags.h"
//!	Entity Events:
#include "shared/entities/entity_events.h"
//! Entity Types:
#include "shared/entities/entity_types.h"
//! Entity State:
#include "shared/entities/entity_state.h"
#ifdef __cplusplus
    //! Shared Server/ServerGame Entity Data:
    #include "shared/server/sv_shared_edict.h"
#endif
//! Gamemode Flags: (TODO: Move into sharedgame and do per gamemode.?)
#include "shared/gamemode_flags.h"

//! Sound:
#include "shared/sound.h"



/**
*   User Flags, most are unused, likely were used in the past:
**/
//! Unused:
//#define UF_AUTOSCREENSHOT   1
//! Unused:
//#define UF_AUTORECORD       2
//! Whether to use the 'local' point of view for lerping player pov.
#define UF_LOCALFOV         4
//! Unused:
//#define UF_MUTE_PLAYERS     8
//! Unused:
//#define UF_MUTE_OBSERVERS   16
//#define UF_MUTE_MISC        32
//! Whether to use the player's 'own' point of view for lerping player pov.
#define UF_PLAYERFOV        64


/**
*   @brief  Destination determinant for gi.multicast()
**/
typedef enum {
    //! Multicast to all client.
    MULTICAST_ALL,
    //! Multicast to all clients inside of PHS(Possible Hearing Set).
    MULTICAST_PHS,
    //! Multicast to all clients inside of PVS(Possible Visibility Set).
    MULTICAST_PVS
} multicast_t;

// Used for identifying brush model solids with so that it uses the internal BSP bounding box.
// An otherwise SOLID_BOUNDS_BOX etc, will never create this value(Used to be 255):
#define BOUNDS_BRUSHMODEL      ( ( cm_solid_t )( 0xffffffff ) )

/***
* 	Config Strings: A general means of communication from the server to all connected clients.
*                   Each config string can be at most MAX_QPATH characters.
***/
#include "shared/net/net_configstrings.h"
//! Elements Communicated across the NET.
#include "shared/net/net_elements.h"
//! Server to Client, and Client to Server CommandMessages.
#include "shared/net/net_command_messages.h"
//! User Commands( User Input ):
#include "shared/net/net_usercommand.h"
//! Network Frame Flags:
#define FF_NONE			0
#define FF_SUPPRESSED   (1<<0)
#define FF_CLIENTDROP   (1<<1)
#define FF_CLIENTPRED   (1<<2)	// Set but unused?
#define FF_RESERVED     (1<<3)	// Literally reserved.


/**
*	Player State:
**/
#include "shared/player_state.h"
/**
*	Player Movement:
**/
#include "shared/player_move.h"


/**
*   Tag Memory Allocation Zones:
**/
#include "shared/zone_tags.h"

//! Import cpplib shared code.
#ifdef __cplusplus

#endif