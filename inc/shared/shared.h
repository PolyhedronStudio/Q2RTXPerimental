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

#ifndef SHARED_H
#define SHARED_H

//
// shared.h -- included first by ALL program modules
//

#if HAVE_CONFIG_H
#include "config.h"
#endif

/**
*	Include based on whether the unit including is .c or .cpp
**/
#ifndef __cplusplus
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
#endif//__cplusplus

#if HAVE_ENDIAN_H
#include <endian.h>
#endif

#include "shared/platform.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define USE_LITTLE_ENDIAN   1
#elif __BYTE_ORDER == __BIG_ENDIAN
#define USE_BIG_ENDIAN      1
#endif

#define q_countof(a)        (sizeof(a) / sizeof(a[0]))

// WID: C++20: For C++ we use int to align with C qboolean type.
#ifdef __cplusplus
	// Include our 'sharedcpp.h' header.
	#include "shared_cpp.h"
#else // __cplusplus
	typedef unsigned char byte;
	typedef enum { qfalse, qtrue } qboolean;    // ABI compat only, don't use
	typedef int qhandle_t;

	#ifndef NULL
	#define NULL ((void *)0)
	#endif
#endif //__cplusplus

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif

// angle indexes
#define PITCH               0       // up / down
#define YAW                 1       // left / right
#define ROLL                2       // fall over

#define MAX_STRING_CHARS    4096    // max length of a string passed to Cmd_TokenizeString
#define MAX_STRING_TOKENS   256     // max tokens resulting from Cmd_TokenizeString
#define MAX_TOKEN_CHARS     1024    // max length of an individual token
#define MAX_NET_STRING      2048    // max length of a string used in network protocol

#define MAX_QPATH				64      // max length of a quake game pathname
#define MAX_OSPATH				256     // max length of a filesystem pathname
#define MAX_CS_STRING_LENGTH	96		// Maximum length of a configstring.

//
// ConfigString type.
//
typedef char configstring_t[ MAX_CS_STRING_LENGTH ];

//
// per-level limits
//
#define MAX_CLIENTS         256     // Absolute limit

#define MAX_EDICTS          8192    // maximum amount of server entities, must change protocol to increase more.
#define MAX_CLIENT_ENTITIES 8192    // maximum amount of client entities.

#define MAX_LIGHTSTYLES     256
#define MAX_MODELS          8192	// These are sent over the net as bytes
#define MAX_SOUNDS          2048	// so they cannot be blindly increased
#define MAX_IMAGES          512
#define MAX_ITEMS           256

#define MAX_GENERAL         (MAX_CLIENTS * 2) // general config strings
#define MAX_MODELS_OLD		256		// Used for player model index.

#define MAX_CLIENT_NAME     16

#define CLIENTNUM_NONE        (MAX_CLIENTS - 1)
#define CLIENTNUM_RESERVED    (MAX_CLIENTS - 1)

typedef enum {
    ERR_FATAL,          // exit the entire game with a popup window
    ERR_DROP,           // print to console and disconnect from game
    ERR_DISCONNECT,     // like drop, but not an error
    ERR_RECONNECT       // make server broadcast 'reconnect' message
} error_type_t;

typedef enum {
    PRINT_ALL,          // general messages
    PRINT_TALK,         // print in green color
    PRINT_DEVELOPER,    // only print when "developer 1"
    PRINT_WARNING,      // print in yellow color
    PRINT_ERROR,        // print in red color
    PRINT_NOTICE        // print in cyan color
} print_type_t;

void    Com_LPrintf(print_type_t type, const char *fmt, ...)
q_printf(2, 3);
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

/*
==============================================================

MATHLIB

==============================================================
*/
// Includes the 'old' math library support. Which is pretty much still in use all over the place.
// Strictly speaking, for consistency sake, we should update all code however it is prone to creating
// possible new bugs and takes a lot of time to invest.
#include "shared/math/math_old.h"

// Undo extern "C" here for 'HandMadeMath'.
#ifdef __cplusplus
};  // Escape extern "C" for raymath.h
#endif

// Include our own custom version of raylib1.5 its raymath library.
#define RAYMATH_STATIC_INLINE
#include <shared/math/qray_math.h>

#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif
/**
*
*	Bit Utilities:
*
**/
#define BIT(n)          (1U << (n))
#define BIT_ULL(n)      (1ULL << (n))

#define Q_IsBitSet(data, bit)   (((data)[(bit) >> 3] & (1 << ((bit) & 7))) != 0)
#define Q_SetBit(data, bit)     ((data)[(bit) >> 3] |= (1 << ((bit) & 7)))
#define Q_ClearBit(data, bit)   ((data)[(bit) >> 3] &= ~(1 << ((bit) & 7)))


/**
*
*	String Utilities:
*
**/
#include "shared/string_utilities.h"


/**
*
*	Endian Utilities:
*
**/
#include "shared/endian.h"



/**
*
*   Key/Value Info Strings:
*
**/
#include "info_strings.h"



/**
*
*   CVARS (console variables):
*
*   Actually part of the /common/ code. What is defined here, is the bare necessity of limited
*   API for the game codes to make use of.
*
**/
#ifndef CVAR
#define CVAR

#define CVAR_ARCHIVE    1   // set to cause it to be saved to vars.rc
#define CVAR_USERINFO   2   // added to userinfo  when changed
#define CVAR_SERVERINFO 4   // added to serverinfo when changed
#define CVAR_NOSET      8   // don't allow change from console at all,
                            // but can be set from the command line
#define CVAR_LATCH      16  // save changes until server restart
#ifndef CVAR_PRIVATE
	#define CVAR_PRIVATE	(1 << 6)  // never macro expanded or saved to config
