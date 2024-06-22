/********************************************************************
*
*
*	SharedGame: Shared
*
*
********************************************************************/
#pragma once

#include "shared/shared.h"
#include "shared/util_list.h"

// Include needed shared refresh types.
#include "refresh/shared_types.h"

//! Define the entity type based on from which game module we're compiling.
#ifdef CLGAME_INCLUDE
typedef struct centity_s sgentity_s;
#endif
#ifdef SVGAME_INCLUDE
typedef struct edict_s sgentity_s;
#endif

#include "sg_time.h"

// Include other shared game headers.
#include "sg_cmd_messages.h"
#include "sg_entity_effects.h"
#include "sg_entity_events.h"
#include "sg_entity_types.h"
#include "sg_gamemode.h"
#include "sg_misc.h"
#include "sg_muzzleflashes.h"
#include "sg_muzzleflashes_monsters.h"
#include "sg_pmove.h"
#include "sg_pmove_slidemove.h"
#include "sg_skm_modelinfo.h"
#include "sg_skm_rootmotion.h"
#include "sg_tempentity_events.h"



/*****************************************************************************************************
*
* 
* 
*	General Game API that is implemented in /sharedgame/game_bindings. Each game module will compile
*	with the specific binding to implement these functions.
* 
* 
* 
*****************************************************************************************************/
/**
*
*	Core:
*
**/
/**
*	@brief	Wrapper for using the appropriate developer print for the specific game module we're building.
**/
void SG_DPrintf( const char *fmt, ... );



/**
*
*	Entities:
*
**/
/**
*	@brief	Returns the entity number, -1 if invalid(nullptr, or out of bounds).
**/
const int32_t SG_GetEntityNumber( sgentity_s *sgent );



/**
*
*	ConfigStrings:
*
**/
/**
*	@brief	Returns the given configstring that sits at index.
**/
configstring_t *SG_GetConfigString( const int32_t configStringIndex );



/**
*
*	CVars:
*
**/
/**
*	@brief	Wraps around CVar_Get
**/
cvar_t *SG_CVar_Get( const char *var_name, const char *value, const int32_t flags );



/**
*
*
*	FileSystem:
*
*
**/
/**
*	@brief	Returns non 0 in case of existance.
**/
const int32_t SG_FS_FileExistsEx( const char *path, const uint32_t flags );
/**
*	@brief	Loads file into designated buffer. A nul buffer will return the file length without loading.
*	@return	length < 0 indicates error.
**/
const int32_t SG_FS_LoadFile( const char *path, void **buffer );



/**
*
*	Skeletal Model:
*
**/
/**
*	@brief
**/
const sg_skm_rootmotion_set_t SG_SKM_GenerateRootMotionSet( const model_t *skm, const int32_t rootBoneID, const int32_t axisFlags );




/**
*
*	Zone (Tag-)Malloc:
*
**/
/**
*	@brief
**/
void *SG_Z_TagMalloc( const uint32_t size, const uint32_t tag );
/**
*	@brief
**/
void SG_Z_TagFree( void *block );
/**
*	@brief	FreeTags
**/
void SG_Z_TagFree( const uint32_t tag );



/**
*
* 
* 
*	General SharedGame API functions, non game module specific.
*
* 
* 
**/
/**
*	@brief	Adds and averages out the blend[4] RGBA color on top of the already existing one in *v_blend.
**/
inline static void SG_AddBlend( const float &r, const float &g, const float &b, const float &a, float *v_blend ) {
	float   a2, a3;

	if ( a <= 0 )
		return;
	a2 = v_blend[ 3 ] + ( 1 - v_blend[ 3 ] ) * a; // new total alpha
	a3 = v_blend[ 3 ] / a2;   // fraction of color from old

	v_blend[ 0 ] = v_blend[ 0 ] * a3 + r * ( 1 - a3 );
	v_blend[ 1 ] = v_blend[ 1 ] * a3 + g * ( 1 - a3 );
	v_blend[ 2 ] = v_blend[ 2 ] * a3 + b * ( 1 - a3 );
	v_blend[ 3 ] = a2;
}

