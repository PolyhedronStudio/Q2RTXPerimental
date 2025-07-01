/********************************************************************
*
*
*   Network Elements communicated across the net:
*
*
********************************************************************/
#pragma once


//! Can't be increased without changing network protocol.
#define MAX_MAP_AREAS           256
//! Bitmasks communicated by server
#define MAX_MAP_AREA_BYTES      (MAX_MAP_AREAS / 8)
#define MAX_MAP_PORTAL_BYTES    MAX_MAP_AREA_BYTES

//! Circular update array.
#define UPDATE_BACKUP           16 //! 16	//! copies of entity_state_t to keep buffered must be power of two
#define UPDATE_MASK             (UPDATE_BACKUP - 1)

//! Circular command array.
#define CMD_BACKUP              1024 //! 128	//! allow a lot of command backups for very fast systems increased from 64
#define CMD_MASK                (CMD_BACKUP - 1)

//! Max entities stuffed per packet.
#define MAX_PACKET_ENTITIES     1024
#define MAX_PARSE_ENTITIES      (MAX_PACKET_ENTITIES * UPDATE_BACKUP)
#define PARSE_ENTITIES_MASK     (MAX_PARSE_ENTITIES - 1)

//! Configstring Bitmap Bytes.
#define CS_BITMAP_BYTES         (MAX_CONFIGSTRINGS / 8) // 260

//! Entity State messaging flags:
typedef enum {
    MSG_ES_NONE = 0,
    //! Send even if unchanged.
    MSG_ES_FORCE = ( 1 << 0 ),
    //! Send old_origin.
    MSG_ES_NEWENTITY = ( 1 << 1 ),
    //! Ignore origin/angles.
    MSG_ES_FIRSTPERSON = ( 1 << 2 ),
    //! Client has RF_BEAM old_origin fix.
    MSG_ES_BEAMORIGIN = ( 1 << 3 ),
} msgEsFlags_t;
QENUM_BIT_FLAGS( msgEsFlags_t );

//! Optional (Read/Write-)Position Encoding Types:
typedef enum {
    //! Writes out as a full float.
    MSG_POSITION_ENCODING_NONE = 0,           
    //! Encodes to short coordinate, -4096/+4096 range.
    MSG_POSITION_ENCODING_SHORT = 1,           
    //! Writes out 13 bits + float bias.
    MSG_POSITION_ENCODING_TRUNCATED_FLOAT = 2, 
} msgPositionEncoding_t;