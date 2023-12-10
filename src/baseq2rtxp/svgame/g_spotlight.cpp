#include "g_local.h"

//void V_AddSphereLight( const vec3_t org, float intensity, float r, float g, float b, float radius ) {
//	dlight_t *dl;
//
//	if ( r_numdlights >= MAX_DLIGHTS )
//		return;
//	dl = &r_dlights[ r_numdlights++ ];
//	memset( dl, 0, sizeof( dlight_t ) );
//	VectorCopy( org, dl->origin );
//	dl->intensity = intensity;
//	dl->color[ 0 ] = r;
//	dl->color[ 1 ] = g;
//	dl->color[ 2 ] = b;
//	dl->radius = radius;
//
//	if ( cl_show_lights->integer && r_numparticles < MAX_PARTICLES ) {
//		particle_t *part = &r_particles[ r_numparticles++ ];
//
//		VectorCopy( dl->origin, part->origin );
//		part->radius = radius;
//		part->brightness = max( r, max( g, b ) );
//		part->color = -1;
//		part->rgba.u8[ 0 ] = (uint8_t)max( 0.f, min( 255.f, r / part->brightness * 255.f ) );
//		part->rgba.u8[ 1 ] = (uint8_t)max( 0.f, min( 255.f, g / part->brightness * 255.f ) );
//		part->rgba.u8[ 2 ] = (uint8_t)max( 0.f, min( 255.f, b / part->brightness * 255.f ) );
//		part->rgba.u8[ 3 ] = 255;
//		part->alpha = 1.f;
//	}
//}
//
//static dlight_t *add_spot_light_common( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b ) {
//	dlight_t *dl;
//
//	if ( r_numdlights >= MAX_DLIGHTS )
//		return NULL;
//	dl = &r_dlights[ r_numdlights++ ];
//	memset( dl, 0, sizeof( dlight_t ) );
//	VectorCopy( org, dl->origin );
//	dl->intensity = intensity;
//	dl->color[ 0 ] = r;
//	dl->color[ 1 ] = g;
//	dl->color[ 2 ] = b;
//	dl->radius = 1.0f;
//	dl->light_type = DLIGHT_SPOT;
//	VectorCopy( dir, dl->spot.direction );
//
//	// what would make sense for cl_show_lights here?
//
//	return dl;
//}
//
//void V_AddSpotLight( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, float falloff_angle ) {
//	dlight_t *dl = add_spot_light_common( org, dir, intensity, r, g, b );
//	if ( !dl )
//		return;
//
//	dl->spot.emission_profile = DLIGHT_SPOT_EMISSION_PROFILE_FALLOFF;
//	dl->spot.cos_total_width = cosf( DEG2RAD( width_angle ) );
//	dl->spot.cos_falloff_start = cosf( DEG2RAD( falloff_angle ) );
//}
//
//void V_AddSpotLightTexEmission( const vec3_t org, const vec3_t dir, float intensity, float r, float g, float b, float width_angle, qhandle_t emission_tex ) {
//	dlight_t *dl = add_spot_light_common( org, dir, intensity, r, g, b );
//	if ( !dl )
//		return;
//
//	dl->spot.emission_profile = DLIGHT_SPOT_EMISSION_PROFILE_AXIS_ANGLE_TEXTURE;
//	dl->spot.total_width = DEG2RAD( width_angle );
//	dl->spot.texture = emission_tex;
//}
//
//void V_AddLight( const vec3_t org, float intensity, float r, float g, float b ) {
//	V_AddSphereLight( org, intensity, r, g, b, 10.f );
//}
//
//
//
//void V_Flashlight( void ) {
//	if ( cls.ref_type == REF_TYPE_VKPT ) {
//		player_state_t *ps = &cl.frame.ps;
//		player_state_t *ops = &cl.oldframe.ps;
//
//		// Flashlight origin
//		vec3_t light_pos;
//		// Flashlight direction (as angles)
//		vec3_t flashlight_angles;
//
//		if ( cls.demo.playback ) {
//			/* If a demo is played we don't have predicted_angles,
//			 * and we can't use cl.refdef.viewangles for the same reason
//			 * below. However, lerping the angles from the old & current frame
//			 * work nicely. */
//			LerpAngles( cl.oldframe.ps.viewangles, cl.frame.ps.viewangles, cl.lerpfrac, flashlight_angles );
//		} else {
//			/* Use cl.playerEntityOrigin+viewoffset, playerEntityAngles instead of
//			 * cl.refdef.vieworg, cl.refdef.viewangles as as the cl.refdef values
//			 * are the camera values, but not the player "eye" values in 3rd person mode. */
//			VectorCopy( cl.predicted_angles, flashlight_angles );
//		}
//		// Add a bit of gun bob to the flashlight as well
//		vec3_t gunangles;
//		LerpVector( ops->gunangles, ps->gunangles, cl.lerpfrac, gunangles );
//		VectorAdd( flashlight_angles, gunangles, flashlight_angles );
//
//		vec3_t view_dir, right_dir, up_dir;
//		AngleVectors( flashlight_angles, view_dir, right_dir, up_dir );
//
//		/* Start off with the player eye position. */
//		vec3_t viewoffset;
//		LerpVector( ops->viewoffset, ps->viewoffset, cl.lerpfrac, viewoffset );
//		VectorAdd( cl.playerEntityOrigin, viewoffset, light_pos );
//
//		/* Slightly move position downward, right, and forward to get a position
//		 * that looks somewhat as if it was attached to the gun.
//		 * Generally, the spot light origin should be placed away from the player model
//		 * to avoid interactions with it (mainly unexpected shadowing).
//		 * When adjusting the offsets care must be taken that
//		 * the flashlight doesn't also light the view weapon. */
//		VectorMA( light_pos, flashlight_offset[ 2 ] * cl_gunscale->value, view_dir, light_pos );
//		float leftright = flashlight_offset[ 0 ] * cl_gunscale->value;
//		if ( info_hand->integer == 1 )
//			leftright = -leftright; // left handed
//		else if ( info_hand->integer == 2 )
//			leftright = 0.f; // "center" handed
//		VectorMA( light_pos, leftright, right_dir, light_pos );
//		VectorMA( light_pos, flashlight_offset[ 1 ] * cl_gunscale->value, up_dir, light_pos );
//
//		V_AddSpotLightTexEmission( light_pos, view_dir, cl_flashlight_intensity->value, 1.f, 1.f, 1.f, 90.0f, flashlight_profile_tex );
//	} else {
//		// Flashlight is VKPT only
//	}
//}

