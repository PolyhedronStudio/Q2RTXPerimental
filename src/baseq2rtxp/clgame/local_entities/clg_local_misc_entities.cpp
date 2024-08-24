/********************************************************************
*
*
*	ClientGame - Local Entities: Misc
*
*
********************************************************************/
#include "../clg_local.h"
#include "../clg_entities.h"
#include "../clg_local_entities.h"
#include "../clg_precache.h"

// Include actual entity class types.
#include "clg_local_entity_classes.h"



/**
*
*
*	client_misc_model:
*
*
**/
/**
*	@brief	Precache function for 'client_misc_model' entity class type.
**/
void CLG_misc_model_Precache( clg_local_entity_t *self, const cm_entity_t *keyValues ) {
	// Get class locals.
	clg_misc_model_locals_t *classLocals = static_cast<clg_misc_model_locals_t *>( self->classLocals );

	// Key/Value: 'model':
	if ( const cm_entity_t *modelKv = clgi.CM_EntityKeyValue( keyValues, "model" ) ) {
		if ( modelKv->parsed_type & cm_entity_parsed_type_t::ENTITY_STRING ) {
			self->model = modelKv->string;
		} else {
			self->model = nullptr;// memset( classLocals->modelname, 0, MAX_QPATH );
		}
	}

	// Key/Value: 'frame':
	if ( const cm_entity_t *frameKv = clgi.CM_EntityKeyValue( keyValues, "frame" ) ) {
		if ( frameKv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
			self->locals.frame = frameKv->integer;
		} else {
			self->locals.frame = 0;
		}
	// Key/Values for skeletal animationIDs:
	} else {
		// Key/Value: 'headAnimationID':
		if ( const cm_entity_t *headAnimationIDKv = clgi.CM_EntityKeyValue( keyValues, "headAnimationID" ) ) {
			if ( headAnimationIDKv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
				classLocals->animation.headID = headAnimationIDKv->integer;
			} else {
				classLocals->animation.headID = 0;
			}
		}
		// Key/Value: 'torsoAnimationID':
		else if ( const cm_entity_t *torsoAnimationIDKv = clgi.CM_EntityKeyValue( keyValues, "torsoAnimationID" ) ) {
			if ( torsoAnimationIDKv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
				classLocals->animation.torsoID = torsoAnimationIDKv->integer;
			} else {
				classLocals->animation.torsoID = 0;
			}
		}
		// Key/Value: 'hipAnimationID':
		else if ( const cm_entity_t *hipAnimationIDKv = clgi.CM_EntityKeyValue( keyValues, "hipAnimationID" ) ) {
			if ( hipAnimationIDKv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
				classLocals->animation.hipID = hipAnimationIDKv->integer;
			} else {
				classLocals->animation.hipID = 0;
			}
		// None:
		} else {
			// Frame, nor any of the above are set, so default to frame = 0:
			self->locals.oldframe = self->locals.frame = 0;
		}
	}

	// Key/Value: 'skin':
	if ( const cm_entity_t *skinKv = clgi.CM_EntityKeyValue( keyValues, "skin" ) ) {
		if ( skinKv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
			self->locals.skinNumber = skinKv->integer;
		} else {
			self->locals.skinNumber = 0;
		}
	}

	// Set up the modelname for precaching the model with.
	if ( self->model && self->model[ 0 ] != '\0' ) {
		self->locals.modelindex = CLG_RegisterLocalModel( self->model );
	} else {
		clgi.Print( PRINT_DEVELOPER, "%s: empty 'model' field. Unable to register local entity model.\n", __func__ );
	}

	// DEBNUG PRINT:
	clgi.Print( PRINT_DEVELOPER, "CLG_misc_model_Precache: model(%s), local_draw_model_handle(%d) frame(%d), skin(%d)\n", self->model, self->locals.modelindex, self->locals.frame, self->locals.skinNumber );
}

