/********************************************************************
*
*
*	Refresh VKPT: Decal Scaffolding.
*
*
********************************************************************/
#include "refresh/vkpt/decals/vkpt_decals.h"

//! Master enable cvar for vkpt decal subsystem.
static cvar_t *cvar_pt_decals_enable = NULL;

//! Tracks initialization state for guard checks.
static qboolean s_vkpt_decals_initialized = false;
//! Tracks active enable state from cvar or explicit toggle.
static qboolean s_vkpt_decals_enabled = false;
//! Tracks the CLGame-controlled render mode hint for decal dispatch.
static int32_t s_vkpt_decals_render_mode = VKPT_DECAL_RENDER_DISABLED;
//! Tracks received decal requests for status and smoke testing.
static uint32_t s_vkpt_decals_submitted = 0;



/**
*
*
*
*	Debug Decal Work:
*
*
*
**/
//! Uncomment to enable logging of decal state summaries at key points in the workflow for debugging purposes.
//#define VKPT_DECALS_DEBUG_LOG_STATE 1

/**
*	@brief	Emit a compact summary of the current decal renderer state.
*	@note	Used to confirm which GPU-backed decal resources are still live during map transitions.
**/
static void vkpt_decals_log_state( const char *reason ) {
	#ifdef VKPT_DECALS_DEBUG_LOG_STATE
	const char *tag = ( reason && reason[ 0 ] ) ? reason : "state";
	Com_WPrintf(
		"vkpt: decals %s init=%d enabled=%d mode=%d submitted=%u\n",
		tag,
		s_vkpt_decals_initialized ? 1 : 0,
		s_vkpt_decals_enabled ? 1 : 0,
		s_vkpt_decals_render_mode,
		s_vkpt_decals_submitted );
	#else
	( void )reason;
	#endif
}



/**
*
*
*
*	Core Decal Work:
*
*
*
**/
/**
*	@brief	Initializes the vkpt decal subsystem and all related resources.
**/
VkResult vkpt_decals_initialize( void ) {
	// Guard against redundant initialization.
	cvar_pt_decals_enable = Cvar_Get( "pt_decals_enable", "1", CVAR_ARCHIVE );

	// Start with a clean state in case of re-initialization after a map transition.
	s_vkpt_decals_initialized = true;
	s_vkpt_decals_enabled = ( cvar_pt_decals_enable && cvar_pt_decals_enable->integer != 0 ) ? true : false;
	s_vkpt_decals_render_mode = VKPT_DECAL_RENDER_DISABLED;
	s_vkpt_decals_submitted = 0;

	// Initialize decal rendering modes in the order they would be submitted by CLGame to ensure correct fallback behavior if one of the modes fails to initialize.
	_VK( vkpt_decals_geometry_initialize() );
	_VK( vkpt_decals_screenspace_initialize() );

	// Log the final state after initialization for debugging purposes.
	return VK_SUCCESS;
}

/**
*	@brief	Shuts down the vkpt decal subsystem and releases all related resources.
**/
VkResult vkpt_decals_shutdown( void ) {
	vkpt_decals_log_state( "shutdown-before" );
	vkpt_decals_screenspace_shutdown();
	vkpt_decals_geometry_shutdown();

	s_vkpt_decals_initialized = false;
	s_vkpt_decals_enabled = false;
	s_vkpt_decals_render_mode = VKPT_DECAL_RENDER_DISABLED;
	s_vkpt_decals_submitted = 0;
	cvar_pt_decals_enable = NULL;
	vkpt_decals_log_state( "shutdown-after" );

	return VK_SUCCESS;
}

/**
*	@brief	Clears decals that are only meant to persist for a single frame, 
*			such as those submitted through the mesh API, while preserving any 
*			legacy markers that were submitted directly as decals for screen-space mode.
*	@param  
**/
void vkpt_decals_clear_transient( void ) {
	vkpt_decals_log_state( "clear_transient-before" );
	s_vkpt_decals_submitted = 0;
	vkpt_decals_geometry_clear_transient();
	(void)vkpt_decals_geometry_upload( NULL, 0u, NULL, 0u );
	vkpt_decals_screenspace_clear();
	vkpt_decals_log_state( "clear_transient-after" );
}

void vkpt_decals_clear( void ) {
	vkpt_decals_log_state( "clear-before" );
	s_vkpt_decals_submitted = 0;
	vkpt_decals_geometry_clear();
	(void)vkpt_decals_geometry_upload( NULL, 0u, NULL, 0u );
	vkpt_decals_screenspace_clear();
	vkpt_decals_log_state( "clear-after" );
}



/**
*
*
*
*	Properties:
*
*
*
**/
void vkpt_decals_set_enabled( const qboolean enabled ) {
	s_vkpt_decals_enabled = enabled ? true : false;
}

void vkpt_decals_set_render_mode( const int32_t renderMode ) {
	s_vkpt_decals_render_mode = renderMode;
}

const int32_t vkpt_decals_get_render_mode( void ) {
	return s_vkpt_decals_render_mode;
}



