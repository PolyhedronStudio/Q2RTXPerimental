/********************************************************************
*
*
*	SVGame: Edicts Functionalities:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_clients.h"



/**
*   @brief  Allocates a new array of clients, and returns a pointer to it.
**/
svg_client_t *SVG_Clients_Reallocate( const int32_t maxClients ) {
    // Allocate the clients.
    svg_client_t *clients = (svg_client_t *)gi.TagMallocz( maxClients * sizeof( clients[ 0 ] ), TAG_SVGAME );
    //new( clients ) svg_client_t[ maxClients ];
    for ( int32_t i = 0; i < maxClients; i++ ) {
        // Use placement new to construct the client.
        new( &clients[ i ] ) svg_client_t();
        // Assign the client number.
        clients[ i ].clientNum = i;
    }
    return clients;
}