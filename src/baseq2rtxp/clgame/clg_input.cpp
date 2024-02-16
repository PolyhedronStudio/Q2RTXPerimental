/********************************************************************
*
*
*	ClientGame: User Input.
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
#include "clg_local.h"

//typedef struct kbutton_s {
//    int32_t     down[ 2 ];        // key nums holding it down.
//    uint64_t    downtime;       // msec timestamp of when key was first pressed.
//    uint64_t	msec;           // msec down this frame
//    int32_t     state;
//} kbutton_t;