#endif
#ifndef CVAR_ROM
	#define CVAR_ROM		(1 << 7)  // can't be changed even from cmdline
#endif

struct cvar_s;
struct genctx_s;

typedef void (*xchanged_t)(struct cvar_s *);
typedef void (*xgenerator_t)(struct genctx_s *);

// nothing outside the cvar.*() functions should modify these fields!
typedef struct cvar_s {
    char        *name;
    char        *string;
    char        *latched_string;    // for CVAR_LATCH vars
    int         flags;
    qboolean    modified;   // set each time the cvar is changed
    float       value;
    struct cvar_s *next;
    // 
    int         integer;
// ------ new stuff ------
#if USE_CLIENT || USE_SERVER
    //int         integer;
    char        *default_string;
    xchanged_t      changed;
    xgenerator_t    generator;
    struct cvar_s   *hashNext;
#endif
} cvar_t;

#endif      // CVAR



/**
*
*	Collision Detection:
*
**/
/**
*   @brief  Brush Contents: lower bits are stronger, and will eat weaker brushes completely
**/
typedef enum {
    CONTENTS_NONE = 0,
    CONTENTS_SOLID = BIT( 0 ),  // An eye is never valid in a solid.
    CONTENTS_WINDOW = BIT( 1 ), // Translucent, but not watery.
    CONTENTS_AUX = BIT( 2 ),
    CONTENTS_LAVA = BIT( 3 ),
    CONTENTS_SLIME = BIT( 4 ),
    CONTENTS_WATER = BIT( 5 ),
    CONTENTS_MIST = BIT( 6 ),

    // Remaining contents are non-visible, and don't eat brushes.
    CONTENTS_NO_WATERJUMP = BIT( 13 ),      // [Paril-KEX] This brush cannot be waterjumped out of.
    CONTENTS_PROJECTILECLIP = BIT( 14 ),    // [Paril-KEX] Projectiles will collide with this.

    CONTENTS_AREAPORTAL = BIT( 15 ),

    CONTENTS_PLAYERCLIP = BIT( 16 ),
    CONTENTS_MONSTERCLIP = BIT( 17 ),

    // Currents can be added to any other contents, and may be mixed.
    CONTENTS_CURRENT_0 = BIT( 18 ),
    CONTENTS_CURRENT_90 = BIT( 19 ),
    CONTENTS_CURRENT_180 = BIT( 20 ),
    CONTENTS_CURRENT_270 = BIT( 21 ),
    CONTENTS_CURRENT_UP = BIT( 22 ),
    CONTENTS_CURRENT_DOWN = BIT( 23 ),

    CONTENTS_ORIGIN = BIT( 24 ), // Removed before bsping an entity.

    CONTENTS_MONSTER = BIT( 25 ), // Should never be on a brush, only in game.
    CONTENTS_DEADMONSTER = BIT( 26 ),

    CONTENTS_DETAIL = BIT( 27 ),        // Brushes to be added after vis leafs.
    CONTENTS_TRANSLUCENT = BIT( 28 ),   // Auto set if any surface has trans.
    CONTENTS_LADDER = BIT( 29 ),

    CONTENTS_PLAYER = BIT( 30 ) , // [Paril-KEX] should never be on a brush, only in game; player
    CONTENTS_PROJECTILE = BIT( 31 )  // [Paril-KEX] should never be on a brush, only in game; projectiles.
    // used to solve deadmonster collision issues.
} contents_t;

/**
*   Surface Types:
**/
#define SURF_LIGHT      0x1     // value will hold the light strength
#define SURF_SLICK      0x2     // effects game physics
#define SURF_SKY        0x4     // don't draw, but add to skybox
#define SURF_WARP       0x8     // turbulent water warp
#define SURF_TRANS33    0x10
#define SURF_TRANS66    0x20
#define SURF_FLOWING    0x40    // scroll towards angle
#define SURF_NODRAW     0x80    // don't bother referencing the texture
#define SURF_ALPHATEST  0x02000000  // used by kmquake2
#define SURF_N64_UV             (1U << 28)
#define SURF_N64_SCROLL_X       (1U << 29)
#define SURF_N64_SCROLL_Y       (1U << 30)
#define SURF_N64_SCROLL_FLIP    (1U << 31)

/**
*   ContentMask Sets:
**/
#define MASK_SOLID              contents_t( CONTENTS_SOLID | CONTENTS_WINDOW )
#define MASK_PLAYERSOLID        contents_t( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_PLAYER )
#define MASK_DEADSOLID          contents_t( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW )
#define MASK_MONSTERSOLID       contents_t( CONTENTS_SOLID | CONTENTS_MONSTERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_PLAYER )
#define MASK_WATER              contents_t( CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME )
#define MASK_OPAQUE             contents_t( CONTENTS_SOLID | CONTENTS_SLIME | CONTENTS_LAVA )
#define MASK_SHOT               contents_t( CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_PLAYER | CONTENTS_WINDOW | CONTENTS_DEADMONSTER )
#define MASK_CURRENT            contents_t( CONTENTS_CURRENT_0 | CONTENTS_CURRENT_90 | CONTENTS_CURRENT_180 | CONTENTS_CURRENT_270 | CONTENTS_CURRENT_UP | CONTENTS_CURRENT_DOWN )
//#define MASK_BLOCK_SIGHT        ( CONTENTS_SOLID | CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_MONSTER | CONTENTS_PLAYER )
//#define MASK_NAV_SOLID          ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW )
//#define MASK_LADDER_NAV_SOLID   ( CONTENTS_SOLID | CONTENTS_WINDOW )
//#define MASK_WALK_NAV_SOLID     ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTERCLIP )
#define MASK_PROJECTILE         contents_t( MASK_SHOT | CONTENTS_PROJECTILECLIP )

