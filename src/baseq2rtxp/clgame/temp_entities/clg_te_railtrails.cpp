/********************************************************************
*
*
*	ClientGame: Rail Trails.
*
*
********************************************************************/
#include "../clg_local.h"

static color_t  railcore_color;
static color_t  railspiral_color;

extern cvar_t *cl_railtrail_type;
extern cvar_t *cl_railtrail_time;
extern cvar_t *cl_railcore_color;
extern cvar_t *cl_railcore_width;
extern cvar_t *cl_railspiral_color;
extern cvar_t *cl_railspiral_radius;

extern cvar_t *cvar_pt_beam_lights;

static void clg_railcore_color_changed( cvar_t *self ) {
    if ( !clgi.SCR_ParseColor( self->string, &railcore_color ) ) {
        clgi.Print( PRINT_WARNING, "Invalid value '%s' for '%s'\n", self->string, self->name );
        clgi.CVar_Reset( self );
        railcore_color.u32 = U32_RED;
    }
}

static void clg_railspiral_color_changed( cvar_t *self ) {
    if ( !clgi.SCR_ParseColor( self->string, &railspiral_color ) ) {
        clgi.Print(PRINT_WARNING, "Invalid value '%s' for '%s'\n", self->string, self->name );
        clgi.CVar_Reset( self );
        railspiral_color.u32 = U32_BLUE;
    }
}

void CLG_RailCore( void ) {
    laser_t *l;

    l = CLG_AllocLaser();
    if ( !l )
        return;

    VectorCopy( level.parsedMessage.events.tempEntity.pos1, l->start );
    VectorCopy( level.parsedMessage.events.tempEntity.pos2, l->end );
    l->color = -1;
    l->lifetime = cl_railtrail_time->integer;
    l->width = cl_railcore_width->integer;
    l->rgba.u32 = railcore_color.u32;
}

void CLG_RailSpiral( void ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    cparticle_t *p;
    vec3_t      right, up;
    int         i;
    float       d, c, s;
    vec3_t      dir;

    VectorCopy( level.parsedMessage.events.tempEntity.pos1, move );
    VectorSubtract( level.parsedMessage.events.tempEntity.pos2, level.parsedMessage.events.tempEntity.pos1, vec );
    len = VectorNormalize( vec );

    MakeNormalVectors( vec, right, up );

    for ( i = 0; i < len; i++ ) {
        p = CLG_AllocParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;
        VectorClear( p->accel );

        d = i * 0.1f;
        c = cos( d );
        s = sin( d );

        VectorScale( right, c, dir );
        VectorMA( dir, s, up, dir );

        p->alpha = 1.0f;
        p->alphavel = -1.0f / ( cl_railtrail_time->value + frand() * 0.2f );
        p->color = -1;
        p->rgba.u32 = railspiral_color.u32;
        p->brightness = cvar_pt_particle_emissive->value;
        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ] + dir[ j ] * cl_railspiral_radius->value;
            p->vel[ j ] = dir[ j ] * 6;
        }

        VectorAdd( move, vec, move );
    }
}

void CLG_RailLights( const color_t color ) {
    vec3_t fcolor;
    fcolor[ 0 ] = (float)color.u8[ 0 ] / 255.f;
    fcolor[ 1 ] = (float)color.u8[ 1 ] / 255.f;
    fcolor[ 2 ] = (float)color.u8[ 2 ] / 255.f;

    vec3_t      move;
    vec3_t      vec;
    float       len;

    VectorCopy( level.parsedMessage.events.tempEntity.pos1, move );
    VectorSubtract( level.parsedMessage.events.tempEntity.pos2, level.parsedMessage.events.tempEntity.pos1, vec );
    len = VectorNormalize( vec );

    float num_segments = ceilf( len / 100.f );
    float segment_size = len / num_segments;

    for ( float segment = 0; segment < num_segments; segment++ ) {
        float offset = ( segment + 0.25f ) * segment_size;
        vec3_t pos;
        VectorMA( move, offset, vec, pos );

        cdlight_t *dl = CLG_AllocDlight( 0 );
        VectorScale( fcolor, 0.25f, dl->color );
        VectorCopy( pos, dl->origin );
        dl->radius = 400;
        dl->decay = 400;
        dl->die = clgi.client->time + 1000;
        VectorScale( vec, segment_size * 0.5f, dl->velosity );
    }
}


// WID: C++20: Needed for linkage.
extern cvar_t *cvar_pt_beam_lights;

void CLG_RailTrail( void ) {
    color_t rail_color;
    const uint32_t *d_8to24table = clgi.R_Get8BitTo24BitTable();

    if ( !cl_railtrail_type->integer ) {
        rail_color.u32 = d_8to24table[ 0x74 ];

        CLG_OldRailTrail();
    } else {
        rail_color = railcore_color;

        if ( cl_railcore_width->integer > 0 ) {
            CLG_RailCore();
        }
        if ( cl_railtrail_type->integer > 1 ) {
            CLG_RailSpiral();
        }
    }

    if ( !cl_railtrail_type->integer || cvar_pt_beam_lights->value <= 0 ) {
        CLG_RailLights( rail_color );
    }
}