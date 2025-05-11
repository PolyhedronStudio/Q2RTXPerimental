/********************************************************************
*
*
*	Save Game Mechanism: 
*
*	Works by iterating up the hierachy of derived classes and 
*	writing the fields in order of the derived class inheritance.
*
* 
********************************************************************/
#pragma once


//! The magic number for the game save file.
static constexpr int32_t SAVE_MAGIC1	= MakeLittleLong('G','S','V','1');
//! The magic number for the level save file.
static constexpr int32_t SAVE_MAGIC2	= MakeLittleLong('L','S','V','1');
// WID: We got our own version number obviously.
static constexpr int32_t SAVE_VERSION	= 1337;


// Forward declarations.
struct svg_base_edict_t;



// <Q2RTXP>: WID: TODO: Need to use the actual FS system instead.
// Include the zlib header if we are using it.
#if USE_ZLIB
	#include <zlib.h>
// Otherwise, we use the standard file I/O functions.
#else
	#define gzopen(name, mode)          fopen(name, mode)
	#define gzclose(file)               fclose(file)
	#define gzwrite(file, buf, len)     fwrite(buf, 1, len, file)
	#define gzread(file, buf, len)      fread(buf, 1, len, file)
	#define gzbuffer(file, size)        (void)0
	#define gzFile                      FILE *
#endif


// For registering function callbacks into the saveable funcptr linked list.
#include "svgame/save/svg_save_callback_global.h"
#include "svgame/save/svg_save_callback_member.h"
// For describing how to treat a field in the save/load process.
#include "svgame/save/svg_save_field_descriptor.h"
// For instancing the registered function callbacks as and in the linked list
#include "svgame/save/svg_save_funcptr_instance.h"
// For reading/writing the save game data.
#include "svgame/save/svg_save_read_context.h"
#include "svgame/save/svg_save_write_context.h"


/******************************
* 
*	
*	LEGACY
* 
* 
*******************************/
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

	F_VECTOR3,
	F_VECTOR4,
	F_ANGLEHACK,

	F_LSTRING,          // string on disk, pointer in memory, TAG_SVGAME_LEVEL
	F_GSTRING,          // string on disk, pointer in memory, TAG_SVGAME
	F_ZSTRING,          // string on disk, string in memory

	F_LEVEL_QSTRING,	// string on disk, sg_qtag_string_t in memory, TAG_SVGAME_LEVEL
	F_GAME_QSTRING,		// string on disk, sg_qtag_string_t in memory, TAG_SVGAME,

	F_LEVEL_QTAG_MEMORY,	// variable sized memory blob on disk, sg_qtag-memory_t in memory, TAG_SVGAME_LEVEL
	F_GAME_QTAG_MEMORY,		// variable sized memory blob on disk, sg_qtag-memory_t in memory, TAG_SVGAME,

	F_EDICT,            // index on disk, pointer in memory
	F_ITEM,             // index on disk, pointer in memory
	F_CLIENT,           // index on disk, pointer in memory
	F_FUNCTION,
	F_POINTER,

	// WID: TODO: Store signal args array.
	//F_SIGNAL_ARGUMENTS,

	F_IGNORE,

	// WID: This was from Q2RTX 1.7.0
	//F_FRAMETIME,         // speciality for savegame compatibility: float on disk, converted to framenum
	// WID: However, we now got QMTime running on int64_t power.
	F_FRAMETIME,	// Same as F_INT64
	F_INT64
};
