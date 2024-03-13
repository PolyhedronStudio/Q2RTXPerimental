/********************************************************************
*
*
*   Shared Collision Detection:
*
*
********************************************************************/
#pragma once


// edict->solid values
/**
*   @brief  The actual 'solid' type of an entity, determines how to behave when colliding with other objects.
**/
typedef enum {
    SOLID_NOT,          //! No interaction with other objects.
    SOLID_TRIGGER,      //! Only touch when inside, after moving. (Optional BSP Brush clip when SVF_HULL is set.)
    SOLID_BBOX,         //! Touch on edge.
    SOLID_BSP           //! BSP clip, touch on edge.
} solid_t;

/**
*   @brief  The water 'level' of said entity.
**/
// edict->waterlevel values
typedef enum {  //: uint8_t {
    WATER_NONE,
    WATER_FEET,
    WATER_WAIST,
    WATER_UNDER
} water_level_t;

/**
*   @brief  Brush Contents: lower bits are stronger, and will eat weaker brushes completely
**/
typedef enum {
    CONTENTS_NONE = 0,
    CONTENTS_SOLID = BIT( 0 ),  // An eye is never valid in a solid.
    CONTENTS_WINDOW = BIT( 1 ), // Translucent, but not watery.
    CONTENTS_AUX = BIT( 2 ),
    CONTENTS_LAVA = BIT( 3 ),
    CONTENTS_SLIME = BIT( 4 ),
    CONTENTS_WATER = BIT( 5 ),
    CONTENTS_MIST = BIT( 6 ),

    // Remaining contents are non-visible, and don't eat brushes.
    CONTENTS_NO_WATERJUMP = BIT( 13 ),      // [Paril-KEX] This brush cannot be waterjumped out of.
    CONTENTS_PROJECTILECLIP = BIT( 14 ),    // [Paril-KEX] Projectiles will collide with this.

    CONTENTS_AREAPORTAL = BIT( 15 ),

    CONTENTS_PLAYERCLIP = BIT( 16 ),
    CONTENTS_MONSTERCLIP = BIT( 17 ),

    // Currents can be added to any other contents, and may be mixed.
    CONTENTS_CURRENT_0 = BIT( 18 ),
    CONTENTS_CURRENT_90 = BIT( 19 ),
    CONTENTS_CURRENT_180 = BIT( 20 ),
    CONTENTS_CURRENT_270 = BIT( 21 ),
    CONTENTS_CURRENT_UP = BIT( 22 ),
    CONTENTS_CURRENT_DOWN = BIT( 23 ),

    CONTENTS_ORIGIN = BIT( 24 ), // Removed before bsping an entity.

    CONTENTS_MONSTER = BIT( 25 ), // Should never be on a brush, only in game.
    CONTENTS_DEADMONSTER = BIT( 26 ),

    CONTENTS_DETAIL = BIT( 27 ),        // Brushes to be added after vis leafs.
    CONTENTS_TRANSLUCENT = BIT( 28 ),   // Auto set if any surface has trans.
    CONTENTS_LADDER = BIT( 29 ),

    CONTENTS_PLAYER = BIT( 30 ), // [Paril-KEX] should never be on a brush, only in game; player
    CONTENTS_PROJECTILE = BIT( 31 )  // [Paril-KEX] should never be on a brush, only in game; projectiles.
    // used to solve deadmonster collision issues.
} contents_t;

/**
*   Surface Types:
**/
#define SURF_LIGHT      0x1     // value will hold the light strength
#define SURF_SLICK      0x2     // effects game physics
#define SURF_SKY        0x4     // don't draw, but add to skybox
#define SURF_WARP       0x8     // turbulent water warp
#define SURF_TRANS33    0x10
#define SURF_TRANS66    0x20
#define SURF_FLOWING    0x40    // scroll towards angle
#define SURF_NODRAW     0x80    // don't bother referencing the texture
#define SURF_ALPHATEST  0x02000000  // used by kmquake2
#define SURF_N64_UV             (1U << 28)
#define SURF_N64_SCROLL_X       (1U << 29)
#define SURF_N64_SCROLL_Y       (1U << 30)
#define SURF_N64_SCROLL_FLIP    (1U << 31)

