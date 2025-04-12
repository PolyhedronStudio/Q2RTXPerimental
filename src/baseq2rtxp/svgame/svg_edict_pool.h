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
    virtual svg_edict_t *operator[]( const size_t index );

    /**
    *	@brief	Returns a pointer to the edict matching the number.
    *	@return	The edict for the given number. (nullptr if out of range).
    **/
    virtual svg_edict_t *EdictForNumber( const int32_t number );
    //! Gets the number for the matching edict ptr.
    virtual const int32_t NumberForEdict( const svg_edict_t *edict );

    ////! Pointer to edicts data array.
    //svg_edict_t *edicts;
    //////! Size of edict type.
    ////int32_t         edict_size;
    ////! Number of active edicts.
    //int32_t         num_edicts;     // current number, <= max_edicts
    ////! Maximum edicts.
    //int32_t         max_edicts;
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
*   @brief  (Re-)initializes the edict pool.
**/
svg_edict_t *SVG_EdictPool_Reallocate( svg_edict_pool_t *edictPool, const int32_t numReservedEntities );
