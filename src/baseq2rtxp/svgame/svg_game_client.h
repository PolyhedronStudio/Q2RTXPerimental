/********************************************************************
*
*
*	ServerGame: Game Client Data Structures
*	NameSpace: "".
*
*
********************************************************************/
#pragma once


/**
*   @brief  Client data that persists to exist across multiple level loads.
**/
typedef struct {
    //! String buffer of the client's user info.
    char        userinfo[ MAX_INFO_STRING ];
    //! The 'nickname' this client has for display.
    char        netname[ 16 ];
    //! The 'hand' we're holding our weapon with.
    int32_t     hand;

    //! A loadgame will leave valid entities that just don't have a connection yet.
    bool        connected;
    //! Stores whether spawned or not. 
    bool        spawned;

    /**
    *   Values saved and restored from edicts when changing levels
    **/
    //! Current health.
    int32_t         health;
    //! Maximum allowed health.
    int32_t         max_health;
    //! Saved entity flags.
    entity_flags_t     savedFlags;

    //! The currently selected item.
    int32_t         selected_item;
    //! A simple integer count for inventory of all item IDs.
    int32_t         inventory[ MAX_ITEMS ];


    //! A pointer to the item matching the currently 'used' ACTIVE weapon.
    const gitem_t *weapon;
    //! A pointer to the last(previously used) matching weapon item.
    const gitem_t *lastweapon;
    //! Stores the current clip ammo for each weapon that uses it.
    int32_t weapon_clip_ammo[ MAX_ITEMS ];

    //! Maximum carry ammo capacities.
    struct {
        //! Pistol Bullets.
        int32_t pistol;
        //! Rifle Bullets.
        int32_t rifle;
        //! SMG Bullets.
        int32_t smg;
        //! Sniper Bullets.
        int32_t sniper;
        //! Shotgun Shells.
        int32_t shotgun;
    } ammoCapacities;

    //! For calculating total unit score in coop games.
    int32_t     score;

    //! If true, this client is engaged in 'Spectator' mode.
    bool        spectator;
} client_persistant_t;

/**
*   @brief  Client respawn data that stays across multiplayer mode respawns.
**/
typedef struct {
    client_persistant_t pers_respawn;	// what to set client->pers to on a respawn.

    int64_t enterframe;     // level.frameNumber the client entered the game
    sg_time_t entertime;    // the moment in time the client entered the game.

    int32_t score;      // Frags, etc
    vec3_t cmd_angles;  // Angles sent over in the last command.

    bool spectator;     // Client is a spectator
} client_respawn_t;

// this structure is cleared on each SVG_Player_PutInServer(),
// except for 'client->pers'
struct gclient_s {
    /**
    *	Known and Shared with the Server:
    /**/
    player_state_t  ps;             // communicated by server to clients
    player_state_t  ops;            // old player state from the previous frame.
    int64_t         ping;			// WID: 64-bit-frame

    /**
    *	Private to game:
    **/
    client_persistant_t pers;
    client_respawn_t    resp;
    pmove_state_t       old_pmove;  // for detecting out-of-pmove changes

    /**
    *	Layout(s):
    **/
    bool        showscores;         //! Set layout stat
    bool        showinventory;      //! Set layout stat
    bool        showhelp;
    bool        showhelpicon;

    /**
    *	User Input:
    **/
    int32_t         buttons;
    int32_t         oldbuttons;
    int32_t         latched_buttons;
    struct userinput_s {
        //! Current frame usercmd buttons.
        int32_t     buttons;
        //! Last frame buttons.
        int32_t     lastButtons;

        //! Buttons that have been pressed but are now being held down.
        int32_t     heldButtons;
        //! Buttons which have been pressed once.
        int32_t     pressedButtons;
        //! Buttons which have been released.
        int32_t     releasedButtons;
    } userInput;

    /**
    *	Weapon Related:
    **/
    // 'Inventory Index' for the actual CURRENT weapon's ammo type.
    int32_t	ammo_index;

    // Set when we want to switch weapons.
    const gitem_t *newweapon;

    //! If true, the weapon thinking process has been executed by a 
    //! usercmd_t in ClientThink. Otherwise it'll be dealt with by the
    //! SVG_Client_BeginServerFrame instead.
    bool weapon_thunk;

    sg_time_t	grenade_time;
    sg_time_t	grenade_finished_time;
    bool        grenade_blew_up;

    /**
    *	Damage Related:
    **/
    // sum up damage over an entire frame, so shotgun and/or other blasts 
    // give a single big kick.
    struct {
        int32_t     armor;       //! Damage absorbed by armor.
        int32_t     blood;       //! Damage taken out of health.
        int32_t     knockBack;   //! Damage taken out of health
        Vector3     from;        //! Origin for vector calculation.
        //! Damage indicators.
        damage_indicator_t		  damage_indicators[ MAX_DAMAGE_INDICATORS ];
        //! Number of current frame damage indicators.
        uint8_t                   num_damage_indicators;
    } frameDamage;

    //! Yaw direction towards the killer entity.
    float   killer_yaw;         //! When dead, look at killer.

    /**
    *   Weapon State:
    **/
    struct weapon_state_s {
        //! The 'mode' is what the weapon is actually doing during its current 'state'.
        weapon_mode_t mode;
        //! The 'old mode' is what the weapon was actually doing during its previous 'state'.
        weapon_mode_t oldMode;