/**
*	@return	A string path to the designated resource file which is randomly
*			selected within ranges min-/+max.
* 
*	@remarks	Do not use the '.'(dot) for the extension parameter!
**/
inline static const std::string SG_RandomResourcePath( const char *path, const char *extension, const int32_t min, const int32_t max ) {
	// Generate the random file index.
	const int32_t index = irandom( min, max ) + 1;
	// Fill the buffer with the final resulting path.
	char pathBuffer[ MAX_OSPATH ] = {};
	if ( index < 10 ) {
		Q_scnprintf( pathBuffer, MAX_OSPATH, "%s0%i.%s", path, index, extension );
	} else {
		Q_scnprintf( pathBuffer, MAX_OSPATH, "%s%i.%s", path, index, extension );
	}
	// Return the buffer as std::string.
	return pathBuffer;
}



/**
*
*
* 
*	Player State Game 'Stats':
* 
* 
* 
**/
static constexpr int32_t STAT_HEALTH_ICON = ( STATS_GAME_OFFSET );
static constexpr int32_t STAT_AMMO_ICON = ( STATS_GAME_OFFSET + 1 );
static constexpr int32_t STAT_AMMO = ( STATS_GAME_OFFSET + 2 );
static constexpr int32_t STAT_ARMOR_ICON = ( STATS_GAME_OFFSET + 3 );
static constexpr int32_t STAT_ARMOR = ( STATS_GAME_OFFSET + 4 );
static constexpr int32_t STAT_SELECTED_ICON = ( STATS_GAME_OFFSET + 5 );
static constexpr int32_t STAT_PICKUP_ICON = ( STATS_GAME_OFFSET + 6 );
static constexpr int32_t STAT_PICKUP_STRING = ( STATS_GAME_OFFSET + 7 );
static constexpr int32_t STAT_TIMER_ICON = ( STATS_GAME_OFFSET + 8 );
static constexpr int32_t STAT_TIMER = ( STATS_GAME_OFFSET + 9 );
static constexpr int32_t STAT_SELECTED_ITEM = ( STATS_GAME_OFFSET + 11 );
static constexpr int32_t STAT_FLASHES = ( STATS_GAME_OFFSET + 12 ); //! Cleared each frame, 1 == health, 2 == armor.
//! Stores whether we are a spectator or not.
static constexpr int32_t STAT_SPECTATOR = ( STATS_GAME_OFFSET + 14 );
//! Indicates who we're spectate chasing.
static constexpr int32_t STAT_CHASE = ( STATS_GAME_OFFSET + 13 );

//! 2nd timer.
static constexpr int32_t STAT_TIMER2_ICON = ( STATS_GAME_OFFSET + 15 );
static constexpr int32_t STAT_TIMER2 = ( STATS_GAME_OFFSET + 16 );

//! Yaw pointing to whichever caused death.
static constexpr int32_t STAT_KILLER_YAW = ( STATS_GAME_OFFSET + 17 );

// TODO: WID: Resort all Stats logically, however, this means readjusting the
// layout strings. Not in a mood for this right now.

//! ItemID for client needs.
static constexpr int32_t STAT_WEAPON_ITEM = ( STATS_GAME_OFFSET + 18 );
//! Amount of ammo left in the weapon's clip.
static constexpr int32_t STAT_WEAPON_CLIP_AMMO = ( STATS_GAME_OFFSET + 19 );
//! The icon of the clip ammo to display.
static constexpr int32_t STAT_WEAPON_CLIP_AMMO_ICON = ( STATS_GAME_OFFSET + 20 );
//! Current recoil of the weapon, used to determine crosshair size with.
static constexpr int32_t STAT_WEAPON_RECOIL = ( STATS_GAME_OFFSET + 21 );

/**
*	@brief	Flags of the client's active weapon's st ate.
**/
static constexpr int32_t STAT_WEAPON_FLAGS = ( STATS_GAME_OFFSET + 22 );
//! Indicates that the client has engaged, and is engaging into 'precision aim' mode.
static constexpr int32_t STAT_WEAPON_FLAGS_IS_AIMING = BIT( 0 );