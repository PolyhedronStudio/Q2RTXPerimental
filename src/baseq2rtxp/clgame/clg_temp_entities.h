/********************************************************************
*
*
*	ClientGame: Effects Header
*
*
********************************************************************/
#pragma once



/*
*
*	clg_temp_entities.cpp
*
*/
/**
*   @brief	Initialize the temporary entities system.
**/
void CLG_TemporaryEntities_Init( void );
/**
*   @brief  Clear all temporary entities for the current frame.
**/
void CLG_TemporaryEntities_Clear( void );
/**
*   @brief   Parses the Temp Entity packet data and creates the appropriate effects.
**/
void CLG_TemporaryEntities_Parse( void );
/**
*   @brief	Add all temporary entity effects for the current frame.
**/
void CLG_TemporaryEntities_Add( void );



/*
*
*	temp_entities/clg_te_beams.cpp
*
*/
/**
*   @brief
**/
void CLG_ClearBeams( void );
/**
*   @brief
**/
void CLG_ParseBeam( const qhandle_t model );
/**
*   @brief
**/
void CLG_ParsePlayerBeam( const qhandle_t model );
/**
*   @brief
**/
void CLG_AddBeams( void );
/**
*   @brief  Draw player locked beams. Currently only used by the plasma beam.
**/
void CLG_AddPlayerBeams( void );



/***
*
*	temp_entities/clg_te_explosions.cpp
*
***/
extern cvar_t *clg_explosion_sprites;
extern cvar_t *clg_explosion_frametime;

extern cvar_t *gibs;

/**
*   @brief
**/
void CLG_ClearExplosions( void );
/**
*   @brief
**/
clg_explosion_t *CLG_AllocExplosion( void );
/**
*   @brief
**/
clg_explosion_t *CLG_PlainExplosion( const bool withSmoke );
/**
*   @brief
**/
void CLG_SmokeAndFlash( const vec3_t origin );
/**
*   @brief
**/
//static void CL_AddExplosionLight( clg_explosion_t *ex, float phase );
/**
*   @brief
**/
void CLG_AddExplosions( void );



/***
*
*	temp_entities/clg_te_lasers.cpp
*
***/
/**
*   @brief
**/
void CLG_ClearLasers( void );
/**
*   @brief
**/
laser_t *CLG_AllocLaser( void );
/**
*   @brief
**/
void CLG_AddLasers( void );

/**
*   @brief
**/
void CLG_ParseLaser( const int32_t colors );



/***
*
*	temp_entities/clg_te_railtrails.cpp
*
***/
/**
*   @brief
**/
void CLG_RailCore( void );
/**
*   @brief
**/
void CLG_RailSpiral( void );
/**
*   @brief
**/
void CLG_RailLights( const color_t color );
/**
*   @brief
**/
void CLG_RailTrail( void );



/***
*
*	temp_entities/clg_te_sustain.cpp
*
***/
/**
*   @brief
**/
void CLG_ClearSustains( void );
/**
*   @brief
**/
clg_sustain_t *CLG_AllocSustain( void );
/**
*   @brief
**/
void CLG_ProcessSustain( void );
/**
*   @brief
**/
void CLG_ParseSteam( void );
/**
*   @brief
**/
void CLG_ParseWidow( void );
/**
*   @brief
**/
void CLG_ParseNuke( void );