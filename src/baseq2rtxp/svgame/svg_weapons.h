/********************************************************************
*
*
*	ServerGame: Weapons
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*   @brief  Describes a weapon's current state.
**/
typedef enum weapon_mode_e {
    //! The weapon is not doing anything else but sitting there, waiting for use.
    WEAPON_MODE_IDLE,

    //! The weapon is being 'Drawn'.
    WEAPON_MODE_DRAWING,
    //! The weapon is being 'Holstered'.
    WEAPON_MODE_HOLSTERING,

    //! The weapon is actively 'Primary' firing a shot.
    WEAPON_MODE_PRIMARY_FIRING,
    //! The weapon is actively 'Secondary' firing a shot.
    WEAPON_MODE_SECONDARY_FIRING,

    //! The weapon is actively 'Reloading' its clip.
    WEAPON_MODE_RELOADING,

    //! The weapon is 'aiming in' to the center of screen.
    WEAPON_MODE_AIM_IN,
    //! The weapon is 'firing' in 'Aimed' mode.
    WEAPON_MODE_AIM_FIRE,
    //! The weapon is idle and 'aimed' to the center of screen.
    WEAPON_MODE_AIM_IDLE,
    //! The weapon is 'aiming out' to the center of screen.
    WEAPON_MODE_AIM_OUT,

    //! Maximum weapon modes available.
    WEAPON_MODE_MAX,
} weapon_mode_t;

/**
*   @brief  Used for constructing a weapon's 'mode' animation setup.
**/
typedef struct weapon_mode_animation_s {
    //! The animation name to search and precache data from the viewmodel.
    const char *skmAnimationName;
    //! IQM Model Animation Index.
    int32_t modelAnimationID;
    //! (0 to start) Absolute frame index.
    int32_t startFrame;
    //! (start to end) Absolute frame index.
    int32_t endFrame;
    //! Relative animation frame duration ( endFrame - startFrame ) * frame_msec.
    QMTime duration;
} weapon_mode_animation_t;

/**
*   @brief  Used as an item's internal 'info' pointer for describing the actual
*           weapon's properties.
**/
typedef struct weapon_item_info_s {
    //! Weapon Mode (ViewModel-)Animations.
    weapon_mode_animation_t modeAnimations[ WEAPON_MODE_MAX ];
    //! 
    //! TODO: Other info.
} weapon_item_info_t;
