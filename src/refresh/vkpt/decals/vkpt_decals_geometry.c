/********************************************************************
*
*
*	Refresh VKPT: Decal Geometry Upload/BLAS Scaffolding.
*
*
********************************************************************/
#include "refresh/vkpt/decals/vkpt_decals_geometry.h"
#include "refresh/vkpt/decals/vkpt_decals.h"
#include "refresh/images.h"
#include "refresh/vkpt/material.h"
#include "refresh/vkpt/vkpt.h"
#include "../../../client/cl_client.h"
#include <SDL_mutex.h>
#include <stddef.h>

typedef struct vkpt_decal_runtime_item_s {
	vkpt_decal_vertex_t vertices[ VKPT_DECAL_GEOMETRY_MAX_VERTICES_PER_ITEM ];
	uint32_t vertexCount;
	uint32_t spawnTimeMs;
	uint32_t lifeMs;
} vkpt_decal_runtime_item_t;

//! Tracks whether the geometry subsystem was initialized.
static qboolean s_vkpt_decals_geometry_initialized = false;
static bool s_vkpt_decals_blas_dirty = false; // Tracks whether decal BLAS needs rebuild
// Mutex to protect decal geometry subsystem state
static SDL_mutex *s_vkpt_decals_mutex = NULL;
// Removed s_decals_blas_dirty
//! Latest uploaded vertex count for phase validation/debugging.
static uint32_t s_vkpt_decals_vertex_count = 0u;
//! CPU-visible vertex buffer used as BLAS input and shader descriptor source.
static BufferResource_t s_vkpt_decal_vertex_buffer[ MAX_FRAMES_IN_FLIGHT ] = { 0 };
//! Runtime retained dynamic decal submissions.
static vkpt_decal_runtime_item_t s_vkptDynamicDecalItems[ VKPT_DECAL_GEOMETRY_DYNAMIC_MAX ] = { 0 };
//! Runtime retained dynamic decal count.
static int32_t s_vkptDynamicDecalItemCount = 0;
//! Runtime retained static decal submissions.
static vkpt_decal_runtime_item_t s_vkptStaticDecalItems[ VKPT_DECAL_GEOMETRY_STATIC_MAX ] = { 0 };
//! Runtime retained static decal count.
static int32_t s_vkptStaticDecalItemCount = 0;
//! Persistent CPU staging array used to flatten runtime decal items before upload.
static vkpt_decal_vertex_t *s_vkptGeneratedVertices = NULL;


//! Maximum amount of CLGame-configured material hash mappings cached by renderer.
#define VKPT_DECAL_MATERIAL_LOOKUP_MAX 64

typedef struct vkpt_decal_material_lookup_s {
    uint32_t hash;
    char materialName[ MAX_QPATH ]; // material name (materials/*.mat entry) or texture path
    uint32_t textureIndex; // image index in r_images (global descriptor array)
	uint32_t maskTextureIndex; // optional mask image index in r_images
    qboolean triedLoad;
} vkpt_decal_material_lookup_t;

//! Runtime mapping table configured by CLGame.
static vkpt_decal_material_lookup_t s_vkpt_decal_material_lookup[ VKPT_DECAL_MATERIAL_LOOKUP_MAX ] = { 0 };
//! Runtime mapping table count.
static int32_t s_vkpt_decal_material_lookup_count = 0;
//! One-shot warning gate to avoid flooding logs before mappings are configured.
static qboolean s_vkpt_warned_missing_material_mappings = false;
//! One-shot warning gate for mesh vertex truncation.
static qboolean s_vkpt_warned_mesh_vertex_truncation = false;
//! One-shot warning gate for runtime upload truncation.
static qboolean s_vkpt_warned_runtime_vertex_truncation = false;

//! Decal shader flag bit used to invert mask coverage on sample.
#define VKPT_DECAL_FLAG_MASK_INVERTED ( 1u << 0u )

//! Runtime debug/authoring override for decal mask inversion.
static cvar_t *cvar_pt_decals_mask_invert = NULL;

/**
*	@brief	Log the decal geometry lifetime state for map-transition diagnostics.
*	@note	Captures CPU-side retained items and the GPU vertex buffer handle/size.
**/
static void vkpt_decals_geometry_log_state( const char *reason ) {
	#if 0
	const char *tag = ( reason && reason[ 0 ] ) ? reason : "state";
	Com_WPrintf(
		"vkpt: decal-geom %s init=%d dyn=%d static=%d vertexCount=%u vb=%p size=%zu\n",
		tag,
		s_vkpt_decals_geometry_initialized ? 1 : 0,
		s_vkptDynamicDecalItemCount,
		s_vkptStaticDecalItemCount,
		s_vkpt_decals_vertex_count,
		(void *)s_vkpt_decal_vertex_buffer[0].buffer,
		(size_t)s_vkpt_decal_vertex_buffer[0].size );
	#endif
}

static void vkpt_decals_geometry_lock( void ) {
    if (s_vkpt_decals_mutex) {
        SDL_LockMutex(s_vkpt_decals_mutex);
    }
}

static void vkpt_decals_geometry_unlock( void ) {
    if (s_vkpt_decals_mutex) {
        SDL_UnlockMutex(s_vkpt_decals_mutex);
    }
}

static VkResult vkpt_decals_geometry_upload_frame( const int32_t frameIndex, const vkpt_decal_vertex_t *vertices, uint32_t vertexCount );

