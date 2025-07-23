/********************************************************************
*
*
*	ServerGame: Player View
*	NameSpace: "".
*
*
********************************************************************/
#pragma once


/**
*   @brief  This will be called once for all clients on each server frame, before running any other entities in the world.
**/
void SVG_Client_BeginServerFrame( svg_base_edict_t *ent );
/**
*	@brief	Called for each player at the end of the server frame, and right after spawning.
**/
void SVG_Client_EndServerFrame( svg_base_edict_t *ent );

/**
*	@brief	Apply possibly applied 'effects'.
**/
void SVG_SetClientEffects( svg_base_edict_t *ent );
/**
*	@brief	Determine the client's event.
**/
void SVG_SetClientEvent( svg_base_edict_t *ent );
/**
*	@brief	Determine client sound.
**/
void SVG_SetClientSound( svg_base_edict_t *ent );
/**
*	@brief	Will set the client entity's animation for the current frame.
**/
void SVG_SetClientFrame( svg_base_edict_t *ent );


/**
*	@brief	Calculate roll view vector angles.
**/
const double SV_CalcRoll( const Vector3 &angles, const Vector3 &velocity );
/**
*	@brief	Check if any world related collisions occure and if so apply necessary effects.
**/
void P_CheckWorldEffects( void );
/**
*	@brief	Determine the contents of the client's view org as well calculate possible
*			damage influenced screen blends.
**/
void P_CalculateBlend( svg_base_edict_t *ent );
/**
*	@brief	Handles and applies the screen color blends and view kicks
**/
void P_DamageFeedback( svg_base_edict_t *player );
/*
===============
P_CalculateViewOffset

Auto pitching on slopes?

  fall from 128: 400 = 160000
  fall from 256: 580 = 336400
  fall from 384: 720 = 518400
  fall from 512: 800 = 640000
  fall from 640: 960 =

  damage = deltavelocity*deltavelocity  * 0.0001

===============
*/
void P_CalculateViewOffset( svg_base_edict_t *ent );