/**
*	@brief	Sets up the local client model entity.
**/
void CLG_misc_model_Spawn( clg_local_entity_t *self ) {
	// Setup appropriate mins, maxs.
	self->locals.mins = { -16, -16, 0 };
	self->locals.maxs = { 16, 16, 40 };

	// Link entity for this frame.
	CLG_LocalEntity_Link( self );

	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*	@brief	Sets up the local client model entity.
**/
void CLG_misc_model_PostSpawn( clg_local_entity_t *self ) {
	// Get class locals.
	clg_misc_model_locals_t *classLocals = static_cast<clg_misc_model_locals_t *>( self->classLocals );

	// Get the modelname.
	const char *modelName = self->model;
	// Get a pointer to the model resource, needed for animation data.
	classLocals->model = clgi.R_GetModelDataForName( modelName );

	// Animation IDs.
	classLocals->animation.headID = 14;
	classLocals->animation.torsoID = 15;
	classLocals->animation.hipID = 3;
	
	// They all start... NOW.
	classLocals->animation.headStartTime = sg_time_t::from_ms( clgi.client->time );
	classLocals->animation.torsoStartTime = sg_time_t::from_ms( clgi.client->time );
	classLocals->animation.hipStartTime = sg_time_t::from_ms( clgi.client->time );
}

/**
*	@brief	Think once per local game tick.
**/
void CLG_misc_model_Think( clg_local_entity_t *self ) {
	// Get class locals.
	clg_misc_model_locals_t *classLocals = static_cast<clg_misc_model_locals_t *>( self->classLocals );

	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*	@brief	Called each refresh frame.
**/
void CLG_misc_model_RefreshFrame( clg_local_entity_t *self ) {
	#if 0
	// Get class.
	auto *selfClass = CLG_LocalEntity_GetClass<clg_misc_model_locals_t>( self );

	// Refresh Entity.
	entity_t *refreshEntity = &selfClass->rent;

	// Get IQM and SKM resource pointers.
	skm_model_t *skmData = selfClass->model->skmData;
	skm_config_t *skmConfig = selfClass->model->skmConfig;

	/**
	*	Head Animation:
	**/
	// Get animation in case one is set.
	if ( selfClass->animation.headID >= 0 ) {
		// Get the animation data.
		skm_anim_t *iqmAnimation = &skmData->animations[ selfClass->animation.headID ];
		
		if ( iqmAnimation ) {
			// First and Last frame.
			const int32_t firstFrame = iqmAnimation->first_frame;
			const int32_t lastFrame = firstFrame + iqmAnimation->num_frames;

			// Backup the previously 'current' frame as its last frame.
			selfClass->animation.lastHeadFrame = selfClass->animation.currentHeadFrame;

			// Calculate the actual current frame for the moment in time of the active animation.
			double lerpFraction = SG_AnimationFrameForTime( &selfClass->animation.currentHeadFrame,
				//sg_time_t::from_ms( clgi.GetRealTime() ), sg_time_t::from_ms( game.viewWeapon.real_time ),
				sg_time_t::from_ms( clgi.client->extrapolatedTime ), selfClass->animation.headStartTime,
				BASE_FRAMETIME,
				firstFrame, lastFrame,
				0, true
			);

			// Animation is running:
			if ( lerpFraction < 1.0 ) {
				// Apply animation to gun model refresh entity.
				selfClass->animation.lastHeadFrame = ( selfClass->animation.currentHeadFrame > firstFrame
					? selfClass->animation.currentHeadFrame - 1 : selfClass->animation.currentHeadFrame );
				// Enforce lerp of 0.0 to ensure that the first frame does not 'bug out'.
				if ( selfClass->animation.currentHeadFrame == firstFrame ) {
					selfClass->animation.headBackLerp = 0.0;
					// Enforce lerp of 1.0 if the calculated frame is equal or exceeds the last one.
				} else if ( selfClass->animation.currentHeadFrame == lastFrame ) {
					selfClass->animation.headBackLerp = 1.0;
					// Otherwise just subtract the resulting lerpFraction.
				} else {
					selfClass->animation.headBackLerp = 1.0 - lerpFraction;
				}
				// Clamp just to be sure.
				clamp( selfClass->animation.headBackLerp, 0.0, 1.0 );
				// Reached the end of the animation:
			} else {
				// Otherwise, oldframe now equals the current(end) frame.
				selfClass->animation.lastHeadFrame = selfClass->animation.currentHeadFrame = lastFrame;
				// No more lerping.
				selfClass->animation.headBackLerp = 1.0;
			}
		}
	}

	/**
	*	Torso Animation:
	**/
	// Get animation in case one is set.
	if ( selfClass->animation.torsoID >= 0 ) {
		// Get the animation data.
		skm_anim_t *iqmAnimation = &skmData->animations[ selfClass->animation.torsoID ];

		if ( iqmAnimation ) {
			// First and Last frame.
			const int32_t firstFrame = iqmAnimation->first_frame;
			const int32_t lastFrame = firstFrame + iqmAnimation->num_frames;

			// Backup the previously 'current' frame as its last frame.
			selfClass->animation.lastTorsoFrame = selfClass->animation.currentTorsoFrame;

			// Calculate the actual current frame for the moment in time of the active animation.
			double lerpFraction = SG_AnimationFrameForTime( &selfClass->animation.currentTorsoFrame,
				//sg_time_t::from_ms( clgi.GetRealTime() ), sg_time_t::from_ms( game.viewWeapon.real_time ),
				sg_time_t::from_ms( clgi.client->extrapolatedTime ), selfClass->animation.torsoStartTime,
				BASE_FRAMETIME,
				firstFrame, lastFrame,
				0, true
			);

			// Animation is running:
			if ( lerpFraction < 1.0 ) {
				// Apply animation to gun model refresh entity.
				selfClass->animation.lastTorsoFrame = ( selfClass->animation.currentTorsoFrame > firstFrame
					? selfClass->animation.currentTorsoFrame - 1 : selfClass->animation.currentTorsoFrame );
				// Enforce lerp of 0.0 to ensure that the first frame does not 'bug out'.
				if ( selfClass->animation.currentTorsoFrame == firstFrame ) {
					selfClass->animation.torsoBackLerp = 0.0;
					// Enforce lerp of 1.0 if the calculated frame is equal or exceeds the last one.
				} else if ( selfClass->animation.currentTorsoFrame == lastFrame ) {
					selfClass->animation.torsoBackLerp = 1.0;
					// Otherwise just subtract the resulting lerpFraction.
				} else {
					selfClass->animation.torsoBackLerp = 1.0 - lerpFraction;
				}
				// Clamp just to be sure.
				clamp( selfClass->animation.torsoBackLerp, 0.0, 1.0 );
				// Reached the end of the animation:
			} else {
				// Otherwise, oldframe now equals the current(end) frame.
				selfClass->animation.lastTorsoFrame = selfClass->animation.currentTorsoFrame = lastFrame;
				// No more lerping.
				selfClass->animation.torsoBackLerp = 1.0;
			}
		}
	}
	/**
	*	Hip Animation:
	**/
	// Get animation in case one is set.
	if ( selfClass->animation.hipID >= 0 ) {
		// Get the animation data.
		skm_anim_t *iqmAnimation = &skmData->animations[ selfClass->animation.hipID ];

		if ( iqmAnimation ) {
			// First and Last frame.
			const int32_t firstFrame = iqmAnimation->first_frame;
			const int32_t lastFrame = firstFrame + iqmAnimation->num_frames;

			// Backup the previously 'current' frame as its last frame.
			selfClass->animation.lastHipFrame = selfClass->animation.currentHipFrame;

			// Calculate the actual current frame for the moment in time of the active animation.
			double lerpFraction = SG_AnimationFrameForTime( &selfClass->animation.currentHipFrame,
				//sg_time_t::from_ms( clgi.GetRealTime() ), sg_time_t::from_ms( game.viewWeapon.real_time ),
				sg_time_t::from_ms( clgi.client->extrapolatedTime ), selfClass->animation.hipStartTime,
				BASE_FRAMETIME,
				firstFrame, lastFrame,
				0, true
			);

			// Animation is running:
			if ( lerpFraction < 1.0 ) {
				// Apply animation to gun model refresh entity.
				selfClass->animation.lastHipFrame = ( selfClass->animation.currentHipFrame > firstFrame
					? selfClass->animation.currentHipFrame - 1 : selfClass->animation.currentHipFrame );
				// Enforce lerp of 0.0 to ensure that the first frame does not 'bug out'.
				if ( selfClass->animation.currentHipFrame == firstFrame ) {
					selfClass->animation.hipBackLerp = 0.0;
					// Enforce lerp of 1.0 if the calculated frame is equal or exceeds the last one.
				} else if ( selfClass->animation.currentHipFrame == lastFrame ) {
					selfClass->animation.hipBackLerp = 1.0;
					// Otherwise just subtract the resulting lerpFraction.
				} else {
					selfClass->animation.hipBackLerp = 1.0 - lerpFraction;
				}
				// Clamp just to be sure.
				clamp( selfClass->animation.hipBackLerp, 0.0, 1.0 );
				// Reached the end of the animation:
			} else {
				// Otherwise, oldframe now equals the current(end) frame.
				selfClass->animation.lastHipFrame = selfClass->animation.currentHipFrame = lastFrame;
				// No more lerping.
				selfClass->animation.hipBackLerp = 1.0;
			}
		}
	}

	#endif
}


/**************************************************************
*
*
*
*
*
*
*
**************************************************************/
static void QuatSlerp( const quat_t from, const quat_t _to, float fraction, quat_t out ) {
	// cos() of angle
	float cosAngle = from[ 0 ] * _to[ 0 ] + from[ 1 ] * _to[ 1 ] + from[ 2 ] * _to[ 2 ] + from[ 3 ] * _to[ 3 ];

	// negative handling is needed for taking shortest path (required for model joints)
	quat_t to;
	if ( cosAngle < 0.0f ) {
		cosAngle = -cosAngle;
		to[ 0 ] = -_to[ 0 ];
		to[ 1 ] = -_to[ 1 ];
		to[ 2 ] = -_to[ 2 ];
		to[ 3 ] = -_to[ 3 ];
	} else {
		QuatCopy( _to, to );
	}

	float backlerp, lerp;
	if ( cosAngle < 0.999999f ) {
		// spherical lerp (slerp)
		const float angle = acosf( cosAngle );
		const float sinAngle = sinf( angle );
		backlerp = sinf( ( 1.0f - fraction ) * angle ) / sinAngle;
		lerp = sinf( fraction * angle ) / sinAngle;
	} else {
		// linear lerp
		backlerp = 1.0f - fraction;
		lerp = fraction;
	}

	out[ 0 ] = from[ 0 ] * backlerp + to[ 0 ] * lerp;
	out[ 1 ] = from[ 1 ] * backlerp + to[ 1 ] * lerp;
	out[ 2 ] = from[ 2 ] * backlerp + to[ 2 ] * lerp;
	out[ 3 ] = from[ 3 ] * backlerp + to[ 3 ] * lerp;
}
static void IQM_ComputeRelativeJoints( const model_t *model, const int32_t frame, const int32_t oldFrame, const float frontLerp, const float backLerp, iqm_transform_t *outBonePose, const int32_t rootMotionBoneID, const int32_t rootMotionAxisFlags ) {
	// Sanity check.
	if ( !model || !model->skmData ) {
		return;
	}

	// Get IQM Data.
	skm_model_t *skmData = model->skmData;

	// Keep frame within bounds.
	const int32_t boundFrame = skmData->num_frames ? frame % (int32_t)skmData->num_frames : 0;
	const int32_t boundOldFrame = skmData->num_frames ? oldFrame % (int32_t)skmData->num_frames : 0;

	// Keep bone ID sane.
	int32_t boundRootMotionBoneID = ( rootMotionBoneID >= 0 ? rootMotionBoneID % (int32_t)( skmData->num_joints - rootMotionBoneID ) : -1 );

	// Fetch first joint.
	skm_transform_t *relativeJoint = outBonePose;

	// Copy the animation frame pos.
	if ( frame == oldFrame ) {
		const skm_transform_t *pose = &skmData->poses[ boundFrame * skmData->num_poses ];
		for ( uint32_t pose_idx = 0; pose_idx < skmData->num_poses; pose_idx++, pose++, relativeJoint++ ) {
			#if 1
			if ( rootMotionAxisFlags != SKM_POSE_TRANSLATE_ALL && pose_idx == boundRootMotionBoneID ) {
				// Translate the selected axises.
				if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_X ) ) {
					relativeJoint->translate[ 0 ] = 0;
				} else {
					relativeJoint->translate[ 0 ] = pose->translate[ 0 ];
				}
				if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_Y ) ) {
					relativeJoint->translate[ 1 ] = 0;
				} else {
					relativeJoint->translate[ 1 ] = pose->translate[ 1 ];
				}
				if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_Z ) ) {
					relativeJoint->translate[ 2 ] = 0;
				} else {
					relativeJoint->translate[ 2 ] = pose->translate[ 2 ];
				}
				// Copy in scale as per usual.
				VectorCopy( pose->scale, relativeJoint->scale );
				// Copy quat rotation as usual.
				QuatCopy( pose->rotate, relativeJoint->rotate );
				continue;
			}
			#endif

			VectorCopy( pose->translate, relativeJoint->translate );
			QuatCopy( pose->rotate, relativeJoint->rotate );
			VectorCopy( pose->scale, relativeJoint->scale );
		}
	// Lerp the animation frame pose.
	} else {
		const skm_transform_t *pose = &skmData->poses[ boundFrame * skmData->num_poses ];
		const skm_transform_t *oldPose = &skmData->poses[ boundOldFrame * skmData->num_poses ];
		for ( uint32_t pose_idx = 0; pose_idx < skmData->num_poses; pose_idx++, oldPose++, pose++, relativeJoint++ ) {
			#if 1
			if ( rootMotionAxisFlags != SKM_POSE_TRANSLATE_ALL && pose_idx == boundRootMotionBoneID ) {
				// Translate the selected axises.
				if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_X ) ) {
					relativeJoint->translate[ 0 ] = 0;
				} else {
					relativeJoint->translate[ 0 ] = oldPose->translate[ 0 ] * backLerp + pose->translate[ 0 ] * frontLerp; //relativeJoint->translate[ 1 ] = 0; //relativeJoint->translate[ 0 ] = 0;
				}
				if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_Y ) ) {
					relativeJoint->translate[ 1 ] = 0;
				} else {
					relativeJoint->translate[ 1 ] = oldPose->translate[ 1 ] * backLerp + pose->translate[ 1 ] * frontLerp; //relativeJoint->translate[ 1 ] = 0;
				}
				if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_Z ) ) {
					relativeJoint->translate[ 2 ] = 0;
				} else {
					relativeJoint->translate[ 2 ] = oldPose->translate[ 2 ] * backLerp + pose->translate[ 2 ] * frontLerp;
				}

				// Scale Lerp as usual.
				relativeJoint->scale[ 0 ] = oldPose->scale[ 0 ] * backLerp + oldPose->scale[ 0 ] * frontLerp;
				relativeJoint->scale[ 1 ] = oldPose->scale[ 1 ] * backLerp + oldPose->scale[ 1 ] * frontLerp;
				relativeJoint->scale[ 2 ] = oldPose->scale[ 2 ] * backLerp + oldPose->scale[ 2 ] * frontLerp;
				// Quat Slerp as usual.
				QuatSlerp( oldPose->rotate, pose->rotate, frontLerp, relativeJoint->rotate );
				continue;
			}
			#endif

			relativeJoint->translate[ 0 ] = oldPose->translate[ 0 ] * backLerp + pose->translate[ 0 ] * frontLerp;
			relativeJoint->translate[ 1 ] = oldPose->translate[ 1 ] * backLerp + pose->translate[ 1 ] * frontLerp;
			relativeJoint->translate[ 2 ] = oldPose->translate[ 2 ] * backLerp + pose->translate[ 2 ] * frontLerp;

			relativeJoint->scale[ 0 ] = oldPose->scale[ 0 ] * backLerp + oldPose->scale[ 0 ] * frontLerp;
			relativeJoint->scale[ 1 ] = oldPose->scale[ 1 ] * backLerp + oldPose->scale[ 1 ] * frontLerp;
			relativeJoint->scale[ 2 ] = oldPose->scale[ 2 ] * backLerp + oldPose->scale[ 2 ] * frontLerp;

			QuatSlerp( oldPose->rotate, pose->rotate, frontLerp, relativeJoint->rotate );
		}
	}
}

