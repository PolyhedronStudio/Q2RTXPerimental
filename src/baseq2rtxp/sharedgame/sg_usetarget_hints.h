/********************************************************************
*
*
*	SharedGame: UseTarget Hints Data Array. Used for displaying info
*				when targeting a (not per seUseTarget) capable entity.
*
*
********************************************************************/
#pragma once


/**
*
*
*   UseTarget Hint IDs:
*
*
**/
/**
*   @brief  Actual IDs for indexing the right UseTarget array string data.
**/
typedef enum sg_usetarget_hint_id_e {
    USETARGET_HINT_ID_NONE = 0,

    USETARGET_HINT_ID_CUSTOM_0,
    USETARGET_HINT_ID_CUSTOM_1,

    USETARGET_HINT_ID_DOOR_OPEN,
    USETARGET_HINT_ID_DOOR_CLOSE,
    //USETARGET_HINT_ID_DOOR_LOCKED,

    USETARGET_HINT_ID_BUTTON_PRESS,
    USETARGET_HINT_ID_BUTTON_UNPRESS,
    USETARGET_HINT_ID_BUTTON_TOGGLE,
    USETARGET_HINT_ID_BUTTON_UNTOGGLE,
    USETARGET_HINT_ID_BUTTON_HOLD,
    USETARGET_HINT_ID_BUTTON_UNHOLD,

    // Max.
    USETARGET_HINT_ID_MAX
} sg_usetarget_hint_id_t;
//! Flags type.
typedef enum sg_usetarget_hint_flags_e {
    //! Default.
    SG_USETARGET_HINT_FLAGS_NONE = 0,

    //! Dispo
    SG_USETARGET_HINT_FLAGS_DISPLAY = BIT( 0 ),
} sg_usetarget_hint_flags_t;

/**
*
*
*   UseTarget Hints Data Array:
*
*
**/
/**
*   @brief  Stores the data of use target hint information.
**/
typedef struct sg_usetarget_hint_s {
    //! The actual numeric index, meant to match with the enum ID.
    const sg_usetarget_hint_id_t index;
    //! Possible flags suggesting display settings.
    const sg_usetarget_hint_flags_t flags;
    //! The hint display string.
    const char *hintString;
} sg_usetarget_hint_t;
/**
*	@brief  Array storing common hint data information for when hovering/targeting entities
*           that have UseTarget capabilities set. 
*
*           This so that only for 'uncommon/very custom specific' cases we need to send 
*           along the actual string display data to a specific client.
**/
inline constexpr const sg_usetarget_hint_t useTargetHints[] = {
    // For no useTarget at all.
    { USETARGET_HINT_ID_NONE,       SG_USETARGET_HINT_FLAGS_NONE,  nullptr },
    // For custom strings that came in by a usetarget hint string cmd. (We still use them for indicing.
    { USETARGET_HINT_ID_CUSTOM_0,   SG_USETARGET_HINT_FLAGS_NONE,  "" },
    { USETARGET_HINT_ID_CUSTOM_1,   SG_USETARGET_HINT_FLAGS_NONE,  "" },
    // For Door UseTargets, default hint information.
    { USETARGET_HINT_ID_DOOR_OPEN,  SG_USETARGET_HINT_FLAGS_NONE,  "Press [+usetarget] to (+Open) this (Door)" },
    { USETARGET_HINT_ID_DOOR_CLOSE, SG_USETARGET_HINT_FLAGS_NONE,  "Press [+usetarget] to (-Close) this (Door)" },
    // For Button UseTargets, default hint information.
    { USETARGET_HINT_ID_BUTTON_PRESS,       SG_USETARGET_HINT_FLAGS_NONE,  "Press [+usetarget] to (+Activate) this (Button)" },
    { USETARGET_HINT_ID_BUTTON_UNPRESS,     SG_USETARGET_HINT_FLAGS_NONE,  "Press [+usetarget] to (-Deactivate) this (Button)" },
    { USETARGET_HINT_ID_BUTTON_TOGGLE,      SG_USETARGET_HINT_FLAGS_NONE,  "Press [+usetarget] to (+Toggle) this (Button)" },
    { USETARGET_HINT_ID_BUTTON_UNTOGGLE,    SG_USETARGET_HINT_FLAGS_NONE,  "Press [+usetarget] to (-UnToggle) this (Button)" },
    // For Hold UseTargets, default hint information.
    { USETARGET_HINT_ID_BUTTON_HOLD,    SG_USETARGET_HINT_FLAGS_NONE,  "Hold the [+usetarget] key to (+Use) this (Button)" },
    { USETARGET_HINT_ID_BUTTON_UNHOLD,  SG_USETARGET_HINT_FLAGS_NONE,  "Release the [+usetarget] key to (-Stop) using this (Button)" },

    // End.
    { USETARGET_HINT_ID_MAX, SG_USETARGET_HINT_FLAGS_NONE, nullptr }
};
// Total useTargetHints.
static const int32_t useTargetHintsCount = sizeof( useTargetHints );



/**
*
*
*   UseTarget Functions:
*
*
**/
/**
*   @brief  If id == 0, returns an empty hint, meaning nothing.
*           If id > 0, it iterates the UseTargetHint array in order to find one with a matching ID.
*           If id < 0, it will return the specific hint index meant for custom strings.
**/
const sg_usetarget_hint_t *SG_UseTargetHint_GetByID( const int32_t id );