# Vanilla Quake 2 BSP Format

Understanding the Quake 2 BSP (Binary Space Partitioning) format is essential for map creation and understanding how entities are placed in the game world.

> **Note:** This page documents the **original Quake 2/Q2RTX** BSP format. Q2RTXPerimental uses the same format with optional extensions (BSPX) for enhanced features.

**File Format:** `.bsp` files
**Header Location:** `inc/shared/formats/format_bsp.h` (shared between vanilla and enhanced)
**Loading Code:** `src/common/bsp.cpp` (engine-level, shared)

## Overview

BSP files contain all the compiled geometry, textures, lighting, and entity data for a Quake 2 level. They are created by compiling `.map` files using tools like:
- **QBSP** - Compiles geometry into BSP tree
- **VIS** - Calculates visibility data (PVS/PHS)
- **LIGHT** - Calculates lightmaps

### BSP Structure

A BSP file is organized as:
1. **Header** - File identification and lump table
2. **Lumps** - Data sections (entities, vertices, faces, etc.)
3. **Entity String** - Text-based entity definitions

## File Header

### Header Structure

```cpp
#define IDBSPHEADER     MakeLittleLong('I','B','S','P')  // Standard Q2
#define IDBSPHEADER_EXT MakeLittleLong('Q','B','S','P')  // Extended format
#define BSPVERSION      38                                // Quake 2 version

typedef struct {
    uint32_t    ident;              // Magic number (IBSP or QBSP)
    uint32_t    version;            // BSP version (38 for Q2)
    lump_t      lumps[HEADER_LUMPS]; // 19 lump entries
} dheader_t;
```

### Lump Entry

Each lump describes a data section:

```cpp
typedef struct {
    uint32_t    fileofs;    // Offset from file start (bytes)
    uint32_t    filelen;    // Length of data (bytes)
} lump_t;
```

### The 19 Lumps

| Index | Name | Description |
|-------|------|-------------|
| 0 | LUMP_ENTITIES | Entity definitions (text string) |
| 1 | LUMP_PLANES | Plane equations for BSP nodes |
| 2 | LUMP_VERTEXES | Vertex positions |
| 3 | LUMP_VISIBILITY | PVS/PHS visibility data |
| 4 | LUMP_NODES | BSP tree internal nodes |
| 5 | LUMP_TEXINFO | Texture information |
| 6 | LUMP_FACES | Surface faces |
| 7 | LUMP_LIGHTING | Lightmap data |
| 8 | LUMP_LEAFS | BSP tree leaf nodes |
| 9 | LUMP_LEAFFACES | Face lists for leafs |
| 10 | LUMP_LEAFBRUSHES | Brush lists for leafs |
| 11 | LUMP_EDGES | Edge definitions |
| 12 | LUMP_SURFEDGES | Surface edge lists |
| 13 | LUMP_MODELS | Inline models (brush entities) |
| 14 | LUMP_BRUSHES | Collision brushes |
| 15 | LUMP_BRUSHSIDES | Brush side planes |
| 16 | LUMP_POP | Population data (unused) |
| 17 | LUMP_AREAS | Area portal areas |
| 18 | LUMP_AREAPORTALS | Area portals |

## Entity Lump (Lump 0)

**The most important lump for game logic** - contains all entity spawn data.

### Format

Plain text string with entity definitions:
```
{
"classname" "worldspawn"
"message" "My Level"
"skybox" "unit1_"
}
{
"classname" "info_player_start"
"origin" "128 256 64"
"angle" "90"
}
{
"classname" "monster_soldier"
"origin" "512 512 64"
"angle" "180"
"targetname" "guard1"
}
```

### Entity Format

Each entity is a dictionary of key/value pairs enclosed in braces:

```
{
"key1" "value1"
"key2" "value2"
...
}
```

