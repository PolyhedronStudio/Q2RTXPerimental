/********************************************************************
*
*
*	ClientGame: Core GameUI Implementation:
*
*
********************************************************************/


/**
*
*
*
*	Game UI - UI Core:
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
*
*
*
*	Game UI - Drawing:
*
*
*
**/
/**
*	@brief	Iterate the mu drawing commands and render them using the client's rendering functions.
**/
void CLG_UI_DrawRenderCommands();



/**
*
* 
*
*	Game UI - User Input:
*
* 
*
**/
//! Called when the client receives a key event, gives the client game a chance to handle it 
//! when the client's current key event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_KeyEvent( const int32_t key, const bool down );
//! Called when the client receives a character event, gives the client game a chance to handle it 
//! when the client's current key event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_CharEvent( const char c );