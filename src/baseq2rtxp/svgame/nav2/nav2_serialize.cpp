/********************************************************************
*
*
*\tServerGame: Nav2 Serialization Foundation - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_serialize.h"

#include <cstring>

#include "common/files.h"


/**
*\t@brief	Append raw bytes into a serialized blob.
*\t@param	blob	Serialized blob being built.
*\t@param	data	Source bytes to append.
*\t@param	size	Number of bytes to append.
**/
static void Nav2_Serialize_AppendBytes( nav2_serialized_blob_t *blob, const void *data, const size_t size ) {
	// Validate the destination blob and source pointer before mutating the byte buffer.
	if ( !blob || !data || size == 0 ) {
		return;
	}

	// Append the requested number of bytes into the serialized blob buffer.
	const uint8_t *src = static_cast<const uint8_t *>( data );
	blob->bytes.insert( blob->bytes.end(), src, src + size );
}


/**
*\t@brief	Initialize a nav2 serialized header from a policy descriptor.
*\t@param	header	[out] Header to initialize.
*\t@param	policy	Serialization policy driving the header contents.
*\t@param	magic	Magic value to embed in the header.
**/
void SVG_Nav2_Serialize_InitHeader( nav2_serialized_header_t *header, const nav2_serialization_policy_t &policy, const uint32_t magic ) {
	// Validate the destination header before attempting to fill any fields.
	if ( !header ) {
		return;
	}

	// Start from deterministic defaults so omitted fields remain stable across builds.
	*header = nav2_serialized_header_t{};
	header->magic = magic;
	header->format_version = NAV_SERIALIZED_FORMAT_VERSION;
	header->build_version = NAV_SERIALIZED_BUILD_VERSION;
	header->category = policy.category;
	header->transport = policy.transport;
	header->map_identity = policy.map_identity;
	header->section_count = 0;
	header->compatibility_flags = 0;
}

/**
*\t@brief	Validate a nav2 serialized header against an expected policy and transport magic.
*\t@param	header	Header to validate.
*\t@param	policy	Expected serialization policy.
*\t@param	expectedMagic	Expected transport magic.
*\t@return	Structured validation result used by later cache and save/load code.
**/
nav2_serialized_validation_t SVG_Nav2_Serialize_ValidateHeader( const nav2_serialized_header_t &header,
	const nav2_serialization_policy_t &policy, const uint32_t expectedMagic ) {
	// Start from a conservative reject stance and only relax once all required header checks pass.
	nav2_serialized_validation_t validation = {};
	validation.compatibility = nav2_serialized_compatibility_t::Reject;

	// Validate transport identity first so callers can reject obviously wrong files immediately.
	validation.magic_ok = ( header.magic == expectedMagic );
	validation.format_ok = ( header.format_version == NAV_SERIALIZED_FORMAT_VERSION );
	validation.build_ok = ( header.build_version == NAV_SERIALIZED_BUILD_VERSION );
	validation.category_ok = ( header.category == policy.category );
	validation.transport_ok = ( header.transport == policy.transport );

	// Static format or transport mismatches are hard rejects because later section parsing would be unsafe.
	if ( !validation.magic_ok || !validation.format_ok || !validation.category_ok || !validation.transport_ok ) {
		validation.compatibility = nav2_serialized_compatibility_t::Reject;
		return validation;
	}

	// Build-version mismatches are softer: the payload shape is understood but the content should be rebuilt.
	if ( !validation.build_ok ) {
		validation.compatibility = nav2_serialized_compatibility_t::RebuildRequired;
		return validation;
	}

	// All relevant compatibility checks passed, so the payload header is acceptable.
	validation.compatibility = nav2_serialized_compatibility_t::Accept;
	return validation;
}

