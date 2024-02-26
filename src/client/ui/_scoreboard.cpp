/*
Copyright (C) 2003-2008 Andrey Nazarov

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "ui.h"
#include "../cl_client.h"
#include "common/files.h"
#include "common/mdfour.h"

/*
=======================================================================

DEMOS MENU

=======================================================================
*/

static menuSound_t Sort( menuList_t *self );
static void Free( menuFrameWork_t *self );
static menuSound_t Sort( menuList_t *self );

static char listStatusBuffer[ 1024 ];
static menuSound_t Change( menuCommon_t *self );


// This is dynamically generated instead.
//static constexpr int32_t SCOREBOARD_WIDTH = 640;
//static constexpr int32_t SCOREBOARD_HALF_WIDTH = ( SCOREBOARD_WIDTH / 2 );
static constexpr int32_t SCOREBOARD_HEIGHT = 240;
static constexpr int32_t SCOREBOARD_HALF_HEIGHT = ( SCOREBOARD_HEIGHT / 2 );

/**
*   @brief  
**/
typedef struct client_list_entry_s {
    char clientName[32] = "";
    uint32_t clientTime = 0;
    int32_t clientScore = 0;
    int16_t clientPing = 0;
    uint8_t clientNumber = 0;
    qboolean isValidSlot = true;
    char    name[ 1 ];
} client_list_entry_t;

/**
*   @brief  
**/
static client_list_entry_t client_entries[] = {
    {
        .clientName = "Client #3\0",
        .clientTime = 24,
        .clientScore = 16,
        .clientPing = 120,
        .clientNumber = 1,
    },
    {
        .clientName = "Client #2\0",
        .clientTime = 44,
        .clientScore = 22,
        .clientPing = 15,
        .clientNumber = 1,
    },
    {
        .clientName = "Client #1\0",
        .clientTime = 69,
        .clientScore = 33,
        .clientPing = 25,
        .clientNumber = 2,
    },
    {
        .clientName = "Client #8\0",
        .clientTime = 80,
        .clientScore = 13,
        .clientPing = 27,
        .clientNumber = 3,
    },
    {
        .clientName = "Client #7\0",
        .clientTime = 5,
        .clientScore = 2,
        .clientPing = 89,
        .clientNumber = 4,
    },
    {
        .clientName = "Client #6\0",
        .clientTime = 16,
        .clientScore = 14,
        .clientPing = 244,
        .clientNumber = 5,
    },
    {
        .clientName = "Client #5\0",
        .clientTime = 120,
        .clientScore = 55,
        .clientPing = 29,
        .clientNumber = 6,
    },
    {
        .clientName = "Client #4\0",
        .clientTime = 90,
        .clientScore = 47,
        .clientPing = 36,
        .clientNumber = 7,
    },
};



/**
*
* 
*   Scoreboard:
* 
* 
**/
#define CLIENT_ENTRY_EXTRASIZE  q_offsetof(client_list_entry_t, name)

#define COL_NAME    0
#define COL_TIME    1
#define COL_SCORE   2
#define COL_PING    3
#define COL_MAX     4


typedef struct m_scoreboard_s {
    //! Main scoreboard menu framework.
    menuFrameWork_t menu;
    //! Lists for: Name, Time, Score and Ping.
    menuList_t      list;

    //! Number of clients.
    int             numClients;
    //! For when re-entering the menu, auto select last selected item.
    int             selection;

    //! Stores the last action executed on the client list. (This can be muting, or..)
    char            lastActionStatus[128];
} m_scoreboard_t;

static m_scoreboard_t    m_scoreboard;


/**
*
*
*   Client List:
*
*
**/
/**
*   @brief  Will toggle ignore/unignore for client name.
**/
extern "C" {
    qboolean CL_CheckForIgnore( const char *s );
}
static void ToggleClientIgnore( const char *clientName ) {
    if ( CL_CheckForIgnore( clientName ) ) {
        char command_str[ 1024 ];
        Q_scnprintf( command_str, 1024, "%s \"%s\"\n", "unignorenick", clientName );
        Cmd_ExecuteString( cmd_current, command_str );
        
        
        Q_scnprintf( m_scoreboard.lastActionStatus, sizeof( m_scoreboard.lastActionStatus ), "Unignored player \"%s\"\n", clientName );
        Com_LPrintf( PRINT_NOTICE, "%s", m_scoreboard.lastActionStatus );
    } else {
        char command_str[ 1024 ];
        Q_scnprintf( command_str, 1024, "%s \"%s\"\n", "ignorenick", clientName );
        Cmd_ExecuteString( cmd_current, command_str );
                
        Q_scnprintf( m_scoreboard.lastActionStatus, sizeof( m_scoreboard.lastActionStatus ), "Ignored player \"%s\"\n", clientName );
        Com_LPrintf( PRINT_NOTICE, "%s", m_scoreboard.lastActionStatus );
    }
}

