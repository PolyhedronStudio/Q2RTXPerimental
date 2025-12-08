/********************************************************************
*
*
*	ClientGame: 'New FX' FX Implementation:
*
*
********************************************************************/
#pragma once



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