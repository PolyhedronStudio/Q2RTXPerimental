/********************************************************************
*
*
*   User(-input) Command:
*
*
********************************************************************/
#pragma once


//! Set in case of no button being pressed for the frame at all.
#define BUTTON_NONE         0
//! Set when the 'attack'(bombs away) button is pressed.
#define BUTTON_ATTACK       1
//! Set when the 'use' button is pressed. (Use item, not actually 'use targetting entity')
//! TODO: Probably rename to USE_ITEM instead later when this moves to the client game.
#define BUTTON_USE          2
//! Set when the holster button is pressed.
#define BUTTON_HOLSTER      4
//! Set when the jump key is pressed.
#define BUTTON_JUMP         8
//! Set when the crouch/duck key is pressed.
#define BUTTON_CROUCH       16
//! Set when any button is pressed at all.
#define BUTTON_ANY          128         // any key whatsoever


/**
*   @brief  usercmd_t is sent multiple times to the server for each client frame.
**/
typedef struct usercmd_s {
    //! Amount of miliseconds for this user command frame.
    uint8_t  msec;
    //! Button bits, determines which keys are pressed.
    uint16_t buttons;
    //! View angles.
    vec3_t  angles;
    //! Direction key when held, 'speeds':
    float   forwardmove, sidemove, upmove;
    //! The impulse.
    byte    impulse;        // remove?
    //! The frame number, can be used for possible anti-lag. TODO: Implement something for that.
    uint64_t frameNumber;
} usercmd_t;