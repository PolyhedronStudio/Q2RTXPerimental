/********************************************************************
*
*
*	ClientGame: View Scene Management.
*
*
********************************************************************/
#include "clg_local.h"


//=============
//
// development tools for weapons
//
//qhandle_t gun;

/**
*
*
*   Field of View.
* 
*
**/
/**
*   @brief  
**/
const float PF_CalculateFieldOfView( const float fov_x, const float width, const float height ) {
    float    a;
    float    x;

    if ( fov_x <= 0 || fov_x > 179 )
        Com_Error( ERR_DROP, "%s: bad fov: %f", __func__, fov_x );

    x = width / tan( fov_x * ( M_PI / 360 ) );

    a = atan( height / x );
    a = a * ( 360 / M_PI );

    return a;
}


/**
*
*
*   View/Scene Entities:
*
*
**/
/**
*   @brief  
**/
void PF_ClearViewScene( void ) {
    //cl.viewScene.r_numdlights = 0;
    //cl.viewScene.r_numentities = 0;
    //cl.viewScene.r_numparticles = 0;
}

/**
*   @brief  Prepares the current frame's view scene for the refdef by
*           emitting all frame data(entities, particles, dynamic lights, lightstyles,
*           and temp entities) to the refresh definition.
**/
void PF_PrepareViewEntites( void ) {

    // Add all 'in-frame' entities, also known as packet entities, to the rendered view.
    CLG_AddPacketEntities();

    // Add any and all other special FX.
    CLG_AddTEnts();
    CLG_AddParticles();
    CLG_AddDLights();
    CLG_AddLightStyles();

    // Add in .md2/.md3/.iqm model 'debugger' entity.
    //CLG_AddTestModel();

    //! Add in the client-side flashlight.
    //if ( cl_flashlight->integer ) {
    //    //CLG_View_Flashlight();
    //}
}