static uint32_t vkpt_decals_geometry_resolve_texture_index( const uint32_t materialHash );

// Fallback texture index to use when no decal texture can be resolved.
// Index 0 is reserved for the 'no texture' invalid/checker image.
#define VKPT_DECAL_FALLBACK_TEXTURE_INDEX 0u

#ifndef __cplusplus
static_assert( sizeof( vkpt_decal_vertex_t ) == 80u, "vkpt_decal_vertex_t size must match shader array stride" );
static_assert( offsetof( vkpt_decal_vertex_t, position ) == 0u, "vkpt_decal_vertex_t.position offset mismatch" );
static_assert( offsetof( vkpt_decal_vertex_t, normal ) == ( sizeof( float ) * 4u ), "vkpt_decal_vertex_t.normal offset mismatch" );
static_assert( offsetof( vkpt_decal_vertex_t, uv ) == ( sizeof( float ) * 8u ), "vkpt_decal_vertex_t.uv offset mismatch" );
static_assert( offsetof( vkpt_decal_vertex_t, albedo ) == ( sizeof( float ) * 10u ), "vkpt_decal_vertex_t.albedo offset mismatch" );
static_assert( offsetof( vkpt_decal_vertex_t, alpha ) == ( sizeof( float ) * 13u ), "vkpt_decal_vertex_t.alpha offset mismatch" );
static_assert( offsetof( vkpt_decal_vertex_t, textureIndex ) == ( sizeof( float ) * 14u ), "vkpt_decal_vertex_t.textureIndex offset mismatch" );
#endif

static void vkpt_decals_geometry_try_resolve_texture_indices_for_name( const char *materialName, uint32_t *outTextureIndex, uint32_t *outMaskTextureIndex ) {
	if ( outTextureIndex ) {
		*outTextureIndex = 0u;
	}
	if ( outMaskTextureIndex ) {
		*outMaskTextureIndex = 0u;
	}

	if ( !materialName || !materialName[ 0 ] ) {
		return;
	}

	pbr_material_t *mat = MAT_Find( materialName, IT_WALL, IF_SRGB | IF_PERMANENT );
	if ( mat ) {
		if ( outTextureIndex && mat->image_base && mat->image_base != R_NOTEXTURE ) {
			*outTextureIndex = (uint32_t)( mat->image_base - r_images );
		}
		if ( outMaskTextureIndex && mat->image_mask && mat->image_mask != R_NOTEXTURE ) {
			*outMaskTextureIndex = (uint32_t)( mat->image_mask - r_images );
		}
	}

	if ( outTextureIndex && *outTextureIndex == 0u ) {
		const image_t *image = IMG_Find( materialName, IT_WALL, IF_SRGB | IF_PERMANENT );
		if ( image && image != R_NOTEXTURE ) {
			*outTextureIndex = (uint32_t)( image - r_images );
		}
	}
}

void vkpt_decals_geometry_clear_material_mappings( void ) {
	memset( s_vkpt_decal_material_lookup, 0, sizeof( s_vkpt_decal_material_lookup ) );
	s_vkpt_decal_material_lookup_count = 0;
	s_vkpt_warned_missing_material_mappings = false;
	s_vkpt_warned_mesh_vertex_truncation = false;
	s_vkpt_warned_runtime_vertex_truncation = false;
}

void vkpt_decals_geometry_set_material_mapping( const uint32_t materialHash, const char *materialName ) {
	if ( materialHash == 0u || !materialName || !materialName[ 0 ] ) {
		return;
	}

	for ( int32_t i = 0; i < s_vkpt_decal_material_lookup_count; i++ ) {
		if ( s_vkpt_decal_material_lookup[ i ].hash == materialHash ) {
			Q_strlcpy( s_vkpt_decal_material_lookup[ i ].materialName, materialName, sizeof( s_vkpt_decal_material_lookup[ i ].materialName ) );
			s_vkpt_decal_material_lookup[ i ].textureIndex = 0u;
			s_vkpt_decal_material_lookup[ i ].maskTextureIndex = 0u;
			s_vkpt_decal_material_lookup[ i ].triedLoad = false;
			return;
		}
	}

	if ( s_vkpt_decal_material_lookup_count >= VKPT_DECAL_MATERIAL_LOOKUP_MAX ) {
		Com_WPrintf( "vkpt: decal material mapping table full (%d), dropping hash 0x%08x\n", VKPT_DECAL_MATERIAL_LOOKUP_MAX, materialHash );
		return;
	}

	vkpt_decal_material_lookup_t *lookup = &s_vkpt_decal_material_lookup[ s_vkpt_decal_material_lookup_count++ ];
	lookup->hash = materialHash;
	Q_strlcpy( lookup->materialName, materialName, sizeof( lookup->materialName ) );
	lookup->textureIndex = 0u;
	lookup->maskTextureIndex = 0u;
	lookup->triedLoad = false;
}