/****************************************************************


*****************************************************************/
#if 0
static iqm_transform_t headAnimationBonePoses[ SKM_MAX_BONES ];
static iqm_transform_t torsoAnimationBonePoses[ SKM_MAX_BONES ];
static iqm_transform_t hipAnimationBonePoses[ SKM_MAX_BONES ];

const int32_t _SKM_RecursiveBlendPose( clg_local_entity_t *self, clg_misc_model_locals_t *selfClass, entity_t *refreshEntity, sg_skm_body_part_e bodyPart, skm_bone_node_t *boneNode, iqm_transform_t *outBonePoses ) {
	// Get IQM and SKM resource pointers.
	iqm_model_t *iqmData = selfClass->model->iqmData;
	skm_config_t *skmConfig = selfClass->model->skmConfig;

	// This pointer is changed to the source animation pose we're currently acquiring data from.
	iqm_transform_t *sourceBonePoses;
	double frontLerp = 1.0 - selfClass->animation.headBackLerp;
	double backLerp = selfClass->animation.headBackLerp;
	if ( bodyPart == SKM_BODY_HEAD ) {
		sourceBonePoses = headAnimationBonePoses;
		frontLerp = 1.0 - selfClass->animation.headBackLerp;
		backLerp = selfClass->animation.headBackLerp;
	} else if ( bodyPart == SKM_BODY_UPPER ) {
		sourceBonePoses = torsoAnimationBonePoses;
		frontLerp = 1.0 - selfClass->animation.torsoBackLerp;
		backLerp = selfClass->animation.torsoBackLerp;
	} else {
		sourceBonePoses = hipAnimationBonePoses;
		frontLerp = 1.0 - selfClass->animation.hipBackLerp;
		backLerp = selfClass->animation.hipBackLerp;
	}

	// Blend this bone.
	if ( boneNode->number >= 0 ) {
		*( outBonePoses + boneNode->number ) = *( sourceBonePoses + boneNode->number );
		//*outBonePoses->translate = *sourceBonePoses->translate;

		//// Lerp the already Lerped Scale.
		//*outBonePoses->scale = *sourceBonePoses->scale;
		////outBonePoses->scale[ 0 ] = outBonePoses->scale[ 0 ] * backLerp + sourceBonePoses->scale[ 0 ] * frontLerp;
		////outBonePoses->scale[ 1 ] = outBonePoses->scale[ 1 ] * backLerp + sourceBonePoses->scale[ 1 ] * frontLerp;
		////outBonePoses->scale[ 2 ] = outBonePoses->scale[ 2 ] * backLerp + sourceBonePoses->scale[ 2 ] * frontLerp;

		//// Slerp the rotation
		//QuatSlerp( outBonePoses->rotate, sourceBonePoses->rotate, fraction, outBonePoses->rotate );

		if ( boneNode->numberOfChildNodes ) {
			for ( int32_t i = 0; i < boneNode->numberOfChildNodes; i++ ) {
				skm_bone_node_t *childNode = boneNode->childBones[ i ];
				if ( !childNode ) {
					continue;
				}

				#if 1
				int32_t childBodyPart = bodyPart;
				if ( childNode == skmConfig->rootBones.hip ) {
					childBodyPart = SKM_BODY_LOWER;
				} else if ( childNode == skmConfig->rootBones.torso ) {
					childBodyPart = SKM_BODY_UPPER;
				} else if ( childNode == skmConfig->rootBones.head ) {
					childBodyPart = SKM_BODY_HEAD;
				}

				// Blend.
				_SKM_RecursiveBlendPose( self, selfClass, refreshEntity, (sg_skm_body_part_e)childBodyPart, childNode, outBonePoses );
				#else
				// Blend.
				_SKM_RecursiveBlendPose( self, selfClass, refreshEntity, (sg_skm_body_part_e)bodyPart, childNode, outBonePoses );
				#endif
			}
		}

		return boneNode->number;
	}

	return -1;
}
#endif