/**
*   @brief
**/
static void BuildClientList( void ) {
    // Update screen.
    SCR_UpdateScreen();

    // Acquire number of client information slots.
    int32_t numClients = q_countof( client_entries );

    // Allocate scoreboard entries.
    m_scoreboard.numClients = numClients;
    m_scoreboard.list.items = static_cast<void **>( UI_Malloc( sizeof( client_list_entry_t * ) * ( numClients + 1 ) ) ); // WID: C++20: Added cast.
    m_scoreboard.list.numItems = 0;
    m_scoreboard.list.curvalue = 0;
    m_scoreboard.list.prestep = 0;

    //
    // Build List.
    //
    for ( int32_t i = 0; i < q_countof( client_entries ); i++ ) {
        client_list_entry_t *client = &client_entries[ i ];

        client_list_entry_t *e = static_cast<client_list_entry_t *>( UI_FormatColumns( CLIENT_ENTRY_EXTRASIZE,
            va( "%s", client->clientName ),
            va( "%i", client->clientTime ),
            va( "%i", client->clientScore ),
            va( "%i", client->clientPing ),
            NULL ) ); // WID: C++20: Added cast.

        Q_strlcpy( e->clientName, client->clientName, 32 );

        m_scoreboard.list.items[ m_scoreboard.list.numItems++ ] = e;
    }

    // Update status line and sort
    Change( &m_scoreboard.list.generic );

    // Set ascending sort order.
    m_scoreboard.list.sortdir = 1;
    m_scoreboard.list.sortcol = COL_SCORE;
    if ( m_scoreboard.list.items && m_scoreboard.list.sortdir != 0 ) {
        m_scoreboard.list.sort( &m_scoreboard.list );
    }

    // Update column sizes.
    m_scoreboard.menu.size( &m_scoreboard.menu );

    // Update screen.
    SCR_UpdateScreen();
}

/**
*   @brief
**/
static void FreeList( void ) {
    int i;

    if ( m_scoreboard.list.items ) {
        for ( i = 0; i < m_scoreboard.list.numItems; i++ ) {
            Z_Free( m_scoreboard.list.items[ i ] );
        }
        Z_Freep( (void **)&m_scoreboard.list.items );
        m_scoreboard.list.numItems = 0;
    }
}



/**
*
* 
*   UI:
* 
* 
**/
/**
*   @brief
**/
static menuSound_t Change( menuCommon_t *self ) {
    client_list_entry_t *e;

    if ( !m_scoreboard.list.numItems ) {
        m_scoreboard.menu.status = "No active clients found";
        return QMS_BEEP;
    }

    // Need a valid current value.
    if ( m_scoreboard.list.curvalue < 0 || m_scoreboard.list.curvalue >= m_scoreboard.list.numItems ) {
        return QMS_SILENT;
    }

    //e = m_scoreboard.list.items[m_scoreboard.list.curvalue];
    e = static_cast<client_list_entry_t*>( m_scoreboard.list.items[ m_scoreboard.list.curvalue ] ); // WID: C++20: Was without cast.

    if ( e && e->clientName ) {
        const qboolean isIgnored = CL_CheckForIgnore( e->clientName );

        // Clear list status buffer.
        memset( listStatusBuffer, 0, sizeof( listStatusBuffer ) );
        // Insert appropriate text:
        if ( isIgnored ) {
            Q_scnprintf( listStatusBuffer, sizeof( listStatusBuffer ), "Press 'Enter' to ignore/mute player(%s).", e->clientName );
        } else {
            Q_scnprintf( listStatusBuffer, sizeof( listStatusBuffer ), "Press 'Enter' to unignore/unmute player(%s).", e->clientName );
        }
        // Assign to menu.
        m_scoreboard.menu.status = listStatusBuffer;
    } else {
        m_scoreboard.menu.status = "Select a player to ignore/mute.";
    }

    return QMS_SILENT;
}
/**
*   @brief
**/
static menuSound_t Activate( menuCommon_t *self ) {
    client_list_entry_t *e;

    if ( !m_scoreboard.list.numItems ) {
        return QMS_BEEP;
    }

    e = static_cast<client_list_entry_t *>( static_cast<m_scoreboard_t>( m_scoreboard ).list.items[ m_scoreboard.list.curvalue ] ); // WID: C++20: Was without cast.
    if ( e->isValidSlot ) {
        ToggleClientIgnore( e->clientName );
        return QMS_BEEP;
    }
    //if ( e->clientName )
    //switch ( e->type ) {
    //case ENTRY_UP:
    //    return LeaveDirectory();
    //case ENTRY_DN:
    //    return EnterDirectory( e );
    //case ENTRY_DEMO:
    //    return PlayDemo( e );
    //}

    return QMS_NOTHANDLED;
}

