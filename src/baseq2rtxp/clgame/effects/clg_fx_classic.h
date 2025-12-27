/********************************************************************
*
*
*	ClientGame: 'Classic' FX Implementation:
*
*
********************************************************************/
#pragma once


/**
*
*
*
*   Miscellaneous Entity Event Definitions:
*
*
*
**/
/**
*   Temp Entity Event - Splash Types:
**/
static constexpr int32_t SPLASH_FX_COLOR_UNKNOWN = 0;
static constexpr int32_t SPLASH_FX_COLOR_SPARKS = 1;
static constexpr int32_t SPLASH_FX_COLOR_BLUE_WATER = 2;
static constexpr int32_t SPLASH_FX_COLOR_BROWN_WATER = 3;
static constexpr int32_t SPLASH_FX_COLOR_SLIME = 4;
static constexpr int32_t SPLASH_FX_COLOR_LAVA = 5;
static constexpr int32_t SPLASH_FX_COLOR_BLOOD = 6;

/********************************************************************/

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
void CLG_FX_PlayerTeleportParticles( const Vector3 &org );