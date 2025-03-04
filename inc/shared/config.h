/**
*
*
*	Q2RTXPerimental Config File:
*
*
**/

/**
*	By CMake set defines concattenated into an engine name/branch/commit version string.
**/
// expand to generate version string
#define STRING(x) #x
#define _STR(x) STRING(x)
#define VERSION_STRING _STR(VERSION_MAJOR) "." _STR(VERSION_MINOR) "." _STR(VERSION_POINT)
#define LONG_VERSION_STRING _STR(VERSION_MAJOR) "." _STR(VERSION_MINOR) "." _STR(VERSION_POINT) "-" _STR(VERSION_BRANCH) "-" _STR(VERSION_SHA)


/**
*	CPU String for display usage.
**/
#ifdef _WIN64
#define CPUSTRING "x86_64"
#define BUILDSTRING "Win64"
#elif  _WIN32
#define CPUSTRING "x86"
#define BUILDSTRING "Win32"
#elif __aarch64__
#define CPUSTRING "aarch64"
#define BUILDSTRING "Linux"
#elif __x86_64__
#define CPUSTRING "x86_64"
#define BUILDSTRING "Linux"
#elif __powerpc64__ && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define CPUSTRING "ppc64le"
#define BUILDSTRING "Linux"
#else
#define CPUSTRING "x86"
#define BUILDSTRING "Linux"
#endif


/**
*	In-Engine Fixes:
**/
//! This can be uncommented and/or removed when ericw-tools-2.0.0-alpha7 or higher
//! has fixed the leafcontents issue.
//#define ERICW_TOOLS_LEAF_CONTENTS_FIX 1


/**
*	Engine Tick Rate:
**/
// Default server FPS: 10hz
//#define BASE_FRAMERATE          10
//#define BASE_FRAMETIME          100
//#define BASE_1_FRAMETIME        0.01f   // 1/BASE_FRAMETIME
//#define BASE_FRAMETIME_1000     0.1f    // BASE_FRAMETIME/1000

// Default Server FPS: 40hz
//#define BASE_FRAMERATE          40		// OLD: 10 fps	NEW: 40fps
//#define BASE_FRAMETIME          25		// OLD: 100		NEW: 1000 / BASE_FRAMERATE = 25
//#define BASE_1_FRAMETIME        0.04f	// OLD: 0.01f   NEW: 1 / BASE_FRAMETIME
//#define BASE_FRAMETIME_1000     0.025f	// OLD: 0.1f	NEW: BASE_FRAMETIME / 1000
#define BASE_FRAMERATE          (40.)						// OLD: 40 fps	NEW: 40fps
#define BASE_FRAMETIME          (1000./BASE_FRAMERATE)		// OLD: 25		NEW: 1000 / BASE_FRAMERATE = 25
#define BASE_1_FRAMETIME        (1./BASE_FRAMETIME)			// OLD: 0.04f   NEW: 1 / BASE_FRAMETIME
#define BASE_FRAMETIME_1000     (BASE_FRAMETIME/1000.)		// OLD: 0.0.025 NEW: BASE_FRAMETIME / 1000


/**
*	Game Path Configuration:
**/
//! The 'base' game directory is where the data and shared libraries of the 'main' game actually resides.
#define BASEGAME "baseq2rtxp"
//! The default mod game directory to launch. When left empty, the default game data and shared libraries to load will be BASEGAME itself.
#define DEFAULT_GAME ""

//! Compile with Windows NT ICMP:
#define USE_ICMP 1
//! Compile with zlib enabled. TODO: We can't really live without it, so...?
#define USE_ZLIB 1
//! Compile with system console output utilities enabled.
#define USE_SYSCON 1
//! Compile with debugging help.
#define USE_DBGHELP 1

/**
*	Client Specific Configuration:
**/
#if USE_CLIENT
//! TODO: These still neccessary?
//#define VID_REF "gl"
//! Used as a list to select from in case SDL fails to deliver a list of its own.
#define VID_MODELIST "640x480 800x600 1024x768 1280x720 1920x1080"
//! Size and optional position of the main window on virtual desktop.
#define VID_GEOMETRY "1280x720"
//! Compile with the UI code enabled.
#define USE_UI 1
//! Whether to render a custom UI cursor or not.
//#define USE_UI_ENABLE_CUSTOM_CURSOR 1
//! Enable Bitmap UI menu item controls.
//#define USE_UI_ENABLE_BITMAP_ITEM_CONTROL 1
//! Enable PNG, JPG and TGA image loading support.
#define USE_PNG 1
#define USE_JPG 1
#define USE_TGA 1
//! Support MD3 format loading.
#define USE_MD3 1
//! Compile with OpenAL sound backend enabled.
#define USE_OPENAL 1
//! Compile with SDL DMA sound backend enabled.
#define USE_SNDDMA 1
//! Whether to send auto reply messages for each frame when in a pause/idle client state, this is to keep the connection alive.
#define USE_AUTOREPLY 1

// Already defined by CMake:
//#define REF_GL 1
//#define USE_REF REF_GL
//#define USE_CURL 0
#endif


/**
*	Server Specific Configuration:
**/
#if USE_SERVER
#define USE_WINSVC !USE_CLIENT
#endif
//! Defined to prevent a compiler warning due to SDL redefining already defined math values.
#define _USE_MATH_DEFINES

//! Have C inline just like C++ inline.
#ifndef __cplusplus
#define inline __inline
#endif // __cplusplus

//! For debugging, macro to get the current function string name.
#define __func__ __FUNCTION__

//! System dependent ssize_t type.
#ifdef _WIN64
typedef __int64     ssize_t;
#define SSIZE_MAX   _I64_MAX
#elif _WIN32
typedef __int32     ssize_t;
#define SSIZE_MAX   _I32_MAX
#endif

/**
*	Disable some visual studio specific warnings:
**/
#if defined(_MSC_VER)
#pragma warning(disable:4018)
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4305)
#endif
