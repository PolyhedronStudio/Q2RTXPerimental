/********************************************************************
*
*
*	ClientGame: View Header
*
*
********************************************************************/
#pragma once


/**
*   @brief
**/
const float PF_CalculateFieldOfView( const float fov_x, const float width, const float height );
/**
*   @brief  Sets clgi.client->refdef view values and sound spatialization params.
*           Usually called from CL_PrepareViewEntities, but may be directly called from the main
*           loop if rendering is disabled but sound is running.
**/
void PF_CalculateViewValues( void );
/**
*   @brief  Finishes the view values, or rather, determines whether to setup a third or first person view.
**/
void CLG_FinishViewValues( void );
/**
*   @brief
**/
void PF_ClearViewScene( void );
/**
*   @brief
**/
void PF_PrepareViewEntities( void );
/**
*	@brief	Returns the predictedState based player view render definition flags.
**/
const refdef_flags_t PF_GetViewRenderDefinitionFlags( void );

