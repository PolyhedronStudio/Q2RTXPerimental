/********************************************************************
*
*
*	Skeletal Model Configuration(.skc) File Load and Parser.
* 
*	Used to load up the extra needed model data such as the specified
*	actions, blends, and events.
*
* 
********************************************************************/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif // #ifdef __cplusplus

/**
*
*
*	Skeletal Model Configuration Loading:
*
*
**/
/**
*	@brief	Will load up the skeletal model configuration file, usually, 'tris.skc'.
*			It does so by allocating the outputBuffer which needs to be Z_Freed by hand.
*	@return	True on success, false on failure.
**/
char *SKM_LoadConfigurationFile( model_t *model, const char *configurationFilePath, int32_t *loadResult );




/**
*
*
*	Skeletal Model Configuration Parsing:
*
*
**/
/**
*	@brief	Parses the buffer, allocates specified memory in the model_t struct and fills it up with the results.
*	@
*	@return	True on success, false on failure.
**/
const int32_t SKM_ParseConfigurationBuffer( model_t *model, const char *configurationFilePath, char *fileBuffer );


#ifdef __cplusplus
}
#endif // #ifdef __cplusplus