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
Q_DEFINE_ZONETAG( TAG_SOUND, 9 )       // Sound memory goes here.
Q_DEFINE_ZONETAG( TAG_CMODEL, 10 )      // Collision Model memory goes here.
//! Server Game DLL tags start here.
Q_DEFINE_ZONETAG( TAG_SVGAME,           ( 11 ) ) //! Remains allocated until the server game is shut down. (Cleared in SVG_Shutdown)
Q_DEFINE_ZONETAG( TAG_SVGAME_LEVEL,     ( 12 ) ) //! Clear when loading a new level.
Q_DEFINE_ZONETAG( TAG_SVGAME_EDICTS,    ( 13 ) ) //! Cleared when erasing entity heap. Happens upon loading a new level.
Q_DEFINE_ZONETAG( TAG_SVGAME_LUA,       ( 14 ) ) //! Used by Lua, and cleared when the game module is unloaded.

//! Client Game DLL tags start here.
Q_DEFINE_ZONETAG( TAG_CLGAME, 15 )	        //! Remains allocated until the client game is shutdown.
Q_DEFINE_ZONETAG( TAG_CLGAME_LEVEL, 16 )    //! Clear when loading a new level.

//! The Game Modules can add custom Tag Zones after TAG_MAX.
Q_DEFINE_ZONETAG( TAG_MAX, 17 ) //! (IDs must be Within the range of UINT16_MAX).