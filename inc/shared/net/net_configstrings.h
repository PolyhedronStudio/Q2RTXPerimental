/********************************************************************
*
*
*   Configuration Strings:
*
*
********************************************************************/
#pragma once

/***
* 	Config Strings are a general means of communication from the server to all connected clients.
*	Each config string can be at most MAX_CS_STRING_LENGTH characters.
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

// Some mods actually exploit CS_STATUSBAR to take space up to CS_AIRACCEL
static inline int32_t CS_SIZE( int32_t cs ) {
	return ( ( cs ) >= CS_STATUSBAR && ( cs ) < CS_AIRACCEL ? \
		MAX_CS_STRING_LENGTH * ( CS_AIRACCEL - ( cs ) ) : MAX_CS_STRING_LENGTH );
}