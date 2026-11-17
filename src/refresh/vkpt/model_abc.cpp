/********************************************************************
*
*
*	Refresh: VKPT Alembic (.abc) Ogawa PolyMesh Loader
*
*	Self-contained native reader for Alembic (.abc) files using the
*	Ogawa binary container format. Extracts animated variable-topology
*	IPolyMesh sequences directly from memory buffers into maliasmesh_t
*	slices for per-frame static BLAS ray tracing.
*
*
********************************************************************/
#include "vkpt.h"
#include "material.h"
#include "conversion.h"
#include "format/abc.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include <array>
#include <string>
#include <vector>



/**
*
*
*	Ogawa Binary Format Constants and Structures:
*
*
**/
//! Flag bit in a 64-bit Ogawa child offset indicating an IData payload rather than an IGroup.
static constexpr uint64_t OGAWA_DATA_FLAG = 0x8000000000000000ULL;
//! Bitmask extracting the 63-bit byte offset of an Ogawa IGroup or IData node.
static constexpr uint64_t OGAWA_OFFSET_MASK = 0x7FFFFFFFFFFFFFFFULL;
//! Size in bytes of the 128-bit Murmur3 digest prefix stored at the start of every non-empty property sample.
static constexpr uint64_t ABC_SAMPLE_DIGEST_SIZE = 16ULL;

/**
*	@brief	Plain Old Data (POD) type enumeration matching `Alembic::Util::PlainOldDataType`.
**/
enum abc_pod_type_e : uint8_t {
	ABC_POD_BOOLEAN = 0,
	ABC_POD_UINT8 = 1,
	ABC_POD_INT8 = 2,
	ABC_POD_UINT16 = 3,
	ABC_POD_INT16 = 4,
	ABC_POD_UINT32 = 5,
	ABC_POD_INT32 = 6,
	ABC_POD_UINT64 = 7,
	ABC_POD_INT64 = 8,
	ABC_POD_FLOAT16 = 9,
	ABC_POD_FLOAT32 = 10,
	ABC_POD_FLOAT64 = 11,
	ABC_POD_STRING = 12,
	ABC_POD_WSTRING = 13,
	ABC_POD_NUM_TYPES = 14
};

/**
*	@brief	Alembic property category matching `Alembic::AbcCoreAbstract::PropertyType`.
**/
enum abc_property_type_e : uint8_t {
	ABC_PROP_COMPOUND = 0,
	ABC_PROP_SCALAR = 1,
	ABC_PROP_ARRAY = 2,
	ABC_PROP_SCALAR_LIKE_ARRAY = 3
};

/**
*	@brief	Bounds-checked view of an Ogawa `IData` byte slice inside the in-memory archive buffer.
**/
struct abc_ogawa_data_t {
	//! Pointer to the first payload byte inside the archive buffer (nullptr if empty).
	const uint8_t *bytes = nullptr;
	//! Number of payload bytes available.
	uint64_t size = 0;
};

/**
*	@brief	Bounds-checked view of an Ogawa `IGroup` node inside the in-memory archive buffer.
**/
struct abc_ogawa_group_t {
	//! Pointer to the start of the entire `.abc` archive memory buffer.
	const uint8_t *archive_base = nullptr;
	//! Total byte length of the `.abc` archive memory buffer.
	uint64_t archive_size = 0;
	//! Number of child entries in this group.
	uint64_t num_children = 0;
	//! Pointer to the array of `num_children` little-endian `uint64_t` child offsets.
	const uint8_t *children_table = nullptr;
};

/**
*	@brief	Parsed Alembic property header (`AbcCoreOgawa::PropertyHeaderAndFriends`) paired with its Ogawa group.
**/
struct abc_property_t {
	//! Property name within its parent compound property (e.g. ".geom", "P", ".faceIndices", "N", "uv").
	std::string name;
	//! Deserialized metadata string (e.g. "schema=AbcGeom_PolyMesh_v1;interpretation=point").
	std::string metadata;
	//! Property type (Compound=0, Scalar=1, Array=2 or 3).
	uint8_t property_type = ABC_PROP_COMPOUND;
	//! Underlying POD element type (`abc_pod_type_e`).
	uint8_t pod_type = ABC_POD_UNKNOWN;
	//! Number of POD scalars per element (e.g. 3 for V3f/P/N, 2 for V2f/uv, 1 for int32 arrays).
	uint32_t extent = 1;
	//! Total number of authored time samples (`nextSampleIndex`).
	uint32_t num_samples = 0;
	//! First sample index that differs from sample 0.
	uint32_t first_changed_index = 0;
	//! Last sample index at which a change occurred.
	uint32_t last_changed_index = 0;
	//! TimeSampling table index used by this property.
	uint32_t time_sampling_index = 0;
	//! Whether all samples share homogeneous dimensions.
	bool is_homogenous = true;
	//! Ogawa group backing this property's sub-properties (if compound) or sample data blobs (if scalar/array).
	abc_ogawa_group_t group;

	static constexpr uint8_t ABC_POD_UNKNOWN = 0xFF;
};

/**
*	@brief	Discovered `IPolyMesh` object within the Alembic hierarchy ready for per-frame extraction.
**/
struct abc_polymesh_node_t {
	//! Object name in the Alembic hierarchy.
	std::string name;
	//! Full path in the Alembic hierarchy.
	std::string full_path;
	//! `.geom` compound property group containing `P`, `.faceCounts`, `.faceIndices`, `N`, `uv`.
	abc_ogawa_group_t geom_group;
	//! Parsed child properties inside `.geom`.
	std::vector<abc_property_t> geom_props;
	//! Maximum sample count across `P`, `.faceCounts`, and `.faceIndices`.
	uint32_t num_samples = 1;
};

/**
*	@brief	POD 3-float vector wrapper for `std::vector` storage compatible with `vec3_t`.
**/
struct abc_vec3_t {
	//! X, Y, Z coordinates.
	float v[ 3 ];
	//! Implicit conversion to mutable float pointer (`vec3_t`).
	operator float *() {
		return v;
	}
	//! Implicit conversion to const float pointer (`const vec3_t`).
	operator const float *() const {
		return v;
	}
};

/**
*	@brief	POD 2-float vector wrapper for `std::vector` storage compatible with `vec2_t`.
**/
struct abc_vec2_t {
	//! U, V texture coordinates.
	float v[ 2 ];
	//! Implicit conversion to mutable float pointer (`vec2_t`).
	operator float *() {
		return v;
	}
	//! Implicit conversion to const float pointer (`const vec2_t`).
	operator const float *() const {
		return v;
	}
};

/**
*	@brief	Temporary per-frame unpacked geometry for a single `IPolyMesh`.
**/
struct abc_unpacked_frame_t {
	//! Unpacked vertex positions for this frame.
	std::vector<abc_vec3_t> positions;
	//! Unpacked vertex normals for this frame.
	std::vector<abc_vec3_t> normals;
	//! Unpacked vertex texture coordinates for this frame.
	std::vector<abc_vec2_t> tex_coords;
	//! Unpacked vertex velocities for this frame (in Quake Z-up coordinates).
	std::vector<abc_vec3_t> velocities;
	//! Unpacked triangle vertex indices (3 per triangle, 0-based relative to this frame's vertices).
	std::vector<int32_t> indices;
};



/**
*
*
*	Low-Level Unaligned Read and Ogawa Navigation Helpers:
*
*
**/
/**
*	@brief	Safely read a little-endian value of type `T` from an unaligned byte pointer.
*	@param	ptr	Source byte pointer.
*	@return	Unpacked value of type `T`.
**/
template<typename T>
static inline T ABC_ReadUnalignedLE( const uint8_t *ptr ) {
	T value;
	// Copy bytes directly (x86_64/ARM64 are little-endian).
	std::memcpy( &value, ptr, sizeof( T ) );
	return value;
}

/**
*	@brief	Return the byte size of a single scalar of the given Alembic `abc_pod_type_e`.
*	@param	pod	POD type identifier.
*	@return	Byte size of the scalar type, or 0 if variable/unsupported.
**/
static inline uint32_t ABC_GetPodByteSize( const uint8_t pod ) {
	// Dispatch by Alembic POD enum value.
	switch ( pod ) {
		case ABC_POD_BOOLEAN:
		case ABC_POD_UINT8:
		case ABC_POD_INT8:
			return 1;
		case ABC_POD_UINT16:
		case ABC_POD_INT16:
		case ABC_POD_FLOAT16:
			return 2;
		case ABC_POD_UINT32:
		case ABC_POD_INT32:
		case ABC_POD_FLOAT32:
			return 4;
		case ABC_POD_UINT64:
		case ABC_POD_INT64:
		case ABC_POD_FLOAT64:
			return 8;
		default:
			return 0;
	}
}

