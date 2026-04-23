/********************************************************************
*
*
*	ServerGame: Nav2 Save/Load Foundation
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_persistence.h"
#include "svgame/nav2/nav2_runtime.h"
#include "svgame/nav2/nav2_serialize.h"


struct game_read_context_t;
struct game_write_context_t;


/**
*
*
*	Nav2 Save/Load Data Structures:
*
*
**/
/**
*	@brief	Minimal savegame-facing nav2 actor state that is safe to persist across loads.
*	@note	This intentionally stores only stable IDs and flags; transient worker scratch and raw pointers are excluded.
**/
struct nav2_savegame_actor_state_t {
    //! Stable entity number of the actor that owns this nav state.
    int32_t actor_entnum = ENTITYNUM_NONE;
    //! Whether the actor had an accepted nav corridor or path state worth restoring.
    bool has_nav_state = false;
    //! Stable query/job identifier that was last accepted by gameplay code, if any.
    uint64_t accepted_job_id = 0;
};

/**
*	@brief	Summary result of nav2 save/load helper operations.
*	@note	This stays compact so callers can log clear outcomes without parsing text blobs.
**/
struct nav2_save_load_result_t {
    //! True when the save/load helper operation completed successfully.
    bool success = false;
    //! Structured serialization validation result used primarily by load paths.
    nav2_serialized_validation_t validation = {};
    //! Number of bytes written to or loaded from the payload.
    uint32_t byte_count = 0;
};

/**
*\t@brief\tVersioned marker stored before nav2 runtime payload bytes inside level save files.
*\t@note\tThis marker lets load paths distinguish missing nav2 payloads from corrupted payloads safely.
**/
struct nav2_savegame_runtime_marker_t {
    //! Fixed marker magic identifying the nav2 runtime payload marker.
    uint32_t magic = 0;
    //! Marker version for forward-compatible extension.
    uint32_t version = 0;
    //! Number of bytes that follow for the nav2 persistence blob.
    uint32_t blob_byte_count = 0;
    //! Number of serialized actor states in this payload.
    uint32_t actor_state_count = 0;
};

/**
*\t@brief\tVersioned runtime payload written into level save files for nav2 continuity.
*\t@note\tThe payload remains pointer-free and stores only gameplay-relevant continuity state.
**/
struct nav2_savegame_runtime_payload_t {
    //! Runtime payload marker describing the encoded bytes.
    nav2_savegame_runtime_marker_t marker = {};
    //! Persistence blob bytes written/read through gz save contexts.
    nav2_serialized_blob_t blob = {};
    //! Persisted actor continuity records.
    std::vector<nav2_savegame_actor_state_t> actor_states = {};
};


/**
*
*
*	Nav2 Save/Load Public API:
*
*
**/
/**
*	@brief	Write a nav2 runtime-chunk header into a gz-backed save context.
*	@param	ctx	Savegame write context receiving the header.
*	@param	policy	Serialization policy describing the runtime payload.
*	@return	Structured save result for diagnostics.
*	@note	Task 2.3 writes only the versioned runtime header foundation; later tasks extend this to actor path continuity data.
**/
nav2_save_load_result_t SVG_Nav2_SaveLoad_WriteRuntimeHeader( game_write_context_t *ctx,
    const nav2_serialization_policy_t &policy );

/**
*	@brief	Read and validate a nav2 runtime-chunk header from a gz-backed save context.
*	@param	ctx	Savegame read context providing the header bytes.
*	@param	policy	Expected runtime payload policy.
*	@param	outHeader	[out] Decoded header on success.
*	@return	Structured load result including compatibility validation.
**/
nav2_save_load_result_t SVG_Nav2_SaveLoad_ReadRuntimeHeader( game_read_context_t *ctx,
    const nav2_serialization_policy_t &policy, nav2_serialized_header_t *outHeader );

/**
* @brief Write a nav2 persistence bundle through the savegame context in one versioned chunk.
* @param ctx Savegame write context receiving the bundle bytes.
* @param bundle Nav2 bundle to persist.
* @return Structured save result for diagnostics.
* @note The payload is written as a versioned nav2 persistence blob so load paths can validate and reconstruct the nav2 runtime bundle deterministically.
**/
nav2_save_load_result_t SVG_Nav2_SaveLoad_WritePersistenceBundle( game_write_context_t *ctx, const nav2_persistence_bundle_t &bundle );

/**
* @brief Read a nav2 persistence bundle through the savegame context and reconstruct runtime state.
* @param ctx Savegame read context providing the bundle bytes.
* @param policy Expected serialization policy used to validate the payload.
* @param outBundle [out] Bundle receiving the decoded state.
* @return Structured load result including compatibility validation.
* @note This decodes the versioned nav2 blob and returns the reconstructed nav2-owned persistence bundle without mutating runtime singletons.
**/
nav2_save_load_result_t SVG_Nav2_SaveLoad_ReadPersistenceBundle( game_read_context_t *ctx,
    const nav2_serialization_policy_t &policy, nav2_persistence_bundle_t *outBundle );

/**
*\t@brief\tWrite nav2 gameplay-relevant runtime payload into a level save context.
*\t@param\tctx\tLevel-save write context receiving nav2 payload bytes.
*\t@param\tbundle\tNav2 persistence bundle to serialize.
*\t@param\tactorStates\tPersisted actor continuity state.
*\t@return\tStructured save result for diagnostics.
**/
nav2_save_load_result_t SVG_Nav2_SaveLoad_WriteRuntimePayload( game_write_context_t *ctx,
    const nav2_persistence_bundle_t &bundle, const std::vector<nav2_savegame_actor_state_t> &actorStates );

/**
*\t@brief\tRead nav2 gameplay-relevant runtime payload from a level save context.
*\t@param\tctx\tLevel-save read context providing nav2 payload bytes.
*\t@param\tpolicy\tExpected serialization policy used for blob validation.
*\t@param\toutPayload\t[out] Decoded nav2 runtime payload.
*\t@return\tStructured load result including compatibility validation.
**/
nav2_save_load_result_t SVG_Nav2_SaveLoad_ReadRuntimePayload( game_read_context_t *ctx,
    const nav2_serialization_policy_t &policy, nav2_savegame_runtime_payload_t *outPayload );

/**
*	@brief	Reset nav2 runtime systems after a load transition according to the Task 2.3 persistence policy.
*	@note	The current policy reconstructs scheduler and worker runtime state rather than attempting to restore in-flight jobs.
**/
void SVG_Nav2_SaveLoad_RebuildRuntimeStateAfterLoad( void );

/**
* @brief Rebuild nav2 runtime state from a decoded persistence bundle after load.
* @param bundle Decoded bundle to publish into runtime state.
* @return True when runtime state could be updated from the decoded bundle.
* @note This is the bundle-level integration seam for future load hooks that want to restore nav2-owned state without reaching into oldnav.
**/
const bool SVG_Nav2_SaveLoad_RebuildRuntimeStateFromBundle( const nav2_persistence_bundle_t &bundle );