// gi.BoxEdicts() can return a list of either solid or trigger entities
// FIXME: eliminate AREA_ distinction?
#define AREA_SOLID      1
#define AREA_TRIGGERS   2


/**
*   @brief  BSP Brush plane_t structure. To acquire the origin,
*           scale normal by dist.
**/
typedef struct cplane_s {
    vec3_t  normal;
    float   dist;
    byte    type;           // for fast side tests
    byte    signbits;       // signx + (signy<<1) + (signz<<1)
    byte    pad[2];
} cplane_t;

// 0-2 are axial planes
#define PLANE_X         0
#define PLANE_Y         1
#define PLANE_Z         2

// 3-5 are non-axial planes snapped to the nearest
#define PLANE_ANYX      3
#define PLANE_ANYY      4
#define PLANE_ANYZ      5

// planes (x&~1) and (x&~1)+1 are always opposites

#define PLANE_NON_AXIAL 6

/**
*   @brief  BSP Brush side surface. 
*
*   Stores material/texture name, flags as well as an 
*   integral 'value' which was commonly used for light flagged surfaces.
**/
typedef struct csurface_s {
    char        name[16];
    int         flags;
    int         value;
} csurface_t;

/**
*   @brief  A trace is basically 'sweeping' the collision shape(bounding box) through the world
*           and returning the results of that 'sweep'. 
*           (ie, how far did it get, whether it is inside of a solid or not, etc)
**/
// a trace is returned when a box is swept through the world
typedef struct trace_s {
    qboolean    allsolid;   // if true, plane is not valid
    qboolean    startsolid; // if true, the initial point was in a solid area
    float       fraction;   // time completed, 1.0 = didn't hit anything
    vec3_t      endpos;     // final position
    cplane_t    plane;      // surface normal at impact
    csurface_t  *surface;   // surface hit
    contents_t  contents;   // contents on other side of surface hit
    struct edict_s  *ent;       // not set by CM_*() functions

	// [Paril-KEX] the second-best surface hit from a trace
	cplane_t	plane2;		// second surface normal at impact
	csurface_t *surface2;	// second surface hit
} trace_t;



/**
*
*	User Commands( User Input ):
*
**/
//! Set in case of no button being pressed for the frame at all.
#define BUTTON_NONE         0
//! Set when the 'attack'(bombs away) button is pressed.
#define BUTTON_ATTACK       1
//! Set when the 'use' button is pressed. (Use item, not actually 'use targetting entity')
//! TODO: Probably rename to USE_ITEM instead later when this moves to the client game.
#define BUTTON_USE          2
//! Set when the holster button is pressed.
#define BUTTON_HOLSTER      4
//! Set when the jump key is pressed.
#define BUTTON_JUMP         8
//! Set when the crouch/duck key is pressed.
#define BUTTON_CROUCH       16
//! Set when any button is pressed at all.
#define BUTTON_ANY          128         // any key whatsoever


/**
*   @brief  usercmd_t is sent multiple times to the server for each client frame.
**/
typedef struct usercmd_s {
    //! Amount of miliseconds for this user command frame.
	uint8_t  msec;
    //! Button bits, determines which keys are pressed.
    uint16_t buttons;
    //! View angles.
	vec3_t  angles;
    //! Direction key when held, 'speeds':
	float   forwardmove, sidemove, upmove;
    //! The impulse.
	byte    impulse;        // remove?
    //! The frame number, can be used for possible anti-lag. TODO: Implement something for that.
    uint64_t frameNumber;
} usercmd_t;



/**
*
*	Player Movement:
*
**/
// edict->solid values
/**
*   @brief  The actual 'solid' type of an entity, determines how to behave when colliding with other objects.
**/
typedef enum {
    SOLID_NOT,          //! No interaction with other objects.
    SOLID_TRIGGER,      //! Only touch when inside, after moving. (Optional BSP Brush clip when SVF_HULL is set.)
    SOLID_BBOX,         //! Touch on edge.
    SOLID_BSP           //! BSP clip, touch on edge.
} solid_t;

/**
*   @brief  The water 'level' of said entity.
**/
typedef enum {  //: uint8_t {
	WATER_NONE,
	WATER_FEET,
	WATER_WAIST,
	WATER_UNDER
} water_level_t;

/**
*   pmove_state_t is the information necessary for client side movement prediction.
**/
typedef enum {  // : uint8_t {
    // Types that can accelerate and turn:
    PM_NORMAL,      //! Gravity. Clips to world and its entities.
    PM_GRAPPLE,     //! No gravity. Pull towards velocity.
    PM_NOCLIP,      //! No gravity. Don't clip against entities/world at all. 
    PM_SPECTATOR,   //! No gravity. Only clip against walls.

    // Types with no acceleration or turning support:
    PM_DEAD,
    PM_GIB,         //! Different bounding box for when the player is 'gibbing out'.
    PM_FREEZE       //! Does not respond to any movement inputs.
} pmtype_t;

// pmove->pm_flags
#define PMF_NONE						0   //! No flags.
#define PMF_DUCKED						1   //! Player is ducked.
#define PMF_JUMP_HELD					2   //! Player is keeping jump button pressed.
#define PMF_ON_GROUND					4   //! Player is on-ground.
#define PMF_TIME_WATERJUMP				8   //! pm_time is waterjump.
#define PMF_TIME_LAND					16  //! pm_time is time before rejump.
#define PMF_TIME_TELEPORT				32  //! pm_time is non-moving time.
#define PMF_NO_POSITIONAL_PREDICTION	64  //! Temporarily disables prediction (used for grappling hook).
//#define PMF_TELEPORT_BIT				128 //! used by q2pro
#define PMF_ON_LADDER					128	//! Signal to game that we are on a ladder.
#define PMF_NO_ANGULAR_PREDICTION		256 //! Temporary disables angular prediction.
#define PMF_IGNORE_PLAYER_COLLISION		512	//! Don't collide with other players.
#define PMF_TIME_TRICK_JUMP				1024//! pm_time is the trick jump time.
//#define PMF_GROUNDENTITY_CHANGED        2048//! Set if the ground entity has changed between previous and current pmove state.