        //! True if the mode has changed for the current caller of weaponthink(ClientThink/BeginServerFrame)
        //! that executed weapon logic. It sets the weapon animationID of the player state, with a toggle bit
        //! in case of it finding itself in repetitive mode changes.
        //!
        //! In other words, this allows the client to be made aware of when an animation changed OR restarted.
        qboolean updatePlayerStateAnimationID;

        //! Determines if the weapon can change 'mode'.
        qboolean canChangeMode;
        //! If set, will be applied to the client entity's state sound member.
        int32_t activeSound;

        //! State for 'Aiming' weapon modes.
        struct {
            //! If true, the weapon is using secondary fire to engage in 'aim' mode.
            qboolean isAiming;
        } aimState;

        //! Stores the 'Weapon Animation' data, which if still actively being processed
        //! prevents the weapon from changing 'mode'.
        struct {
            //! Model AnimationID.
            uint8_t modelAnimationID;

            //! The frame matching for the time processed so far with the current animation.
            int32_t currentFrame;

            //! The starting frame.
            int32_t startFrame;
            //! The frame we want to be at by the end of the animation.
            int32_t endFrame;

            //! Start time of animation.
            sg_time_t startTime;
            //! End time of animation.
            sg_time_t endTime;

            //! Current time of animation relative to startTime.
            sg_time_t currentTime;

            //! Optional callback function pointer.
            //void ( *finished_animating )( edict_t *ent );
        } animation;

        //! Recoil.
        struct {
            //! Amount
            float amount;
            //! Total accumulated 'ms' by rapidly firing.
            sg_time_t accumulatedTime;
        } recoil;

        //! Timers
        struct {
            //! Last time we played an 'empty weapon click' sound.
            sg_time_t lastEmptyWeaponClick;

            //! Used to prevent firing too rapidly
            sg_time_t lastPrimaryFire;
            //! Used to prevent firing too rapidly
            sg_time_t lastSecondaryFire;
            //! Used to prevent firing too rapidly.
            sg_time_t lastAimedFire;
            //! Time the weapon was drawn. (Used for sound).
            sg_time_t lastDrawn;
            //! Time the weapon was holstered. (Used for sound).
            sg_time_t lastHolster;
        } timers;
    } weaponState;

    /**
    *   View Angles/Movement/Offset:
    **/
    //! Summed Weapon Kicks, to be applied to client 'viewoffset' and 'kick_angles'.
    struct {
        //! Additional weapon 'Kick Effect' angles, auto decays. 
        //! These are to be added to playerState.kick_angles.
        Vector3 offsetAngles;
        //! Additional weapon 'Kick Effect' origin. These are to be added to viewOffset.
        Vector3 offsetOrigin;
    } weaponKicks;
    //! Stores view movement related information.
    struct {
        //! Aiming direction.
        Vector3 viewAngles, viewForward, viewRight, viewUp;

        // View Movement Timers:
        sg_time_t	damageTime;
        sg_time_t	fallTime;
        sg_time_t	quakeTime;
        //! View Damage Kicks.
        float       damageRoll, damagePitch;
        //! For view drop on fall.
        float		fallValue;
    } viewMove;

    /**
    *   View Blends:
    **/
    float       damage_alpha;
    float       bonus_alpha;
    vec3_t      damage_blend;
    int64_t     bobCycle;
    int64_t     oldBobCycle;
    double      bobFracSin;
    //float       bobtime;            // Store it, so we know where we're at (To Prevent off-ground from changing it).

    /**
    *   Old/Previous frames data:
    **/
    uint64_t    last_stair_step_frame;

    vec3_t      last_ladder_pos; // For ladder step sounds.
    sg_time_t   last_ladder_sound;

    vec3_t          oldviewangles;
    vec3_t          oldvelocity;
    edict_t *oldgroundentity; // [Paril-KEX]
    liquid_level_t	old_waterlevel;
    sg_time_t       flash_time; // [Paril-KEX] for high tickrate

    /**
    *   Misc:
    **/
    //! Time for another drown sound event.
    sg_time_t		next_drown_time;

    /**
    *   UseTarget:
    **/
    struct {
        //! The entity we are currently pointing at.
        edict_t *currentEntity;
        //! The previous frame entity which we were pointing at.
        edict_t *previousEntity;

        //! To ensure we check for processing useTargets only once.
        uint64_t tracedFrameNumber;
    } useTarget;

    /**
    *	Animation Related:
    **/
    //! Used for 'Humanoid Skeletal Model' entities. (Monsters, Players, ...)
    //! The body states are encoded into the 'frame' state value.
    //! Each body state is made out of 9 bits, where the 9th bit is the animation toggle flag.
    sg_skm_animation_mixer_t animationMixer;

    /**
    *	Item/Use Event Timers:
    **/
    sg_time_t	pickup_msg_time;
    sg_time_t	respawn_time;		// can respawn when time > this

    /**
    *	Chat Flood Related:
    **/
    sg_time_t	flood_locktill;     // locked from talking
    sg_time_t	flood_when[ 10 ];     // when messages were said
    int64_t		flood_whenhead;     // head pointer for when said

    /**
    *   Spectator Chasing:
    **/
    edict_t *chase_target;      // player we are chasing
    bool        update_chase;       // need to update chase info?
};