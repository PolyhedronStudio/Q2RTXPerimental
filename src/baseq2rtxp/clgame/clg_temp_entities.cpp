/********************************************************************
*
*
*	ClientGame: Temp Entities.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_precache.h"
#include "clgame/clg_temp_entities.h"

#include "sharedgame/sg_tempentity_events.h"

static color_t  railcore_color;
static color_t  railspiral_color;

cvar_t *clg_railtrail_type = nullptr;
cvar_t *clg_railtrail_time = nullptr;
cvar_t *clg_railcore_color = nullptr;
cvar_t *clg_railcore_width = nullptr;
cvar_t *cl_railspiral_color = nullptr;
cvar_t *clg_railspiral_radius = nullptr;

cvar_t *cvar_pt_beam_lights = nullptr;

//cvar_t *clg_disable_particles = nullptr;
//cvar_t *clg_disable_explosions = nullptr;
cvar_t *clg_explosion_sprites = nullptr;
cvar_t *clg_explosion_frametime = nullptr;


/**
*   @brief  
**/
static void clg_timeout_changed( cvar_t *self ) {
    self->integer = 1000 * clgi.CVar_ClampValue( self, 0, 24 * 24 * 60 * 60 );
}
/**
*   @brief
**/
static void clg_railcore_color_changed( cvar_t *self ) {
    if ( !clgi.SCR_ParseColor( self->string, &railcore_color ) ) {
        Com_WPrintf( "Invalid value '%s' for '%s'\n", self->string, self->name );
        clgi.CVar_Reset( self );
        railcore_color.u32 = U32_RED;
    }
}
/**
*   @brief
**/
static void clg_railspiral_color_changed( cvar_t *self ) {
    if ( !clgi.SCR_ParseColor( self->string, &railspiral_color ) ) {
        Com_WPrintf( "Invalid value '%s' for '%s'\n", self->string, self->name );
        clgi.CVar_Reset( self );
        railspiral_color.u32 = U32_BLUE;
    }
}

//! Water Splash Color.
static const byte splash_color[] = { 0x00, 0xe0, 0xb0, 0x50, 0xd0, 0xe0, 0xe8 };

/**
*   @brief  Will select and play a random grenade explosion.
**/
static void CLG_StartRandomExplosionSfx( const bool isInWater, const Vector3 &point ) {
    if ( isInWater ) {
        clgi.S_StartSound( &point.x, 0, 0, precache.sfx.explosions.water, 1, ATTN_NORM, 0 );
        clgi.Print( PRINT_WARNING, "%f %f %f contents_water\n", point.x, point.y, point.z );
    } else {
        clgi.Print( PRINT_WARNING, "%f %f %f contents_none\n", point.x, point.y, point.z );
        // In case we are in SOLID_NONE spaces:
        const int32_t index = irandom( 2 + 1 );
        if ( index == 0 ) {
            clgi.S_StartSound( &point.x, 0, 0, precache.sfx.explosions.grenade01, 1, ATTN_NORM, 0 );
        } else {
            clgi.S_StartSound( &point.x, 0, 0, precache.sfx.explosions.grenade02, 1, ATTN_NORM, 0 );
        }
    }
}

