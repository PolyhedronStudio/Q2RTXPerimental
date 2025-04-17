/********************************************************************
*
*
*	SVGame: Server(Game Admin Stuff alike) Console Commands:
*
*
********************************************************************/
#include "svgame/svg_local.h"

/**
*   @brief  
**/
void ServerCommand_Test_f(void)
{
    gi.cprintf(NULL, PRINT_HIGH, "ServerCommand_Test_f()\n");
}


/*
==============================================================================

PACKET FILTERING


You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".

Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.

listip
Prints the current list of filters.

writeip
Dumps "addip <ip>" commands to listip.cfg so it can be execed at a later date.  The filter lists are not saved and restored by default, because I beleive it would cause too much confusion.

filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.


==============================================================================
*/

typedef struct {
    unsigned    mask;
    unsigned    compare;
} ipfilter_t;

#define MAX_IPFILTERS   1024

ipfilter_t  ipfilters[MAX_IPFILTERS];
int         numipfilters;

/*
=================
StringToFilter
=================
*/
static bool StringToFilter(char *s, ipfilter_t *f)
{
    char    num[128];
    int     i, j;
    union {
        byte bytes[4];
        unsigned u32;
    } b, m;

    b.u32 = m.u32 = 0;

    for (i = 0 ; i < 4 ; i++) {
        if (*s < '0' || *s > '9') {
            gi.cprintf(NULL, PRINT_HIGH, "Bad filter address: %s\n", s);
            return false;
        }

        j = 0;
        while (*s >= '0' && *s <= '9') {
            num[j++] = *s++;
        }
        num[j] = 0;
        b.bytes[i] = atoi(num);
        if (b.bytes[i] != 0)
            m.bytes[i] = 255;

        if (!*s)
            break;
        s++;
    }

    f->mask = m.u32;
    f->compare = b.u32;

    return true;
}

/*
=================
SVG_FilterPacket
=================
*/
const bool SVG_FilterPacket(char *from)
{
    int     i;
    unsigned    in;
    union {
        byte b[4];
        unsigned u32;
    } m;
    char *p;

    m.u32 = 0;

    i = 0;
    p = from;
    while (*p && i < 4) {
        m.b[i] = 0;
        while (*p >= '0' && *p <= '9') {
            m.b[i] = m.b[i] * 10 + (*p - '0');
            p++;
        }
        if (!*p || *p == ':')
            break;
        i++, p++;
    }

    in = m.u32;

    for (i = 0 ; i < numipfilters ; i++)
        if ((in & ipfilters[i].mask) == ipfilters[i].compare)
            return (int)filterban->value;

    return (int)!filterban->value;
}


/*
=================
SV_AddIP_f
=================
*/
void ServerCommand_AddIP_f(void)
{
    int     i;

    if (gi.argc() < 3) {
        gi.cprintf(NULL, PRINT_HIGH, "Usage:  addip <ip-mask>\n");
        return;
    }

    for (i = 0 ; i < numipfilters ; i++)
        if (ipfilters[i].compare == 0xffffffff)
            break;      // free spot
    if (i == numipfilters) {
        if (numipfilters == MAX_IPFILTERS) {
            gi.cprintf(NULL, PRINT_HIGH, "IP filter list is full\n");
            return;
        }
        numipfilters++;
    }

    if (!StringToFilter(gi.argv(2), &ipfilters[i]))
        ipfilters[i].compare = 0xffffffff;
}

/*
=================
SV_RemoveIP_f
=================
*/
void ServerCommand_RemoveIP_f(void)
{
    ipfilter_t  f;
    int         i, j;

    if (gi.argc() < 3) {
        gi.cprintf(NULL, PRINT_HIGH, "Usage:  sv removeip <ip-mask>\n");
        return;
    }

    if (!StringToFilter(gi.argv(2), &f))
        return;

    for (i = 0 ; i < numipfilters ; i++)
        if (ipfilters[i].mask == f.mask
            && ipfilters[i].compare == f.compare) {
            for (j = i + 1 ; j < numipfilters ; j++)
                ipfilters[j - 1] = ipfilters[j];
            numipfilters--;
            gi.cprintf(NULL, PRINT_HIGH, "Removed.\n");
            return;
        }
    gi.cprintf(NULL, PRINT_HIGH, "Didn't find %s.\n", gi.argv(2));
}

/*
=================
SV_ListIP_f
=================
*/
void ServerCommand_ListIP_f(void)
{
    int     i;
    union {
        byte    b[4];
        unsigned u32;
    } b;

    gi.cprintf(NULL, PRINT_HIGH, "Filter list:\n");
    for (i = 0 ; i < numipfilters ; i++) {
        b.u32 = ipfilters[i].compare;
        gi.cprintf(NULL, PRINT_HIGH, "%3i.%3i.%3i.%3i\n", b.b[0], b.b[1], b.b[2], b.b[3]);
    }
}

/*
=================
SV_WriteIP_f
=================
*/
void ServerCommand_WriteIP_f(void)
{
    FILE    *f;
    char    name[MAX_OSPATH];
    size_t  len;
    union {
        byte    b[4];
        unsigned u32;
    } b;
    int     i;
    cvar_t  *cvar_game;

    cvar_game = gi.cvar("game", "", 0);

    if (!*cvar_game->string)
        len = Q_snprintf(name, sizeof(name), "%s/listip.cfg", GAMEVERSION);
    else
        len = Q_snprintf(name, sizeof(name), "%s/listip.cfg", cvar_game->string);

    if (len >= sizeof(name)) {
        gi.cprintf(NULL, PRINT_HIGH, "File name too long\n");
        return;
    }

    gi.cprintf(NULL, PRINT_HIGH, "Writing %s.\n", name);

    f = fopen(name, "wb");
    if (!f) {
        gi.cprintf(NULL, PRINT_HIGH, "Couldn't open %s\n", name);
        return;
    }

    fprintf(f, "set filterban %d\n", (int)filterban->value);

    for (i = 0 ; i < numipfilters ; i++) {
        b.u32 = ipfilters[i].compare;
        fprintf(f, "sv addip %i.%i.%i.%i\n", b.b[0], b.b[1], b.b[2], b.b[3]);
    }

    fclose(f);
}

/**
*   @brief  SVG_ServerCommand will be called when an "sv" command is issued.
*           The game can issue gi.argc() / gi.argv() commands to get the rest
*           of the parameters
**/
void SVG_ServerCommand(void) {
    char    *cmd;

    cmd = gi.argv(1);
    if ( Q_stricmp( cmd, "test" ) == 0 )
        ServerCommand_Test_f();
    else if ( Q_stricmp( cmd, "addip" ) == 0 )
        ServerCommand_AddIP_f();
    else if ( Q_stricmp( cmd, "removeip" ) == 0 )
        ServerCommand_RemoveIP_f();
    else if ( Q_stricmp( cmd, "listip" ) == 0 )
        ServerCommand_ListIP_f();
    else if ( Q_stricmp( cmd, "writeip" ) == 0 )
        ServerCommand_WriteIP_f();
    else
        gi.cprintf( NULL, PRINT_HIGH, "Unknown server command \"%s\"\n", cmd );
}

