/********************************************************************
*
*
*	ClientGame: Effects Header
*
*
********************************************************************/
#pragma once



/**
*	@brief	Called by the client when it receives a configstring update, this
*			allows us to interscept it and respond to it. If not interscepted the
*			control is passed back to the client again.
*
*	@return	True if we interscepted one succesfully, false otherwise.
**/
const qboolean PF_UpdateConfigString( const int32_t index );
/**
*	@brief	Called by the client BEFORE all server messages have been parsed.
**/
void PF_StartServerMessage( const qboolean isDemoPlayback );
/**
*	@brief	Called by the client AFTER all server messages have been parsed.
**/
void PF_EndServerMessage( const qboolean isDemoPlayback );
/**
*	@brief	Called by the client when it does not recognize the server message itself,
*			so it gives the client game a chance to handle and respond to it.
*	@return	True if the message was handled properly. False otherwise.
**/
const qboolean PF_ParseServerMessage( const int32_t serverMessage );
/**
*	@brief	A variant of ParseServerMessage that skips over non-important action messages,
*			used for seeking in demos.
*	@return	True if the message was handled properly. False otherwise.
**/
const qboolean PF_SeekDemoMessage( const int32_t serverMessage );
/**
*	@brief	Parsess entity events.
**/
void PF_ParseEntityEvent( const int32_t entityNumber );
/**
*	@brief	Parses a clientinfo configstring.
**/
void PF_ParsePlayerSkin( char *name, char *model, char *skin, const char *s );
