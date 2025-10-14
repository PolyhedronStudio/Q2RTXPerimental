/********************************************************************
*
*
*   Shared Entity Functions:
*
*
********************************************************************/
#pragma once



/**
*   @brief  Encodes client index, weapon index, and viewheight.
**/
union sg_player_skinnum_t {
    //! Actual value.
    int32_t         skinnum;
    //! Union for easy access.
    struct {
        uint8_t     clientNumber;
        uint8_t     viewWeaponIndex;
        int8_t      viewHeight;
        //uint8_t     reserved;
    };
};



/**
*
*
*
*   Entity State:
*
*
*
**/
/**
*	@brief	Convert a player state to entity state.
**/
void SG_PlayerStateToEntityState( const int32_t clientNumber, player_state_t *playerState, entity_state_t *entityState, const bool snapOrigin = false );
