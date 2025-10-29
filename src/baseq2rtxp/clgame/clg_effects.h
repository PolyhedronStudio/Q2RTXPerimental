/********************************************************************
*
*
*	ClientGame: Effects Header
*
*
********************************************************************/
#pragma once



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
*	effects/clg_fx_footsteps.cpp
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
void CLG_FootstepEvent( const int32_t entityNumber, const bool isLadder = false );
/**
*   @brief  Passes on to CLG_FootstepEvent with isLadder beign true. Used by EV_FOOTSTEP_LADDER.
**/
void CLG_FootstepLadderEvent( const int32_t entityNumber );

/***
*
*	effects/clg_fx_classic.cpp
*
***/
// Wall Impact Puffs.
void CLG_ParticleEffect( const Vector3 &org, const Vector3 &dir, int color, int count );
void CLG_ParticleEffectWaterSplash( const Vector3 &org, const Vector3 &dir, int color, int count );
void CLG_BloodParticleEffect( const Vector3 &org, const Vector3 &dir, int color, int count );
void CLG_ParticleEffect2( const Vector3 &org, const Vector3 &dir, int color, int count );
void CLG_TeleporterParticles( const Vector3 &org );
void CLG_LogoutEffect( const Vector3 &org, int type );
void CLG_ItemRespawnParticles( const Vector3 &org );
void CLG_ExplosionParticles( const Vector3 &org );
void CLG_BigTeleportParticles( const Vector3 &org );
void CLG_BlasterParticles( const Vector3 &org, const Vector3 &dir );
void CLG_BlasterTrail( const Vector3 &start, const Vector3 &end );
void CLG_FlagTrail( const Vector3 &start, const Vector3 &end, int color );
void CLG_DiminishingTrail( const Vector3 &start, const Vector3 &end, centity_t *old, int flags );
void CLG_RocketTrail( const Vector3 &start, const Vector3 &end, centity_t *old );
void CLG_OldRailTrail( void );
void CLG_BubbleTrail( const Vector3 &start, const Vector3 &end );
//static void CL_FlyParticles( const Vector3 &origin, int count );
void CLG_FlyEffect( centity_t *ent, const Vector3 &origin );
void CLG_BfgParticles( entity_t *ent );
//FIXME combined with CL_ExplosionParticles
void CLG_BFGExplosionParticles( const Vector3 &org );
void CLG_TeleportParticles( const Vector3 &org );



/***
*
*	effects/clg_fx_dynamiclights.cpp
*
***/
/**
*   @brief
**/
void CLG_ClearDlights( void );
/**
*   @brief
**/
clg_dlight_t *CLG_AllocDlight( const int32_t key );
/**
*   @brief
**/
void CLG_AddDLights( void );



/***
*
*	effects/clg_fx_lightstyles.cpp
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
*	effects/clg_fx_muzzleflash.cpp & effects/clg_fx_muzzleflash2.cpp
*
***/
/**
*	@brief	Adds a muzzleflash dynamic light.
**/
clg_dlight_t *CLG_AddMuzzleflashDLight( centity_t *pl, Vector3 &fv, Vector3 &rv );
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
*	effects/clg_fx_newfx.cpp
*
***/
void CLG_Flashlight( int ent, const Vector3 &pos );
void CLG_ColorFlash( const Vector3 &pos, int ent, int intensity, float r, float g, float b );
void CLG_DebugTrail( const Vector3 &start, const Vector3 &end );
void CLG_ForceWall( const Vector3 &start, const Vector3 &end, int color );
void CLG_BubbleTrail2( const Vector3 &start, const Vector3 &end, int dist );
void CLG_Heatbeam( const Vector3 &start, const Vector3 &forward );
void CLG_ParticleSteamEffect( const Vector3 &org, const Vector3 &dir, int color, int count, int magnitude );
void CLG_ParticleSteamEffect2( clg_sustain_t *self );
void CLG_TrackerTrail( const Vector3 &start, const Vector3 &end, int particleColor );
void CLG_Tracker_Shell( const Vector3 &origin );
void CLG_MonsterPlasma_Shell( const Vector3 &origin );
void CLG_Widowbeamout( clg_sustain_t *self );
void CLG_Nukeblast( clg_sustain_t *self );
void CLG_WidowSplash( void );
void CLG_TagTrail( const Vector3 &start, const Vector3 &end, int color );
void CLG_ColorExplosionParticles( const Vector3 &org, int color, int run );
void CLG_ParticleSmokeEffect( const Vector3 &org, const Vector3 &dir, int color, int count, int magnitude );
void CLG_BlasterParticles2( const Vector3 &org, const Vector3 &dir, unsigned int color );
void CLG_BlasterTrail2( const Vector3 &start, const Vector3 &end );
void CLG_IonripperTrail( const Vector3 &start, const Vector3 &ent );
void CLG_TrapParticles( centity_t *ent, const Vector3 &origin );
void CLG_ParticleEffect3( const Vector3 &org, const Vector3 &dir, int color, int count );



/***
*
*	effects/clg_fx_particles.cpp
* 
***/
/**
*   @brief
**/
void CLG_ClearParticles( void );
/**
*   @brief
**/
clg_particle_t *CLG_AllocParticle( void );
/**
*	@brief
**/
void CLG_AddParticles( void );