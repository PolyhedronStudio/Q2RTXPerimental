/********************************************************************
*
*
*	ServerGame: Nav2 Save/Load Foundation
*
*
********************************************************************/
#pragma once

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
*	@brief	Reset nav2 runtime systems after a load transition according to the Task 2.3 persistence policy.
*	@note	The current policy reconstructs scheduler and worker runtime state rather than attempting to restore in-flight jobs.
**/
void SVG_Nav2_SaveLoad_RebuildRuntimeStateAfterLoad( void );
