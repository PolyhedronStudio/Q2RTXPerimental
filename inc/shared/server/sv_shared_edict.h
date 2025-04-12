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
    /**
    *   The following is shared memory with the Server.
    *
    *   Since we rely on memory overlap between the Server and the ServerGame.
    *   Both their own edict_t types need to remain plain old POD types.
    *
    *   (In other words, we don't want a ~vtable() and/or alignment/sizeof differences.)
    **/
    //! Entity state.
    entity_state_t  s;
    //! NULL if not a player the server expects the first part
    //! of gclient_s to be a player_state_t but the rest of it is opaque
    BaseClientType *client;
    qboolean inuse;
    int32_t linkcount;

    // FIXME: move these fields to a server private sv_entity_t
    list_t area;    //! Linked to a division node or leaf

    int32_t num_clusters;       // If -1, use headnode instead.
    int32_t clusternums[ MAX_ENT_CLUSTERS ];
    int32_t headnode;           // Unused if num_clusters != -1

    int32_t areanum, areanum2;

    //================================

    int32_t         svflags;    // SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
    vec3_t          mins, maxs;
    vec3_t          absmin, absmax, size;
    cm_solid_t      solid;
    cm_contents_t   clipmask;
    cm_contents_t   hullContents;
    BaseEdictType   *owner;

    //! If set, the entity was part of the initial bsp entity set.
    //! Stores the key/value pair for each entity field.
    const cm_entity_t *entityDictionary;
};