/**
*	@brief	Parse an Ogawa `IGroup` header at `group_offset` inside the archive buffer.
*	@param	archive_base	Base pointer of the `.abc` file buffer.
*	@param	archive_size	Total byte length of the `.abc` file buffer.
*	@param	group_offset	Byte offset of the group within the archive (0 represents an empty group).
*	@param	out_group		[out] Populated group view.
*	@return	True if the group offset and child table lie strictly within the archive bounds.
**/
static bool ABC_ReadGroup( const uint8_t *archive_base, const uint64_t archive_size, const uint64_t group_offset, abc_ogawa_group_t *out_group ) {
	/**
	*	Handle empty group offset (0).
	**/
	// Initialize output group fields.
	out_group->archive_base = archive_base;
	out_group->archive_size = archive_size;
	out_group->num_children = 0;
	out_group->children_table = nullptr;

	// Offset 0 is a valid empty Ogawa group.
	if ( group_offset == 0 ) {
		return true;
	}

	/**
	*	Validate that the 8-byte child count and the child offset array fit in the buffer.
	**/
	// Check if the child count field is within archive bounds.
	if ( group_offset > archive_size || ( archive_size - group_offset ) < sizeof( uint64_t ) ) {
		return false;
	}

	// Read the number of children in this group.
	const uint64_t num_children = ABC_ReadUnalignedLE<uint64_t>( archive_base + group_offset );
	const uint64_t table_offset = group_offset + sizeof( uint64_t );
	const uint64_t remaining_bytes = archive_size - table_offset;

	// Ensure the child offset array does not overflow the archive buffer.
	if ( num_children > ( remaining_bytes / sizeof( uint64_t ) ) ) {
		return false;
	}

	// Commit child count and table pointer.
	out_group->num_children = num_children;
	out_group->children_table = archive_base + table_offset;
	return true;
}

/**
*	@brief	Check whether child `child_idx` of `group` is a sub-group (`IGroup`).
*	@param	group		Parent Ogawa group.
*	@param	child_idx	Zero-based child index.
*	@return	True if the child exists and is an `IGroup`.
**/
static inline bool ABC_GroupIsChildGroup( const abc_ogawa_group_t &group, const uint64_t child_idx ) {
	// Validate child index against group bounds.
	if ( child_idx >= group.num_children || !group.children_table ) {
		return false;
	}
	// Read the raw 64-bit child descriptor.
	const uint64_t raw_offset = ABC_ReadUnalignedLE<uint64_t>( group.children_table + child_idx * sizeof( uint64_t ) );
	return ( raw_offset & OGAWA_DATA_FLAG ) == 0;
}

/**
*	@brief	Check whether child `child_idx` of `group` is a data blob (`IData`).
*	@param	group		Parent Ogawa group.
*	@param	child_idx	Zero-based child index.
*	@return	True if the child exists and is an `IData` node.
**/
static inline bool ABC_GroupIsChildData( const abc_ogawa_group_t &group, const uint64_t child_idx ) {
	// Validate child index against group bounds.
	if ( child_idx >= group.num_children || !group.children_table ) {
		return false;
	}
	// Read the raw 64-bit child descriptor.
	const uint64_t raw_offset = ABC_ReadUnalignedLE<uint64_t>( group.children_table + child_idx * sizeof( uint64_t ) );
	return ( raw_offset & OGAWA_DATA_FLAG ) != 0;
}

/**
*	@brief	Retrieve child `child_idx` of `group` as a sub-group (`IGroup`).
*	@param	group			Parent Ogawa group.
*	@param	child_idx		Zero-based child index.
*	@param	out_child_group	[out] Populated child group view.
*	@return	True if the child is a valid `IGroup` within archive bounds.
**/
static bool ABC_GroupGetChildGroup( const abc_ogawa_group_t &group, const uint64_t child_idx, abc_ogawa_group_t *out_child_group ) {
	// Ensure the requested child is a group.
	if ( !ABC_GroupIsChildGroup( group, child_idx ) ) {
		return false;
	}
	// Extract the 63-bit byte offset and parse the child group.
	const uint64_t raw_offset = ABC_ReadUnalignedLE<uint64_t>( group.children_table + child_idx * sizeof( uint64_t ) );
	return ABC_ReadGroup( group.archive_base, group.archive_size, raw_offset & OGAWA_OFFSET_MASK, out_child_group );
}

/**
*	@brief	Retrieve child `child_idx` of `group` as a data slice (`IData`).
*	@param	group		Parent Ogawa group.
*	@param	child_idx	Zero-based child index.
*	@param	out_data	[out] Populated data slice view.
*	@return	True if the child is a valid `IData` blob within archive bounds.
**/
static bool ABC_GroupGetChildData( const abc_ogawa_group_t &group, const uint64_t child_idx, abc_ogawa_data_t *out_data ) {
	// Reset output slice.
	out_data->bytes = nullptr;
	out_data->size = 0;

	// Ensure the requested child is a data node.
	if ( !ABC_GroupIsChildData( group, child_idx ) ) {
		return false;
	}

	// Mask off the high data flag bit to obtain the file offset.
	const uint64_t raw_offset = ABC_ReadUnalignedLE<uint64_t>( group.children_table + child_idx * sizeof( uint64_t ) );
	const uint64_t data_offset = raw_offset & OGAWA_OFFSET_MASK;

	// Offset 0 (raw 0x8000000000000000ULL) represents an empty 0-byte data blob.
	if ( data_offset == 0 ) {
		return true;
	}

	// Validate that the 8-byte byte count header fits in the archive.
	if ( data_offset > group.archive_size || ( group.archive_size - data_offset ) < sizeof( uint64_t ) ) {
		return false;
	}

	// Read the payload size and validate payload bounds.
	const uint64_t data_size = ABC_ReadUnalignedLE<uint64_t>( group.archive_base + data_offset );
	const uint64_t payload_offset = data_offset + sizeof( uint64_t );
	if ( data_size > ( group.archive_size - payload_offset ) ) {
		return false;
	}

	// Populate the output byte slice.
	out_data->bytes = group.archive_base + payload_offset;
	out_data->size = data_size;
	return true;
}



/**
*
*
*	Alembic Metadata, Object Header, and Property Header Parsers:
*
*
**/
/**
*	@brief	Read a variable-width `uint32_t` encoded with an AbcCoreOgawa `sizeHint` (0=uint8, 1=uint16, 2=uint32).
*	@param	data		Source `IData` byte slice.
*	@param	size_hint	Width hint (0, 1, or 2).
*	@param	io_pos		[in,out] Current read cursor inside `data`.
*	@param	out_val		[out] Decoded 32-bit unsigned integer.
*	@return	True if enough bytes remained in `data`.
**/
static bool ABC_ReadUint32WithHint( const abc_ogawa_data_t &data, const uint32_t size_hint, uint64_t *io_pos, uint32_t *out_val ) {
	// Dispatch on the 2-bit size hint encoded in the property header info word.
	if ( size_hint == 0 ) {
		if ( *io_pos + 1 > data.size ) {
			return false;
		}
		*out_val = ( uint32_t )data.bytes[ *io_pos ];
		*io_pos += 1;
		return true;
	} else if ( size_hint == 1 ) {
		if ( *io_pos + 2 > data.size ) {
			return false;
		}
		*out_val = ( uint32_t )ABC_ReadUnalignedLE<uint16_t>( data.bytes + *io_pos );
		*io_pos += 2;
		return true;
	} else if ( size_hint == 2 ) {
		if ( *io_pos + 4 > data.size ) {
			return false;
		}
		*out_val = ABC_ReadUnalignedLE<uint32_t>( data.bytes + *io_pos );
		*io_pos += 4;
		return true;
	}

	return false;
}

/**
*	@brief	Parse the archive-wide indexed metadata table stored at child 5 of the Alembic root group.
*	@param	data			`IData` slice from root group child 5.
*	@param	out_metadata	[out] Vector of metadata strings (index 0 is always "").
*	@return	True if the metadata stream was parsed without bounds errors.
**/
static bool ABC_ParseIndexedMetaData( const abc_ogawa_data_t &data, std::vector<std::string> *out_metadata ) {
	// Index 0 is always an empty MetaData entry in AbcCoreOgawa.
	out_metadata->clear();
	out_metadata->emplace_back( "" );

	uint64_t pos = 0;
	// Each entry is stored as [uint8_t size][char str[size]].
	while ( pos < data.size ) {
		const uint8_t md_size = data.bytes[ pos++ ];
		if ( pos + md_size > data.size ) {
			return false;
		}
		out_metadata->emplace_back( reinterpret_cast<const char *>( data.bytes + pos ), md_size );
		pos += md_size;
	}

	return true;
}

