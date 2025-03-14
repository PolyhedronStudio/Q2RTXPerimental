/*********************************************************************
*
*
*	Server: Send.
*
*
********************************************************************/
#pragma once


#define SV_OUTPUTBUF_LENGTH     (MAX_PACKETLEN_DEFAULT - 16)

#define SV_ClientRedirect() \
    Com_BeginRedirect(RD_CLIENT, sv_outputbuf, MAX_STRING_CHARS - 1, SV_FlushRedirect)

#define SV_PacketRedirect() \
    Com_BeginRedirect(RD_PACKET, sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect)

extern char sv_outputbuf[ SV_OUTPUTBUF_LENGTH ];

void SV_FlushRedirect( int redirected, char *outputbuf, size_t len );

void SV_SendClientMessages( void );
void SV_SendAsyncPackets( void );

void SV_Multicast( const vec3_t origin, multicast_t to, bool reliable );
void SV_ClientPrintf( client_t *cl, int level, const char *fmt, ... ) q_printf( 3, 4 );
void SV_BroadcastPrintf( int level, const char *fmt, ... ) q_printf( 2, 3 );
void SV_ClientCommand( client_t *cl, const char *fmt, ... ) q_printf( 2, 3 );
void SV_BroadcastCommand( const char *fmt, ... ) q_printf( 1, 2 );
void SV_ClientAddMessage( client_t *client, int flags );
void SV_ShutdownClientSend( client_t *client );
void SV_InitClientSend( client_t *newcl );