/**
*\t@brief	Build a placeholder standalone nav2 cache blob for the requested policy.
*\t@param	policy	Serialization policy describing the payload.
*\t@param	outBlob	[out] Blob receiving the serialized bytes.
*\t@return	Structured serialization result including byte count.
*\t@note	The current foundation writes only the versioned header and section container.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_BuildCacheBlob( const nav2_serialization_policy_t &policy,
	nav2_serialized_blob_t *outBlob ) {
	// Reject requests that do not provide output storage for the serialized blob.
	nav2_serialization_result_t result = {};
	if ( !outBlob ) {
		return result;
	}

	// Reset the output blob so callers never observe stale bytes from a prior build attempt.
	outBlob->bytes.clear();

	// Build a header-only placeholder payload that establishes versioning and transport policy.
	nav2_serialized_header_t header = {};
	SVG_Nav2_Serialize_InitHeader( &header, policy, NAV_SERIALIZED_FILE_MAGIC );
	Nav2_Serialize_AppendBytes( outBlob, &header, sizeof( header ) );

	// Publish the serialization result summary.
	result.success = true;
	result.byte_count = ( uint32_t )outBlob->bytes.size();
	result.validation.compatibility = nav2_serialized_compatibility_t::Accept;
	return result;
}

/**
*\t@brief	Write a nav2 standalone cache blob through engine filesystem helpers.
*\t@param	cachePath	Absolute or game-relative path to write.
*\t@param	blob	Serialized blob to write.
*\t@return	Structured serialization result including byte count.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_WriteCacheFile( const char *cachePath, const nav2_serialized_blob_t &blob ) {
	// Validate the caller inputs before attempting filesystem IO.
	nav2_serialization_result_t result = {};
	if ( !cachePath || cachePath[ 0 ] == '\0' || blob.bytes.empty() ) {
		return result;
	}

	// Build a writable path buffer and let the filesystem helper resolve the final location.
	char pathBuffer[ MAX_OSPATH ] = {};
	const bool writeOk = ( gi.FS_EasyWriteFile )( pathBuffer, sizeof( pathBuffer ), FS_MODE_WRITE, nullptr, cachePath, nullptr,
		blob.bytes.data(), blob.bytes.size() );
	if ( !writeOk ) {
		return result;
	}

	// Publish the byte count on successful write.
	result.success = true;
	result.byte_count = ( uint32_t )blob.bytes.size();
	result.validation.compatibility = nav2_serialized_compatibility_t::Accept;
	return result;
}

/**
*\t@brief	Load and validate a nav2 standalone cache blob through engine filesystem helpers.
*\t@param	cachePath	Absolute or game-relative path to load.
*\t@param	policy	Expected serialization policy.
*\t@param	outHeader	[out] Decoded header on success.
*\t@return	Structured load result including validation state.
*\t@note	Loaded FS buffers remain owned by the engine and are released internally via `gi.FS_FreeFile`.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_ReadCacheHeader( const char *cachePath,
	const nav2_serialization_policy_t &policy, nav2_serialized_header_t *outHeader ) {
	// Validate input storage before attempting filesystem IO.
	nav2_serialization_result_t result = {};
	if ( !cachePath || cachePath[ 0 ] == '\0' || !outHeader ) {
		return result;
	}

	// Load the serialized file through the engine filesystem so ownership stays compatible with the
	// existing svgame memory rules.
	void *fileBuffer = nullptr;
	const int32_t fileLength = ( gi.FS_LoadFile )( cachePath, &fileBuffer );
	if ( fileLength < ( int32_t )sizeof( nav2_serialized_header_t ) || !fileBuffer ) {
		if ( fileBuffer ) {
			( gi.FS_FreeFile )( fileBuffer );
		}
		return result;
	}

	// Copy the header out of the FS-owned buffer before validating or freeing the engine allocation.
	nav2_serialized_header_t header = {};
	std::memcpy( &header, fileBuffer, sizeof( header ) );
	( gi.FS_FreeFile )( fileBuffer );

	// Validate the header against the expected standalone-cache policy.
	result.validation = SVG_Nav2_Serialize_ValidateHeader( header, policy, NAV_SERIALIZED_FILE_MAGIC );
	if ( result.validation.compatibility == nav2_serialized_compatibility_t::Reject ) {
		return result;
	}

	// Publish the decoded header and load summary once validation succeeded or requested rebuild.
	*outHeader = header;
	result.success = true;
	result.byte_count = ( uint32_t )fileLength;
	return result;
}
