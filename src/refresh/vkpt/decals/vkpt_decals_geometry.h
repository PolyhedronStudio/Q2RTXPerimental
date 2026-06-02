/********************************************************************
*
*
*	Refresh VKPT: Decal Geometry Upload/BLAS Scaffolding.
*
*
********************************************************************/
#pragma once

#include "refresh/vkpt/vkpt.h"

typedef struct vkpt_decal_vertex_s {
	float position[ 3 ];
	float pad0;
	float normal[ 3 ];
	float pad1;
	float uv[ 2 ];
	float albedo[ 3 ];
	float alpha;
	uint32_t textureIndex;
	uint32_t maskTextureIndex;
	uint32_t decalFlags;
	uint32_t pad2[ 3 ];
} vkpt_decal_vertex_t;

VkResult vkpt_decals_geometry_initialize( void );
void vkpt_decals_geometry_shutdown( void );
void vkpt_decals_geometry_clear( void );
void vkpt_decals_geometry_clear_material_mappings( void );
void vkpt_decals_geometry_set_material_mapping( const uint32_t materialHash, const char *materialName );
void vkpt_decals_geometry_dump_material_mappings( void );
void vkpt_decals_geometry_submit_legacy( const decal_t *decal );
void vkpt_decals_geometry_submit_mesh( const decal_mesh_vertex_t *vertices, int32_t vertexCount, const vec3_t albedo, float alpha, uint32_t materialHash, float lifeSeconds );
void vkpt_decals_geometry_get_descriptor_buffer_info( VkDescriptorBufferInfo *outVertexBufferInfo );
VkResult vkpt_decals_geometry_upload( const vkpt_decal_vertex_t *vertices, uint32_t vertexCount, const uint16_t *indices, uint32_t indexCount );
VkResult vkpt_decals_geometry_build_blas( VkCommandBuffer cmd_buf, const int32_t frameIndex );
void vkpt_decals_geometry_append_tlas_instance( const int32_t frameIndex );
