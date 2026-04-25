/********************************************************************
*
*
*	ClientGame: MicroUI Core Implementation:
*
*
********************************************************************/


/**
*
*
*
*	UI Core Functions:
*
*
*
**/
/**
*	@brief	Allocate the client's UI context, called when the client begins and after loading plague has ended.
**/
void CLG_UI_AllocateContext();
/**
*	Free the client's UI context, called when the client disconnects and before clearing its state.
**/
void CLG_UI_FreeContext();
/**
*	@brief	Called each frame to update the client's UI state,
*			it runs at the same framerate as the client does so it remains responsive.
**/
void CLG_UI_RefreshFrame();