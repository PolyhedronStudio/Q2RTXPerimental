/********************************************************************
*
*
*	ClientGame: Player State View Transition Header
*
*
********************************************************************/
#pragma once


/**
*   @brief  Sets clgi.client->refdef view values and sound spatialization params.
*           Usually called from CL_PrepareViewEntities, but may be directly called from the main
*           loop if rendering is disabled but sound is running.
*	@note	CLG_PrepareViewEntities is called from CL_A
**/
void CLG_CalculateViewValues( void );