/**
*    @brief  Dumps the current decal material lookup table for runtime validation.
**/
void vkpt_decals_geometry_dump_material_mappings( void ) {
	Com_Printf( "[vkpt decals] material mappings: %d entries\n", s_vkpt_decal_material_lookup_count );

	for ( int32_t i = 0; i < s_vkpt_decal_material_lookup_count; i++ ) {
		vkpt_decal_material_lookup_t *lookup = &s_vkpt_decal_material_lookup[ i ];
		if ( !lookup->triedLoad ) {
			// Resolve lazily so the dump reports the same texture index the shader will use.
			lookup->textureIndex = vkpt_decals_geometry_resolve_texture_index( lookup->hash );
		}

		Com_Printf( "[vkpt decals]   0x%08x -> '%s' -> texture %u%s\n",
			lookup->hash,
			lookup->materialName,
			lookup->textureIndex,
			( lookup->textureIndex == VKPT_DECAL_FALLBACK_TEXTURE_INDEX ) ? " (fallback)" : "" );
		Com_Printf( "[vkpt decals]                 mask texture %u%s\n",
			lookup->maskTextureIndex,
			( lookup->maskTextureIndex == 0u ) ? " (none)" : "" );
	}
}

static int32_t vkpt_decals_geometry_find_material_lookup( const uint32_t materialHash ) {
	for ( int32_t i = 0; i < s_vkpt_decal_material_lookup_count; i++ ) {
		if ( s_vkpt_decal_material_lookup[ i ].hash == materialHash ) {
			return i;
		}
	}

	return -1;
}

/**
*    @brief  Resolves one decal material hash to a global texture descriptor index.
*    @param  materialHash Stable material hash provided by CLGame.
*    @return Global texture index for shader sampling; 0 on fallback.
**/
static uint32_t vkpt_decals_geometry_resolve_texture_index( const uint32_t materialHash ) {
	if ( s_vkpt_decal_material_lookup_count <= 0 ) {
		if ( !s_vkpt_warned_missing_material_mappings ) {
			s_vkpt_warned_missing_material_mappings = true;
			Com_WPrintf( "vkpt: no decal material mappings configured; using fallback texture index %u\n", VKPT_DECAL_FALLBACK_TEXTURE_INDEX );
		}
		return VKPT_DECAL_FALLBACK_TEXTURE_INDEX;
	}

	const int32_t lookupIndex = vkpt_decals_geometry_find_material_lookup( materialHash );

	if ( lookupIndex < 0 ) {
		Com_WPrintf( "vkpt: decal material hash 0x%08x has no mapping; using fallback texture index %u\n", materialHash, VKPT_DECAL_FALLBACK_TEXTURE_INDEX );
		return VKPT_DECAL_FALLBACK_TEXTURE_INDEX;
	}

	vkpt_decal_material_lookup_t *lookup = &s_vkpt_decal_material_lookup[ lookupIndex ];
	if ( !lookup->triedLoad ) {
		lookup->triedLoad = true;
		lookup->textureIndex = 0u;
		lookup->maskTextureIndex = 0u;

		vkpt_decals_geometry_try_resolve_texture_indices_for_name( lookup->materialName, &lookup->textureIndex, &lookup->maskTextureIndex );
		if ( lookup->textureIndex == 0u && lookup->materialName[ 0 ] == '/' ) {
			vkpt_decals_geometry_try_resolve_texture_indices_for_name( lookup->materialName + 1, &lookup->textureIndex, &lookup->maskTextureIndex );
		}

		if ( lookup->textureIndex == 0u ) {
			Com_WPrintf( "vkpt: decal material '%s' resolved to no texture; using fallback index %u\n", lookup->materialName, VKPT_DECAL_FALLBACK_TEXTURE_INDEX );
			lookup->textureIndex = VKPT_DECAL_FALLBACK_TEXTURE_INDEX;
		}

		// Ensure the texture descriptors are updated so shaders can sample the (resolved or fallback) image.
		vkpt_invalidate_texture_descriptors();
	}

	return lookup->textureIndex;
}

/**
*    @brief  Resolves one decal material hash to an optional mask texture descriptor index.
*    @param  materialHash Stable material hash provided by CLGame.
*    @return Global mask texture index; 0 when no mask is available.
**/
static uint32_t vkpt_decals_geometry_resolve_mask_texture_index( const uint32_t materialHash ) {
	if ( s_vkpt_decal_material_lookup_count <= 0 ) {
		return 0u;
	}

	const int32_t lookupIndex = vkpt_decals_geometry_find_material_lookup( materialHash );
	if ( lookupIndex < 0 ) {
		return 0u;
	}

	vkpt_decal_material_lookup_t *lookup = &s_vkpt_decal_material_lookup[ lookupIndex ];
	if ( !lookup->triedLoad ) {
		(void)vkpt_decals_geometry_resolve_texture_index( materialHash );
	}

	return lookup->maskTextureIndex;
}

