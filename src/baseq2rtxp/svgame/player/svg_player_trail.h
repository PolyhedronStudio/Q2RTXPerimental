/********************************************************************
*
*
*	ServerGame: Player Trail for Monster Navigation.
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
    @brief	Initialize the player breadcrumb trail system.
    @note	This allocates trail edict objects from the global edict pool
        and must be called after the edict pool has been initialized.
**/
void PlayerTrail_Init( void );

/**
    @brief	Add a breadcrumb spot to the circular player trail buffer.
    @param	spot	World-space feet-origin position to store as a breadcrumb.
    @note	The function stamps the breadcrumb with the current `level.time`
        and computes a yaw for the breadcrumb based on the vector from
        the previous breadcrumb to this one. Monsters use the trail
        entries for breadcrumb-following across multiple floors.
**/
void PlayerTrail_Add( const Vector3 &spot );

/**
    @brief	Reset the trail list and add the first breadcrumb `spot`.
    @param	spot	Initial breadcrumb position.
    @note	This is a convenience helper that ensures the trail is
        initialized and contains a single valid spot.
**/
void PlayerTrail_New( const Vector3 &spot );

/**
    @brief	Pick the first trail spot for `self` to pursue.
    @param	self	Monster/entity that is following the trail. The
            entity's `trail_time` is used to skip older breadcrumbs.
    @return	Pointer to the selected breadcrumb edict, or nullptr if the
            trail system is not active.
    @note	The selection algorithm prefers breadcrumbs newer than
            `self->trail_time` and performs simple visibility checks to
            prefer an immediately visible breadcrumb when available.
**/
svg_base_edict_t *PlayerTrail_PickFirst( svg_base_edict_t *self );

/**
    @brief	Pick the next trail spot for `self` after the current pick.
    @param	self	Monster/entity that is following the trail.
    @return	Pointer to the next breadcrumb edict, or nullptr if the trail
            system is not active.
**/
svg_base_edict_t *PlayerTrail_PickNext( svg_base_edict_t *self );

/**
    @brief	Return the most recently added breadcrumb spot.
    @return	Pointer to the last spot in the circular trail buffer.
**/
svg_base_edict_t *PlayerTrail_LastSpot( void );
