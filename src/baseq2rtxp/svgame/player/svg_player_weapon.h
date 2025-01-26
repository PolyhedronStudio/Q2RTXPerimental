/********************************************************************
*
*
*	ServerGame: Player Weapon Functionality:
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*   @detail Each player can have two noise objects associated with it:
*           a personal noise (jumping, pain, weapon firing), and a weapon
*           target noise (bullet wall impacts)
*
*           Monsters that don't directly see the player can move
*           to a noise in hopes of seeing the player from there.
**/
void SVG_Player_PlayerNoise( edict_t *who, const vec3_t where, int type );

/**
*   @brief  Wraps up the new more modern SVG_Player_ProjectDistance.
**/
void SVG_Player_ProjectDistance( edict_t *ent, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result );
/**
*   @brief Project the 'ray of fire' from the source to its (source + dir * distance) target.
**/
const Vector3 SVG_Player_ProjectDistance( edict_t *ent, const Vector3 &point, const Vector3 &distance, const Vector3 &forward, const Vector3 &right );
/**
*   @brief Project the 'ray of fire' from the source, and then target the crosshair/scope's center screen
*          point as the final destination.
*   @note   The forward vector is normalized.
**/
void SVG_Player_ProjectSource( edict_t *ent, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result );


/**
*   @brief  Acts as a sub method for cleaner code, used by weapon item animation data precaching.
**/
void SVG_Player_Weapon_SetModeAnimationFromSKM( weapon_item_info_t *itemInfo, const skm_anim_t *iqmAnim, const int32_t modeID, const int32_t iqmAnimationID );
/**
*   @brief  Will iterate the model animations, assigning animationIDs for the matching queried weapon mode its animation names.
*   @note   modeAnimations and modeAnimationNames are expected to be of size WEAPON_MODE_MAX!!
*   @return True if all animation names were matched and precached with the SKM data.
**/
const bool SVG_Player_Weapon_PrecacheItemInfo( weapon_item_info_t *weaponItemInfo, const std::string &weaponName, const std::string &weaponViewModel );
/**
*   @brief  Called when a weapon item has been touched.
**/
const bool SVG_Player_Weapon_Pickup( edict_t *ent, edict_t *other );
/**
*   @brief  Called if the weapon item is wanted to be dropped by the player.
**/
void SVG_Player_Weapon_Drop( edict_t *ent, const gitem_t *inv );
/**
*   @brief  Will prepare a switch to the newly passed weapon.
**/
void SVG_Player_Weapon_Use( edict_t *ent, const gitem_t *inv );
/**
*   @brief  Called when the 'Old Weapon' has been dropped all the way. This function will
*           make the 'newweapon' as the client's current weapon.
**/
void SVG_Player_Weapon_Change( edict_t *ent );
/**
*   @brief  Will switch the weapon to its 'newMode' if it can, unless enforced(force == true).
**/
void SVG_Player_Weapon_SwitchMode( edict_t *ent, const weapon_mode_t newMode, const weapon_mode_animation_t *weaponModeAnimations, const bool force );
/**
*   @brief  Advances the animation of the 'mode' we're currently in.
**/
const bool SVG_Player_Weapon_ProcessModeAnimation( edict_t *ent, const weapon_mode_animation_t *weaponModeAnimation );
/**
*   @brief  Perform the weapon's logical 'think' routine. This is Is either
*           called by SVG_Client_BeginServerFrame or ClientThink.
**/
void SVG_Player_Weapon_Think( edict_t *ent, const bool processUserInputOnly );