**Key Rules:**
- Maximum key length: 32 characters (`MAX_KEY`)
- Maximum value length: 1024 characters (`MAX_VALUE`)
- Case-insensitive for most keys
- Some keys are engine-recognized, others are entity-specific

### Worldspawn Entity

**Always the first entity** in the entity lump. Defines global map properties:

```
{
"classname" "worldspawn"
"message" "Level Title"          // Map name shown to players
"skybox" "sky_name"              // Skybox textures
"gravity" "800"                  // World gravity (default 800)
"sounds" "2"                     // CD track number
"light" "100"                    // Global ambient light
"_sun_light" "150"               // Sunlight intensity
"_sun_color" "1.0 0.9 0.8"      // Sunlight color
"_sun_angle" "45 135"            // Sunlight direction
}
```

### Common Entity Keys

Keys recognized by the engine and game code:

**Position and Orientation:**
```
"origin" "x y z"        // Entity position in world units
"angle" "yaw"           // Horizontal rotation (0-360)
"angles" "pitch yaw roll"  // Full 3D rotation
"mangle" "pitch yaw roll"  // Alternative to angles
```

**Identification:**
```
"classname" "entity_type"   // Entity type (REQUIRED for all entities)
"targetname" "name"         // Unique name for targeting
"target" "name"             // Target to activate
"killtarget" "name"         // Target to remove
```

**Visual Properties:**
```
"model" "*1"                // Inline model number (brush entities)
"modelindex" "5"            // Model index
"skin" "0"                  // Skin/texture variant
"frame" "0"                 // Animation frame
"effects" "0"               // Visual effects flags
```

**Spawn Control:**
```
"spawnflags" "2051"         // Spawn flag bitfield
"delay" "2.0"               // Activation delay
"wait" "1.0"                // Cooldown time
"random" "0.5"              // Random time variance
```

**Game Properties:**
```
"health" "100"              // Entity health
"dmg" "10"                  // Damage amount
"speed" "100"               // Movement speed
"count" "5"                 // Counter value
"team" "red"                // Team name
```

### Entity Parsing

When a map loads, the engine parses the entity string:

1. **Read entity lump** from BSP file
2. **Tokenize** the string (handle braces, quotes)
3. **For each entity:**
   - Parse key/value pairs into entity dictionary
   - Spawn entity based on `classname`
   - Apply key values to entity fields

**Example parsing in game code:**
```cpp
char *entityString = bsp->entitystring;
edict_t *ent = nullptr;

// Parse all entities
while (1) {
    char *token = COM_Parse(&entityString);
    if (!token[0])
        break;
        
    if (token[0] != '{')
        gi.error("ED_LoadFromFile: found %s when expecting {", token);
    
    // Create new entity
    if (!ent)
        ent = g_edicts;  // worldspawn
    else
        ent = G_Spawn();
    
    // Parse entity dictionary
    if (!ED_ParseEntity(entityString, ent)) {
        G_FreeEdict(ent);
        continue;
    }
    
    // Spawn the entity
    ED_CallSpawn(ent);
}
```

## BSP Tree Structure

The BSP tree spatially partitions the world for rendering and collision.

### Planes (Lump 1)

Plane equations that divide space:

```cpp
typedef struct {
    vec3_t  normal;     // Plane normal vector
    float   dist;       // Distance from origin
    int32_t type;       // Axis-aligned type (0=X, 1=Y, 2=Z, 3=arbitrary)
} dplane_t;
```

**Plane equation:** `dot(normal, point) = dist`
- Points where `dot(normal, point) > dist` are **in front**
- Points where `dot(normal, point) < dist` are **behind**

### Nodes (Lump 4)

Internal nodes of BSP tree:

```cpp
typedef struct mnode_s {
    cm_plane_t      *plane;         // Splitting plane (never NULL for nodes)
    vec3_t          mins, maxs;     // Bounding box
    struct mnode_s  *parent;        // Parent node
    struct mnode_s  *children[2];   // Child nodes [0]=front, [1]=back
    
    // Rendering data
    int             numfaces;       // Faces in this node
    mface_t         *firstface;     // First face
    unsigned        visframe;       // Visibility frame marker
} mnode_t;
```

