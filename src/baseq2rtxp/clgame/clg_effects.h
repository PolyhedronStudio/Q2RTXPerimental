/********************************************************************
*
*
*	ClientGame: Effects Header
*
*
********************************************************************/
#pragma once



/***
*
*
*
*
*	clg_effects.cpp
*
*
*
*
***/
/**
*   @brief
**/
void CLG_ClearEffects( void );

/**
*   @brief
**/
void CLG_InitEffects( void );


/***
* 
* 
* 
*
*	effects/clg_fx_footsteps.cpp
*
* 
* 
* 
***/
/**
*	@brief  Precaches footstep audio for all material "kinds".
**/
void CLG_PrecacheFootsteps( void );
/**
*   @brief  Will play an appropriate footstep sound effect depending on the material that we're currently
*           standing on.
**/
void CLG_FX_FootStepSound( const int32_t entityNumber, const Vector3 &lerpOrigin, const bool isLadder = false, const bool isLocalClient = false );


/***
*
* 
* 
* 
*	effects/clg_fx_classic.cpp
*
* 
* 
* 
***/
// Wall Impact Puffs.
void CLG_FX_ParticleEffect( const Vector3 &org, const Vector3 &dir, int color, int count );
void CLG_FX_ParticleEffectWaterSplash( const Vector3 &org, const Vector3 &dir, int color, int count );
void CLG_FX_BloodParticleEffect( const Vector3 &org, const Vector3 &dir, int color, int count );
void CLG_FX_ParticleEffect2( const Vector3 &org, const Vector3 &dir, int color, int count );
void CLG_FX_TeleporterParticles( const Vector3 &org );
void CLG_FX_LogoutEffect( const Vector3 &org, int type );
void CLG_FX_ItemRespawnParticles( const Vector3 &org );
void CLG_FX_ExplosionParticles( const Vector3 &org );
void CLG_FX_BigTeleportParticles( const Vector3 &org );
void CLG_FX_BlasterParticles( const Vector3 &org, const Vector3 &dir );
void CLG_FX_BlasterTrail( const Vector3 &start, const Vector3 &end );
void CLG_FX_FlagTrail( const Vector3 &start, const Vector3 &end, int color );
void CLG_FX_DiminishingTrail( const Vector3 &start, const Vector3 &end, centity_t *old, int flags );
void CLG_FX_RocketTrail( const Vector3 &start, const Vector3 &end, centity_t *old );
void CLG_FX_OldRailTrail( void );
void CLG_FX_BubbleTrail( const Vector3 &start, const Vector3 &end );
//static void CL_FlyParticles( const Vector3 &origin, int count );
void CLG_FX_FlyEffect( centity_t *ent, const Vector3 &origin );
void CLG_FX_BfgParticles( entity_t *ent );
//FIXME combined with CL_ExplosionParticles
void CLG_FX_BFGExplosionParticles( const Vector3 &org );
void CLG_FX_TeleportParticles( const Vector3 &org );



/***
* 
* 
* 
*
*	effects/clg_fx_dynamiclights.cpp
*
* 
* 
* 
***/
/**
*   @brief
**/
void CLG_ClearDynamicLights( void );
/**
*   @brief
**/
clg_dlight_t *CLG_AllocateDynamicLight( const int32_t key );
/**
*   @brief
**/
void CLG_AddDynamicLights( void );



/***
* 
* 
* 
*
*	effects/clg_fx_lightstyles.cpp
*
* 
* 
* 
***/
/**
*	@brief
**/
void CLG_ClearLightStyles( void );
/**
*   @brief
**/
void CLG_SetLightStyle( const int32_t index, const char *s );
/**
*   @brief
**/
void CLG_AddLightStyles( void );



