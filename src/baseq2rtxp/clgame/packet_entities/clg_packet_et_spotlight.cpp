/********************************************************************
*
*
*	ClientGame: 'ET_SPOTLIGHT' Packet Entities.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_temp_entities.h"

/**
*	@brief	Will setup the refresh entity for the ET_SPOTLIGHT centity with the newState.
**/
void CLG_PacketEntity_AddSpotlight( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    // Lerp Origin:
    Vector3 lerpedOrigin = QM_Vector3Lerp( packetEntity->prev.origin, packetEntity->current.origin, clgi.client->lerpfrac );
    VectorCopy( lerpedOrigin, refreshEntity->origin );
    VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );

    // Lerp the angles.
    LerpAngles( packetEntity->prev.angles, packetEntity->current.angles, clgi.client->lerpfrac, refreshEntity->angles );

    // Just in case?
    refreshEntity->flags = newState->renderfx;

    // Calculate RGB vector.
    vec3_t rgb = { 1.f, 1.f, 1.f };
    rgb[ 0 ] = ( 1.0f / 255.f ) * newState->spotlight.rgb[ 0 ];
    rgb[ 1 ] = ( 1.0f / 255.f ) * newState->spotlight.rgb[ 1 ];
    rgb[ 2 ] = ( 1.0f / 255.f ) * newState->spotlight.rgb[ 2 ];

    // Extract light intensity from "frame".
    float lightIntensity = newState->spotlight.intensity;

    // Calculate the spotlight's view direction based on set euler angles.
    Vector3 viewForwardDir = {};
    QM_AngleVectors( refreshEntity->angles, &viewForwardDir, nullptr, nullptr );
    VectorCopy( viewForwardDir, refreshEntity->angles );

    // Add the spotlight. (x = 90, y = 0, z = 0) should give us one pointing right down to the floor. (width 90, falloff 0)
    // Use the image based texture profile in case one is set.
#if 0
    if ( newState->image_profile ) {
        qhandle_t spotlightPicHandle = R_RegisterImage( "flashlight_profile_01", IT_PIC, static_cast<imageflags_t>( IF_PERMANENT | IF_BILERP ) );
        V_AddSpotLightTexEmission( ent->origin, view_dir, lightIntensity,
            // TODO: Multiply the RGB?
            rgb[ 0 ] * 2, rgb[ 1 ] * 2, rgb[ 2 ] * 2,
            newState->spotlight.angle_width, spotlightPicHandle );
    } else
#endif
    {
        clgi.V_AddSpotLight( refreshEntity->origin, &viewForwardDir.x, lightIntensity,
            // TODO: Multiply the RGB?
            rgb[ 0 ] * 2, rgb[ 1 ] * 2, rgb[ 2 ] * 2,
            newState->spotlight.angle_width, newState->spotlight.angle_falloff );
    }

    // Add entity to refresh list
    clgi.V_AddEntity( refreshEntity );

    // skip:
    VectorCopy( refreshEntity->origin, packetEntity->lerp_origin );

    // Add spotlight. (x = 90, y = 0, z = 0) should give us one pointing right down to the floor. (width 90, falloff 0)
    //V_AddSpotLight( ent->origin, view_dir, 225.0, 1.f, 0.1f, 0.1f, 45, 0 );
    //V_AddSphereLight( ent.origin, 500.f, 1.6f, 1.0f, 0.2f, 10.f );
    //V_AddSpotLightTexEmission( light_pos, view_dir, cl_flashlight_intensity->value, 1.f, 1.f, 1.f, 90.0f, flashlight_profile_tex );
}