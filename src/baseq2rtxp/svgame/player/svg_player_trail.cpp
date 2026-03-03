/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"


#include "svgame/entities/svg_player_edict.h"
#include "svgame/entities/monster/svg_monster_testdummy_debug.h"

#include "svgame/player/svg_player_trail.h"

/*
==============================================================================

PLAYER TRAIL

==============================================================================

This is a circular list containing the a list of points of where
the player has been recently.  It is used by monsters for pursuit.

.origin     the spot
.owner      forward link
.aiment     backward link
*/

#define ENABLE_PLAYER_TRAIL_ENTITIES

#ifdef ENABLE_PLAYER_TRAIL_ENTITIES
//! Number of breadcrumb spots to keep in the circular buffer.
#define TRAIL_LENGTH    128

//! The circular buffer storing breadcrumb entities used by monsters for pursuit.
svg_base_edict_t     *trail[TRAIL_LENGTH];
//! Index of the next slot to write into inside `trail`.
int         trail_head;
//! Whether the trail system has been initialized and is active.
bool        trail_active = false;

#define NEXT(n)     (((n) + 1) & (TRAIL_LENGTH - 1))
#define PREV(n)     (((n) - 1) & (TRAIL_LENGTH - 1))

/**
*	@brief	Initialize the trail circular buffer by allocating edicts.
*	@note	This must be called after the global edict pool is available so
*		the trail edicts can be allocated from it.
**/
void PlayerTrail_Init(void)
{
    /**
    *	Allocate each breadcrumb edict from the edict pool and store the
    *	pointer in the circular buffer.
    **/
    for ( int n = 0; n < TRAIL_LENGTH; n++ ) {
        trail[ n ] = g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>( "player_trail" );
    }

    // Reset head index and mark the system active.
    trail_head = 0;
    trail_active = true;
}


/**
*	@brief		Append a breadcrumb at `spot` into the circular trail buffer.
*	@details	Helper function for `PlayerTrail_New` and `PlayerTrail_Add`. Stores the given
*	@param	spot	World-space feet-origin position to store as a breadcrumb.
*	@note	Stores the current `level.time` in the breadcrumb's `timestamp`
*			and computes a yaw based on the vector from the previous
*			breadcrumb to this one.
**/
void PlayerTrail_Add( const Vector3 &spot )
{
    // Early out when the trail system hasn't been initialized.
    if ( !trail_active ) {
        return;
    }

    /**
    *	Store the spot in the trail edict and stamp it with the current time.
    *	Use the utility helpers so entity absolute/linking data is kept
    *	consistent with the rest of the engine.
    **/
    SVG_Util_SetEntityOrigin( trail[ trail_head ], spot, true );
    trail[ trail_head ]->timestamp = level.time;

    /**
    *	Compute a yaw for the breadcrumb so monsters can face along the
    *	breadcrumb chain when visualizing or using `IsInFrontOf` checks.
    **/
    vec3_t temp;
    VectorSubtract( spot, trail[ PREV( trail_head ) ]->currentOrigin, temp );
    trail[ trail_head ]->currentAngles[ 1 ] = QM_Vector3ToYaw( temp );
    SVG_Util_SetEntityAngles( trail[ trail_head ], trail[ trail_head ]->currentAngles, true );

    // Advance head to the next slot in the circular buffer.
    trail_head = NEXT( trail_head );
}


/**
*	@brief	Initialize the trail system and add the first breadcrumb.
*	@param	spot	Initial breadcrumb position.
**/
void PlayerTrail_New( const Vector3 &spot )
{
    // Ensure the trail system is initialized before adding the first spot.
    if ( !trail_active ) {
        PlayerTrail_Init();
    }

    PlayerTrail_Add( spot );
}


/**
*	@brief	Pick the first breadcrumb spot that `self` should follow.
*	@param	self	The monster/entity performing the pick. Uses
*			`self->trail_time` to skip breadcrumbs older than the
*			current follow-time marker.
*	@return	The chosen breadcrumb edict or nullptr if the trail system is
*			not active.
**/
svg_base_edict_t *PlayerTrail_PickFirst( svg_monster_testdummy_debug_t *self )
{
    int32_t marker = 0;
    int32_t n = 0;

    // If the trail system isn't active, there's nothing to pick.
    if ( !trail_active ) {
        return nullptr;
    }

    /**
    *	Walk backwards from the head until we find a breadcrumb newer than
    *	`self->trail_time`. This skips breadcrumbs we've already followed or
    *	explicitly marked as consumed by the entity.
    **/
    for ( marker = trail_head, n = TRAIL_LENGTH; n; n-- ) {
        if ( trail[ marker ]->timestamp <= self->trailNavigationState.trailTimeStamp ) {
            marker = NEXT( marker );
        } else {
            break;
        }
    }

    // Prefer a visible breadcrumb if possible to improve robustness of picks.
    if ( SVG_Entity_IsVisible( static_cast< svg_base_edict_t * >( self ), trail[ marker ] ) ) {
        return trail[ marker ];
    }
    if ( SVG_Entity_IsVisible( static_cast< svg_base_edict_t* >( self ), trail[ PREV( marker ) ] ) ) {
        return trail[ PREV( marker ) ];
    }

    return trail[ marker ];
}

/**
*	@brief	Return the next breadcrumb after the one that would be returned
*			by `PlayerTrail_PickFirst` for `self`.
*	@param	self	The monster/entity that is following the trail.
*	@return	The next breadcrumb edict or nullptr if the trail system is
*			not active.
**/
svg_base_edict_t *PlayerTrail_PickNext( svg_monster_testdummy_debug_t *self )
{
    int32_t marker = 0;
    int32_t n = 0;

    if ( !trail_active ) {
        return nullptr;
    }

    // Find the first candidate as in PickFirst, then return it.
    for ( marker = trail_head, n = TRAIL_LENGTH; n; n-- ) {
        if ( trail[ marker ]->timestamp <= self->trailNavigationState.trailTimeStamp ) {
            marker = NEXT( marker );
        } else {
            break;
        }
    }

    return trail[ marker ];
}
	
/**
*	@brief	Return the most recently added breadcrumb spot.
*	@return	Pointer to the last spot in the circular trail buffer.
**/
svg_base_edict_t *PlayerTrail_LastSpot( void )
{
    return trail[ PREV( trail_head ) ];
}
#else // !ENABLE_PLAYER_TRAIL_ENTITIES
#define TRAIL_LENGTH    128

svg_base_edict_t *trail[ TRAIL_LENGTH ];
int         trail_head;
bool        trail_active = false;

#define NEXT(n)     (((n) + 1) & (TRAIL_LENGTH - 1))
#define PREV(n)     (((n) - 1) & (TRAIL_LENGTH - 1))
void PlayerTrail_Init( void ) {
	// Nothing to do.
}
void PlayerTrail_Add( const Vector3 &spot ) {
	// Nothing to do.
}
void PlayerTrail_New( const Vector3 &spot ) {
	// Nothing to do.
}
svg_base_edict_t *PlayerTrail_PickFirst( svg_base_edict_t *self ) {
	// Nothing to do.
	return nullptr;
}

#endif // ENABLE_PLAYER_TRAIL_ENTITIES