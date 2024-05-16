/********************************************************************
*
*
*   User(-input) Command:
*
*
********************************************************************/
#pragma once


//! Set in case of no button being pressed for the frame at all.
#define BUTTON_NONE             BIT(0)
//! Set when the 'Primary Fire' button is held/down.
#define BUTTON_PRIMARY_FIRE     BIT(1)
//! Set when the 'Secondary Fire' button is held/down.
#define BUTTON_SECONDARY_FIRE   BIT(2)
//! Set when the jump key is held/pressed.
#define BUTTON_JUMP         BIT(3)
//! Set when the crouch/duck key is held/pressed.
#define BUTTON_CROUCH       BIT(4)
//! Set when the walk/run(defaults to shift) key is held/pressed.
#define BUTTON_WALK         BIT(5)
//! Set when a strafe to the left or right key is held/pressed.
#define BUTTON_STRAFE       BIT(6)
//! Set when the reload button is pressed.
#define BUTTON_RELOAD       BIT(7)
////! Set when the holster button is pressed.
//#define BUTTON_HOLSTER      BIT(7)
//! Set when the '+use' button is pressed. (Use item, not actually 'use targetting entity')
#define BUTTON_USE_ITEM     BIT(8)
//! Set when any button is pressed at all. Any key whatsoever(We keep the value lower for optimnized net code, max is 16 bits)
#define BUTTON_ANY          BIT(9)


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
    int64_t frameNumber;
} usercmd_t;