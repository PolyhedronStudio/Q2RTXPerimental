/*********************************************************************
*
*
*	Server: Commands.
*
*
********************************************************************/
#pragma once


/**
*	@brief	
**/
void SV_InitOperatorCommands( void );

/**
*	@brief
**/
void SV_DropClient( client_t *drop, const char *reason );
/**
*	@brief
**/
void SV_RemoveClient( client_t *client );
/**
*	@brief
**/
void SV_CleanClient( client_t *client );

/**
*	@brief
**/
client_t *SV_GetPlayer( const char *s, const bool partial );
/**
*	@brief
**/
void SV_PrintMiscInfo( void );

/**
*	@brief
**/
void SV_UserinfoChanged( client_t *cl );


/**
*	@brief
**/
bool SV_RateLimited( ratelimit_t *r );
/**
*	@brief
**/
void SV_RateRecharge( ratelimit_t *r );
/**
*	@brief
**/
void SV_RateInit( ratelimit_t *r, const char *s );


/**
*	@brief
**/
addrmatch_t *SV_MatchAddress( list_t *list, netadr_t *address );


/**
*	@brief
**/
int SV_CountClients( void );


/**
*	@brief
**/
#if USE_ZLIB
/**
*	@brief
**/
voidpf SV_zalloc( voidpf opaque, uInt items, uInt size );
/**
*	@brief
**/
void SV_zfree( voidpf opaque, voidpf address );
#endif


/**
*	@brief
**/
void sv_sec_timeout_changed( cvar_t *self );
/**
*	@brief
**/
void sv_min_timeout_changed( cvar_t *self );