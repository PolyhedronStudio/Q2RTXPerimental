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
    //! For general messages.
    PRINT_ALL,
    //! Prints in green color.
    PRINT_TALK,
    //! Only prints when the cvar "developer" == 1
    PRINT_DEVELOPER,
    PRINT_WARNING,      // Print in Yellow color.
    PRINT_ERROR,        // Print in Red color.
    PRINT_NOTICE        // Print in Cyan color.
} print_type_t;

//! Prints to console the specific message in its 'type'.
void    Com_LPrintf( print_type_t type, const char *fmt, ... )
q_printf( 2, 3 );
//! For erroring out and returning(in most cases) back to a disconnect console.
void    Com_Error( error_type_t code, const char *fmt, ... )
q_noreturn q_printf( 2, 3 );

#define Com_Printf(...) Com_LPrintf(PRINT_ALL, __VA_ARGS__)
#define Com_WPrintf(...) Com_LPrintf(PRINT_WARNING, __VA_ARGS__)
#define Com_EPrintf(...) Com_LPrintf(PRINT_ERROR, __VA_ARGS__)

#define Q_assert(expr) \
    do { if (!(expr)) Com_Error(ERR_FATAL, "%s: assertion `%s' failed", __func__, #expr); } while (0)

// Game Print Flags:
#define PRINT_LOW           0       // Pickup messages.
#define PRINT_MEDIUM        1       // Death messages.
#define PRINT_HIGH          2       // Critical messages.
#define PRINT_CHAT          3       // Chat messages.