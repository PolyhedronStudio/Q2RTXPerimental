/**
*
*   Command Console Printing:
*
*   Actually part of the /common/ code. What is defined here, is the bare necessity of limited
*   API for the game codes to make use of.
*
**/
#pragma once


/**
*   @brief  Com_Error specific types:
**/
typedef enum {
    //! Exit the entire game with a popup window.
    ERR_FATAL,
    //! Print to console and disconnect from game.
    ERR_DROP,
    //! Like ERR_DROP, but not an error.
    ERR_DISCONNECT,
    //! Make the server broadcast 'reconnect' message.
    ERR_RECONNECT
} error_type_t;
/**
*   @brief  Com_LPrintf specific types:
**/
typedef enum {
    //! Prints using default conchars text and thus color.
    //! Used for General messages..
    PRINT_ALL,
    //! Prints in Green color.
    PRINT_TALK,
    //! Only prints when the cvar "developer" >= 1
    PRINT_DEVELOPER,
    //! Print a Warning in Yellow color.
    PRINT_WARNING,
    //! Print an Error in Red color.
    PRINT_ERROR,
    //! Print a Notice in bright Cyan color.
    PRINT_NOTICE
} print_type_t;

QEXTERN_C_OPEN
    //! Prints to console the specific message in its 'type'.
    void    Com_LPrintf( print_type_t type, const char *fmt, ... )
        q_printf( 2, 3 );
    //! For erroring out and returning(in most cases) back to a disconnect console.
    void    Com_Error( error_type_t code, const char *fmt, ... )
        q_noreturn q_printf( 2, 3 );
QEXTERN_C_CLOSE

#define Com_Printf(...) Com_LPrintf(PRINT_ALL, __VA_ARGS__)
#define Com_WPrintf(...) Com_LPrintf(PRINT_WARNING, __VA_ARGS__)
#define Com_EPrintf(...) Com_LPrintf(PRINT_ERROR, __VA_ARGS__)

//! WID: Assertions that always trigger.
#define Q_assert(expr) \
    do { if (!(expr)) Com_Error(ERR_FATAL, "%s: assertion `%s' failed", __func__, #expr); } while (0)
//! WID: For when cvar developer >= 1
#define Q_DevAssert(expr) \
    do { if ( !(expr)) Com_LPrintf( PRINT_DEVELOPER, "%s:%s:%d: DevAssert failed: `%s'\n", __FILE__, __func__, __LINE__, #expr); } while (0)
//! WID: For when cvar developer >= 1
#define Q_DevPrint(...) \
    do { Com_LPrintf( PRINT_DEVELOPER, "%s:%s:%d: %s\n", __FILE__, __func__, __LINE__, __VA_ARGS__ ); } while (0)

// Game Print Flags:
#define PRINT_LOW           0       // Pickup messages.
#define PRINT_MEDIUM        1       // Death messages.
#define PRINT_HIGH          2       // Critical messages.
#define PRINT_CHAT          3       // Chat messages.