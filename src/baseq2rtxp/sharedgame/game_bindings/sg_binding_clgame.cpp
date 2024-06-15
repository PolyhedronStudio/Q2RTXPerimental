/********************************************************************
*
*
*	SharedGame: Client Game SG Function Implementations.
*
*
********************************************************************/
#include "shared/shared.h"
#include "shared/util_list.h"

// define CLGAME_INCLUDE so that game.h does not define the
// short, server-visible gclient_t and edict_t structures,
// because we define the full size ones in this file
#include "shared/clgame.h"
#include "../../clgame/clg_local.h"

// Extern here right after including shared/clgame.h
extern clgame_import_t clgi;



/**
*
*	Core:
*
**/
/**
*	@brief	Wrapper for using the appropriate developer print for the specific game module we're building.
**/
void SG_DPrintf( const char *fmt, ... ) {
	static constexpr int32_t MAXPRINTMSG = 4096;
	char        msg[ MAXPRINTMSG ];
	va_list     argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	// Developer print.
	clgi.Print( PRINT_DEVELOPER, "%s", msg );
	//Com_LPrintf( PRINT_DEVELOPER, "%s", msg );
}



/**
*
*	Entities:
*
**/
/**
*	@brief	Returns the entity number, -1 if invalid(nullptr, or out of bounds).
**/
const int32_t SG_GetEntityNumber( sgentity_s *sgent ) {
	if ( sgent ) {
		return sgent->current.number;
	} else {
		return -1;
	}
}



/**
*
*	ConfigStrings:
*
**/
/**
*	@brief	Returns the given configstring that sits at index.
**/
configstring_t *SG_GetConfigString( const int32_t configStringIndex ) {
	return clgi.GetConfigString( configStringIndex );
}



/**
*
*	CVars:
*
**/
/**
*	@brief	Wraps around CVar_Get
**/
cvar_t *SG_CVar_Get( const char *var_name, const char *value, const int32_t flags ) {
	return clgi.CVar_Get( var_name, value, flags );
}



/**
*
*
*	FileSystem:
*
*
**/
/**
*	@brief	Returns non 0 in case of existance.
**/
const int32_t SG_FS_FileExistsEx( const char *path, const uint32_t flags ) {
	return clgi.FS_FileExistsEx( path, flags );
}
/**
*	@brief	Loads file into designated buffer. A nul buffer will return the file length without loading.
*	@return	length < 0 indicates error.
**/
const int32_t SG_FS_LoadFile( const char *path, void **buffer ) {
	return clgi.FS_LoadFile( path, buffer );
}



/**
*
*	Skeletal Model:
*
**/
/**
*   @brief  Pointer to model data matching the name, otherwise a (nullptr) on failure.
**/
const model_t *SG_GetModelDataForName( const char *name ) {
	return clgi.R_GetModelDataForName( name );
}
/**
*   @return Pointer to model data matching the resource handle, otherwise a (nullptr) on failure.
**/
const model_t *SG_GetModelDataForHandle( const qhandle_t handle ) {
	return clgi.R_GetModelDataForHandle( handle );
}



/**
*
*	Zone (Tag-)Malloc:
*
**/
/**
*	@brief
**/
void *SG_Z_TagMalloc( const uint32_t size, const uint32_t tag ) {
	return clgi.TagMalloc( size, tag );
}
/**
*	@brief
**/
void SG_Z_TagFree( void *block ) {
	clgi.TagFree( block );
}
/**
*	@brief	FreeTags
**/
void SG_Z_TagFree( const uint32_t tag ) {
	clgi.FreeTags( tag );
}


/**
*
*	Other:
*
**/
/**
*	@brief	Client side sharedgame implementation of sg_time_t::frames.
**/
int64_t sg_time_t::frames() const {
	return _ms / clgi.frame_time_ms;
}