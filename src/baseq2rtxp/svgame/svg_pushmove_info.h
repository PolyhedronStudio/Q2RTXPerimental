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
typedef void( *svg_pushmove_endcallback )( edict_t * );

/**
*   @brief  Stores movement info for 'Pushers(also known as Movers)'.
***/
typedef struct {
    //
    // Start/End Data:
    //
    //! Move start origin.
    Vector3     startOrigin;
    //! Move start angles.
    Vector3     startAngles;
    //! Move end origin.
    Vector3     endOrigin;
    //! Move end angles.
    Vector3     endAngles;
    //! The start position, texture animation frame index.
    int32_t     startFrame;
    //! The end position, texture animation frame index.
    int32_t     endFrame;

    //
    // Dynamic State Data
    //
    svg_pushmove_state_t state;
    //! Destination origin.
    Vector3     dest;
    //! Direction vector(not normalized.)
    Vector3     dir;
    bool        in_motion;  //! Hard set by begin and final functions.
    float       current_speed;
    float       move_speed;
    float       next_speed;
    float       remaining_distance;
    float       decel_distance;

    //
    //  Acceleration Data.
    // 
    //! Acceleration.
    float       accel;
    //! Speed.
    float       speed;
    //! Deceleration.
    float       decel;
    //! Distance, or, amount of rotation angle in degrees.
    float       distance;
    //! Time to wait before returning back to initial state. (Non toggleables).
    float       wait;

    //
    // SubMove Data(Q2RE Style):
    //
    // Curve reference.
    struct {
        //! Origin position we're currently at.
        Vector3 referenceOrigin;
        //! Number of curve positions.
        int64_t countPositions;
        //! Dynamically (re-)allocated in-game.
        //float *positions;
        sg_qtag_memory_t<float, TAG_SVGAME_LEVEL> positions;
        //float positions[ 1024 ]; // WID:TODO: Make saveable dynamic alloc block.
        // Frame index.
        int64_t frame;
        //! Current curve.subFrame, total number of subframes.
        uint8_t subFrame, numberSubFrames;
        //! Number of subframes processed.
        int64_t numberFramesDone;
    } curve;

    //
    // Lock/Unlock Data
    //
    struct {
        //! Whether this pusher is 'locked', think of doors or even buttons.
        bool isLocked;

        //! Locked sound.
        qhandle_t lockedSound;
        //! Lock sound.
        qhandle_t lockingSound;
        //! Unlock sound.
        qhandle_t unlockingSound;
    } lockState;

    //
    // Sounds.
    //
    struct {
        //! Start move sound.
        qhandle_t   start;
        //! Continuous repeating move sound.
        qhandle_t   middle;
        //! End move sound.
        qhandle_t   end;
    } sounds;

    //! Callback function for when a 'Move' has ended by reached its destination.
    svg_pushmove_endcallback endMoveCallback;

    // WID: MoveWith:
    Vector3 lastVelocity;
} svg_pushmove_info_t;