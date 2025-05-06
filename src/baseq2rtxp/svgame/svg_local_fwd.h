/********************************************************************
*
*
*	ServerGame: Forward Declarations Header.
*
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*
*	Various FWD:
*
**/
//
// Collision Model Structures.
//
typedef struct cm_entity_s cm_entity_t;

//
// Player Move/State Structures:
//
typedef struct pmove_state_s pmove_state_t;
typedef struct player_state_s player_state_t;



/**
* 
*   SVG FWD Structs & Classes:
* 
**/
//
//	Client Structures:
//
struct client_persistant_t;
struct client_respawn_t;
struct svg_client_t;
struct damage_indicator_t;

//
// Edict Structures:
//
struct svg_edict_t;
struct svg_edict_pool_t;
struct svg_base_edict_t;
struct svg_info_player_start_t;
struct svg_info_player_coop_t;
struct svg_info_player_deathmatch_t;
struct svg_info_player_intermission_t;
struct svg_player_edict_t;

//
// Save Descriptor Field Structure(s):
//
struct svg_save_descriptor_field_t;

//
// Tracing Structures:
//
struct svg_trace_t;

//
//	MonsterMove Structures:
//
struct mm_ground_info_t;
struct mm_liquid_info_t;

//
// Game/Level Locals:
//
struct svg_game_locals_t;
struct svg_level_locals_t;

//
//	PushMove:
//
typedef void( *svg_pushmove_endcallback )( svg_base_edict_t * );
struct svg_pushmove_info_t;

//
//	UseTargets ( Signal(-I/O) && Triggers ):
//
typedef struct svg_signal_argument_s svg_signal_argument_t;
//! Typedef an std::vector for varying argument counts.
typedef std::vector<svg_signal_argument_t> svg_signal_argument_array_t;

//
//	Items/Weapons:
//
typedef struct gitem_s gitem_t;
typedef struct weapon_item_info_s weapon_item_info_t;
typedef struct weapon_mode_animation_s weapon_mode_animation_t;



/**
* 
*   FWD Enums:
* 
**/
enum entity_flags_t;
enum svg_movetype_t;

typedef enum entity_type_e entity_type_t;

typedef enum crosslevel_target_flags_e crosslevel_target_flags_t;

typedef enum entity_usetarget_flags_e entity_usetarget_flags_t;
typedef enum entity_usetarget_state_e entity_usetarget_state_t;
typedef enum entity_usetarget_type_e entity_usetarget_type_t;

typedef enum entity_damageflags_e entity_damageflags_t;
typedef enum entity_lifestatus_e entity_lifestatus_t;
typedef enum entity_takedamage_e entity_takedamage_t;

typedef enum gitem_tag_e gitem_tag_t;

//typedef enum pmtype_t;

typedef enum svg_pushmove_state_e svg_pushmove_state_t;

typedef enum svg_signal_argument_type_e svg_signal_argument_type_t;
typedef enum weapon_mode_e weapon_mode_t;
