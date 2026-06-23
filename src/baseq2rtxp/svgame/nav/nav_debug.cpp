#include "svgame/nav/nav_core.h"
#include "svgame/nav/nav_containers.h"
#include "svgame/nav/nav_types.h"
#include "svgame/nav/nav_generate.h"
#include "svgame/nav/nav_debug.h"

#include "svgame/nav/nav_debug_draw.h"
#include "svgame/svg_utils.h"

cvar_t *s_nav_debug_nodes = nullptr;
cvar_t *s_nav_debug_polys = nullptr;

void Nav_DebugInit() {
    s_nav_debug_nodes = gi.cvar("nav_debug_nodes", "0", 0);
    s_nav_debug_polys = gi.cvar("nav_debug_polys", "0", 0);
}

static void RecursiveDrawNodes(int32_t nodeIndex, const Vector3 &playerPos, float radius) {
    if (nodeIndex == -1 || nodeIndex >= g_nav_nodes.size()) return;
    
    const nav_kdtree_node_t &node = g_nav_nodes[nodeIndex];
    
    // Simple AABB vs AABB check
    if (node.mins.x > playerPos.x + radius || node.maxs.x < playerPos.x - radius ||
        node.mins.y > playerPos.y + radius || node.maxs.y < playerPos.y - radius ||
        node.mins.z > playerPos.z + radius || node.maxs.z < playerPos.z - radius) {
        return;
    }

    if (s_nav_debug_nodes && s_nav_debug_nodes->value != 0) {
        SVG_Nav_DebugDraw_AddAabb(node.mins, node.maxs, U32_CYAN);
    }
    
    // Draw polys by directly iterating over g_nav_polys later in Nav_DebugDraw.
    
    if (node.left_child != -1) RecursiveDrawNodes(node.left_child, playerPos, radius);
    if (node.right_child != -1) RecursiveDrawNodes(node.right_child, playerPos, radius);
}

void Nav_DebugDraw() {
    if (!s_nav_debug_nodes || !s_nav_debug_polys) return;
    if (s_nav_debug_nodes->value == 0 && s_nav_debug_polys->value == 0) return;
    if (g_nav_nodes.size() == 0) return;

    svg_base_edict_t *player = g_edict_pool.EdictForNumber(1);
    if (!player || !player->inUse) return;

    RecursiveDrawNodes(0, player->currentOrigin, CM_MAX_WORLD_SIZE );

    if (s_nav_debug_polys && s_nav_debug_polys->value != 0) {
        for (int32_t i = 0; i < g_nav_faces.size(); i++) {
            const nav_face_t &face = g_nav_faces[i];
            
            // Simple distance check
            if (QM_Vector3DistanceSqr(face.center, player->currentOrigin) > ((float)CM_MAX_WORLD_SIZE * (float)CM_MAX_WORLD_SIZE)) {
                continue;
            }

            for (int32_t e = 0; e < face.num_edges; e++) {
                const nav_halfedge_t& he = g_nav_halfedges[face.first_edge_idx + e];
                
                Vector3 start = g_nav_vertices[he.vertex_idx];
                Vector3 end = g_nav_vertices[g_nav_halfedges[he.next_idx].vertex_idx];

                if (he.twin_idx != -1) {
                    // Draw internal twinned edges in BLUE so we can see the mesh grid cells!
                    SVG_Nav_DebugDraw_AddLine(start, end, U32_BLUE, SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST);
                } else {
                    // Draw boundary edges in YELLOW
                    SVG_Nav_DebugDraw_AddLine(start, end, U32_YELLOW, SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST);
                }
            }
        }
    }
}