/***
*
* 
* 
* 
*	effects/clg_fx_muzzleflash.cpp & effects/clg_fx_muzzleflash2.cpp
*
* 
* 
* 
***/
/**
*	@brief	Adds a muzzleflash dynamic light.
**/
clg_dlight_t *CLG_AddMuzzleflashDynamicLight( const centity_t *pl, const Vector3 &vForward, const Vector3 &vRight );
/**
*   @brief  Handles the parsed client muzzleflash effects.
**/
void CLG_MuzzleFlash( void );
#if 0 // WID: Removed
/**
*   @brief  Handles the parsed entities/monster muzzleflash effects.
**/
void CLG_MuzzleFlash2( void );
#endif


/***
*
* 
* 
* 
*	effects/clg_fx_newfx.cpp
*
* 
* 
* 
***/
void CLG_FX_Flashlight( int ent, const Vector3 &pos );
void CLG_FX_ColorFlash( const Vector3 &pos, int ent, int intensity, float r, float g, float b );
void CLG_FX_DebugTrail( const Vector3 &start, const Vector3 &end );
void CLG_FX_ForceWall( const Vector3 &start, const Vector3 &end, int color );
void CLG_FX_BubbleTrail2( const Vector3 &start, const Vector3 &end, int dist );
void CLG_FX_Heatbeam( const Vector3 &start, const Vector3 &forward );
void CLG_FX_ParticleSteamEffect( const Vector3 &org, const Vector3 &dir, int color, int count, int magnitude );
void CLG_FX_ParticleSteamEffect2( clg_sustain_t *self );
void CLG_FX_TrackerTrail( const Vector3 &start, const Vector3 &end, int particleColor );
void CLG_FX_Tracker_Shell( const Vector3 &origin );
void CLG_FX_MonsterPlasma_Shell( const Vector3 &origin );
void CLG_FX_Widowbeamout( clg_sustain_t *self );
void CLG_FX_Nukeblast( clg_sustain_t *self );
void CLG_FX_WidowSplash( void );
void CLG_FX_TagTrail( const Vector3 &start, const Vector3 &end, int color );
void CLG_FX_ColorExplosionParticles( const Vector3 &org, int color, int run );
void CLG_FX_ParticleSmokeEffect( const Vector3 &org, const Vector3 &dir, int color, int count, int magnitude );
void CLG_FX_BlasterParticles2( const Vector3 &org, const Vector3 &dir, unsigned int color );
void CLG_FX_BlasterTrail2( const Vector3 &start, const Vector3 &end );
void CLG_FX_IonripperTrail( const Vector3 &start, const Vector3 &ent );
void CLG_FX_TrapParticles( centity_t *ent, const Vector3 &origin );
void CLG_FX_ParticleEffect3( const Vector3 &org, const Vector3 &dir, int color, int count );



/***
*
* 
* 
* 
*	effects/clg_fx_particles.cpp
* 
* 
* 
* 
***/
/**
*   @brief
**/
void CLG_ClearParticles( void );
/**
*   @brief
**/
clg_particle_t *CLG_AllocateParticle( void );
/**
*	@brief
**/
void CLG_AddParticles( void );






/**
*
*
*
*	FootStep Effects:
*
*
*
**/
/**
*   @brief  The (LOCAL PLAYER) footstep sound event handler.
**/
void CLG_FX_LocalFootStep( const int32_t entityNumber, const Vector3 &lerpOrigin );

/**
*   @brief  The generic (PLAYER) footstep sound event handler.
**/
void CLG_FX_PlayerFootStep( const int32_t entityNumber, const Vector3 &lerpOrigin );
/**
*   @brief  The generic (OTHER entity) footstep sound event handler.
**/
void CLG_FX_OtherFootStep( const int32_t entityNumber, const Vector3 &lerpOrigin );

/**
*   @brief  Passes on to CLG_FX_FootStepSound with isLadder beign true. Used by EV_FOOTSTEP_LADDER.
**/
void CLG_FX_LadderFootStep( const int32_t entityNumber, const Vector3 &lerpOrigin );