**Tree traversal:**
- Start at root node
- Test point against node's plane
- Recurse into front child if point is in front
- Recurse into back child if point is behind
- Continue until reaching a leaf

### Leafs (Lump 8)

Terminal nodes containing actual game space:

```cpp
typedef struct {
    cm_plane_t      *plane;             // Always NULL (identifies as leaf)
    vec3_t          mins, maxs;         // Bounding box
    struct mnode_s  *parent;            // Parent node
    
    int             contents;           // Leaf contents (CONTENTS_* flags)
    int             cluster;            // Visibility cluster (-1 if solid)
    int             area;               // Area portal area number
    
    mbrush_t        **firstleafbrush;   // Collision brushes in this leaf
    int             numleafbrushes;     // Number of brushes
    
    mface_t         **firstleafface;    // Visible faces in this leaf
    int             numleaffaces;       // Number of faces
    
    unsigned        visframe;           // Visibility frame marker
} mleaf_t;
```

**Contents flags** indicate what's in the leaf:
- `CONTENTS_SOLID` - Solid space (walls)
- `CONTENTS_WINDOW` - Transparent solid
- `CONTENTS_WATER` - Water volume
- `CONTENTS_SLIME` - Slime/acid volume
- `CONTENTS_LAVA` - Lava volume
- `CONTENTS_MIST` - Non-blocking fog
- `CONTENTS_EMPTY` - Open air (can occupy)

## Collision System

### Brushes (Lump 14)

Convex collision volumes:

```cpp
typedef struct {
    int             contents;           // Brush contents flags
    int             numsides;           // Number of sides
    mbrushside_t    *firstbrushside;    // First brush side
    unsigned        checkcount;         // Trace cache counter
} mbrush_t;
```

### Brush Sides (Lump 15)

Planes that define brush faces:

```cpp
typedef struct {
    cm_plane_t      *plane;     // Side plane
    mtexinfo_t      *texinfo;   // Surface properties
} mbrushside_t;
```

**Brush example:**
A simple box brush has 6 sides (one per face), each with a plane equation and texture.

### Collision Detection

**Point in brush test:**
```cpp
bool PointInBrush(vec3_t point, mbrush_t *brush) {
    for (int i = 0; i < brush->numsides; i++) {
        mbrushside_t *side = &brush->firstbrushside[i];
        float dist = DotProduct(point, side->plane->normal) - side->plane->dist;
        
        if (dist > 0)
            return false;  // Outside this side
    }
    return true;  // Inside all sides
}
```

## Rendering Data

### Vertices (Lump 2)

3D positions:

```cpp
typedef struct {
    vec3_t  point;  // X, Y, Z coordinates
} mvertex_t;
```

### Edges (Lump 11)

Connections between vertices:

```cpp
typedef struct {
    mvertex_t   *v[2];  // Two vertex pointers
} medge_t;
```

### Faces (Lump 6)

Renderable surfaces:

```cpp
typedef struct mface_s {
    msurfedge_t     *firstsurfedge;     // First surface edge
    int             numsurfedges;       // Number of edges
    
    cm_plane_t      *plane;             // Face plane
    int             drawflags;          // DSURF_PLANEBACK, etc.
    
    byte            *lightmap;          // Lightmap data
    byte            styles[MAX_LIGHTMAPS];  // Lightstyle indices
    int             numstyles;          // Number of lightstyles
    
    mtexinfo_t      *texinfo;           // Texture and projection info
    vec3_t          lm_axis[2];         // Lightmap axes
    vec2_t          lm_offset;          // Lightmap offset
    vec2_t          lm_scale;           // Lightmap scale
    int             lm_width;           // Lightmap width
    int             lm_height;          // Lightmap height
    
    // Frame tracking
    unsigned        drawframe;          // Last draw frame
    unsigned        dlightframe;        // Dynamic light frame
    unsigned        dlightbits;         // Dynamic light flags
} mface_t;
```

