/********************************************************************
*
*
*	ServerGame: Monster Movement
*
*
********************************************************************/
#pragma once



/**
*
*
*	Monster Move Constants:
*
*
**/
/**
*	Default BoundingBoxes:
**/
//! For when monster is standing straight up.
static constexpr Vector3 MM_BBOX_STANDUP_MINS = { -16.f, -16.f, -36.f };
static constexpr Vector3 MM_BBOX_STANDUP_MAXS = { 16.f, 16.f, 36.f };
static constexpr float   MM_VIEWHEIGHT_STANDUP = 30.f;
//! For when monster is crouching.
static constexpr Vector3 MM_BBOX_DUCKED_MINS = { -16.f, -16.f, -36.f };
static constexpr Vector3 MM_BBOX_DUCKED_MAXS = { 16.f, 16.f, 8.f };
static constexpr float   MM_VIEWHEIGHT_DUCKED = 4.f;
//! For when monster is gibbed out.
//static constexpr Vector3 PM_BBOX_GIBBED_MINS = { -16.f, -16.f, 0.f };
//static constexpr Vector3 PM_BBOX_GIBBED_MAXS = { 16.f, 16.f, 24.f };
//static constexpr float   PM_VIEWHEIGHT_GIBBED = 8.f;



/**
*	StepHeight:
**/
//! Minimal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr float MM_MIN_STEP_SIZE = 1.f;
//! Maximal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr float MM_MAX_STEP_SIZE = 18.f;



/**
*	Slide Move Results:
**/
//! Succesfully performed the move.
static constexpr int32_t MM_SLIDEMOVEFLAG_MOVED = BIT( 0 );
//! It was blocked at some point, doesn't mean it didn't slide along the blocking object.
static constexpr int32_t MM_SLIDEMOVEFLAG_BLOCKED = BIT( 1 );
//! It is trapped.
static constexpr int32_t MM_SLIDEMOVEFLAG_TRAPPED = BIT( 2 );
//! Blocked by a literal wall.
static constexpr int32_t MM_SLIDEMOVEFLAG_WALL_BLOCKED = BIT( 3 );
//! Touched at least a single plane along the way.
static constexpr int32_t MM_SLIDEMOVEFLAG_PLANE_TOUCHED = BIT( 4 );



/**
*
*
*   Monster Move:
*
*
**/
/**
*   mmove_type_t
**/
typedef enum {  // : uint8_t {
    // Types that can accelerate and turn:
    MM_NORMAL,      //! Gravity. Clips to world and its entities.
    MM_ROOT_MOTION, //! Same as MM_Normal but operates by using the rootbone to influence its velocity.
    MM_GRAPPLE,     //! No gravity. Pull towards velocity.
    MM_NOCLIP,      //! No gravity. Don't clip against entities/world at all. 

    // Types with no acceleration or turning support:
    MM_DEAD,        //! Dead.
    MM_GIB,         //! Different bounding box for when the monster is 'gibbing out'.
    MM_FREEZE       //! Freezes the monster right on the spot.
} mmove_type_t;

/**
*   mmove_state->pm_flags
**/
#define MMF_NONE						BIT( 0 )    //! No flags.
#define MMF_DUCKED						BIT( 1 )    //! Player is ducked.
#define MMF_JUMP_HELD					BIT( 2 )    //! Player is keeping jump button pressed.
#define MMF_ON_GROUND					BIT( 3 )    //! Player is on-ground.
#define MMF_TIME_WATERJUMP				BIT( 4 )    //! mm_time is waterjump.
#define MMF_TIME_LAND					BIT( 5 )    //! mm_time is time before rejump.
#define MMF_TIME_TELEPORT				BIT( 6 )    //! mm_time is non-moving time.
//#define PMF_TELEPORT_BIT				BIT( 7 )    //! used by q2pro
#define MMF_TIME_TRICK_JUMP				BIT( 7 )   //! pm_time is the trick jump time.
#define MMF_ON_LADDER					BIT( 8 )    //! Signal to game that we are on a ladder.
#define	MMF_ALL_TIMES                   ( MMF_TIME_WATERJUMP | MMF_TIME_LAND | MMF_TIME_TELEPORT | MMF_TIME_TRICK_JUMP )