/**
*    @brief  Ensures one host-visible GPU buffer has at least requested size.
*    @param  buffer Buffer to allocate or resize.
*    @param  requiredSize Required byte size.
*    @param  usage Vulkan buffer usage flags.
*    @return Vulkan result code.
**/
static VkResult vkpt_decals_geometry_ensure_buffer( BufferResource_t *buffer, const size_t requiredSize, const VkBufferUsageFlags usage ) {
	if ( !buffer || requiredSize == 0 ) {
		return VK_SUCCESS;
	}

	if ( buffer->buffer && buffer->size >= requiredSize ) {
		return VK_SUCCESS;
	}

	if ( buffer->buffer ) {
		buffer_destroy( buffer );
	}

	return buffer_create( buffer,
		requiredSize,
		usage | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
}

/**
*    @brief  Builds an orthonormal basis around decal normal.
*    @param  normal Input decal normal.
*    @param  outRight [out] Tangent vector.
*    @param  outUp [out] Bitangent vector.
**/
static void vkpt_decals_geometry_build_basis( const vec3_t normal, vec3_t outRight, vec3_t outUp ) {
	vec3_t n = { normal[ 0 ], normal[ 1 ], normal[ 2 ] };
	if ( VectorLength( n ) <= 0.001f ) {
		VectorSet( n, 0.0f, 0.0f, 1.0f );
	}
	VectorNormalize( n );

	vec3_t reference = { 0.0f, 0.0f, 1.0f };
	if ( fabsf( DotProduct( n, reference ) ) > 0.95f ) {
		VectorSet( reference, 0.0f, 1.0f, 0.0f );
	}

	CrossProduct( reference, n, outRight );
	if ( VectorLength( outRight ) <= 0.001f ) {
		VectorSet( outRight, 1.0f, 0.0f, 0.0f );
	}
	VectorNormalize( outRight );

	CrossProduct( n, outRight, outUp );
	VectorNormalize( outUp );
}

/**
*    @brief  Removes expired dynamic decal runtime submissions.
*    @param  nowMs Current wall-clock time in milliseconds.
**/
static void vkpt_decals_geometry_prune_dynamic( const uint32_t nowMs ) {
	int32_t writeIndex = 0;
	for ( int32_t i = 0; i < s_vkptDynamicDecalItemCount; i++ ) {
		vkpt_decal_runtime_item_t *item = &s_vkptDynamicDecalItems[ i ];
		if ( item->lifeMs > 0u && nowMs - item->spawnTimeMs > item->lifeMs ) {
			continue;
		}

		if ( writeIndex != i ) {
			s_vkptDynamicDecalItems[ writeIndex ] = *item;
		}
		writeIndex++;
	}
	s_vkptDynamicDecalItemCount = writeIndex;
}

/**
*    @brief  Appends one decal quad as two triangles to output vertex array.
*    @param  decal Legacy decal payload.
*    @param  outVertices Destination vertex array.
*    @param  inOutVertexCount [in/out] Current vertex count.
*    @param  maxVertices Maximum destination vertices.
**/
static void vkpt_decals_geometry_append_decal_quad( const decal_t *decal, vkpt_decal_vertex_t *outVertices, uint32_t *inOutVertexCount, const uint32_t maxVertices ) {
	if ( !decal || !outVertices || !inOutVertexCount ) {
		return;
	}

	if ( *inOutVertexCount + 6u > maxVertices ) {
		return;
	}

	vec3_t right = { 0 };
	vec3_t up = { 0 };
	vkpt_decals_geometry_build_basis( decal->dir, right, up );
	const uint32_t textureIndex = vkpt_decals_geometry_resolve_texture_index( decal->materialHash );
	const uint32_t maskTextureIndex = vkpt_decals_geometry_resolve_mask_texture_index( decal->materialHash );
	const uint32_t decalFlags = ( cvar_pt_decals_mask_invert && cvar_pt_decals_mask_invert->integer != 0 ) ? VKPT_DECAL_FLAG_MASK_INVERTED : 0u;
	const float normalBias = 0.10f;

	const float halfSize = ( decal->spread > 0.05f ) ? decal->spread : 4.0f;

	vec3_t corners[ 4 ] = {
		{ -halfSize, -halfSize, 0.0f },
		{ halfSize, -halfSize, 0.0f },
		{ halfSize, halfSize, 0.0f },
		{ -halfSize, halfSize, 0.0f },
	};

	const vec2_t uvs[ 4 ] = {
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f },
	};

	const int32_t triCornerOrder[ 6 ] = { 0, 1, 2, 0, 2, 3 };
	for ( int32_t i = 0; i < 6; i++ ) {
		const int32_t cornerIndex = triCornerOrder[ i ];
		vkpt_decal_vertex_t *vertex = &outVertices[ ( *inOutVertexCount )++ ];
		memset( vertex, 0, sizeof( *vertex ) );

		vec3_t position = { decal->pos[ 0 ], decal->pos[ 1 ], decal->pos[ 2 ] };
		VectorMA( position, corners[ cornerIndex ][ 0 ], right, position );
		VectorMA( position, corners[ cornerIndex ][ 1 ], up, position );
		VectorMA( position, normalBias, decal->dir, position );

		vertex->position[ 0 ] = position[ 0 ];
		vertex->position[ 1 ] = position[ 1 ];
		vertex->position[ 2 ] = position[ 2 ];

		vertex->normal[ 0 ] = decal->dir[ 0 ];
		vertex->normal[ 1 ] = decal->dir[ 1 ];
		vertex->normal[ 2 ] = decal->dir[ 2 ];

		vertex->uv[ 0 ] = uvs[ cornerIndex ][ 0 ];
		vertex->uv[ 1 ] = uvs[ cornerIndex ][ 1 ];

		vertex->albedo[ 0 ] = decal->albedo[ 0 ];
		vertex->albedo[ 1 ] = decal->albedo[ 1 ];
		vertex->albedo[ 2 ] = decal->albedo[ 2 ];

		vertex->alpha = ( decal->alpha > 0.0f ) ? decal->alpha : 1.0f;
		vertex->textureIndex = textureIndex;
		vertex->maskTextureIndex = maskTextureIndex;
		vertex->decalFlags = decalFlags;
	}
}

