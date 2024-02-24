/********************************************************************
*
*
*   Sound:
*
*
********************************************************************/
#pragma once

/**
*	Sound Channels:
*	
*	Channel 0 never willingly overrides
*	Other channels (1-7) allways override a playing sound on that channel
**/
#define CHAN_AUTO               0
#define CHAN_WEAPON             1
#define CHAN_VOICE              2
#define CHAN_ITEM               3
#define CHAN_BODY               4
//! Modifier flags:
#define CHAN_NO_PHS_ADD         8   //! Send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define CHAN_RELIABLE           16  //! Send by reliable message, not datagram.

// Sound attenuation values:
#define ATTN_NONE               0   //! Full volume the entire level
#define ATTN_NORM               1
#define ATTN_IDLE               2
#define ATTN_STATIC             3   //! Diminish very rapidly with distance.