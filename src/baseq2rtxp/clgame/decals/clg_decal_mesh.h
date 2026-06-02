/********************************************************************
*
*
*    ClientGame: Decal Mesh Builder.
*
*
********************************************************************/
#pragma once

#include <cstdint>

#include "clgame/decals/clg_decal_clip.h"

/**
*    @brief  One generated decal mesh vertex.
**/
typedef struct clg_decal_mesh_vertex_s {
    vec3_t position;
    vec3_t normal;
    vec2_t uv;
} clg_decal_mesh_vertex_t;

/**
*    @brief  Accumulated mesh triangles for one decal.
**/
typedef struct clg_decal_mesh_s {
    clg_decal_mesh_vertex_t vertices[ 256 ];
    int32_t vertexCount;
    int32_t triangleCount;
} clg_decal_mesh_t;

/**
*    @brief  Appends a clipped polygon into triangle list form.
*    @param  mesh Destination mesh.
*    @param  polygon Polygon from clip stage.
*    @return True if append succeeded.
**/
const bool CLG_DecalMesh_AppendPolygon( clg_decal_mesh_t *mesh, const clg_decal_clip_polygon_t &polygon, const vec3_t normal );

/**
*    @brief  Clears mesh contents.
*    @param  mesh Mesh to clear.
**/
void CLG_DecalMesh_Clear( clg_decal_mesh_t *mesh );
