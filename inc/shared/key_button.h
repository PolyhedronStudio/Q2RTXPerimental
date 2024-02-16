/********************************************************************
*
*
*   KeyButton/KeyButton State:
*
*	Continuous button event tracking is complicated by the fact that two different
*	input sources (say, mouse button 1 and the control key) can both press the
*	same button, but the button should only be released when both of the
*	pressing key have been released.
*
*	When a key event issues a button command (+forward, +attack, etc), it appends
*	its key number as a parameter to the command so it can be matched up with
*	the release.
*
*	state bit 0 is the current state of the key
*	state bit 1 is edge triggered on the up to down transition
*	state bit 2 is edge triggered on the down to up transition
*
*
*	Key_Event (int key, bool down, unsigned time);
*
*	+mlook src time
*
********************************************************************/
#pragma once

/**
*   @brief  A button's state is either up(not pressed at all), down(pressed for a frame), or held(multiple frames).
**/
typedef enum keybutton_state_s {
    //! The current state of the key.
    BUTTON_STATE_HELD = BIT( 0 ),
    //! Edge triggered on the up to down transition.
    BUTTON_STATE_DOWN = BIT( 1 ),
    //! Edge triggered on the down to up transition
    BUTTON_STATE_UP = BIT( 2 )
} keybutton_state_t;

/**
*
**/
typedef struct keybutton_s {
    int32_t     down[ 2 ];      //! Key numbers that are holding it down.
    uint64_t    downtime;       //! Msec timestamp of when key was first pressed.
    uint64_t	msec;           //! Msec down this frame.
    keybutton_state_t state;    //! Actual state of the key at the downtime+msec moment in time.
} keybutton_t;