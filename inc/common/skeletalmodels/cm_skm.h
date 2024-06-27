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
const float SKM_CalculateFramePoseTranslation( iqm_model_t *iqmModel, const int32_t frame, const int32_t oldFrame, const int32_t rootBoneID, const uint32_t axisFlags, Vector3 &translation );
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
skm_bone_node_t *SKM_GetBoneByName( model_t *model, const char *boneName );
/**
*   @brief  Returns a pointer to the first bone that has a matching number.
**/
skm_bone_node_t *SKM_GetBoneByNumber( model_t *model, const int32_t boneNumber );



#ifdef __cplusplus
}
#endif // #ifdef __cplusplus