/**
*	@brief	Parse all sub-property headers of a CompoundProperty (`CprData`) Ogawa group.
*	@param	cpr_group		CompoundProperty Ogawa group.
*	@param	indexed_md		Archive-wide indexed metadata table.
*	@param	out_properties	[out] Parsed list of child properties paired with their child Ogawa groups.
*	@return	True if property headers were parsed cleanly.
**/
static bool ABC_ParseCompoundProperties(
	const abc_ogawa_group_t &cpr_group,
	const std::vector<std::string> &indexed_md,
	std::vector<abc_property_t> *out_properties ) {
	/**
	*	Empty compound property groups have 0 children.
	**/
	out_properties->clear();
	if ( cpr_group.num_children == 0 ) {
		return true;
	}

	/**
	*	In AbcCoreOgawa, children 0 .. num_children - 2 are sub-property groups,
	*	and child num_children - 1 is the IData blob containing all property headers.
	**/
	const uint64_t headers_idx = cpr_group.num_children - 1;
	abc_ogawa_data_t data;
	if ( !ABC_GroupGetChildData( cpr_group, headers_idx, &data ) ) {
		return false;
	}

	uint64_t pos = 0;
	uint64_t prop_idx = 0;

	// Parse each packed property header record from the IData stream.
	while ( pos < data.size ) {
		if ( pos + sizeof( uint32_t ) > data.size ) {
			return false;
		}

		// Read the packed bitfield word describing the property.
		const uint32_t info = ABC_ReadUnalignedLE<uint32_t>( data.bytes + pos );
		pos += sizeof( uint32_t );

		abc_property_t prop;
		prop.property_type = ( uint8_t )( info & 0x0003u );
		const uint32_t size_hint = ( info & 0x000Cu ) >> 2;

		// Scalar and Array properties carry POD type, extent, and sample change ranges.
		if ( prop.property_type != ABC_PROP_COMPOUND ) {
			prop.pod_type = ( uint8_t )( ( info & 0x00F0u ) >> 4 );
			prop.is_homogenous = ( ( info & 0x0400u ) != 0 );
			prop.extent = ( info & 0xFF000u ) >> 12;

			// Read nextSampleIndex (total number of samples written).
			if ( !ABC_ReadUint32WithHint( data, size_hint, &pos, &prop.num_samples ) ) {
				return false;
			}

			// Decode firstChangedIndex and lastChangedIndex according to bitmask flags.
			if ( ( info & 0x0200u ) != 0 ) {
				if ( !ABC_ReadUint32WithHint( data, size_hint, &pos, &prop.first_changed_index ) ||
					!ABC_ReadUint32WithHint( data, size_hint, &pos, &prop.last_changed_index ) ) {
					return false;
				}
			} else if ( ( info & 0x0800u ) != 0 ) {
				prop.first_changed_index = 0;
				prop.last_changed_index = 0;
			} else {
				prop.first_changed_index = 1;
				prop.last_changed_index = ( prop.num_samples > 0 ) ? ( prop.num_samples - 1 ) : 0;
			}

			// Decode optional non-zero timeSamplingIndex.
			if ( ( info & 0x0100u ) != 0 ) {
				if ( !ABC_ReadUint32WithHint( data, size_hint, &pos, &prop.time_sampling_index ) ) {
					return false;
				}
			}
		}

		// Read the property name string.
		uint32_t name_size = 0;
		if ( !ABC_ReadUint32WithHint( data, size_hint, &pos, &name_size ) || ( pos + name_size > data.size ) ) {
			return false;
		}
		prop.name.assign( reinterpret_cast<const char *>( data.bytes + pos ), name_size );
		pos += name_size;

		// Read inline or indexed metadata string.
		const uint32_t md_index = ( info & 0xFF00000u ) >> 20;
		if ( md_index == 0xFFu ) {
			uint32_t md_size = 0;
			if ( !ABC_ReadUint32WithHint( data, size_hint, &pos, &md_size ) || ( pos + md_size > data.size ) ) {
				return false;
			}
			prop.metadata.assign( reinterpret_cast<const char *>( data.bytes + pos ), md_size );
			pos += md_size;
		} else if ( md_index < indexed_md.size() ) {
			prop.metadata = indexed_md[ md_index ];
		} else {
			return false;
		}

		// Bind the corresponding child IGroup for this property.
		if ( prop_idx >= headers_idx || !ABC_GroupGetChildGroup( cpr_group, prop_idx, &prop.group ) ) {
			return false;
		}
		prop_idx++;

		out_properties->push_back( std::move( prop ) );
	}

	return true;
}

/**
*	@brief	Find a named property in a parsed property list.
*	@param	props	Property list to search.
*	@param	name	Exact property name (e.g. "P", ".faceCounts", ".faceIndices", "N", "uv").
*	@return	Pointer to the matching `abc_property_t`, or `nullptr` if not found.
**/
static const abc_property_t *ABC_FindProperty( const std::vector<abc_property_t> &props, const char *name ) {
	// Linear scan over the compound property's children.
	for ( const abc_property_t &prop : props ) {
		if ( prop.name == name ) {
			return &prop;
		}
	}
	return nullptr;
}

/**
*	@brief	Map a logical animation sample index `sample_idx` to the physical sample index stored in the Ogawa group.
*	@param	prop		Property header containing change range metadata.
*	@param	sample_idx	Requested logical sample/frame index.
*	@return	Physical sample index inside `prop.group`.
**/
static uint64_t ABC_VerifySampleIndex( const abc_property_t &prop, const uint32_t sample_idx ) {
	// If the property has 0 or 1 samples, always use physical index 0.
	if ( prop.num_samples <= 1 ) {
		return 0;
	}

	// Clamp logical index to the available sample range.
	const uint32_t clamped_idx = std::min( sample_idx, prop.num_samples - 1 );

	// Samples before first_changed_index are identical to sample 0.
	if ( clamped_idx < prop.first_changed_index ) {
		return 0;
	}
	// Constant property across all samples.
	if ( prop.first_changed_index == prop.last_changed_index && prop.first_changed_index == 0 ) {
		return 0;
	}
	// Samples at or after last_changed_index reuse the final changed sample.
	if ( clamped_idx >= prop.last_changed_index ) {
		return ( uint64_t )( prop.last_changed_index - prop.first_changed_index + 1 );
	}

	// Variable sample within the active change window.
	return ( uint64_t )( clamped_idx - prop.first_changed_index + 1 );
}



/**
*
*
*	Typed Sample Data Extraction Helpers:
*
*
**/
/**
*	@brief	Retrieve the raw `IData` slice for sample `sample_idx` of a scalar or array property.
*	@param	prop		Property descriptor.
*	@param	sample_idx	Logical sample index.
*	@param	out_data	[out] Data slice positioned at the start of the sample (including the 16-byte digest).
*	@return	True if the sample data blob was retrieved.
**/
static bool ABC_GetPropertySampleData( const abc_property_t &prop, const uint32_t sample_idx, abc_ogawa_data_t *out_data ) {
	// Compute physical sample index inside the property's Ogawa group.
	const uint64_t phys_idx = ABC_VerifySampleIndex( prop, sample_idx );

	// Scalar properties store 1 IData child per sample; Array properties store 2 IData children (data, dims) per sample.
	const uint64_t child_idx = ( prop.property_type == ABC_PROP_SCALAR ) ? phys_idx : ( phys_idx * 2ULL );
	return ABC_GroupGetChildData( prop.group, child_idx, out_data );
}

/**
*	@brief	Read an integer array sample (`.faceCounts`, `.faceIndices`, or `.indices`) as `int32_t` values.
*	@param	prop		Property descriptor.
*	@param	sample_idx	Logical sample index.
*	@param	out_ints	[out] Vector of decoded 32-bit signed integers.
*	@return	True if the sample was decoded successfully.
**/
static bool ABC_ReadInt32ArraySample( const abc_property_t &prop, const uint32_t sample_idx, std::vector<int32_t> *out_ints ) {
	out_ints->clear();

	abc_ogawa_data_t data;
	if ( !ABC_GetPropertySampleData( prop, sample_idx, &data ) ) {
		return false;
	}

	// Empty sample (0 elements).
	if ( data.size <= ABC_SAMPLE_DIGEST_SIZE ) {
		return true;
	}

	// Skip the 16-byte Murmur3 key at the start of the sample payload.
	const uint8_t *payload = data.bytes + ABC_SAMPLE_DIGEST_SIZE;
	const uint64_t payload_bytes = data.size - ABC_SAMPLE_DIGEST_SIZE;
	const uint32_t pod_size = ABC_GetPodByteSize( prop.pod_type );
	if ( pod_size == 0 ) {
		return false;
	}

	const uint64_t count = payload_bytes / pod_size;
	out_ints->resize( ( size_t )count );

	// Fast path for native 32-bit integers.
	if ( prop.pod_type == ABC_POD_INT32 || prop.pod_type == ABC_POD_UINT32 ) {
		std::memcpy( out_ints->data(), payload, ( size_t )( count * sizeof( int32_t ) ) );
		return true;
	}

	// Fallback conversions for 16-bit or 64-bit integer index arrays.
	for ( uint64_t i = 0; i < count; i++ ) {
		const uint8_t *elem = payload + i * pod_size;
		switch ( prop.pod_type ) {
			case ABC_POD_INT16:
				( *out_ints )[ ( size_t )i ] = ( int32_t )ABC_ReadUnalignedLE<int16_t>( elem );
				break;
			case ABC_POD_UINT16:
				( *out_ints )[ ( size_t )i ] = ( int32_t )ABC_ReadUnalignedLE<uint16_t>( elem );
				break;
			case ABC_POD_INT64:
			case ABC_POD_UINT64:
				( *out_ints )[ ( size_t )i ] = ( int32_t )ABC_ReadUnalignedLE<int64_t>( elem );
				break;
			default:
				return false;
		}
	}

	return true;
}

/**
*	@brief	Read a float array sample (`P`, `N`, `uv`, or `.vals`) as flat `float` values.
*	@param	prop				Property descriptor.
*	@param	sample_idx			Logical sample index.
*	@param	expected_tuple_size	Expected number of floats per tuple (e.g. 3 for V3f, 2 for V2f).
*	@param	out_floats			[out] Vector of decoded floats (`num_tuples * expected_tuple_size`).
*	@return	True if the sample was decoded successfully.
**/
static bool ABC_ReadFloatArraySample(
	const abc_property_t &prop,
	const uint32_t sample_idx,
	const uint32_t expected_tuple_size,
	std::vector<float> *out_floats ) {
	out_floats->clear();

	abc_ogawa_data_t data;
	if ( !ABC_GetPropertySampleData( prop, sample_idx, &data ) ) {
		return false;
	}

	// Empty sample (0 elements).
	if ( data.size <= ABC_SAMPLE_DIGEST_SIZE ) {
		return true;
	}

	// Skip the 16-byte Murmur3 digest prefix.
	const uint8_t *payload = data.bytes + ABC_SAMPLE_DIGEST_SIZE;
	const uint64_t payload_bytes = data.size - ABC_SAMPLE_DIGEST_SIZE;
	const uint32_t pod_size = ABC_GetPodByteSize( prop.pod_type );
	if ( pod_size == 0 || ( payload_bytes / pod_size ) % expected_tuple_size != 0 ) {
		return false;
	}

	const uint64_t num_scalars = payload_bytes / pod_size;
	out_floats->resize( ( size_t )num_scalars );

	// Fast path for native 32-bit IEEE-754 floats.
	if ( prop.pod_type == ABC_POD_FLOAT32 ) {
		std::memcpy( out_floats->data(), payload, ( size_t )( num_scalars * sizeof( float ) ) );
		return true;
	}

	// Support 64-bit doubles and 16-bit half-floats.
	for ( uint64_t i = 0; i < num_scalars; i++ ) {
		const uint8_t *elem = payload + i * pod_size;
		if ( prop.pod_type == ABC_POD_FLOAT64 ) {
			( *out_floats )[ ( size_t )i ] = ( float )ABC_ReadUnalignedLE<double>( elem );
		} else if ( prop.pod_type == ABC_POD_FLOAT16 ) {
			( *out_floats )[ ( size_t )i ] = halfToFloat( ABC_ReadUnalignedLE<uint16_t>( elem ) );
		} else {
			return false;
		}
	}

	return true;
}

