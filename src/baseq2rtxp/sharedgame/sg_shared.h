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

/**
*	@brief	Returns the given configstring that sits at index.
**/
configstring_t *SG_GetConfigString( const int32_t configStringIndex );

/**
*
*	[OUT]: General SharedGame API functions, non game module specific.
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