### Texture Info (Lump 5)

Texture projection and properties:

```cpp
typedef struct mtexinfo_s {
    cm_surface_t    c;                      // Surface flags
    char            name[CM_MAX_TEXNAME];   // Texture name
    
    // Texture projection (for UV mapping)
    vec3_t          axis[2];                // S and T axes
    vec2_t          offset;                 // S and T offsets
    
    // Animation
    int             numframes;              // Animation frame count
    struct mtexinfo_s *next;                // Next frame in animation
    
    uint32_t        texInfoID;              // Unique texinfo ID
} mtexinfo_t;
```

**UV calculation:**
```cpp
// Calculate texture coordinates for a vertex
vec2_t CalculateUV(vec3_t vertex, mtexinfo_t *texinfo) {
    vec2_t uv;
    uv[0] = DotProduct(vertex, texinfo->axis[0]) + texinfo->offset[0];
    uv[1] = DotProduct(vertex, texinfo->axis[1]) + texinfo->offset[1];
    return uv;
}
```

## Visibility System

### PVS (Potentially Visible Set)

The PVS determines which areas of the map can potentially see each other.

### Visibility Lump (Lump 3)

Compressed visibility data:

```cpp
typedef struct {
    uint32_t    numclusters;        // Number of visibility clusters
    uint32_t    bitofs[][2];        // Offset table [cluster][PVS/PHS]
    // Followed by compressed visibility data
} dvis_t;
```

**Visibility types:**
- `DVIS_PVS` (0) - Potentially Visible Set
- `DVIS_PHS` (1) - Potentially Hearable Set
- `DVIS_PVS2` (16) - Q2RTX: 2nd order PVS

### Clusters

**Cluster** = Group of leafs that share visibility
- Each non-solid leaf belongs to a cluster
- Cluster visibility is pre-calculated by VIS tool
- Each cluster has a compressed bitset of visible clusters

**Constants:**
```cpp
#define MAX_MAP_CLUSTERS    65536       // Maximum clusters
#define VIS_MAX_BYTES       (MAX_MAP_CLUSTERS >> 3)  // PVS size in bytes
```

### Using Visibility Data

**Check if cluster B is visible from cluster A:**
```cpp
bool ClusterVisible(bsp_t *bsp, int clusterA, int clusterB) {
    if (clusterA < 0 || clusterB < 0)
        return true;  // Always visible if no cluster
    
    // Get PVS data for cluster A
    byte *pvs = GetClusterPVS(bsp, clusterA);
    
    // Check if cluster B is in the bitset
    return (pvs[clusterB >> 3] & (1 << (clusterB & 7))) != 0;
}
```

**Decompressing PVS:**
```cpp
byte *DecompressPVS(bsp_t *bsp, byte *in, byte *out) {
    int row = (bsp->numclusters + 7) >> 3;
    byte *out_p = out;
    
    if (!in) {
        // No visibility - set all visible
        memset(out, 0xff, row);
        return out;
    }
    
    do {
        if (*in) {
            *out_p++ = *in++;
            continue;
        }
        
        // Run-length encoding: 0 followed by count
        int c = in[1];
        in += 2;
        while (c) {
            *out_p++ = 0;
            c--;
        }
    } while (out_p - out < row);
    
    return out;
}
```

## Area Portals

Area portals optimize visibility and sound propagation for large maps.

### Areas (Lump 17)

Subdivisions of the map:

```cpp
typedef struct {
    int             numareaportals;     // Number of portals
    mareaportal_t   *firstareaportal;   // First portal
    unsigned        floodvalid;         // Flood-fill marker
} marea_t;
```

### Area Portals (Lump 18)