/**
*	@brief	Extract a Geometric Parameter (`N` or `uv`), which in Alembic may be either a direct
*			`ArrayProperty` or a `CompoundProperty` containing `".vals"` and optional `".indices"`.
*	@param	prop				Property descriptor for `"N"` or `"uv"`.
*	@param	indexed_md			Archive-wide indexed metadata table.
*	@param	sample_idx			Logical sample index.
*	@param	expected_tuple_size	3 for normals (`N`), 2 for texture coordinates (`uv`).
*	@param	out_vals			[out] Flat float array of values.
*	@param	out_indices			[out] Optional index array (empty if unindexed).
*	@return	True if valid values were read for this sample.
**/
static bool ABC_ReadGeomParamSample(
	const abc_property_t &prop,
	const std::vector<std::string> &indexed_md,
	const uint32_t sample_idx,
	const uint32_t expected_tuple_size,
	std::vector<float> *out_vals,
	std::vector<int32_t> *out_indices ) {
	out_vals->clear();
	out_indices->clear();

	// Case 1: Unindexed ArrayProperty (e.g. kFacevaryingScope or kVertexScope).
	if ( prop.property_type != ABC_PROP_COMPOUND ) {
		return ABC_ReadFloatArraySample( prop, sample_idx, expected_tuple_size, out_vals );
	}

	// Case 2: Indexed CompoundProperty containing ".vals" and optional ".indices".
	std::vector<abc_property_t> sub_props;
	if ( !ABC_ParseCompoundProperties( prop.group, indexed_md, &sub_props ) ) {
		return false;
	}

	const abc_property_t *vals_prop = ABC_FindProperty( sub_props, ".vals" );
	if ( !vals_prop || !ABC_ReadFloatArraySample( *vals_prop, sample_idx, expected_tuple_size, out_vals ) ) {
		return false;
	}

	const abc_property_t *indices_prop = ABC_FindProperty( sub_props, ".indices" );
	if ( indices_prop ) {
		ABC_ReadInt32ArraySample( *indices_prop, sample_idx, out_indices );
	}

	return true;
}



/**
*
*
*	Alembic Object Tree Traversal:
*
*
**/
/**
*	@brief	Recursively traverse an Alembic Object (`OrData`) group and collect all `IPolyMesh` nodes.
*	@param	obj_group		Current Object's Ogawa group.
*	@param	obj_name		Current Object's short name.
*	@param	full_path		Current Object's full path in the archive.
*	@param	obj_metadata	Current Object's metadata string.
*	@param	indexed_md		Archive-wide indexed metadata table.
*	@param	out_meshes		[out] Discovered `IPolyMesh` objects.
**/
static void ABC_DiscoverPolyMeshesRecursive(
	const abc_ogawa_group_t &obj_group,
	const std::string &obj_name,
	const std::string &full_path,
	const std::string &obj_metadata,
	const std::vector<std::string> &indexed_md,
	std::vector<abc_polymesh_node_t> *out_meshes ) {
	/**
	*	Stop if we reached the maximum supported submeshes per model.
	**/
	if ( out_meshes->size() >= ABC_MAX_MESHES || obj_group.num_children == 0 ) {
		return;
	}

	/**
	*	Check child 0 (the Object's top-level CompoundProperty `CprData`) for a `.geom` property
	*	containing `IPolyMesh` arrays (`P`, `.faceCounts`, `.faceIndices`).
	**/
	if ( ABC_GroupIsChildGroup( obj_group, 0 ) ) {
		abc_ogawa_group_t top_cpr;
		if ( ABC_GroupGetChildGroup( obj_group, 0, &top_cpr ) ) {
			std::vector<abc_property_t> top_props;
			if ( ABC_ParseCompoundProperties( top_cpr, indexed_md, &top_props ) ) {
				const abc_property_t *geom_prop = ABC_FindProperty( top_props, ".geom" );
				if ( geom_prop && geom_prop->property_type == ABC_PROP_COMPOUND ) {
					std::vector<abc_property_t> geom_props;
					if ( ABC_ParseCompoundProperties( geom_prop->group, indexed_md, &geom_props ) ) {
						const abc_property_t *p_prop = ABC_FindProperty( geom_props, "P" );
						const abc_property_t *fc_prop = ABC_FindProperty( geom_props, ".faceCounts" );
						const abc_property_t *fi_prop = ABC_FindProperty( geom_props, ".faceIndices" );

						// Verify that all three required PolyMesh properties exist.
						if ( p_prop && fc_prop && fi_prop ) {
							abc_polymesh_node_t node;
							node.name = obj_name.empty() ? "polymesh" : obj_name;
							node.full_path = full_path;
							node.geom_group = geom_prop->group;
							node.geom_props = std::move( geom_props );
							node.num_samples = std::max( { 1u, p_prop->num_samples, fc_prop->num_samples, fi_prop->num_samples } );
							out_meshes->push_back( std::move( node ) );
						}
					}
				}
			}
		}
	}

	/**
	*	Parse child object headers from the last child IData (if present) and recurse into child objects.
	*	In AbcCoreOgawa, child 0 is CprData, children 1 .. N-2 are child Object groups, and child N-1
	*	is the ObjectHeaders IData (ending with a 32-byte hash trailer).
	**/
	if ( obj_group.num_children < 3 ) {
		return;
	}

	const uint64_t headers_child_idx = obj_group.num_children - 1;
	abc_ogawa_data_t headers_data;
	if ( !ABC_GroupGetChildData( obj_group, headers_child_idx, &headers_data ) || headers_data.size <= 32 ) {
		return;
	}

	// Strip the trailing 32-byte child/object hashes.
	const uint64_t buf_size = headers_data.size - 32;
	uint64_t pos = 0;
	uint64_t child_obj_idx = 1;

	// Iterate over each serialized child ObjectHeader.
	while ( pos < buf_size && child_obj_idx < headers_child_idx ) {
		if ( pos + sizeof( uint32_t ) > buf_size ) {
			break;
		}

		// Read child object name length and string.
		const uint32_t name_size = ABC_ReadUnalignedLE<uint32_t>( headers_data.bytes + pos );
		pos += sizeof( uint32_t );
		if ( name_size == 0 || ( pos + name_size + 1 ) > buf_size ) {
			break;
		}

		std::string child_name( reinterpret_cast<const char *>( headers_data.bytes + pos ), name_size );
		pos += name_size;

		// Read child metadata index or inline metadata string.
		const uint8_t md_index = headers_data.bytes[ pos++ ];
		std::string child_md;
		if ( md_index == 0xFFu ) {
			if ( pos + sizeof( uint32_t ) > buf_size ) {
				break;
			}
			const uint32_t md_size = ABC_ReadUnalignedLE<uint32_t>( headers_data.bytes + pos );
			pos += sizeof( uint32_t );
			if ( pos + md_size > buf_size ) {
				break;
			}
			child_md.assign( reinterpret_cast<const char *>( headers_data.bytes + pos ), md_size );
			pos += md_size;
		} else if ( md_index < indexed_md.size() ) {
			child_md = indexed_md[ md_index ];
		} else {
			break;
		}

		// Recurse into the child object group.
		abc_ogawa_group_t child_group;
		if ( ABC_GroupGetChildGroup( obj_group, child_obj_idx, &child_group ) ) {
			const std::string child_path = full_path + "/" + child_name;
			ABC_DiscoverPolyMeshesRecursive( child_group, child_name, child_path, child_md, indexed_md, out_meshes );
		}

		child_obj_idx++;
	}
}



