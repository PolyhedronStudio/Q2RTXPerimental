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
    *	@brief	Reuse a matching freed entity or replace the next unused typed-slot placeholder.
    *	@param	classnameOverRuler	Optional authored class name used for initialization.
    *	@return	Current typed entity, or nullptr after a fatal capacity/factory error.
    *	@note	Preserves the slot spawn count; matching-type reuse retains FreeEdict's lifetime increment.
    **/
    template<typename EdictType>
    EdictType *AllocateNextFreeEdict( const char *classnameOverRuler = nullptr ) {
        /**
        *	Prefer a matching freed owner, removing its unique free-list membership before activation.
        **/
        svg_base_edict_t *reusable = nullptr;
        // Type matching preserves the established entity reuse and interpolation policy.
        for ( auto it = freeEdicts.begin(); it != freeEdicts.end(); ++it ) {
            if ( (*it)->GetTypeInfo() == &EdictType::ClassInfo ) {
                reusable = *it;
                freeEdicts.erase( it );
                break;
            }
        }

        /**
        *	Validate fresh capacity before constructing an entity or discarding a reserved placeholder.
        **/
        if ( reusable == nullptr ) {
            if ( edicts == nullptr || num_edicts < 0 || num_edicts >= max_edicts || num_edicts >= game.maxentities ) {
                gi.error( "SVG_AllocateEdict: no free edicts" );
                return nullptr;
            }
            const int32_t slot = num_edicts;
            svg_base_edict_t *placeholder = edicts[ slot ];
            Q_assert( placeholder == nullptr || !placeholder->inUse );
            const int32_t spawnCount = placeholder != nullptr ? placeholder->spawn_count : 0;
            // This inactive preallocated object owns storage; overwriting its pointer would leak it.
            delete placeholder;
            edicts[ slot ] = nullptr;
            reusable = EdictType::ClassInfo.allocateEdictInstanceCallback( nullptr );
            // A failed factory cannot publish an incomplete slot or advance the pool's live range.
            if ( reusable == nullptr ) {
                gi.error( "SVG_AllocateEdict: factory returned nullptr" );
                return nullptr;
            }
            reusable->s.number = slot;
            reusable->spawn_count = spawnCount;
            edicts[ slot ] = reusable;
            ++num_edicts;
        }

        /**
        *	Activate only the resolved owner; do not reset or reinterpret its spawn-count lifetime.
        **/
        EdictType *ent = static_cast<EdictType *>( reusable );
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