/**
*   This structure needs to be communicated bit-accurate from the server to the client to guarantee that
*   prediction stays in sync. If any part of the game code modifies this struct, it will result in a 
*   prediction error of some degree.
**/
typedef struct {
    pmtype_t    pm_type;

    Vector3		origin;
    Vector3		velocity;
    uint16_t    pm_flags;		//! Ducked, jump_held, etc
	uint16_t	pm_time;		//! Each unit = 8 ms
    short       gravity;
    Vector3     delta_angles;	//! Add to command angles to get view direction
								//! changed by spawns, rotating objects, and teleporters
	int8_t		viewheight;		//! View height, added to origin[2] + viewoffset[2], for crouching
} pmove_state_t;

/**
*	@brief	Used to configure player movement with, it is set by SG_ConfigurePlayerMoveParameters.
* 
*			NOTE: In the future this will change, obviously.
**/
typedef struct {
	bool        qwmode;
	int32_t     airaccelerate;
	bool        strafehack;
	bool        flyhack;
	bool        waterhack;
	float       speedmult;
	float       watermult;
	float       maxspeed;
	float       friction;
	float       waterfriction;
	float       flyfriction;
} pmoveParams_t;

/**
*   @brief  Used for registering entity touching resulting traces.
**/
#define MAX_TOUCH_TRACES    32
typedef struct pm_touch_trace_list_s {
    uint32_t numberOfTraces;
    trace_t traces[ MAX_TOUCH_TRACES ];
} pm_touch_trace_list_t;

/**
*   @brief  Object storing data such as the player's move state, in order to perform another 
*           frame of movement on its data.
**/
typedef struct {
    /**
    *   (In/Out):
    **/
    pmove_state_t s;

    /**
    *   (In):
    **/
    //! The player's move command.
	usercmd_t	cmd;
    bool		snapinitial;    // if s has been changed outside pmove

    struct edict_s *player;

    /**
    *   (Out):
    **/
    //! Contains the trace results of any entities touched.
	pm_touch_trace_list_t touchTraces;

    /**
    *   (In/Out):
    **/
    //! Actual view angles, clamped to (0 .. 360) and for Pitch(-89 .. 89).
    Vector3 viewangles;
    //! bounding box size.
    Vector3 mins, maxs;

    //! Pointer ot the actual ground entity we are on or not(nullptr).
    struct edict_s  *groundentity;
    //! A copy of the plane data from our ground entity.
    cplane_t        groundplane;
    //! The actual BSP 'contents' type we're in.
    contents_t			watertype;
    //! The depth of the player in the actual water solid.
    water_level_t	waterlevel;

    /**
    *   (Out):
    **/
    //! Callbacks to test the world with.
    //! Trace against all entities.
    const trace_t( *q_gameabi trace )( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const void *passEntity, const contents_t contentMask );
    //! Clips to world only.
    const trace_t( *q_gameabi clip )( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, /*const void *clipEntity,*/ const contents_t contentMask );
    //! PointContents.
    const contents_t( *q_gameabi pointcontents )( const vec3_t point );

    /**
    *   (In):
    **/
    // [KEX] variables (in)
    Vector3 viewoffset; // last viewoffset (for accurate calculation of blending)

    /**
    *   (Out):
    **/
    // [KEX] results (out)
    Vector4 screen_blend;
    //! Merged with rdflags from server.
    int32_t rdflags;
    //! Play jump sound.
    bool jump_sound;
    //! We clipped on top of an object from below.
    bool step_clip; 
    //! Impact delta, for falling damage.
    float impact_delta;
} pmove_t;



/**
*
*	Muzzleflashes/Player Effects:
*
**/
//
// entity_state_t->effects
// Effects are things handled on the client side (lights, particles, frame animations)
// that happen constantly on the given entity.
// An entity that has effects will be sent to the client
// even if it has a zero index model.
#define EF_NONE             0x00000000
#define EF_ROTATE           0x00000001      // rotate (bonus items)
#define EF_GIB              0x00000002      // leave a trail
#define EF_BLASTER          0x00000008      // redlight + trail
#define EF_ROCKET           0x00000010      // redlight + trail
#define EF_GRENADE          0x00000020
#define EF_HYPERBLASTER     0x00000040
#define EF_BFG              0x00000080
#define EF_COLOR_SHELL      0x00000100
#define EF_POWERSCREEN      0x00000200
#define EF_ANIM01           0x00000400      // automatically cycle between frames 0 and 1 at 2 hz
#define EF_ANIM23           0x00000800      // automatically cycle between frames 2 and 3 at 2 hz
#define EF_ANIM_ALL         0x00001000      // automatically cycle through all frames at 2hz
#define EF_ANIM_ALLFAST     0x00002000      // automatically cycle through all frames at 10hz
#define EF_FLIES            0x00004000
#define EF_QUAD             0x00008000
#define EF_PENT             0x00010000
#define EF_TELEPORTER       0x00020000      // particle fountain
#define EF_FLAG1            0x00040000
#define EF_FLAG2            0x00080000
// RAFAEL
#define EF_IONRIPPER        0x00100000
#define EF_GREENGIB         0x00200000
#define EF_BLUEHYPERBLASTER 0x00400000
#define EF_SPINNINGLIGHTS   0x00800000
#define EF_PLASMA           0x01000000
#define EF_TRAP             0x02000000

