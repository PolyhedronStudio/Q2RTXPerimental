/********************************************************************
*
*
*	ServerGame: UseTargets
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



//! edict->entityUseFlags
typedef enum {
    //! No (+usetarget) key press supporting entity at all.
    ENTITY_USETARGET_FLAG_NONE = 0,
    //! Pressable UseTarget( (+usetarget) single key press activates )
    ENTITY_USETARGET_FLAG_PRESS = BIT( 0 ),
    //! Toggleable UseTarget( (+usetarget) single key press toggles, activates at end of press movement ).
    ENTITY_USETARGET_FLAG_TOGGLE = BIT( 1 ),
    //! Continuous UseTarget( (+usetarget) key held down, activates continuous ).
    ENTITY_USETARGET_FLAG_CONTINUOUS = BIT( 2 ),
    //! Should send +/- 1 values.
    //ENTITY_USETARGET_FLAG_DIRECTIONAL   = BIT( 3 ),

    //! For temporarily disabling this useTarget.
    ENTITY_USETARGET_FLAG_DISABLED = BIT( 4 ),
} entity_usetarget_flags_t;
// Enumerator Type Bit Flags Support:
QENUM_BIT_FLAGS( entity_usetarget_flags_t );
//! edict->entityUseState
typedef enum {
    //! Generic state.
    ENTITY_USETARGET_STATE_DEFAULT = 0,
    //! 'OFF' state.
    ENTITY_USETARGET_STATE_OFF = ENTITY_USETARGET_STATE_DEFAULT,//BIT( 0 ),
    //! 'ON' state.
    ENTITY_USETARGET_STATE_ON = BIT( 0 ),
    //! The entity its usetarget state has been toggled.
    ENTITY_USETARGET_STATE_TOGGLED = BIT( 1 ),
    //! Entity is continuously 'usetargetted'.
    ENTITY_USETARGET_STATE_CONTINUOUS = BIT( 2 ),
} entity_usetarget_state_t;
// Enumerator Type Bit Flags Support:
QENUM_BIT_FLAGS( entity_usetarget_state_t );
/**
*   @brief  For SVG_UseTargets
**/
typedef enum {
    //! Triggers as 'Off' type.
    ENTITY_USETARGET_TYPE_OFF = 0,
    //! Triggers as 'On' type.
    ENTITY_USETARGET_TYPE_ON,
    //! Triggers as 'Set' type.
    ENTITY_USETARGET_TYPE_SET,
    //! Triggers as 'Toggle' type.
    ENTITY_USETARGET_TYPE_TOGGLE
} entity_usetarget_type_t;
// Enumerator Type Bit Flags Support:
//QENUM_BIT_FLAGS( entity_usetarget_type_t );

/**
*
*
*
*   UseTargets:
*
*
*
**/
/**
*   @brief  Use to check and prevent an entity from reacting in case it is spammed by
*           ON or OFF typed triggering.
**/
static inline const bool SVG_UseTarget_ShouldToggle( const entity_usetarget_type_t useType, const int32_t currentState ) {
    // We always toggle for USE_TOGGLE and USE_SET:
    if ( useType != ENTITY_USETARGET_TYPE_TOGGLE && useType != ENTITY_USETARGET_TYPE_SET ) {
        // If its current state is 'ON' and useType is 'ON', don't toggle:
        if ( currentState && useType == ENTITY_USETARGET_TYPE_ON ) {
            return false;
        }
        // If its current state is 'OFF' and useType is 'OFF', don't toggle:
        if ( !currentState && useType == ENTITY_USETARGET_TYPE_OFF ) {
            return false;
        }
    }

    return true;
}
