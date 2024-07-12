/********************************************************************
*
*
*   Shared "Server to Client", and "Client to Server" Messages:
*
*
********************************************************************/
#pragma once

// Server to Client
typedef enum {
    svc_bad,

    // these are private to the client and server
    svc_nop,
    svc_disconnect,
    svc_reconnect,
    svc_sound,                  // <see code>
    svc_print,                  // [byte] id [string] null terminated string
    svc_stufftext,              // [string] stuffed into client's console buffer should be \n terminated
    svc_serverdata,             // [long] protocol ...
    svc_configstring,           // [short] [string]
    svc_spawnbaseline,
    svc_centerprint,            // [string] to put in center of the screen
    svc_download,               // [short] size [size bytes]
    svc_playerinfo,             // variable
    svc_packetentities,         // [...]
    svc_deltapacketentities,    // [...]
    svc_frame,

    svc_portalbits,             // Will send all portal state bits as they are to the client when the frame is 'FF_NODELTA', which'll (also) be true for the very first svc_frame.
    svc_set_portalbit,          // Will send any portal state bit changes to clients when the svc_frame is a delta frame.

    svc_zpacket,
    svc_zdownload,

    svc_gamestate,
    svc_configstringstream,
    svc_baselinestream,

    // These are also known to the game dlls, however, also needed by the engine.
    svc_muzzleflash,
    //svc_muzzleflash2, WID: Removed, monster muzzleflashes based on hard coded static array.
    svc_temp_entity,
    svc_layout,
    svc_inventory,
    //svc_scoreboard,

    svc_svgame                 // The server game is allowed to add custom commands after this. Max limit is a byte, 255.
} server_command_t;

// Client to Server
typedef enum {
    clc_bad,                // Bad command.
    clc_nop,                // 'No' command.

    clc_move,               // [usercmd_t]
    clc_move_nodelta,		// [usercmd_t]
    clc_move_batched,		// [batched_usercmd_t]

    clc_userinfo,           // [userinfo string]
    clc_userinfo_delta,		// [userinfo_key][userinfo_value]

    clc_stringcmd,          // [string] message

    clc_clgame                 // The client game is allowed to add custom commands after this. Max limit is a byte, 255.
} client_command_t;