//ROGUE
#define EF_TRACKER          0x04000000
#define EF_DOUBLE           0x08000000
#define EF_SPOTLIGHT		0x10000000
#define EF_TAGTRAIL         0x20000000
#define EF_HALF_DAMAGE      0x40000000
#define EF_TRACKERTRAIL     0x80000000
//ROGUE

//
//! entity_state_t->renderfx flags
#define RF_NONE             0
#define RF_MINLIGHT         1       // allways have some light (viewmodel)
#define RF_VIEWERMODEL      2       // don't draw through eyes, only mirrors
#define RF_WEAPONMODEL      4       // only draw through eyes
#define RF_FULLBRIGHT       8       // allways draw full intensity
#define RF_DEPTHHACK        16      // for view weapon Z crunching
#define RF_TRANSLUCENT      32
#define RF_FRAMELERP        64
#define RF_BEAM             128
#define RF_CUSTOMSKIN       256     // skin is an index in image_precache
#define RF_GLOW             512     // pulse lighting for bonus items
#define RF_SHELL_RED        1024
#define RF_SHELL_GREEN      2048
#define RF_SHELL_BLUE       4096
#define RF_NOSHADOW         8192    // used by YQ2
#define	RF_OLD_FRAME_LERP	16384	// [Paril-KEX] force model to lerp from oldframe in entity state; otherwise it uses last frame client received
#define RF_STAIR_STEP		32768	// [Paril-KEX] re-tuned, now used to handle stair steps for monsters

//ROGUE
#define RF_IR_VISIBLE       0x00008000      // 32768
#define RF_SHELL_DOUBLE     0x00010000      // 65536
#define RF_SHELL_HALF_DAM   0x00020000
#define RF_USE_DISGUISE     0x00040000
//ROGUE

// player_state_t->refdef flags
#define RDF_NONE            0
#define RDF_UNDERWATER      1       // warp the screen as apropriate
#define RDF_NOWORLDMODEL    2       // used for player configuration screen

//ROGUE
#define RDF_IRGOGGLES       4
#define RDF_UVGOGGLES       8
//ROGUE

enum {
    MZ_BLASTER,
    MZ_MACHINEGUN,
    MZ_SHOTGUN,
    MZ_CHAINGUN1,
    MZ_CHAINGUN2,
    MZ_CHAINGUN3,
    MZ_RAILGUN,
    MZ_ROCKET,
    MZ_GRENADE,
    MZ_LOGIN,
    MZ_LOGOUT,
    MZ_RESPAWN,
    MZ_BFG,
    MZ_SSHOTGUN,
    MZ_HYPERBLASTER,
    MZ_ITEMRESPAWN,

// RAFAEL
    MZ_IONRIPPER,
    MZ_BLUEHYPERBLASTER,
    MZ_PHALANX,

//ROGUE
    MZ_ETF_RIFLE = 30,
    MZ_UNUSED,
    MZ_SHOTGUN2,
    MZ_HEATBEAM,
    MZ_BLASTER2,
    MZ_TRACKER,
    MZ_NUKE1,
    MZ_NUKE2,
    MZ_NUKE4,
    MZ_NUKE8,
//ROGUE
// Q2RTX
#define MZ_FLARE            40
// Q2RTX

    MZ_SILENCED = 128,  // bit flag ORed with one of the above numbers
};



/**
*
*	Monster Muzzleflashes:
*
**/
#include "shared/m_flash.h"



/**
*
*	Temp Entity Events:
*
*   Temp entity events are for things that happen at a location seperate from any existing entity.
*   Temporary entity messages are explicitly constructed and broadcast.
* 
**/
// temp entity events
typedef enum {
    TE_GUNSHOT,
    TE_BLOOD,
    TE_BLASTER,
    TE_RAILTRAIL,
    TE_SHOTGUN,
    TE_EXPLOSION1,
    TE_EXPLOSION2,
    TE_ROCKET_EXPLOSION,
    TE_GRENADE_EXPLOSION,
    TE_SPARKS,
    TE_SPLASH,
    TE_BUBBLETRAIL,
    TE_SCREEN_SPARKS,
    TE_SHIELD_SPARKS,
    TE_BULLET_SPARKS,
    TE_LASER_SPARKS,
    TE_PARASITE_ATTACK,
    TE_ROCKET_EXPLOSION_WATER,
    TE_GRENADE_EXPLOSION_WATER,
    TE_MEDIC_CABLE_ATTACK,
    TE_BFG_EXPLOSION,
    TE_BFG_BIGEXPLOSION,
    TE_BOSSTPORT,           // used as '22' in a map, so DON'T RENUMBER!!!
    TE_BFG_LASER,
    TE_GRAPPLE_CABLE,
    TE_WELDING_SPARKS,
    TE_GREENBLOOD,
    TE_BLUEHYPERBLASTER,
    TE_PLASMA_EXPLOSION,
    TE_TUNNEL_SPARKS,
//ROGUE
    TE_BLASTER2,
    TE_RAILTRAIL2,
    TE_FLAME,
    TE_LIGHTNING,
    TE_DEBUGTRAIL,
    TE_PLAIN_EXPLOSION,
    TE_FLASHLIGHT,
    TE_FORCEWALL,
    TE_HEATBEAM,
    TE_MONSTER_HEATBEAM,
    TE_STEAM,
    TE_BUBBLETRAIL2,
    TE_MOREBLOOD,
    TE_HEATBEAM_SPARKS,
    TE_HEATBEAM_STEAM,
    TE_CHAINFIST_SMOKE,
    TE_ELECTRIC_SPARKS,
    TE_TRACKER_EXPLOSION,
    TE_TELEPORT_EFFECT,
    TE_DBALL_GOAL,
    TE_WIDOWBEAMOUT,
    TE_NUKEBLAST,
    TE_WIDOWSPLASH,
    TE_EXPLOSION1_BIG,
    TE_EXPLOSION1_NP,
    TE_FLECHETTE,
//ROGUE
// Q2RTX
	TE_FLARE,
// Q2RTX

    TE_NUM_ENTITIES
} temp_event_t;

