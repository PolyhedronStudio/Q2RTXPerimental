#include "nav_persistence.h"
#include "nav_generate.h"

bool Nav_Save(const char* filepath) {
    if (g_nav_polys.empty()) {
        gi.dprintf("NavMesh Save Error: No navmesh generated.\n");
        return false;
    }

    FILE* f = fopen(filepath, "wb");
    if (!f) {
        gi.dprintf("NavMesh Save Error: Could not open %s for writing.\n", filepath);
        return false;
    }

    nav_header_t header = {};
    header.magic = NAV6_MAGIC;
    header.version = NAV6_VERSION;
    // TODO: get actual map checksum from engine
    header.map_checksum = 0; 
    
    header.num_polys = (int32_t)g_nav_polys.size();
    header.num_kdtree_nodes = (int32_t)g_nav_nodes.size();
    header.num_leaf_links = (int32_t)g_nav_leaf_links.size();
    header.num_leaf_poly_ids = (int32_t)g_nav_leaf_poly_ids.size();

    // Write header
    fwrite(&header, sizeof(nav_header_t), 1, f);
    
    // Write data blocks
    if (header.num_polys > 0)
        fwrite(g_nav_polys.get_data(), sizeof(nav_poly_t), header.num_polys, f);
    if (header.num_kdtree_nodes > 0)
        fwrite(g_nav_nodes.get_data(), sizeof(nav_kdtree_node_t), header.num_kdtree_nodes, f);
    if (header.num_leaf_links > 0)
        fwrite(g_nav_leaf_links.get_data(), sizeof(nav_leaf_link_t), header.num_leaf_links, f);
    if (header.num_leaf_poly_ids > 0)
        fwrite(g_nav_leaf_poly_ids.get_data(), sizeof(int32_t), header.num_leaf_poly_ids, f);

    fclose(f);
    gi.dprintf("NavMesh Saved to %s successfully.\n", filepath);
    return true;
}

bool Nav_Load(const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        gi.dprintf("NavMesh Load Error: Could not open %s.\n", filepath);
        return false;
    }

    nav_header_t header;
    if (fread(&header, sizeof(nav_header_t), 1, f) != 1) {
        gi.dprintf("NavMesh Load Error: Failed to read header.\n");
        fclose(f);
        return false;
    }

    if (header.magic != NAV6_MAGIC || header.version != NAV6_VERSION) {
        gi.dprintf("NavMesh Load Error: Invalid format or version mismatch.\n");
        fclose(f);
        return false;
    }

    // Clear existing navmesh before loading
    Nav_Clear();

    // Allocate and read blocks
    for (int i = 0; i < header.num_polys; i++) {
        nav_poly_t p;
        fread(&p, sizeof(nav_poly_t), 1, f);
        g_nav_polys.push_back(p);
    }
    
    for (int i = 0; i < header.num_kdtree_nodes; i++) {
        nav_kdtree_node_t n;
        fread(&n, sizeof(nav_kdtree_node_t), 1, f);
        g_nav_nodes.push_back(n);
    }
    
    for (int i = 0; i < header.num_leaf_links; i++) {
        nav_leaf_link_t l;
        fread(&l, sizeof(nav_leaf_link_t), 1, f);
        g_nav_leaf_links.push_back(l);
    }
    
    for (int i = 0; i < header.num_leaf_poly_ids; i++) {
        int32_t id;
        fread(&id, sizeof(int32_t), 1, f);
        g_nav_leaf_poly_ids.push_back(id);
    }

    fclose(f);
    gi.dprintf("NavMesh Loaded from %s successfully (Polys: %d, Nodes: %d).\n", 
               filepath, header.num_polys, header.num_kdtree_nodes);
    return true;
}
