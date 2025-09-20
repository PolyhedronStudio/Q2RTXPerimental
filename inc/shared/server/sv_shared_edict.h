/********************************************************************
*
*
*   Shared/Server: Shared Edict Data, for composite construction of
*   the server and server game edict data.
*
*
********************************************************************/
#pragma once


/**
*   @brief  We use template trickery to allow the server and servergame
*           to define memory-wise overlapping pointers to the same object.
* 
*           This structure is meant to be used by means of compositing by
*           making it a member at the top of both game and server edict_t.
**/
template<typename BaseEdictType, typename BaseClientType>
struct sv_shared_edict_t {
	//! Default constructor.
    sv_shared_edict_t<BaseEdictType,BaseClientType>() = default;
	//! Destructor.
    virtual ~sv_shared_edict_t<BaseEdictType, BaseClientType>() = default;

    //! Constructor for use with entityDictionary.
    sv_shared_edict_t<BaseEdictType, BaseClientType>( const cm_entity_t *ed ) : entityDictionary( ed ) { };

    /**
    *   The following is shared memory with the Server.
    *
    *   Since we rely on memory overlap between the Server and the ServerGame.
    *   Both their own edict_t types need to remain plain old POD types.
    *
    *   (In other words, we don't want a ~vtable() and/or alignment/sizeof differences.)
    **/
    //! Entity state.
    entity_state_t  s = {};
    //! NULL if not a player the server expects the first part
    //! of gclient_s to be a player_state_t but the rest of it is opaque
    BaseClientType *client = nullptr;
    bool inuse = false;
    int32_t linkcount = 0;

    // FIXME: move these fields to a server private sv_entity_t
    list_t area = {};    //! Linked to a division node or leaf

    int32_t num_clusters = 0;       // If -1, use headnode instead.
    int32_t clusternums[ MAX_ENT_CLUSTERS ] = {};
    int32_t headnode = 0;           // Unused if num_clusters != -1

    int32_t areanum = 0, areanum2 = 0;

    //================================

    int32_t         svflags = 0;    // SVF_NOCLIENT, SVF_DEADENTITY, SVF_MONSTER, etc
    vec3_t          mins = { 0.f, 0.f, 0.f }, maxs = { 0.f, 0.f, 0.f };
    vec3_t          absmin = { 0.f, 0.f, 0.f }, absmax = { 0.f, 0.f, 0.f }, size = { 0.f, 0.f, 0.f };
    cm_solid_t      solid = SOLID_NOT;
    cm_contents_t   clipmask = cm_contents_t::CONTENTS_NONE;
    cm_contents_t   hullContents = cm_contents_t::CONTENTS_NONE;
    BaseEdictType   *owner = nullptr;

    //! If set, the entity was part of the initial bsp entity set.
    //! Stores the key/value pair for each entity field.
    const cm_entity_t *entityDictionary = nullptr;

    /**
	*   Reconstructs the object, optionally retaining the entityDictionary.
    **/
	virtual void Reset( const bool retainDictionary = false ) {
        //! Entity state.
        s = {};
        //! NULL if not a player the server expects the first part
        //! of gclient_s to be a player_state_t but the rest of it is opaque
        client = nullptr;
        inuse = false;
        linkcount = 0;

        // FIXME: move these fields to a server private sv_entity_t
        area = {};    //! Linked to a division node or leaf

        num_clusters = 0;       // If -1, use headnode instead.
        for ( int32_t i = 0; i < MAX_ENT_CLUSTERS; i++ ) {
			clusternums[ i ] = 0;
        }
        headnode = 0;           // Unused if num_clusters != -1
        areanum = 0, areanum2 = 0;

        //================================

        svflags = 0;    // SVF_NOCLIENT, SVF_DEADENTITY, SVF_MONSTER, etc

		VectorSet( mins, 0.f, 0.f, 0.f );
		VectorSet( maxs, 0.f, 0.f, 0.f );

        VectorSet( absmin, 0.f, 0.f, 0.f );
        VectorSet( absmax, 0.f, 0.f, 0.f );
        VectorSet( size, 0.f, 0.f, 0.f );

        solid = SOLID_NOT;
        clipmask = cm_contents_t::CONTENTS_NONE;
        hullContents = cm_contents_t::CONTENTS_NONE;

        owner = nullptr;

		// Reset the cm_entity_t dictionary.
        if ( !retainDictionary ) {
            entityDictionary = nullptr;
        }
	};
};