#define SPLASH_UNKNOWN      0
#define SPLASH_SPARKS       1
#define SPLASH_BLUE_WATER   2
#define SPLASH_BROWN_WATER  3
#define SPLASH_SLIME        4
#define SPLASH_LAVA         5
#define SPLASH_BLOOD        6



/**
*
*	Sound:
*
**/
// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
#define CHAN_AUTO               0
#define CHAN_WEAPON             1
#define CHAN_VOICE              2
#define CHAN_ITEM               3
#define CHAN_BODY               4
// modifier flags
#define CHAN_NO_PHS_ADD         8   // send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define CHAN_RELIABLE           16  // send by reliable message, not datagram


// sound attenuation values
#define ATTN_NONE               0   // full volume the entire level
#define ATTN_NORM               1
#define ATTN_IDLE               2
#define ATTN_STATIC             3   // diminish very rapidly with distance



/**
*
*	Stats Array Indexes, there is room for a game's own stats
*   after STAT_SPECTATOR up to MAX_STATS(64).
*
**/
// player_state->stats[] indexes
#define STAT_HEALTH_ICON        0
#define STAT_HEALTH             1
#define STAT_AMMO_ICON          2
#define STAT_AMMO               3
#define STAT_ARMOR_ICON         4
#define STAT_ARMOR              5
#define STAT_SELECTED_ICON      6
#define STAT_PICKUP_ICON        7
#define STAT_PICKUP_STRING      8
#define STAT_TIMER_ICON         9
#define STAT_TIMER              10
#define STAT_HELPICON           11
#define STAT_SELECTED_ITEM      12
#define STAT_LAYOUTS            13
#define STAT_FRAGS              14
#define STAT_FLASHES            15      // cleared each frame, 1 = health, 2 = armor
#define STAT_CHASE              16
#define STAT_SPECTATOR          17

#define MAX_STATS               64



/**
*
*	Gamemode Flags: (TODO: Move into sharedgame and do per gamemode.?)
*
**/
// dmflags->value flags
#define DF_NO_HEALTH        0x00000001  // 1
#define DF_NO_ITEMS         0x00000002  // 2
#define DF_WEAPONS_STAY     0x00000004  // 4
#define DF_NO_FALLING       0x00000008  // 8
#define DF_INSTANT_ITEMS    0x00000010  // 16
#define DF_SAME_LEVEL       0x00000020  // 32
#define DF_SKINTEAMS        0x00000040  // 64
#define DF_MODELTEAMS       0x00000080  // 128
#define DF_NO_FRIENDLY_FIRE 0x00000100  // 256
#define DF_SPAWN_FARTHEST   0x00000200  // 512
#define DF_FORCE_RESPAWN    0x00000400  // 1024
#define DF_NO_ARMOR         0x00000800  // 2048
#define DF_ALLOW_EXIT       0x00001000  // 4096
#define DF_INFINITE_AMMO    0x00002000  // 8192
#define DF_QUAD_DROP        0x00004000  // 16384
#define DF_FIXED_FOV        0x00008000  // 32768

// RAFAEL
#define DF_QUADFIRE_DROP    0x00010000  // 65536

//ROGUE
#define DF_NO_MINES         0x00020000
#define DF_NO_STACK_DOUBLE  0x00040000
#define DF_NO_NUKES         0x00080000
#define DF_NO_SPHERES       0x00100000
//ROGUE


#define UF_AUTOSCREENSHOT   1
#define UF_AUTORECORD       2
#define UF_LOCALFOV         4
#define UF_MUTE_PLAYERS     8
#define UF_MUTE_OBSERVERS   16
#define UF_MUTE_MISC        32
#define UF_PLAYERFOV        64



/**
*
*	Encode/Decode utilities
* 
**/
/**
*	Byte to Angles/Angles to Bytes.
**/
//! Used for 'wiring' angles, encoded in a 'byte/int8_t'.
static inline const uint8_t ANGLE2BYTE( const float coord ) {
	return ( (int)( ( coord ) * 256.0f / 360 ) & 255 );
}
//! Used for decoding the 'wired' angle in a 'float'.
static inline const float BYTE2ANGLE( const int s ) {
	return ( ( s ) * ( 360.0f / 256 ) );
}
/**
*	Short to Angles/Angles to Shorts.
**/
//! Used for 'wiring' angles encoded in a 'short/int16_t'.
static inline const int16_t ANGLE2SHORT( const float coord ) {
	return ( (int)( ( coord ) * 65536 / 360 ) & 65535 );
}
//! Used for decoding the 'wired' angle in a 'float'.
static inline const float SHORT2ANGLE( const int s ) {
	return ( ( s ) * ( 360.0f / 65536 ) );
}
///**
//*	float to HalfFloat-Angles/HalfFloat-Angles to Floats.
//**/
////! Used for 'wiring' angles encoded in a 'short/int16_t'.
//static inline const float ANGLE2HALFFLOAT( const float coord ) {
//	return ( (int)( ( coord ) * 65536 / 360.f ) & 65535 );
//}
////! Used for decoding the 'wired' angle in a 'float'.
//static inline const float HALFFLOAT2ANGLE( const int s ) {
//	return ( ( s ) * ( 360.f / 65536 ) );
//}
/**
*	Short to Origin/Origin to float.
**/
//! Used for 'wiring' origins encoded in a 'short/int16_t'..
static inline const int16_t COORD2SHORT( const float coord ) {
	return ( (int)( ( coord ) * 8.0f ) );
}
//! Used for decoding the 'wired' origin in a 'float'.
static inline const float SHORT2COORD( const int s ) {
	return ( ( s ) * ( 1.0f / 8 ) );
}


