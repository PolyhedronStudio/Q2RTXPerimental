/**
*
*
*   (In-Game)Refresh Material Editor:
*
*
**/
#include "ui.h"
#include "../cl_client.h"

//! We need these here.
extern "C" {
    #include "refresh/vkpt/vkpt.h"
    #include "refresh/vkpt/material.h"
    #include "refresh/vkpt/shader/constants.h"
}
//! Enables material test model rendering.
//#define ENABLE_MATERIAL_TEST_MODEL_RENDERING


//! Model ID to use:
static constexpr int32_t ID_MODEL   = 103;
//! Skin ID to use:
static constexpr int32_t ID_SKIN    = 104;

/**
*   
**/
//list_t entry;
//char name[ MAX_QPATH ];
//char filename_base[ MAX_QPATH ];
//char filename_normals[ MAX_QPATH ];
//char filename_emissive[ MAX_QPATH ];
//char filename_mask[ MAX_QPATH ];
//char source_matfile[ MAX_QPATH ];
//uint32_t source_line;
//int original_width;
//int original_height;
//image_t *image_base;
//image_t *image_normals;
//image_t *image_emissive;
//image_t *image_mask;
//float bump_scale;
//float roughness_override;
//float metalness_factor;
//float emissive_factor;
//float specular_factor;
//float base_factor;
//uint32_t flags;
//int registration_sequence;
//int num_frames;
//int next_frame;
//bool light_styles;
//bool bsp_radiance;
//float default_radiance;
//imageflags_t image_flags;
//imagetype_t image_type;
//bool synth_emissive;
//int emissive_threshold;
// 
//! Menu data.
typedef struct m_rmaterial_s {
    //! Lol, irony, a menu title buffer lmao...
    char titleBuffer[ 1024 ];
    //! UI Menu:
    menuFrameWork_t     menu;
    //! UI Material Key Fields:
    struct {
        menuStatic_t        staticMaterialName;

        menuSpinControl_t   materialKind;

        menuField_t         filenameBaseTexture;
        menuField_t         filenameNormalsTexture;
        menuField_t         filenameEmissiveTexture;
        menuField_t         filenameMaskTexture;
        
        menuSlider_t        factorBase;
        menuSlider_t        factorEmissive;
        menuSlider_t        factorMetalness;
        menuSlider_t        factorSpecular;

        menuSlider_t        bumpScale;
        menuSlider_t        roughnessOverride;

        menuSpinControl_t   isLight; // Yes/No
        menuSpinControl_t   lightStyles; // Enabled/Disabled
        menuSpinControl_t   bspRadiance; // Yes/No

        menuSlider_t        defaultRadiance; // 0.0f to 1.0f TODO: Should be slider

        menuSpinControl_t   synthEmissiveTexture; // Yes/No
        menuField_t         emissiveThreshold; // 0 to 255

        // Apply/Cancel
        menuAction_t        actionSave;
        menuAction_t        actionCancel;
    } materialKeyFields;
    struct {
        // Holds a copy of editing material as it were, used to restore when cancelling.
        pbr_material_t materialDefaults = {};
        // For initializing/invalid mats.
        pbr_material_t nullMaterial = {};
        // Points to the actual material we are editing.
        pbr_material_t *material;
    } materialKeyValues;
    
    /**
    *   For 3D Test Model Rendering
    **/
    //! View area definition.
    refdef_t    refdef;
    //! Actual entities in view, model and light.
    entity_t    entities[ 2 ];
    //! Used for animating the model.
    uint64_t    time;
    uint64_t    oldTime;

    //char *pmnames[ MAX_PLAYERMODELS ];
    //char *pmskins[ MAX_PLAYERMODELS ];

    //char **_testPmNames;
    //int32_t _testPmNumNames;
    //char **_testPmSkins;
    //int32_t _testPmNumSkins;
} m_rmaterial_t;

static m_rmaterial_t m_rmaterial;

// To check how to act depending on refresh type.
extern cvar_t *vid_rtx;
extern "C" vkpt_refdef_t vkpt_refdef;


/**
*
*
*   SpinControl Options:
*
*
**/
//! For Material KIND SpinControl:
static const char *materialKind_ItemNames[] = {
    "INVALID",
    "REGULAR",
    "CHROME",
    "GLASS",
    "WATER",
    "LAVA",
    // WID: TODO: A sky can only be reverted if pt_showsky == 1... oof hehe..
    //"SKY",
    "SLIME",
    "INVISIBLE",
    "SCREEN",
    "CAMERA",
    "UNLIT",
    nullptr,
};

static const char *materialKind_ItemValues[] = {
    STRINGIFY( MATERIAL_KIND_INVALID ),
    STRINGIFY( MATERIAL_KIND_REGULAR ),
    STRINGIFY( MATERIAL_KIND_CHROME ),
    STRINGIFY( MATERIAL_KIND_GLASS ),
    STRINGIFY( MATERIAL_KIND_WATER ),
    STRINGIFY( MATERIAL_KIND_LAVA ),
    // WID: TODO: A sky can only be reverted if pt_showsky == 1... oof hehe..
    //STRINGIFY( MATERIAL_KIND_SKY ),
    STRINGIFY( MATERIAL_KIND_SLIME ),
    STRINGIFY( MATERIAL_KIND_INVISIBLE ),
    STRINGIFY( MATERIAL_KIND_SCREEN ),
    STRINGIFY( MATERIAL_KIND_CAMERA ),
    STRINGIFY( MATERIAL_KIND_UNLIT ),
    nullptr
};
// is_light
static const char *isLight_ItemNames[] = {
    "0",
    "1",
    nullptr
};
static const char *isLight_ItemValues[] = {
    "0",
    "1",
    nullptr
};
// LightStyles
static const char *lightStyles_ItemNames[] = {
    "Disabled",
    "Enabled",
    nullptr
};
static const char *lightStyles_ItemValues[] = {
    "0",
    "1",
    nullptr
};
// BSP Radiance
static const char *bspRadiance_ItemNames[] = {
    "0",
    "1",
    nullptr
};
static const char *bspRadiance_ItemValues[] = {
    "0",
    "1",
    nullptr
};

// Synth Emissive Texture
static const char *synthEmissive_ItemNames[] = {
    "No",
    "Yes",
    nullptr
};
static const char *synthEmissive_ItemValues[] = {
    "0",
    "1",
    nullptr
};

#ifdef ENABLE_MATERIAL_TEST_MODEL_RENDERING
static dlight_t dlights[] = {
    {.origin = { -120.f, -80.f, 80.f },.color = {1.f, 1.f, 1.f},.intensity = 200.f,.radius = 20.f },
    {.origin = { 100.f, 80.f, 20.f },.color = {0.5f, 0.5f, 1.f},.intensity = 200.f,.radius = 20.f }
};

static void ReloadMedia( void ) {
    char scratch[ MAX_QPATH ];
    char *model = uis.pmi[ m_rmaterial.model.curvalue ].directory;
    char *skin = uis.pmi[ m_rmaterial.model.curvalue ].skindisplaynames[ m_rmaterial.skin.curvalue ];

    m_rmaterial.refdef.num_entities = 0;

    Q_concat( scratch, sizeof( scratch ), "players/", model, "/tris.iqm" );
    m_rmaterial.entities[ 0 ].model = R_RegisterModel( scratch );
    if ( !m_rmaterial.entities[ 0 ].model )
        return;

    m_rmaterial.refdef.num_entities++;

    Q_concat( scratch, sizeof( scratch ), "players/", model, "/", skin, ".pcx" );
    m_rmaterial.entities[ 0 ].skin = R_RegisterSkin( scratch );

    if ( !uis.weaponModel[ 0 ] )
        return;

    Q_concat( scratch, sizeof( scratch ), "players/", model, "/", uis.weaponModel );
    m_rmaterial.entities[ 1 ].model = R_RegisterModel( scratch );
    if ( !m_rmaterial.entities[ 1 ].model )
        return;

    m_rmaterial.refdef.num_entities++;
}

