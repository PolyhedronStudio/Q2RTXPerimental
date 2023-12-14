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
#define MAX_EDICTS          8192    // must change protocol to increase more.
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

#include "shared/math/math.h"

/*
==============================================================

BIT UTILITIES:

==============================================================
*/
#define Q_IsBitSet(data, bit)   (((data)[(bit) >> 3] & (1 << ((bit) & 7))) != 0)
#define Q_SetBit(data, bit)     ((data)[(bit) >> 3] |= (1 << ((bit) & 7)))
#define Q_ClearBit(data, bit)   ((data)[(bit) >> 3] &= ~(1 << ((bit) & 7)))


/*
==============================================================

STRING UTILITIES:

==============================================================
*/
#include "shared/string_utilities.h"


/*
==============================================================

ENDIAN UTILITIES:

==============================================================
*/
#include "shared/endian.h"
//=============================================

/*
==============================================================

KEY / VALUE INFO STRINGS:

==============================================================
*/
#define MAX_INFO_KEY        64
#define MAX_INFO_VALUE      64
#define MAX_INFO_STRING     512

char    *Info_ValueForKey(const char *s, const char *key);
void    Info_RemoveKey(char *s, const char *key);
bool    Info_SetValueForKey(char *s, const char *key, const char *value);
bool    Info_Validate(const char *s);
size_t  Info_SubValidate(const char *s);
void    Info_NextPair(const char **string, char *key, char *value);
void    Info_Print(const char *infostring);



/*
==========================================================

CVARS (console variables) - Actually part of the /common/
code. What is defined here, is the bare necessity of limited
API for the game code to make use of.

==========================================================
*/
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

// ------ new stuff ------
    int         integer;
    char        *default_string;
    xchanged_t      changed;
    xgenerator_t    generator;
    struct cvar_s   *hashNext;
} cvar_t;

#endif      // CVAR



/*
==============================================================

COLLISION DETECTION

==============================================================
*/
// lower bits are stronger, and will eat weaker brushes completely
#define CONTENTS_SOLID          1       // an eye is never valid in a solid
#define CONTENTS_WINDOW         2       // translucent, but not watery
#define CONTENTS_AUX            4
#define CONTENTS_LAVA           8
#define CONTENTS_SLIME          16
#define CONTENTS_WATER          32
#define CONTENTS_MIST           64
#define LAST_VISIBLE_CONTENTS   64

// remaining contents are non-visible, and don't eat brushes

#define CONTENTS_AREAPORTAL     0x8000

#define CONTENTS_PLAYERCLIP     0x10000
#define CONTENTS_MONSTERCLIP    0x20000

// currents can be added to any other contents, and may be mixed
#define CONTENTS_CURRENT_0      0x40000
#define CONTENTS_CURRENT_90     0x80000
#define CONTENTS_CURRENT_180    0x100000
#define CONTENTS_CURRENT_270    0x200000
#define CONTENTS_CURRENT_UP     0x400000
#define CONTENTS_CURRENT_DOWN   0x800000

#define CONTENTS_ORIGIN         0x1000000   // removed before bsping an entity

#define CONTENTS_MONSTER        0x2000000   // should never be on a brush, only in game
#define CONTENTS_DEADMONSTER    0x4000000
#define CONTENTS_DETAIL         0x8000000   // brushes to be added after vis leafs
#define CONTENTS_TRANSLUCENT    0x10000000  // auto set if any surface has trans
#define CONTENTS_LADDER         0x20000000



#define SURF_LIGHT      0x1     // value will hold the light strength

#define SURF_SLICK      0x2     // effects game physics

#define SURF_SKY        0x4     // don't draw, but add to skybox
#define SURF_WARP       0x8     // turbulent water warp
#define SURF_TRANS33    0x10
#define SURF_TRANS66    0x20
#define SURF_FLOWING    0x40    // scroll towards angle
#define SURF_NODRAW     0x80    // don't bother referencing the texture

