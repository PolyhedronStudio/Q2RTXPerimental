/********************************************************************
*
*
*	Save Game mechanism related types:
*
* 
********************************************************************/
#pragma once

#include "shared/shared.h"


/**
*	@brief	Fields are needed for spawning from the entity string
*			and saving / loading games
**/
enum fieldtype_t : int32_t {
	F_BAD,
	F_BYTE,
	F_SHORT,
	F_INT,

	F_BOOL,
	F_FLOAT,
	F_DOUBLE,

	F_LSTRING,          // string on disk, pointer in memory, TAG_SVGAME_LEVEL
	F_GSTRING,          // string on disk, pointer in memory, TAG_SVGAME
	F_ZSTRING,          // string on disk, string in memory

	F_VECTOR,
	F_ANGLEHACK,

	F_EDICT,            // index on disk, pointer in memory
	F_ITEM,             // index on disk, pointer in memory
	F_CLIENT,           // index on disk, pointer in memory
	F_FUNCTION,
	F_POINTER,

	F_IGNORE,

	// WID: This was from Q2RTX 1.7.0
	//F_FRAMETIME,         // speciality for savegame compatibility: float on disk, converted to framenum
	// WID: However, we now got sg_time_t running on int64_t power.
	F_FRAMETIME,	// Same as F_INT64
	F_INT64
};

/**
*	@brief	Indexes what type of pointer is/was being read/written to.
**/
enum ptr_type_t : int32_t {
    P_bad,

	//
	// edict-><methodname> function pointer addresses.
	//
	P_postspawn,
    P_prethink,
    P_think,
	P_postthink,
    P_blocked,
    P_touch,
    P_use,
    P_pain,
    P_die,

	//
	// edict->moveinfo.<methodname> function pointer addresses.
	//
    P_moveinfo_endfunc,

	//
	// edict->monsterinfo.<methodname> function pointer addresses.
	//
    P_monsterinfo_currentmove,
	P_monsterinfo_nextmove,
    P_monsterinfo_stand,
    P_monsterinfo_idle,
    P_monsterinfo_search,
    P_monsterinfo_walk,
    P_monsterinfo_run,
    P_monsterinfo_dodge,
    P_monsterinfo_attack,
    P_monsterinfo_melee,
    P_monsterinfo_sight,
    P_monsterinfo_checkattack
};

/**
*	@brief	Used for constructing our array(located in g_ptrs.cpp) containing all possible callback save methods and their type.
**/
typedef struct {
    ptr_type_t type;
    void *ptr;
} save_ptr_t;

/**
*	For accessing them outside of g_ptrs.cpp
**/
extern const save_ptr_t save_ptrs[];
extern const int num_save_ptrs;