/***
* 	Config Strings are a general means of communication from the server to all connected clients.
*	Each config string can be at most MAX_QPATH characters.
***/
#define CS_NAME             0
#define CS_CDTRACK          1
#define CS_SKY              2
#define CS_SKYAXIS          3       // %f %f %f format
#define CS_SKYROTATE        4
#define CS_STATUSBAR        5       // display program string

#define CS_AIRACCEL         59      // air acceleration control
#define CS_MAXCLIENTS       60
#define CS_MAPCHECKSUM      61      // for catching cheater maps
#define CS_MODELS           62

#define CS_SOUNDS           (CS_MODELS+MAX_MODELS)
#define CS_IMAGES           (CS_SOUNDS+MAX_SOUNDS)
#define CS_LIGHTS           (CS_IMAGES+MAX_IMAGES)
#define CS_ITEMS            (CS_LIGHTS+MAX_LIGHTSTYLES)
#define CS_PLAYERSKINS      (CS_ITEMS+MAX_ITEMS)
#define CS_GENERAL          (CS_PLAYERSKINS+MAX_CLIENTS)
#define MAX_CONFIGSTRINGS   (CS_GENERAL+MAX_GENERAL)

#define MODELINDEX_PLAYER	(MAX_MODELS_OLD - 1)

// Some mods actually exploit CS_STATUSBAR to take space up to CS_AIRACCEL
static inline int32_t CS_SIZE( int32_t cs ) {
	return ( ( cs ) >= CS_STATUSBAR && ( cs ) < CS_AIRACCEL ? \
	  MAX_CS_STRING_LENGTH * ( CS_AIRACCEL - ( cs ) ) : MAX_CS_STRING_LENGTH );
}



/**
*
*	Elements Communicated across the NET.
*
**/
// Default server FPS: 10hz
//#define BASE_FRAMERATE          10
//#define BASE_FRAMETIME          100
//#define BASE_1_FRAMETIME        0.01f   // 1/BASE_FRAMETIME
//#define BASE_FRAMETIME_1000     0.1f    // BASE_FRAMETIME/1000

// Default Server FPS: 40hz
#define BASE_FRAMERATE          40		// OLD: 10
#define BASE_FRAMETIME          25		// OLD: 100		1000 / 10 = 100     NEW: 1000 / 40 = 25
#define BASE_1_FRAMETIME        0.04f	// OLD: 0.01f   1/BASE_FRAMETIME
#define BASE_FRAMETIME_1000     0.025f	// OLD: 0.1f    BASE_FRAMETIME/1000

//! Can't be increased without changing network protocol.
#define MAX_MAP_AREAS       256
//! Bitmasks communicated by server
#define MAX_MAP_AREA_BYTES      (MAX_MAP_AREAS / 8)
#define MAX_MAP_PORTAL_BYTES    MAX_MAP_AREA_BYTES

//! Circular update array.
#define UPDATE_BACKUP   512 //! 16	//! copies of entity_state_t to keep buffered must be power of two
#define UPDATE_MASK     (UPDATE_BACKUP - 1)

//! Circular command array.
#define CMD_BACKUP      512 //! 128	//! allow a lot of command backups for very fast systems increased from 64
#define CMD_MASK        (CMD_BACKUP - 1)

//! Max entities stuffed per packet.
#define MAX_PACKET_ENTITIES     1024
#define MAX_PARSE_ENTITIES      (MAX_PACKET_ENTITIES * UPDATE_BACKUP)
#define PARSE_ENTITIES_MASK     (MAX_PARSE_ENTITIES - 1)

//! Configstring Bitmap Bytes.
#define CS_BITMAP_BYTES         (MAX_CONFIGSTRINGS / 8) // 260

//! Entity State messaging flags:
typedef enum {
    MSG_ES_FORCE = ( 1 << 0 ),
    MSG_ES_NEWENTITY = ( 1 << 1 ),
    MSG_ES_FIRSTPERSON = ( 1 << 2 ),
    MSG_ES_UMASK = ( 1 << 4 ),
    MSG_ES_BEAMORIGIN = ( 1 << 5 ),
    MSG_ES_REMOVE = ( 1 << 7 )
} msgEsFlags_t;

/**
*   Server to Client, and Client to Server CommandMessages.
**/
// Server to Client
typedef enum {
    svc_bad,

    // these are private to the client and server
    svc_nop,
    svc_disconnect,
    svc_reconnect,
    svc_sound,                  // <see code>
    svc_print,                  // [byte] id [string] null terminated string
    svc_stufftext,              // [string] stuffed into client's console buffer should be \n terminated
    svc_serverdata,             // [long] protocol ...
    svc_configstring,           // [short] [string]
    svc_spawnbaseline,
    svc_centerprint,            // [string] to put in center of the screen
    svc_download,               // [short] size [size bytes]
    svc_playerinfo,             // variable
    svc_packetentities,         // [...]
    svc_deltapacketentities,    // [...]
    svc_frame,

    svc_zpacket,
    svc_zdownload,
    svc_gamestate,
    svc_configstringstream,
    svc_baselinestream,
    svc_setting,


    // These are also known to the game dlls, however, also needed by the engine.
    svc_muzzleflash,
    svc_muzzleflash2,
    svc_temp_entity,
    svc_layout,
    svc_inventory,

    svc_svgame                 // The server game is allowed to add custom commands after this. Max limit is a byte, 255.
} server_command_t;

