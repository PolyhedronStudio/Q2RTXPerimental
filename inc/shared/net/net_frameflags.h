/********************************************************************
*
*
*   Frame Flags, for both client and server:
*
*
********************************************************************/
#pragma once


/**
*   FrameFlags:
**/
//! Type.
typedef uint32_t frameflags_t;

/**
*   Server FrameFlags(Wired to the client):
**/
//! No specific flags.
#define FF_NONE         ( 0 )
//! The client suppressed this frame.
#define FF_SUPPRESSED   BIT(0)
//! The client dropped this frame.
#define FF_CLIENTDROP   BIT(1)
//! The client has to rely on prediction for this frame.
#define FF_CLIENTPRED   BIT(2)  // Set but unused?
//! Literally reserved.
#define FF_RESERVED     BIT(3)

/**
*   Locally calculated frame flags for debug display.
**/
// The server dropped this frame.
#define FF_SERVERDROP   BIT(4)
// A bad frame, so delta'd from the last good one.
#define FF_BADFRAME     BIT(5)
// No delta compression used.
#define FF_OLDFRAME     BIT(6)
// Delta'd from oldframe instead of lastframe.
#define FF_OLDENT       BIT(7)
// No delta, a full update.
#define FF_NODELTA      BIT(8)