static void vkpt_decals_geometry_submit_runtime_vertices( const vkpt_decal_vertex_t *vertices, const uint32_t vertexCount, const uint32_t lifeMs, const qboolean isStatic ) {
	if ( !vertices || vertexCount < 3u ) {
		return;
	}

	vkpt_decal_runtime_item_t *targetItems = isStatic ? s_vkptStaticDecalItems : s_vkptDynamicDecalItems;
	int32_t *targetItemCount = isStatic ? &s_vkptStaticDecalItemCount : &s_vkptDynamicDecalItemCount;
	const int32_t targetCapacity = isStatic ? VKPT_DECAL_GEOMETRY_STATIC_MAX : VKPT_DECAL_GEOMETRY_DYNAMIC_MAX;

	if ( *targetItemCount >= targetCapacity ) {
		// Keep the pool bounded by evicting the oldest submission before appending the new one.
		if ( !s_vkpt_warned_runtime_vertex_truncation ) {
			s_vkpt_warned_runtime_vertex_truncation = true;
			Com_WPrintf( "vkpt: decal runtime item pool full (%d); dropping oldest submission before append\n", targetCapacity );
		}

		memmove( &targetItems[ 0 ], &targetItems[ 1 ], sizeof( targetItems[ 0 ] ) * ( targetCapacity - 1 ) );
		*targetItemCount = targetCapacity - 1;
	}

	vkpt_decal_runtime_item_t *item = &targetItems[ ( *targetItemCount )++ ];
	const uint32_t copyCount = ( vertexCount > VKPT_DECAL_GEOMETRY_MAX_VERTICES_PER_ITEM ) ? VKPT_DECAL_GEOMETRY_MAX_VERTICES_PER_ITEM : vertexCount;
	if ( copyCount != vertexCount && !s_vkpt_warned_runtime_vertex_truncation ) {
		 s_vkpt_warned_runtime_vertex_truncation = true;
		 Com_WPrintf( "vkpt: decal runtime vertex payload truncated from %u to %u vertices\n", vertexCount, copyCount );
	}
	memcpy( item->vertices, vertices, sizeof( vkpt_decal_vertex_t ) * copyCount );
	item->vertexCount = copyCount;
	item->spawnTimeMs = Sys_Milliseconds();
	item->lifeMs = isStatic ? 0u : lifeMs;
	// Any runtime vertex submission may change the BLAS contents.
	s_vkpt_decals_blas_dirty = true;
}

VkResult vkpt_decals_geometry_initialize( void ) {
    vkpt_decals_geometry_log_state( "init-before" );
    // Create mutex for thread safety
    if (!s_vkpt_decals_mutex) {
        s_vkpt_decals_mutex = SDL_CreateMutex();
    }
    s_vkpt_decals_geometry_initialized = true;
    s_vkpt_decals_vertex_count = 0u;
    s_vkptDynamicDecalItemCount = 0;
    s_vkptStaticDecalItemCount = 0;
    cvar_pt_decals_mask_invert = Cvar_Get( "pt_decals_mask_invert", "0", CVAR_ARCHIVE );
    memset( s_vkpt_decal_vertex_buffer, 0, sizeof( s_vkpt_decal_vertex_buffer ) );

	if ( !s_vkptGeneratedVertices ) {
		s_vkptGeneratedVertices = (vkpt_decal_vertex_t *)Z_Malloc( sizeof( vkpt_decal_vertex_t ) * VKPT_DECAL_GEOMETRY_MAX_VERTICES );
		if ( !s_vkptGeneratedVertices ) {
			s_vkpt_decals_geometry_initialized = false;
			Com_WPrintf( "vkpt: failed to allocate persistent CPU decal staging array (%u vertices)\n", VKPT_DECAL_GEOMETRY_MAX_VERTICES );
			return VK_ERROR_OUT_OF_HOST_MEMORY;
		}
	}

	// Allocate the full decal working set up front so later submissions only rewrite contents.
	for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
		VkResult result = vkpt_decals_geometry_ensure_buffer( &s_vkpt_decal_vertex_buffer[ i ], sizeof( vkpt_decal_vertex_t ) * VKPT_DECAL_GEOMETRY_MAX_VERTICES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT );
		if ( result != VK_SUCCESS ) {
			Com_WPrintf( "vkpt: failed to allocate persistent decal vertex buffer for frame %d (%u vertices)\n", i, VKPT_DECAL_GEOMETRY_MAX_VERTICES );
			s_vkpt_decals_geometry_initialized = false;
			vkpt_decals_geometry_log_state( "init-failed" );
			return result;
		}
	}

	vkpt_decals_geometry_log_state( "init-after" );
	return VK_SUCCESS;
}

void vkpt_decals_geometry_clear_transient( void ) {
	if ( !s_vkpt_decals_geometry_initialized ) {
		return;
	}

	vkpt_decals_geometry_lock();

	s_vkptDynamicDecalItemCount = 0;
	s_vkptStaticDecalItemCount = 0;
	s_vkpt_decals_vertex_count = 0u;
	// Clearing transient decals invalidates the BLAS.
	s_vkpt_decals_blas_dirty = true;

	vkpt_decals_geometry_unlock();
}