/**
*	@brief
**/
void spotlight_think( edict_t *self ) {
	self->think = spotlight_think;
	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*	@brief	
**/
void spotlight_use( edict_t *self, edict_t *other, edict_t *activator ) {

}

/**
*	@brief
**/
void SP_spotlight( edict_t *self ) {
	G_InitEdict( self );
	
	// Spotlight Type
	self->s.entityType = ET_SPOTLIGHT;
	self->classname = "spotlight";
	self->s.effects |= EF_SPOTLIGHT;
////
//	//self->solid = SOLID_BBOX;
//	//self->movetype = MOVETYPE_STEP;
//	self->s.skinnum = MakeColor( 255, 0, 0, 255 );
//	self->s.frame = self->light;
//	//self->model = "models/objects/barrels/tris.md2";
//	//self->s.modelindex = gi.modelindex( self->model );
//	VectorSet( self->mins, -16, -16, 0 );
//	VectorSet( self->maxs, 16, 16, 40 );

	//if ( !self->mass )
	//	self->mass = 400;
	//if ( !self->health )
	//	self->health = 10;
	//if ( !self->dmg )
	//	self->dmg = 150;
//

	self->think = spotlight_think;
	self->use = spotlight_use;
	self->nextthink = level.time + FRAME_TIME_MS;

	gi.linkentity( self );
}