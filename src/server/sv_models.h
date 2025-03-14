/*********************************************************************
*
*
*	Server: Models.
*
*
********************************************************************/
#pragma once


/**
*   @brief  Increments registration sequence for loading.
**/
void SV_Models_IncrementRegistrationSequence();
/**
*   @brief
**/
void SV_Models_Reference( struct model_s *model );
/**
*   @brief
**/
qhandle_t SV_RegisterModel( const char *name );
/**
*   @brief  Pointer to model data matching the name, otherwise a (nullptr) on failure.
**/
struct model_s *SV_Models_Find( const char *name );
/**
*   @return Pointer to model data matching the resource handle, otherwise a (nullptr) on failure.
**/
struct model_s *SV_Models_ForHandle( qhandle_t h );
/**
*   @brief  Initialize server models memory cache.
**/
void SV_Models_Init( void );
/**
*   @brief  Shutdown and free server models memory cache.
**/
void SV_Models_Shutdown( void );
/**
*   @brief
**/
void SV_Models_FreeUnused( void );
/**
*   @brief
**/
void SV_Models_FreeAll( void );
//typedef struct maliasmesh_s {
//    int             numverts;
//    int             numtris;
//    int             numindices;
//    int             numskins;
//    int             tri_offset; /* offset in vertex buffer on device */
//    int *indices;
//    vec3_t *positions;
//    vec3_t *normals;
//    vec2_t *tex_coords;
//    vec3_t *tangents;
//    uint32_t *blend_indices; // iqm only
//    uint32_t *blend_weights; // iqm only
//    struct pbr_material_s **materials;
//    bool            handedness;
//} maliasmesh_t;