#define SURF_ALPHATEST  0x02000000  // used by kmquake2



// content masks
#define MASK_ALL                (-1)
#define MASK_SOLID              (CONTENTS_SOLID|CONTENTS_WINDOW)
#define MASK_PLAYERSOLID        (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_DEADSOLID          (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW)
#define MASK_MONSTERSOLID       (CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_WATER              (CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define MASK_OPAQUE             (CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define MASK_SHOT               (CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEADMONSTER)
#define MASK_CURRENT            (CONTENTS_CURRENT_0|CONTENTS_CURRENT_90|CONTENTS_CURRENT_180|CONTENTS_CURRENT_270|CONTENTS_CURRENT_UP|CONTENTS_CURRENT_DOWN)


// gi.BoxEdicts() can return a list of either solid or trigger entities
// FIXME: eliminate AREA_ distinction?
#define AREA_SOLID      1
#define AREA_TRIGGERS   2


// plane_t structure
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

typedef struct csurface_s {
    char        name[16];
    int         flags;
    int         value;
} csurface_t;

// a trace is returned when a box is swept through the world
typedef struct {
    qboolean    allsolid;   // if true, plane is not valid
    qboolean    startsolid; // if true, the initial point was in a solid area
    float       fraction;   // time completed, 1.0 = didn't hit anything
    vec3_t      endpos;     // final position
    cplane_t    plane;      // surface normal at impact
    csurface_t  *surface;   // surface hit
    int         contents;   // contents on other side of surface hit
    struct edict_s  *ent;       // not set by CM_*() functions
} trace_t;




/*
==============================================================

USER COMMANDS( User Input. ):

==============================================================
*/
//
// button bits
//
#define BUTTON_ATTACK       1
#define BUTTON_USE          2
#define BUTTON_ANY          128         // any key whatsoever


// usercmd_t is sent to the server each client frame
typedef struct usercmd_s {
	byte    msec;
	byte    buttons;
	short   angles[ 3 ];
	short   forwardmove, sidemove, upmove;
	byte    impulse;        // remove?
} usercmd_t;



/*
==============================================================

PLAYER MOVEMENT

==============================================================
*/
// pmove_state_t is the information necessary for client side movement
// prediction
typedef enum {
    // can accelerate and turn
    PM_NORMAL,
    PM_SPECTATOR,
    // no acceleration or turning
    PM_DEAD,
    PM_GIB,     // different bounding box
    PM_FREEZE
} pmtype_t;

// pmove->pm_flags
#define PMF_DUCKED          1
#define PMF_JUMP_HELD       2
#define PMF_ON_GROUND       4
#define PMF_TIME_WATERJUMP  8   // pm_time is waterjump
#define PMF_TIME_LAND       16  // pm_time is time before rejump
#define PMF_TIME_TELEPORT   32  // pm_time is non-moving time
#define PMF_NO_PREDICTION   64  // temporarily disables prediction (used for grappling hook)
#define PMF_TELEPORT_BIT    128 // used by q2pro

// this structure needs to be communicated bit-accurate
// from the server to the client to guarantee that
// prediction stays in sync, so no floats are used.
// if any part of the game code modifies this struct, it
// will result in a prediction error of some degree.
typedef struct {
    pmtype_t    pm_type;

    vec3_t		origin;//short       origin[3];      // 12.3 // WID: float-movement
    vec3_t		velocity;//short       velocity[3];    // 12.3 // WID: float-movement
    byte        pm_flags;       // ducked, jump_held, etc
    byte        pm_time;        // each unit = 8 ms
    short       gravity;
    short       delta_angles[3];    // add to command angles to get view direction
                                    // changed by spawns, rotating objects, and teleporters
} pmove_state_t;

