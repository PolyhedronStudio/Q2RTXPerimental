/**
*
*   Shared Cmd Types, these used to be in common, however for game API's sake, some moved here.
*
**/
#ifndef CMDDEF
#define CMDDEF

// where did current command come from?
typedef enum {
    FROM_STUFFTEXT,
    FROM_RCON,
    FROM_MENU,
    FROM_CONSOLE,
    FROM_CMDLINE,
    FROM_CODE
} from_t;

typedef struct cmdbuf_s {
    from_t      from;
    char *text; // may not be NULL terminated
    size_t      cursize;
    size_t      maxsize;
    int64_t     waitCount;
    int64_t		aliasCount; // for detecting runaway loops
    void        ( *exec )( struct cmdbuf_s *, const char * );
} cmdbuf_t;

typedef struct genctx_s {
    const char *partial;
    size_t length;
    int argnum;
    char **matches;
    int count, size;
    void *data;
    bool ignorecase;
    bool ignoredups;
} genctx_t;

typedef void ( *xcommand_t )( void );
typedef void ( *xcommandex_t )( cmdbuf_t * );
typedef size_t( *xmacro_t )( char *, size_t );
typedef void ( *xcompleter_t )( struct genctx_s *, int );

typedef struct cmd_macro_s {
    struct cmd_macro_s *next, *hashNext;
    const char *name;
    xmacro_t            function;
} cmd_macro_t;

typedef struct {
    const char *sh, *lo, *help;
} cmd_option_t;

typedef struct cmdreg_s {
    const char *name;
    xcommand_t      function;
    xcompleter_t    completer;
} cmdreg_t;

#endif // CMDDEF