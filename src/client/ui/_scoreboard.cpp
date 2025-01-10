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

//! Stores the actual per frame entries of the clients' statuses.
static client_list_entry_t client_entries[MAX_CLIENTS] = { };


/**
*
* 
*   Scoreboard:
* 
* 
**/
//! Extra space needed for allocation per entry.
static const int32_t CLIENT_ENTRY_EXTRASIZE = q_offsetof( client_list_entry_t, name );

//! Display column IDs.
static constexpr int32_t COL_NAME   = 0;
static constexpr int32_t COL_TIME   = 1;
static constexpr int32_t COL_SCORE  = 2;
static constexpr int32_t COL_PING   = 3;
static constexpr int32_t COL_MAX    = 4;

//! Scoreboard data structure.
typedef struct m_scoreboard_s {
    //! Main scoreboard menu framework.
    menuFrameWork_t menu;
    //! Lists for: Name, Time, Score and Ping.
    menuList_t list;

    //! Number of clients.
    int32_t numClients;

    //! For when re-entering the menu, auto select last selected item.
    int32_t selection;
    int32_t sortcol;
    int32_t sortdir;

    //! Stores the last action executed on the client list. (This can be muting, or..)
    char lastActionStatus[128];

    //! For determining hide/show.
    uint64_t lastFrame;
} m_scoreboard_t;

//! Static m_scoreboard.
static m_scoreboard_t m_scoreboard;

// We need these up here.
static void FreeList( void );
static void BuildClientList( const uint8_t totalClients );

/**
*   @brief  Clear the scoreboard entries for the current frame.
**/
void CL_Scoreboard_ClearFrame( void ) {
    // Clear entries.
    memset( client_entries, 0, sizeof( client_list_entry_t ) * MAX_CLIENTS );

    m_scoreboard.sortcol = m_scoreboard.list.sortcol;
    m_scoreboard.sortdir = m_scoreboard.list.sortdir;
}

/**
*   @brief  Add a client scoreboard entry for the current frame.
**/
void CL_Scoreboard_AddEntry( const int64_t clientNumber, const int64_t clientTime, const int64_t clientScore, const int64_t clientPing ) {
    client_entries[ clientNumber ] = {
        .clientTime = (uint32_t)clientTime,
        .clientScore = (int32_t)clientScore,
        .clientPing = (int16_t)clientPing,
        .clientNumber = (uint8_t)clientNumber,
        .isValidSlot = true
    };

    Q_strlcpy( client_entries[ clientNumber ].clientName, cl.clientinfo[ clientNumber ].name, 32 );
}

/**
*   @brief  Rebuild the client list menu for the current frame.
**/
void CL_Scoreboard_RebuildFrame( const uint8_t totalClients ) {
    // Free previous list.
    FreeList();

    // Rebuild(re-allocate) the client list items.
    BuildClientList( totalClients );

    // Sort col mofuckahs
    m_scoreboard.list.sortcol = m_scoreboard.sortcol;
    m_scoreboard.list.sortdir = m_scoreboard.sortdir;

    // Sort the client list items.
    m_scoreboard.list.sort( &m_scoreboard.list );

    // Ensure all is properly sized.
    m_scoreboard.menu.size( &m_scoreboard.menu );
}

/**
*   @brief  Determines whether to start showing or hiding the scoreboard.
*/
void CL_Scoreboard_CheckVisibility() {
    // First determine whether the menu is already active or not.
    menuFrameWork_t *scoreboardMenu = UI_FindMenu( "scoreboard" );

    #if 0
    // Show if the lastFrame is equals(or impossibly, higher than current frame.)
    if ( ( cl.frame.ps.stats[ STAT_SHOW_SCORES ] & 1 ) ) {
        if ( uis.activeMenu != scoreboardMenu ) {
            UI_PushMenu( scoreboardMenu );
        }
    // Did not receive any svc_scoreboard this frame, so let's assume we gonna hide it.
    } else if ( !( cl.frame.ps.stats[ STAT_SHOW_SCORES ] & 1 ) ) {
        // Make sure it is the active scoreboard menu that we're going to hide.
        if ( uis.activeMenu == scoreboardMenu ) {
            UI_PopMenu();
            UI_ForceMenuOff();
        }
    }
    #endif
}

