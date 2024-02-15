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
#ifndef __cplusplus
    // C STD Headers:
    #include <math.h>
    #include <ctype.h>
    #include <stdio.h>
    #include <stdarg.h>
    #include <string.h>
    #include <stdlib.h>
    #include <stdint.h>
    #include <stdbool.h>
    #include <inttypes.h>
    #include <limits.h>
    #include <time.h>
#else//__cplusplus
    // C STD Headers:
    #include <cctype>
    #include <cinttypes>
    #include <climits>
    #include <cmath>
    #include <cstdarg>
    #include <cstdbool>
    #include <cstdint>
    #include <cstdio>
    #include <cstdlib>
    #include <cstring>
    #include <ctime>
    // C++ STL Headers:
    #include <type_traits>
    #include <algorithm>
    #include <array>
    #include <functional>
    #include <map>
    #include <numeric>
    #include <unordered_map>
    #include <set>
    #include <unordered_set>
    #include <random>
    #include <string_view>
    #include <vector>
    #include <chrono>
#endif//__cplusplus

//! Include Endianness utilities if not already included by another system header.
#if HAVE_ENDIAN_H
    #include <endian.h>
#endif

//! Include shared platform data.
#include "shared/platform.h"

//! Determine and define which endianness we're compiling for.
#if __BYTE_ORDER == __LITTLE_ENDIAN
    #define USE_LITTLE_ENDIAN   1
#elif __BYTE_ORDER == __BIG_ENDIAN
    #define USE_BIG_ENDIAN      1
#endif

//! Utility to get the count of arrays.
#define q_countof(a) (sizeof(a) / sizeof(a[0]))

/**
*   
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
#endif //__cplusplus

#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif

// Angle Indexes
#define PITCH               0       // up / down
#define YAW                 1       // left / right
#define ROLL                2       // fall over

#define MAX_STRING_CHARS    4096    // max length of a string passed to Cmd_TokenizeString
#define MAX_STRING_TOKENS   256     // max tokens resulting from Cmd_TokenizeString
#define MAX_TOKEN_CHARS     1024    // max length of an individual token
#define MAX_NET_STRING      2048    // max length of a string used in network protocol

#define MAX_QPATH				64      // max length of a quake game pathname
#define MAX_OSPATH				256     // max length of a filesystem pathname

//
// ConfigString type.
//
//! Maximum length of a configstring.
#define MAX_CS_STRING_LENGTH	96	
//! The actual configstring_t char array type.
typedef char configstring_t[ MAX_CS_STRING_LENGTH ];

/**
*
*   Per-Level Limits:
*
**/
//! // Absolute limit
#define MAX_CLIENTS         256     
//! Maximum amount of server entities, must change protocol to increase more.
#define MAX_EDICTS          8192    
//! Maximum amount of client entities.
#define MAX_CLIENT_ENTITIES 8192

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

//! Client number for 'none'.
#define CLIENTNUM_NONE        (MAX_CLIENTS - 1)
#define CLIENTNUM_RESERVED    (MAX_CLIENTS - 1)



/**
*
*   Shared "Common":
*
**/
/**
*   @brief  Com_Error specific types:
**/
typedef enum {
    //! Exit the entire game with a popup window.
    ERR_FATAL,          
    //! Print to console and disconnect from game.
    ERR_DROP,           
    //! Like ERR_DROP, but not an error.
    ERR_DISCONNECT,     
    //! Make the server broadcast 'reconnect' message.
    ERR_RECONNECT       
} error_type_t;
/**
*   @brief  Com_LPrintf specific types:
**/
typedef enum {
    //! For general messages.
    PRINT_ALL,          
    //! Prints in green color.
    PRINT_TALK,         
    //! Only prints when the cvar "developer" == 1
    PRINT_DEVELOPER,
    PRINT_WARNING,      // print in yellow color
    PRINT_ERROR,        // print in red color
    PRINT_NOTICE        // print in cyan color
} print_type_t;

