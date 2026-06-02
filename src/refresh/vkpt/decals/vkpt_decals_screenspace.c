/********************************************************************
*
*
*    Refresh VKPT: Decal Screen-Space Validation.
*
*
********************************************************************/
#include "refresh/vkpt/decals/vkpt_decals.h"

//! Maximum retained decal markers for screen-space validation.
#define VKPT_DECAL_SCREENSPACE_MAX 512

//! Retained legacy decal marker with local spawn time.
typedef struct vkpt_decal_screenspace_item_s {
	decal_t decal;
	uint64_t spawnTimeMs;
} vkpt_decal_screenspace_item_t;

//! Screen-space marker size scalar.
static cvar_t *cvar_pt_decal_screenspace_size = NULL;
//! Marker lifetime in seconds.
static cvar_t *cvar_pt_decal_screenspace_life = NULL;

//! Marker storage used by the Phase 2 validation path.
static vkpt_decal_screenspace_item_t s_vkptDecalScreenspaceItems[ VKPT_DECAL_SCREENSPACE_MAX ] = { 0 };
//! Active marker count.
static int32_t s_vkptDecalScreenspaceCount = 0;

/**
*    @brief  Projects a world-space point into the current refdef viewport.
*    @param  worldPos World-space position to project.
*    @param  outX [out] Screen X coordinate.
*    @param  outY [out] Screen Y coordinate.
*    @param  outViewDepth [out] Positive forward depth from camera.
*    @return True when the point projects to the visible screen region.
**/
static qboolean vkpt_decals_project_world_to_screen( const vec3_t worldPos, float *outX, float *outY, float *outViewDepth ) {
	if ( !vkpt_refdef.fd || !outX || !outY || !outViewDepth ) {
		return false;
	}

	vec3_t forward, right, up;
	AngleVectors( vkpt_refdef.fd->viewangles, forward, right, up );

	vec3_t toPoint;
	VectorSubtract( worldPos, vkpt_refdef.fd->vieworg, toPoint );

	const float viewDepth = DotProduct( toPoint, forward );
	if ( viewDepth <= 0.01f ) {
		return false;
	}

	const float tanHalfFovX = tanf( vkpt_refdef.fd->fov_x * 0.5f * (float)M_PI / 180.0f );
	const float tanHalfFovY = tanf( vkpt_refdef.fd->fov_y * 0.5f * (float)M_PI / 180.0f );
	if ( tanHalfFovX <= 0.0f || tanHalfFovY <= 0.0f ) {
		return false;
	}

	const float ndcX = DotProduct( toPoint, right ) / ( viewDepth * tanHalfFovX );
	const float ndcY = DotProduct( toPoint, up ) / ( viewDepth * tanHalfFovY );
	if ( ndcX < -1.2f || ndcX > 1.2f || ndcY < -1.2f || ndcY > 1.2f ) {
		return false;
	}

	*outX = (float)vkpt_refdef.fd->x + ( ( ndcX * 0.5f ) + 0.5f ) * (float)vkpt_refdef.fd->width;
	*outY = (float)vkpt_refdef.fd->y + ( 0.5f - ( ndcY * 0.5f ) ) * (float)vkpt_refdef.fd->height;
	*outViewDepth = viewDepth;
	return true;
}

/**
*    @brief  Removes expired marker entries based on configured life.
*    @param  nowMs Current wall-clock timestamp in milliseconds.
**/
static void vkpt_decals_screenspace_prune_expired( const uint32_t nowMs ) {
	const float lifeSeconds = ( cvar_pt_decal_screenspace_life && cvar_pt_decal_screenspace_life->value > 0.05f )
		? cvar_pt_decal_screenspace_life->value
		: 1.0f;
	const uint32_t lifeMs = (uint32_t)( lifeSeconds * 1000.0f );

	int32_t writeIndex = 0;
	for ( int32_t i = 0; i < s_vkptDecalScreenspaceCount; i++ ) {
		vkpt_decal_screenspace_item_t *item = &s_vkptDecalScreenspaceItems[ i ];
		if ( nowMs - item->spawnTimeMs > lifeMs ) {
			continue;
		}

		if ( writeIndex != i ) {
			s_vkptDecalScreenspaceItems[ writeIndex ] = *item;
		}
		writeIndex++;
	}

	s_vkptDecalScreenspaceCount = writeIndex;
}

VkResult vkpt_decals_screenspace_initialize( void ) {
	cvar_pt_decal_screenspace_size = Cvar_Get( "pt_decal_screenspace_size", "32.5", CVAR_ARCHIVE );
	cvar_pt_decal_screenspace_life = Cvar_Get( "pt_decal_screenspace_life", "1.25", CVAR_ARCHIVE );

	s_vkptDecalScreenspaceCount = 0;
	return VK_SUCCESS;
}

