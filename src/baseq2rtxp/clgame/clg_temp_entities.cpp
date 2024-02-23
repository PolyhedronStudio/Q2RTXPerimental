/********************************************************************
*
*
*	ClientGame: Temp Entities.
*
*
********************************************************************/
#include "clg_local.h"

static color_t  railcore_color;
static color_t  railspiral_color;

cvar_t *cl_railtrail_type = nullptr;
cvar_t *cl_railtrail_time = nullptr;
cvar_t *cl_railcore_color = nullptr;
cvar_t *cl_railcore_width = nullptr;
cvar_t *cl_railspiral_color = nullptr;
cvar_t *cl_railspiral_radius = nullptr;

cvar_t *cvar_pt_beam_lights = nullptr;

cvar_t *cl_disable_particles = nullptr;
cvar_t *cl_disable_explosions = nullptr;
cvar_t *cl_explosion_sprites = nullptr;
cvar_t *cl_explosion_frametime = nullptr;
cvar_t *cl_dlight_hacks = nullptr;


/**
*   @brief  
**/
static void cl_timeout_changed( cvar_t *self ) {
    self->integer = 1000 * clgi.CVar_ClampValue( self, 0, 24 * 24 * 60 * 60 );
}
/**
*   @brief
**/
static void cl_railcore_color_changed( cvar_t *self ) {
    if ( !clgi.SCR_ParseColor( self->string, &railcore_color ) ) {
        Com_WPrintf( "Invalid value '%s' for '%s'\n", self->string, self->name );
        clgi.CVar_Reset( self );
        railcore_color.u32 = U32_RED;
    }
}
/**
*   @brief
**/
static void cl_railspiral_color_changed( cvar_t *self ) {
    if ( !clgi.SCR_ParseColor( self->string, &railspiral_color ) ) {
        Com_WPrintf( "Invalid value '%s' for '%s'\n", self->string, self->name );
        clgi.CVar_Reset( self );
        railspiral_color.u32 = U32_BLUE;
    }
}

//!
static const byte splash_color[] = { 0x00, 0xe0, 0xb0, 0x50, 0xd0, 0xe0, 0xe8 };

//qhandle_t   precache.cl_sfx_ric1;
//qhandle_t   precache.cl_sfx_ric2;
//qhandle_t   precache.cl_sfx_ric3;
//qhandle_t   precache.cl_sfx_lashit;
//qhandle_t   precache.cl_sfx_flare;
//qhandle_t   precache.cl_sfx_spark5;
//qhandle_t   precache.cl_sfx_spark6;
//qhandle_t   precache.cl_sfx_spark7;
//qhandle_t   precache.cl_sfx_railg;
//qhandle_t   precache.cl_sfx_rockexp;
//qhandle_t   precache.cl_sfx_grenexp;
//qhandle_t   precache.cl_sfx_watrexp;
//qhandle_t   precache.cl_sfx_footsteps[ 4 ];
//
//qhandle_t   precache.cl_sfx_lightning;
//qhandle_t   precache.cl_sfx_disrexp;
//
//qhandle_t   precache.cl_mod_explode;
//qhandle_t   precache.cl_mod_smoke;
//qhandle_t   precache.cl_mod_flash;
//qhandle_t   precache.cl_mod_parasite_segment;
//qhandle_t   precache.cl_mod_grapple_cable;
//qhandle_t   precache.cl_mod_explo4;
//qhandle_t   precache.cl_mod_bfg_explo;
//qhandle_t   precache.cl_mod_powerscreen;
//qhandle_t   precache.cl_mod_laser;
//qhandle_t   precache.cl_mod_dmspot;
//qhandle_t   precache.cl_mod_explosions[ 4 ];
//
//qhandle_t   precache.cl_mod_lightning;
//qhandle_t   precache.cl_mod_heatbeam;
//qhandle_t   precache.cl_mod_explo4_big;


