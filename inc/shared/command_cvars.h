/**
*
*   CVARS (console variables):
*
*   Actually part of the /common/ code. What is defined here, is the bare necessity of limited
*   API for the game codes to make use of.
*
**/
#ifndef CVAR
#define CVAR

#define CVAR_ARCHIVE    1   // set to cause it to be saved to vars.rc
#define CVAR_USERINFO   2   // added to userinfo  when changed
#define CVAR_SERVERINFO 4   // added to serverinfo when changed
#define CVAR_NOSET      8   // don't allow change from console at all,
                            // but can be set from the command line
#define CVAR_LATCH      16  // save changes until server restart
#ifndef CVAR_CHEAT
#define CVAR_CHEAT      (1 << 5)  // can't be changed when connected
#endif
#ifndef CVAR_PRIVATE
#define CVAR_PRIVATE	(1 << 6)  // never macro expanded or saved to config
#endif
#ifndef CVAR_ROM
#define CVAR_ROM		(1 << 7)  // can't be changed even from cmdline
#endif

struct cvar_s;
struct genctx_s;

typedef void ( *xchanged_t )( struct cvar_s * );
typedef void ( *xgenerator_t )( struct genctx_s * );

// nothing outside the cvar.*() functions should modify these fields!
typedef struct cvar_s {
    char *name;
    char *string;
    char *latched_string;    // for CVAR_LATCH vars
    int         flags;
    qboolean    modified;   // set each time the cvar is changed
    float       value;
    struct cvar_s *next;
    // 
    int         integer;
    char *default_string;
    xchanged_t      changed;
    xgenerator_t    generator;

    // ------ new stuff ------
    #if USE_CLIENT || USE_SERVER
        //int         integer;
        //char *default_string;
        //xchanged_t      changed;
        //xgenerator_t    generator;
    struct cvar_s *hashNext;
    #endif
} cvar_t;

#endif      // CVAR