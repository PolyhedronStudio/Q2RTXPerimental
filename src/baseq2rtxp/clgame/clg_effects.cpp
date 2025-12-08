/********************************************************************
*
*
*	ClientGame: Effects
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"



/**
*   @brief 
**/
void CLG_ClearEffects( void ) {
    CLG_ClearLightStyles();
    CLG_ClearParticles();
    CLG_ClearDynamicLights();
}

/**
*   @brief
**/
void CLG_InitEffects( void ) {
    for ( int32_t i = 0; i < NUMVERTEXNORMALS; i++ ) {
        for ( int32_t j = 0; j < 3; j++ ) {
            game.avelocities[ i ][ j ] = ( Q_rand() & 255 ) * 0.01f; // TODO: 0.0025f?
        }
    }
}