/**
*	@brief	Blend the Head and the Torso animations sequentially on top of the Hip animation.
**/
static void CLG_misc_model_BlendAnimations( clg_local_entity_t *self, clg_misc_model_locals_t *selfClass, entity_t *refreshEntity ) {
	// Get IQM and SKM resource pointers.
	skm_model_t *iqmData = selfClass->model->skmData;
	skm_config_t *skmConfig = selfClass->model->skmConfig;

	#if 1
	// TODO: Acquire bone cache space.
	static iqm_transform_t headAnimationBonePoses[ SKM_MAX_BONES ];
	double headFrontLerp = 1.0 - selfClass->animation.headBackLerp;
	double headBackLerp = selfClass->animation.headBackLerp;
	IQM_ComputeRelativeJoints( selfClass->model, selfClass->animation.currentHeadFrame, selfClass->animation.lastHeadFrame, headFrontLerp, headBackLerp, headAnimationBonePoses, skmConfig->rootBones.hip->number, SKM_POSE_TRANSLATE_ALL );
	static iqm_transform_t torsoAnimationBonePoses[ SKM_MAX_BONES ];
	double torsoFrontLerp = 1.0 - selfClass->animation.torsoBackLerp;
	double torsoBackLerp = selfClass->animation.torsoBackLerp;
	IQM_ComputeRelativeJoints( selfClass->model, selfClass->animation.currentTorsoFrame, selfClass->animation.lastTorsoFrame, torsoFrontLerp, torsoBackLerp, torsoAnimationBonePoses, skmConfig->rootBones.hip->number, SKM_POSE_TRANSLATE_ALL );
	static iqm_transform_t hipAnimationBonePoses[ SKM_MAX_BONES ];
	double hipFrontLerp = 1.0 - selfClass->animation.hipBackLerp;
	double hipBackLerp = selfClass->animation.hipBackLerp;
	IQM_ComputeRelativeJoints( selfClass->model, selfClass->animation.currentHipFrame, selfClass->animation.lastHipFrame, hipFrontLerp, hipBackLerp, hipAnimationBonePoses, skmConfig->rootBones.hip->number, SKM_POSE_TRANSLATE_Z | SKM_POSE_TRANSLATE_Y );

	// Now recursively blend from the specified bones in skm data.
	static iqm_transform_t blendedBonePoses[ SKM_MAX_BONES ];
	clgi.SKM_RecursiveBlendFromBone( torsoAnimationBonePoses, hipAnimationBonePoses, skmConfig->rootBones.torso, nullptr, 0, torsoBackLerp, 0.5f );
	clgi.SKM_RecursiveBlendFromBone( hipAnimationBonePoses, blendedBonePoses, skmConfig->rootBones.hip, nullptr, 0, hipBackLerp, 1.0f );
	
	clgi.SKM_RecursiveBlendFromBone( headAnimationBonePoses, blendedBonePoses, skmConfig->rootBones.head, nullptr, 0, headBackLerp, 0.5f );

	refreshEntity->bonePoses = blendedBonePoses;
	#else
	// TODO: Acquire bone cache space.
	double headFrontLerp = 1.0 - selfClass->animation.headBackLerp;
	double headBackLerp = selfClass->animation.headBackLerp;
	IQM_ComputeRelativeJoints( selfClass->model, selfClass->animation.currentHeadFrame, selfClass->animation.lastHeadFrame, headFrontLerp, headBackLerp, headAnimationBonePoses, skmConfig->rootBones.hip->number, SKM_POSE_TRANSLATE_Z | SKM_POSE_TRANSLATE_Y );
	double torsoFrontLerp = 1.0 - selfClass->animation.torsoBackLerp;
	double torsoBackLerp = selfClass->animation.torsoBackLerp;
	IQM_ComputeRelativeJoints( selfClass->model, selfClass->animation.currentTorsoFrame, selfClass->animation.lastTorsoFrame, torsoFrontLerp, torsoBackLerp, torsoAnimationBonePoses, skmConfig->rootBones.hip->number, SKM_POSE_TRANSLATE_Z | SKM_POSE_TRANSLATE_Y );
	double hipFrontLerp = 1.0 - selfClass->animation.hipBackLerp;
	double hipBackLerp = selfClass->animation.hipBackLerp;
	IQM_ComputeRelativeJoints( selfClass->model, selfClass->animation.currentHipFrame, selfClass->animation.lastHipFrame, hipFrontLerp, hipBackLerp, hipAnimationBonePoses, skmConfig->rootBones.hip->number, SKM_POSE_TRANSLATE_Z | SKM_POSE_TRANSLATE_Y );

	// Now recursively blend from the specified bones in skm data.
	//SKM_RecursiveBlendFromBone( hipAnimationBonePoses, blendedBonePoses, skmConfig->rootBones.hip, hipBackLerp, 1.0f );
	//SKM_RecursiveBlendFromBone( torsoAnimationBonePoses, blendedBonePoses, skmConfig->rootBones.torso, torsoBackLerp, 0.5f );
	//SKM_RecursiveBlendFromBone( headAnimationBonePoses, blendedBonePoses, skmConfig->rootBones.head, headBackLerp, 0.5f );

	// Resulting pose.
	static iqm_transform_t finalBonePoses[ SKM_MAX_BONES ];
	// Start off with the root bone.
	skm_bone_node_t *rootBoneNode = skmConfig->boneTree;
	// Begin recursively blending animations.
	int32_t result = _SKM_RecursiveBlendPose( self, selfClass, refreshEntity, SKM_BODY_HEAD, skmConfig->rootBones.head, finalBonePoses );
	result = _SKM_RecursiveBlendPose( self, selfClass, refreshEntity, SKM_BODY_UPPER, skmConfig->rootBones.torso, finalBonePoses );
	result = _SKM_RecursiveBlendPose( self, selfClass, refreshEntity, SKM_BODY_LOWER, skmConfig->rootBones.hip, finalBonePoses );
	// Assign resulting bone pose to refresh entity.
	refreshEntity->bonePoses = finalBonePoses;

	#endif
}