/**
*   @brief   
**/
void CLG_ParseTEnt( void ) {
    explosion_t *ex;
    int r;

    switch ( level.parsedMessage.events.tempEntity.type ) {
    case TE_BLOOD:          // bullet hitting flesh
        if ( !( cl_disable_particles->integer & NOPART_BLOOD ) ) {
            // CLG_ParticleEffect(level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xe8, 60);
            CLG_BloodParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xe8, 1000 );
        }
        break;

    case TE_GUNSHOT:            // bullet hitting wall
    case TE_SPARKS:
    case TE_BULLET_SPARKS:
        if ( level.parsedMessage.events.tempEntity.type == TE_GUNSHOT )
            CLG_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0, 40 );
        else
            CLG_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xe0, 6 );

        if ( level.parsedMessage.events.tempEntity.type != TE_SPARKS ) {
            CLG_SmokeAndFlash( level.parsedMessage.events.tempEntity.pos1 );

            // impact sound
            r = Q_rand() & 15;
            if ( r == 1 )
                clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_ric1, 1, ATTN_NORM, 0 );
            else if ( r == 2 )
                clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_ric2, 1, ATTN_NORM, 0 );
            else if ( r == 3 )
                clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_ric3, 1, ATTN_NORM, 0 );
        }
        break;

    case TE_SCREEN_SPARKS:
    case TE_SHIELD_SPARKS:
        if ( level.parsedMessage.events.tempEntity.type == TE_SCREEN_SPARKS )
            CLG_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xd0, 40 );
        else
            CLG_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xb0, 40 );
        //FIXME : replace or remove this sound
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 257, precache.cl_sfx_lashit, 1, ATTN_NORM, 0 );
        break;

    case TE_SHOTGUN:            // bullet hitting wall
        CLG_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0, 20 );
        CLG_SmokeAndFlash( level.parsedMessage.events.tempEntity.pos1 );
        break;

    case TE_SPLASH:         // bullet hitting water
        if ( level.parsedMessage.events.tempEntity.color < 0 || level.parsedMessage.events.tempEntity.color > 6 )
            r = 0x00;
        else
            r = splash_color[ level.parsedMessage.events.tempEntity.color ];
        CLG_ParticleEffectWaterSplash( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, r, level.parsedMessage.events.tempEntity.count );

        if ( level.parsedMessage.events.tempEntity.color == SPLASH_SPARKS ) {
            r = Q_rand() & 3;
            if ( r == 0 )
                clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_spark5, 1, ATTN_STATIC, 0 );
            else if ( r == 1 )
                clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_spark6, 1, ATTN_STATIC, 0 );
            else
                clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_spark7, 1, ATTN_STATIC, 0 );
        }
        break;

    case TE_LASER_SPARKS:
        CLG_ParticleEffect2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, level.parsedMessage.events.tempEntity.color, level.parsedMessage.events.tempEntity.count );
        break;

    case TE_BLUEHYPERBLASTER:
        CLG_BlasterParticles( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir );
        break;

    case TE_BLASTER:            // blaster hitting wall
    case TE_BLASTER2:           // green blaster hitting wall
    case TE_FLECHETTE:          // flechette
    case TE_FLARE:              // flare
        ex = CLG_AllocExplosion();
        VectorCopy( level.parsedMessage.events.tempEntity.pos1, ex->ent.origin );
        QM_Vector3ToAngles( Vector3( 0, 1.f, 0.f ), ex->ent.angles ); //dirtoangles( ex->ent.angles );
        ex->type = explosion_t::ex_blaster;
        ex->ent.flags = RF_FULLBRIGHT | RF_TRANSLUCENT;
        ex->ent.tent_type = level.parsedMessage.events.tempEntity.type;
        switch ( level.parsedMessage.events.tempEntity.type ) {
        case TE_BLASTER:
            CLG_BlasterParticles( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir );
            ex->lightcolor[ 0 ] = 1;
            ex->lightcolor[ 1 ] = 1;
            break;
        case TE_BLASTER2:
            CLG_BlasterParticles2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xd0 );
            ex->ent.skinnum = 1;
            ex->lightcolor[ 1 ] = 1;
            break;
        case TE_FLECHETTE:
            CLG_BlasterParticles2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0x6f );  // 75
            ex->ent.skinnum = 2;
            ex->lightcolor[ 0 ] = 0.19f;
            ex->lightcolor[ 1 ] = 0.41f;
            ex->lightcolor[ 2 ] = 0.75f;
            break;
        case TE_FLARE:
            CLG_BlasterParticles2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xd0 );
            ex->lightcolor[ 0 ] = 1;
            ex->lightcolor[ 1 ] = 1;
            ex->type = explosion_t::ex_flare;
            break;
        }
        ex->start = clgi.client->servertime - clgi.frame_time_ms;
        ex->light = 150;
        ex->ent.model = precache.cl_mod_explode;
        ex->frames = 4;

        if ( level.parsedMessage.events.tempEntity.type != TE_FLARE ) {
            clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_lashit, 1, ATTN_NORM, 0 );
        } else {
            // level.parsedMessage.events.tempEntity.count is set to 1 on the first tick of the flare, 0 afterwards
            if ( level.parsedMessage.events.tempEntity.count != 0 )
                clgi.S_StartSound( NULL, level.parsedMessage.events.tempEntity.entity1, 0, precache.cl_sfx_flare, 0.5, ATTN_NORM, 0 );
        }
        break;

    case TE_RAILTRAIL:          // railgun effect
        CLG_RailTrail();
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos2, 0, 0, precache.cl_sfx_railg, 1, ATTN_NORM, 0 );
        break;

    case TE_GRENADE_EXPLOSION:
    case TE_GRENADE_EXPLOSION_WATER:
        ex = CLG_PlainExplosion( false );
        if ( !cl_explosion_sprites->integer ) {
            ex->frames = 19;
            ex->baseframe = 30;
        }
        if ( cl_disable_explosions->integer & NOEXP_GRENADE )
            ex->type = explosion_t::ex_light; // WID: C++20: Was without explosion_t::

        if ( !( cl_disable_particles->integer & NOPART_GRENADE_EXPLOSION ) )
            CLG_ExplosionParticles( level.parsedMessage.events.tempEntity.pos1 );

        if ( cl_dlight_hacks->integer & DLHACK_SMALLER_EXPLOSION )
            ex->light = 200;

        if ( level.parsedMessage.events.tempEntity.type == TE_GRENADE_EXPLOSION_WATER )
            clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_watrexp, 1, ATTN_NORM, 0 );
        else
            clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_grenexp, 1, ATTN_NORM, 0 );
        break;

    case TE_EXPLOSION2:
        ex = CLG_PlainExplosion( false );
        if ( !cl_explosion_sprites->integer ) {
            ex->frames = 19;
            ex->baseframe = 30;
        }
        CLG_ExplosionParticles( level.parsedMessage.events.tempEntity.pos1 );
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_grenexp, 1, ATTN_NORM, 0 );
        break;

    case TE_PLASMA_EXPLOSION:
        CLG_PlainExplosion( false );
        CLG_ExplosionParticles( level.parsedMessage.events.tempEntity.pos1 );
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_rockexp, 1, ATTN_NORM, 0 );
        break;

    case TE_ROCKET_EXPLOSION:
    case TE_ROCKET_EXPLOSION_WATER:
        ex = CLG_PlainExplosion( false );
        if ( cl_disable_explosions->integer & NOEXP_ROCKET )
            ex->type = explosion_t::ex_light; // WID: C++20: This was without explosion_t::

        if ( !( cl_disable_particles->integer & NOPART_ROCKET_EXPLOSION ) )
            CLG_ExplosionParticles( level.parsedMessage.events.tempEntity.pos1 );

        if ( cl_dlight_hacks->integer & DLHACK_SMALLER_EXPLOSION )
            ex->light = 200;

        if ( level.parsedMessage.events.tempEntity.type == TE_ROCKET_EXPLOSION_WATER )
            clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_watrexp, 1, ATTN_NORM, 0 );
        else
            clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_rockexp, 1, ATTN_NORM, 0 );
        break;

    case TE_EXPLOSION1:
        CLG_PlainExplosion( false );
        CLG_ExplosionParticles( level.parsedMessage.events.tempEntity.pos1 );
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_rockexp, 1, ATTN_NORM, 0 );
        break;

    case TE_EXPLOSION1_NP:
        CLG_PlainExplosion( false );
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_rockexp, 1, ATTN_NORM, 0 );
        break;

    case TE_EXPLOSION1_BIG:
        ex = CLG_PlainExplosion( true );
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_rockexp, 1, ATTN_NORM, 0 );
        break;

    case TE_BFG_EXPLOSION:
        ex = CLG_AllocExplosion();
        VectorCopy( level.parsedMessage.events.tempEntity.pos1, ex->ent.origin );
        ex->type = explosion_t::ex_poly;
        ex->ent.flags = RF_FULLBRIGHT;
        ex->start = clgi.client->servertime - clgi.frame_time_ms;
        ex->light = 350;
        ex->lightcolor[ 0 ] = 0.0f;
        ex->lightcolor[ 1 ] = 1.0f;
        ex->lightcolor[ 2 ] = 0.0f;
        ex->ent.model = precache.cl_mod_bfg_explo;
        ex->ent.flags |= RF_TRANSLUCENT;
        ex->ent.alpha = 0.80;
        ex->frames = 4;
        break;

    case TE_BFG_BIGEXPLOSION:
        CLG_BFGExplosionParticles( level.parsedMessage.events.tempEntity.pos1 );
        break;

    case TE_BFG_LASER:
        CLG_ParseLaser( 0xd0d1d2d3 );
        break;

    case TE_BUBBLETRAIL:
        CLG_BubbleTrail( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.pos2 );
        break;

    case TE_PARASITE_ATTACK:
    case TE_MEDIC_CABLE_ATTACK:
        VectorClear( level.parsedMessage.events.tempEntity.offset );
        level.parsedMessage.events.tempEntity.entity2 = 0;
        CLG_ParseBeam( precache.cl_mod_parasite_segment );
        break;

    case TE_BOSSTPORT:          // boss teleporting to station
        CLG_BigTeleportParticles( level.parsedMessage.events.tempEntity.pos1 );
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, clgi.S_RegisterSound( "misc/bigtele.wav" ), 1, ATTN_NONE, 0 );
        break;

    case TE_GRAPPLE_CABLE:
        level.parsedMessage.events.tempEntity.entity2 = 0;
        CLG_ParseBeam( precache.cl_mod_grapple_cable );
        break;

    case TE_WELDING_SPARKS:
        CLG_ParticleEffect2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, level.parsedMessage.events.tempEntity.color, level.parsedMessage.events.tempEntity.count );

        ex = CLG_AllocExplosion();
        VectorCopy( level.parsedMessage.events.tempEntity.pos1, ex->ent.origin );
        ex->type = explosion_t::ex_flash;
        // note to self
        // we need a better no draw flag
        ex->ent.flags = RF_BEAM;
        ex->start = clgi.client->servertime - CL_FRAMETIME;
        ex->light = 100 + ( irandom( 75 ) );
        ex->lightcolor[ 0 ] = 1.0f;
        ex->lightcolor[ 1 ] = 1.0f;
        ex->lightcolor[ 2 ] = 0.3f;
        ex->ent.model = precache.cl_mod_flash;
        ex->frames = 2;
        break;

    case TE_GREENBLOOD:
        CLG_ParticleEffect2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xdf, 30 );
        break;

    case TE_TUNNEL_SPARKS:
        CLG_ParticleEffect3( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, level.parsedMessage.events.tempEntity.color, level.parsedMessage.events.tempEntity.count );
        break;

    case TE_LIGHTNING:
        clgi.S_StartSound( NULL, level.parsedMessage.events.tempEntity.entity1, CHAN_WEAPON, precache.cl_sfx_lightning, 1, ATTN_NORM, 0 );
        VectorClear( level.parsedMessage.events.tempEntity.offset );
        CLG_ParseBeam( precache.cl_mod_lightning );
        break;

    case TE_DEBUGTRAIL:
        CLG_DebugTrail( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.pos2 );
        break;

    case TE_PLAIN_EXPLOSION:
        CLG_PlainExplosion( false );
        break;

    case TE_FLASHLIGHT:
        CLG_Flashlight( level.parsedMessage.events.tempEntity.entity1, level.parsedMessage.events.tempEntity.pos1 );
        break;

    case TE_FORCEWALL:
        CLG_ForceWall( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.pos2, level.parsedMessage.events.tempEntity.color );
        break;

    case TE_HEATBEAM:
        VectorSet( level.parsedMessage.events.tempEntity.offset, 2, 7, -3 );
        CLG_ParsePlayerBeam( precache.cl_mod_heatbeam );
        break;

    case TE_MONSTER_HEATBEAM:
        VectorClear( level.parsedMessage.events.tempEntity.offset );
        CLG_ParsePlayerBeam( precache.cl_mod_heatbeam );
        break;

    case TE_HEATBEAM_SPARKS:
        CLG_ParticleSteamEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0x8, 50, 60 );
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_lashit, 1, ATTN_NORM, 0 );
        break;

    case TE_HEATBEAM_STEAM:
        CLG_ParticleSteamEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xE0, 20, 60 );
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_lashit, 1, ATTN_NORM, 0 );
        break;

    case TE_STEAM:
        CLG_ParseSteam();
        break;

    case TE_BUBBLETRAIL2:
        CLG_BubbleTrail2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.pos2, 8 );
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_lashit, 1, ATTN_NORM, 0 );
        break;

    case TE_MOREBLOOD:
        CLG_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xe8, 250 );
        break;

    case TE_CHAINFIST_SMOKE:
        VectorSet( level.parsedMessage.events.tempEntity.dir, 0, 0, 1 );
        CLG_ParticleSmokeEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0, 20, 20 );
        break;

    case TE_ELECTRIC_SPARKS:
        CLG_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0x75, 40 );
        //FIXME : replace or remove this sound
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_lashit, 1, ATTN_NORM, 0 );
        break;

    case TE_TRACKER_EXPLOSION:
        CLG_ColorFlash( level.parsedMessage.events.tempEntity.pos1, 0, 150, -1, -1, -1 );
        CLG_ColorExplosionParticles( level.parsedMessage.events.tempEntity.pos1, 0, 1 );
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.cl_sfx_disrexp, 1, ATTN_NORM, 0 );
        break;

    case TE_TELEPORT_EFFECT:
    case TE_DBALL_GOAL:
        CLG_TeleportParticles( level.parsedMessage.events.tempEntity.pos1 );
        break;

    case TE_WIDOWBEAMOUT:
        CLG_ParseWidow();
        break;

    case TE_NUKEBLAST:
        CLG_ParseNuke();
        break;

    case TE_WIDOWSPLASH:
        CLG_WidowSplash();
        break;

    default:
        Com_Error( ERR_DROP, "%s: bad type", __func__ );
    }
}

