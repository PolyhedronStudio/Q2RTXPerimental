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

#define MAX_PACKETENTITY_BYTES	64

#define MAX_PACKET_USERCMDS     32
#define MAX_PACKET_FRAMES       4

// Malicious users may try using too many string commands
// Define a capped limit to prevent doing so.
#define MAX_PACKET_STRINGCMDS   8
// Malicious users may try sending too many userinfo updates
// Define a capped limit to prevent doing so.
#define MAX_PACKET_USERINFOS    8





// player_state_t communication
#define PS_M_TYPE				BIT_ULL( 0 )
#define PS_M_ORIGIN				BIT_ULL( 1 )
#define PS_M_VELOCITY			BIT_ULL( 2 )
#define PS_M_SPEED				BIT_ULL( 3 )
#define PS_M_TIME				BIT_ULL( 4 )
#define PS_M_FLAGS				BIT_ULL( 5 )
#define PS_M_DELTA_ANGLES		BIT_ULL( 6 )
#define PS_M_VIEWHEIGHT			BIT_ULL( 7 )

#define PS_BOB_CYCLE			BIT_ULL( 8 )

#define PS_EVENT_SEQUENCE		BIT_ULL( 9 )
#define PS_EVENT_FIRST			BIT_ULL( 10 )
#define PS_EVENT_FIRST_PARM		BIT_ULL( 11 )
#define PS_EVENT_SECOND			BIT_ULL( 12 )
#define PS_EVENT_SECOND_PARM	BIT_ULL( 13 )

#define PS_GUN_ANIMATION	BIT_ULL( 14 )
#define PS_GUN_MODELINDEX	BIT_ULL( 15 )

#define PS_VIEWANGLES       BIT_ULL( 16 )
#define PS_KICKANGLES       BIT_ULL( 17 )
#define PS_VIEWOFFSET       BIT_ULL( 18 )

#define PS_EXTERNAL_EVENT		BIT_ULL( 19 )
#define PS_EXTERNAL_EVENT_PARM0	BIT_ULL( 20 )
//#define PS_EXTERNAL_EVENT_PARM1	BIT_ULL( 20 )

#define PS_BLEND            BIT_ULL( 21 )
#define PS_FOV				BIT_ULL( 22 )
#define PS_RDFLAGS			BIT_ULL( 23 )

#define PS_M_GRAVITY		BIT_ULL( 24 )
//#define PS_CLIENT_NUMBER	BIT_ULL( 25 )

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
#define U_ORIGIN1				BIT_ULL(  0 )
#define U_ORIGIN2				BIT_ULL(  1 )
#define U_ORIGIN3				BIT_ULL(  2 )
#define U_ANGLE1				BIT_ULL(  3 )
#define U_ANGLE2				BIT_ULL(  4 )
#define U_ANGLE3				BIT_ULL(  5 )
#define U_ENTITY_TYPE			BIT_ULL(  6 )
#define U_OTHER_ENTITY_NUMBER	BIT_ULL(  7 )

// second byte
#define U_EVENT				BIT_ULL(  8 )
#define U_EVENT_PARM_0		BIT_ULL(  9 )
#define U_EVENT_PARM_1		BIT_ULL(  10 )
#define U_FRAME				BIT_ULL(  11 )
#define U_MODEL				BIT_ULL( 12 )
#define U_EFFECTS			BIT_ULL( 13 )	
#define U_RENDERFX			BIT_ULL( 14 )	// fullbright, etc
#define U_SOLID				BIT_ULL( 15 )

// third byte
#define U_SOUND				BIT_ULL(  16 )
#define U_SKIN			BIT_ULL( 17 )
#define U_OLDORIGIN		BIT_ULL( 18 )	// autorotate, trails, etc
#define U_MODEL2		BIT_ULL( 19 )
#define U_MODEL3		BIT_ULL( 20 )
#define U_MODEL4		BIT_ULL( 21 )
// TODO: Move to other bit location.
#define U_SPOTLIGHT_RGB				BIT_ULL( 22 )
#define U_SPOTLIGHT_INTENSITY		BIT_ULL( 23 )
#define U_SPOTLIGHT_ANGLE_WIDTH		BIT_ULL( 24 )
#define U_SPOTLIGHT_ANGLE_FALLOFF	BIT_ULL( 25 )
#define U_CLIPMASK					BIT_ULL( 26 )

// fourth byte
#define U_HULL_CONTENTS	BIT_ULL( 27 )
#define U_OWNER 		BIT_ULL( 28 )
#define U_BOUNDINGBOX	BIT_ULL( 29 )
#define U_UNUSED0		BIT_ULL( 29 )
#define U_UNUSED1		BIT_ULL( 30 )
#define U_UNUSED2		BIT_ULL( 31 )
#define U_UNUSED3		BIT_ULL( 32 )
#define U_UNUSED4		BIT_ULL( 33 )

// fifth byte
#define U_UNUSED5		BIT_ULL(34 )
#define U_UNUSED6		BIT_ULL(35 )
#define U_UNUSED7		BIT_ULL(36 )
#define U_UNUSED8		BIT_ULL(37 )
#define U_UNUSED9		BIT_ULL(38 )
#define U_UNUSED10		BIT_ULL(39 )
#define U_UNUSED11		BIT_ULL(40 )
#define U_UNUSED12		BIT_ULL(41 )

// sixth byte
#define U_UNUSED13		BIT_ULL(42 )
#define U_UNUSED14		BIT_ULL(43 )
#define U_UNUSED15		BIT_ULL(44 )
#define U_UNUSED16		BIT_ULL(45 )
#define U_UNUSED17		BIT_ULL(46 )
#define U_UNUSED18		BIT_ULL(47 )
#define U_UNUSED19		BIT_ULL(48 )
#define U_UNUSED20		BIT_ULL(49 )

// seventh byte
#define U_UNUSED21		BIT_ULL(50 )
#define U_UNUSED22		BIT_ULL(51 )
#define U_UNUSED23		BIT_ULL(52 )
#define U_UNUSED24		BIT_ULL(53 )
#define U_UNUSED25		BIT_ULL(54 )
#define U_UNUSED26		BIT_ULL(55 )
#define U_UNUSED27		BIT_ULL(56 )
#define U_UNUSED28		BIT_ULL(57 )

// eigth byte
#define U_UNUSED29		BIT_ULL(58 )
#define U_UNUSED30		BIT_ULL(59 )
#define U_UNUSED31		BIT_ULL(60 )
#define U_UNUSED32		BIT_ULL(61 )
#define U_UNUSED33		BIT_ULL(62 )
#define U_UNUSED34		BIT_ULL(63 )

// ==============================================================

// WID: Moved to shared/shared.h
// a SOLID_BOUNDS_BOX will never create this value
//#define PACKED_BSP      255

// q2pro frame flags sent by the server
// only SUPPRESSCOUNT_BITS can be used
// Moved to shared.h
//#define FF_NONE			0
//#define FF_SUPPRESSED   (1<<0)
//#define FF_CLIENTDROP   (1<<1)
//#define FF_CLIENTPRED   (1<<2)	// Set but unused?
//#define FF_RESERVED     (1<<3)	// Literally reserved.