/**
*	@brief	Gives the chance to prepare and add a 'refresh entity' for this client local's frame view.
**/
void CLG_misc_model_PrepareRefreshEntity( clg_local_entity_t *self ) {
	// Get class.
	clg_misc_model_locals_t *selfClass = CLG_LocalEntity_GetClass<clg_misc_model_locals_t>( self );

	///**
	//*	Setup Origin, Angles, model, skin, alpha and scale.
	//**/
	//// Clean slate refresh entity.
	////selfClass->rent = {};
	//// Setup the refresh entity ID to start off at RENTITIY_OFFSET_LOCALENTITIES.
	//selfClass->rent.id = RENTITIY_OFFSET_LOCALENTITIES + self->id;

	//// Copy spatial information over into the refresh entity.
	//VectorCopy( self->locals.origin, selfClass->rent.origin );
	//VectorCopy( self->locals.origin, selfClass->rent.oldorigin );
	//VectorCopy( self->locals.angles, selfClass->rent.angles );

	//// Copy model information.
	//if ( self->locals.modelindex ) {
	//	selfClass->rent.model = precache.local_draw_models[ self->locals.modelindex ];
	//	// Copy skin information.
	//	//rent.skin = 0; // inline skin, -1 would use rgba.
	//	//rent.skinnum = self->locals.skinNumber;
	//} else {
	//	selfClass->rent.model = 0;
	//	selfClass->rent.skin = 0; // inline skin, -1 would use rgba.
	//	selfClass->rent.skinnum = 0;
	//}
	//selfClass->rent.rgba.u32 = MakeColor( 255, 255, 255, 255 );

	//// Copy general render properties.
	//selfClass->rent.alpha = 1.0f;
	//selfClass->rent.scale = 1.0f;

	//// Process the animations for the misc model.
	//CLG_misc_model_BlendAnimations( self, selfClass, &selfClass->rent );

	//// Add entity
	//clgi.V_AddEntity( &selfClass->rent );
}

