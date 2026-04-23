/********************************************************************
*
*
*	ServerGame: Nav2 Save/Load Foundation - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_save_load.h"

#include "svgame/nav2/nav2_persistence.h"
#include "svgame/nav2/nav2_scheduler.h"
#include "svgame/nav2/nav2_worker_iface.h"

//! Runtime payload marker magic used by Task 5.3 level save integration.
static constexpr uint32_t NAV2_SAVEGAME_RUNTIME_MARKER_MAGIC = MakeLittleLong( 'N', '2', 'R', 'T' );
//! Runtime payload marker version used by Task 5.3 level save integration.
static constexpr uint32_t NAV2_SAVEGAME_RUNTIME_MARKER_VERSION = 1;


/**
*
*
*	Nav2 Save/Load Internal Helpers:
*
*
**/
/**
*	@brief	Write a nav2 serialized header through the gz save wrapper one field at a time.
*	@param	ctx	Savegame write context receiving the header fields.
*	@param	header	Header to serialize.
**/
static void Nav2_SaveLoad_WriteHeaderFields( game_write_context_t *ctx, const nav2_serialized_header_t &header ) {
	/**
	*    Guard against invalid save contexts before writing any fields.
	**/
	if ( !ctx ) {
		return;
	}

	/**
	*    Write the fixed-width header fields explicitly so the save/load layer stays independent from
	*    the standalone filesystem blob layout.
	**/
	ctx->write_int32( ( int32_t )header.magic );
	ctx->write_int32( ( int32_t )header.format_version );
	ctx->write_int32( ( int32_t )header.build_version );
	ctx->write_int32( ( int32_t )header.category );
	ctx->write_int32( ( int32_t )header.transport );
	ctx->write_int64( ( int64_t )header.map_identity );
	ctx->write_int32( ( int32_t )header.section_count );
	ctx->write_int32( ( int32_t )header.compatibility_flags );
}

/**
*	@brief	Read a nav2 serialized header through the gz save wrapper one field at a time.
*	@param	ctx	Savegame read context providing the header fields.
*	@param	outHeader	[out] Header receiving the decoded fields.
**/
static void Nav2_SaveLoad_ReadHeaderFields( game_read_context_t *ctx, nav2_serialized_header_t *outHeader ) {
	/**
	*    Guard against invalid read contexts or output storage before reading any fields.
	**/
	if ( !ctx || !outHeader ) {
		return;
	}

	/**
	*    Read each fixed-width field explicitly in the same order used by the writer.
	**/
	outHeader->magic = ( uint32_t )ctx->read_int32();
	outHeader->format_version = ( uint32_t )ctx->read_int32();
	outHeader->build_version = ( uint32_t )ctx->read_int32();
	outHeader->category = ( nav2_serialized_category_t )ctx->read_int32();
	outHeader->transport = ( nav2_serialized_transport_t )ctx->read_int32();
	outHeader->map_identity = ( uint64_t )ctx->read_int64();
	outHeader->section_count = ( uint32_t )ctx->read_int32();
	outHeader->compatibility_flags = ( uint32_t )ctx->read_int32();
}

/**
*	@brief	Write one runtime payload marker through the gz save wrapper.
*	@param	ctx	Savegame write context receiving marker fields.
*	@param	marker	Marker to serialize.
**/
static void Nav2_SaveLoad_WriteRuntimeMarker( game_write_context_t *ctx, const nav2_savegame_runtime_marker_t &marker ) {
	/**
	*    Guard against invalid save contexts before writing marker fields.
	**/
	if ( !ctx ) {
		return;
	}

	/**
	*    Write each fixed-width marker field explicitly to keep read/write order deterministic.
	**/
	ctx->write_int32( ( int32_t )marker.magic );
	ctx->write_int32( ( int32_t )marker.version );
	ctx->write_int32( ( int32_t )marker.blob_byte_count );
	ctx->write_int32( ( int32_t )marker.actor_state_count );
}

/**
*	@brief	Read one runtime payload marker through the gz save wrapper.
*	@param	ctx	Savegame read context providing marker fields.
*	@param	outMarker	[out] Marker receiving decoded values.
**/
static void Nav2_SaveLoad_ReadRuntimeMarker( game_read_context_t *ctx, nav2_savegame_runtime_marker_t *outMarker ) {
	/**
	*    Guard against invalid read contexts or output storage before reading marker fields.
	**/
	if ( !ctx || !outMarker ) {
		return;
	}

	/**
	*    Read each fixed-width marker field explicitly in writer order.
	**/
	outMarker->magic = ( uint32_t )ctx->read_int32();
	outMarker->version = ( uint32_t )ctx->read_int32();
	outMarker->blob_byte_count = ( uint32_t )ctx->read_int32();
	outMarker->actor_state_count = ( uint32_t )ctx->read_int32();
}

