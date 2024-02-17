/**
*
*
*   The BSP format is now part of shared data. Certain game module specific
*   pieces of code require knowledge of its data structure.
*
*
**/
#pragma once 


/**
*   When not compiling with USE_REF defined by CMake, we know we're not dealing
*   with the client. Since defining USE_REF with CMake will cause other parts of
*   the code we don't want to compile as well. In order to prevent that, we use
*   this in-file silly hack. It is undefined afterwards at the end of the file.
**/
#ifndef USE_REF
#define USE_REF_HEADER_HACK
#define USE_REF 1
#endif

/**
*
*
*   Content which was in: /inc/format/bsp.h
*
*
**/
#define IDBSPHEADER     MakeLittleLong('I','B','S','P')
#define IDBSPHEADER_EXT MakeLittleLong('Q','B','S','P')
#define BSPVERSION      38


// Moved to shared.h since we need it there for MAX_MAP_AREA_BYTES
// Other than that, this define is only used 1 time in bsp.cpp
// can't be increased without changing network protocol
//#define     MAX_MAP_AREAS       256

// arbitrary limit
#define     MAX_MAP_CLUSTERS    65536

// QBSP stuff
#define QBSPHEADER    (('P'<<24)+('S'<<16)+('B'<<8)+'Q')

// key / value pair sizes

#define     MAX_KEY         32
#define     MAX_VALUE       1024

#define     MAX_TEXNAME     32

//=============================================================================

typedef struct {
    uint32_t        fileofs, filelen;
} lump_t;

#define    HEADER_LUMPS         19

typedef struct {
    uint32_t    ident;
    uint32_t    version;
    lump_t      lumps[ HEADER_LUMPS ];
} dheader_t;

#define    MAX_LIGHTMAPS    4

#define ANGLE_UP    -1
#define ANGLE_DOWN  -2

// the visibility lump consists of a header with a count, then
// byte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define DVIS_PVS    0
#define DVIS_PHS    1
#define DVIS_PVS2   16 // Q2RTX : 2nd order PVS

typedef struct {
    uint32_t    numclusters;
    uint32_t    bitofs[][ 2 ];    // bitofs[numclusters][2]
} dvis_t;

//=============================================================================

#define BSPXHEADER      MakeLittleLong('B','S','P','X')

typedef struct {
    char        name[ 24 ];
    uint32_t    fileofs;
    uint32_t    filelen;
} xlump_t;



/**
*
*
*   Content which was in: /inc/common/bsp.h
*
*
**/
// maximum size of a PVS row, in bytes
#define VIS_MAX_BYTES   (MAX_MAP_CLUSTERS >> 3)

// take advantage of 64-bit systems
#define VIS_FAST_LONGS(bsp) \
    (((bsp)->visrowsize + sizeof(size_t) - 1) / sizeof(size_t))

typedef struct mtexinfo_s {  // used internally due to name len probs //ZOID
    csurface_t          c;
    char                name[ MAX_TEXNAME ];

    #if USE_REF
    int                 radiance;
    vec3_t              axis[ 2 ];
    vec2_t              offset;
    #if REF_VKPT
    struct pbr_material_s *material;
    #endif
    #if REF_GL
    struct image_s *image; // used for texturing
    #endif
    int                 numframes;
    struct mtexinfo_s *next; // used for animation
    #endif
} mtexinfo_t;

#if USE_REF
typedef struct {
    vec3_t      point;
} mvertex_t;

typedef struct {
    // indices into the bsp->basisvectors array
    uint32_t normal;
    uint32_t tangent;
    uint32_t bitangent;
} mbasis_t;

typedef struct {
    mvertex_t *v[ 2 ];
} medge_t;

typedef struct {
    medge_t *edge;
    int         vert;
} msurfedge_t;

#define SURF_TRANS_MASK (SURF_TRANS33 | SURF_TRANS66)
#define SURF_COLOR_MASK (SURF_TRANS_MASK | SURF_WARP)
#define SURF_NOLM_MASK  (SURF_COLOR_MASK | SURF_FLOWING | SURF_SKY | SURF_NODRAW)

#define DSURF_PLANEBACK     1