/**
*	@brief	Used to configure player movement with, it is set by SG_ConfigurePlayerMoveParameters.
* 
*			NOTE: In the future this will change, obviously.
**/
typedef struct {
	bool        qwmode;
	bool        airaccelerate;
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

#define MAXTOUCH    32
typedef struct {
    // state (in / out)
    pmove_state_t   s;

    // command (in)
    usercmd_t       cmd;
    qboolean        snapinitial;    // if s has been changed outside pmove

    // results (out)
    int         numtouch;
    struct edict_s  *touchents[MAXTOUCH];

    vec3_t      viewangles;         // clamped
    float       viewheight;

    vec3_t      mins, maxs;         // bounding box size

    struct edict_s  *groundentity;
    int         watertype;
    int         waterlevel;

    // callbacks to test the world
    trace_t     (* q_gameabi trace)(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end);
    int         (*pointcontents)(const vec3_t point);
} pmove_t;



/*
==============================================================

MUZZLE FLASHES / PLAYER EFFECTS ETC:

==============================================================
*/


// entity_state_t->effects
// Effects are things handled on the client side (lights, particles, frame animations)
// that happen constantly on the given entity.
// An entity that has effects will be sent to the client
// even if it has a zero index model.
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

// entity_state_t->renderfx flags
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
#define RDF_UNDERWATER      1       // warp the screen as apropriate
#define RDF_NOWORLDMODEL    2       // used for player configuration screen

//ROGUE
#define RDF_IRGOGGLES       4
#define RDF_UVGOGGLES       8
//ROGUE
//
// muzzle flashes / player effects
//
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

//
// monster muzzle flashes
//
enum {
    MZ2_TANK_BLASTER_1 = 1, MZ2_TANK_BLASTER_2, MZ2_TANK_BLASTER_3,
    MZ2_TANK_MACHINEGUN_1, MZ2_TANK_MACHINEGUN_2, MZ2_TANK_MACHINEGUN_3,
    MZ2_TANK_MACHINEGUN_4, MZ2_TANK_MACHINEGUN_5, MZ2_TANK_MACHINEGUN_6,
    MZ2_TANK_MACHINEGUN_7, MZ2_TANK_MACHINEGUN_8, MZ2_TANK_MACHINEGUN_9,
    MZ2_TANK_MACHINEGUN_10, MZ2_TANK_MACHINEGUN_11, MZ2_TANK_MACHINEGUN_12,
    MZ2_TANK_MACHINEGUN_13, MZ2_TANK_MACHINEGUN_14, MZ2_TANK_MACHINEGUN_15,
    MZ2_TANK_MACHINEGUN_16, MZ2_TANK_MACHINEGUN_17, MZ2_TANK_MACHINEGUN_18,
    MZ2_TANK_MACHINEGUN_19, MZ2_TANK_ROCKET_1, MZ2_TANK_ROCKET_2,
    MZ2_TANK_ROCKET_3, MZ2_INFANTRY_MACHINEGUN_1, MZ2_INFANTRY_MACHINEGUN_2,
    MZ2_INFANTRY_MACHINEGUN_3, MZ2_INFANTRY_MACHINEGUN_4,
    MZ2_INFANTRY_MACHINEGUN_5, MZ2_INFANTRY_MACHINEGUN_6,
    MZ2_INFANTRY_MACHINEGUN_7, MZ2_INFANTRY_MACHINEGUN_8,
    MZ2_INFANTRY_MACHINEGUN_9, MZ2_INFANTRY_MACHINEGUN_10,
    MZ2_INFANTRY_MACHINEGUN_11, MZ2_INFANTRY_MACHINEGUN_12,
    MZ2_INFANTRY_MACHINEGUN_13, MZ2_SOLDIER_BLASTER_1, MZ2_SOLDIER_BLASTER_2,
    MZ2_SOLDIER_SHOTGUN_1, MZ2_SOLDIER_SHOTGUN_2, MZ2_SOLDIER_MACHINEGUN_1,
    MZ2_SOLDIER_MACHINEGUN_2, MZ2_GUNNER_MACHINEGUN_1, MZ2_GUNNER_MACHINEGUN_2,
    MZ2_GUNNER_MACHINEGUN_3, MZ2_GUNNER_MACHINEGUN_4, MZ2_GUNNER_MACHINEGUN_5,
    MZ2_GUNNER_MACHINEGUN_6, MZ2_GUNNER_MACHINEGUN_7, MZ2_GUNNER_MACHINEGUN_8,
    MZ2_GUNNER_GRENADE_1, MZ2_GUNNER_GRENADE_2, MZ2_GUNNER_GRENADE_3,
    MZ2_GUNNER_GRENADE_4, MZ2_CHICK_ROCKET_1, MZ2_FLYER_BLASTER_1,
    MZ2_FLYER_BLASTER_2, MZ2_MEDIC_BLASTER_1, MZ2_GLADIATOR_RAILGUN_1,
    MZ2_HOVER_BLASTER_1, MZ2_ACTOR_MACHINEGUN_1, MZ2_SUPERTANK_MACHINEGUN_1,
    MZ2_SUPERTANK_MACHINEGUN_2, MZ2_SUPERTANK_MACHINEGUN_3,
    MZ2_SUPERTANK_MACHINEGUN_4, MZ2_SUPERTANK_MACHINEGUN_5,
    MZ2_SUPERTANK_MACHINEGUN_6, MZ2_SUPERTANK_ROCKET_1, MZ2_SUPERTANK_ROCKET_2,
    MZ2_SUPERTANK_ROCKET_3, MZ2_BOSS2_MACHINEGUN_L1, MZ2_BOSS2_MACHINEGUN_L2,
    MZ2_BOSS2_MACHINEGUN_L3, MZ2_BOSS2_MACHINEGUN_L4, MZ2_BOSS2_MACHINEGUN_L5,
    MZ2_BOSS2_ROCKET_1, MZ2_BOSS2_ROCKET_2, MZ2_BOSS2_ROCKET_3,
    MZ2_BOSS2_ROCKET_4, MZ2_FLOAT_BLASTER_1, MZ2_SOLDIER_BLASTER_3,
    MZ2_SOLDIER_SHOTGUN_3, MZ2_SOLDIER_MACHINEGUN_3, MZ2_SOLDIER_BLASTER_4,
    MZ2_SOLDIER_SHOTGUN_4, MZ2_SOLDIER_MACHINEGUN_4, MZ2_SOLDIER_BLASTER_5,
    MZ2_SOLDIER_SHOTGUN_5, MZ2_SOLDIER_MACHINEGUN_5, MZ2_SOLDIER_BLASTER_6,
    MZ2_SOLDIER_SHOTGUN_6, MZ2_SOLDIER_MACHINEGUN_6, MZ2_SOLDIER_BLASTER_7,
    MZ2_SOLDIER_SHOTGUN_7, MZ2_SOLDIER_MACHINEGUN_7, MZ2_SOLDIER_BLASTER_8,
    MZ2_SOLDIER_SHOTGUN_8, MZ2_SOLDIER_MACHINEGUN_8,

// --- Xian shit below ---
    MZ2_MAKRON_BFG, MZ2_MAKRON_BLASTER_1, MZ2_MAKRON_BLASTER_2,
    MZ2_MAKRON_BLASTER_3, MZ2_MAKRON_BLASTER_4, MZ2_MAKRON_BLASTER_5,
    MZ2_MAKRON_BLASTER_6, MZ2_MAKRON_BLASTER_7, MZ2_MAKRON_BLASTER_8,
    MZ2_MAKRON_BLASTER_9, MZ2_MAKRON_BLASTER_10, MZ2_MAKRON_BLASTER_11,
    MZ2_MAKRON_BLASTER_12, MZ2_MAKRON_BLASTER_13, MZ2_MAKRON_BLASTER_14,
    MZ2_MAKRON_BLASTER_15, MZ2_MAKRON_BLASTER_16, MZ2_MAKRON_BLASTER_17,
    MZ2_MAKRON_RAILGUN_1, MZ2_JORG_MACHINEGUN_L1, MZ2_JORG_MACHINEGUN_L2,
    MZ2_JORG_MACHINEGUN_L3, MZ2_JORG_MACHINEGUN_L4, MZ2_JORG_MACHINEGUN_L5,
    MZ2_JORG_MACHINEGUN_L6, MZ2_JORG_MACHINEGUN_R1, MZ2_JORG_MACHINEGUN_R2,
    MZ2_JORG_MACHINEGUN_R3, MZ2_JORG_MACHINEGUN_R4, MZ2_JORG_MACHINEGUN_R5,
    MZ2_JORG_MACHINEGUN_R6, MZ2_JORG_BFG_1, MZ2_BOSS2_MACHINEGUN_R1,
    MZ2_BOSS2_MACHINEGUN_R2, MZ2_BOSS2_MACHINEGUN_R3, MZ2_BOSS2_MACHINEGUN_R4,
    MZ2_BOSS2_MACHINEGUN_R5,

//ROGUE
    MZ2_CARRIER_MACHINEGUN_L1, MZ2_CARRIER_MACHINEGUN_R1, MZ2_CARRIER_GRENADE,
    MZ2_TURRET_MACHINEGUN, MZ2_TURRET_ROCKET, MZ2_TURRET_BLASTER,
    MZ2_STALKER_BLASTER, MZ2_DAEDALUS_BLASTER, MZ2_MEDIC_BLASTER_2,
    MZ2_CARRIER_RAILGUN, MZ2_WIDOW_DISRUPTOR, MZ2_WIDOW_BLASTER,
    MZ2_WIDOW_RAIL, MZ2_WIDOW_PLASMABEAM, MZ2_CARRIER_MACHINEGUN_L2,
    MZ2_CARRIER_MACHINEGUN_R2, MZ2_WIDOW_RAIL_LEFT, MZ2_WIDOW_RAIL_RIGHT,
    MZ2_WIDOW_BLASTER_SWEEP1, MZ2_WIDOW_BLASTER_SWEEP2,
    MZ2_WIDOW_BLASTER_SWEEP3, MZ2_WIDOW_BLASTER_SWEEP4,
    MZ2_WIDOW_BLASTER_SWEEP5, MZ2_WIDOW_BLASTER_SWEEP6,
    MZ2_WIDOW_BLASTER_SWEEP7, MZ2_WIDOW_BLASTER_SWEEP8,
    MZ2_WIDOW_BLASTER_SWEEP9, MZ2_WIDOW_BLASTER_100, MZ2_WIDOW_BLASTER_90,
    MZ2_WIDOW_BLASTER_80, MZ2_WIDOW_BLASTER_70, MZ2_WIDOW_BLASTER_60,
    MZ2_WIDOW_BLASTER_50, MZ2_WIDOW_BLASTER_40, MZ2_WIDOW_BLASTER_30,
    MZ2_WIDOW_BLASTER_20, MZ2_WIDOW_BLASTER_10, MZ2_WIDOW_BLASTER_0,
    MZ2_WIDOW_BLASTER_10L, MZ2_WIDOW_BLASTER_20L, MZ2_WIDOW_BLASTER_30L,
    MZ2_WIDOW_BLASTER_40L, MZ2_WIDOW_BLASTER_50L, MZ2_WIDOW_BLASTER_60L,
    MZ2_WIDOW_BLASTER_70L, MZ2_WIDOW_RUN_1, MZ2_WIDOW_RUN_2, MZ2_WIDOW_RUN_3,
    MZ2_WIDOW_RUN_4, MZ2_WIDOW_RUN_5, MZ2_WIDOW_RUN_6, MZ2_WIDOW_RUN_7,
    MZ2_WIDOW_RUN_8, MZ2_CARRIER_ROCKET_1, MZ2_CARRIER_ROCKET_2,
    MZ2_CARRIER_ROCKET_3, MZ2_CARRIER_ROCKET_4, MZ2_WIDOW2_BEAMER_1,
    MZ2_WIDOW2_BEAMER_2, MZ2_WIDOW2_BEAMER_3, MZ2_WIDOW2_BEAMER_4,
    MZ2_WIDOW2_BEAMER_5, MZ2_WIDOW2_BEAM_SWEEP_1, MZ2_WIDOW2_BEAM_SWEEP_2,
    MZ2_WIDOW2_BEAM_SWEEP_3, MZ2_WIDOW2_BEAM_SWEEP_4, MZ2_WIDOW2_BEAM_SWEEP_5,
    MZ2_WIDOW2_BEAM_SWEEP_6, MZ2_WIDOW2_BEAM_SWEEP_7, MZ2_WIDOW2_BEAM_SWEEP_8,
    MZ2_WIDOW2_BEAM_SWEEP_9, MZ2_WIDOW2_BEAM_SWEEP_10,
    MZ2_WIDOW2_BEAM_SWEEP_11,
//ROGUE
};

extern const vec3_t monster_flash_offset[256];



/*
==============================================================

TEMP ENTITY EVENTS:

==============================================================
*/
// temp entity events
//
// Temp entity events are for things that happen
// at a location seperate from any existing entity.
// Temporary entity messages are explicitly constructed
// and broadcast.
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



/*
==============================================================

GAMEMODE FLAGS(TODO: Actually make it game mode related.)

==============================================================
*/
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



/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/
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
* 
*	Config Strings are a general means of communication from the server to all connected clients.
*	Each config string can be at most MAX_QPATH characters.
* 
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



/*
==============================================================

ENTITY EVENTS:

==============================================================
*/
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



/*
==============================================================

ENTITY TYPES:

==============================================================
*/
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



/*
==============================================================

ENTITY STATE:

==============================================================
*/
// entity_state_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
typedef struct entity_state_s {
	int32_t	number;         // edict index

	int32_t	entityType;		// ET_GENERIC, ET_PLAYER, ET_MONSTER_PLAYER, ET_SPOTLIGHT etc..
	
	vec3_t  origin;
    vec3_t  angles;
    vec3_t  old_origin;     // for lerping
	
	uint32_t solid;		// WID: upgr-solid: Now is uint32_t.
						// For client side prediction, 8*(bits 0-4) is x/y radius
						// 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
						// gi.linkentity sets this properly

	int32_t	modelindex;		// Main model.
	int32_t	modelindex2, modelindex3, modelindex4;  // Used for weapons, CTF flags, etc
	int32_t	renderfx;	// Render Effect Flags: RF_NOSHADOW etc.

	int32_t	frame;
    int32_t	skinnum;
	uint32_t effects;	// General Effect Flags: EF_ROTATE etc.

	int32_t	sound;	// For looping sounds, to guarantee shutoff
	int32_t	event;	// Impulse events -- muzzle flashes, footsteps, etc.
					// Events only go out for a single frame, and they are automatically cleared after that.
							
	// [Paril-KEX] for custom interpolation stuff
	int32_t        old_frame;


	// Spotlights
	vec3_t rgb;

	float intensity;

	float angle_width;
	float angle_falloff;

} entity_state_t;



/*
==============================================================

PLAYER STATE:

==============================================================
*/
// player_state_t is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 40(since we're running 40hz) player_state_t sent each second,
// but the number of pmove_state_t changes will be reletive to client
// frame rates
typedef struct {
    pmove_state_t   pmove;      // for prediction

    // these fields do not need to be communicated bit-precise

    vec3_t viewangles;     // for fixed views
    vec3_t viewoffset;     // add to pmovestate->origin
    vec3_t kick_angles;    // add to view direction to get render angles
                                // set by weapon kicks, pain effects, etc

    vec3_t gunangles;
    vec3_t gunoffset;
    uint32_t gunindex;
    uint32_t gunframe;
// WID: 40hz.
	int32_t gunrate;
// WID: 40hz.

    float blend[4];       // rgba full screen effect
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