Connections between areas:

```cpp
typedef struct {
    unsigned    portalnum;      // Portal entity number
    unsigned    otherarea;      // Connected area number
} mareaportal_t;
```

**Usage:**
- **func_areaportal** entities control portal open/close state
- When closed, areas are not visible/hearable through portal
- Optimizes rendering and sound propagation

## Inline Models

### Models (Lump 13)

Brush-based entities (doors, platforms, etc.):

```cpp
typedef struct mmodel_s {
    int         type;               // Model type
    vec3_t      mins, maxs;         // Bounding box
    vec3_t      origin;             // Center point
    mnode_t     *headnode;          // BSP tree root for this model
    
    float       radius;             // Bounding sphere radius
    int         numfaces;           // Number of faces
    mface_t     *firstface;         // First face
} mmodel_t;
```

**Model numbering:**
- Model 0 = World geometry
- Model 1+ = Brush entities (doors, platforms, etc.)

**Entity reference:**
```
{
"classname" "func_door"
"model" "*1"        // References inline model 1
}
```

## Lightmaps

### Lighting Lump (Lump 7)

Precalculated lighting data:

```cpp
// Lightmap samples (RGB bytes)
byte lightmap[lm_width * lm_height * 3];
```

**Q2RTXPerimental enhancement:**
```cpp
#define MAX_LIGHTMAPS   256     // Extended from 4 (vanilla Q2)
```

### Lightstyles

Animated lighting effects:
- `styles[0]` = Static lighting
- `styles[1-3]` = Animated lightstyles

**Lightstyle indices:**
- 0 = Normal (static)
- 1-11 = Predefined animations (flicker, strobe, etc.)
- 12-63 = Custom animations

## Loading and Using BSP Data

### Loading Process

1. **Open BSP file** and read header
2. **Validate** magic number and version
3. **Load each lump** into memory:
   ```cpp
   for (int i = 0; i < HEADER_LUMPS; i++) {
       lump_t *lump = &header->lumps[i];
       fseek(file, lump->fileofs, SEEK_SET);
       fread(lump_data[i], 1, lump->filelen, file);
   }
   ```
4. **Build internal structures** (link pointers, etc.)
5. **Parse entity string** and spawn entities

### Memory Structure

**Runtime BSP structure:**
```cpp
typedef struct bsp_s {
    list_t          entry;          // Linked list
    int             refcount;       // Reference count
    unsigned        checksum;       // File checksum
    memhunk_t       hunk;           // Memory allocator
    
    // Collision data
    int             numbrushsides;
    mbrushside_t    *brushsides;
    int             numbrushes;
    mbrush_t        *brushes;
    
    // Textures
    int             numtexinfo;
    mtexinfo_t      *texinfo;
    
    // BSP tree
    int             numplanes;
    cm_plane_t      *planes;
    int             numnodes;
    mnode_t         *nodes;
    int             numleafs;
    mleaf_t         *leafs;
    
    // Models
    int             nummodels;
    mmodel_t        *models;
    
    // Visibility
    int             numvisibility;
    int             visrowsize;
    dvis_t          *vis;
    
    // Entities
    int             numentitychars;
    char            *entitystring;
    
    // Areas
    int             numareas;
    marea_t         *areas;
    int             numareaportals;
    mareaportal_t   *areaportals;
    
    // Rendering (REF_GL or REF_VKPT)
    #if USE_REF
    int             numvertexes;
    mvertex_t       *vertices;
    int             numedges;
    medge_t         *edges;
    int             numsurfedges;
    msurfedge_t     *surfedges;
    int             numfaces;
    mface_t         *faces;
    // ... lighting, etc.
    #endif
} bsp_t;
```

## BSP Extensions (BSPX)

Q2RTXPerimental supports extended BSP features.

### BSPX Header

