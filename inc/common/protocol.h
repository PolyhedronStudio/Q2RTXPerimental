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

#ifndef PROTOCOL_H
#define PROTOCOL_H


// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif


//
// protocol.h -- communications protocols
//

#define MAX_MSGLEN  0x10000  // max length of a message, 64k

#define PROTOCOL_VERSION_OLD        26
// WID: net-code: This is the 'DEFAULT' protocol code, renamed the definition to Q2RTXPERIMENTAL. 
// TODO: Change the number later on. For now we want to work, and edit, the default protocol routes.
#define PROTOCOL_VERSION_Q2RTXPERIMENTAL    34
#define PROTOCOL_VERSION_R1Q2       35
#define PROTOCOL_VERSION_Q2PRO      36
#define PROTOCOL_VERSION_MVD        37 // not used for UDP connections

#define PROTOCOL_VERSION_R1Q2_MINIMUM           1903    // b6377
#define PROTOCOL_VERSION_R1Q2_UCMD              1904    // b7387
#define PROTOCOL_VERSION_R1Q2_LONG_SOLID        1905    // b7759
#define PROTOCOL_VERSION_R1Q2_CURRENT           1905    // b7759

#define PROTOCOL_VERSION_Q2PRO_MINIMUM          1011    // r161
#define PROTOCOL_VERSION_Q2PRO_UCMD             1012    // r179
#define PROTOCOL_VERSION_Q2PRO_CLIENTNUM_FIX    1013    // r226
#define PROTOCOL_VERSION_Q2PRO_LONG_SOLID       1014    // r243
#define PROTOCOL_VERSION_Q2PRO_WATERJUMP_HACK   1015    // r335
#define PROTOCOL_VERSION_Q2PRO_RESERVED         1016    // r364
#define PROTOCOL_VERSION_Q2PRO_BEAM_ORIGIN      1017    // r1037-8
#define PROTOCOL_VERSION_Q2PRO_SHORT_ANGLES     1018    // r1037-44
#define PROTOCOL_VERSION_Q2PRO_SERVER_STATE     1019    // r1302
#define PROTOCOL_VERSION_Q2PRO_EXTENDED_LAYOUT  1020    // r1354
#define PROTOCOL_VERSION_Q2PRO_ZLIB_DOWNLOADS   1021    // r1358
#define PROTOCOL_VERSION_Q2PRO_CLIENTNUM_SHORT  1022    // r2161
#define PROTOCOL_VERSION_Q2PRO_CINEMATICS       1023    // r2263
#define PROTOCOL_VERSION_Q2PRO_CURRENT          1023    // r2263

#define PROTOCOL_VERSION_MVD_MINIMUM            2009    // r168
#define PROTOCOL_VERSION_MVD_CURRENT            2010    // r177

#define R1Q2_SUPPORTED(x) \
    ((x) >= PROTOCOL_VERSION_R1Q2_MINIMUM && \
     (x) <= PROTOCOL_VERSION_R1Q2_CURRENT)

#define Q2PRO_SUPPORTED(x) \
    ((x) >= PROTOCOL_VERSION_Q2PRO_MINIMUM && \
     (x) <= PROTOCOL_VERSION_Q2PRO_CURRENT)

#define MVD_SUPPORTED(x) \
    ((x) >= PROTOCOL_VERSION_MVD_MINIMUM && \
     (x) <= PROTOCOL_VERSION_MVD_CURRENT)

#define VALIDATE_CLIENTNUM(x) \
    ((x) >= -1 && (x) < MAX_EDICTS - 1)

//=========================================

#define UPDATE_BACKUP   128 //16	// copies of entity_state_t to keep buffered
									// must be power of two
#define UPDATE_MASK     (UPDATE_BACKUP - 1)

#define CMD_BACKUP      512 //128	// allow a lot of command backups for very fast systems
									// increased from 64
#define CMD_MASK        (CMD_BACKUP - 1)


#define SVCMD_BITS              5
#define SVCMD_MASK              ((1 << SVCMD_BITS) - 1)

//#define FRAMENUM_BITS           27
//#define FRAMENUM_MASK           ((1 << FRAMENUM_BITS) - 1)

