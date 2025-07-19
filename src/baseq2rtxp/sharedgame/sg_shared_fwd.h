/********************************************************************
*
*
*	SharedGame: Shared
*
*
********************************************************************/
#pragma once

// Include needed shared refresh types.
#include "refresh/shared_types.h"

//! Define the entity type based on from which game module we're compiling.
#ifdef CLGAME_INCLUDE
	//! Game Module Name STR.
	#define SG_GAME_MODULE_STR "CLGame"
	//! Entity type.
	typedef struct centity_s sgentity_s;
#endif
#ifdef SVGAME_INCLUDE
	//! Game Module Name STR.
	#define SG_GAME_MODULE_STR "SVGame"
	//! Entity type.
	typedef struct svg_base_edict_t sgentity_s;
#endif

// Game Times.
#include "sharedgame/sg_time.h"

// Include other shared game headers.
//#include "sharedgame/sg_cmd_messages.h"
//#include "sharedgame/sg_entity_effects.h"
typedef enum sg_entity_events_e sg_entity_events_t;
typedef enum sg_entity_type_e sg_entity_type_t;
//#include "sharedgame/sg_gamemode.h"
typedef enum sg_gamemode_type_e sg_gamemode_type_t;
struct sg_igamemode_t;
struct sg_singleplayer_gamemode_t;
struct sg_cooperative_gamemode_t;
struct sg_deathmatch_gamemode_t;

typedef enum sg_means_of_death_e sg_means_of_death_t;
//#include "sharedgame/sg_misc.h"
//#include "sharedgame/sg_muzzleflashes.h"
//#include "sharedgame/sg_pmove.h"
typedef struct pm_touch_trace_list_s pm_touch_trace_list_t;
typedef struct pm_ground_info_s pm_ground_info_t;
typedef struct pm_contents_info_s pm_contents_info_t;
typedef enum sg_player_state_event_e sg_player_state_event_t;
//#include "sharedgame/sg_pmove_slidemove.h"
typedef enum sg_skm_body_part_e sg_skm_body_part_t;
typedef struct sg_skm_animation_state_s sg_skm_animation_state_t;
typedef struct sg_skm_animation_mixer_s sg_skm_animation_mixer_t;
typedef enum temp_entity_event_e temp_entity_event_t;
//// We need these for this.
//#include "sharedgame/sg_qtag_string.hpp"
//#include "sharedgame/sg_qtag_memory.hpp"
struct sg_usetarget_hint_t;
template<typename T, const memtag_t tag> struct sg_qtag_string_t;
template<typename T, const memtag_t tag> struct sg_qtag_memory_t;