```cpp
#define BSPXHEADER  MakeLittleLong('B','S','P','X')

typedef struct {
    char        name[24];       // Extension name
    uint32_t    fileofs;        // Offset in file
    uint32_t    filelen;        // Data length
} xlump_t;
```

**Common extensions:**
- **BRUSHLIST** - Enhanced brush data with bounds
- **LMSTYLE256** - Support for 256 lightstyles (vs 4 in vanilla)
- **LIGHTGRID** - Volumetric lighting data

## Practical Examples

### Example 1: Finding Player Start

```cpp
// Parse entity string to find info_player_start
char *entities = bsp->entitystring;
char *start = strstr(entities, "\"classname\" \"info_player_start\"");

if (start) {
    // Find the origin key
    char *origin = strstr(start, "\"origin\"");
    if (origin) {
        // Parse "x y z"
        vec3_t pos;
        sscanf(origin + 10, "\"%f %f %f\"", &pos[0], &pos[1], &pos[2]);
        // Use position...
    }
}
```

### Example 2: Trace Ray Through BSP

```cpp
mleaf_t *PointInLeaf(bsp_t *bsp, vec3_t point) {
    mnode_t *node = bsp->nodes;  // Start at root
    
    while (node->plane) {  // While not a leaf
        float dist = DotProduct(point, node->plane->normal) - node->plane->dist;
        node = node->children[dist >= 0 ? 0 : 1];
    }
    
    return (mleaf_t *)node;
}
```

### Example 3: Visibility Check

```cpp
bool EntityVisible(edict_t *viewer, edict_t *target) {
    // Find leafs
    mleaf_t *viewerLeaf = PointInLeaf(bsp, viewer->s.origin);
    mleaf_t *targetLeaf = PointInLeaf(bsp, target->s.origin);
    
    // Check cluster visibility
    if (!ClusterVisible(bsp, viewerLeaf->cluster, targetLeaf->cluster))
        return false;
    
    // Do line trace for exact visibility
    trace_t trace = gi.trace(viewer->s.origin, vec3_origin, vec3_origin,
                            target->s.origin, viewer, MASK_OPAQUE);
    
    return trace.fraction == 1.0f;
}
```

## Common Pitfalls

### 1. Entity Dictionary Limits

```cpp
#define MAX_KEY     32      // Key string max length
#define MAX_VALUE   1024    // Value string max length
```

**Problem:** Long values get truncated
**Solution:** Split data across multiple keys or use external files

### 2. Brush Limits

Vanilla Quake 2 has limits:
- Max brushes per map
- Max brush sides
- Max faces

**Solution:** Optimize geometry, use hint brushes, simplify detail

### 3. Visibility Errors

**Problem:** PVS too large causes performance issues
**Solution:** 
- Add hint brushes to control VIS
- Use func_detail for non-structural geometry
- Seal map properly

### 4. Leak Detection

**Problem:** Map leaks (not sealed) prevent VIS from running
**Solution:**
- Load pointfile (`.pts`) in editor
- Follow red line to find leak
- Seal gap in geometry

## Related Documentation

- [**Vanilla Game Module Architecture**](Vanilla-Game-Module-Architecture.md) - Entity spawning
- [**Entity System Overview**](Entity-System-Overview.md) - Q2RTXPerimental entities
- [**Entity Lifecycle**](Entity-Lifecycle.md) - Spawn process
- [**Vanilla Networking**](Vanilla-Networking.md) - Entity state updates

## Summary

The BSP format contains:

- **Entity lump** - Text-based entity definitions
- **BSP tree** - Spatial partitioning for rendering/collision
- **Collision data** - Brushes and brush sides
- **Rendering data** - Vertices, edges, faces, lightmaps
- **Visibility data** - PVS/PHS for optimization
- **Area portals** - Map subdivision for visibility control

Understanding BSP structure is crucial for:
- Creating custom maps
- Spawning entities at map load
- Implementing collision detection
- Optimizing rendering performance
- Understanding entity placement in world space