// Class definition.
const clg_local_entity_class_t client_misc_model = {
	.classname = "client_misc_model",
	.callbackPrecache = CLG_misc_model_Precache,
	.callbackSpawn = CLG_misc_model_Spawn,
	.callbackPostSpawn = CLG_misc_model_PostSpawn,
	.callbackThink = CLG_misc_model_Think,
	.callbackRFrame = CLG_misc_model_RefreshFrame,
	.callbackPrepareRefreshEntity = CLG_misc_model_PrepareRefreshEntity,
	.class_locals_size = sizeof( clg_misc_model_locals_t )
};



/**
*
*
*	client_misc_te (te stands for, temp entity(event).
*
*
**/
/**
*	@brief	Precache function for 'client_misc_te' entity class type.
**/
void CLG_misc_te_Precache( clg_local_entity_t *self, const cm_entity_t *keyValues ) {
	// Get class.
	auto *selfClass = CLG_LocalEntity_GetClass<clg_misc_te_locals_t>( self );

	// Key/Value: 'event':
	if ( const cm_entity_t *eventTypeKv = clgi.CM_EntityKeyValue( keyValues, "event" ) ) {
		if ( eventTypeKv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
			selfClass->teEvent = static_cast<temp_entity_event_t>( eventTypeKv->integer );
		} else {
			selfClass->teEvent = static_cast<temp_entity_event_t>( 0 );
		}
	}

	// TODO: Other key values for Temp Event Entity.
	self->locals.modelindex = 0;

	// DEBNUG PRINT:
	clgi.Print( PRINT_DEVELOPER, "CLG_misc_te_Precache: event(%d)\n", selfClass->teEvent );
}