void vkpt_decals_screenspace_shutdown( void ) {
	s_vkptDecalScreenspaceCount = 0;
	cvar_pt_decal_screenspace_size = NULL;
	cvar_pt_decal_screenspace_life = NULL;
}

void vkpt_decals_screenspace_clear( void ) {
	s_vkptDecalScreenspaceCount = 0;
}

void vkpt_decals_screenspace_submit_legacy( const decal_t *decal ) {
	if ( !decal ) {
		return;
	}

	if ( s_vkptDecalScreenspaceCount >= VKPT_DECAL_SCREENSPACE_MAX ) {
		memmove( &s_vkptDecalScreenspaceItems[ 0 ],
			&s_vkptDecalScreenspaceItems[ 1 ],
			sizeof( s_vkptDecalScreenspaceItems[ 0 ] ) * ( VKPT_DECAL_SCREENSPACE_MAX - 1 ) );
		s_vkptDecalScreenspaceCount = VKPT_DECAL_SCREENSPACE_MAX - 1;
	}

	vkpt_decal_screenspace_item_t *item = &s_vkptDecalScreenspaceItems[ s_vkptDecalScreenspaceCount++ ];
	item->decal = *decal;
	item->spawnTimeMs = Sys_Milliseconds();
}

VkResult vkpt_decals_screenspace_upload( const void *items, const int32_t count ) {
	(void)items;
	(void)count;
	return VK_SUCCESS;
}

VkResult vkpt_decals_screenspace_dispatch( VkCommandBuffer cmd_buf ) {
	(void)cmd_buf;

	if ( vkpt_decals_get_render_mode() != VKPT_DECAL_RENDER_SCREENSPACE ) {
		return VK_SUCCESS;
	}

	if ( !R_DrawFill32f || !vkpt_refdef.fd ) {
		return VK_SUCCESS;
	}

	const uint32_t nowMs = Sys_Milliseconds();
	vkpt_decals_screenspace_prune_expired( nowMs );
	if ( s_vkptDecalScreenspaceCount <= 0 ) {
		return VK_SUCCESS;
	}

	const float markerScale = ( cvar_pt_decal_screenspace_size && cvar_pt_decal_screenspace_size->value > 0.1f )
		? cvar_pt_decal_screenspace_size->value
		: 3.5f;
	const float lifeSeconds = ( cvar_pt_decal_screenspace_life && cvar_pt_decal_screenspace_life->value > 0.05f )
		? cvar_pt_decal_screenspace_life->value
		: 1.0f;
	const float invLifeMs = 1.0f / ( lifeSeconds * 1000.0f );
	int32_t projectedDrawCount = 0;

	for ( int32_t i = 0; i < s_vkptDecalScreenspaceCount; i++ ) {
		const vkpt_decal_screenspace_item_t *item = &s_vkptDecalScreenspaceItems[ i ];

		float screenX = 0.0f;
		float screenY = 0.0f;
		float viewDepth = 0.0f;
		if ( !vkpt_decals_project_world_to_screen( item->decal.pos, &screenX, &screenY, &viewDepth ) ) {
			continue;
		}

		const float spread = ( item->decal.spread > 0.1f ) ? item->decal.spread : 8.0f;
		float size = markerScale * spread * ( 64.0f / ( viewDepth + 16.0f ) );
		if ( size < 2.0f ) {
			size = 2.0f;
		} else if ( size > 24.0f ) {
			size = 24.0f;
		}

		const float age01 = (float)( nowMs - item->spawnTimeMs ) * invLifeMs;
		float alpha01 = 1.0f - age01;
		if ( alpha01 < 0.0f ) {
			alpha01 = 0.0f;
		} else if ( alpha01 > 1.0f ) {
			alpha01 = 1.0f;
		}

		const uint32_t alpha = (uint32_t)( alpha01 * 220.0f );
		const uint32_t color = ( alpha << 24 ) | ( 48u << 16 ) | ( 170u << 8 ) | 255u;

		R_DrawFill32f( screenX - ( size * 0.5f ), screenY - ( size * 0.5f ), size, size, color );
		projectedDrawCount++;
	}

	// If all retained decals failed projection, draw an obvious fallback marker so Phase 2
	// validation still shows renderer activity and confirms submission is working.
	if ( projectedDrawCount == 0 ) {
		const uint32_t fallbackColor = ( 220u << 24 ) | ( 255u << 16 ) | ( 64u << 8 ) | 255u;
		const float x = (float)vkpt_refdef.fd->x + 24.0f;
		const float y = (float)vkpt_refdef.fd->y + 24.0f;
		R_DrawFill32f( x, y, 18.0f, 18.0f, fallbackColor );
	}

	return VK_SUCCESS;
}