/**
*   @brief   Parses the Temp Entity packet data and creates the appropriate effects.
**/
void CLG_TemporaryEntities_Parse( void ) {
    clg_explosion_t *ex;
    int r;

    switch ( level.parsedMessage.events.tempEntity.type ) {
    case TE_BLOOD: // bullet hitting flesh
        //if ( !( clg_disable_particles->integer & NOPART_BLOOD ) ) {
            // CLG_FX_ParticleEffect(level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xe8, 60);
        CLG_FX_BloodParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xe8, 1000 );
        //}
        break;

    case TE_GUNSHOT:            // bullet hitting wall
    case TE_SPARKS:
    case TE_BULLET_SPARKS:
        if ( level.parsedMessage.events.tempEntity.type == TE_GUNSHOT )
            CLG_FX_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0, 40 );
        else
            CLG_FX_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xe0, 6 );

        if ( level.parsedMessage.events.tempEntity.type != TE_SPARKS ) {
            // <Q2RTXP>: TODO: Bullet hit "smoke and flash" model anim.
            //CLG_FX_SmokeAndFlash( level.parsedMessage.events.tempEntity.pos1 );

            // impact sound
            r = Q_rand() & 15;
            if ( r == 1 )
                clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.ricochets.ric1, 1, ATTN_NORM, 0 );
            else if ( r == 2 )
                clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.ricochets.ric2, 1, ATTN_NORM, 0 );
            else if ( r == 3 )
                clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.ricochets.ric3, 1, ATTN_NORM, 0 );
        }
        break;

    //! A bullet hitting water.
    case TE_SPLASH:
        if ( level.parsedMessage.events.tempEntity.color < 0 || level.parsedMessage.events.tempEntity.color > 6 )
            r = 0x00;
        else
            r = splash_color[ level.parsedMessage.events.tempEntity.color ];
        CLG_FX_ParticleEffectWaterSplash( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, r, level.parsedMessage.events.tempEntity.count );
        // Never used anyhow.
        //if ( level.parsedMessage.events.tempEntity.color == SPLASH_SPARKS ) {
        //    r = Q_rand() & 3;
        //    if ( r == 0 )
        //        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.spark5, 1, ATTN_STATIC, 0 );
        //    else if ( r == 1 )
        //        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.spark6, 1, ATTN_STATIC, 0 );
        //    else
        //        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.spark7, 1, ATTN_STATIC, 0 );
        //}
        break;

    case TE_LASER_SPARKS:
        CLG_FX_ParticleEffect2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, level.parsedMessage.events.tempEntity.color, level.parsedMessage.events.tempEntity.count );
        break;

    case TE_BUBBLETRAIL:
        CLG_FX_BubbleTrail( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.pos2 );
        break;

    case TE_WELDING_SPARKS:
        CLG_FX_ParticleEffect2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, level.parsedMessage.events.tempEntity.color, level.parsedMessage.events.tempEntity.count );

        ex = CLG_AllocateExplosion();
        VectorCopy( level.parsedMessage.events.tempEntity.pos1, ex->ent.origin );
        ex->type = clg_explosion_t::ex_flash;
        // note to self
        // we need a better no draw flag
        ex->ent.flags = RF_BEAM;
        ex->start = QMTime::FromMilliseconds( clgi.client->servertime - CL_FRAMETIME );
        ex->light = 100 + ( irandom( 75 ) );
        ex->lightcolor[ 0 ] = 1.0f;
        ex->lightcolor[ 1 ] = 1.0f;
        ex->lightcolor[ 2 ] = 0.3f;
        // <Q2RTXP>: TODO: We need a model for this or what are we gonna do with this anyhow?
        //ex->ent.model = precache.models.flash;
        //ex->frames = 2;
        break;

    case TE_TUNNEL_SPARKS:
        CLG_FX_ParticleEffect3( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, level.parsedMessage.events.tempEntity.color, level.parsedMessage.events.tempEntity.count );
        break;

    case TE_DEBUGTRAIL:
        CLG_FX_DebugTrail( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.pos2 );
        break;

    case TE_PLAIN_EXPLOSION:
    {
        // Test for what solid type we're in.
        Vector3 pointPos = level.parsedMessage.events.tempEntity.pos1;
        const cm_contents_t pointContents = clgi.PointContents( &pointPos );
        // First determine whether we're actually under water.
        const bool isUnderWater = ( pointContents & CM_CONTENTMASK_LIQUID ) != 0;
        //! Do an explosion, if underwater, without smoke.
        CLG_PlainExplosion( !isUnderWater /* withSmoke == false if under water*/);
        // Handles playing the appropriate sound for the solid type found at the origin of pos1 vector.
        CLG_StartRandomExplosionSfx( isUnderWater, level.parsedMessage.events.tempEntity.pos1 );
        break;
    }

    case TE_FLASHLIGHT:
        CLG_FX_Flashlight( level.parsedMessage.events.tempEntity.entity1, level.parsedMessage.events.tempEntity.pos1 );
        break;

    case TE_HEATBEAM_SPARKS:
        CLG_FX_ParticleSteamEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0x8, 50, 60 );
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.ricochets.lashit, 1, ATTN_NORM, 0 );
        break;

    case TE_HEATBEAM_STEAM:
        CLG_FX_ParticleSteamEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xE0, 20, 60 );
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.ricochets.lashit, 1, ATTN_NORM, 0 );
        break;

    case TE_STEAM:
        CLG_ParseSteam();
        break;

    case TE_BUBBLETRAIL2:
        CLG_FX_BubbleTrail2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.pos2, 8 );
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.ricochets.lashit, 1, ATTN_NORM, 0 );
        break;

    case TE_MOREBLOOD:
        CLG_FX_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xe8, 250 );
        break;

    case TE_ELECTRIC_SPARKS:
        CLG_FX_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0x75, 40 );
        //FIXME : replace or remove this sound
        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.ricochets.lashit, 1, ATTN_NORM, 0 );
        break;

    case TE_TELEPORT_EFFECT:
        CLG_FX_TeleportParticles( level.parsedMessage.events.tempEntity.pos1 );
        break;


        //case TE_GRENADE_EXPLOSION:
        //case TE_GRENADE_EXPLOSION_WATER:
        //    ex = CLG_PlainExplosion( false );
        //    if ( !clg_explosion_sprites->integer ) {
        //        ex->frames = 19;
        //        ex->baseframe = 30;
        //    }
        //    //if ( clg_disable_explosions->integer & NOEXP_GRENADE )
        //    //    ex->type = clg_explosion_t::ex_light; // WID: C++20: Was without clg_explosion_t::

        //    //if ( !( clg_disable_particles->integer & NOPART_GRENADE_EXPLOSION ) )
        //        CLG_ExplosionParticles( level.parsedMessage.events.tempEntity.pos1 );

        //    //if ( cl_dlight_hacks->integer & DLHACK_SMALLER_EXPLOSION )
        //    //    ex->light = 200;

        //    if ( level.parsedMessage.events.tempEntity.type == TE_GRENADE_EXPLOSION_WATER ) {
        //        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.explosion_water, 1, ATTN_NORM, 0 );
        //    } else {
        //        //clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.explosion_grenade01, 1, ATTN_NORM, 0 );
        //        CLG_StartRandomExplosionSfx();
        //    }
        //    break;

        //case TE_EXPLOSION2:
        //    ex = CLG_PlainExplosion( false );
        //    if ( !clg_explosion_sprites->integer ) {
        //        ex->frames = 19;
        //        ex->baseframe = 30;
        //    }
        //    CLG_ExplosionParticles( level.parsedMessage.events.tempEntity.pos1 );
        //    CLG_StartRandomExplosionSfx(); //clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.grenexp, 1, ATTN_NORM, 0 );
        //    break;

        //case TE_ROCKET_EXPLOSION:
        //case TE_ROCKET_EXPLOSION_WATER:
        //    ex = CLG_PlainExplosion( false );
        //    //if ( clg_disable_explosions->integer & NOEXP_ROCKET )
        //    //    ex->type = clg_explosion_t::ex_light; // WID: C++20: This was without clg_explosion_t::

        //    //if ( !( clg_disable_particles->integer & NOPART_ROCKET_EXPLOSION ) )
        //        CLG_ExplosionParticles( level.parsedMessage.events.tempEntity.pos1 );

        //    //if ( cl_dlight_hacks->integer & DLHACK_SMALLER_EXPLOSION )
        //    //    ex->light = 200;

        //    if ( level.parsedMessage.events.tempEntity.type == TE_ROCKET_EXPLOSION_WATER )
        //        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.explosion_water, 1, ATTN_NORM, 0 );
        //    else
        //        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.explosion_rocket, 1, ATTN_NORM, 0 );
        //    break;

        //case TE_EXPLOSION1:
        //    CLG_PlainExplosion( false );
        //    CLG_ExplosionParticles( level.parsedMessage.events.tempEntity.pos1 );
        //    clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.explosion_rocket, 1, ATTN_NORM, 0 );
        //    break;

        //case TE_EXPLOSION1_NP:
        //    CLG_PlainExplosion( false );
        //    clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.explosion_rocket, 1, ATTN_NORM, 0 );
        //    break;

        //case TE_EXPLOSION1_BIG:
        //    ex = CLG_PlainExplosion( true );
        //    clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.explosion_rocket, 1, ATTN_NORM, 0 );
        //    break;

        //case TE_GRAPPLE_CABLE:
        //    level.parsedMessage.events.tempEntity.entity2 = 0;
        //    CLG_ParseBeam( precache.models.grapple_cable );
        //    break;
    //case TE_FORCEWALL:
    //    CLG_ForceWall( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.pos2, level.parsedMessage.events.tempEntity.color );
    //    break;
    // 
    //case TE_NUKEBLAST:
    //    CLG_ParseNuke();
    //    break;

        //case TE_SCREEN_SPARKS:
        //case TE_SHIELD_SPARKS:
        //    if ( level.parsedMessage.events.tempEntity.type == TE_SCREEN_SPARKS )
        //        CLG_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xd0, 40 );
        //    else
        //        CLG_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xb0, 40 );
        //    //FIXME : replace or remove this sound
        //    clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 257, precache.sfx.lashit, 1, ATTN_NORM, 0 );
        //    break;

        //case TE_SHOTGUN:            // bullet hitting wall
        //    CLG_ParticleEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0, 20 );
        //    CLG_SmokeAndFlash( level.parsedMessage.events.tempEntity.pos1 );
        //    break;


        //case TE_BLUEHYPERBLASTER:
        //    CLG_BlasterParticles( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir );
        //    break;

        //case TE_BLASTER:            // blaster hitting wall
        //case TE_BLASTER2:           // green blaster hitting wall
        //case TE_FLECHETTE:          // flechette
        //case TE_FLARE:              // flare
        //    ex = CLG_AllocateExplosion();
        //    VectorCopy( level.parsedMessage.events.tempEntity.pos1, ex->ent.origin );
        //    QM_Vector3ToAngles( Vector3( 0, 1.f, 0.f ), ex->ent.angles ); //dirtoangles( ex->ent.angles );
        //    ex->type = clg_explosion_t::ex_blaster;
        //    ex->ent.flags = RF_FULLBRIGHT | RF_TRANSLUCENT;
        //    ex->ent.tent_type = level.parsedMessage.events.tempEntity.type;
        //    switch ( level.parsedMessage.events.tempEntity.type ) {
        //    case TE_BLASTER:
        //        CLG_BlasterParticles( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir );
        //        ex->lightcolor[ 0 ] = 1;
        //        ex->lightcolor[ 1 ] = 1;
        //        break;
        //    case TE_BLASTER2:
        //        CLG_BlasterParticles2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xd0 );
        //        ex->ent.skinnum = 1;
        //        ex->lightcolor[ 1 ] = 1;
        //        break;
        //    case TE_FLECHETTE:
        //        CLG_BlasterParticles2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0x6f );  // 75
        //        ex->ent.skinnum = 2;
        //        ex->lightcolor[ 0 ] = 0.19f;
        //        ex->lightcolor[ 1 ] = 0.41f;
        //        ex->lightcolor[ 2 ] = 0.75f;
        //        break;
        //    case TE_FLARE:
        //        CLG_BlasterParticles2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xd0 );
        //        ex->lightcolor[ 0 ] = 1;
        //        ex->lightcolor[ 1 ] = 1;
        //        ex->type = clg_explosion_t::ex_flare;
        //        break;
        //    }
        //    ex->start = clgi.client->servertime - clgi.frame_time_ms;
        //    ex->light = 150;
        //    ex->ent.model = precache.models.explode;
        //    ex->frames = 4;

        //    if ( level.parsedMessage.events.tempEntity.type != TE_FLARE ) {
        //        clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.lashit, 1, ATTN_NORM, 0 );
        //    } else {
        //        // level.parsedMessage.events.tempEntity.count is set to 1 on the first tick of the flare, 0 afterwards
        //        if ( level.parsedMessage.events.tempEntity.count != 0 )
        //            clgi.S_StartSound( NULL, level.parsedMessage.events.tempEntity.entity1, 0, precache.sfx.flare, 0.5, ATTN_NORM, 0 );
        //    }
        //    break;

        //case TE_RAILTRAIL:          // railgun effect
        //    CLG_RailTrail();
        //    clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos2, 0, 0, precache.sfx.railg, 1, ATTN_NORM, 0 );
        //    break;


        //case TE_PLASMA_EXPLOSION:
        //    CLG_PlainExplosion( false );
        //    CLG_ExplosionParticles( level.parsedMessage.events.tempEntity.pos1 );
        //    clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, precache.sfx.rockexp, 1, ATTN_NORM, 0 );
        //    break;


        //case TE_BFG_EXPLOSION:
        //    ex = CLG_AllocateExplosion();
        //    VectorCopy( level.parsedMessage.events.tempEntity.pos1, ex->ent.origin );
        //    ex->type = clg_explosion_t::ex_polygon_curvature;
        //    ex->ent.flags = RF_FULLBRIGHT;
        //    ex->start = clgi.client->servertime - clgi.frame_time_ms;
        //    ex->light = 350;
        //    ex->lightcolor[ 0 ] = 0.0f;
        //    ex->lightcolor[ 1 ] = 1.0f;
        //    ex->lightcolor[ 2 ] = 0.0f;
        //    ex->ent.model = precache.models.bfg_explo;
        //    ex->ent.flags |= RF_TRANSLUCENT;
        //    ex->ent.alpha = 0.80;
        //    ex->frames = 4;
        //    break;

        //case TE_BFG_BIGEXPLOSION:
        //    CLG_BFGExplosionParticles( level.parsedMessage.events.tempEntity.pos1 );
        //    break;

        //case TE_BFG_LASER:
        //    CLG_ParseLaser( 0xd0d1d2d3 );
        //    break;

        //case TE_PARASITE_ATTACK:
        //case TE_MEDIC_CABLE_ATTACK:
        //    VectorClear( level.parsedMessage.events.tempEntity.offset );
        //    level.parsedMessage.events.tempEntity.entity2 = 0;
        //    CLG_ParseBeam( precache.models.parasite_segment );
        //    break;

        //case TE_BOSSTPORT:          // boss teleporting to station
        //    CLG_BigTeleportParticles( level.parsedMessage.events.tempEntity.pos1 );
        //    clgi.S_StartSound( level.parsedMessage.events.tempEntity.pos1, 0, 0, clgi.S_RegisterSound( "misc/bigtele.wav" ), 1, ATTN_NONE, 0 );
        //    break;
        //case TE_GREENBLOOD:
        //    CLG_ParticleEffect2( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0xdf, 30 );
        //    break;

        //case TE_LIGHTNING:
        //    clgi.S_StartSound( NULL, level.parsedMessage.events.tempEntity.entity1, CHAN_WEAPON, precache.sfx.lightning, 1, ATTN_NORM, 0 );
        //    VectorClear( level.parsedMessage.events.tempEntity.offset );
        //    CLG_ParseBeam( precache.models.lightning );
        //    break;

    //case TE_HEATBEAM:
    //    VectorSet( level.parsedMessage.events.tempEntity.offset, 2, 7, -3 );
    //    CLG_ParsePlayerBeam( precache.models.heatbeam );
    //    break;

    //case TE_MONSTER_HEATBEAM:
    //    VectorClear( level.parsedMessage.events.tempEntity.offset );
    //    CLG_ParsePlayerBeam( precache.models.heatbeam );
    //    break;

    //case TE_CHAINFIST_SMOKE:
    //    VectorSet( level.parsedMessage.events.tempEntity.dir, 0, 0, 1 );
    //    CLG_ParticleSmokeEffect( level.parsedMessage.events.tempEntity.pos1, level.parsedMessage.events.tempEntity.dir, 0, 20, 20 );
    //    break;

    //case TE_WIDOWBEAMOUT:
    //    CLG_ParseWidow();
    //    break;


    //case TE_WIDOWSPLASH:
    //    CLG_WidowSplash();
    //    break;

    default:
        Com_Error( ERR_DROP, "%s: unknown temp entity type(ID: #%d)", __func__, level.parsedMessage.events.tempEntity.type );
    }
}