typedef struct mface_s {
    msurfedge_t *firstsurfedge;
    int             numsurfedges;

    cplane_t *plane;
    int             drawflags; // DSURF_PLANEBACK, etc

    byte *lightmap;
    byte            styles[ MAX_LIGHTMAPS ];
    int             numstyles;

    mtexinfo_t *texinfo;
    vec3_t          lm_axis[ 2 ];
    vec2_t          lm_offset;
    vec2_t          lm_scale;
    int             lm_width;
    int             lm_height;

    #if USE_REF == REF_GL
    int             texnum[ 2 ];
    int             statebits;
    int             firstvert;
    int             light_s, light_t;
    float           stylecache[ MAX_LIGHTMAPS ];
    #else
    #ifndef MIPLEVELS
    #define MIPLEVELS 4
    #define IN_HEADER_MIPLEVELS
    #endif
    struct surfcache_s *cachespots[ MIPLEVELS ]; // surface generation data
    #ifndef IN_HEADER_MIPLEVELS
    #undef MIPLEVELS 4
    #undef IN_HEADER_MIPLEVELS
    #endif
    #endif

    int             firstbasis;

    unsigned        drawframe;

    unsigned        dlightframe;
    unsigned        dlightbits;

    struct entity_s *entity;
    struct mface_s *next;
} mface_t;
#endif

typedef struct mnode_s {
    /* ======> */
    cplane_t *plane;     // never NULL to differentiate from leafs
    #if USE_REF
    vec3_t              mins;
    vec3_t              maxs;

    unsigned            visframe;
    #endif
    struct mnode_s *parent;
    /* <====== */

    struct mnode_s *children[ 2 ];

    #if USE_REF
    int                 numfaces;
    mface_t *firstface;
    #endif
} mnode_t;

typedef struct {
    cplane_t *plane;
    mtexinfo_t *texinfo;
} mbrushside_t;

typedef struct {
    int                 contents;
    int                 numsides;
    mbrushside_t *firstbrushside;
    unsigned            checkcount;         // to avoid repeated testings
} mbrush_t;

typedef struct {
    /* ======> */
    cplane_t *plane;     // always NULL to differentiate from nodes
    #if USE_REF
    vec3_t              mins;
    vec3_t              maxs;

    unsigned            visframe;
    #endif
    struct mnode_s *parent;
    /* <====== */

    int             contents;
    int             cluster;
    int             area;
    mbrush_t **firstleafbrush;
    int             numleafbrushes;
    #if USE_REF
    mface_t **firstleafface;
    int             numleaffaces;
    #endif
} mleaf_t;

typedef struct {
    unsigned    portalnum;
    unsigned    otherarea;
} mareaportal_t;

typedef struct {
    int             numareaportals;
    mareaportal_t *firstareaportal;
    unsigned        floodvalid;
} marea_t;

typedef struct mmodel_s {
    #if USE_REF
    /* ======> */
    int             type;
    /* <====== */
    #endif
    vec3_t          mins, maxs;
    vec3_t          origin;        // for sounds or lights
    mnode_t *headnode;

    #if USE_REF
    float           radius;

    int             numfaces;
    mface_t *firstface;

    #if USE_REF == REF_GL
    unsigned        drawframe;
    #endif
    #endif
} mmodel_t;

typedef struct bsp_s {
    list_t      entry;
    int         refcount;

    unsigned    checksum;

    memhunk_t   hunk;

    int             numbrushsides;
    mbrushside_t *brushsides;

    int             numtexinfo;
    mtexinfo_t *texinfo;

    int             numplanes;
    cplane_t *planes;

    int             numnodes;
    mnode_t *nodes;

    int             numleafs;
    mleaf_t *leafs;

    int             numleafbrushes;
    mbrush_t **leafbrushes;

    int             nummodels;
    mmodel_t *models;

    int             numbrushes;
    mbrush_t *brushes;

    int             numvisibility;
    int             visrowsize;
    dvis_t *vis;

    int             numentitychars;
    char *entitystring;

    int             numareas;
    marea_t *areas;

    int             numportals;     // largest portal number used plus one
    int             numareaportals; // size of the array below
    mareaportal_t *areaportals;

    #if USE_REF
    int             numfaces;
    mface_t *faces;

    int             numleaffaces;
    mface_t **leaffaces;

    int             numlightmapbytes;
    byte *lightmap;

    int             numvertices;
    mvertex_t *vertices;

    int             numedges;
    medge_t *edges;

    int             numsurfedges;
    msurfedge_t *surfedges;

    int             numbasisvectors;
    vec3_t *basisvectors;

    int             numbases;
    mbasis_t *bases;

    bool            lm_decoupled;
    #endif

    byte *pvs_matrix;
    byte *pvs2_matrix;
    bool            pvs_patched;

    bool            extended;

    // WARNING: the 'name' string is actually longer than this, and the bsp_t structure is allocated larger than sizeof(bsp_t) in BSP_Load
    char            name[ 1 ];
} bsp_t;

#if USE_REF
typedef struct {
    mface_t *surf;
    cplane_t    plane;
    float       s, t;
    float       fraction;
} lightpoint_t;
#endif

/**
*   The END of USE_REF Header hack.
**/
#ifdef USE_REF_HEADER_HACK
#undef USE_REF
#endif