// Client to Server
typedef enum {
    clc_bad,                // Bad command.
    clc_nop,                // 'No' command.

    clc_move,               // [usercmd_t]
    clc_move_nodelta,		// [usercmd_t]
    clc_move_batched,		// [batched_usercmd_t]

    clc_userinfo,           // [userinfo string]
    clc_userinfo_delta,		// [userinfo_key][userinfo_value]

    clc_stringcmd,          // [string] message

    clc_clgame                 // The client game is allowed to add custom commands after this. Max limit is a byte, 255.
} client_command_t;



/**
*
*	Entity Events:
*
**/
/**
*	@description entity_state_t->event values
*				 ertity events are for effects that take place reletive
*				 to an existing entities origin.  Very network efficient.
*				 All muzzle flashes really should be converted to events...
**/
typedef enum {
    EV_NONE = 0,
    EV_ITEM_RESPAWN,
    EV_FOOTSTEP,
    EV_FALLSHORT,
    EV_FALL,
    EV_FALLFAR,
    EV_PLAYER_TELEPORT,
    EV_OTHER_TELEPORT
} entity_event_t;



/**
*
*	Entity Types:
*
**/
/**
*	@description	Determines the actual entity type, in order to allow for appropriate state transmission
*					and efficient client-side handling. (ET_SPOTLIGHT demands different data than 'generic' entities.
*/
typedef enum {
	ET_ENGINE_TYPES = 0,
	ET_GENERIC = ET_ENGINE_TYPES,
	ET_SPOTLIGHT,

	ET_GAME_TYPES,

	// TODO: Game Types.
	//ET_PLAYER,
	//ET_CORPSE,
	//ET_BEAM,
	//ET_PORTALSURFACE,
	//ET_PUSH_TRIGGER,

	//ET_GIB,         // leave a trail
	//ET_BLASTER,     // redlight + trail
	//ET_ELECTRO_WEAK,
	//ET_ROCKET,      // redlight + trail
	//ET_GRENADE,
	//ET_PLASMA,

	//ET_SPRITE,

	//ET_ITEM,        // for simple items
	//ET_LASERBEAM,   // for continuous beams
	////ET_CURVELASERBEAM, // for curved beams

	//ET_PARTICLES,

	//ET_MONSTER_PLAYER,
	//ET_MONSTER_CORPSE,

	// eventual entities: types below this will get event treatment
	//ET_EVENT = EVENT_ENTITIES_START,
	//ET_SOUNDEVENT,

	//ET_TOTAL_TYPES, // current count
	MAX_ENTITY_TYPES = 128
} entity_type_t;



/**
*
*	Entity State:
*
**/
// entity_state_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
typedef struct entity_state_s {
    //! Server edict index.
	int32_t	number;

    //! The specific type of entity.
	int32_t	entityType; // ET_GENERIC, ET_PLAYER, ET_MONSTER_PLAYER, ET_SPOTLIGHT etc..
	
    //! Entity origin.
	vec3_t  origin;
    //! Entity 'view' angles.
    vec3_t  angles;
    //! For lerping.
    vec3_t  old_origin;
	
    //! The following fields (solid,clipmask,owner) are for client side prediction.
    //! gi.linkentity sets these properly.
    uint32_t solid;
    //! Clipmask for collision.
    contents_t clipmask;
    //! Entity who owns this entity.
    int32_t ownerNumber;

    //! Main entity model.
	int32_t	modelindex;
    //! Used for weapons, CTF flags, etc
	int32_t	modelindex2, modelindex3, modelindex4;
    //! Skinnumber, in case of clients, packs model index and weapon model.
    int32_t	skinnum;
    //! Render Effect Flags: RF_NOSHADOW etc.
	int32_t	renderfx;
    //! General Effect Flags: EF_ROTATE etc.
    uint32_t effects;

    //! Current animation frame.
	int32_t	frame;
    //! [Paril-KEX] for custom interpolation stuff
    int32_t old_frame;   //! Old animation frame.

	int32_t	sound;  //! For looping sounds, to guarantee shutoff
	int32_t	event;  //! Impulse events -- muzzle flashes, footsteps, etc.
					//! Events only go out for a single frame, and they are automatically cleared after that.

    //
	//! Spotlights
    //
    //! RGB Color of spotlight.
	vec3_t rgb;
    //! Intensity of the spotlight.
	float intensity;
    //! Angle width.
	float angle_width;
    //! Angle falloff.
	float angle_falloff;
} entity_state_t;



/**
*
*	Player State:
*
**/
// player_state_t is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 40(since we're running 40hz) player_state_t sent each second,
// but the number of pmove_state_t changes will be reletive to client
// frame rates
typedef struct {
    pmove_state_t   pmove;      // for prediction

    // these fields do not need to be communicated bit-precise
    vec3_t viewangles;		// for fixed views
    vec3_t viewoffset;		// add to pmovestate->origin
    vec3_t kick_angles;		// add to view direction to get render angles
							// set by weapon kicks, pain effects, etc

    vec3_t gunangles;
    vec3_t gunoffset;
    uint32_t gunindex;
    uint32_t gunframe;

	int32_t gunrate;

    //float damage_blend[ 4 ];      // rgba full screen damage blend effect
    float screen_blend[4];          // rgba full screen general blend effect
    float fov;            // horizontal field of view

    int32_t rdflags;        // refdef flags

    int32_t stats[MAX_STATS];       // fast status bar updates
} player_state_t;


// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
};
#endif

#endif // SHARED_H
