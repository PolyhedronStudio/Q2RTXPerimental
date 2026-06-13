#include "svgame/nav/nav_core.h"
#include "svgame/nav/nav_containers.h"
#include "svgame/nav/nav_types.h"
#include "svgame/nav/nav_generate.h"
#include "svgame/nav/nav_debug.h"

#include "svgame/svg_utils.h"

cvar_t *s_nav6_debug_nodes = nullptr;
cvar_t *s_nav6_debug_polys = nullptr;

void Nav_DebugInit() {
    s_nav6_debug_nodes = gi.cvar("nav6_debug_nodes", "0", 0);
    s_nav6_debug_polys = gi.cvar("nav6_debug_polys", "0", 0);
}

static void DebugDrawBox(const Vector3 &mins, const Vector3 &maxs) {
    Vector3 corners[8] = {
        {mins.x, mins.y, mins.z},
        {maxs.x, mins.y, mins.z},
        {maxs.x, maxs.y, mins.z},
        {mins.x, maxs.y, mins.z},
        {mins.x, mins.y, maxs.z},
        {maxs.x, mins.y, maxs.z},
        {maxs.x, maxs.y, maxs.z},
        {mins.x, maxs.y, maxs.z}
    };
    
    // Bottom
    SVG_DebugDrawLine_TE(corners[0], corners[1], MULTICAST_ALL, false);
    SVG_DebugDrawLine_TE(corners[1], corners[2], MULTICAST_ALL, false);
    SVG_DebugDrawLine_TE(corners[2], corners[3], MULTICAST_ALL, false);
    SVG_DebugDrawLine_TE(corners[3], corners[0], MULTICAST_ALL, false);
    
    // Top
    SVG_DebugDrawLine_TE(corners[4], corners[5], MULTICAST_ALL, false);
    SVG_DebugDrawLine_TE(corners[5], corners[6], MULTICAST_ALL, false);
    SVG_DebugDrawLine_TE(corners[6], corners[7], MULTICAST_ALL, false);
    SVG_DebugDrawLine_TE(corners[7], corners[4], MULTICAST_ALL, false);
    
    // Sides
    SVG_DebugDrawLine_TE(corners[0], corners[4], MULTICAST_ALL, false);
    SVG_DebugDrawLine_TE(corners[1], corners[5], MULTICAST_ALL, false);
    SVG_DebugDrawLine_TE(corners[2], corners[6], MULTICAST_ALL, false);
    SVG_DebugDrawLine_TE(corners[3], corners[7], MULTICAST_ALL, false);
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

    if (s_nav6_debug_nodes && s_nav6_debug_nodes->value != 0) {
        DebugDrawBox(node.mins, node.maxs);
    }
    
    if (s_nav6_debug_polys && s_nav6_debug_polys->value != 0 && node.poly_id != -1) {
        if (node.poly_id >= 0 && node.poly_id < g_nav_polys.size()) {
            const nav_poly_t &poly = g_nav_polys[node.poly_id];
            for (int32_t v = 0; v < poly.num_vertices; v++) {
                Vector3 start = poly.vertices[v];
                Vector3 end = poly.vertices[(v + 1) % poly.num_vertices];
                SVG_DebugDrawLine_TE(start, end, MULTICAST_ALL, false);
            }
        }
    }
    
    if (node.left_child != -1) RecursiveDrawNodes(node.left_child, playerPos, radius);
    if (node.right_child != -1) RecursiveDrawNodes(node.right_child, playerPos, radius);
}

void Nav_DebugDraw() {
    if (!s_nav6_debug_nodes || !s_nav6_debug_polys) return;
    if (s_nav6_debug_nodes->value == 0 && s_nav6_debug_polys->value == 0) return;
    if (g_nav_nodes.size() == 0) return;

    // Throttle drawing so we don't overflow the TE buffer
    static QMTime nextDrawTime = 0_sec;
    if (level.time < nextDrawTime) return;
    nextDrawTime = level.time + QMTime::FromSeconds(0.1f);

    svg_base_edict_t *player = g_edict_pool.EdictForNumber(1);
    if (!player || !player->inUse) return;

    RecursiveDrawNodes(0, player->currentOrigin, 512.0f);
}
