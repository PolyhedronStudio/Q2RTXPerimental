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


/**
*   @description    WriteGame
*
*   This will be called whenever the game goes to a new level,
*   and when the user explicitly saves the game.
*
*   Game information include cross level data, like multi level
*   triggers, help computer info, and all client states.
*
*   A single player death will automatically restore from the
*   last save position.
**/
void SVG_WriteGame( const char *filename, const bool isAutoSave );
/**
*   @brief  Reads a game save file and initializes the game state.
*   @param  filename The name of the save file to read.
*   @description    This function reads a save file and initializes the game state.
*                   It allocates memory for the game state and clients, and reads the game data from the file.
*                   It also checks the version of the save file and ensures that the game state is valid.
**/
void SVG_ReadGame( const char *filename );

/**
*   @brief  Writes the state of the current level to a file.
**/
void SVG_WriteLevel( const char *filename );
/**
*   @brief  SpawnEntities will allready have been called on the
*	level the same way it was when the level was saved.
*
*   That is necessary to get the baselines set up identically.
*
*   The server will have cleared all of the world links before
*   calling ReadLevel.
*
*   No clients are connected yet.
**/
void SVG_ReadLevel( const char *filename );