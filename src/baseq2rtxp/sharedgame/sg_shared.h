#pragma once

#include "shared/shared.h"
#include "shared/list.h"

#include "sg_gamemode.h"
#include "sg_pmove.h"
#include "sg_pmove_slidemove.h"
#include "sg_time.h"

/**
*
*	General Game API that is implemented in /sharedgame/game_bindings. Each game module will compile
*	with the specific binding to implement these functions.
* 
**/
/**
*	@brief	Wrapper for using the appropriate developer print for the specific game module we're building.
**/
void SG_DPrintf( const char *fmt, ... );
