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
void CLG_FootstepEvent( const int32_t entityNumber );


/***
*
*	effects/clg_fx_classic.cpp
*
***/
// Wall Impact Puffs.
void CLG_ParticleEffect( const vec3_t org, const vec3_t dir, int color, int count );
void CLG_ParticleEffectWaterSplash( const vec3_t org, const vec3_t dir, int color, int count );
void CLG_BloodParticleEffect( const vec3_t org, const vec3_t dir, int color, int count );
void CLG_ParticleEffect2( const vec3_t org, const vec3_t dir, int color, int count );
void CLG_TeleporterParticles( const vec3_t org );
void CLG_LogoutEffect( const vec3_t org, int type );
void CLG_ItemRespawnParticles( const vec3_t org );
void CLG_ExplosionParticles( const vec3_t org );
void CLG_BigTeleportParticles( const vec3_t org );
void CLG_BlasterParticles( const vec3_t org, const vec3_t dir );
void CLG_BlasterTrail( const vec3_t start, const vec3_t end );
void CLG_FlagTrail( const vec3_t start, const vec3_t end, int color );
void CLG_DiminishingTrail( const vec3_t start, const vec3_t end, centity_t *old, int flags );
void CLG_RocketTrail( const vec3_t start, const vec3_t end, centity_t *old );
void CLG_OldRailTrail( void );
void CLG_BubbleTrail( const vec3_t start, const vec3_t end );
//static void CL_FlyParticles( const vec3_t origin, int count );
void CLG_FlyEffect( centity_t *ent, const vec3_t origin );
void CLG_BfgParticles( entity_t *ent );
//FIXME combined with CL_ExplosionParticles
void CLG_BFGExplosionParticles( const vec3_t org );
void CLG_TeleportParticles( const vec3_t org );



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
*   @brief  Handles the parsed client muzzleflash effects.
**/
void CLG_MuzzleFlash( void );
/**
*   @brief  Handles the parsed entities/monster muzzleflash effects.
**/
void CLG_MuzzleFlash2( void );



/***
*
*	effects/clg_fx_newfx.cpp
*
***/
void CLG_Flashlight( int ent, const vec3_t pos );
void CLG_ColorFlash( const vec3_t pos, int ent, int intensity, float r, float g, float b );
void CLG_DebugTrail( const vec3_t start, const vec3_t end );
void CLG_ForceWall( const vec3_t start, const vec3_t end, int color );
void CLG_BubbleTrail2( const vec3_t start, const vec3_t end, int dist );
void CLG_Heatbeam( const vec3_t start, const vec3_t forward );
void CLG_ParticleSteamEffect( const vec3_t org, const vec3_t dir, int color, int count, int magnitude );
void CLG_ParticleSteamEffect2( clg_sustain_t *self );
void CLG_TrackerTrail( const vec3_t start, const vec3_t end, int particleColor );
void CLG_Tracker_Shell( const vec3_t origin );
void CLG_MonsterPlasma_Shell( const vec3_t origin );
void CLG_Widowbeamout( clg_sustain_t *self );
void CLG_Nukeblast( clg_sustain_t *self );
void CLG_WidowSplash( void );
void CLG_TagTrail( const vec3_t start, const vec3_t end, int color );
void CLG_ColorExplosionParticles( const vec3_t org, int color, int run );
void CLG_ParticleSmokeEffect( const vec3_t org, const vec3_t dir, int color, int count, int magnitude );
void CLG_BlasterParticles2( const vec3_t org, const vec3_t dir, unsigned int color );
void CLG_BlasterTrail2( const vec3_t start, const vec3_t end );
void CLG_IonripperTrail( const vec3_t start, const vec3_t ent );
void CLG_TrapParticles( centity_t *ent, const vec3_t origin );
void CLG_ParticleEffect3( const vec3_t org, const vec3_t dir, int color, int count );



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