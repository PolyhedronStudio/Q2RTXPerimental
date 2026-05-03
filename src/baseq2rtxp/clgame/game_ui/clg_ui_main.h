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
*	@brief	Close the active GameUI menu and return input focus to normal gameplay.
**/
void CLG_UI_CloseMenu();

/**
*	@brief	Will open the specified menu, based on the menuID received from the server.
**/
enum class sg_game_ui_menu_id;
void CLG_UI_OpenMenu( const sg_game_ui_menu_id menuID );



/**
*
*
*
*	Game UI - User Input:
*
*	Fired by user input events when the engine's event destination is set to keyEventDest_t::GAME_UI.
*	It gives the client game a chance to handle user input events that are meant for the UI, such as when
*	user is navigating the in-game menu, etc.
*
*
*
**/
//! Called when the client receives a mouse move event, gives the client game a chance to handle it 
//! when the client's current input(key) event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_MouseMoveEvent( const int32_t x, const int32_t y );
//! Called when the client receives a mouse button event, gives the client game a chance to handle it 
//! when the client's current input(key) event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_MouseButtonEvent( const int32_t button, const bool isDown, const int32_t x, const int32_t y );
//! Called when the client receives a mouse scroll event, gives the client game a chance to handle it 
//! when the client's current input(key) event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_MouseScrollEvent( const int32_t scrollY );

//! Called when the client receives a key event, gives the client game a chance to handle it 
//! when the client's current key event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_KeyEvent( const int32_t key, const bool down );
//! Called when the client receives a character event, gives the client game a chance to handle it 
//! when the client's current key event destination is set to keyEventDest_t::GAME_UI.
void CLG_UI_TextInputEvent( const char *str );



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



