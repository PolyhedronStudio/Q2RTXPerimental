/********************************************************************
*
*
*   Entity Events:
*
*
********************************************************************/
#pragma once



/**
*
*
*
*   Entity Events:
*
*
*
**/
/**
*	@description entity_state_t->event values
*				 Ertity events are for effects that take place reletive
*				 to an existing entities origin. Very network efficient.
*				 All muzzle flashes really should be converted to events...
**/
typedef enum sg_entity_events_e {
    /**
    *   Footstep Events:
    **/
    #if 1
    //! For players.
    EV_PLAYER_FOOTSTEP = EV_ENGINE_MAX,
	//! For other entities.
    EV_OTHER_FOOTSTEP,
    #else
    //! For players and monsters.
    EV_FOOTSTEP = EV_ENGINE_MAX,
    #endif
	//! For ladder climbing entities.
    EV_FOOTSTEP_LADDER,

    /**
	*   Water (Footstep-)Splash Events:
    **/
	    //! For when the feet touch water.
    EV_WATER_ENTER_FEET, // Also plays a possible footstep sound.
    //! For when deep diving in water.
    EV_WATER_ENTER_WAIST,
    //! For when the head goes underwater.
    EV_WATER_ENTER_HEAD,
    //! For when the feet leave water.
    EV_WATER_LEAVE_FEET,
    //! For when waist leaves water.
    EV_WATER_LEAVE_WAIST,
    //! For when head leaves water.
    EV_WATER_LEAVE_HEAD,

    /**
    *   Player/(Humanoid-)Monster Events:
    **/
    //! For when jumping up.
    EV_JUMP_UP,
    //! Or Fall, FallShort, FallFar??
    EV_JUMP_LAND,

    //! "Short" Fell to the ground.
    EV_FALL_SHORT,
    //! "Short" fall to the ground.
    EV_FALL_MEDIUM,
    // "Hard Far" fall to the ground.
    EV_FALL_FAR,


    //
    // External Events:
    //
    //! Reload Weapon.
    EV_WEAPON_RELOAD,
    //! Weapon Primary Fire.
    EV_WEAPON_PRIMARY_FIRE,
    //! Weapon Secondary Fire.
    EV_WEAPON_SECONDARY_FIRE,

    //! Weapon Holster/Draw:
    EV_WEAPON_HOLSTER_AND_DRAW,

    // TODO: We really do wanna split weapon draw and holstering, but, alas, lack animation skills.
    //! Draw Weapon.
    EV_WEAPON_DRAW,
    //! Holster Weapon.
    EV_WEAPON_HOLSTER,

    //!
    //! The maximum predictable player events.
    //! 
    PS_EV_MAX,

    /**
    *   Teleport Effects:
    **/
    //! Player Login.
    EV_PLAYER_LOGIN,
    //! Player Logout.
    EV_PLAYER_LOGOUT,
    //! Player teleporting.
    EV_PLAYER_TELEPORT, // <Q2RTXP>: TODO: PS_EV_MAX? Or... just use these for PS_EV_...

    //! Other entity teleporting.
    EV_OTHER_TELEPORT,

    /**
    *   Item Events:
    **/
    EV_ITEM_RESPAWN,


    /******************************************
    *   Temporary (EXTERNAL)-Entity Events:
    *
    *   (Can be spawned too instead of being latched on to an entity, as they will be created as an entity of their own.)
    *******************************************/
    /**
    *	Sound Events:
    **/
    //! General sound event for entities, play a sound on the (client's-) entity playing on the specified channel.
    EV_GENERAL_SOUND,
    //! Same as EV_GENERAL_SOUND but with attenuation parameter.
    EV_GENERAL_SOUND_EX,
    //! Positioned sound event for entities, play a sound at the designated position, or
    //! at the entity's origin if used with an entity. The sound will remain persistent at
    //! said position.
    EV_POSITIONED_SOUND,
    //! Global sound event, will play at a client's head so the sound is not attenuated. (No diminishing.)
    EV_GLOBAL_SOUND,

	/**
	*	"Blood/Damage/Hurt/Pain" Particle Events:
	**/
	//! A "proper" blood impact
	EV_FX_BLOOD,
	//! A "proper" MORE blood impact.
	EV_FX_MORE_BLOOD,

	/**
	*   "Impact" Particle Events:
	**/
	//! Gunshot impact effects/sparks.
	EV_FX_IMPACT_GUNSHOT,
	EV_FX_IMPACT_SPARKS,
	EV_FX_IMPACT_BULLET_SPARKS,

    /**
    *   "Splash" Particle Events:
    **/
    //! Unknown splash type effect.
    EV_FX_SPLASH_UNKNOWN,
    //! Spark splash effect.
    EV_FX_SPLASH_SPARKS,
    //! A blue water splash effect.
    EV_FX_SPLASH_WATER_BLUE,
    //! A blue water splash effect.
    EV_FX_SPLASH_WATER_BROWN,
    //! A green slime splash effect.
    EV_FX_SPLASH_SLIME,
    //! A red lava splash effect.
    EV_FX_SPLASH_LAVA,
    //! A red blood splash effect.
    EV_FX_SPLASH_BLOOD,

	/**
	*	"Trail" Particle Events:
	**/
	//! Bubble trail effect 01.
	EV_FX_TRAIL_BUBBLES01,
	//! Bubble trail effect 02. CLG_FX_BubbleTrail2 (lets you control the # of bubbles by setting the distance between the spawns).
	EV_FX_TRAIL_BUBBLES02,
	//! For debugging purposes.
	EV_FX_TRAIL_DEBUG_LINE,

    /**
    *   Maximum number of entity events, must be last.
    **/
    EV_GAME_MAX
} sg_entity_events_t;