void vkpt_decals_geometry_clear( void ) {
	if ( !s_vkpt_decals_geometry_initialized ) {
		return;
	}

	vkpt_decals_geometry_lock();
	vkpt_decals_geometry_log_state( "clear-before" );

	/**
	*    Clear retained runtime submission counters so old map decals cannot be rebuilt into the
	*    next frame BLAS after a client/map state reset.
	*
	*    Do not memset the retained item arrays here: renderer rebuilds can overlap with this
	*    call path on another thread, and zeroing the shared staging data was corrupting in-flight
	*    decal/BLAS rebuild work.
	**/
	s_vkptDynamicDecalItemCount = 0;
	s_vkptStaticDecalItemCount = 0;
	s_vkpt_decals_vertex_count = 0u;
	// Clearing all decals means BLAS must be rebuilt (or cleared).
	s_vkpt_decals_blas_dirty = true;


	// Reset resolved texture indices since the image library is rebuilt on map changes
	for ( int32_t i = 0; i < s_vkpt_decal_material_lookup_count; i++ ) {
		s_vkpt_decal_material_lookup[ i ].textureIndex = 0u;
		s_vkpt_decal_material_lookup[ i ].maskTextureIndex = 0u;
		s_vkpt_decal_material_lookup[ i ].triedLoad = false;
	}

	vkpt_decals_geometry_log_state( "clear-after" );
	vkpt_decals_geometry_unlock();
}

void vkpt_decals_geometry_shutdown( void ) {
    vkpt_decals_geometry_lock();
    vkpt_decals_geometry_log_state( "shutdown-before" );
    // Ensure all GPU work is finished before tearing down resources
    vkDeviceWaitIdle(qvk.device);
    for ( int32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        (void)vkpt_pt_create_decal_blas( NULL, i, NULL, 0, 0, NULL, 0, 0 );
    }
    for ( int32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        if ( s_vkpt_decal_vertex_buffer[ i ].buffer ) {
            buffer_destroy( &s_vkpt_decal_vertex_buffer[ i ] );
        }
    }
    if ( s_vkptGeneratedVertices ) {
        Z_Free( s_vkptGeneratedVertices );
        s_vkptGeneratedVertices = NULL;
    }
    vkpt_decals_geometry_clear_material_mappings();

    s_vkpt_decals_geometry_initialized = false;
    s_vkpt_decals_vertex_count = 0u;
    s_vkptDynamicDecalItemCount = 0;
    s_vkptStaticDecalItemCount = 0;
    vkDeviceWaitIdle(qvk.device);
    vkpt_decals_geometry_log_state( "shutdown-after" );
    // Destroy mutex
    if (s_vkpt_decals_mutex) {
        SDL_DestroyMutex(s_vkpt_decals_mutex);
        s_vkpt_decals_mutex = NULL;
    }
    vkpt_decals_geometry_unlock();
}

void vkpt_decals_geometry_submit_legacy( const decal_t *decal ) {
	if ( !s_vkpt_decals_geometry_initialized || !decal ) {
		return;
	}

	vkpt_decals_geometry_lock();
	vkpt_decal_vertex_t quadVertices[ 6 ] = { 0 };
	uint32_t quadVertexCount = 0u;
	vkpt_decals_geometry_append_decal_quad( decal, quadVertices, &quadVertexCount, (uint32_t)LENGTH( quadVertices ) );
	vkpt_decals_geometry_submit_runtime_vertices( quadVertices, quadVertexCount, VKPT_DECAL_GEOMETRY_LIFE_MS, false );
	s_vkpt_decals_blas_dirty = true; // New decal added, mark BLAS dirty
	vkpt_decals_geometry_unlock();
}