/**
*
*
*	Per-Frame PolyMesh Triangulation, Normal Synthesis, and UV Generation:
*
*
**/
/**
*	@brief	Unpack and triangulate a single frame `frame_idx` of an `abc_polymesh_node_t`.
*	@param	mesh_node	Discovered `IPolyMesh` node.
*	@param	indexed_md	Archive-wide indexed metadata table.
*	@param	frame_idx	Zero-based animation frame index.
*	@param	out_frame	[out] Unpacked positions, normals, UVs, and triangle indices for this frame.
*	@return	True if the frame was decoded (empty frames with 0 triangles are allowed and return true).
**/
static bool ABC_UnpackPolyMeshFrame(
	const abc_polymesh_node_t &mesh_node,
	const std::vector<std::string> &indexed_md,
	const uint32_t frame_idx,
	abc_unpacked_frame_t *out_frame ) {
	/**
	*	Clear output frame containers.
	**/
	out_frame->positions.clear();
	out_frame->normals.clear();
	out_frame->tex_coords.clear();
	out_frame->velocities.clear();
	out_frame->indices.clear();

	/**
	*	Fetch required `P`, `.faceCounts`, and `.faceIndices` properties for this frame.
	**/
	const abc_property_t *p_prop = ABC_FindProperty( mesh_node.geom_props, "P" );
	const abc_property_t *fc_prop = ABC_FindProperty( mesh_node.geom_props, ".faceCounts" );
	const abc_property_t *fi_prop = ABC_FindProperty( mesh_node.geom_props, ".faceIndices" );
	if ( !p_prop || !fc_prop || !fi_prop ) {
		return false;
	}

	std::vector<float> raw_positions;
	std::vector<int32_t> face_counts;
	std::vector<int32_t> face_indices;
	if ( !ABC_ReadFloatArraySample( *p_prop, frame_idx, 3, &raw_positions ) ||
		!ABC_ReadInt32ArraySample( *fc_prop, frame_idx, &face_counts ) ||
		!ABC_ReadInt32ArraySample( *fi_prop, frame_idx, &face_indices ) ) {
		return false;
	}

	const uint32_t num_raw_verts = ( uint32_t )( raw_positions.size() / 3 );
	const uint32_t num_corners = ( uint32_t )face_indices.size();

	// In fluid simulations, early or late frames may legitimately have 0 vertices/faces.
	if ( num_raw_verts < 3 || face_counts.empty() || num_corners < 3 ) {
		return true;
	}

	/**
	*	Fetch optional `N` (normals) and `uv` (texture coordinates) for this frame.
	**/
	std::vector<float> raw_normals;
	std::vector<int32_t> normal_indices;
	const abc_property_t *n_prop = ABC_FindProperty( mesh_node.geom_props, "N" );
	const bool has_normals = ( n_prop != nullptr ) &&
		ABC_ReadGeomParamSample( *n_prop, indexed_md, frame_idx, 3, &raw_normals, &normal_indices ) &&
		( raw_normals.size() >= 9 );
	const uint32_t num_raw_normals = ( uint32_t )( raw_normals.size() / 3 );

	std::vector<float> raw_uvs;
	std::vector<int32_t> uv_indices;
	const abc_property_t *uv_prop = ABC_FindProperty( mesh_node.geom_props, "uv" );
	const bool has_uvs = ( uv_prop != nullptr ) &&
		ABC_ReadGeomParamSample( *uv_prop, indexed_md, frame_idx, 2, &raw_uvs, &uv_indices ) &&
		( raw_uvs.size() >= 6 );
	const uint32_t num_raw_uvs = ( uint32_t )( raw_uvs.size() / 2 );

	/**
	*	Fetch optional `.velocities` for this frame.
	**/
	std::vector<float> raw_velocities;
	const abc_property_t *vel_prop = ABC_FindProperty( mesh_node.geom_props, ".velocities" );
	const bool has_velocities = ( vel_prop != nullptr ) &&
		ABC_ReadFloatArraySample( *vel_prop, frame_idx, 3, &raw_velocities ) &&
		( raw_velocities.size() >= 3 );
	const uint32_t num_raw_velocities = ( uint32_t )( raw_velocities.size() / 3 );

	/**
	*	Determine whether normals or UVs are face-varying (requiring per-corner vertex deduplication)
	*	or vertex-varying / absent (allowing direct 1:1 vertex indexing from `P`).
	**/
	const bool normals_face_varying = has_normals &&
		( !normal_indices.empty() || ( num_raw_normals == num_corners && num_raw_normals != num_raw_verts ) );
	const bool uvs_face_varying = has_uvs &&
		( !uv_indices.empty() || ( num_raw_uvs == num_corners && num_raw_uvs != num_raw_verts ) );
	const bool needs_corner_split = normals_face_varying || uvs_face_varying;

	/**
	*	Helper lambdas to fetch the normal and UV for a given face corner `corner_idx` and position `pos_idx`.
	**/
	auto fetch_corner_normal = [&]( const uint32_t corner_idx, const uint32_t pos_idx, vec3_t out_n ) -> bool {
		if ( !has_normals ) {
			VectorClear( out_n );
			return false;
		}
		uint32_t n_idx = 0;
		if ( !normal_indices.empty() ) {
			if ( corner_idx >= normal_indices.size() || normal_indices[ corner_idx ] < 0 ) {
				return false;
			}
			n_idx = ( uint32_t )normal_indices[ corner_idx ];
		} else if ( num_raw_normals == num_corners ) {
			n_idx = corner_idx;
		} else if ( num_raw_normals == num_raw_verts ) {
			n_idx = pos_idx;
		} else {
			return false;
		}
		if ( n_idx >= num_raw_normals ) {
			return false;
		}
		// Convert Alembic coordinates (-raw_Y, raw_X, raw_Z) to Quake model space (+Forward, +Left, +Up).
		out_n[ 0 ] = -raw_normals[ n_idx * 3 + 1 ];
		out_n[ 1 ] =  raw_normals[ n_idx * 3 + 0 ];
		out_n[ 2 ] =  raw_normals[ n_idx * 3 + 2 ];
		return true;
	};

	auto fetch_corner_uv = [&]( const uint32_t corner_idx, const uint32_t pos_idx, vec2_t out_uv ) -> bool {
		if ( !has_uvs ) {
			out_uv[ 0 ] = 0.0f;
			out_uv[ 1 ] = 0.0f;
			return false;
		}
		uint32_t u_idx = 0;
		if ( !uv_indices.empty() ) {
			if ( corner_idx >= uv_indices.size() || uv_indices[ corner_idx ] < 0 ) {
				return false;
			}
			u_idx = ( uint32_t )uv_indices[ corner_idx ];
		} else if ( num_raw_uvs == num_corners ) {
			u_idx = corner_idx;
		} else if ( num_raw_uvs == num_raw_verts ) {
			u_idx = pos_idx;
		} else {
			return false;
		}
		if ( u_idx >= num_raw_uvs ) {
			return false;
		}
		out_uv[ 0 ] = raw_uvs[ u_idx * 2 + 0 ];
		// Flip V from Alembic bottom-left convention to Quake top-left texture convention.
		out_uv[ 1 ] = 1.0f - raw_uvs[ u_idx * 2 + 1 ];
		return true;
	};

	/**
	*	Build vertex arrays and triangulate polygon faces.
	*	Track raw vertex indices per output vertex so smooth normals can be accumulated across UV seams.
	**/
	std::vector<int32_t> raw_pos_for_vert;
	if ( !needs_corner_split ) {
		// Direct 1:1 vertex mapping with `P`.
		if ( num_raw_verts > ABC_MAX_VERTS_PER_FRAME ) {
			Com_WPrintf( "MOD_LoadABC_RTX: frame %u vertex count %u exceeds max %u\n",
				frame_idx, num_raw_verts, ( uint32_t )ABC_MAX_VERTS_PER_FRAME );
			return false;
		}

		out_frame->positions.resize( num_raw_verts );
		out_frame->normals.resize( num_raw_verts );
		out_frame->tex_coords.resize( num_raw_verts );
		out_frame->velocities.resize( num_raw_verts );
		raw_pos_for_vert.resize( num_raw_verts );

		for ( uint32_t v = 0; v < num_raw_verts; v++ ) {
			// Record raw position index for smooth normal accumulation.
			raw_pos_for_vert[ v ] = ( int32_t )v;
			// Convert Alembic coordinates (-raw_Y, raw_X, raw_Z) to Quake model space (+Forward, +Left, +Up).
			out_frame->positions[ v ][ 0 ] = -raw_positions[ v * 3 + 1 ];
			out_frame->positions[ v ][ 1 ] =  raw_positions[ v * 3 + 0 ];
			out_frame->positions[ v ][ 2 ] =  raw_positions[ v * 3 + 2 ];
			fetch_corner_normal( v, v, out_frame->normals[ v ] );
			fetch_corner_uv( v, v, out_frame->tex_coords[ v ] );

			if ( has_velocities && v < num_raw_velocities ) {
				// Convert Alembic velocities (-raw_Vy, raw_Vx, raw_Vz) to Quake model space (+Forward, +Left, +Up).
				out_frame->velocities[ v ][ 0 ] = -raw_velocities[ v * 3 + 1 ];
				out_frame->velocities[ v ][ 1 ] =  raw_velocities[ v * 3 + 0 ];
				out_frame->velocities[ v ][ 2 ] =  raw_velocities[ v * 3 + 2 ];
			} else {
				out_frame->velocities[ v ][ 0 ] = 0.0f;
				out_frame->velocities[ v ][ 1 ] = 0.0f;
				out_frame->velocities[ v ][ 2 ] = 0.0f;
			}
		}

		// Triangulate each polygon face as a triangle fan.
		uint32_t corner_offset = 0;
		for ( const int32_t face_deg : face_counts ) {
			if ( face_deg < 3 || corner_offset + ( uint32_t )face_deg > num_corners ) {
				corner_offset += std::max( 0, face_deg );
				continue;
			}

			const int32_t i0 = face_indices[ corner_offset ];
			if ( i0 < 0 || ( uint32_t )i0 >= num_raw_verts ) {
				corner_offset += ( uint32_t )face_deg;
				continue;
			}

			for ( int32_t k = 1; k + 1 < face_deg; k++ ) {
				const int32_t i1 = face_indices[ corner_offset + k + 1 ];
				const int32_t i2 = face_indices[ corner_offset + k ];
				if ( i1 < 0 || ( uint32_t )i1 >= num_raw_verts ||
					i2 < 0 || ( uint32_t )i2 >= num_raw_verts ) {
					continue;
				}
				if ( i0 == i1 || i1 == i2 || i2 == i0 ) {
					continue;
				}

				out_frame->indices.push_back( i0 );
				out_frame->indices.push_back( i1 );
				out_frame->indices.push_back( i2 );
			}

			corner_offset += ( uint32_t )face_deg;
		}
	} else {
		// Face-varying normals or UVs: deduplicate (pos_idx, normal, uv) tuples using a hash-chain per raw position.
		std::vector<int32_t> head_for_pos( num_raw_verts, -1 );
		std::vector<int32_t> next_vert;
		next_vert.reserve( num_corners );

		out_frame->positions.reserve( std::min<uint32_t>( num_corners, ABC_MAX_VERTS_PER_FRAME ) );
		out_frame->normals.reserve( std::min<uint32_t>( num_corners, ABC_MAX_VERTS_PER_FRAME ) );
		out_frame->tex_coords.reserve( std::min<uint32_t>( num_corners, ABC_MAX_VERTS_PER_FRAME ) );
		out_frame->velocities.reserve( std::min<uint32_t>( num_corners, ABC_MAX_VERTS_PER_FRAME ) );
		raw_pos_for_vert.reserve( std::min<uint32_t>( num_corners, ABC_MAX_VERTS_PER_FRAME ) );

		// Helper that finds or inserts a deduplicated vertex for a polygon corner.
		auto find_or_add_corner_vertex = [&]( const uint32_t corner_idx ) -> int32_t {
			const int32_t raw_pos_idx = face_indices[ corner_idx ];
			if ( raw_pos_idx < 0 || ( uint32_t )raw_pos_idx >= num_raw_verts ) {
				return -1;
			}

			vec3_t n = { 0.0f, 0.0f, 0.0f };
			vec2_t uv = { 0.0f, 0.0f };
			fetch_corner_normal( corner_idx, ( uint32_t )raw_pos_idx, n );
			fetch_corner_uv( corner_idx, ( uint32_t )raw_pos_idx, uv );

			// Search existing vertices sharing `raw_pos_idx`.
			for ( int32_t v = head_for_pos[ raw_pos_idx ]; v != -1; v = next_vert[ v ] ) {
				if ( has_normals ) {
					const float dn0 = fabsf( out_frame->normals[ v ][ 0 ] - n[ 0 ] );
					const float dn1 = fabsf( out_frame->normals[ v ][ 1 ] - n[ 1 ] );
					const float dn2 = fabsf( out_frame->normals[ v ][ 2 ] - n[ 2 ] );
					if ( dn0 > 1e-4f || dn1 > 1e-4f || dn2 > 1e-4f ) {
						continue;
					}
				}
				if ( has_uvs ) {
					const float du = fabsf( out_frame->tex_coords[ v ][ 0 ] - uv[ 0 ] );
					const float dv = fabsf( out_frame->tex_coords[ v ][ 1 ] - uv[ 1 ] );
					if ( du > 1e-5f || dv > 1e-5f ) {
						continue;
					}
				}
				return v;
			}

			// Allocate a new vertex if under the per-frame limit.
			if ( out_frame->positions.size() >= ABC_MAX_VERTS_PER_FRAME ) {
				return -1;
			}

			const int32_t new_v = ( int32_t )out_frame->positions.size();
			// Convert Alembic coordinates (-raw_Y, raw_X, raw_Z) to Quake model space (+Forward, +Left, +Up).
			vec3_t pos = {
				-raw_positions[ raw_pos_idx * 3 + 1 ],
				 raw_positions[ raw_pos_idx * 3 + 0 ],
				 raw_positions[ raw_pos_idx * 3 + 2 ]
			};
			out_frame->positions.push_back( { pos[ 0 ], pos[ 1 ], pos[ 2 ] } );
			out_frame->normals.push_back( { n[ 0 ], n[ 1 ], n[ 2 ] } );
			out_frame->tex_coords.push_back( { uv[ 0 ], uv[ 1 ] } );
			raw_pos_for_vert.push_back( raw_pos_idx );

			vec3_t vel = { 0.0f, 0.0f, 0.0f };
			if ( has_velocities && ( uint32_t )raw_pos_idx < num_raw_velocities ) {
				// Convert Alembic velocities (-raw_Vy, raw_Vx, raw_Vz) to Quake model space (+Forward, +Left, +Up).
				vel[ 0 ] = -raw_velocities[ raw_pos_idx * 3 + 1 ];
				vel[ 1 ] =  raw_velocities[ raw_pos_idx * 3 + 0 ];
				vel[ 2 ] =  raw_velocities[ raw_pos_idx * 3 + 2 ];
			}
			out_frame->velocities.push_back( { vel[ 0 ], vel[ 1 ], vel[ 2 ] } );

			next_vert.push_back( head_for_pos[ raw_pos_idx ] );
			head_for_pos[ raw_pos_idx ] = new_v;
			return new_v;
		};

		// Triangulate each polygon face using the deduplicated corner vertices.
		uint32_t corner_offset = 0;
		for ( const int32_t face_deg : face_counts ) {
			if ( face_deg < 3 || corner_offset + ( uint32_t )face_deg > num_corners ) {
				corner_offset += std::max( 0, face_deg );
				continue;
			}

			const int32_t i0 = find_or_add_corner_vertex( corner_offset );
			if ( i0 < 0 ) {
				corner_offset += ( uint32_t )face_deg;
				continue;
			}

			for ( int32_t k = 1; k + 1 < face_deg; k++ ) {
				const int32_t i1 = find_or_add_corner_vertex( corner_offset + k + 1 );
				const int32_t i2 = find_or_add_corner_vertex( corner_offset + k );
				if ( i1 < 0 || i2 < 0 || i0 == i1 || i1 == i2 || i2 == i0 ) {
					continue;
				}

				out_frame->indices.push_back( i0 );
				out_frame->indices.push_back( i1 );
				out_frame->indices.push_back( i2 );
			}

			corner_offset += ( uint32_t )face_deg;
		}
	}

	const uint32_t num_verts = ( uint32_t )out_frame->positions.size();
	const uint32_t num_tris = ( uint32_t )( out_frame->indices.size() / 3 );
	if ( num_tris == 0 || num_verts == 0 ) {
		out_frame->positions.clear();
		out_frame->normals.clear();
		out_frame->tex_coords.clear();
		out_frame->indices.clear();
		return true;
	}

	if ( num_tris > ABC_MAX_TRIS_PER_FRAME ) {
		Com_WPrintf( "MOD_LoadABC_RTX: frame %u triangle count %u exceeds max %u\n",
			frame_idx, num_tris, ( uint32_t )ABC_MAX_TRIS_PER_FRAME );
		return false;
	}

	/**
	*	Verify outward triangle winding and generate/normalize smooth vertex normals.
	**/
	if ( has_normals ) {
		// Flip any individual triangle whose geometric normal disagrees with its authored vertex normals.
		for ( uint32_t t = 0; t < num_tris; t++ ) {
			const int32_t i0 = out_frame->indices[ t * 3 + 0 ];
			const int32_t i1 = out_frame->indices[ t * 3 + 1 ];
			const int32_t i2 = out_frame->indices[ t * 3 + 2 ];

			vec3_t e1, e2, fn, avg_n;
			VectorSubtract( out_frame->positions[ i1 ], out_frame->positions[ i0 ], e1 );
			VectorSubtract( out_frame->positions[ i2 ], out_frame->positions[ i0 ], e2 );
			CrossProduct( e1, e2, fn );

			VectorAdd( out_frame->normals[ i0 ], out_frame->normals[ i1 ], avg_n );
			VectorAdd( avg_n, out_frame->normals[ i2 ], avg_n );

			// Swap winding if geometric normal opposes authored shading normal so VKPT does not flip geo_normal.
			if ( DotProduct( fn, avg_n ) < -1e-6f ) {
				std::swap( out_frame->indices[ t * 3 + 1 ], out_frame->indices[ t * 3 + 2 ] );
			}
		}

		// Ensure all authored normals are unit-length.
		for ( uint32_t v = 0; v < num_verts; v++ ) {
			if ( VectorNormalize( out_frame->normals[ v ] ) < 1e-5f ) {
				VectorSet( out_frame->normals[ v ], 0.0f, 0.0f, 1.0f );
			}
		}
	} else {
		// Measure signed volume to determine whether the archive stores inward- or outward-wound faces.
		double signed_volume = 0.0;
		for ( uint32_t t = 0; t < num_tris; t++ ) {
			const int32_t i0 = out_frame->indices[ t * 3 + 0 ];
			const int32_t i1 = out_frame->indices[ t * 3 + 1 ];
			const int32_t i2 = out_frame->indices[ t * 3 + 2 ];

			vec3_t e1, e2, area_normal;
			VectorSubtract( out_frame->positions[ i1 ], out_frame->positions[ i0 ], e1 );
			VectorSubtract( out_frame->positions[ i2 ], out_frame->positions[ i0 ], e2 );
			CrossProduct( e1, e2, area_normal );

			signed_volume += ( double )DotProduct( out_frame->positions[ i0 ], area_normal );
		}

		// Invert all triangles if signed volume is negative so front-facing normals point outward.
		if ( signed_volume < 0.0 ) {
			for ( uint32_t t = 0; t < num_tris; t++ ) {
				std::swap( out_frame->indices[ t * 3 + 1 ], out_frame->indices[ t * 3 + 2 ] );
			}
		}

		// Accumulate area-weighted face normals per raw position to stay smooth across UV seams.
		std::vector<std::array<float, 3>> raw_accum( num_raw_verts, { 0.0f, 0.0f, 0.0f } );
		for ( uint32_t t = 0; t < num_tris; t++ ) {
			const int32_t i0 = out_frame->indices[ t * 3 + 0 ];
			const int32_t i1 = out_frame->indices[ t * 3 + 1 ];
			const int32_t i2 = out_frame->indices[ t * 3 + 2 ];

			vec3_t e1, e2, area_normal;
			VectorSubtract( out_frame->positions[ i1 ], out_frame->positions[ i0 ], e1 );
			VectorSubtract( out_frame->positions[ i2 ], out_frame->positions[ i0 ], e2 );
			CrossProduct( e1, e2, area_normal );

			const int32_t tri_verts[ 3 ] = { i0, i1, i2 };
			for ( int32_t c = 0; c < 3; c++ ) {
				const int32_t raw_idx = raw_pos_for_vert[ tri_verts[ c ] ];
				if ( raw_idx >= 0 && ( uint32_t )raw_idx < num_raw_verts ) {
					raw_accum[ raw_idx ][ 0 ] += area_normal[ 0 ];
					raw_accum[ raw_idx ][ 1 ] += area_normal[ 1 ];
					raw_accum[ raw_idx ][ 2 ] += area_normal[ 2 ];
				}
			}
		}

		// Normalize shared raw normals and assign each output vertex its smoothed unit normal.
		for ( uint32_t r = 0; r < num_raw_verts; r++ ) {
			if ( VectorNormalize( raw_accum[ r ].data() ) < 1e-5f ) {
				raw_accum[ r ] = { 0.0f, 0.0f, 1.0f };
			}
		}
		for ( uint32_t v = 0; v < num_verts; v++ ) {
			const int32_t raw_idx = raw_pos_for_vert[ v ];
			if ( raw_idx >= 0 && ( uint32_t )raw_idx < num_raw_verts ) {
				VectorCopy( raw_accum[ raw_idx ].data(), out_frame->normals[ v ] );
			} else {
				VectorSet( out_frame->normals[ v ], 0.0f, 0.0f, 1.0f );
			}
		}
	}

	/**
	*	If UVs were NOT provided in the `.abc` file, generate normalized bounding-box / spherical UVs
	*	so that PBR textures and `vkpt_compute_missing_model_tangents` always have non-degenerate UVs.
	**/
	if ( !has_uvs ) {
		vec3_t bmin, bmax;
		ClearBounds( bmin, bmax );
		for ( uint32_t v = 0; v < num_verts; v++ ) {
			AddPointToBounds( out_frame->positions[ v ], bmin, bmax );
		}

		vec3_t span;
		VectorSubtract( bmax, bmin, span );
		const float inv_sx = ( span[ 0 ] > 1e-4f ) ? ( 1.0f / span[ 0 ] ) : 1.0f;
		const float inv_sy = ( span[ 1 ] > 1e-4f ) ? ( 1.0f / span[ 1 ] ) : 1.0f;
		const float inv_sz = ( span[ 2 ] > 1e-4f ) ? ( 1.0f / span[ 2 ] ) : 1.0f;

		for ( uint32_t v = 0; v < num_verts; v++ ) {
			const float nx = ( out_frame->positions[ v ][ 0 ] - bmin[ 0 ] ) * inv_sx;
			const float ny = ( out_frame->positions[ v ][ 1 ] - bmin[ 1 ] ) * inv_sy;
			const float nz = ( out_frame->positions[ v ][ 2 ] - bmin[ 2 ] ) * inv_sz;
			out_frame->tex_coords[ v ][ 0 ] = nx + 0.5f * nz;
			out_frame->tex_coords[ v ][ 1 ] = ny + 0.5f * nz;
		}
	}

	return true;
}