static void RunFrame( void ) {
    int frame;
    int i;

    if ( m_rmaterial.time < uis.realtime ) {
        m_rmaterial.oldTime = m_rmaterial.time;

        m_rmaterial.time += 120;
        if ( m_rmaterial.time < uis.realtime ) {
            m_rmaterial.time = uis.realtime;
        }

        frame = ( m_rmaterial.time / 120 ) % 40;

        for ( i = 0; i < m_rmaterial.refdef.num_entities; i++ ) {
            m_rmaterial.entities[ i ].oldframe = m_rmaterial.entities[ i ].frame;
            m_rmaterial.entities[ i ].frame = frame;
        }
    }
}
#endif


/**
*
*
*   Menu Field Change Callbacks, these call upon the material adjustment commands.
*
*
**/
/**
*   @brief  Utilized, rolls back to default material properties as they were.
**/
void Menu_Material_RollBack() {

    // Get the matching material name for its current value.
    const int32_t kindValue = m_rmaterial.materialKeyValues.materialDefaults.flags;
    const char *kindValueStr = MAT_GetMaterialKindName( kindValue );
    // Generate cmd string.
    std::string cmdString = "mat kind " + std::string( kindValueStr ) + "\n";
    //Com_DPrintf( "%s\n", cmdString.c_str() );
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );
    // Get factor.
    const float base_factor = m_rmaterial.materialKeyValues.materialDefaults.base_factor;
    // Generate cmd string.
    cmdString = "mat base_factor " + std::to_string( base_factor ) + "\n";
    //Com_DPrintf( "%s\n", cmdString.c_str() );
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    // Get factor.
    const float emissive_factor = m_rmaterial.materialKeyValues.materialDefaults.emissive_factor;
    // Generate cmd string.
    cmdString = "mat emissive_factor " + std::to_string( emissive_factor ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    // Get factor.
    const float metalness_factor = m_rmaterial.materialKeyValues.materialDefaults.metalness_factor;
    // Generate cmd string.
    cmdString = "mat metalness_factor " + std::to_string( metalness_factor ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    // Get factor.
    const float specular_factor = m_rmaterial.materialKeyValues.materialDefaults.specular_factor;
    // Generate cmd string.
    cmdString = "mat specular_factor " + std::to_string( specular_factor ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    // Get factor.
    const float bump_scale = m_rmaterial.materialKeyValues.materialDefaults.bump_scale;
    // Generate cmd string.
    cmdString = "mat bump_scale " + std::to_string( bump_scale ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    // Get factor.
    const float roughness_override = m_rmaterial.materialKeyValues.materialDefaults.roughness_override;
    // Generate cmd string.
    cmdString = "mat roughness_override " + std::to_string( roughness_override ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    // Get factor.
    const int is_light = m_rmaterial.materialKeyValues.materialDefaults.flags & MATERIAL_FLAG_LIGHT;
    // Generate cmd string.
    cmdString = "mat is_light " + std::to_string( is_light) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    // Get factor.
    const int light_styles = m_rmaterial.materialKeyValues.materialDefaults.light_styles;
    // Generate cmd string.
    cmdString = "mat light_styles " + std::to_string( light_styles ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    // Get factor.
    const float bsp_radiance = m_rmaterial.materialKeyValues.materialDefaults.bsp_radiance;
    // Generate cmd string.
    cmdString = "mat bsp_radiance " + std::to_string( bsp_radiance ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    // Get factor.
    const float default_radiance = m_rmaterial.materialKeyValues.materialDefaults.default_radiance;
    // Generate cmd string.
    cmdString = "mat default_radiance " + std::to_string( default_radiance ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    // Get factor.
    const int32_t synth_emissive = m_rmaterial.materialKeyValues.materialDefaults.synth_emissive;
    // Generate cmd string.
    cmdString = "mat synth_emissive " + std::to_string( synth_emissive ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    // Get factor.
    const int32_t emissive_threshold = m_rmaterial.materialKeyValues.materialDefaults.emissive_threshold;
    // Generate cmd string.
    cmdString = "mat emissive_threshold " + std::string( m_rmaterial.materialKeyFields.emissiveThreshold.field.text ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );
}
/**
*   @brief  Adjusts material kind.
**/
menuSound_t MenuChange_Material_Kind( menuCommon_t *item ) {
    // Get the matching material name for its current value.
    const int32_t kindValue = m_rmaterial.materialKeyFields.materialKind.curvalue;
    // Ensure it is save in bounds. ( -1 for the nullptr... )
    if ( kindValue >= 0 && kindValue < q_countof( materialKind_ItemNames ) - 1 ) {
        // Get the string for the name.
        const char *kindValueStr = materialKind_ItemNames[ kindValue ];
        // Generate cmd string.
        std::string cmdString = "mat kind " + std::string( kindValueStr ) + "\n";
        Com_DPrintf( "%s\n", cmdString.c_str() );
        // Generate cmd command.
        Cmd_ExecuteString( cmd_current, cmdString.c_str() );
    }

    return QMS_MOVE;
}

/**
*   @brief  Adjusts 'base_factor'.
**/
menuSound_t MenuChange_Material_BaseFactor( menuCommon_t *item ) {
    // Get factor.
    const float factor = m_rmaterial.materialKeyFields.factorBase.curvalue;
    // Generate cmd string.
    std::string cmdString = "mat base_factor " + std::to_string( factor ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    return QMS_MOVE;
}
/**
*   @brief  Adjusts 'emissive_factor':
**/
menuSound_t MenuChange_Material_EmissiveFactor( menuCommon_t *item ) {
    // Get factor.
    const float factor = m_rmaterial.materialKeyFields.factorEmissive.curvalue;
    // Generate cmd string.
    std::string cmdString = "mat emissive_factor " + std::to_string( factor ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    return QMS_MOVE;
}
/**
*   @brief  Adjusts base factor.
**/
menuSound_t MenuChange_Material_MetalnessFactor( menuCommon_t *item ) {
    // Get factor.
    const float factor = m_rmaterial.materialKeyFields.factorMetalness.curvalue;
    // Generate cmd string.
    std::string cmdString = "mat metalness_factor " + std::to_string( factor ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    return QMS_MOVE;
}
/**
*   @brief  Adjusts 'specular_factor':
**/
menuSound_t MenuChange_Material_SpecularFactor( menuCommon_t *item ) {
    // Get factor.
    const float factor = m_rmaterial.materialKeyFields.factorSpecular.curvalue;
    // Generate cmd string.
    std::string cmdString = "mat specular_factor " + std::to_string( factor ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    return QMS_MOVE;
}

/**
*   @brief  Adjusts 'bump_scale':
**/
menuSound_t MenuChange_Material_BumpScale( menuCommon_t *item ) {
    // Get factor.
    const float factor = m_rmaterial.materialKeyFields.bumpScale.curvalue;
    // Generate cmd string.
    std::string cmdString = "mat bump_scale " + std::to_string( factor ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    return QMS_MOVE;
}
/**
*   @brief  Adjusts 'roughness_override':
**/
menuSound_t MenuChange_Material_RoughnessOverride( menuCommon_t *item ) {
    // Get factor.
    const float factor = m_rmaterial.materialKeyFields.roughnessOverride.curvalue;
    // Generate cmd string.
    std::string cmdString = "mat roughness_override " + std::to_string( factor ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    return QMS_MOVE;
}


/**
*   @brief  Adjusts 'is_light':
**/
menuSound_t MenuChange_Material_IsLight( menuCommon_t *item ) {
    // Get factor.
    const int factor = m_rmaterial.materialKeyFields.isLight.curvalue;
    // Generate cmd string.
    std::string cmdString = "mat is_light " + std::to_string( factor ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    return QMS_MOVE;
}
/**
*   @brief  Adjusts 'light_styles':
**/
menuSound_t MenuChange_Material_LightStyles( menuCommon_t *item ) {
    // Get factor.
    const int factor = m_rmaterial.materialKeyFields.lightStyles.curvalue;
    // Generate cmd string.
    std::string cmdString = "mat light_styles " + std::to_string( factor ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    return QMS_MOVE;
}
/**
*   @brief  Adjusts 'bsp_radiance':
**/
menuSound_t MenuChange_Material_BSPRadiance( menuCommon_t *item ) {
    // Get factor.
    const float factor = m_rmaterial.materialKeyFields.bspRadiance.curvalue;
    // Generate cmd string.
    std::string cmdString = "mat bsp_radiance " + std::to_string( factor ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    return QMS_MOVE;
}
/**
*   @brief  Adjusts 'default_radiance':
**/
menuSound_t MenuChange_Material_DefaultRadiance( menuCommon_t *item ) {
    // Get factor.
    const float factor = m_rmaterial.materialKeyFields.defaultRadiance.curvalue;
    // Generate cmd string.
    std::string cmdString = "mat default_radiance " + std::to_string( factor ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    return QMS_MOVE;
}

/**
*   @brief  Adjusts 'synth_emissive':
**/
menuSound_t MenuChange_Material_SynthesizeEmissiveTexture( menuCommon_t *item ) {
    // Get factor.
    const int32_t synth_emissive = m_rmaterial.materialKeyFields.synthEmissiveTexture.curvalue;
    // Generate cmd string.
    std::string cmdString = "mat synth_emissive " + std::to_string( synth_emissive ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    return QMS_MOVE;
}


/**
*   @brief  Adjusts 'emissive_threshold':
**/
menuSound_t MenuChange_Material_EmissiveThreshold( menuCommon_t *item ) {
    // Generate cmd string.
    std::string cmdString = "mat emissive_threshold " + std::string( m_rmaterial.materialKeyFields.emissiveThreshold.field.text ) + "\n";
    // Generate cmd command.
    Cmd_ExecuteString( cmd_current, cmdString.c_str() );

    return QMS_MOVE;
}

/**
*
*
*   Menu Core Callbacks:
*
*
**/
/**
*   @brief  Draw the refresh material editor menu.
**/
static void Draw( menuFrameWork_t *self ) {
    float backlerp;
    int i;

    // Process the 3D Material Test Model for a frame.
    #ifdef ENABLE_MATERIAL_TEST_MODEL_RENDERING
    m_rmaterial.refdef.time = uis.realtime * 0.001f;

    RunFrame();

    if ( m_rmaterial.time == m_rmaterial.oldTime ) {
        backlerp = 0;
    } else {
        backlerp = 1 - (float)( uis.realtime - m_rmaterial.oldTime ) /
            (float)( m_rmaterial.time - m_rmaterial.oldTime );
    }

    for ( i = 0; i < m_rmaterial.refdef.num_entities; i++ ) {
        m_rmaterial.entities[ i ].backlerp = backlerp;
    }
    #endif

    // Draw Editor Menu.
    Menu_Draw( self );

    // Draw 3D Material Test Model
    #ifdef ENABLE_MATERIAL_TEST_MODEL_RENDERING
    R_RenderFrame( &m_rmaterial.refdef );
    #endif

    R_SetScale( uis.scale );
}

static void Size( menuFrameWork_t *self ) {
    Menu_Size( self );
    return;

    int w = 0;// ( uis.width * 0.25 ) *uis.scale;
    int h = 0;// ( uis.height * 0.25 ) *uis.scale;
    //int x = ( uis.width * 0.5f ) - w * 0.5f;
    //int32_t numMenuItems = 5;
    //int y = ( uis.height * 0.5f ) - ( ( MENU_SPACING * numMenuItems ) *0.5f );

    // Configure the 3D Material Model its view render properties.
    #ifdef ENABLE_MATERIAL_TEST_MODEL_RENDERING
    m_rmaterial.refdef.x = w / 2;
    m_rmaterial.refdef.y = h / 10;
    m_rmaterial.refdef.width = w / 2;
    m_rmaterial.refdef.height = h - h / 5;

    m_rmaterial.refdef.fov_x = 90;
    m_rmaterial.refdef.fov_y = V_CalcFov( m_rmaterial.refdef.fov_x, m_rmaterial.refdef.width, m_rmaterial.refdef.height );
    #endif
}

/**
*   @brief  Menu field changed.
**/
static menuSound_t Change( menuCommon_t *self ) {
    switch ( self->id ) {
    #ifdef ENABLE_MATERIAL_TEST_MODEL_RENDERING
    case ID_MODEL:
        m_rmaterial.skin.itemnames =
            uis.pmi[ m_rmaterial.model.curvalue ].skindisplaynames;
        m_rmaterial.skin.curvalue = 0;
        SpinControl_Init( &m_rmaterial.skin );
        // fall through
    case ID_SKIN:
        ReloadMedia();
        break;
    #endif
    default:
        break;
    }
    return QMS_MOVE;
}

/**
*   For Applying Action.
**/
static void Pop_Apply( menuFrameWork_t *self ) {
    char scratch[ MAX_OSPATH ];

    // Disable bloom again.
    uis.bloomEnabled = true;
    // We won't be needing this however..
    #ifdef ENABLE_MATERIAL_TEST_MODEL_RENDERING
    //Cvar_SetEx( "name", m_rmaterial.name.field.text, FROM_CONSOLE );

    //Q_concat( scratch, sizeof( scratch ),
    //    uis.pmi[ m_rmaterial.model.curvalue ].directory, "/",
    //    uis.pmi[ m_rmaterial.model.curvalue ].skindisplaynames[ m_rmaterial.skin.curvalue ] );

    //Cvar_SetEx( "skin", scratch, FROM_CONSOLE );

    //Cvar_SetEx( "hand", va( "%d", m_rmaterial.hand.curvalue ), FROM_CONSOLE );

    //Cvar_SetEx( "aimfix", va( "%d", m_rmaterial.aimfix.curvalue ), FROM_CONSOLE );

    //Cvar_SetEx( "cl_player_model", va( "%d", m_rmaterial.view.curvalue ), FROM_CONSOLE );
    #endif
}
/**
*   @brief  For Cancel, will reset the material to what it once was.
**/
static void Pop_Cancel( menuFrameWork_t *self ) {
    // Disable bloom again.
    uis.bloomEnabled = true;

    // Reset the material its properties to what they were instead.
    if ( memcmp( m_rmaterial.materialKeyValues.material, &m_rmaterial.materialKeyValues.materialDefaults, sizeof( pbr_material_t ) ) ) {
        Menu_Material_RollBack();
    }    
}

static bool Push( menuFrameWork_t *self ) {
    char currentdirectory[ MAX_QPATH ];
    char currentskin[ MAX_QPATH ];
    int i, j;
    int currentdirectoryindex = 0;
    int currentskinindex = 0;
    char *p;

    // Find the material we want to edit.
    pbr_material_t *targetMaterial = nullptr;
    // Get pointer to the material.
    if ( vkpt_refdef.fd ) {
        targetMaterial = MAT_ForIndex( vkpt_refdef.fd->feedback.view_material_index );
    }
    // Exit, don't pop, we can't proceed.
    if ( !targetMaterial ) {
        return false;
    }
    // Store a copy to reset to if cancelling editing.
    m_rmaterial.materialKeyValues.materialDefaults = *targetMaterial;
    // Store pointer to actual material being edited.
    m_rmaterial.materialKeyValues.material = targetMaterial;

    // Disable bloom, we want to see the material.
    uis.bloomEnabled = false;
    // We don't want the client to pause, this seems to get in the way.
    //cl_paused->integer = 0;

    // Register the 3D Material Test Model(s)
    #ifdef ENABLE_MATERIAL_TEST_MODEL_RENDERING
    if ( !uis.numPlayerModels ) {
        PlayerModel_Load();
        if ( !uis.numPlayerModels ) {
            Com_Printf( "No player models found.\n" );
            return false;
        }
    }

    // Get skin.
    Cvar_VariableStringBuffer( "skin", currentdirectory, sizeof( currentdirectory ) );

    if ( ( p = strchr( currentdirectory, '/' ) ) || ( p = strchr( currentdirectory, '\\' ) ) ) {
        *p++ = 0;
        Q_strlcpy( currentskin, p, sizeof( currentskin ) );
    } else {
        strcpy( currentdirectory, "playerdummy" );
        strcpy( currentskin, "playerdummy" );
    }

    for ( i = 0; i < 8; i++ ) {
        m_rmaterial.pmnames[ i ] = uis.pmi[ i ].directory;
        if ( Q_stricmp( uis.pmi[ i ].directory, currentdirectory ) == 0 ) {
            currentdirectoryindex = i;

            for ( j = 0; j < uis.pmi[ i ].nskins; j++ ) {
                if ( Q_stricmp( uis.pmi[ i ].skindisplaynames[ j ], currentskin ) == 0 ) {
                    currentskinindex = j;
                    break;
                }
            }
        }
    }
    #endif
    /**
    *   Material Filename this Material resides in.
    **/
    //strcpy( m_rmaterial.materialKeyValues.material->source_matfile, "materials/pbr02.mat" );
    m_rmaterial.materialKeyFields.staticMaterialName.generic.type = MTYPE_STATIC;
    m_rmaterial.materialKeyFields.staticMaterialName.generic.uiFlags = UI_CENTER;
    m_rmaterial.materialKeyFields.staticMaterialName.generic.flags = QMF_DISABLED | QMF_GRAYED;
    m_rmaterial.materialKeyFields.staticMaterialName.generic.name = m_rmaterial.materialKeyValues.material->name;
    //m_rmaterial.materialKeyFields.staticMaterialName.generic.width = 32 - 1;
    m_rmaterial.materialKeyFields.staticMaterialName.maxChars = 64;

    /**
    *   Material Kind:
    **/
    m_rmaterial.materialKeyFields.materialKind.generic.type = MTYPE_SPINCONTROL;
    m_rmaterial.materialKeyFields.materialKind.generic.name = "Material Kind";
    m_rmaterial.materialKeyFields.materialKind.generic.flags = 0;
    //m_rmaterial.materialKeyFields.materialKind.curvalue = m_rmaterial.materialKeyValues.material->;
    m_rmaterial.materialKeyFields.materialKind.itemnames = (char **)materialKind_ItemNames;
    m_rmaterial.materialKeyFields.materialKind.itemvalues = (char **)materialKind_ItemValues;
    m_rmaterial.materialKeyFields.materialKind.curvalue = 1; // "REGULAR" kind.
    m_rmaterial.materialKeyFields.materialKind.generic.change = MenuChange_Material_Kind;
    // Iterate over the actual material kinds to determine value to use.
    for ( int32_t i = 0; i < q_countof( materialKind_ItemNames ); i++ ) {
        // Break out.
        if ( materialKind_ItemValues[ i ] == nullptr ) {
            break;
        }
        // Get string.
        const char *kindValueStr = materialKind_ItemNames[ i ];
        // Get integer.
        const char *matKindName = MAT_GetMaterialKindName( m_rmaterial.materialKeyValues.material->flags );

        // Found the matching index.
        if ( !strcmp( matKindName, kindValueStr ) ) {
            // Set value.
            m_rmaterial.materialKeyFields.materialKind.curvalue = i;
            // Stop seeking.
            break;
        }
    }

    /**
    *   Texture File Path Fields:
    **/
    // [TEXTURE_BASE]:
    IF_Init( 
        &m_rmaterial.materialKeyFields.filenameBaseTexture.field, 
        32,
        m_rmaterial.materialKeyFields.filenameBaseTexture.width
    );
    IF_Replace( &m_rmaterial.materialKeyFields.filenameBaseTexture.field, m_rmaterial.materialKeyValues.material->filename_base );
    // [TEXTURE_NORMALS]:
    IF_Init(
        &m_rmaterial.materialKeyFields.filenameNormalsTexture.field,
        32,
        m_rmaterial.materialKeyFields.filenameNormalsTexture.width
    );
    IF_Replace( &m_rmaterial.materialKeyFields.filenameNormalsTexture.field, m_rmaterial.materialKeyValues.material->filename_normals );
    // [TEXTURE_EMISSIVE]:
    IF_Init(
        &m_rmaterial.materialKeyFields.filenameEmissiveTexture.field,
        32,
        m_rmaterial.materialKeyFields.filenameEmissiveTexture.width
    );
    IF_Replace( &m_rmaterial.materialKeyFields.filenameEmissiveTexture.field, m_rmaterial.materialKeyValues.material->filename_emissive );
    // [TEXTURE_MASK]:
    IF_Init(
        &m_rmaterial.materialKeyFields.filenameMaskTexture.field,
        32,
        m_rmaterial.materialKeyFields.filenameMaskTexture.width
    );
    IF_Replace( &m_rmaterial.materialKeyFields.filenameMaskTexture.field, m_rmaterial.materialKeyValues.material->filename_mask );

    /**
    *   Sliders for factors:
    **/
    m_rmaterial.materialKeyFields.factorBase.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->base_factor, 0.f, 10.f );
    m_rmaterial.materialKeyFields.factorEmissive.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->emissive_factor, 0.f, 1.f );
    m_rmaterial.materialKeyFields.factorMetalness.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->metalness_factor, 0.f, 1.f );
    m_rmaterial.materialKeyFields.factorSpecular.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->specular_factor, 0.f, 1.f );

    /**
    *   Bump Scale/Roughness Override Sliders:
    **/
    m_rmaterial.materialKeyFields.bumpScale.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->bump_scale, 0.f, 10.f );
    m_rmaterial.materialKeyFields.roughnessOverride.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->roughness_override, -1.f, 1.f );

    /**
    *   LightStyle/BSPRadiance/DefaultRadiance:
    **/
    m_rmaterial.materialKeyFields.isLight.curvalue = ( m_rmaterial.materialKeyValues.material->flags & MATERIAL_FLAG_LIGHT ? 1 : 0 );
    m_rmaterial.materialKeyFields.lightStyles.curvalue = ( m_rmaterial.materialKeyValues.material->light_styles ? 1 : 0 );
    m_rmaterial.materialKeyFields.bspRadiance.curvalue = ( m_rmaterial.materialKeyValues.material->bsp_radiance ? 1 : 0 );
    m_rmaterial.materialKeyFields.defaultRadiance.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->default_radiance, 0.f, 1.f );

    /**
    *   Synth Emissive/Emissive Threshhold:
    **/
    m_rmaterial.materialKeyFields.synthEmissiveTexture.curvalue = QM_ClampInt32( m_rmaterial.materialKeyValues.material->synth_emissive, 0, 1 );
    // [EMISSIVE_THRESHOLD]:
    IF_Init(
        &m_rmaterial.materialKeyFields.emissiveThreshold.field,
        3,
        m_rmaterial.materialKeyFields.emissiveThreshold.width
    );
    std::string threshold = std::to_string( QM_ClampInt32( m_rmaterial.materialKeyValues.material->emissive_threshold, 0, 255 ) );
    IF_Replace( &m_rmaterial.materialKeyFields.emissiveThreshold.field, threshold.c_str() );


    #ifdef ENABLE_MATERIAL_TEST_MODEL_RENDERING
    m_rmaterial.model.curvalue = currentdirectoryindex;
    m_rmaterial.model.itemnames = m_player.pmnames;

    m_rmaterial.skin.curvalue = currentskinindex;
    m_rmaterial.skin.itemnames = uis.pmi[ currentdirectoryindex ].skindisplaynames;
    #endif

    // Annoying but this way we can manage the title. I am merely adding this editor in for ease of use and all that.
    // So yeah, cheap code, I hate UI work.
    memset( m_rmaterial.titleBuffer, 0, sizeof( m_rmaterial.titleBuffer ) );
    if ( m_rmaterial.materialKeyValues.material->source_matfile[ 0 ] ) {
        Q_scnprintf( m_rmaterial.titleBuffer, 1024, "Editing RMaterial[\"%s\"] - File[%s:%d]\n",
            m_rmaterial.materialKeyValues.material->name, m_rmaterial.materialKeyValues.material->source_matfile, m_rmaterial.materialKeyValues.material->source_line );
    } else {
        Q_scnprintf( m_rmaterial.titleBuffer, 1024, "Editing RMaterial[\"%s\"] - [Automatically Generated]\n"
            , m_rmaterial.materialKeyValues.material->name );
    }
    m_rmaterial.menu.title = m_rmaterial.titleBuffer;
 
    // Load media and run for a frame.
    #ifdef ENABLE_MATERIAL_TEST_MODEL_RENDERING
    ReloadMedia();

    // set up oldframe correctly
    m_rmaterial.time = uis.realtime - 120;
    m_rmaterial.oldTime = m_rmaterial.time;
    RunFrame();
    #endif

    return true;
}

/**
*   @brief  Clear menu memory.
**/
static void Free( menuFrameWork_t *self ) {
    Z_Free( m_rmaterial.menu.items );
    memset( &m_rmaterial, 0, sizeof( m_rmaterial ) );
}

//menuFrameWork_t *menu, menuAction_t *a, int32_t key
int32_t MenuAction_Save_Callback( menuFrameWork_t *menu, menuAction_t *a, int32_t key ) {
    if ( key == K_KP_ENTER || key == K_ENTER ) {
        Com_DPrintf( "Applied Material Changes\n" );
        menu->pop = Pop_Apply;
        UI_ForceMenuOff();
        return QMS_BEEP;
    } else {
        return QMS_NOTHANDLED;
    }
}
int32_t MenuAction_Cancel_Callback( menuFrameWork_t *menu, menuAction_t *a, int32_t key ) {
    if ( key == K_KP_ENTER || key == K_ENTER ) {
        menu->pop = Pop_Cancel;
        UI_ForceMenuOff();
        return QMS_OUT;
    } else {
        return QMS_NOTHANDLED;
    }
}

/**
*   @brief
**/
static menuSound_t Keydown_Material_Fields( menuCommon_t *item, int key ) {
    // Ignore autorepeats
    if ( Key_IsDown( key ) > 1 ) {
        return QMS_NOTHANDLED;
    }

    // We know it is a field so, lazy.
    menuField_t *field = (menuField_t *)item;
    if ( key == K_ENTER || key == K_KP_ENTER ) {
        // Special handling for 'texture_base'.
        if ( field == &m_rmaterial.materialKeyFields.filenameBaseTexture ) {
            std::string fieldValue = field->field.text;
            // Generate cmd string.
            std::string cmdString = "mat texture_base ";
            if ( fieldValue.empty() ) {
                cmdString += "\"\"";
            } else {
                cmdString += fieldValue;
            }
            // Debug print.
            Com_DPrintf( "%s\n", cmdString.c_str() );
            // Generate cmd command.
            Cmd_ExecuteString( cmd_current, cmdString.c_str() );
            return QMS_MOVE;
        }
        // Special handling for 'texture_normals'.
        if ( field == &m_rmaterial.materialKeyFields.filenameNormalsTexture ) {
            std::string fieldValue = field->field.text;
            // Generate cmd string.
            std::string cmdString = "mat texture_normals ";
            if ( fieldValue.empty() ) {
                cmdString += "\"\"";
            } else {
                cmdString += fieldValue;
            }
            // Debug print.
            Com_DPrintf( "%s\n", cmdString.c_str() );
            // Generate cmd command.
            Cmd_ExecuteString( cmd_current, cmdString.c_str() );
            return QMS_MOVE;
        }
        // Special handling for 'texture_emissive'.
        if ( field == &m_rmaterial.materialKeyFields.filenameEmissiveTexture) {
            std::string fieldValue = field->field.text;
            // Generate cmd string.
            std::string cmdString = "mat texture_emissive ";
            if ( fieldValue.empty() ) {
                cmdString += "\"\"";
            } else {
                cmdString += fieldValue;
            }
            // Debug print.
            Com_DPrintf( "%s\n", cmdString.c_str() );
            // Generate cmd command.
            Cmd_ExecuteString( cmd_current, cmdString.c_str() );
            return QMS_MOVE;
        }
        // Special handling for 'texture_mask'.
        if ( field == &m_rmaterial.materialKeyFields.filenameMaskTexture ) {
            std::string fieldValue = field->field.text;
            // Generate cmd string.
            std::string cmdString = "mat texture_mask ";
            if ( fieldValue.empty() ) {
                cmdString += "\"\"";
            } else {
                cmdString += fieldValue;
            }
            // Debug print.
            Com_DPrintf( "%s\n", cmdString.c_str() );
            // Generate cmd command.
            Cmd_ExecuteString( cmd_current, cmdString.c_str() );
            return QMS_MOVE;
        }
        // Special handling for 'emissive_threshold'.
        if ( field == &m_rmaterial.materialKeyFields.emissiveThreshold ) {
            std::string fieldValue = field->field.text;
            // Generate cmd string.
            std::string cmdString = "mat emissive_threshold ";
            if ( fieldValue.empty() ) {
                cmdString += "\"\"";
            } else {
                cmdString += fieldValue;
            }
            // Debug print.
            Com_DPrintf( "%s\n", cmdString.c_str() );
            // Generate cmd command.
            Cmd_ExecuteString( cmd_current, cmdString.c_str() );
            return QMS_MOVE;
        }
    }

    return QMS_NOTHANDLED;
}

/**
*   @brief  Setup the refresh material editor UI.
**/
void M_Menu_Editor_RefreshMaterial( void ) {
    static const vec3_t origin = { 40.0f, 0.0f, 0.0f };
    static const vec3_t angles = { 0.0f, 260.0f, 0.0f };

    m_rmaterial.menu.name = "editor_rmaterial";
    m_rmaterial.menu.push = Push;
    // Set to cancel by default, so it will undo any active changes.
    m_rmaterial.menu.pop = Pop_Cancel;
    m_rmaterial.menu.size = Size;
    m_rmaterial.menu.draw = Draw;
    m_rmaterial.menu.free = Free;
    if ( cls.ref_type == REF_TYPE_VKPT ) {
        // Q2RTX: make the player menu transparent so that we can see 
        // the model below: all 2D stuff is rendered after 3D, in stretch_pics.
        //m_rmaterial.menu.color.u32 = 0;
    } else {
        //m_rmaterial.menu.color.u32 = MakeColor( 0, 0, 0, 255 );
    }
    m_rmaterial.menu.color.u32 = MakeColor( 0, 0, 0, 255 );


    // Setup the 3D view and entities for the Test Model to render in:
    #ifdef ENABLE_MATERIAL_TEST_MODEL_RENDERING
    m_rmaterial.entities[ 0 ].flags = RF_FULLBRIGHT;
    m_rmaterial.entities[ 0 ].id = 1; // Q2RTX: add entity id to fix motion vectors
    VectorCopy( angles, m_rmaterial.entities[ 0 ].angles );
    VectorCopy( origin, m_rmaterial.entities[ 0 ].origin );
    VectorCopy( origin, m_rmaterial.entities[ 0 ].oldorigin );

    m_rmaterial.entities[ 1 ].flags = RF_FULLBRIGHT;
    m_rmaterial.entities[ 1 ].id = 2; // Q2RTX: add entity id to fix motion vectors
    VectorCopy( angles, m_rmaterial.entities[ 1 ].angles );
    VectorCopy( origin, m_rmaterial.entities[ 1 ].origin );
    VectorCopy( origin, m_rmaterial.entities[ 1 ].oldorigin );

    m_rmaterial.refdef.num_entities = 0;
    m_rmaterial.refdef.entities = m_rmaterial.entities;
    m_rmaterial.refdef.rdflags = RDF_NOWORLDMODEL;

    if ( cls.ref_type == REF_TYPE_VKPT ) {
        m_rmaterial.refdef.num_dlights = sizeof( dlights ) / sizeof( *dlights );
        m_rmaterial.refdef.dlights = dlights;
    } else {
        m_rmaterial.refdef.num_dlights = 0;
    }
    #endif

    // We have no material so point to fake 'null' one.
    MAT_Reset( &m_rmaterial.materialKeyValues.nullMaterial );
    MAT_Reset( &m_rmaterial.materialKeyValues.materialDefaults );
    m_rmaterial.materialKeyValues.material = &m_rmaterial.materialKeyValues.nullMaterial;

    /**
    *   Material Filename this Material resides in.
    **/
    strcpy( m_rmaterial.materialKeyValues.material->source_matfile, "materials/null.mat" );
    m_rmaterial.materialKeyFields.staticMaterialName.generic.type = MTYPE_STATIC;
    m_rmaterial.materialKeyFields.staticMaterialName.generic.uiFlags = UI_CENTER;
    m_rmaterial.materialKeyFields.staticMaterialName.generic.flags = QMF_DISABLED | QMF_GRAYED;
    m_rmaterial.materialKeyFields.staticMaterialName.generic.name = m_rmaterial.materialKeyValues.material->source_matfile;
    //m_rmaterial.materialKeyFields.staticMaterialName.generic.width = 32 - 1;
    m_rmaterial.materialKeyFields.staticMaterialName.maxChars = 64;

    /**
    *   Material Kind:
    **/
    m_rmaterial.materialKeyFields.materialKind.generic.type = MTYPE_SPINCONTROL;
    m_rmaterial.materialKeyFields.materialKind.generic.name = "Material Kind";
    m_rmaterial.materialKeyFields.materialKind.generic.flags = 0;
    //m_rmaterial.materialKeyFields.materialKind.curvalue = m_rmaterial.materialKeyValues.material->;
    m_rmaterial.materialKeyFields.materialKind.itemnames = (char **)materialKind_ItemNames;
    m_rmaterial.materialKeyFields.materialKind.itemvalues = (char **)materialKind_ItemValues;
    m_rmaterial.materialKeyFields.materialKind.curvalue = 1; // "REGULAR" kind.
    // Iterate over the actual material kinds to determine value to use.
    for ( int32_t i = 0; i < q_countof( materialKind_ItemNames ); i++ ) {
        // Break out.
        if ( materialKind_ItemValues[ i ] == nullptr ) {
            break;
        }
        // Get string.
        const char *kindValueStr = materialKind_ItemValues[ i ];
        // Get integer.
        const int32_t kindFlags = atoi( kindValueStr );

        // Found the matching index.
        if ( m_rmaterial.materialKeyValues.material->flags & kindFlags ) {
            // Set value.
            m_rmaterial.materialKeyFields.materialKind.curvalue = i;
            // Stop seeking.
            break;
        }
    }

    /**
    *   Texture File Path Fields:
    **/
    m_rmaterial.materialKeyFields.filenameBaseTexture.generic.type = MTYPE_FIELD;
    m_rmaterial.materialKeyFields.filenameBaseTexture.generic.flags = QMF_HASFOCUS;
    m_rmaterial.materialKeyFields.filenameBaseTexture.generic.name = "Diffuse Map";
    m_rmaterial.materialKeyFields.filenameBaseTexture.generic.keydown = Keydown_Material_Fields;
    //m_rmaterial.materialKeyFields.filenameBaseTexture.generic.width = 64;
    m_rmaterial.materialKeyFields.filenameBaseTexture.width = 64;

    m_rmaterial.materialKeyFields.filenameNormalsTexture.generic.type = MTYPE_FIELD;
    m_rmaterial.materialKeyFields.filenameNormalsTexture.generic.flags = 0;// |= QMF_LEFT_JUSTIFY;
    m_rmaterial.materialKeyFields.filenameNormalsTexture.generic.name = "Normal Map";
    m_rmaterial.materialKeyFields.filenameNormalsTexture.generic.keydown = Keydown_Material_Fields;
    //m_rmaterial.materialKeyFields.filenameNormalsTexture.generic.width = 32;
    m_rmaterial.materialKeyFields.filenameNormalsTexture.width = 64;

    m_rmaterial.materialKeyFields.filenameEmissiveTexture.generic.type = MTYPE_FIELD;
    m_rmaterial.materialKeyFields.filenameEmissiveTexture.generic.flags = 0;// |= QMF_LEFT_JUSTIFY;
    m_rmaterial.materialKeyFields.filenameEmissiveTexture.generic.name = "Emissive Map";
    m_rmaterial.materialKeyFields.filenameEmissiveTexture.generic.keydown = Keydown_Material_Fields;
    //m_rmaterial.materialKeyFields.filenameEmissiveTexture.generic.width = 32 - 1;
    m_rmaterial.materialKeyFields.filenameEmissiveTexture.width = 64;

    m_rmaterial.materialKeyFields.filenameMaskTexture.generic.type = MTYPE_FIELD;
    m_rmaterial.materialKeyFields.filenameMaskTexture.generic.flags = 0;//|= QMF_LEFT_JUSTIFY;
    m_rmaterial.materialKeyFields.filenameMaskTexture.generic.name = "Mask Map";
    m_rmaterial.materialKeyFields.filenameMaskTexture.generic.keydown = Keydown_Material_Fields;
    //m_rmaterial.materialKeyFields.filenameMaskTexture.generic.width = 32 - 1;
    m_rmaterial.materialKeyFields.filenameMaskTexture.width = 64;

    /**
    *   Sliders for factors:
    **/
    m_rmaterial.materialKeyFields.factorBase.generic.type = MTYPE_SLIDER;
    m_rmaterial.materialKeyFields.factorBase.generic.name = "Base Factor";
    m_rmaterial.materialKeyFields.factorBase.generic.flags = 0;
    m_rmaterial.materialKeyFields.factorBase.generic.change = MenuChange_Material_BaseFactor;
    m_rmaterial.materialKeyFields.factorBase.minvalue = 0;
    m_rmaterial.materialKeyFields.factorBase.maxvalue = 10;
    m_rmaterial.materialKeyFields.factorBase.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->base_factor, 0.f, 10.f );
    m_rmaterial.materialKeyFields.factorBase.format = UI_CopyString( "%.1f" );
    m_rmaterial.materialKeyFields.factorBase.step = 0.1f;

    m_rmaterial.materialKeyFields.factorEmissive.generic.type = MTYPE_SLIDER;
    m_rmaterial.materialKeyFields.factorEmissive.generic.name = "Emissive Factor";
    m_rmaterial.materialKeyFields.factorEmissive.generic.flags = 0;
    m_rmaterial.materialKeyFields.factorEmissive.generic.change = MenuChange_Material_EmissiveFactor;
    m_rmaterial.materialKeyFields.factorEmissive.minvalue = 0;
    m_rmaterial.materialKeyFields.factorEmissive.maxvalue = 1;
    m_rmaterial.materialKeyFields.factorEmissive.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->emissive_factor, 0.f, 1.f );
    m_rmaterial.materialKeyFields.factorEmissive.format = UI_CopyString( "%.4f" );
    m_rmaterial.materialKeyFields.factorEmissive.step = 0.0125f;

    m_rmaterial.materialKeyFields.factorMetalness.generic.type = MTYPE_SLIDER;
    m_rmaterial.materialKeyFields.factorMetalness.generic.name = "Metalness Factor";
    m_rmaterial.materialKeyFields.factorMetalness.generic.flags = 0;
    m_rmaterial.materialKeyFields.factorMetalness.generic.change = MenuChange_Material_MetalnessFactor;
    m_rmaterial.materialKeyFields.factorMetalness.minvalue = 0;
    m_rmaterial.materialKeyFields.factorMetalness.maxvalue = 1;
    m_rmaterial.materialKeyFields.factorMetalness.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->metalness_factor, 0.f, 1.f );
    m_rmaterial.materialKeyFields.factorMetalness.format = UI_CopyString( "%.4f" );
    m_rmaterial.materialKeyFields.factorMetalness.step = 0.0125f;

    m_rmaterial.materialKeyFields.factorSpecular.generic.type = MTYPE_SLIDER;
    m_rmaterial.materialKeyFields.factorSpecular.generic.name = "Specular Factor";
    m_rmaterial.materialKeyFields.factorSpecular.generic.flags = 0;
    m_rmaterial.materialKeyFields.factorSpecular.generic.change = MenuChange_Material_SpecularFactor;
    m_rmaterial.materialKeyFields.factorSpecular.minvalue = 0;
    m_rmaterial.materialKeyFields.factorSpecular.maxvalue = 1;
    m_rmaterial.materialKeyFields.factorSpecular.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->specular_factor, 0.f, 1.f );
    m_rmaterial.materialKeyFields.factorSpecular.format = UI_CopyString( "%.4f" );
    m_rmaterial.materialKeyFields.factorSpecular.step = 0.0125f;

    /**
    *   Bump Scale/Roughness Override Sliders:
    **/
    m_rmaterial.materialKeyFields.bumpScale.generic.type = MTYPE_SLIDER;
    m_rmaterial.materialKeyFields.bumpScale.generic.name = "Bump Scale";
    m_rmaterial.materialKeyFields.bumpScale.generic.flags = 0;
    m_rmaterial.materialKeyFields.bumpScale.generic.change = MenuChange_Material_BumpScale;
    m_rmaterial.materialKeyFields.bumpScale.minvalue = 0;
    m_rmaterial.materialKeyFields.bumpScale.maxvalue = 10;
    m_rmaterial.materialKeyFields.bumpScale.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->bump_scale, 0.f, 10.f );
    m_rmaterial.materialKeyFields.bumpScale.format = UI_CopyString( "%.4f" );
    m_rmaterial.materialKeyFields.bumpScale.step = 0.0125f;

    m_rmaterial.materialKeyFields.roughnessOverride.generic.type = MTYPE_SLIDER;
    m_rmaterial.materialKeyFields.roughnessOverride.generic.name = "Roughness Override";
    m_rmaterial.materialKeyFields.roughnessOverride.generic.flags = 0;
    m_rmaterial.materialKeyFields.roughnessOverride.generic.change = MenuChange_Material_RoughnessOverride;
    m_rmaterial.materialKeyFields.roughnessOverride.minvalue = -1;
    m_rmaterial.materialKeyFields.roughnessOverride.maxvalue = 1;
    m_rmaterial.materialKeyFields.roughnessOverride.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->roughness_override, -1.f, 1.f );
    m_rmaterial.materialKeyFields.roughnessOverride.format = UI_CopyString( "%.1f" );
    m_rmaterial.materialKeyFields.roughnessOverride.step = 0.1f;

    /**
    *   IsLight/LightStyle/BSPRadiance/DefaultRadiance:
    **/
    m_rmaterial.materialKeyFields.isLight.generic.type = MTYPE_SPINCONTROL;
    m_rmaterial.materialKeyFields.isLight.generic.name = "Is Light";
    m_rmaterial.materialKeyFields.isLight.generic.flags = 0;
    m_rmaterial.materialKeyFields.isLight.generic.change = MenuChange_Material_IsLight;
    m_rmaterial.materialKeyFields.isLight.curvalue = ( m_rmaterial.materialKeyValues.material->flags & MATERIAL_FLAG_LIGHT ? 1 : 0 );
    m_rmaterial.materialKeyFields.isLight.itemnames = (char **)isLight_ItemNames;
    m_rmaterial.materialKeyFields.isLight.itemvalues = (char **)isLight_ItemValues;

    m_rmaterial.materialKeyFields.lightStyles.generic.type = MTYPE_SPINCONTROL;
    m_rmaterial.materialKeyFields.lightStyles.generic.name = "Light Styles";
    m_rmaterial.materialKeyFields.lightStyles.generic.flags = 0;
    m_rmaterial.materialKeyFields.lightStyles.generic.change = MenuChange_Material_LightStyles;
    m_rmaterial.materialKeyFields.lightStyles.curvalue = ( m_rmaterial.materialKeyValues.material->light_styles ? 1 : 0 );
    m_rmaterial.materialKeyFields.lightStyles.itemnames = (char**)lightStyles_ItemNames;
    m_rmaterial.materialKeyFields.lightStyles.itemvalues= (char**)lightStyles_ItemValues;

    m_rmaterial.materialKeyFields.bspRadiance.generic.type = MTYPE_SPINCONTROL;
    m_rmaterial.materialKeyFields.bspRadiance.generic.name = "BSP Radiance";
    m_rmaterial.materialKeyFields.bspRadiance.generic.flags = 0;
    m_rmaterial.materialKeyFields.bspRadiance.generic.change = MenuChange_Material_BSPRadiance;
    m_rmaterial.materialKeyFields.bspRadiance.curvalue = ( m_rmaterial.materialKeyValues.material->bsp_radiance ? 1 : 0 );
    m_rmaterial.materialKeyFields.bspRadiance.itemnames = (char **)bspRadiance_ItemNames;
    m_rmaterial.materialKeyFields.bspRadiance.itemvalues = (char **)bspRadiance_ItemValues;

    m_rmaterial.materialKeyFields.defaultRadiance.generic.type = MTYPE_SLIDER;
    m_rmaterial.materialKeyFields.defaultRadiance.generic.name = "Default Radiance";
    m_rmaterial.materialKeyFields.defaultRadiance.generic.flags = 0;
    m_rmaterial.materialKeyFields.defaultRadiance.generic.change = MenuChange_Material_DefaultRadiance;
    m_rmaterial.materialKeyFields.defaultRadiance.minvalue = 0;
    m_rmaterial.materialKeyFields.defaultRadiance.maxvalue = 1;
    m_rmaterial.materialKeyFields.defaultRadiance.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->default_radiance, 0.f, 1.f );
    m_rmaterial.materialKeyFields.defaultRadiance.format = UI_CopyString( "%.1f" );
    m_rmaterial.materialKeyFields.defaultRadiance.step = 0.1f;

    /**
    *   Synth Emissive/Emissive Threshhold:
    **/
    m_rmaterial.materialKeyFields.synthEmissiveTexture.generic.type = MTYPE_SPINCONTROL;
    m_rmaterial.materialKeyFields.synthEmissiveTexture.generic.name = "Synthesize Emissive Texture";
    m_rmaterial.materialKeyFields.synthEmissiveTexture.generic.flags = 0;
    m_rmaterial.materialKeyFields.synthEmissiveTexture.generic.change = MenuChange_Material_SynthesizeEmissiveTexture;
    m_rmaterial.materialKeyFields.synthEmissiveTexture.curvalue = QM_Clampf( m_rmaterial.materialKeyValues.material->synth_emissive, 0.f, 1.f );
    m_rmaterial.materialKeyFields.synthEmissiveTexture.itemnames = (char **)synthEmissive_ItemNames;
    m_rmaterial.materialKeyFields.synthEmissiveTexture.itemvalues = (char **)synthEmissive_ItemValues;

    m_rmaterial.materialKeyFields.emissiveThreshold.generic.type = MTYPE_FIELD;
    m_rmaterial.materialKeyFields.emissiveThreshold.generic.flags = QMF_NUMBERSONLY;
    m_rmaterial.materialKeyFields.emissiveThreshold.generic.name = "Emissive Threshold(0 - 255)";
    //m_rmaterial.materialKeyFields.emissiveThreshold.generic.change = MenuChange_Material_EmissiveThreshold;
    m_rmaterial.materialKeyFields.emissiveThreshold.generic.keydown = Keydown_Material_Fields;
    m_rmaterial.materialKeyFields.emissiveThreshold.generic.width = 3;
    m_rmaterial.materialKeyFields.emissiveThreshold.width = 3;
    const int32_t clampedThreshold = QM_ClampInt32( m_rmaterial.materialKeyValues.material->emissive_threshold, 0, 255 );
    Q_snprintf( m_rmaterial.materialKeyFields.emissiveThreshold.field.text, 3, "%d", clampedThreshold );


    /**
    *   Cancel/Apply Action Buttons:
    **/
    m_rmaterial.materialKeyFields.actionSave.generic.type = MTYPE_ACTION;
    m_rmaterial.materialKeyFields.actionSave.generic.name = "Save Changes";
    m_rmaterial.materialKeyFields.actionSave.generic.uiFlags = UI_LEFT | UI_RIGHT;
    m_rmaterial.materialKeyFields.actionSave.generic.flags = 0;
    m_rmaterial.materialKeyFields.actionSave.callback = MenuAction_Save_Callback;

    m_rmaterial.materialKeyFields.actionCancel.generic.type = MTYPE_ACTION;
    m_rmaterial.materialKeyFields.actionCancel.generic.name = "Cancel";
    m_rmaterial.materialKeyFields.actionCancel.generic.uiFlags = UI_LEFT | UI_RIGHT;
    m_rmaterial.materialKeyFields.actionCancel.generic.flags = 0;
    m_rmaterial.materialKeyFields.actionCancel.callback = MenuAction_Cancel_Callback;

    //m_rmaterial.model.generic.type = MTYPE_SPINCONTROL;
    //m_rmaterial.model.generic.id = ID_MODEL;
    //m_rmaterial.model.generic.name = "model";
    //m_rmaterial.model.generic.change = Change;

    //m_rmaterial.skin.generic.type = MTYPE_SPINCONTROL;
    //m_rmaterial.skin.generic.id = ID_SKIN;
    //m_rmaterial.skin.generic.name = "skin";
    //m_rmaterial.skin.generic.change = Change;

    //m_rmaterial.hand.generic.type = MTYPE_SPINCONTROL;
    //m_rmaterial.hand.generic.name = "handedness";
    //m_rmaterial.hand.itemnames = (char **)handedness;

    //m_rmaterial.aimfix.generic.type = MTYPE_SPINCONTROL;
    //m_rmaterial.aimfix.generic.name = "aiming point";
    //m_rmaterial.aimfix.itemnames = (char **)aiming_points;

    //m_rmaterial.view.generic.type = MTYPE_SPINCONTROL;
    //m_rmaterial.view.generic.name = "view";
    //m_rmaterial.view.itemnames = (char **)viewmodes;
    m_rmaterial.menu.compact = true;
    m_rmaterial.menu.transparent = true;

    // Looks faaackt lol.
    //Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.staticMaterialName );

    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.materialKind );

    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.filenameBaseTexture );
    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.filenameNormalsTexture );
    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.filenameEmissiveTexture );
    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.filenameMaskTexture );

    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.factorBase );
    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.factorEmissive );
    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.factorMetalness );
    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.factorSpecular );
    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.bumpScale );
    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.roughnessOverride );

    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.isLight );
    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.lightStyles );
    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.bspRadiance );

    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.defaultRadiance );

    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.synthEmissiveTexture );
    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.emissiveThreshold );

    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.actionSave );
    Menu_AddItem( &m_rmaterial.menu, &m_rmaterial.materialKeyFields.actionCancel );

    Menu_Init( &m_rmaterial.menu );

    List_Append( &ui_menus, &m_rmaterial.menu.entry );
}

