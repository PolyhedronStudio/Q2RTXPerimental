/********************************************************************
*
*
*    ClientGame: Decal Mesh Builder.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/decals/clg_decal_mesh.h"

//! Small normal-space push used to keep clipped decal triangles from z-fighting the receiver.
static constexpr float CLG_DECAL_MESH_NORMAL_BIAS = 0.10f;

void CLG_DecalMesh_Clear( clg_decal_mesh_t *mesh ) {
    if ( !mesh ) {
        return;
    }

    memset( mesh, 0, sizeof( *mesh ) );
}

const bool CLG_DecalMesh_AppendPolygon( clg_decal_mesh_t *mesh, const clg_decal_clip_polygon_t &polygon, const vec3_t normal ) {
    if ( !mesh ) {
        return false;
    }

    if ( polygon.vertexCount < 3 ) {
        return false;
    }

    // Triangulate as a fan around the first vertex in polygon order.
    for ( int32_t i = 1; i < polygon.vertexCount - 1; i++ ) {
        vec3_t edge1 = { 0 };
        vec3_t edge2 = { 0 };
        vec3_t triNormal = { 0 };
        VectorSubtract( polygon.positions[ i ], polygon.positions[ 0 ], edge1 );
        VectorSubtract( polygon.positions[ i + 1 ], polygon.positions[ 0 ], edge2 );
        CrossProduct( edge1, edge2, triNormal );

        /**
        *    Skip fan steps that collapse into a nearly zero-area triangle. These usually
        *    come from clipping-generated collinear points and otherwise render as stretched
        *    sliver artifacts along edges.
        **/
        if ( VectorLengthSquared( triNormal ) <= ( 0.001f * 0.001f ) ) {
            continue;
        }

        if ( mesh->vertexCount + 3 > (int32_t)std::size( mesh->vertices ) ) {
            return false;
        }

        clg_decal_mesh_vertex_t *v0 = &mesh->vertices[ mesh->vertexCount++ ];
        VectorCopy( polygon.positions[ 0 ], v0->position );
        VectorMA( v0->position, CLG_DECAL_MESH_NORMAL_BIAS, normal, v0->position );
        VectorCopy( normal, v0->normal );
        v0->uv[ 0 ] = polygon.uv[ 0 ][ 0 ];
        v0->uv[ 1 ] = polygon.uv[ 0 ][ 1 ];

        clg_decal_mesh_vertex_t *v1 = &mesh->vertices[ mesh->vertexCount++ ];
        VectorCopy( polygon.positions[ i ], v1->position );
        VectorMA( v1->position, CLG_DECAL_MESH_NORMAL_BIAS, normal, v1->position );
        VectorCopy( normal, v1->normal );
        v1->uv[ 0 ] = polygon.uv[ i ][ 0 ];
        v1->uv[ 1 ] = polygon.uv[ i ][ 1 ];

        clg_decal_mesh_vertex_t *v2 = &mesh->vertices[ mesh->vertexCount++ ];
        VectorCopy( polygon.positions[ i + 1 ], v2->position );
        VectorMA( v2->position, CLG_DECAL_MESH_NORMAL_BIAS, normal, v2->position );
        VectorCopy( normal, v2->normal );
        v2->uv[ 0 ] = polygon.uv[ i + 1 ][ 0 ];
        v2->uv[ 1 ] = polygon.uv[ i + 1 ][ 1 ];

        // Keep winding consistent with surface normal so adjacent clipped surfaces do not flip triangles.
        if ( DotProduct( triNormal, normal ) < 0.0f ) {
            clg_decal_mesh_vertex_t tmp = *v1;
            *v1 = *v2;
            *v2 = tmp;
        }

        mesh->triangleCount++;
    }

    return true;
}
