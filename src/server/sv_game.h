/*********************************************************************
*
*
*	Server: SVGame.
*
*
********************************************************************/
#pragma once



/**
*	@brief
**/
void SV_InitGameProgs( void );
/**
*	@brief
**/
void SV_ShutdownGameProgs( void );

/**
*	@brief	Requests that game program shutdown be deferred until the end of the current frame.
**/
void SV_RequestDeferredGameProgsShutdown( void );
/**
*	@brief	Processes any pending deferred game program shutdown requests. Should be called at the end of each server frame.
**/
void SV_ProcessDeferredGameProgsShutdown( void );
/**
*	@brief	Returns true if a deferred game program shutdown has been requested and is pending processing.
**/
const bool SV_IsGameProgsShutdownDeferred( void );