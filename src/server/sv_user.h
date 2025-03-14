/*********************************************************************
*
*
*	Server: User.
*
*
********************************************************************/
#pragma once


void SV_New_f( void );
void SV_Begin_f( void );
void SV_ExecuteClientMessage( client_t *cl );
void SV_CloseDownload( client_t *client );
cvarban_t *SV_CheckInfoBans( const char *info, bool match_only );