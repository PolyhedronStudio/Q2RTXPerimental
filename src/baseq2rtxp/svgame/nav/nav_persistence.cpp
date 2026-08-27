#include "nav_persistence.h"
#include "nav_generate.h"
#include "nav_cover_types.h"
#include "nav_cover_query.h"

//! External reference to the global list of generated cover points.
extern std::vector<nav_cover_point_t> g_nav_cover_points;

/**
* @brief Save the current navmesh to a .nav7 file.
* @param filepath Destination file path.
* @return True when the navmesh was written successfully.
**/
bool Nav_Save( const char *filepath ) {
    if (g_nav_faces.empty()) {
        gi.dprintf("NavMesh Save Error: No navmesh generated.\n");
        return false;
    }

    FILE* f = fopen(filepath, "wb");
    if (!f) {
        gi.dprintf("NavMesh Save Error: Could not open %s for writing.\n", filepath);
        return false;
    }

    nav_header_t header = {};
    header.magic = NAV7_MAGIC;
    header.version = NAV7_VERSION;
    // TODO: get actual map checksum from engine
    header.map_checksum = 0; 
    
    header.num_vertices = (int32_t)g_nav_vertices.size();
    header.num_halfedges = (int32_t)g_nav_halfedges.size();
    header.num_faces = (int32_t)g_nav_faces.size();
    header.num_kdtree_nodes = (int32_t)g_nav_nodes.size();
    header.num_leaf_links = (int32_t)g_nav_leaf_links.size();
    header.num_leaf_face_ids = (int32_t)g_nav_leaf_poly_ids.size();
    header.num_cover_points = (int32_t)g_nav_cover_points.size();

    // Write header
    fwrite(&header, sizeof(nav_header_t), 1, f);
    
    // Write data blocks
    if (header.num_vertices > 0)
        fwrite(g_nav_vertices.data(), sizeof(Vector3DP), header.num_vertices, f);
    if (header.num_halfedges > 0)
        fwrite(g_nav_halfedges.data(), sizeof(nav_halfedge_t), header.num_halfedges, f);
    if (header.num_faces > 0)
        fwrite(g_nav_faces.data(), sizeof(nav_face_t), header.num_faces, f);
    if (header.num_kdtree_nodes > 0)
        fwrite(g_nav_nodes.get_data(), sizeof(nav_kdtree_node_t), header.num_kdtree_nodes, f);
    if (header.num_leaf_links > 0)
        fwrite(g_nav_leaf_links.get_data(), sizeof(nav_leaf_link_t), header.num_leaf_links, f);
    if (header.num_leaf_face_ids > 0)
        fwrite(g_nav_leaf_poly_ids.get_data(), sizeof(int32_t), header.num_leaf_face_ids, f);
    if (header.num_cover_points > 0)
        fwrite(g_nav_cover_points.data(), sizeof(nav_cover_point_t), header.num_cover_points, f);

    fclose(f);
    gi.dprintf("NavMesh Saved to %s successfully (Faces: %d, Cover: %d).\n", filepath, header.num_faces, header.num_cover_points);
    return true;
}

/**
* @brief Load a navmesh from a .nav7 file.
* @param filepath Source file path.
* @return True when the navmesh was loaded successfully.
**/
bool Nav_Load( const char *filepath ) {
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

    if (header.magic != NAV7_MAGIC || header.version != NAV7_VERSION) {
        gi.dprintf("NavMesh Load Error: Invalid format or version mismatch (Expected v%u, got v%u).\n",
            NAV7_VERSION, header.version);
        fclose(f);
        return false;
    }

    // Clear existing navmesh before loading
    Nav_Clear();

    // Allocate and read blocks
    g_nav_vertices.resize(header.num_vertices);
    if (header.num_vertices > 0)
        fread(g_nav_vertices.data(), sizeof(Vector3DP), header.num_vertices, f);
        
    g_nav_halfedges.resize(header.num_halfedges);
    if (header.num_halfedges > 0)
        fread(g_nav_halfedges.data(), sizeof(nav_halfedge_t), header.num_halfedges, f);
        
    g_nav_faces.resize(header.num_faces);
    if (header.num_faces > 0)
        fread(g_nav_faces.data(), sizeof(nav_face_t), header.num_faces, f);
    
    for (int32_t i = 0; i < header.num_kdtree_nodes; i++) {
        nav_kdtree_node_t n;
        fread(&n, sizeof(nav_kdtree_node_t), 1, f);
        g_nav_nodes.push_back(n);
    }
    
    for (int32_t i = 0; i < header.num_leaf_links; i++) {
        nav_leaf_link_t l;
        fread(&l, sizeof(nav_leaf_link_t), 1, f);
        g_nav_leaf_links.push_back(l);
    }
    
    for (int32_t i = 0; i < header.num_leaf_face_ids; i++) {
        int32_t id;
        fread(&id, sizeof(int32_t), 1, f);
        g_nav_leaf_poly_ids.push_back(id);
    }

    g_nav_cover_points.resize(header.num_cover_points);
    if (header.num_cover_points > 0) {
        fread(g_nav_cover_points.data(), sizeof(nav_cover_point_t), header.num_cover_points, f);
        // Reset transient reservation state on loaded cover points
        for (auto &cp : g_nav_cover_points) {
            cp.claimed_by_ent = ENTITYNUM_NONE;
            cp.claim_expiration = 0_ms;
        }
    }

    Nav_RebuildCoverSpatialIndex();

    fclose(f);
    gi.dprintf("NavMesh Loaded from %s successfully (Faces: %d, Nodes: %d, Cover Points: %d).\n", 
               filepath, header.num_faces, header.num_kdtree_nodes, header.num_cover_points);
    return true;
}
