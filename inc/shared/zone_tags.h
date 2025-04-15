/********************************************************************
*
*
*   Zone Memory Tags:
*
*
********************************************************************/
#pragma once

//! Memory tag type.
typedef int32_t memtag_t;

//! Define a memory tag
#ifdef __cplusplus
    #define Q_DEFINE_ZONETAG(tag, value) static constexpr memtag_t \
        tag = (memtag_t)value;
#else
    #define Q_DEFINE_ZONETAG(tag, value) static const memtag_t \
        tag = (memtag_t)value;
#endif

/**
*   @brief  Memory tags to allow dynamic memory to be cleaned up.
* 
*   @note   Game DLLs have a separate tag space, starting at TAG_MAX + 128 offset.
*           This leaves room to add custom game tag memory blocks.
**/
Q_DEFINE_ZONETAG( TAG_FREE, 0 )         // Should have never been set.
Q_DEFINE_ZONETAG( TAG_STATIC, 1 )       // Static memory that will never be freed.
Q_DEFINE_ZONETAG( TAG_GENERAL, 2 )      // General memory goes here.
Q_DEFINE_ZONETAG( TAG_CMD, 3 )          // Command memory goes here.
Q_DEFINE_ZONETAG( TAG_CVAR, 4 )         // CVar memory goes here.
Q_DEFINE_ZONETAG( TAG_FILESYSTEM, 5 )   // Filesystem memory goes here.
Q_DEFINE_ZONETAG( TAG_RENDERER, 6 )     // Renderer memory goes here.
Q_DEFINE_ZONETAG( TAG_UI, 7 )           // UI memory goes here.
Q_DEFINE_ZONETAG( TAG_SERVER, 8 )       // Server memory goes here.
Q_DEFINE_ZONETAG( TAG_SOUND, 10 )       // Sound memory goes here.
Q_DEFINE_ZONETAG( TAG_CMODEL, 11 )      // Collision Model memory goes here.
Q_DEFINE_ZONETAG( TAG_MAX, 12 )         // Game DLLs offset from here.

/**
*   Server Game DLL tags start here. These defined here are also used by the Server. 
*
*   However, optionally the game can add custom Tag Zones to the remaining, non-used, 
*   reserved tag IDs.
**/
// (TAG_MAX + 128 = 140) 
Q_DEFINE_ZONETAG( TAG_SVGAME,           ( 13 ) ) //! Remains allocated until the game is shut down. (When unloading the dll.)
Q_DEFINE_ZONETAG( TAG_SVGAME_LEVEL,     ( 14 ) ) //! Clear when loading a new level.
Q_DEFINE_ZONETAG( TAG_SVGAME_EDICTS,    ( 15 ) ) //! Clear for erasing entity heap.
Q_DEFINE_ZONETAG( TAG_SVGAME_LUA,       ( 16 ) ) //! (TAG_MAX + 128 = 140) Server Game DLL tags start here.

/**
*   Client Game DLL tags start here. These defined here are also used by the Client.
*   
*   However, the game optionally add custom Tag Zones to the remaining, non-used,
*   reserved tag IDs.
**/
// (TAG_SVGAME + 128 = 268)
Q_DEFINE_ZONETAG( TAG_CLGAME, ( 17 ) )