/**
*	@brief	Build default actor continuity records for Task 5.3 runtime persistence.
*	@param	outActorStates	[out] Vector receiving persisted actor state records.
**/
static void Nav2_SaveLoad_BuildDefaultActorStates( std::vector<nav2_savegame_actor_state_t> *outActorStates ) {
	/**
	*    Guard against missing actor-state output storage.
	**/
	if ( !outActorStates ) {
		return;
	}

	/**
	*    Current Task 5.3 foundation persists no actor-owned path continuity by default.
	**/
	outActorStates->clear();
}


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
	const nav2_serialization_policy_t &policy ) {
	/**
	*    Validate the caller-provided save context before writing any header fields.
	**/
	nav2_save_load_result_t result = {};
	if ( !ctx ) {
		return result;
	}

	/**
	*    Build a runtime/savegame transport header and write it field-by-field through the gz wrapper.
	**/
	nav2_serialized_header_t header = {};
	SVG_Nav2_Serialize_InitHeader( &header, policy, NAV_SERIALIZED_SAVE_MAGIC );
	Nav2_SaveLoad_WriteHeaderFields( ctx, header );

	/**
	*    Publish the save result summary.
	**/
	result.success = true;
	result.validation.compatibility = nav2_serialized_compatibility_t::Accept;
	return result;
}

/**
*	@brief	Read and validate a nav2 runtime-chunk header from a gz-backed save context.
*	@param	ctx	Savegame read context providing the header bytes.
*	@param	policy	Expected runtime payload policy.
*	@param	outHeader	[out] Decoded header on success.
*	@return	Structured load result including compatibility validation.
**/
nav2_save_load_result_t SVG_Nav2_SaveLoad_ReadRuntimeHeader( game_read_context_t *ctx,
	const nav2_serialization_policy_t &policy, nav2_serialized_header_t *outHeader ) {
	/**
	*    Validate the caller-provided read context and header output storage before reading bytes.
	**/
	nav2_save_load_result_t result = {};
	if ( !ctx || !outHeader ) {
		return result;
	}

	/**
	*    Read the fixed-size runtime header directly from the gz save context.
	**/
	nav2_serialized_header_t header = {};
	Nav2_SaveLoad_ReadHeaderFields( ctx, &header );

	/**
	*    Validate the runtime payload header against the expected savegame transport policy.
	**/
	result.validation = SVG_Nav2_Serialize_ValidateHeader( header, policy, NAV_SERIALIZED_SAVE_MAGIC );
	if ( result.validation.compatibility == nav2_serialized_compatibility_t::Reject ) {
		return result;
	}

	/**
	*    Publish the decoded header and successful load result.
	**/
	*outHeader = header;
	result.success = true;
	return result;
}

/**
* @brief Write a nav2 persistence bundle through the savegame context in one versioned chunk.
* @param ctx Savegame write context receiving the bundle bytes.
* @param bundle Nav2 bundle to persist.
* @return Structured save result for diagnostics.
* @note The payload is written as a versioned nav2 persistence blob so load paths can validate and reconstruct the nav2 runtime bundle deterministically.
**/
nav2_save_load_result_t SVG_Nav2_SaveLoad_WritePersistenceBundle( game_write_context_t *ctx, const nav2_persistence_bundle_t &bundle ) {
	/**
	*    Build default actor continuity state for this persistence pass.
	**/
	std::vector<nav2_savegame_actor_state_t> actorStates = {};
	Nav2_SaveLoad_BuildDefaultActorStates( &actorStates );
	return SVG_Nav2_SaveLoad_WriteRuntimePayload( ctx, bundle, actorStates );
}