/**
*
*
*
*   Entity Event String Names:
*
*
*
**/
static constexpr const char *sg_event_string_names[ /*EV_GAME_MAX*/ ] = {
    "EV_ENGINE_NONE",

    /**
    *   Footstep Events:
    **/
    #if 1
    //! For players.
    "EV_PLAYER_FOOTSTEP",
	//! For other entities.
    "EV_OTHER_FOOTSTEP",
    #else
    //! For players and monsters.
    EV_FOOTSTEP = EV_ENGINE_MAX,
    #endif
	//! For ladder climbing entities.
    "EV_FOOTSTEP_LADDER",

    /**
	*   Water (Footstep-)Splash Events:
    **/
	    //! For when the feet touch water.
    "EV_WATER_ENTER_FEET", // Also plays a possible footstep sound.
    //! For when deep diving in water.
    "EV_WATER_ENTER_WAIST",
    //! For when the head goes underwater.
    "EV_WATER_ENTER_HEAD",
    //! For when the feet leave water.
    "EV_WATER_LEAVE_FEET",
    //! For when waist leaves water.
    "EV_WATER_LEAVE_WAIST",
    //! For when head leaves water.
    "EV_WATER_LEAVE_HEAD",

    /**
    *   Player/(Humanoid-)Monster Events:
    **/
    //! For when jumping up.
    "EV_JUMP_UP",
    //! Or Fall, FallShort, FallFar??
    "EV_JUMP_LAND",

    //! "Short" Fell to the ground.
    "EV_FALL_SHORT",
    //! "Short" fall to the ground.
    "EV_FALL_MEDIUM",
    // "Hard Far" fall to the ground.
    "EV_FALL_FAR",


    //
    // External Events:
    //
    //! Reload Weapon.
    "EV_WEAPON_RELOAD",
    //! Weapon Primary Fire.
    "EV_WEAPON_PRIMARY_FIRE",
    //! Weapon Secondary Fire.
    "EV_WEAPON_SECONDARY_FIRE",

    //! Weapon Holster/Draw:
    "EV_WEAPON_HOLSTER_AND_DRAW",

    // TODO: We really do wanna split weapon draw and holstering, but, alas, lack animation skills.
    //! Draw Weapon.
    "EV_WEAPON_DRAW",
    //! Holster Weapon.
    "EV_WEAPON_HOLSTER",

    //!
    //! The maximum predictable player events.
    //! 
    "PS_EV_MAX",

    /**
    *   Teleport Effects:
    **/
    //! Player Login.
    "EV_PLAYER_LOGIN",
    //! Player Logout.
    "EV_PLAYER_LOGOUT",
    //! Player teleporting.
    "EV_PLAYER_TELEPORT", // <Q2RTXP>: TODO: PS_EV_MAX? Or... just use these for PS_EV_...

    //! Other entity teleporting.
    "EV_OTHER_TELEPORT",

    /**
    *   Item Events:
    **/
    "EV_ITEM_RESPAWN",


    /******************************************
    *   Temporary (EXTERNAL)-Entity Events:
    *
    *   (Can be spawned too instead of being latched on to an entity, as they will be created as an entity of their own.)
    *******************************************/
    /**
    * Sound Events:
    **/
    //! General sound event for entities, play a sound on the (client's-) entity playing on the specified channel.
    "EV_GENERAL_SOUND",
    //! Same as EV_GENERAL_SOUND but with attenuation parameter.
    "EV_GENERAL_SOUND_EX",
    //! Positioned sound event for entities, play a sound at the designated position, or
    //! at the entity's origin if used with an entity. The sound will remain persistent at
    //! said position.
    "EV_POSITIONED_SOUND",
    //! Global sound event, will play at a client's head so the sound is not attenuated. (No diminishing.)
    "EV_GLOBAL_SOUND",

	/**
	*	"Blood/Damage/Hurt/Pain" Particle Events:
	**/
	//! A "proper" blood impact
	"EV_FX_BLOOD",
	//! A "proper" MORE blood impact.
	"EV_FX_MORE_BLOOD",

	/**
	*   "Impact" Particle Events:
	**/
	//! Gunshot impact effects/sparks.
	"EV_FX_IMPACT_GUNSHOT",
	"EV_FX_IMPACT_SPARKS",
	"EV_FX_IMPACT_BULLET_SPARKS",

	/**
	*   "Splash" Particle Events:
	**/
    //! Unknown splash type effect.
	"EV_FX_SPLASH_UNKNOWN",
    //! Spark splash effect.
	"EV_FX_SPLASH_SPARKS",
    //! A blue water splash effect.
	"EV_FX_SPLASH_WATER_BLUE",
    //! A blue water splash effect.
	"EV_FX_SPLASH_WATER_BROWN",
    //! A green slime splash effect.
	"EV_FX_SPLASH_SLIME",
    //! A red lava splash effect.
	"EV_FX_SPLASH_LAVA",
    //! A red blood splash effect.
	"EV_FX_SPLASH_BLOOD",

	/**
	*	"Trail" Particle Events:
	**/
	//! Bubble trail effect 01.
	"EV_FX_TRAIL_BUBBLES01",
	//! Bubble trail effect 02. (Also plays a sound.)
	"EV_FX_TRAIL_BUBBLES02",
	//! For debugging purposes.
	"EV_FX_TRAIL_DEBUG_LINE",

    /**
    *   Maximum number of entity events, must be last.
    **/
    //"EV_GAME_MAX"
};

/**
*   @brief   Compile-time assert to ensure that the sg_event_string_names array size matches EV_GAME_MAX.
**/
static_assert( std::size( sg_event_string_names ) == EV_GAME_MAX, "sg_event_string_names array size does not match expected size(EV_GAME_MAX), make sure all string identifiers for each event exist and are validly indexed." );