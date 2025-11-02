/********************************************************************
*
*
*	ClientGame: View Header
*
*
********************************************************************/
#pragma once



/**
*	@brief	Called whenever the refresh module is (re-)initialized.
**/
void CLG_InitViewScene( void );
/**
*	@brief	Called whenever the refresh module is shutdown.
**/
void CLG_ShutdownViewScene( void );

/**
*   @details    Whenever we have received a valid frame from the server, we will
*               build up a fresh refdef for rendering the scene from the player's point of view.
*
*               In doing so we'll prepare the scene by clearing out the previous scene, transitioning
*               the entities to their new states, interpolating where necessary. Similarly the player's
*               state will be predicted, the states transitioned and at last we'll
*               calculate the view values calculated from that.
**/
void CLG_DrawActiveViewState( void );
/**
*   @brief
**/
void CLG_ClearViewScene( void );
/**
*   @brief
**/
void CLG_PrepareViewEntities( void );


/**
*   @brief  Sets clgi.client->refdef view values and sound spatialization params.
*           Usually called from CL_PrepareViewEntities, but may be directly called from the main
*           loop if rendering is disabled but sound is running.
**/
void CLG_CalculateViewValues( void );
/**
*   @brief  Finishes the view values, or rather, determines whether to setup a third or first person view.
**/
void CLG_FinishViewValues( void );
/**
*   @brief  Calculates the client's field of view.
**/
const float CLG_CalculateFieldOfView( const float fov_x, const float width, const float height );


/**
*	@brief	Returns the predictedState based player view render definition flags.
**/
const refdef_flags_t CLG_GetViewRenderDefinitionFlags( void );