/**
* @brief Read a nav2 persistence bundle through the savegame context and reconstruct runtime state.
* @param ctx Savegame read context providing the bundle bytes.
* @param policy Expected serialization policy used to validate the payload.
* @param outBundle [out] Bundle receiving the decoded state.
* @return Structured load result including compatibility validation.
* @note This decodes the versioned nav2 blob and returns the reconstructed nav2-owned persistence bundle without mutating runtime singletons.
**/
nav2_save_load_result_t SVG_Nav2_SaveLoad_ReadPersistenceBundle( game_read_context_t *ctx,
	const nav2_serialization_policy_t &policy, nav2_persistence_bundle_t *outBundle ) {
	/**
	*    Validate caller-provided read context and output bundle storage before reading payload bytes.
	**/
	nav2_save_load_result_t result = {};
	if ( !ctx || !outBundle ) {
		return result;
	}

	/**
	*    Decode the runtime payload envelope first.
	**/
	nav2_savegame_runtime_payload_t payload = {};
	const nav2_save_load_result_t payloadResult = SVG_Nav2_SaveLoad_ReadRuntimePayload( ctx, policy, &payload );
	if ( !payloadResult.success ) {
		result.validation = payloadResult.validation;
		return result;
	}

	/**
	*    Decode the nav2 persistence blob into the caller-provided runtime bundle.
	**/
	const nav2_persistence_result_t decodeResult = SVG_Nav2_Persistence_ReadBundleBlob( payload.blob, policy, outBundle );
	if ( !decodeResult.success ) {
		result.validation = decodeResult.validation;
		return result;
	}

	/**
	*    Publish structured read summary.
	**/
	result.success = true;
	result.validation = decodeResult.validation;
	result.byte_count = payloadResult.byte_count;
	return result;
}

/**
*	@brief	Write nav2 gameplay-relevant runtime payload into a level save context.
*	@param	ctx	Level-save write context receiving nav2 payload bytes.
*	@param	bundle	Nav2 persistence bundle to serialize.
*	@param	actorStates	Persisted actor continuity state.
*	@return	Structured save result for diagnostics.
**/
nav2_save_load_result_t SVG_Nav2_SaveLoad_WriteRuntimePayload( game_write_context_t *ctx,
	const nav2_persistence_bundle_t &bundle, const std::vector<nav2_savegame_actor_state_t> &actorStates ) {
	/**
	*    Validate the caller-provided save context before writing nav2 payload bytes.
	**/
	nav2_save_load_result_t result = {};
	if ( !ctx ) {
		return result;
	}

	/**
	*    Build a versioned nav2 persistence blob from the runtime bundle.
	**/
	nav2_serialized_blob_t blob = {};
	const nav2_persistence_result_t buildResult = SVG_Nav2_Persistence_BuildBundleBlob( bundle, &blob );
	if ( !buildResult.success ) {
		result.validation = buildResult.validation;
		return result;
	}

	/**
	*    Emit runtime header first so load paths can validate compatibility before reading blob bytes.
	**/
	const nav2_save_load_result_t headerResult = SVG_Nav2_SaveLoad_WriteRuntimeHeader( ctx, bundle.policy );
	if ( !headerResult.success ) {
		result.validation = headerResult.validation;
		return result;
	}

	/**
	*    Write runtime marker describing the encoded nav2 payload and actor continuity record count.
	**/
	nav2_savegame_runtime_marker_t marker = {};
	marker.magic = NAV2_SAVEGAME_RUNTIME_MARKER_MAGIC;
	marker.version = NAV2_SAVEGAME_RUNTIME_MARKER_VERSION;
	marker.blob_byte_count = ( uint32_t )blob.bytes.size();
	marker.actor_state_count = ( uint32_t )actorStates.size();
	Nav2_SaveLoad_WriteRuntimeMarker( ctx, marker );

	/**
	*    Write raw nav2 persistence blob bytes through the gz write context.
	**/
	if ( marker.blob_byte_count > 0 ) {
		ctx->write_data( blob.bytes.data(), marker.blob_byte_count );
	}

	/**
	*    Write actor continuity records using only stable IDs and flags.
	**/
	for ( const nav2_savegame_actor_state_t &actorState : actorStates ) {
		ctx->write_int32( actorState.actor_entnum );
		ctx->write_int32( actorState.has_nav_state ? 1 : 0 );
		ctx->write_int64( ( int64_t )actorState.accepted_job_id );
	}

	/**
	*    Publish structured success summary for the runtime payload write operation.
	**/
	result.success = true;
	result.validation.compatibility = nav2_serialized_compatibility_t::Accept;
	result.byte_count = marker.blob_byte_count;
	return result;
}

