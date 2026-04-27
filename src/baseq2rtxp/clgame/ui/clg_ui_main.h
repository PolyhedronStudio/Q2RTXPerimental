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
*	@note	it runs equal to the framerate at which the client runs, to make sure that 
*			it remains responsive.
**/
void CLG_UI_ProcessFrame();

/**
*	@brief	Iterate the mu drawing commands and render them using the client's rendering functions.
**/
void CLG_UI_DrawRenderCommands();