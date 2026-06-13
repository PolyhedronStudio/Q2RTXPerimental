/********************************************************************
*
*
*	ServerGame: EdictPool Interface Memory Management Implementation.
*	NameSpace: "".
*
*
********************************************************************/
#pragma once

#include <vector>


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
    *   @brief  Free list of edicts that can be reused.
    **/
    std::vector<svg_base_edict_t *> freeEdicts;

    /**
    *   @brief  Pushes an edict to the free list.
    **/
    void PushFree( svg_base_edict_t *ed ) {
        freeEdicts.push_back( ed );
    }

    /**
    *   @brief  Pops an edict from the free list.
    **/
    svg_base_edict_t *PopFree() {
        if ( freeEdicts.empty() ) {
            return nullptr;
        }
        svg_base_edict_t *ed = freeEdicts.back();
        freeEdicts.pop_back();
        return ed;
    }

    /**
    *   @brief  Either finds a free edict of the matching type, or allocates a new one.
    **/
    svg_base_edict_t *AllocateNextFreeEdict( EdictTypeInfo *typeInfo, const cm_entity_t *cm_entity = nullptr, const char *classnameOverRuler = nullptr );
    /**
    *   @brief  Either finds a free edict, or allocates a new one.
    *   @remark This function tries to avoid reusing an entity that was recently freed, 
    *           because it can cause the client to think the entity morphed into something 
    *           else instead of being removed and recreated, which can cause interpolated
    *           angles and bad trails.
   
    **/
    template<typename EdictType>
    EdictType *AllocateNextFreeEdict( const char *classnameOverRuler = nullptr ) {
        svg_base_edict_t *reusable = nullptr;
        // Search free list for matching type
        for ( auto it = freeEdicts.begin(); it != freeEdicts.end(); ++it ) {
            if ( (*it)->GetTypeInfo() == &EdictType::ClassInfo ) {
                reusable = *it;
                freeEdicts.erase(it);
                break;
            }
        }
        
        if ( !reusable ) {
            // Allocate new
            reusable = EdictType::ClassInfo.allocateEdictInstanceCallback(nullptr);
            if ( num_edicts >= game.maxentities ) {
                gi.error("SVG_AllocateEdict: no free edicts");
                return nullptr;
            }
            reusable->s.number = num_edicts++;
            edicts[reusable->s.number] = reusable;
        }

        EdictType *ent = static_cast<EdictType*>(reusable);
        _InitEdict<EdictType>( ent, ent->s.number, classnameOverRuler );
        return ent;
    }
	/**
	*   @brief  Marks the edict as free.
    **/
    void FreeEdict( svg_base_edict_t *ed, const bool forceEvenIfSpecialEntity = false );

    /**
    *   @brief  Support routine for AllocateNextFreeEdict.
    **/
    template<class EdictType>
    inline void _InitEdict( EdictType *ed, const int32_t stateNumber, const char *classnameOverRuler = nullptr ) {
        ed->inUse = true;
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