//#define SUPPRESSCOUNT_BITS      4
//#define SUPPRESSCOUNT_MASK      ((1 << SUPPRESSCOUNT_BITS) - 1)

#define MAX_PACKET_ENTITIES     512
#define MAX_PARSE_ENTITIES      (MAX_PACKET_ENTITIES * UPDATE_BACKUP)
#define PARSE_ENTITIES_MASK     (MAX_PARSE_ENTITIES - 1)

#define MAX_PACKET_USERCMDS     32
#define MAX_PACKET_FRAMES       4

#define MAX_PACKET_STRINGCMDS   8
#define MAX_PACKET_USERINFOS    8

#define CS_BITMAP_BYTES         (MAX_CONFIGSTRINGS / 8) // 260
#define CS_BITMAP_LONGS         (CS_BITMAP_BYTES / 4)

#define MVD_MAGIC               MakeRawLong('M','V','D','2')

//
// server to client
//
typedef enum {
    svc_bad,

    // these ops are known to the game dll
    svc_muzzleflash,
    svc_muzzleflash2,
    svc_temp_entity,
    svc_layout,
    svc_inventory,

    // the rest are private to the client and server
    svc_nop,
    svc_disconnect,
    svc_reconnect,
    svc_sound,                  // <see code>
    svc_print,                  // [byte] id [string] null terminated string
    svc_stufftext,              // [string] stuffed into client's console buffer
                                // should be \n terminated
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
	
	svc_num_types
} svc_ops_t;

//==============================================

//
// client to server
//
typedef enum {
    clc_bad,
    clc_nop,
    clc_move,               // [usercmd_t]
    clc_userinfo,           // [userinfo string]
    clc_stringcmd,          // [string] message

    // q2pro specific operations
    clc_userinfo_delta
} clc_ops_t;

//==============================================

// player_state_t communication

#define PS_M_TYPE           (1<<0)
#define PS_M_ORIGIN         (1<<1)
#define PS_M_VELOCITY       (1<<2)
#define PS_M_TIME           (1<<3)
#define PS_M_FLAGS          (1<<4)
#define PS_M_GRAVITY        (1<<5)
#define PS_M_DELTA_ANGLES   (1<<6)

#define PS_VIEWOFFSET       (1<<7)
#define PS_VIEWANGLES       (1<<8)
#define PS_KICKANGLES       (1<<9)
#define PS_BLEND            (1<<10)
#define PS_FOV              (1<<11)
#define PS_WEAPONINDEX      (1<<12)
#define PS_WEAPONFRAME      (1<<13)
#define PS_RDFLAGS          (1<<14)
#define PS_WEAPONRATE       (1<<15)

#define PS_BITS             16
#define PS_MASK             ((1<<PS_BITS)-1)

//==============================================

// user_cmd_t communication

// ms and light always sent, the others are optional
#define CM_ANGLE1   (1<<0)
#define CM_ANGLE2   (1<<1)
#define CM_ANGLE3   (1<<2)
#define CM_FORWARD  (1<<3)
#define CM_SIDE     (1<<4)
#define CM_UP       (1<<5)
#define CM_BUTTONS  (1<<6)
#define CM_IMPULSE  (1<<7)

// r1q2 button byte hacks
#define BUTTON_MASK     (BUTTON_ATTACK|BUTTON_USE|BUTTON_ANY)
#define BUTTON_FORWARD  4
#define BUTTON_SIDE     8
#define BUTTON_UP       16
#define BUTTON_ANGLE1   32
#define BUTTON_ANGLE2   64

//==============================================

// a sound without an ent or pos will be a local only sound
#define SND_VOLUME          (1<<0)  // a byte
#define SND_ATTENUATION     (1<<1)  // a byte
#define SND_POS             (1<<2)  // three coordinates
#define SND_ENT             (1<<3)  // a short 0-2: channel, 3-12: entity
#define SND_OFFSET          (1<<4)  // a byte, msec offset from frame start
#define SND_INDEX16         (1<<5)  // index is 16-bit

#define DEFAULT_SOUND_PACKET_VOLUME         1.0f
#define DEFAULT_SOUND_PACKET_ATTENUATION    1.0f

//==============================================

