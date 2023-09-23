#pragma once

//
// fields are needed for spawning from the entity string
// and saving / loading games
//
typedef enum {
	F_BAD,
	F_BYTE,
	F_SHORT,
	F_INT,
	F_BOOL,
	F_FLOAT,
	F_LSTRING,          // string on disk, pointer in memory, TAG_LEVEL
	F_GSTRING,          // string on disk, pointer in memory, TAG_GAME
	F_ZSTRING,          // string on disk, string in memory
	F_VECTOR,
	F_ANGLEHACK,
	F_EDICT,            // index on disk, pointer in memory
	F_ITEM,             // index on disk, pointer in memory
	F_CLIENT,           // index on disk, pointer in memory
	F_FUNCTION,
	F_POINTER,
	F_IGNORE,

	F_FRAMETIME         // speciality for savegame compatibility: float on disk, converted to framenum
} fieldtype_t;

typedef enum {
    P_bad,

    P_prethink,
    P_think,
    P_blocked,
    P_touch,
    P_use,
    P_pain,
    P_die,

    P_moveinfo_endfunc,

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
} ptr_type_t;

typedef struct {
    ptr_type_t type;
    void *ptr;
} save_ptr_t;

extern const save_ptr_t save_ptrs[];
extern const int num_save_ptrs;
extern const save_ptr_t save_ptrs_v2[];
extern const int num_save_ptrs_v2;