/**
*   @brief
**/
void CLG_AddTEnts( void ) {
    CLG_AddBeams();
    CLG_AddPlayerBeams();
    CLG_AddExplosions();
    CLG_ProcessSustain();
    CLG_AddLasers();
}

/**
*   @brief
**/
void CLG_ClearTEnts( void ) {
    CLG_ClearBeams();
    CLG_ClearExplosions();
    CLG_ClearLasers();
    CLG_ClearSustains();
}

/**
*   @brief  
**/
void TE_Color_g( genctx_t *ctx ) {
    for ( int32_t color = /*0*/COLOR_BLACK; color < COLOR_ALT; color++ ) {
        clgi.Prompt_AddMatch( ctx, clgi.SCR_GetColorName( (color_index_t)color ) );
    }
}

void CLG_InitTEnts( void ) {
    cl_railtrail_type = clgi.CVar_Get( "cl_railtrail_type", "0", 0 );
    cl_railtrail_time = clgi.CVar_Get( "cl_railtrail_time", "1.0", 0 );
    cl_railtrail_time->changed = cl_timeout_changed;
    cl_railtrail_time->changed( cl_railtrail_time );
    cl_railcore_color = clgi.CVar_Get( "cl_railcore_color", "red", 0 );
    cl_railcore_color->changed = cl_railcore_color_changed;
    cl_railcore_color->generator = TE_Color_g;
    cl_railcore_color_changed( cl_railcore_color );
    cl_railcore_width = clgi.CVar_Get( "cl_railcore_width", "2", 0 );
    cl_railspiral_color = clgi.CVar_Get( "cl_railspiral_color", "blue", 0 );
    cl_railspiral_color->changed = cl_railspiral_color_changed;
    cl_railspiral_color->generator = TE_Color_g;
    cl_railspiral_color_changed( cl_railspiral_color );
    cl_railspiral_radius = clgi.CVar_Get( "cl_railspiral_radius", "3", 0 );

    cvar_pt_beam_lights = clgi.CVar_Get( "pt_beam_lights", nullptr, 0 );

    cl_disable_particles = clgi.CVar_Get( "cl_disable_particles", "0", 0 );
    cl_disable_explosions = clgi.CVar_Get( "cl_disable_explosions", "0", 0 );
    cl_explosion_sprites = clgi.CVar_Get( "cl_explosion_sprites", "1", 0 );
    cl_explosion_frametime = clgi.CVar_Get( "cl_explosion_frametime", std::to_string(clgi.frame_time_ms).c_str(), 0);
    cl_dlight_hacks = clgi.CVar_Get( "cl_dlight_hacks", "0", 0 );
}