/**
*   @brief  Add all temporary entity effects for the current frame.
**/
void CLG_TemporaryEntities_Add( void ) {
    CLG_AddBeams();
    CLG_AddPlayerBeams();
    CLG_AddExplosions();
    CLG_ProcessSustain();
    CLG_AddLasers();
}

/**
*   @brief  Clear all temporary entities for the current frame.
**/
void CLG_TemporaryEntities_Clear( void ) {
    CLG_ClearBeams();
    CLG_ClearExplosions();
    CLG_ClearLasers();
    CLG_ClearSustains();
}

/**
*   @brief  Support routine for the rail core and spiral color cvar suggestion generator.
**/
void TE_Color_g( genctx_t *ctx ) {
    for ( int32_t color = /*0*/COLOR_BLACK; color < COLOR_ALT; color++ ) {
        clgi.Prompt_AddMatch( ctx, clgi.SCR_GetColorName( (color_index_t)color ) );
    }
}

/**
*   @brief	Initialize the temporary entities system.
**/
void CLG_TemporaryEntities_Init( void ) {
    clg_railtrail_type = clgi.CVar_Get( "clg_railtrail_type", "0", 0 );
    clg_railtrail_time = clgi.CVar_Get( "clg_railtrail_time", "1.0", 0 );
    clg_railtrail_time->changed = clg_timeout_changed;
    clg_railtrail_time->changed( clg_railtrail_time );
    clg_railcore_color = clgi.CVar_Get( "clg_railcore_color", "red", 0 );
    clg_railcore_color->changed = clg_railcore_color_changed;
    clg_railcore_color->generator = TE_Color_g;
    clg_railcore_color_changed( clg_railcore_color );
    clg_railcore_width = clgi.CVar_Get( "clg_railcore_width", "2", 0 );
    cl_railspiral_color = clgi.CVar_Get( "cl_railspiral_color", "blue", 0 );
    cl_railspiral_color->changed = clg_railspiral_color_changed;
    cl_railspiral_color->generator = TE_Color_g;
    clg_railspiral_color_changed( cl_railspiral_color );
    clg_railspiral_radius = clgi.CVar_Get( "clg_railspiral_radius", "3", 0 );

	// Fetched by engine for beam light effects on temp entities. (Created in renderer.)
    cvar_pt_beam_lights = clgi.CVar_Get( "pt_beam_lights", nullptr, 0 );

    //clg_disable_particles = clgi.CVar_Get( "clg_disable_particles", "0", 0 );
    //clg_disable_explosions = clgi.CVar_Get( "clg_disable_explosions", "0", 0 );
    clg_explosion_sprites = clgi.CVar_Get( "clg_explosion_sprites", "1", 0 );
    clg_explosion_frametime = clgi.CVar_Get( "clg_explosion_frametime", std::to_string(50).c_str(), 0);
}