//! Maximum number of player state events.
#define MAX_MMS_EVENTS 2

/**
*   @brief  Stores in gedict_t and contains the actual relevant monster movement state.
**/
typedef struct mmove_state_s {
    //! The player move type.
    mmove_type_t    mm_type;
    //! The state's flags describing the move's situation.
    uint16_t        mm_flags;
    //! Timer value for a specific few of state flags.
    //! Each unit = 8 ms
    uint16_t        mm_time;

    //! Gravity to apply.
    int16_t gravity;

    //! Origin.
    Vector3		origin;
    //! Velocity.
    Vector3		velocity;
    //! Previous Origin.
    Vector3     previousOrigin;
    //! Previous Velocity.
    Vector3		previousVelocity;
} mmove_state_t;


/**
*   @brief  Used for registering entity touching resulting traces.
**/
static constexpr int32_t MM_MAX_TOUCH_TRACES = 32;
typedef struct mm_touch_trace_list_s {
    uint32_t numberOfTraces;
    svg_trace_t traces[ MM_MAX_TOUCH_TRACES ];
} mm_touch_trace_list_t;

///**
//*   @brief  Stores the final ground information results.
//**/
//typedef struct mm_ground_info_s {
//    //! Pointer to the actual ground entity we are on.(nullptr if none).
//    struct edict_s *entity;
//
//    //! A copy of the plane data from the ground entity.
//    cm_plane_t        plane;
//    //! A copy of the ground plane's surface data. (May be none, in which case, it has a 0 name.)
//    cm_surface_t      surface;
//    //! A copy of the contents data from the ground entity's brush.
//    cm_contents_t      contents;
//    //! A pointer to the material data of the ground brush' surface we are standing on. (nullptr if none).
//    cm_material_t *material;
//} mm_ground_info_t;
//
///**
//*   @brief  Stores the final 'liquid' information results. This can be lava, slime, or water, or none.
//**/
//typedef struct mm_liquid_info_s {
//    //! The actual BSP liquid 'contents' type we're residing in.
//    cm_contents_t      type;
//    //! The depth of the player in the actual liquid.
//    cm_liquid_level_t	level;
//} mm_liquid_info_t;

/**
*   @brief  Structure going in and out of the monster move process.
**/
typedef struct mm_move_s {
    //! [In]: Used as trace skip/pass-entity, usually points to the monster itself.
    svg_base_edict_t *monster;
    //! [In]: Frametime.
    double frameTime;
    //! [In]: Bounds.
    Vector3 mins, maxs;

    //! [In/Out]: The actual monster's move state.
    mmove_state_t state;
    //! [In/Out]: Stores the ground information. If there is no actual ground, ground.entity will be nullptr.
    ground_info_t ground;
    //! [In/Out]: Stores the possible solid liquid type brush we're in(-touch with/inside of)
    liquid_info_t liquid;

    //! [Out]: Touch Traces.
    mm_touch_trace_list_t touchTraces;
    //! [Out]:
    bool jump_sound;
    //! [Out]:
    struct {
        //! If clipped to stair.
        bool clipped;
        //! Height of step.
        float height;
    } step;
} mm_move_t;



/**
*
*
*	Monster Move Utility Functions:
*
*
**/
/**
*	@brief	Clips trace against world only.
**/
const svg_trace_t SVG_MMove_Clip( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, const cm_contents_t contentMask );
/**
*	@brief	Determines the mask to use and returns a trace doing so. If spectating, it'll return clip instead.
**/
const svg_trace_t SVG_MMove_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, svg_base_edict_t *passEntity, cm_contents_t contentMask = CONTENTS_NONE );



/**
*
*
*	Monster Move Functions:
*
*
**/
/**
*	@brief	Each intersection will try to step over the obstruction instead of
*			sliding along it.
*
*			Returns a new origin, velocity, and contact entity
*			Does not modify any world state?
**/
const int32_t SVG_MMove_StepSlideMove( mm_move_t *monsterMove );

/**
*	@brief	Will move the yaw to its ideal position based on the yaw speed(per frame) value.
**/
void SVG_MMove_FaceIdealYaw( svg_base_edict_t *ent, const float idealYaw, const float yawSpeed );