/**
*	@brief	Sets up the local client temp entity event entity.
**/
void CLG_misc_te_Spawn( clg_local_entity_t *self ) {
	// Setup appropriate mins, maxs.
	self->locals.mins = { -4, -4, -4 };
	self->locals.maxs = { 4, 4, 4 };

	// Link entity for this frame.
	CLG_LocalEntity_Link( self );

	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;
}
/**
*	@brief	Think once per local game tick.
**/
void CLG_misc_te_Think( clg_local_entity_t *self ) {
	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;
}
/**
*	@brief	Called each refresh frame.
**/
void CLG_misc_te_RefreshFrame( clg_local_entity_t *self ) {

}
/**
*	@brief	Gives the chance to prepare and add a 'refresh entity' for this client local's frame view.
**/
void CLG_misc_te_PrepareRefreshEntity( clg_local_entity_t *self ) {

}

// Class definition.
const clg_local_entity_class_t client_misc_te = {
	.classname = "client_misc_te",
	.callbackPrecache = CLG_misc_te_Precache,
	.callbackSpawn = CLG_misc_te_Spawn,
	.callbackThink = CLG_misc_te_Think,
	.callbackRFrame = CLG_misc_te_RefreshFrame,
	.callbackPrepareRefreshEntity = CLG_misc_te_PrepareRefreshEntity,
	.class_locals_size = sizeof( clg_misc_te_locals_t )
};



/**
*
*
*	client_misc_playermirror:
*
*
**/
/**
*	@brief	Precache function for 'client_misc_te' entity class type.
**/
void CLG_misc_playerholo_Precache( clg_local_entity_t *self, const cm_entity_t *keyValues ) {
	// Get class.
	auto *selfClass = CLG_LocalEntity_GetClass<clg_misc_playerholo_locals_t>( self );

	// Key/Value: 'frame':
	if ( const cm_entity_t *clientnumKv = clgi.CM_EntityKeyValue( keyValues, "clientnum" ) ) {
		if ( clientnumKv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
			selfClass->clientNumber = clientnumKv->integer;
		} else {
			selfClass->clientNumber = 0;
		}
	}

	// Set up the modelname for precaching the model with.
	self->locals.modelindex = MODELINDEX_PLAYER;
	self->locals.skin = 0;
	self->locals.skinNumber = 0;

	// DEBNUG PRINT:
	clgi.Print( PRINT_DEVELOPER, "CLG_misc_playerholo_Precache: model(%s), local_draw_model_handle(%d) frame(%d), skin(%d)\n", self->model, self->locals.modelindex, self->locals.frame, self->locals.skinNumber );
}

