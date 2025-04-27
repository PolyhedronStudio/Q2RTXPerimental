/********************************************************************
*
*
*	SharedGame: Server Game SG Function Implementations.
*
*
********************************************************************/
#include "shared/shared.h"

// define SVGAME_INCLUDE so that game.h does not define the
// short, server-visible svg_client_t and edict_t structures,
// because we define the full size ones in this file
#include "shared/server/sv_game.h"
#include "svgame/svg_local.h"
#include "svgame/svg_local_fwd.h"

// Extern here right after including shared/client/cl_game.h
extern svgame_import_t gi;



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
	gi.dprintf( "%s", msg );
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
		return sgent->s.number;
	} else {
		return -1;
	}
}
/**
*	@brief	Returns the matching entity pointer for the given entity number.
**/
sgentity_s *SG_GetEntityForNumber( const int32_t number ) {
	return g_edict_pool.EdictForNumber( number );
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
	return gi.GetConfigString( configStringIndex );
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
	return gi.cvar( var_name, value, flags );
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
	return gi.FS_FileExistsEx( path, flags );
}
/**
*	@brief	Loads file into designated buffer. A nul buffer will return the file length without loading.
*	@return	length < 0 indicates error.
**/
const int32_t SG_FS_LoadFile( const char *path, void **buffer ) {
	return gi.FS_LoadFile( path, buffer );
}
/**
*	@brief	Frees FS_FILESYSTEM Tag Malloc file buffer.
**/
void SG_FS_FreeFile( void *buffer ) {
	return gi.FS_FreeFile( buffer );
}


/**
*
*	(Skeletal-) Model:
*
**/
/**
*   @brief  Pointer to model data matching the name, otherwise a (nullptr) on failure.
**/
const model_t *SG_GetModelDataForName( const char *name ) {
	return gi.GetModelDataForName( name );
}
/**
*   @return Pointer to model data matching the resource handle, otherwise a (nullptr) on failure.
**/
const model_t *SG_GetModelDataForHandle( const qhandle_t handle ) {
	return gi.GetModelDataForHandle( handle );
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
	return gi.TagMalloc( size, tag );
}
/**
*	@brief
**/
void *SG_Z_TagMallocz( const uint32_t size, const uint32_t tag ) {
	return gi.TagMallocz( size, tag );
}
/**
*	@brief
**/
void *SG_Z_TagReMalloc( void *ptr, const uint32_t size ) {
	return gi.TagReMalloc( ptr, size );
}
/**
*	@brief
**/
void SG_Z_TagFree( void *block ) {
	gi.TagFree( block );
}
/**
*	@brief	FreeTags
**/
void SG_Z_TagFree( const uint32_t tag ) {
	gi.FreeTags( tag );
}



/**
*
*	Other:
*
**/
/**
*	@brief	Server side sharedgame implementation of QMTime::frames.
**/
int64_t QMTime::Frames() const {
	return _ms / gi.frame_time_ms;
}

/**
*   @return The realtime of the server since boot time.
**/
const QMTime &SG_GetLevelTime() {
	return level.time;
}
/**
*   @return The realtime of the server since boot time.
**/
const uint64_t SG_GetRealTime() {
	return gi.GetRealTime();
}

/**
*	@return	The linear interpolated frame fraction value.
**/
const double SG_GetFrameLerpFraction() {
	return 1.0;
}

/**
*	@return	The linear extrapolated frame fraction value.
**/
const double SG_GetFrameXerpFraction() {
	return 1.0;
}