/**
*   ContentMask Sets:
**/
#define MASK_SOLID              contents_t( CONTENTS_SOLID | CONTENTS_WINDOW )
#define MASK_PLAYERSOLID        contents_t( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_PLAYER )
#define MASK_DEADSOLID          contents_t( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW )
#define MASK_MONSTERSOLID       contents_t( CONTENTS_SOLID | CONTENTS_MONSTERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_PLAYER )
#define MASK_WATER              contents_t( CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME )
#define MASK_OPAQUE             contents_t( CONTENTS_SOLID | CONTENTS_SLIME | CONTENTS_LAVA )
#define MASK_SHOT               contents_t( CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_PLAYER | CONTENTS_WINDOW | CONTENTS_DEADMONSTER )
#define MASK_CURRENT            contents_t( CONTENTS_CURRENT_0 | CONTENTS_CURRENT_90 | CONTENTS_CURRENT_180 | CONTENTS_CURRENT_270 | CONTENTS_CURRENT_UP | CONTENTS_CURRENT_DOWN )
//#define MASK_BLOCK_SIGHT        ( CONTENTS_SOLID | CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_MONSTER | CONTENTS_PLAYER )
//#define MASK_NAV_SOLID          ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW )
//#define MASK_LADDER_NAV_SOLID   ( CONTENTS_SOLID | CONTENTS_WINDOW )
//#define MASK_WALK_NAV_SOLID     ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTERCLIP )
#define MASK_PROJECTILE         contents_t( MASK_SHOT | CONTENTS_PROJECTILECLIP )

// gi.BoxEdicts() can return a list of either solid or trigger entities
// FIXME: eliminate AREA_ distinction?
#define AREA_SOLID      1
#define AREA_TRIGGERS   2


/**
*   @brief  BSP Brush plane_t structure. To acquire the origin,
*           scale normal by dist.
**/
typedef struct cplane_s {
    vec3_t  normal;
    float   dist;
    byte    type;           // for fast side tests
    byte    signbits;       // signx + (signy<<1) + (signz<<1)
    byte    pad[ 2 ];
} cplane_t;

// 0-2 are axial planes
#define PLANE_X         0
#define PLANE_Y         1
#define PLANE_Z         2

// 3-5 are non-axial planes snapped to the nearest
#define PLANE_ANYX      3
#define PLANE_ANYY      4
#define PLANE_ANYZ      5

// planes (x&~1) and (x&~1)+1 are always opposites

#define PLANE_NON_AXIAL 6

/**
*   @brief  BSP Brush side surface.
*
*   Stores material/texture name, flags as well as an
*   integral 'value' which was commonly used for light flagged surfaces.
**/
typedef struct csurface_s {
    char        name[ 16 ];
    int         flags;
    int         value;
} csurface_t;

/**
*   @brief  A trace is basically 'sweeping' the collision shape(bounding box) through the world
*           and returning the results of that 'sweep'.
*           (ie, how far did it get, whether it is inside of a solid or not, etc)
**/
// a trace is returned when a box is swept through the world
typedef struct trace_s {
    qboolean    allsolid;   // if true, plane is not valid
    qboolean    startsolid; // if true, the initial point was in a solid area
    float       fraction;   // time completed, 1.0 = didn't hit anything
    vec3_t      endpos;     // final position
    cplane_t    plane;      // surface normal at impact
    csurface_t *surface;   // surface hit
    contents_t  contents;   // contents on other side of surface hit
    struct edict_s *ent;       // not set by CM_*() functions

    // [Paril-KEX] the second-best surface hit from a trace
    cplane_t	plane2;		// second surface normal at impact
    csurface_t *surface2;	// second surface hit
} trace_t;