void vkpt_decals_geometry_submit_mesh( const decal_mesh_vertex_t *vertices, int32_t vertexCount, const vec3_t albedo, float alpha, uint32_t materialHash, float lifeSeconds ) {
	if ( !s_vkpt_decals_geometry_initialized || !vertices || vertexCount < 3 ) {
		return;
	}

	vkpt_decals_geometry_lock();
	const int32_t clampedVertexCount = ( vertexCount > (int32_t)VKPT_DECAL_GEOMETRY_MAX_VERTICES_PER_ITEM )
		? (int32_t)VKPT_DECAL_GEOMETRY_MAX_VERTICES_PER_ITEM
		: vertexCount;
	if ( clampedVertexCount != vertexCount && !s_vkpt_warned_mesh_vertex_truncation ) {
		s_vkpt_warned_mesh_vertex_truncation = true;
		Com_WPrintf( "vkpt: decal mesh payload truncated from %d to %d vertices\n", vertexCount, clampedVertexCount );
	}
	const int32_t triangleAlignedVertexCount = ( clampedVertexCount / 3 ) * 3;
	if ( triangleAlignedVertexCount < 3 ) {
		goto cleanup;
	}

	vkpt_decal_vertex_t convertedVertices[ VKPT_DECAL_GEOMETRY_MAX_VERTICES_PER_ITEM ] = { 0 };
	const uint32_t textureIndex = vkpt_decals_geometry_resolve_texture_index( materialHash );
	const uint32_t maskTextureIndex = vkpt_decals_geometry_resolve_mask_texture_index( materialHash );
	const uint32_t decalFlags = ( cvar_pt_decals_mask_invert && cvar_pt_decals_mask_invert->integer != 0 ) ? VKPT_DECAL_FLAG_MASK_INVERTED : 0u;
	const float resolvedAlpha = ( alpha > 0.0f ) ? alpha : 1.0f;
	const float normalBias = 0.10f;

	for ( int32_t i = 0; i < triangleAlignedVertexCount; i++ ) {
		vkpt_decal_vertex_t *dst = &convertedVertices[ i ];
		const decal_mesh_vertex_t *src = &vertices[ i ];

		dst->position[ 0 ] = src->position[ 0 ] + ( src->normal[ 0 ] * normalBias );
		dst->position[ 1 ] = src->position[ 1 ] + ( src->normal[ 1 ] * normalBias );
		dst->position[ 2 ] = src->position[ 2 ] + ( src->normal[ 2 ] * normalBias );

		dst->normal[ 0 ] = src->normal[ 0 ];
		dst->normal[ 1 ] = src->normal[ 1 ];
		dst->normal[ 2 ] = src->normal[ 2 ];

		dst->uv[ 0 ] = src->uv[ 0 ];
		dst->uv[ 1 ] = src->uv[ 1 ];

		dst->albedo[ 0 ] = albedo[ 0 ];
		dst->albedo[ 1 ] = albedo[ 1 ];
		dst->albedo[ 2 ] = albedo[ 2 ];

		dst->alpha = resolvedAlpha;
		dst->textureIndex = textureIndex;
		dst->maskTextureIndex = maskTextureIndex;
		dst->decalFlags = decalFlags;
	}

	qboolean isStaticSubmission = false;
	uint32_t lifeMs = VKPT_DECAL_GEOMETRY_LIFE_MS;
	if ( lifeSeconds > 0.0f ) {
		lifeMs = (uint32_t)( lifeSeconds * 1000.0f );
		if ( lifeMs == 0u ) {
			lifeMs = 1u;
		}
	} else {
		isStaticSubmission = true;
		lifeMs = 0u;
	}

	vkpt_decals_geometry_submit_runtime_vertices( convertedVertices, (uint32_t)triangleAlignedVertexCount, lifeMs, isStaticSubmission );
	s_vkpt_decals_blas_dirty = true; // New or updated decal mesh, mark BLAS dirty

cleanup:
	vkpt_decals_geometry_unlock();
}

void vkpt_decals_geometry_get_descriptor_buffer_info( const int32_t frameIndex, VkDescriptorBufferInfo *outVertexBufferInfo ) {
	if ( !outVertexBufferInfo ) {
		return;
	}

	outVertexBufferInfo->buffer = s_vkpt_decal_vertex_buffer[ frameIndex ].buffer;
	outVertexBufferInfo->offset = 0;
	outVertexBufferInfo->range = ( s_vkpt_decal_vertex_buffer[ frameIndex ].size > 0 ) ? s_vkpt_decal_vertex_buffer[ frameIndex ].size : (VkDeviceSize)( sizeof( vkpt_decal_vertex_t ) * 3u );
}

VkResult vkpt_decals_geometry_upload( const vkpt_decal_vertex_t *vertices, uint32_t vertexCount, const uint16_t *indices, uint32_t indexCount ) {
	(void)indices;
	(void)indexCount;

	if ( !s_vkpt_decals_geometry_initialized ) {
		return VK_SUCCESS;
	}

	if ( !vertices || vertexCount == 0u ) {
		s_vkpt_decals_vertex_count = 0u;
		return VK_SUCCESS;
	}

	vkpt_decals_geometry_lock();
	const VkResult uploadResult = vkpt_decals_geometry_upload_frame( 0, vertices, vertexCount );
	vkpt_decals_geometry_unlock();
	return uploadResult;
}

static VkResult vkpt_decals_geometry_upload_frame( const int32_t frameIndex, const vkpt_decal_vertex_t *vertices, uint32_t vertexCount ) {
	const size_t vertexBytes = (size_t)vertexCount * sizeof( vkpt_decal_vertex_t );
	if ( !s_vkpt_decal_vertex_buffer[ frameIndex ].buffer || s_vkpt_decal_vertex_buffer[ frameIndex ].size < vertexBytes ) {
		Com_WPrintf( "vkpt: decal vertex upload would exceed persistent buffer size (%zu > %zu bytes)\n", vertexBytes, (size_t)s_vkpt_decal_vertex_buffer[ frameIndex ].size );
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}

	void *mappedVertices = buffer_map( &s_vkpt_decal_vertex_buffer[ frameIndex ] );
	if ( !mappedVertices ) {
		return VK_ERROR_MEMORY_MAP_FAILED;
	}
	memcpy( mappedVertices, vertices, vertexBytes );
	buffer_unmap( &s_vkpt_decal_vertex_buffer[ frameIndex ] );

	s_vkpt_decals_vertex_count = vertexCount;
	return VK_SUCCESS;
}

