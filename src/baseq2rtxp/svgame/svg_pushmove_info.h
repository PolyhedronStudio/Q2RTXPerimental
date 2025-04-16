/********************************************************************
*
*
*	ServerGame: PushMoveInfo Data Structure
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*	@brief	State Constants, top and down are synonymous to open and close, up to opening, down to closing.
**/
typedef enum svg_pushmove_state_e {
    PUSHMOVE_STATE_TOP          = 0,
    PUSHMOVE_STATE_BOTTOM       = 1,
    PUSHMOVE_STATE_MOVING_UP    = 2,
    PUSHMOVE_STATE_MOVING_DOWN  = 3,
} svg_pushmove_state_t;

//! Typedef for pushmove end move callback.
typedef void( *svg_pushmove_endcallback )( svg_base_edict_t * );

/**
*   @brief  Stores movement info for 'Pushers(also known as Movers)'.
***/
struct svg_pushmove_info_t {
    //
    // Start/End Data:
    //
    //! Move start origin.
    Vector3     startOrigin = QM_Vector3Zero();
    //! Move start angles.
    Vector3     startAngles = QM_Vector3Zero();
    //! Move end origin.
    Vector3     endOrigin = QM_Vector3Zero();
    //! Move end angles.
    Vector3     endAngles = QM_Vector3Zero();
    //! The start position, texture animation frame index.
    int32_t     startFrame = 0;
    //! The end position, texture animation frame index.
    int32_t     endFrame = 0;

    //
    // Dynamic State Data
    //
    svg_pushmove_state_t state = {};
    //! Destination origin.
    Vector3     dest = QM_Vector3Zero();
    //! Direction vector(not normalized.)
    Vector3     dir = QM_Vector3Zero();
    bool        in_motion = false;  //! Hard set by begin and final functions.
    float       current_speed = 0.f;
    float       move_speed = 0.f;
    float       next_speed = 0.f;
    float       remaining_distance = 0.f;
    float       decel_distance = 0.f;

    //
    //  Acceleration Data.
    // 
    //! Acceleration.
    float       accel = 0.f;
    //! Speed.
    float       speed = 0.f;
    //! Deceleration.
    float       decel = 0.f;
    //! Distance, or, amount of rotation angle in degrees.
    float       distance = 0.f;
    //! Time to wait before returning back to initial state. (Non toggleables).
    float       wait = 0.f;

    //
    // SubMove Data(Q2RE Style):
    //
    // Curve reference.
    struct {
        //! Origin position we're currently at.
        Vector3 referenceOrigin = QM_Vector3Zero();
        //! Number of curve positions.
        int64_t countPositions = 0;
        //! Dynamically (re-)allocated in-game.
        //float *positions;
        sg_qtag_memory_t<float, TAG_SVGAME_LEVEL> positions = sg_qtag_memory_t<float, TAG_SVGAME_LEVEL>::sg_qtag_memory_t(nullptr, 0);
        //float positions[ 1024 ]; // WID:TODO: Make saveable dynamic alloc block.
        // Frame index.
        int64_t frame = 0;
        //! Current curve.subFrame, total number of subframes.
        uint8_t subFrame = 0, numberSubFrames = 0;
        //! Number of subframes processed.
        int64_t numberFramesDone = 0;
    } curve;

    //
    // Lock/Unlock Data
    //
    struct {
        //! Whether this pusher is 'locked', think of doors or even buttons.
        bool isLocked = false;

        //! Locked sound.
        qhandle_t lockedSound = 0;
        //! Lock sound.
        qhandle_t lockingSound = 0;
        //! Unlock sound.
        qhandle_t unlockingSound = 0;
    } lockState;

    //
    // Sounds.
    //
    struct {
        //! Start move sound.
        qhandle_t   start = 0;
        //! Continuous repeating move sound.
        qhandle_t   middle = 0;
        //! End move sound.
        qhandle_t   end = 0;
    } sounds;

    //! Callback function for when a 'Move' has ended by reached its destination.
    svg_pushmove_endcallback endMoveCallback = nullptr;

    // WID: MoveWith:
    Vector3 lastVelocity = QM_Vector3Zero();
};