// entity_state_t communication

// try to pack the common update flags into the first byte
#define U_ORIGIN1   (1<<0)
#define U_ORIGIN2   (1<<1)
#define U_ANGLE2    (1<<2)
#define U_ANGLE3    (1<<3)
#define U_FRAME8    (1<<4)			// frame is a byte
#define U_EVENT     (1<<5)
#define U_REMOVE    (1<<6)			// REMOVE this entity, don't add it
#define U_UNUSED1   (1<<7)			//#define U_MOREBITS1 (1<<7)        // read one additional byte


// second byte
#define U_NUMBER16  (1<<8)			// NUMBER8 is implicit if not set
#define U_ORIGIN3   (1<<9)
#define U_ANGLE1    (1<<10)
#define U_MODEL     (1<<11)
#define U_RENDERFX8 (1<<12)			// fullbright, etc
#define U_ANGLE16   (1<<13)
#define U_EFFECTS8  (1<<14)			// autorotate, trails, etc
#define U_UNUSED2	(1<<15)			//#define U_MOREBITS2 (1<<15)        // read one additional byte

// third byte
#define U_SKIN8         (1<<16)
#define U_FRAME16       (1<<17)     // frame is a short
#define U_RENDERFX16    (1<<18)     // 8 + 16 = 32
#define U_EFFECTS16     (1<<19)     // 8 + 16 = 32
#define U_MODEL2        (1<<20)     // weapons, flags, etc
#define U_MODEL3        (1<<21)
#define U_MODEL4        (1<<22)
#define U_UNUSED3		(1<<23)		//#define U_MOREBITS3     (1<<23)     // read one additional byte

// fourth byte
#define U_OLDORIGIN     (1<<24)     // FIXME: get rid of this
#define U_SKIN16        (1<<25)
#define U_SOUND8        (1<<26)
#define U_SOLID         (1<<27)
#define U_SOUND16		(1<<28)
#define U_UNUSED5		(1<<29)
#define U_UNUSED6		(1<<30)
#define U_UNUSED7		(1<<31)

// fifth byte
#define U_UNUSED8		(1<<32)
#define U_UNUSED9		(1<<33)
#define U_UNUSED10		(1<<34)
#define U_UNUSED11		(1<<35)
#define U_UNUSED12		(1<<36)
#define U_UNUSED13		(1<<37)
#define U_UNUSED14		(1<<38)
#define U_UNUSED15		(1<<39)

// sixth byte
#define U_UNUSED16		(1<<40)
#define U_UNUSED17		(1<<41)
#define U_UNUSED18		(1<<42)
#define U_UNUSED19		(1<<43)
#define U_UNUSED20		(1<<44)
#define U_UNUSED21		(1<<45)
#define U_UNUSED22		(1<<46)
#define U_UNUSED23		(1<<47)

// seventh byte
#define U_UNUSED24		(1<<48)
#define U_UNUSED25		(1<<49)
#define U_UNUSED26		(1<<50)
#define U_UNUSED27		(1<<51)
#define U_UNUSED28		(1<<52)
#define U_UNUSED29		(1<<53)
#define U_UNUSED30		(1<<54)
#define U_UNUSED31		(1<<55)

// eigth byte
#define U_UNUSED32		(1<<56)
#define U_UNUSED33		(1<<57)
#define U_UNUSED34		(1<<58)
#define U_UNUSED35		(1<<59)
#define U_UNUSED36		(1<<60)
#define U_UNUSED37		(1<<61)
#define U_UNUSED38		(1<<62)
#define U_UNUSED39		(1<<63)

// ==============================================================

#define CLIENTNUM_NONE        (MAX_CLIENTS - 1)
#define CLIENTNUM_RESERVED    (MAX_CLIENTS - 1)

// a SOLID_BBOX will never create this value
#define PACKED_BSP      255

// q2pro frame flags sent by the server
// only SUPPRESSCOUNT_BITS can be used
#define FF_SUPPRESSED   (1<<0)
#define FF_CLIENTDROP   (1<<1)
#define FF_CLIENTPRED   (1<<2)
#define FF_RESERVED     (1<<3)

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
};
#endif

#endif // PROTOCOL_H
