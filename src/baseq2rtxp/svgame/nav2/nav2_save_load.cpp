/********************************************************************
*
*
*	ServerGame: Nav2 Save/Load Foundation - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_save_load.h"


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
*	@brief	Reset nav2 runtime systems after a load transition according to the Task 2.3 persistence policy.
*	@note	The current policy reconstructs scheduler and worker runtime state rather than attempting to restore in-flight jobs.
**/
void SVG_Nav2_SaveLoad_RebuildRuntimeStateAfterLoad( void ) {
    // Task 2.3 defines the persistence policy boundary only: transient scheduler and worker state must be reconstructed after load instead of being deserialized directly.
    // The concrete runtime reset wiring is intentionally deferred so this foundation layer does not take hard compile-time dependencies on later scheduler and worker implementation units.
    return;
}