/**
*   @brief
**/
static menuSound_t Keydown( menuFrameWork_t *self, int key ) {
    if ( key == K_TAB ) {
        //Menu_Pop( self );
        UI_ForceMenuOff();
        return QMS_OUT;
    }

    return QMS_NOTHANDLED;
}

/**
*   @brief
**/
static void Draw( menuFrameWork_t *self ) {
    Menu_Draw( self );
    if ( uis.width >= 640 ) {
        //UI_DrawString( uis.width, uis.height - CHAR_HEIGHT,
        //    UI_RIGHT, m_scoreboard.lastActionStatus );
        UI_DrawString( CHAR_WIDTH, CHAR_HEIGHT,
            UI_LEFT, m_scoreboard.lastActionStatus );
    }
}

/**
*   @brief
**/
static void Size( menuFrameWork_t *self ) {
    int32_t _x = ( uis.width / 2 );// -SCOREBOARD_HALF_WIDTH;
    int32_t _y = ( uis.height / 2 );// -SCOREBOARD_HALF_HEIGHT;
    

    // Generate scoreboard list.
    int32_t _w = 32 * CHAR_WIDTH + MLIST_PADDING;
    _w += ( 4 * CHAR_WIDTH + MLIST_PADDING ) * 3;

    m_scoreboard.list.generic.x = _x - (_w / 2);// _x - SCOREBOARD_HALF_WIDTH;//( uis.width / 2 ) - 320;
    m_scoreboard.list.generic.y = _y - (SCOREBOARD_HALF_HEIGHT / 2);//( uis.height / 2 ) - 240;//CHAR_HEIGHT;
    m_scoreboard.list.generic.height = SCOREBOARD_HEIGHT / 2;// uis.height - CHAR_HEIGHT * 2 - 1;
    m_scoreboard.list.generic.width = _w;
    
    m_scoreboard.list.generic.rect.x = m_scoreboard.list.generic.x;
    m_scoreboard.list.generic.rect.y = m_scoreboard.list.generic.y;
    m_scoreboard.list.generic.rect.width = m_scoreboard.list.generic.width;
    m_scoreboard.list.generic.rect.height= m_scoreboard.list.generic.height;

    m_scoreboard.list.columns[ 0 ].width = 32 * CHAR_WIDTH + MLIST_PADDING;
    m_scoreboard.list.columns[ 1 ].width = 4 * CHAR_WIDTH + MLIST_PADDING;
    m_scoreboard.list.columns[ 2 ].width = 4 * CHAR_WIDTH + MLIST_PADDING;//m_scoreboard.widest_score * CHAR_WIDTH + MLIST_PADDING;
    m_scoreboard.list.columns[ 3 ].width = 4 * CHAR_WIDTH + MLIST_PADDING;//m_scoreboard.widest_name * CHAR_WIDTH + MLIST_PADDING;

    m_scoreboard.menu.y1 = 240;
    m_scoreboard.menu.y2 = uis.height - 240;
}

/**
*   @brief
**/
static void Expose( menuFrameWork_t *self ) {
    // Rebuild the client list.
    BuildClientList();

    // Sort the client list items.
    m_scoreboard.list.sort( &m_scoreboard.list );

    // Ensure all is properly sized.
    Size( &m_scoreboard.menu );

    // Move cursor to last time's position.
    MenuList_SetValue( &m_scoreboard.list, m_scoreboard.selection );
}

/**
*   @brief
**/
static void Pop( menuFrameWork_t *self ) {
    // Save current selection as last selection.
    m_scoreboard.selection = m_scoreboard.list.curvalue;

    // Free the client list.
    FreeList();

    memset( m_scoreboard.lastActionStatus, 0, sizeof( m_scoreboard.lastActionStatus ) );
}

