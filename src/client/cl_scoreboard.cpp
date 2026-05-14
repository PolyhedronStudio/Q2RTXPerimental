/********************************************************************
*
*
*	Client: Scoreboard Data Management
*
*	This module provides scoreboard entry management functions that are
*	called from the client parse and client game modules. These functions
*	handle storing and retrieving scoreboard data received from the server.
*
*
********************************************************************/
#include "cl_client.h"
#include "shared/client/scoreboard.h"
#include <cstring>

//! Global scoreboard entries array, one entry per possible client.
static scoreboard_entry_t client_entries[MAX_CLIENTS] = {};

/**
*	@brief	Clear all scoreboard entries for the current frame.
*	@note	Called at the start of a new svc_scoreboard message from the server.
**/
void CL_Scoreboard_ClearFrame( void ) {
	// Clear all entries.
	memset( client_entries, 0, sizeof( scoreboard_entry_t ) * MAX_CLIENTS );
}

/**
*	@brief	Add a client entry to the scoreboard for the current frame.
*	@param	clientNumber	Client index (0-63).
*	@param	clientTeamId	Team identifier sent by the server.
*	@param	clientTime		Time the client has been in the game.
*	@param	clientScore		Client's current score.
*	@param	clientPing		Client's ping in milliseconds.
*	@note	Called once per client in a svc_scoreboard message.
**/
void CL_Scoreboard_AddEntry( const int64_t clientNumber, const int64_t clientTeamId, const int64_t clientTime, const int64_t clientScore, const int64_t clientPing ) {
	// Validate client index.
	if ( clientNumber < 0 || clientNumber >= MAX_CLIENTS ) {
		return;
	}

	// Store the entry data.
	client_entries[clientNumber].clientNumber = (int32_t)clientNumber;
	client_entries[clientNumber].clientTeamId = (int32_t)clientTeamId;
	client_entries[clientNumber].clientTime = (int32_t)clientTime;
	client_entries[clientNumber].clientScore = (int32_t)clientScore;
	client_entries[clientNumber].clientPing = (int32_t)clientPing;
	client_entries[clientNumber].isValidSlot = true;

	// Copy the client name from the clientinfo.
	if ( clientNumber >= 0 && clientNumber < MAX_CLIENTS && cl.clientinfo[clientNumber].name ) {
		Q_strlcpy( client_entries[clientNumber].clientName, cl.clientinfo[clientNumber].name, sizeof( client_entries[clientNumber].clientName ) );
	}
}

/**
*	@brief	Rebuild the scoreboard display after adding all entries for the frame.
*	@param	totalClients	Total number of active clients in this frame.
*	@note	Called after all entries have been added via CL_Scoreboard_AddEntry.
**/
void CL_Scoreboard_RebuildFrame( const uint8_t totalClients ) {
	// Currently just a placeholder for future UI rebuilding if needed.
	// The clgame module will handle UI updates via CLG_UI_OpenMenu.
}

/**
*	@brief	Get the array of scoreboard entries.
*	@return	Pointer to the scoreboard entries array, or nullptr if unavailable.
*	@note	Used by clgame to populate the scoreboard UI.
**/
const scoreboard_entry_t *CL_GetScoreboardEntries( void ) {
	return client_entries;
}

/**
*	@brief	Get the number of active scoreboard clients.
*	@return	Number of currently active clients in the scoreboard.
*	@note	Used by clgame to determine scoreboard display size.
**/
int CL_GetScoreboardClientCount( void ) {
	int count = 0;
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( client_entries[i].isValidSlot ) {
			count++;
		}
	}
	return count;
}