/**
*	@brief	Sets up the local client model entity.
**/
void CLG_misc_playerholo_Spawn( clg_local_entity_t *self ) {
	// Setup appropriate mins, maxs, absmins, absmaxs, size
	self->locals.mins = { -16, -16, -24 };
	self->locals.maxs = { 16, 16, 32 };

	// Link entity for this frame.
	CLG_LocalEntity_Link( self );

	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*	@brief	Think once per local game tick.
**/
void CLG_misc_playerholo_Think( clg_local_entity_t *self ) {
	// Get class.
	auto *selfClass = CLG_LocalEntity_GetClass<clg_misc_playerholo_locals_t>( self );

	// Update animation based on the client entity's last and current frame.
	centity_t *cent = clgi.client->clientEntity;
	if ( cent && cent->last_frame != cent->current_frame ) {
		// Update regular frame locals to the newly found client entity last and current frame.
		self->locals.frame = cent->current_frame;
		self->locals.oldframe = cent->last_frame;

		// Store entity specific needed frame_servertime as well.
		selfClass->frame_servertime = cent->frame_servertime;
	}

	// Negate angles so we face the client entity always.
	self->locals.angles = QM_Vector3Negate( clgi.client->playerEntityAngles );

	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*	@brief	Called each refresh frame.
**/
void CLG_misc_playerholo_RefreshFrame( clg_local_entity_t *self ) {

}

/**
*	@brief	Gives the chance to prepare and add a 'refresh entity' for this client local's frame view.
**/
void CLG_misc_playerholo_PrepareRefreshEntity( clg_local_entity_t *self ) {
	// Spawn flag.
	static constexpr int32_t SPAWNFLAG_CONNECTED_CLIENTS_ONLY = 8192;

	// Get class.
	auto *selfClass = CLG_LocalEntity_GetClass<clg_misc_playerholo_locals_t>( self );

	// Clamp client number value to prevent invalid index accessing.
	const int32_t clientInfoNumber = constclamp( selfClass->clientNumber, 0, MAX_CLIENTS );
	// Get the client info for the client that this client_misc_playerholo entity is a hologram of.
	clientinfo_t *clientInfo = &clgi.client->clientinfo[ selfClass->clientNumber ];
	
	// In case of it not being our base client info, make sure to check it has a valid model and skin.
	if ( clientInfo != &clgi.client->baseclientinfo ) {
		// We need it to have valid model and skin, otherwise resort to our own base client info instead.
		if ( !clientInfo || !clientInfo->model || !clientInfo->skin || !clientInfo->weaponmodel[ 0 ] ) {
			// In case the spawnflag is set for not rendering the model at all when there is no valid client info.
			if ( !(self->spawnflags & SPAWNFLAG_CONNECTED_CLIENTS_ONLY ) ) {
				clientInfo = &clgi.client->baseclientinfo;
			} else {
				clientInfo = nullptr;
			}
		}
	}

	// If not having any valid clientInfo data, don't add ourselves to the render frame's view.
	if ( !clientInfo ) {
		return;
	}

	// Clean slate refresh entity.
	entity_t rent = {};

	// Setup the refresh entity ID to start off at RENTITIY_OFFSET_LOCALENTITIES.
	rent.id = RENTITIY_OFFSET_LOCALENTITIES + self->id;

	// Copy spatial information over into the refresh entity.
	VectorCopy( self->locals.origin, rent.origin );
	VectorCopy( self->locals.origin, rent.oldorigin );
	VectorCopy( self->locals.angles, rent.angles );
	
	// Copy model information.
	rent.model = clientInfo->model; //clgi.client->clientinfo[ selfClass->clientNumber ].model;
	rent.skin = clientInfo->skin; //clgi.client->clientinfo[ selfClass->clientNumber ].skin;
	rent.skinnum = 0;
	rent.rgba.u32 = MakeColor( 255, 255, 255, 255 );

	// Copy general render properties.
	rent.alpha = 1.0f;
	rent.scale = 1.0f;

	// Calculate back lerpfraction at 10hz.
	rent.backlerp = 1.0f - ( ( clgi.client->time - ( (float)selfClass->frame_servertime - BASE_FRAMETIME ) ) / 100.f );
	clamp( rent.backlerp, 0.0f, 1.0f );

	rent.frame = self->locals.frame;
	rent.oldframe = self->locals.oldframe;

	// Add entity
	clgi.V_AddEntity( &rent );

	// Now prepare its weapon entity, to be added later on also.
	rent.skin = 0;
	rent.model = clientInfo->weaponmodel[ 0 ]; //clgi.R_RegisterModel( "players/male/weapon.md2" );
	// Add it to the view.
	clgi.V_AddEntity( &rent );
}

// Class definition.
const clg_local_entity_class_t client_misc_playerholo = {
	.classname = "client_misc_playerholo",
	.callbackPrecache = CLG_misc_playerholo_Precache,
	.callbackSpawn = CLG_misc_playerholo_Spawn,
	.callbackThink = CLG_misc_playerholo_Think,
	.callbackRFrame = CLG_misc_playerholo_RefreshFrame,
	.callbackPrepareRefreshEntity = CLG_misc_playerholo_PrepareRefreshEntity,
	.class_locals_size = sizeof( clg_misc_playerholo_locals_t )
};