/**
*	@brief	Read nav2 gameplay-relevant runtime payload from a level save context.
*	@param	ctx	Level-save read context providing nav2 payload bytes.
*	@param	policy	Expected serialization policy used for blob validation.
*	@param	outPayload	[out] Decoded nav2 runtime payload.
*	@return	Structured load result including compatibility validation.
**/
nav2_save_load_result_t SVG_Nav2_SaveLoad_ReadRuntimePayload( game_read_context_t *ctx,
	const nav2_serialization_policy_t &policy, nav2_savegame_runtime_payload_t *outPayload ) {
	/**
	*    Validate caller-provided read context and payload output storage before reading bytes.
	**/
	nav2_save_load_result_t result = {};
	if ( !ctx || !outPayload ) {
		return result;
	}

	/**
	*    Read and validate runtime header first so incompatible payloads can be rejected safely.
	**/
	nav2_serialized_header_t header = {};
	const nav2_save_load_result_t headerResult = SVG_Nav2_SaveLoad_ReadRuntimeHeader( ctx, policy, &header );
	if ( !headerResult.success ) {
		result.validation = headerResult.validation;
		return result;
	}

	/**
	*    Read and validate runtime marker fields before consuming payload bytes.
	**/
	nav2_savegame_runtime_marker_t marker = {};
	Nav2_SaveLoad_ReadRuntimeMarker( ctx, &marker );
	if ( marker.magic != NAV2_SAVEGAME_RUNTIME_MARKER_MAGIC || marker.version != NAV2_SAVEGAME_RUNTIME_MARKER_VERSION ) {
		result.validation.compatibility = nav2_serialized_compatibility_t::Reject;
		return result;
	}

	/**
	*    Decode raw nav2 persistence blob bytes from the save stream.
	**/
	outPayload->blob.bytes.clear();
	outPayload->blob.bytes.resize( marker.blob_byte_count );
	if ( marker.blob_byte_count > 0 ) {
		ctx->read_data( outPayload->blob.bytes.data(), marker.blob_byte_count );
	}

	/**
	*    Read actor continuity records after blob bytes were decoded.
	**/
	outPayload->actor_states.clear();
	outPayload->actor_states.reserve( marker.actor_state_count );
	for ( uint32_t i = 0; i < marker.actor_state_count; i++ ) {
		nav2_savegame_actor_state_t actorState = {};
		actorState.actor_entnum = ctx->read_int32();
		actorState.has_nav_state = ( ctx->read_int32() != 0 );
		actorState.accepted_job_id = ( uint64_t )ctx->read_int64();
		outPayload->actor_states.push_back( actorState );
	}
	outPayload->marker = marker;

	/**
	*    Publish structured success summary for the runtime payload read operation.
	**/
	result.success = true;
	result.validation = headerResult.validation;
	result.byte_count = marker.blob_byte_count;
	return result;
}

/**
*	@brief	Reset nav2 runtime systems after a load transition according to the Task 2.3 persistence policy.
*	@note	The current policy reconstructs scheduler and worker runtime state rather than attempting to restore in-flight jobs.
**/
void SVG_Nav2_SaveLoad_RebuildRuntimeStateAfterLoad( void ) {
	/**
	*    Reconstruct worker runtime first so scheduler service can safely dispatch after load.
	**/
	SVG_Nav2_Worker_Shutdown();
	SVG_Nav2_Worker_Init();

	/**
	*    Reconstruct scheduler runtime so in-flight job state is dropped and rebuilt safely.
	**/
	SVG_Nav2_Scheduler_Shutdown();
	SVG_Nav2_Scheduler_Init();

	/**
	*    Publish conservative snapshot invalidation so stale in-flight versions are not reused.
	**/
	nav2_scheduler_runtime_t *schedulerRuntime = SVG_Nav2_Scheduler_GetRuntime();
	if ( schedulerRuntime ) {
		SVG_Nav2_Snapshot_BumpStaticNavVersion( &schedulerRuntime->snapshot_runtime );
		SVG_Nav2_Snapshot_BumpOccupancyVersion( &schedulerRuntime->snapshot_runtime );
		SVG_Nav2_Snapshot_BumpMoverVersion( &schedulerRuntime->snapshot_runtime );
		SVG_Nav2_Snapshot_BumpConnectorVersion( &schedulerRuntime->snapshot_runtime );
		SVG_Nav2_Snapshot_BumpModelVersion( &schedulerRuntime->snapshot_runtime );
	}
}

/**
* @brief Rebuild nav2 runtime state from a decoded persistence bundle after load.
* @param bundle Decoded bundle to publish into runtime state.
* @return True when runtime state could be updated from the decoded bundle.
* @note This currently rebuilds scheduler/worker runtime and publishes conservative snapshot invalidation.
**/
const bool SVG_Nav2_SaveLoad_RebuildRuntimeStateFromBundle( const nav2_persistence_bundle_t &bundle ) {
	/**
	*    Mark the bundle parameter as intentionally consumed by future runtime publish steps.
	**/
	( void )bundle;

	/**
	*    Rebuild transient runtime systems according to Task 5.3 persistence policy.
	**/
	SVG_Nav2_SaveLoad_RebuildRuntimeStateAfterLoad();
	return true;
}