/**
*
*
*   Client List:
*
*
**/
/**
*   @brief  Will toggle ignore/unignore for client name.
*   @return True if the client is ignored.
**/
qboolean CL_CheckForIgnore( const char *s );
static qboolean ToggleClientIgnore( const char *clientName ) {
    if ( CL_CheckForIgnore( clientName ) ) {
        char command_str[ 1024 ];
        Q_scnprintf( command_str, 1024, "%s \"%s\"\n", "unignorenick", clientName );
        Cmd_ExecuteString( cmd_current, command_str );
        
        // Update last action notification message.
        Q_scnprintf( m_scoreboard.lastActionStatus, sizeof( m_scoreboard.lastActionStatus ), "Unignored player \"%s\"\n", clientName );
        
        // Print notify it as an action.
        //Com_LPrintf( PRINT_NOTICE, "%s", m_scoreboard.lastActionStatus );

        return false;
    } else {
        char command_str[ 1024 ];
        Q_scnprintf( command_str, 1024, "%s \"%s\"\n", "ignorenick", clientName );
        Cmd_ExecuteString( cmd_current, command_str );
                
        Q_scnprintf( m_scoreboard.lastActionStatus, sizeof( m_scoreboard.lastActionStatus ), "Ignored player \"%s\"\n", clientName );
        // Print notify it as an action.
        //Com_LPrintf( PRINT_NOTICE, "%s", m_scoreboard.lastActionStatus );

        return true;
    }

    // Shouldn't happen.
    return false;
}

/**
*   @brief
**/
static void BuildClientList( const uint8_t numClients ) {
    // Allocate scoreboard entries.
    m_scoreboard.numClients = numClients;
    m_scoreboard.list.items = static_cast<void **>( UI_Malloc( sizeof( client_list_entry_t * ) * ( numClients + 1 ) ) ); // WID: C++20: Added cast.
    m_scoreboard.list.numItems = 0;
    m_scoreboard.list.curvalue = m_scoreboard.selection;
    m_scoreboard.list.prestep = 0;

    //
    // Build List.
    //
    for ( int32_t i = 0; i < numClients; i++ ) {
        client_list_entry_t *client = &client_entries[ i ];
        if ( client && client->isValidSlot ) {
            client_list_entry_t *e = static_cast<client_list_entry_t *>( UI_FormatColumns( CLIENT_ENTRY_EXTRASIZE,
                va( "%s", client->clientName ),
                va( "%i", client->clientTime ),
                va( "%i", client->clientScore ),
                va( "%i", client->clientPing ),
                NULL ) ); // WID: C++20: Added cast.

            Q_strlcpy( e->clientName, client->clientName, 32 );

            m_scoreboard.list.items[ m_scoreboard.list.numItems++ ] = e;
        }
    }

    //// Set ascending sort order.
    //m_scoreboard.list.sortdir = 1;
    ////m_scoreboard.list.sortcol = COL_SCORE;
    //if ( m_scoreboard.list.items && m_scoreboard.list.sortdir != 0 ) {
    //    m_scoreboard.list.sort( &m_scoreboard.list );
    //}

    //// Update column sizes.
    //m_scoreboard.menu.size( &m_scoreboard.menu );
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
        m_scoreboard.menu.status = "No clients to display!";
        return QMS_BEEP;
    }

    // Need a valid current value.
    if ( m_scoreboard.list.curvalue < 0 || m_scoreboard.list.curvalue >= m_scoreboard.list.numItems ) {
        return QMS_SILENT;
    }

    //e = m_scoreboard.list.items[m_scoreboard.list.curvalue];
    e = static_cast<client_list_entry_t*>( m_scoreboard.list.items[ m_scoreboard.list.curvalue ] ); // WID: C++20: Was without cast.

    // The client is a valid pointer, and has a name, determine its ignore state and fill the 
    // status buffer with a string based on that.
    if ( e && e->clientName ) {
        // Update selection.
        m_scoreboard.selection = m_scoreboard.list.curvalue;

        // Determine whether it is an ignored player or not for correct status buffer text.
        const bool isIgnored = CL_CheckForIgnore( e->clientName );

        // Clear list status buffer.
        memset( listStatusBuffer, 0, sizeof( listStatusBuffer ) );
        // Insert appropriate text:
        if ( !isIgnored ) {
            Q_scnprintf( listStatusBuffer, sizeof( listStatusBuffer ), "Press 'Enter' to ignore/mute player(%s).", e->clientName );
        } else {
            Q_scnprintf( listStatusBuffer, sizeof( listStatusBuffer ), "Press 'Enter' to unignore/unmute player(%s).", e->clientName );
        }
        // Assign to menu.
        m_scoreboard.menu.status = listStatusBuffer;
    // In case for whichever reason selection has failed, so we got no client list entry to work with:
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
        // Update the actual list status buffer 
        bool isIgnored = ( ToggleClientIgnore( e->clientName ) );

        // Clear list status buffer.
        memset( listStatusBuffer, 0, sizeof( listStatusBuffer ) );
        // Update with what the action would be right now.
        Q_scnprintf( listStatusBuffer, sizeof( listStatusBuffer ), "Press 'Enter' to %s player(%s).", 
            ( isIgnored ? "unignore/unmute" : "ignore/mute" ),
            e->clientName );

        // Assign updated status to menu.
        m_scoreboard.menu.status = listStatusBuffer;
        // Return tha beeeeep.
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

    // ignore autorepeats
    if ( Key_IsDown( key ) > 1 ) {
        return QMS_NOTHANDLED;
    }

//    if ( uis.activeMenu == self ) {
    if ( key == K_TAB || key == K_ESCAPE || key == K_BACKSPACE ) {
        UI_ForceMenuOff();
        return QMS_SILENT;
    }
//    } else {
        //Cmd_ExecuteString( cmd_current, "score" );
        //CL_ForwardToServer();
    //    Menu_Pop( self );
    //    UI_ForceMenuOff();
    //    //return QMS_OUT;
    //}

    return QMS_NOTHANDLED;
}