//! Prints to console the specific message in its 'type'.
void    Com_LPrintf(print_type_t type, const char *fmt, ...)
q_printf(2, 3);
//! For erroring out and returning(in most cases) back to a disconnect console.
void    Com_Error(error_type_t code, const char *fmt, ...)
q_noreturn q_printf(2, 3);

#define Com_Printf(...) Com_LPrintf(PRINT_ALL, __VA_ARGS__)
#define Com_WPrintf(...) Com_LPrintf(PRINT_WARNING, __VA_ARGS__)
#define Com_EPrintf(...) Com_LPrintf(PRINT_ERROR, __VA_ARGS__)

#define Q_assert(expr) \
    do { if (!(expr)) Com_Error(ERR_FATAL, "%s: assertion `%s' failed", __func__, #expr); } while (0)

// game print flags
#define PRINT_LOW           0       // pickup messages
#define PRINT_MEDIUM        1       // death messages
#define PRINT_HIGH          2       // critical messages
#define PRINT_CHAT          3       // chat messages    

// destination class for gi.multicast()
typedef enum {
    MULTICAST_ALL,
    MULTICAST_PHS,
    MULTICAST_PVS
} multicast_t;

/**
*
*
*   The Math Library Code:
*
**/
// Includes the 'old' math library support. Which is pretty much still in use all over the place.
// Strictly speaking, for consistency sake, we should update all code however it is prone to creating
// possible new bugs and takes a lot of time to invest.
#include "shared/math/math_old.h"
#ifdef __cplusplus
};  // Escape extern "C" for raymath.h include. ( This is because we got some C++ specifics code in there. )
#endif
// Use static inlining for all its functions.
#define RAYMATH_STATIC_INLINE
// Include our own custom version of raylib1.5 its raymath library.
#include <shared/math/qray_math.h>
#ifdef __cplusplus
// We extern back to "C"
extern "C" {
#endif

/**
*   Color Index Table:
**/
#include "shared/color_index_table.h"
/**
*	Bit Utilities:
**/
#include "shared/util_bits.h"

/**
*	Endian Utilities:
**/
#include "shared/util_endian.h"

/**
*	String Utilities:
**/
#include "shared/util_strings.h"

/**
*	Encode/Decode utilities
**/
#include "shared/util_encode.h"
#include "shared/util_decode.h"


/**
*   Key/Value Info Strings:
**/
#include "shared/info_strings.h"

/**
*   CVARS (console variables)/Command Buffer
*
*   Actually part of the /common/ code. What is defined here, is the bare necessity of limited
*   API for the game codes to make use of.
*
**/
#include "shared/command_cvars.h"
#include "shared/command_buffer.h"

/**
*	Collision Detection:
**/
#include "shared/collision.h"

/**
*	Muzzleflashes/Player Effects:
**/
#include "shared/entity_effects.h"

/**
*   Entity Render Flags:
**/
#include "shared/entity_renderflags.h"

/**
*	Entity Events:
**/
#include "shared/entity_events.h"

/**
*	Entity Types:
**/
#include "shared/entity_types.h"

/**
*	Entity State:
**/
#include "shared/entity_state.h"

/**
*	Muzzleflashes:
**/
#include "shared/muzzleflashes.h"

/**
*	Monster Muzzleflashes:
**/
#include "shared/muzzleflashes_monsters.h"

/**
*	Temp Entity Events:
**/
#include "shared/temp_entity_events.h"

/**
*	Sound:
**/
#include "shared/sound.h"

/**
*	Gamemode Flags: (TODO: Move into sharedgame and do per gamemode.?)
**/
#include "shared/gamemode_flags.h"

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

/***
* 	Config Strings: A general means of communication from the server to all connected clients.
*                   Each config string can be at most MAX_QPATH characters.
***/
#include "shared/net_configstrings.h"

/**
*	Elements Communicated across the NET.
**/
#include "shared/net_elements.h"

/**
*   Server to Client, and Client to Server CommandMessages.
**/
#include "shared/net_command_messages.h"

/**
*	User Commands( User Input ):
**/
#include "shared/net_usercommand.h"

/**
*	Player Movement:
**/
#include "shared/player_move.h"

/**
*	Player State:
**/
#include "shared/player_state.h"

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
};
#endif