/**
*
*
*
*	Decal Submission:
*
*
*
**/
void vkpt_decals_submit( const decal_t *decal ) {
	if ( !s_vkpt_decals_initialized || !decal ) {
		return;
	}

	// Keep runtime enable state in sync with cvar changes after initialization.
	if ( cvar_pt_decals_enable ) {
		s_vkpt_decals_enabled = ( cvar_pt_decals_enable->integer != 0 ) ? true : false;
	}

	s_vkpt_decals_submitted++;

	vkpt_decals_geometry_submit_legacy( decal );

	// Phase 2 screen-space validation consumes the legacy decal payload path.
	vkpt_decals_screenspace_submit_legacy( decal );
}

/**
*	@brief	Builds a legacy decal marker from one submitted mesh payload.
*	@param	vertices	Mesh vertices provided by CLGame.
*	@param	vertexCount	Number of vertices in the mesh payload.
*	@param	albedo		Mesh albedo tint.
*	@param	alpha		Mesh alpha tint.
*	@param	materialHash	Stable material hash for texture lookup.
*	@param	outDecal	[out] Legacy marker payload for screen-space mode.
*	@return	True if marker generation succeeded.
**/
static qboolean vkpt_decals_build_legacy_marker_from_mesh( const decal_mesh_vertex_t *vertices, const int32_t vertexCount, const vec3_t albedo, const float alpha, const uint32_t materialHash, decal_t *outDecal ) {
	if ( !vertices || vertexCount < 3 || !outDecal ) {
		if ( vertexCount < 3 ) {
			Com_WPrintf( "vkpt: legacy decal build called with insufficient vertices – ignoring.\n" );
			return false;   // abort the creation instead of leaving a zeroed decal
		}
		return false;
	}

	memset( outDecal, 0, sizeof( *outDecal ) );

	/**
	*	Accumulate centroid and representative normal from submitted mesh vertices.
	**/
	for ( int32_t i = 0; i < vertexCount; i++ ) {
		outDecal->pos[ 0 ] += vertices[ i ].position[ 0 ];
		outDecal->pos[ 1 ] += vertices[ i ].position[ 1 ];
		outDecal->pos[ 2 ] += vertices[ i ].position[ 2 ];

		outDecal->dir[ 0 ] += vertices[ i ].normal[ 0 ];
		outDecal->dir[ 1 ] += vertices[ i ].normal[ 1 ];
		outDecal->dir[ 2 ] += vertices[ i ].normal[ 2 ];
	}

	const float invVertexCount = 1.0f / ( float )vertexCount;
	outDecal->pos[ 0 ] *= invVertexCount;
	outDecal->pos[ 1 ] *= invVertexCount;
	outDecal->pos[ 2 ] *= invVertexCount;
	outDecal->dir[ 0 ] *= invVertexCount;
	outDecal->dir[ 1 ] *= invVertexCount;
	outDecal->dir[ 2 ] *= invVertexCount;

	/**
	*	Normalize marker normal so downstream consumers can assume unit length.
	**/
	if ( VectorLength( outDecal->dir ) <= 0.00001 ) {
		VectorSet( outDecal->dir, 0.0f, 0.0f, 1.0f );
	} else {
		VectorNormalize( outDecal->dir );
	}

	/**
	*	Estimate screen-space marker spread from mesh extent around its centroid.
	**/
	float maxDistance = 0.0f;
	for ( int32_t i = 0; i < vertexCount; i++ ) {
		vec3_t delta = { 0 };
		VectorSubtract( vertices[ i ].position, outDecal->pos, delta );
		const float distance = VectorLength( delta );
		if ( distance > maxDistance ) {
			maxDistance = distance;
		}
	}

	outDecal->spread = ( maxDistance > 0.1f ) ? maxDistance : 1.0f;
	outDecal->length = 1.0f;
	outDecal->alpha = ( alpha > 0.0f ) ? alpha : 1.0f;
	outDecal->materialHash = materialHash;
	VectorCopy( albedo, outDecal->albedo );

	return true;
}

void vkpt_decals_submit_mesh( const decal_mesh_vertex_t *vertices, int32_t vertexCount, const vec3_t albedo, float alpha, uint32_t materialHash, float lifeSeconds ) {
	if ( !s_vkpt_decals_initialized || !vertices || vertexCount < 3 ) {
		return;
	}

	/**
	*	When CLGame requests screen-space mode, convert mesh submissions back to one
	*	legacy marker so mode 1 remains visible.
	**/
	if ( vkpt_decals_get_render_mode() == VKPT_DECAL_RENDER_SCREENSPACE ) {
		decal_t legacyMarker = { 0 };
		if ( vkpt_decals_build_legacy_marker_from_mesh( vertices, vertexCount, albedo, alpha, materialHash, &legacyMarker ) ) {
			vkpt_decals_screenspace_submit_legacy( &legacyMarker );
		}
		return;
	}

	vkpt_decals_geometry_submit_mesh( vertices, vertexCount, albedo, alpha, materialHash, lifeSeconds );
}



/**
* 
* 
* 
*	Material Mapping Functionality:
* 
* 
* 
**/
void vkpt_decals_clear_material_mappings( void ) {
	vkpt_decals_geometry_clear_material_mappings();
}

void vkpt_decals_set_material_mapping( const uint32_t materialHash, const char *materialName ) {
	vkpt_decals_geometry_set_material_mapping( materialHash, materialName );
}

void vkpt_decals_dump_material_mappings( void ) {
	vkpt_decals_geometry_dump_material_mappings();
}
