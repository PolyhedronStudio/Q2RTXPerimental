/********************************************************************
*
*
*	ClientGame: Server Message Parsing.
*
*
********************************************************************/
#include "clg_local.h"

/**
*	@brief	Parses the layout string for server cmd: svc_inventory.
**/
static void CLG_ParseLayout( void ) {
	clgi.MSG_ReadString( clgi.client->layout, sizeof( clgi.client->layout ) );
	clgi.ShowNet( 2, "    \"%s\"\n", clgi.client->layout );
}

/**
*	@brief	Parses the player inventory for server cmd: svc_inventory.
**/
static void CLG_ParseInventory( void ) {
	for ( int32_t i = 0; i < MAX_ITEMS; i++ ) {
		clgi.client->inventory[ i ] = clgi.MSG_ReadIntBase128();
	}
}

/**
*	@brief	Called by the client BEFORE all server messages have been parsed.
**/
void PF_StartServerMessage( ) {

}

/**
*	@brief	Called by the client AFTER all server messages have been parsed.
**/
void PF_EndServerMessage() {

}

/**
*	@brief	Called by the client when it does not recognize the server message itself,
*			so it gives the client game a chance to handle and respond to it.
*	@return	True if the message was handled properly. False otherwise.
**/
const bool PF_ParseServerMessage( const int32_t serverMessage ) {
	switch ( serverMessage ) {
	//case svc_centerprint:
	//	return true;
	//	break;
	case svc_inventory:
		CLG_ParseInventory();
		return true;
	break;
	case svc_layout:
		CLG_ParseLayout();
		return true;
	break;
	//case svc_muzzleflash:
	//	return true;
	//	break;
	//case svc_muzzleflash2:
	//	return true;
	//	break;
	//case svc_temp_entity:
	//	return true;
	//	break;
	default:
		return false;
	}

	return false;
}