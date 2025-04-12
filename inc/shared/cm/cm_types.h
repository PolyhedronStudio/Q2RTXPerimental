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
    SOLID_BOUNDS_BOX,         //! Touch on bounding box edge.
    SOLID_BOUNDS_OCTAGON,   //! Touch on its 8 edges.
    SOLID_BSP           //! BSP clip, touch on edge.
} cm_solid_t;

/**
*   @brief  The water 'level' of said entity.
**/
// edict->liquidlevel values
typedef enum {  //: uint8_t {
    LIQUID_NONE,
    LIQUID_FEET,
    LIQUID_WAIST,
    LIQUID_UNDER
} cm_liquid_level_t;

/**
*   @brief  Brush Contents: lower bits are stronger, and will eat weaker brushes completely
**/
typedef enum cm_contents_s {
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
} cm_contents_t;
QENUM_BIT_FLAGS( cm_contents_t );



/**
*   ContentMask Sets:
**/
#define CM_CONTENTMASK_SOLID              /*cm_contents_t*/( CONTENTS_SOLID | CONTENTS_WINDOW )
#define CM_CONTENTMASK_PLAYERSOLID        /*cm_contents_t*/( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_PLAYER )
#define CM_CONTENTMASK_DEADSOLID          /*cm_contents_t*/( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW )
#define CM_CONTENTMASK_MONSTERSOLID       /*cm_contents_t*/( CONTENTS_SOLID | CONTENTS_MONSTERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_PLAYER )
#define CM_CONTENTMASK_WATER              /*cm_contents_t*/( CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME )
#define CM_CONTENTMASK_OPAQUE             /*cm_contents_t*/( CONTENTS_SOLID | CONTENTS_SLIME | CONTENTS_LAVA )
#define CM_CONTENTMASK_SHOT               /*cm_contents_t*/( CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_PLAYER | CONTENTS_WINDOW | CONTENTS_DEADMONSTER )
#define CM_CONTENTMASK_CURRENT            /*cm_contents_t*/( CONTENTS_CURRENT_0 | CONTENTS_CURRENT_90 | CONTENTS_CURRENT_180 | CONTENTS_CURRENT_270 | CONTENTS_CURRENT_UP | CONTENTS_CURRENT_DOWN )
//#define MASK_BLOCK_SIGHT        ( CONTENTS_SOLID | CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_MONSTER | CONTENTS_PLAYER )
//#define MASK_NAV_SOLID          ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW )
//#define MASK_LADDER_NAV_SOLID   ( CONTENTS_SOLID | CONTENTS_WINDOW )
//#define MASK_WALK_NAV_SOLID     ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTERCLIP )
#define CM_CONTENTMASK_PROJECTILE         /*cm_contents_t*/( CM_CONTENTMASK_SHOT | CONTENTS_PROJECTILECLIP )