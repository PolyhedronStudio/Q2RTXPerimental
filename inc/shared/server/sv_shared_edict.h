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

    //! Only send to this client when SVF_SENDCLIENT_SEND_TO_ID is set.
	//! Or send to all clients BUT this client when SVF_SENDCLIENT_EXCLUDE_ID is set.
    //! And if SVF_SENDCLIENT_BITMASK_IDS is set, use it as a bitmask for clients to send to (maxclients must be <= 32, up to the mod to enforce this)
	//! <Q2RTXP>: TODO: In the future, perhaps a 64 player limit 
    int64_t sendClientID = SENDCLIENT_TO_ALL;

	//! Is the entity in use?
    bool inUse = false;
	//! Number of links to this entity.
    int32_t linkCount = 0;

    // FIXME: move these fields to a server private sv_entity_t
    list_t area = {};    //! Linked to a division node or leaf

    int32_t numberOfClusters = 0;       // If -1, use headNode instead.
    int32_t clusterNumbers[ MAX_ENT_CLUSTERS ] = {};
    int32_t headNode = 0;           // Unused if num_clusters != -1

    int32_t areaNumber0 = 0, areaNumber2 = 0;

    //================================

    int32_t         svFlags = 0;    // SVF_NOCLIENT, SVF_DEADENTITY, SVF_MONSTER, etc
    Vector3         mins = { 0.f, 0.f, 0.f }, maxs = { 0.f, 0.f, 0.f };
    Vector3         absMin = { 0.f, 0.f, 0.f }, absMax = { 0.f, 0.f, 0.f }, size = { 0.f, 0.f, 0.f };
    cm_solid_t      solid = SOLID_NOT;
    cm_contents_t   clipMask = cm_contents_t::CONTENTS_NONE;
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
        //! NULL, make sure it is not a player.
        client = nullptr;
        // Send to all clients by default.
        sendClientID = SENDCLIENT_TO_ALL;
		// Not in use by default.
        inUse = false;
		// Not linked into the world by default.
        linkCount = 0;

        // FIXME: move these fields to a server private sv_entity_t
        //! Linked to a division node or leaf
        area = {};

        // If -1, use headNode instead.
        numberOfClusters = 0;
        for ( int32_t i = 0; i < MAX_ENT_CLUSTERS; i++ ) {
			clusterNumbers[ i ] = 0;
        }
        // Unused if num_clusters != -1.
        headNode = 0;           
        areaNumber0 = 0, areaNumber2 = 0;

        //================================

        svFlags = 0;    // SVF_NOCLIENT, SVF_DEADENTITY, SVF_MONSTER, etc

		// Reset bounding box.
		mins = { 0.f, 0.f, 0.f };
		maxs = { 0.f, 0.f, 0.f };
		// Reset absolute bounding box and size.
        absMin = { 0.f, 0.f, 0.f };
        absMax = { 0.f, 0.f, 0.f };
        size = { 0.f, 0.f, 0.f };
		// Reset solid type, entity contents and trace clipping mask.
        solid = SOLID_NOT;
        clipMask = cm_contents_t::CONTENTS_NONE;
        hullContents = cm_contents_t::CONTENTS_NONE;
		// Reset owner.
        owner = nullptr;
		// Reset the cm_entity_t dictionary.
        if ( !retainDictionary ) {
            entityDictionary = nullptr;
        }
	};
};