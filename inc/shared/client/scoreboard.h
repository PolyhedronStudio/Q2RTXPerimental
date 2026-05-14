#pragma once

#include "client/client_types.h"

/**
*   @brief  A single entry in the client-side scoreboard.
**/
typedef struct scoreboard_entry_s {
    //! Client slot number used by the server.
    int32_t clientNumber;
    //! Team identifier sent by the server (0 = none/unassigned).
    int32_t clientTeamId;
    //! Player connect / score time value sent by svc_scoreboard.
    int32_t clientTime;
    //! Player score value sent by svc_scoreboard.
    int32_t clientScore;
    //! Player ping value sent by svc_scoreboard.
    int32_t clientPing;
    //! Display name for the client.
    char clientName[ 32 ];
    //! Whether this entry is currently valid for display.
    bool isValidSlot;
} scoreboard_entry_t;
