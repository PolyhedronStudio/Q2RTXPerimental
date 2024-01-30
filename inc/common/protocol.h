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

//! Max length of a message, 64k
#define MAX_MSGLEN 0x10000 // 65536  

//! WID: net-protocol2: This is our own new protocol.
#define PROTOCOL_VERSION_Q2RTXPERIMENTAL    1337

//! Validate the client number.
inline static const bool VALIDATE_CLIENTNUM( int32_t x ) {
	return ( ( x ) >= -1 && ( x ) < MAX_CLIENTS );
}

//=========================================

#define MAX_PACKETENTITY_BYTES 64

#define MAX_PACKET_USERCMDS     32
#define MAX_PACKET_FRAMES       4

// Malicious users may try using too many string commands
// Define a capped limit to prevent doing so.
#define MAX_PACKET_STRINGCMDS   8
// Malicious users may try sending too many userinfo updates
// Define a capped limit to prevent doing so.
#define MAX_PACKET_USERINFOS    8





// player_state_t communication
#define PS_M_TYPE           (1<<0)
#define PS_M_ORIGIN         (1<<1)
#define PS_M_VELOCITY       (1<<2)
#define PS_M_TIME           (1<<3)
#define PS_M_FLAGS          (1<<4)
#define PS_M_GRAVITY        (1<<5)
#define PS_M_DELTA_ANGLES   (1<<6)

#define PS_VIEWOFFSET       (1<<7)
#define PS_VIEWHEIGHT		(1<<8)
#define PS_VIEWANGLES       (1<<9)
#define PS_KICKANGLES       (1<<10)
#define PS_BLEND            (1<<11)
#define PS_FOV              (1<<12)
#define PS_WEAPONINDEX      (1<<13)
#define PS_WEAPONFRAME      (1<<14)
#define PS_RDFLAGS          (1<<15)
#define PS_WEAPONRATE       (1<<16)


//#define PS_BITS             16
//#define PS_MASK             ((1<<PS_BITS)-1)

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
#define U_FRAME     (1<<4)			// 
#define U_EVENT     (1<<5)
#define U_SKIN		(1<<6)
#define U_SOLID		(1<<7)


// second byte
#define U_SOUND		(1<<8)
#define U_ORIGIN3   (1<<9)
#define U_ANGLE1    (1<<10)
#define U_MODEL     (1<<11)
#define U_RENDERFX  (1<<12)			// fullbright, etc
#define U_EFFECTS	(1<<13)
#define U_OLDORIGIN	(1<<14)			// autorotate, trails, etc
#define U_MODEL2	(1<<15)

// third byte
#define U_MODEL3		(1<<16)
#define U_MODEL4		(1<<17)
#define U_ENTITY_TYPE	(1<<18)		// TODO: Move to other bit location.
#define U_SPOTLIGHT_RGB		(1<<19)
#define U_SPOTLIGHT_INTENSITY	(1<<20)
#define U_SPOTLIGHT_ANGLE_WIDTH	(1<<21)
#define U_SPOTLIGHT_ANGLE_FALLOFF (1<<22)
#define U_UNUSED46		(1<<23)

// fourth byte
#define U_CLIPMASK	(1<<24)
#define U_OWNER 	(1<<25)
#define U_OLD_FRAME	(1<<26)
#define U_UNUSED5	(1<<27)
#define U_UNUSED6	(1<<28)
#define U_UNUSED7	(1<<29)
#define U_UNUSED8	(1<<30)
#define U_UNUSED9	(1<<31)

// fifth byte
#define U_UNUSED40		(1<<32)
#define U_UNUSED41		(1<<33)
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

// a SOLID_BBOX will never create this value
#define PACKED_BSP      255

// q2pro frame flags sent by the server
// only SUPPRESSCOUNT_BITS can be used
#define FF_SUPPRESSED   (1<<0)
#define FF_CLIENTDROP   (1<<1)
#define FF_CLIENTPRED   (1<<2)	// Set but unused?
#define FF_RESERVED     (1<<3)	// Literally reserved.

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
};
#endif

#endif // PROTOCOL_H
