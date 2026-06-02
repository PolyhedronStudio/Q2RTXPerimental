/********************************************************************
*
*
*	Refresh VKPT: Decal Scaffolding.
*
*
********************************************************************/
#pragma once

#include "refresh/vkpt/vkpt.h"
#include "refresh/vkpt/decals/vkpt_decals_geometry.h"

/**
*    @brief  Renderer-local decal render mode values fed by CLGame through R_ API.
*    @note   Values intentionally mirror CLGame/shared values but remain renderer-local.
**/
typedef enum vkpt_decal_render_mode_e {
	VKPT_DECAL_RENDER_DISABLED = 0,
	VKPT_DECAL_RENDER_SCREENSPACE = 1,
	VKPT_DECAL_RENDER_PATH_TRACED = 2
} vkpt_decal_render_mode_t;

VkResult vkpt_decals_initialize( void );
void vkpt_decals_shutdown( void );
void vkpt_decals_clear( void );
void vkpt_decals_set_enabled( const qboolean enabled );
void vkpt_decals_set_render_mode( const int32_t renderMode );
const int32_t vkpt_decals_get_render_mode( void );
void vkpt_decals_submit( const decal_t *decal );
void vkpt_decals_submit_mesh( const decal_mesh_vertex_t *vertices, int32_t vertexCount, const vec3_t albedo, float alpha, uint32_t materialHash, float lifeSeconds );
void vkpt_decals_clear_material_mappings( void );
void vkpt_decals_set_material_mapping( const uint32_t materialHash, const char *materialName );
void vkpt_decals_dump_material_mappings( void );

VkResult vkpt_decals_screenspace_initialize( void );
void vkpt_decals_screenspace_shutdown( void );
void vkpt_decals_screenspace_clear( void );
void vkpt_decals_screenspace_submit_legacy( const decal_t *decal );
VkResult vkpt_decals_screenspace_upload( const void *items, const int32_t count );
VkResult vkpt_decals_screenspace_dispatch( VkCommandBuffer cmd_buf );
