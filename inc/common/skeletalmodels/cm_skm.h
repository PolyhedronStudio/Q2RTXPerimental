/********************************************************************
*
*
*	Skeletal Model Functions
* 
*
********************************************************************/
#pragma once



#ifdef __cplusplus
extern "C" {
#endif // #ifdef __cplusplus



/**
*	@brief	Support method for calculating the exact translation of the root bone, in between frame poses.
**/
const float SKM_CalculateFramePoseTranslation( skm_model_t *iqmModel, const int32_t frame, const int32_t oldFrame, const int32_t rootBoneID, const uint32_t axisFlags, Vector3 *translation );
/**
*	@brief
**/
const skm_rootmotion_set_t *SKM_GenerateRootMotionSet( model_t *model, const int32_t rootBoneID, const int32_t axisFlags );



/**
*
*
*	Skeletal Model Common:
*
*
**/
/**
*   @brief  Returns a pointer to the first bone that has a matching name.
**/
skm_bone_node_t *SKM_GetBoneByName( const model_t *model, const char *boneName );
/**
*   @brief  Returns a pointer to the first bone that has a matching number.
**/
skm_bone_node_t *SKM_GetBoneByNumber( const model_t *model, const int32_t boneNumber );



/**
*
*
*
*	Skeletal Model(IQM) Pose Transform and Lerping:
*
*
*
**/
/**
*	@return	A pointer to the model's bone poses for 'frame'. 
*			Frame will wrap around in case of exceeding the frame pose limit.
**/
const skm_transform_t *SKM_GetBonePosesForFrame( const model_t *model, const int32_t frame );
/**
*	@brief	:erped pose transformations between frameBonePoses and oldFrameBonePoses.
*			'outBonePose' must have enough room for model->num_poses
**/
void SKM_LerpBonePoses( const model_t *model, const skm_transform_t *frameBonePoses, const skm_transform_t *oldFrameBonePoses, const float frontLerp, const float backLerp, skm_transform_t *outBonePose, const int32_t rootMotionBoneID, const int32_t rootMotionAxisFlags );
/**
*	@brief	Compute pose transformations for the given model + data
*			'outBonePose' must have enough room for model->num_poses
**/
void SKM_ComputeLerpBonePoses( const model_t *model, const int32_t frame, const int32_t oldFrame, const float frontLerp, const float backLerp, skm_transform_t *outBonePose, const int32_t rootMotionBoneID, const int32_t rootMotionAxisFlags );
/**
*	@brief	Combine 2 poses into one by performing a recursive blend starting from the given boneNode, using the given fraction as "intensity".
*	@param	fraction		When set to 1.0, it blends in the animation at 100% intensity. Take 0.5 for example,
*							and a tpose(frac 0.5)+walk would have its arms half bend.
*	@param	addBonePose		The actual animation that you want to blend in on top of inBonePoses.
*	@param	addToBonePose	A lerped bone pose which we want to blend addBonePoses animation on to.
**/
void SKM_RecursiveBlendFromBone( const skm_transform_t *addBonePoses, skm_transform_t *addToBonePoses, const skm_bone_node_t *boneNode, const skm_bone_node_t **excludeNodes, const int32_t numExcludeNodes, const double backLerp, const double fraction );
/**
*	@brief	Compute "Local/Model-Space" matrices for the given pose transformations.
**/
void SKM_TransformBonePosesLocalSpace( const skm_model_t *model, const skm_transform_t *relativeBonePose, const skm_bone_controller_t *boneControllers, float *pose_matrices );
/**
*	@brief	Compute "World-Space" matrices for the given pose transformations.
*			This is generally a slower procedure, but can be used to get the
*			actual bone's position for in-game usage.
*
*	@note	Before calling this function it is expected that the bonePoses
*			have already been transformed into local model space.
**/
void SKM_TransformBonePosesWorldSpace( const skm_model_t *model, const skm_transform_t *relativeBonePose, float *pose_matrices );



#ifdef __cplusplus
}
#endif // #ifdef __cplusplus