/**
*   @brief
**/
void M_Menu_Scoreboard( void ) {
    m_scoreboard.menu.name = "scoreboard";
    m_scoreboard.menu.title = "Scoreboard:";

    m_scoreboard.menu.draw = Draw;
    m_scoreboard.menu.expose = Expose;
    m_scoreboard.menu.pop = Pop;
    m_scoreboard.menu.size = Size;
    m_scoreboard.menu.keydown = Keydown;
    m_scoreboard.menu.free = Free;
    m_scoreboard.menu.image = 0;// uis.backgroundHandle;
    m_scoreboard.menu.color.u32 = MakeColor( 77, 191, 191, 0 ); //uis.color.background.u32;
    m_scoreboard.menu.transparent = true; // uis.transparent;

    m_scoreboard.list.generic.type = MTYPE_LIST;
    m_scoreboard.list.generic.flags = QMF_HASFOCUS;
    m_scoreboard.list.generic.activate = Activate;
    m_scoreboard.list.generic.change = Change;
    //m_scoreboard.list.generic.parent = &m_scoreboard.menu;
    m_scoreboard.list.numcolumns = COL_MAX;
    m_scoreboard.list.sortdir = 1;
    m_scoreboard.list.sortcol = COL_SCORE;
    m_scoreboard.list.extrasize = CLIENT_ENTRY_EXTRASIZE;
    m_scoreboard.list.sort = Sort;
    m_scoreboard.list.mlFlags = MLF_HEADER/* | MLF_COLOR*/;// | MLF_SCROLLBAR;

    m_scoreboard.list.columns[ 0 ].name = "Name";
    m_scoreboard.list.columns[ 0 ].uiFlags = UI_LEFT;
    m_scoreboard.list.columns[ 1 ].name = "Time";
    m_scoreboard.list.columns[ 1 ].uiFlags = UI_CENTER;
    m_scoreboard.list.columns[ 2 ].name = "Score";
    m_scoreboard.list.columns[ 2 ].uiFlags = UI_CENTER;
    m_scoreboard.list.columns[ 3 ].name = "Ping";
    m_scoreboard.list.columns[ 3 ].uiFlags = UI_CENTER;

    // Build client list item
    BuildClientList();
    // Sort the client list.
    m_scoreboard.list.sort( &m_scoreboard.list );
    Size( &m_scoreboard.menu );

    // Add list as a child menu item.
    Menu_AddItem( &m_scoreboard.menu, &m_scoreboard.list );

    // Append menu into the menus list.
    List_Append( &ui_menus, &m_scoreboard.menu.entry );
}

static void Free( menuFrameWork_t *self ) {
    Z_Free( m_scoreboard.menu.items );
    memset( &m_scoreboard, 0, sizeof( m_scoreboard ) );
}



/**
*
*   Sorting:
*
*
**/
//!
static int scorecmp( const void *p1, const void *p2 ) {
    client_list_entry_t *e1 = *(client_list_entry_t **)p1;
    client_list_entry_t *e2 = *(client_list_entry_t **)p2;

    int n1 = atoi( UI_GetColumn( e1->name, COL_SCORE ) );
    int n2 = atoi( UI_GetColumn( e2->name, COL_SCORE ) );

    return ( n1 - n2 ) * -m_scoreboard.list.sortdir;
}
//!
static int pingcmp( const void *p1, const void *p2 ) {
    client_list_entry_t *e1 = *(client_list_entry_t **)p1;
    client_list_entry_t *e2 = *(client_list_entry_t **)p2;

    int n1 = atoi( UI_GetColumn( e1->name, COL_PING ) );
    int n2 = atoi( UI_GetColumn( e2->name, COL_PING ) );

    return ( n1 - n2 ) * m_scoreboard.list.sortdir;
}
//!
static int timecmp( const void *p1, const void *p2 ) {
    client_list_entry_t *e1 = *(client_list_entry_t **)p1;
    client_list_entry_t *e2 = *(client_list_entry_t **)p2;

    int n1 = atoi( UI_GetColumn( e1->name, COL_TIME ) );
    int n2 = atoi( UI_GetColumn( e2->name, COL_TIME ) );

    return ( n1 - n2 ) * -m_scoreboard.list.sortdir;
}
//!
static int namecmp( const void *p1, const void *p2 ) {
    client_list_entry_t *e1 = *(client_list_entry_t **)p1;
    client_list_entry_t *e2 = *(client_list_entry_t **)p2;
    char *s1 = UI_GetColumn( e1->clientName, m_scoreboard.list.sortcol );
    char *s2 = UI_GetColumn( e2->clientName, m_scoreboard.list.sortcol );

    return Q_stricmp( s1, s2 ) * m_scoreboard.list.sortdir;
}

/**
*   @brief
**/
static menuSound_t Sort( menuList_t *self ) {
    switch ( m_scoreboard.list.sortcol ) {
    case COL_NAME:
        MenuList_Sort( &m_scoreboard.list, q_offsetof( client_list_entry_t, clientName ), namecmp );
        break;
    case COL_TIME:
        MenuList_Sort( &m_scoreboard.list, 0, timecmp );
        break;
    case COL_SCORE:
        MenuList_Sort( &m_scoreboard.list, 0, scorecmp );
        break;
    case COL_PING:
        MenuList_Sort( &m_scoreboard.list, 0, pingcmp );
        break;
    }

    return QMS_SILENT;
}