/**
*   @brief
**/
static void Draw( menuFrameWork_t *self ) {
    Menu_Draw( self );
    //if ( uis.width >= 640 ) {
        //UI_DrawString( uis.width, uis.height - CHAR_HEIGHT,
        //    UI_RIGHT, m_scoreboard.lastActionStatus );
    
    // Draw the last action performed status.
    UI_DrawString( CHAR_WIDTH, CHAR_HEIGHT,
            UI_LEFT, m_scoreboard.lastActionStatus );


    // TEMP
    char fpsBuffer[ 1024 ];
    Q_snprintf( fpsBuffer, 1024, "FPS: %i\n", CL_GetRefreshFps() );
    UI_DrawString( uis.width - CHAR_WIDTH, CHAR_HEIGHT,
        UI_RIGHT, fpsBuffer );
    //}///


    // Determine whether to show UI scoreboard or not.
    //CL_Scoreboard_CheckVisibility();
}

/**
*   @brief
**/
static void Size( menuFrameWork_t *self ) {
    // Get the center points of our uis screen.
    int32_t screenCenterX = ( uis.width / 2 );
    int32_t screenCenterY = ( uis.height / 2 );
    

    // Calculate the exact width to be for the client display list item.
    int32_t _w = 32 * CHAR_WIDTH + MLIST_PADDING; // For the name column.
    _w += ( 4 * CHAR_WIDTH + MLIST_PADDING ) * 3; // For the numeric remaining columns, time, score, ping.
    _w += MLIST_SCROLLBAR_WIDTH; // For the scrollbar.

    m_scoreboard.list.generic.x = screenCenterX - (_w / 2); // Center the list to screen on X-Axis.
    m_scoreboard.list.generic.y = screenCenterY - (SCOREBOARD_HALF_HEIGHT / 2);//( uis.height / 2 ) - 240;//CHAR_HEIGHT;
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

    m_scoreboard.menu.y1 = screenCenterY - ( SCOREBOARD_HALF_HEIGHT );
    m_scoreboard.menu.y2 = screenCenterY + ( SCOREBOARD_HALF_HEIGHT );
}

/**
*   @brief
**/
static void Expose( menuFrameWork_t *self ) {
    // Rebuild the client list.
    //BuildClientList( 0 );
    // 
    // Make sure that the server is starting to send score svc_scoreboard commands from here on.
    CL_ClientCommand( "score" );
}