VkResult vkpt_decals_geometry_build_blas( VkCommandBuffer cmd_buf, const int32_t frameIndex ) {
	if ( !s_vkpt_decals_geometry_initialized ) {
		return VK_SUCCESS;
	}

	vkpt_decals_geometry_log_state( "build-entry" );
	vkpt_decals_geometry_lock();

	// If no decal data changed since last build, skip BLAS rebuild.
	if ( !s_vkpt_decals_blas_dirty ) {
		vkpt_decals_geometry_unlock();
		return VK_SUCCESS;
	}

	const uint32_t nowMs = Sys_Milliseconds();
	vkpt_decals_geometry_prune_dynamic( nowMs );

	if ( cls.state < ca_active || vkpt_decals_get_render_mode() != VKPT_DECAL_RENDER_PATH_TRACED || ( s_vkptDynamicDecalItemCount <= 0 && s_vkptStaticDecalItemCount <= 0 ) ) {
		s_vkpt_decals_vertex_count = 0u;
		vkpt_decals_geometry_log_state( "build-empty" );
		VkResult blasResult = vkpt_pt_create_decal_blas( cmd_buf, frameIndex, NULL, 0, 0, NULL, 0, 0 );
		vkpt_decals_geometry_unlock();
		return blasResult;
	}



	uint32_t generatedVertexCount = 0u;
	for ( int32_t i = 0; i < s_vkptDynamicDecalItemCount; i++ ) {
		generatedVertexCount += s_vkptDynamicDecalItems[ i ].vertexCount;
	}
	for ( int32_t i = 0; i < s_vkptStaticDecalItemCount; i++ ) {
		generatedVertexCount += s_vkptStaticDecalItems[ i ].vertexCount;
	}

	// Guard the flattened vertex staging buffer even if upstream counts become inconsistent.
	if ( generatedVertexCount > VKPT_DECAL_GEOMETRY_MAX_VERTICES ) {
		if ( !s_vkpt_warned_runtime_vertex_truncation ) {
			s_vkpt_warned_runtime_vertex_truncation = true;
			Com_WPrintf( "vkpt: decal geometry flatten would exceed staging capacity (%u > %u); truncating upload\n",
				generatedVertexCount, VKPT_DECAL_GEOMETRY_MAX_VERTICES );
		}

		generatedVertexCount = VKPT_DECAL_GEOMETRY_MAX_VERTICES;
	}

	if ( generatedVertexCount < 3u ) {
		s_vkpt_decals_vertex_count = 0u;
		vkpt_decals_geometry_log_state( "build-written-too-small" );
		vkpt_decals_geometry_unlock();
		VkResult blasResult = vkpt_pt_create_decal_blas( cmd_buf, frameIndex, NULL, 0, 0, NULL, 0, 0 );
		return blasResult;
	}

	if ( !s_vkptGeneratedVertices ) {
		vkpt_decals_geometry_unlock();
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}

	uint32_t writeVertex = 0u;
	for ( int32_t i = 0; i < s_vkptDynamicDecalItemCount; i++ ) {
		vkpt_decal_runtime_item_t *item = &s_vkptDynamicDecalItems[ i ];
		if ( item->vertexCount == 0u ) {
			continue;
		}

		if ( writeVertex + item->vertexCount > VKPT_DECAL_GEOMETRY_MAX_VERTICES ) {
			break;
		}

		memcpy( &s_vkptGeneratedVertices[ writeVertex ], item->vertices, sizeof( vkpt_decal_vertex_t ) * item->vertexCount );
		writeVertex += item->vertexCount;
	}
	for ( int32_t i = 0; i < s_vkptStaticDecalItemCount; i++ ) {
		vkpt_decal_runtime_item_t *item = &s_vkptStaticDecalItems[ i ];
		if ( item->vertexCount == 0u ) {
			continue;
		}

		if ( writeVertex + item->vertexCount > VKPT_DECAL_GEOMETRY_MAX_VERTICES ) {
			break;
		}

		memcpy( &s_vkptGeneratedVertices[ writeVertex ], item->vertices, sizeof( vkpt_decal_vertex_t ) * item->vertexCount );
		writeVertex += item->vertexCount;
	}

	if ( writeVertex < 3u ) {
		s_vkpt_decals_vertex_count = 0u;
		vkpt_decals_geometry_log_state( "build-written-too-small" );
		vkpt_decals_geometry_unlock();
		VkResult blasResult = vkpt_pt_create_decal_blas( cmd_buf, frameIndex, NULL, 0, 0, NULL, 0, 0 );
		return blasResult;
	}

	s_vkpt_decals_vertex_count = writeVertex;

	// Unlock here: we have safely captured the CPU decal items into s_vkptGeneratedVertices.
	// We no longer need to stall the client thread while uploading to Vulkan or building the BLAS.
	vkpt_decals_geometry_unlock();

	VkResult uploadResult = vkpt_decals_geometry_upload_frame( frameIndex, s_vkptGeneratedVertices, writeVertex );
	if ( uploadResult != VK_SUCCESS ) {
		vkpt_decals_geometry_log_state( "build-upload-failed" );
		return uploadResult;
	}

	// Reset dirty flag after successful build.
	s_vkpt_decals_blas_dirty = false;

	vkpt_decals_geometry_log_state( "build-uploaded" );
	VkResult blasResult = vkpt_pt_create_decal_blas( cmd_buf, frameIndex, &s_vkpt_decal_vertex_buffer[ frameIndex ], 0, s_vkpt_decals_vertex_count, NULL, 0, 0 );

	return blasResult;
}

void vkpt_decals_geometry_append_tlas_instance( const int32_t frameIndex ) {
	if ( !s_vkpt_decals_geometry_initialized ) {
		return;
	}

	if ( s_vkpt_decals_vertex_count == 0u ) {
		return;
	}

	vkpt_pt_append_decal_instance( frameIndex );
}
