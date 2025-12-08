/********************************************************************
*
*
*	ClientGame: Particles for the client game module.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/effects/clg_fx_particles.h"



static clg_particle_t *active_particles, *free_particles;

static clg_particle_t particles[ MAX_PARTICLES ];



/**
*   @brief  
**/
void CLG_ClearParticles( void ) {
    int32_t i = 0;

    free_particles = &particles[ 0 ];
    active_particles = nullptr;

    for ( i = 0; i < MAX_PARTICLES - 1; i++ ) {
        particles[ i ].next = &particles[ i + 1 ];
    }
    particles[ i ].next = nullptr;
}

/**
*   @brief
**/
clg_particle_t *CLG_AllocateParticle( void ) {
    clg_particle_t *p;

    if ( !free_particles ) {
        return nullptr;
    }
    p = free_particles;
    free_particles = p->next;
    p->next = active_particles;
    active_particles = p;

    return p;
}


//extern int          r_numparticles;
//extern particle_t   r_particles[ MAX_PARTICLES ];

/**
*   @brief
**/
void CLG_AddParticles( void ) {
    clg_particle_t *p, *next;
    float           alpha;
    float           time = 0, time2;
    int             color;
    clg_particle_t *active, *tail;
    particle_t *part;

    active = NULL;
    tail = NULL;

    for ( p = active_particles; p; p = next ) {
        next = p->next;

        if ( p->alphavel != INSTANT_PARTICLE ) {
            time = ( clgi.client->time - p->time ) * 0.001f;
            alpha = p->alpha + time * p->alphavel;
            if ( alpha <= 0 ) {
                // faded out
                p->next = free_particles;
                free_particles = p;
                continue;
            }
        } else {
            alpha = p->alpha;
        }

        if ( clgi.client->viewScene.r_numparticles >= MAX_PARTICLES )
            break;
        part = &clgi.client->viewScene.r_particles[ clgi.client->viewScene.r_numparticles++ ];

        p->next = NULL;
        if ( !tail )
            active = tail = p;
        else {
            tail->next = p;
            tail = p;
        }

        if ( alpha > 1.0f )
            alpha = 1;
        color = p->color;

        time2 = time * time;

        part->origin[ 0 ] = p->org[ 0 ] + p->vel[ 0 ] * time + p->accel[ 0 ] * time2;
        part->origin[ 1 ] = p->org[ 1 ] + p->vel[ 1 ] * time + p->accel[ 1 ] * time2;
        part->origin[ 2 ] = p->org[ 2 ] + p->vel[ 2 ] * time + p->accel[ 2 ] * time2;

        part->rgba = p->rgba;
        part->color = color;
        part->brightness = p->brightness;
        part->alpha = alpha;
        part->radius = 0.f;

        if ( p->alphavel == INSTANT_PARTICLE ) {
            p->alphavel = 0.0f;
            p->alpha = 0.0f;
        }
    }

    active_particles = active;
}