/**
*
*
*	Public Model Loader Entrypoint (MOD_LoadABC_RTX):
*
*
**/
/**
*	@brief	Load an Alembic (`.abc`) Ogawa model into `model_t` for the VKPT renderer.
*	@param	model		Target `model_t` structure to populate.
*	@param	rawdata		Pointer to the in-memory `.abc` file bytes loaded via `FS_LoadFile`.
*	@param	length		Size of `rawdata` in bytes.
*	@param	mod_name	Virtual filesystem path of the model (e.g. `"models/fx/blood_splat_01.abc"`).
*	@return	`Q_ERR_SUCCESS` (0) on success, or a negative `Q_ERR_*` error code on failure.
**/
extern "C" int MOD_LoadABC_RTX( model_t *model, const void *rawdata, size_t length, const char *mod_name ) {
	/**
	*	Sanity checks: validate pointers and minimum 16-byte Ogawa file header.
	**/
	// Reject null pointers or files smaller than the 16-byte Ogawa header.
	if ( !model || !rawdata || length < 16 ) {
		return Q_ERR_FILE_TOO_SMALL;
	}

	const uint8_t *bytes = static_cast<const uint8_t *>( rawdata );

	// Verify the 5-byte "Ogawa" ASCII signature.
	if ( LittleLong( *reinterpret_cast<const uint32_t *>( bytes ) ) != ABC_IDENT ||
		bytes[ 4 ] != ABC_MAGIC_BYTE4 ) {
		return Q_ERR_UNKNOWN_FORMAT;
	}

	// Verify the big-endian uint16_t Ogawa version at bytes 6..7 (must be 0x0001).
	const uint16_t ogawa_version = ( ( uint16_t )bytes[ 6 ] << 8 ) | ( uint16_t )bytes[ 7 ];
	if ( ogawa_version != ABC_OGAWA_VERSION ) {
		Com_WPrintf( "MOD_LoadABC_RTX: '%s' has unsupported Ogawa version %u (expected %u)\n",
			mod_name, ( uint32_t )ogawa_version, ( uint32_t )ABC_OGAWA_VERSION );
		return Q_ERR_UNKNOWN_FORMAT;
	}

	/**
	*	Parse the root Ogawa group at the 64-bit file offset stored at bytes 8..15.
	**/
	const uint64_t root_group_offset = ABC_ReadUnalignedLE<uint64_t>( bytes + 8 );
	abc_ogawa_group_t root_group;
	if ( !ABC_ReadGroup( bytes, ( uint64_t )length, root_group_offset, &root_group ) ||
		root_group.num_children < 6 ) {
		return Q_ERR_INVALID_FORMAT;
	}

	// Validate AbcCoreOgawa file version (child 0) and Alembic library version (child 1).
	abc_ogawa_data_t version_data;
	abc_ogawa_data_t file_version_data;
	if ( !ABC_GroupGetChildData( root_group, 0, &version_data ) || version_data.size != sizeof( int32_t ) ||
		!ABC_GroupGetChildData( root_group, 1, &file_version_data ) || file_version_data.size != sizeof( int32_t ) ) {
		return Q_ERR_INVALID_FORMAT;
	}

	const int32_t archive_version = ABC_ReadUnalignedLE<int32_t>( version_data.bytes );
	const int32_t library_version = ABC_ReadUnalignedLE<int32_t>( file_version_data.bytes );
	if ( archive_version != 0 || library_version < 9999 ) {
		return Q_ERR_UNKNOWN_FORMAT;
	}

	/**
	*	Read the indexed MetaData table (root child 5) and discover all `IPolyMesh` objects
	*	under the top-level Object group (root child 2).
	**/
	abc_ogawa_data_t md_table_data;
	std::vector<std::string> indexed_md;
	if ( !ABC_GroupGetChildData( root_group, 5, &md_table_data ) ||
		!ABC_ParseIndexedMetaData( md_table_data, &indexed_md ) ) {
		return Q_ERR_INVALID_FORMAT;
	}

	abc_ogawa_group_t top_object_group;
	if ( !ABC_GroupGetChildGroup( root_group, 2, &top_object_group ) ) {
		return Q_ERR_INVALID_FORMAT;
	}

	std::vector<abc_polymesh_node_t> poly_meshes;
	ABC_DiscoverPolyMeshesRecursive( top_object_group, "", "", "", indexed_md, &poly_meshes );
	if ( poly_meshes.empty() ) {
		Com_WPrintf( "MOD_LoadABC_RTX: '%s' contains no IPolyMesh objects\n", mod_name );
		return Q_ERR_TOO_FEW;
	}

	/**
	*	Determine the model's total animation frame count across all discovered `IPolyMesh` nodes.
	**/
	uint32_t num_frames = 1;
	for ( const abc_polymesh_node_t &node : poly_meshes ) {
		num_frames = std::max( num_frames, node.num_samples );
	}
	num_frames = std::min<uint32_t>( num_frames, ABC_MAX_FRAMES );

	/**
	*	Unpack all frames for each `IPolyMesh` and verify that at least one triangle exists.
	**/
	const int32_t num_meshes = ( int32_t )poly_meshes.size();
	std::vector<std::vector<abc_unpacked_frame_t>> unpacked_meshes( ( size_t )num_meshes );
	size_t total_model_verts = 0;
	size_t total_model_tris = 0;

	for ( int32_t m = 0; m < num_meshes; m++ ) {
		unpacked_meshes[ m ].resize( num_frames );
		for ( uint32_t f = 0; f < num_frames; f++ ) {
			if ( !ABC_UnpackPolyMeshFrame( poly_meshes[ m ], indexed_md, f, &unpacked_meshes[ m ][ f ] ) ) {
				return Q_ERR_INVALID_FORMAT;
			}
			total_model_verts += unpacked_meshes[ m ][ f ].positions.size();
			total_model_tris += unpacked_meshes[ m ][ f ].indices.size() / 3;
		}
	}

	if ( total_model_tris == 0 || total_model_verts == 0 ) {
		model->type = model_s::MOD_EMPTY;
		return Q_ERR_SUCCESS;
	}

	/**
	*	Resolve base path and model stem for companion PBR material lookup.
	*	For example, `models/fx/blood_splat_01.abc` resolves `models/fx/blood_splat_01.pcx`
	*	or `models/fx/<mesh_name>.pcx` via `MAT_Find`.
	**/
	char base_dir[ MAX_QPATH ] = {};
	COM_FilePath( mod_name, base_dir, sizeof( base_dir ) );

	char model_stem[ MAX_QPATH ] = {};
	COM_StripExtension( model_stem, mod_name, sizeof( model_stem ) );

	/**
	*	Compute required hunk allocation size and allocate `model->frames` and `model->meshes`.
	**/
	size_t hunk_bytes = ( size_t )num_meshes * sizeof( maliasmesh_t )
		+ ( size_t )num_frames * sizeof( maliasframe_t )
		+ total_model_verts * ( sizeof( vec3_t ) * 4 + sizeof( vec2_t ) )
		+ total_model_tris * 3 * sizeof( int32_t )
		+ ( size_t )num_meshes * ( size_t )num_frames * sizeof( int32_t ) * 5
		+ ( size_t )num_meshes * sizeof( pbr_material_t * )
		+ ( 1u << 20 ); // 1 MB headroom for alignment and light poly extraction

	int32_t ret = Q_ERR_SUCCESS;
	Hunk_Begin( &model->hunk, std::max<size_t>( hunk_bytes, 0x400000u ) );
	model->type = model_s::MOD_ALIAS;
	model->numframes = ( int32_t )num_frames;
	model->nummeshes = num_meshes;

	CHECK( model->meshes = static_cast<maliasmesh_t *>( MOD_Malloc( sizeof( maliasmesh_t ) * num_meshes ) ) );
	CHECK( model->frames = static_cast<maliasframe_t *>( MOD_Malloc( sizeof( maliasframe_t ) * num_frames ) ) );

	/**
	*	Initialize per-frame bounding boxes (`model->frames`).
	**/
	for ( uint32_t f = 0; f < num_frames; f++ ) {
		maliasframe_t *dst_frame = &model->frames[ f ];
		VectorSet( dst_frame->scale, 1.0f, 1.0f, 1.0f );
		VectorClear( dst_frame->translate );
		ClearBounds( dst_frame->bounds[ 0 ], dst_frame->bounds[ 1 ] );
		dst_frame->radius = 0.0f;
	}

	/**
	*	Populate each `maliasmesh_t` with concatenated per-frame vertex/index buffers and slice tables.
	**/
	for ( int32_t m = 0; m < num_meshes; m++ ) {
		maliasmesh_t *dst_mesh = &model->meshes[ m ];
		memset( dst_mesh, 0, sizeof( *dst_mesh ) );

		// Sum total vertices and triangles across all frames of this mesh.
		int32_t mesh_total_verts = 0;
		int32_t mesh_total_tris = 0;
		for ( uint32_t f = 0; f < num_frames; f++ ) {
			mesh_total_verts += ( int32_t )unpacked_meshes[ m ][ f ].positions.size();
			mesh_total_tris += ( int32_t )( unpacked_meshes[ m ][ f ].indices.size() / 3 );
		}

		dst_mesh->numverts = mesh_total_verts;
		dst_mesh->numtris = mesh_total_tris;
		dst_mesh->numindices = mesh_total_tris * 3;
		dst_mesh->tri_offset = 0;

		// Allocate flat vertex/index buffers and per-frame slice descriptor arrays on the model hunk.
		if ( mesh_total_verts > 0 && mesh_total_tris > 0 ) {
			CHECK( dst_mesh->positions = static_cast<vec3_t *>( MOD_Malloc( ( size_t )mesh_total_verts * sizeof( vec3_t ) ) ) );
			CHECK( dst_mesh->normals = static_cast<vec3_t *>( MOD_Malloc( ( size_t )mesh_total_verts * sizeof( vec3_t ) ) ) );
			CHECK( dst_mesh->tex_coords = static_cast<vec2_t *>( MOD_Malloc( ( size_t )mesh_total_verts * sizeof( vec2_t ) ) ) );
			CHECK( dst_mesh->velocities = static_cast<vec3_t *>( MOD_Malloc( ( size_t )mesh_total_verts * sizeof( vec3_t ) ) ) );
			CHECK( dst_mesh->indices = static_cast<int *>( MOD_Malloc( ( size_t )mesh_total_tris * 3 * sizeof( int ) ) ) );
		}

		CHECK( dst_mesh->frame_vert_offsets = static_cast<int32_t *>( MOD_Malloc( ( size_t )num_frames * sizeof( int32_t ) ) ) );
		CHECK( dst_mesh->frame_numverts = static_cast<int32_t *>( MOD_Malloc( ( size_t )num_frames * sizeof( int32_t ) ) ) );
		CHECK( dst_mesh->frame_index_offsets = static_cast<int32_t *>( MOD_Malloc( ( size_t )num_frames * sizeof( int32_t ) ) ) );
		CHECK( dst_mesh->frame_numtris = static_cast<int32_t *>( MOD_Malloc( ( size_t )num_frames * sizeof( int32_t ) ) ) );
		CHECK( dst_mesh->frame_tri_offsets = static_cast<int32_t *>( MOD_Malloc( ( size_t )num_frames * sizeof( int32_t ) ) ) );

		// Copy each frame's vertices and indices into the flat mesh arrays and record offsets.
		int32_t running_vert_offset = 0;
		int32_t running_index_offset = 0;
		for ( uint32_t f = 0; f < num_frames; f++ ) {
			const abc_unpacked_frame_t &src_frame = unpacked_meshes[ m ][ f ];
			const int32_t f_verts = ( int32_t )src_frame.positions.size();
			const int32_t f_tris = ( int32_t )( src_frame.indices.size() / 3 );

			dst_mesh->frame_vert_offsets[ f ] = running_vert_offset;
			dst_mesh->frame_numverts[ f ] = f_verts;
			dst_mesh->frame_index_offsets[ f ] = running_index_offset;
			dst_mesh->frame_numtris[ f ] = f_tris;
			dst_mesh->frame_tri_offsets[ f ] = 0;

			if ( f_verts > 0 ) {
				std::memcpy( dst_mesh->positions + running_vert_offset, src_frame.positions.data(), ( size_t )f_verts * sizeof( vec3_t ) );
				std::memcpy( dst_mesh->normals + running_vert_offset, src_frame.normals.data(), ( size_t )f_verts * sizeof( vec3_t ) );
				std::memcpy( dst_mesh->tex_coords + running_vert_offset, src_frame.tex_coords.data(), ( size_t )f_verts * sizeof( vec2_t ) );
				if ( !src_frame.velocities.empty() ) {
					std::memcpy( dst_mesh->velocities + running_vert_offset, src_frame.velocities.data(), ( size_t )f_verts * sizeof( vec3_t ) );
				} else {
					std::memset( dst_mesh->velocities + running_vert_offset, 0, ( size_t )f_verts * sizeof( vec3_t ) );
				}

				// Expand the model's per-frame bounding box.
				maliasframe_t *dst_model_frame = &model->frames[ f ];
				for ( int32_t v = 0; v < f_verts; v++ ) {
					AddPointToBounds( src_frame.positions[ v ], dst_model_frame->bounds[ 0 ], dst_model_frame->bounds[ 1 ] );
				}
			}

			if ( f_tris > 0 ) {
				std::memcpy( dst_mesh->indices + running_index_offset, src_frame.indices.data(), ( size_t )f_tris * 3 * sizeof( int32_t ) );
			}

			running_vert_offset += f_verts;
			running_index_offset += f_tris * 3;
		}

		// Resolve PBR material for this mesh (`<model_stem>.pcx` or `<base_dir>/<mesh_name>.pcx`).
		char skin_path[ MAX_QPATH ] = {};
		if ( num_meshes > 1 && !poly_meshes[ m ].name.empty() ) {
			Q_snprintf( skin_path, sizeof( skin_path ), "%s/%s.pcx", base_dir, poly_meshes[ m ].name.c_str() );
		} else {
			Q_snprintf( skin_path, sizeof( skin_path ), "%s.pcx", model_stem );
		}

		pbr_material_t *mat = MAT_Find( skin_path, IT_SKIN, IF_NONE );
		CHECK( dst_mesh->materials = static_cast<pbr_material_t **>( MOD_Malloc( sizeof( pbr_material_t * ) ) ) );
		dst_mesh->materials[ 0 ] = mat;
		dst_mesh->numskins = 1;
	}

	/**
	*	Finalize per-frame bounding radii, compute missing tangent vectors, and seal the model hunk.
	**/
	for ( uint32_t f = 0; f < num_frames; f++ ) {
		maliasframe_t *dst_frame = &model->frames[ f ];
		if ( dst_frame->bounds[ 0 ][ 0 ] > dst_frame->bounds[ 1 ][ 0 ] ) {
			VectorClear( dst_frame->bounds[ 0 ] );
			VectorClear( dst_frame->bounds[ 1 ] );
			dst_frame->radius = 0.0f;
		} else {
			dst_frame->radius = RadiusFromBounds( dst_frame->bounds[ 0 ], dst_frame->bounds[ 1 ] );
		}
	}

	vkpt_compute_missing_model_tangents( model );
	vkpt_extract_model_lights( model );

	Hunk_End( &model->hunk );
	return Q_ERR_SUCCESS;

fail:
	Hunk_Free( &model->hunk );
	return ret;
}
