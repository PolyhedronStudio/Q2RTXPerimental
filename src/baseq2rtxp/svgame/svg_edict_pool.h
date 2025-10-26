/********************************************************************
*
*
*	ServerGame: EdictPool Interface Memory Management Implementation.
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*   @brief  Interface to be implemented by the ServerGame which is a
*           Memory Pool for game allocated EDICTS.
**/
struct svg_edict_pool_t : sv_edict_pool_i {
    //! Don't allow instancing of this 'interface'.
    svg_edict_pool_t() = default;
    //! Virtual destructor.
    virtual ~svg_edict_pool_t() = default;

    /**
    *   @brief  For accessing as if it were a regular edicts array.
	*   @return The edict at the given index. (nullptr if out of range).
    **/
    virtual svg_base_edict_t *operator[]( const size_t index );

    /**
    *	@brief	Returns a pointer to the edict matching the number.
    *	@return	The edict for the given number. (nullptr if out of range).
    **/
    virtual svg_base_edict_t *EdictForNumber( const int32_t number );
    /**
    *   @brief  Gets the number for the matching edict ptr. 
	*   @return The slot index number of the given edict, or -1 if the edict is out of range or (nullptr).
    **/
    virtual const int32_t NumberForEdict( const svg_base_edict_t *edict );

    /**
    *   @brief  Either finds a free edict, or allocates a new one.
    *   @remark This function tries to avoid reusing an entity that was recently freed, 
    *           because it can cause the client to think the entity morphed into something 
    *           else instead of being removed and recreated, which can cause interpolated
    *           angles and bad trails.
    **/
    svg_base_edict_t *EmplaceNextFreeEdict( svg_base_edict_t *ent );
    /**
    *   @brief  Either finds a free edict, or allocates a new one.
    *   @remark This function tries to avoid reusing an entity that was recently freed, 
    *           because it can cause the client to think the entity morphed into something 
    *           else instead of being removed and recreated, which can cause interpolated
    *           angles and bad trails.
   
    **/
    template<typename EdictType>
    EdictType *AllocateNextFreeEdict( const char *classnameOverRuler = nullptr ) {
        svg_base_edict_t *entity = nullptr;
        EdictType *freedEntity = nullptr;

        // Start after the client slots.
        int32_t i = game.maxclients + 1;

        entity = edicts[ i ];

        // Iterate and seek.
        for ( i; i < num_edicts; i++, entity = edicts[ i ] ) {

            // the first couple seconds of server time can involve a lot of
            // freeing and allocating, so relax the replacement policy
            if ( entity != nullptr && !entity->inuse && ( entity->freetime < 2_sec || level.time - entity->freetime > 500_ms ) ) {
                _InitEdict<EdictType>( static_cast<EdictType *>( entity ), i, classnameOverRuler );
                return static_cast<EdictType *>( entity );
            }

            // this is going to be our second chance to spawn an entity in case all free
            // entities have been freed only recently
            if ( !freedEntity ) {
                freedEntity = static_cast<EdictType *>( entity );
            }
        }

        // If we reached the maximum number of entities.
        if ( i == game.maxentities ) {
            // If we have a freed entity, use it.
            if ( freedEntity ) {
                // Initialize it.
                _InitEdict<EdictType>( freedEntity, i, classnameOverRuler );
                // Return it.
                return static_cast<EdictType *>( freedEntity );
            }
            // If we don't have any free edicts, error out.
            gi.error( "SVG_AllocateEdict: no free edicts" );
        }

        // Initialize it.
        _InitEdict<EdictType>( static_cast<EdictType *>( entity ), num_edicts, classnameOverRuler );
        // If we have free edicts left to go, use those instead.
        num_edicts++;

        return static_cast<EdictType *>( entity );
    }
	/**
	*   @brief  Marks the edict as free.
    **/
    void FreeEdict( svg_base_edict_t *ed );

    /**
    *   @brief  Support routine for AllocateNextFreeEdict.
    **/
    template<class EdictType>
    inline void _InitEdict( EdictType *ed, const int32_t stateNumber, const char *classnameOverRuler = nullptr ) {
        ed->inuse = true;
        ed->classname = svg_level_qstring_t::from_char_str( ( classnameOverRuler != nullptr ? classnameOverRuler : EdictType::ClassInfo.worldSpawnClassName ) );
        ed->gravity = 1.0f;
        ed->s.number = stateNumber;
		// <Q2RTXP>: For temp entities spawned by other entities.
		ed->owner = nullptr;

        // A generic entity type by default.
        ed->s.entityType = ET_GENERIC;

        // PGM - do this before calling the spawn function so it can be overridden.
        ed->gravityVector = QM_Vector3Gravity();
        // PGM
    }
};



/**
*
*
*
*   Entity Init/Alloc/Free:
*
*
*
**/
/**
*   @brief	Frees any previously allocated edicts in the pool.
**/
svg_base_edict_t **SVG_EdictPool_Release( svg_edict_pool_t *edictPool );
/**
*   @brief  (Re-)initializes the edict pool.
**/
svg_base_edict_t **SVG_EdictPool_Allocate( svg_edict_pool_t *edictPool, const int32_t numReservedEntities );