/**
*   @brief
**/
static void Pop( menuFrameWork_t *self ) {
    // Make sure that the server is undoing score svc_scoreboard commands from here on.
    CL_ClientCommand( "score" );

    // Save current selection as last selection.
    m_scoreboard.selection = m_scoreboard.list.curvalue;

    // Free the client list.
    FreeList();

    // Clear last action status buffer.
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
    m_scoreboard.list.mlFlags = MLF_HEADER | MLF_SCROLLBAR/* | MLF_COLOR*/;// | MLF_SCROLLBAR;

    m_scoreboard.list.columns[ 0 ].name = "Name";
    m_scoreboard.list.columns[ 0 ].uiFlags = UI_LEFT;
    m_scoreboard.list.columns[ 1 ].name = "Time";
    m_scoreboard.list.columns[ 1 ].uiFlags = UI_CENTER;
    m_scoreboard.list.columns[ 2 ].name = "Score";
    m_scoreboard.list.columns[ 2 ].uiFlags = UI_CENTER;
    m_scoreboard.list.columns[ 3 ].name = "Ping";
    m_scoreboard.list.columns[ 3 ].uiFlags = UI_CENTER;

    // Build client list item
    BuildClientList( 0 );

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

    //int n1 = atoi( UI_GetColumn( e1->name, COL_SCORE ) );
    //int n2 = atoi( UI_GetColumn( e2->name, COL_SCORE ) );

    //if ( n1 == 0 && n2 == 0 ) {
    //    return 1 * -m_scoreboard.list.sortdir;
    //}

    //return ( n1 - n2 ) * -m_scoreboard.list.sortdir;
    if ( e1->clientScore > e2->clientScore ) {
        return m_scoreboard.list.sortdir;
    }
    if ( e1->clientScore < e2->clientScore ) {
        return -m_scoreboard.list.sortdir;
    }
    return 0;
}
//!
static int pingcmp( const void *p1, const void *p2 ) {
    client_list_entry_t *e1 = *(client_list_entry_t **)p1;
    client_list_entry_t *e2 = *(client_list_entry_t **)p2;

    //int n1 = atoi( UI_GetColumn( e1->name, COL_PING ) );
    //int n2 = atoi( UI_GetColumn( e2->name, COL_PING ) );
    //
    //if ( n1 == 0 && n2 == 0 ) {
    //    return 1 * -m_scoreboard.list.sortdir;
    //}

    //return ( n1 - n2 ) * -m_scoreboard.list.sortdir;
    if ( e1->clientPing > e2->clientPing ) {
        return m_scoreboard.list.sortdir;
    }
    if ( e1->clientPing < e2->clientPing ) {
        return -m_scoreboard.list.sortdir;
    }
    return 0;
}
//!
static int timecmp( const void *p1, const void *p2 ) {
    client_list_entry_t *e1 = *(client_list_entry_t **)p1;
    client_list_entry_t *e2 = *(client_list_entry_t **)p2;

    //int n1 = atoi( UI_GetColumn( e1->name, COL_TIME ) );
    //int n2 = atoi( UI_GetColumn( e2->name, COL_TIME ) );

    //return ( n1 - n2 ) * -m_scoreboard.list.sortdir;
    if ( e1->clientTime > e2->clientTime ) {
        return m_scoreboard.list.sortdir;
    }
    if ( e1->clientTime < e2->clientTime ) {
        return -m_scoreboard.list.sortdir;
    }
    return 0;
}
//!
static int namecmp( const void *p1, const void *p2 ) {
    client_list_entry_t *e1 = *(client_list_entry_t **)p1;
    client_list_entry_t *e2 = *(client_list_entry_t **)p2;
    char *s1 = UI_GetColumn( e1->name, m_scoreboard.list.sortcol );
    char *s2 = UI_GetColumn( e2->name, m_scoreboard.list.sortcol );

    return Q_stricmp( s1, s2 ) * m_scoreboard.list.sortdir;
}

/**
*   @brief
**/
static menuSound_t Sort( menuList_t *self ) {
    switch ( m_scoreboard.list.sortcol ) {
    //case COL_NAME:
    //    MenuList_Sort( &m_scoreboard.list, 0, scorecmp );
    //    break;
    //case COL_TIME:
    //    MenuList_Sort( &m_scoreboard.list, 0, scorecmp );
    //    break;
    case COL_SCORE:
        MenuList_Sort( &m_scoreboard.list, 0, scorecmp );
        break;
    //case COL_PING:
    //    MenuList_Sort( &m_scoreboard.list, 0, scorecmp );
    //    break;
    }

    return QMS_SILENT;
}