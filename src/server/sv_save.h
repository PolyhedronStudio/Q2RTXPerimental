/*********************************************************************
*
*
*	Server: Save.
*
*
********************************************************************/
#pragma once


/**
*	@brief
**/
void SV_AutoSaveBegin( mapcmd_t *cmd );
/**
*	@brief
**/
void SV_AutoSaveEnd( void );
/**
*	@brief
**/
void SV_CheckForSavegame( mapcmd_t *cmd );
/**
*	@brief
**/
void SV_RegisterSavegames( void );
/**
*	@brief
**/
const int32_t SV_NoSaveGames( void );