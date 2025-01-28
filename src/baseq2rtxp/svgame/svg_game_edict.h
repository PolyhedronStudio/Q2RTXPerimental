/********************************************************************
*
*
*	ServerGame: Edicts Entity Data
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*   @brief  edict->spawnflags T
*           These are set with checkboxes on each entity in the map editor.
**/
//! Excludes the entity from DeathMatch game maps.
static constexpr spawnflag_t SPAWNFLAG_NOT_DEATHMATCH = BIT( 19 );
//! Excludes the entity from Coop game maps.
static constexpr spawnflag_t SPAWNFLAG_NOT_COOP = BIT( 20 );
//! Excludes the entity from 'easy' skill level maps.
static constexpr spawnflag_t SPAWNFLAG_NOT_EASY = BIT( 21 );
//! Excludes the entity from 'medium' skill level maps.
static constexpr spawnflag_t SPAWNFLAG_NOT_MEDIUM = BIT( 22 );
//! Excludes the entity from 'hard' skill level maps.
static constexpr spawnflag_t SPAWNFLAG_NOT_HARD = BIT( 23 );

//! (Temporarily-) Disable (+usetarget) key interacting for this this usetarget supporting entity.
static constexpr spawnflag_t SPAWNFLAG_USETARGET_DISABLED = BIT( 15 );
//! Indicates that the entity is a 'presser' when +usetargetted.
static constexpr spawnflag_t SPAWNFLAG_USETARGET_PRESSABLE = BIT( 16 );
//! Indicates that the entity is a 'presser' when +usetargetted.
static constexpr spawnflag_t SPAWNFLAG_USETARGET_TOGGLEABLE = BIT( 17 );
//! Will toggle on press, untoggle on release of press or focus, dispatch hold callback if toggled.
static constexpr spawnflag_t SPAWNFLAG_USETARGET_HOLDABLE = BIT( 18 );


/**
*   @brief  edict->movetype values
**/
typedef enum {
    MOVETYPE_NONE,          // never moves
    MOVETYPE_NOCLIP,        // origin and angles change with no interaction
    MOVETYPE_PUSH,          // no clip to world, push on box contact
    MOVETYPE_STOP,          // no clip to world, stops on box contact

    MOVETYPE_WALK,          // gravity
    MOVETYPE_STEP,          // gravity, special edge handling

    MOVETYPE_ROOTMOTION,    // gravity, animation root motion influences velocities.

    MOVETYPE_FLY,
    MOVETYPE_TOSS,          // gravity
    MOVETYPE_FLYMISSILE,    // extra size to monsters
    MOVETYPE_BOUNCE
} movetype_t;

/**
*   Server Game Entity Structure:
* 
*   ...
**/
struct edict_s {
    entity_state_t  s;
    struct gclient_s *client;   //! NULL if not a player the server expects the first part
    //! of gclient_s to be a player_state_t but the rest of it is opaque
    qboolean inuse;
    int32_t linkcount;

    // FIXME: move these fields to a server private sv_entity_t
    list_t area;    //! Linked to a division node or leaf

    int32_t num_clusters;       // If -1, use headnode instead.
    int32_t clusternums[ MAX_ENT_CLUSTERS ];
    int32_t headnode;           // Unused if num_clusters != -1

    int32_t areanum, areanum2;

    //================================

    int32_t     svflags;            // SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
    vec3_t      mins, maxs;
    vec3_t      absmin, absmax, size;
    solid_t     solid;
    contents_t  clipmask;
    contents_t  hullContents;
    edict_t     *owner;

    const cm_entity_t *entityDictionary;

    /**
    *   DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
    *   EXPECTS THE FIELDS IN THAT ORDER!
    **/
    /**
    *
    *
    * The following fields are only used locally in game, not by server.
    *
    *
    **/
    //! Used for projectile skip checks and in general for checking if the entity has happened to been respawned.
    int32_t     spawn_count;
    //! sv.time when the object was freed
    sg_time_t   freetime;
    //! Used for deferring client info so that disconnected, etc works
    sg_time_t	timestamp;

    //! [SpawnKey]: Entity classname key/value.
    svg_lstring_t classname;
    //! [SpawnKey]: Path to model.
    const char *model;
    //! [SpawnKey]: Key Spawn Angle.
    float       angle;          // set in qe3, -1 = up, -2 = down

    //! [SpawnKey]: Entity spawnflags key/value.
    spawnflag_t spawnflags;
    //! Generic Entity flags.
    entity_flags_t flags;


    /**
    *   Health/Body Status Conditions:
    **/
    //! [SpawnKey]: Current Health.
    int32_t     health;
    //! [SpawnKey]: Maximum Health. (Usually used to reset health with in respawn scenarios.)
    int32_t     max_health;
    int32_t     gib_health;
    //! Officially dead, or still respawnable etc.
    entity_lifestatus_t     lifeStatus;
    //! To take damage or not.
    entity_takedamage_t     takedamage;

    /**
    *   UseTarget Properties and State:
    **/
    struct {
        //! The entity's current useTarget value.
        //union {
        //    int32_t integral;
        //    float   fltpoint;
        //} value;
        //int32_t value;
        //! The use target features for this entity.
        entity_usetarget_flags_t flags;
        //! The use target state.
        entity_usetarget_state_t state;
        //! The time at which this entity was last (+usetarget) activated.
        //sg_time_t timeChanged;
    } useTarget;

    /**
    *   Target Name Fields:
    **/
    //! [SpawnKey]: Targetname of this entity.
    svg_lstring_t targetname;
    struct {
        //! [SpawnKey]: Name of the entity with a matching 'targetname' to (trigger-)use target.
        svg_lstring_t target;
        //! [SpawnKey]: Name of entity to kill if triggered.
        svg_lstring_t kill;
        //! [SpawnKey]: Name of the team this entity is on. (For movers.)
        svg_lstring_t team;
        //! [SpawnKey]: The path to traverse (For certain movers.)
        svg_lstring_t path;
        //! [SpawnKey]: The targetted entity to trigger when this entity dies.
        svg_lstring_t death;
        //! [SpawnKey]: Name of the entity with a matching 'targetname' to move along with. (PushMovers).
        svg_lstring_t movewith;
    } targetNames;

    /**
    *   Target Entities:
    **/
    struct {
        //! The active 'target' entity.
        edict_t *target;
        //! The parent entity to move along with.
        edict_t *movewith;
        //! Next child in the list of this 'movewith group' chain.
        //edict_t *movewith_next;
    } targetEntities;


    /**
    *   Lua Properties:
    **/
    struct {
        //! [SpawnKey]: The name which its script methods are prepended by.
        const char *luaName;
    } luaProperties;

    /**
    *   "Delay" entities, these are created when (UseTargets/SignalOut) with a
    *   special "delay" worldspawn key/value set.
    **/
    struct {
        //! For the SVG_UseTargets and its Lua companion.
        struct {
            //! The source entity that when UseTarget, created the DelayedUse entity.
            edict_t *creatorEntity;
            //! The useType for delayed UseTarget.
            entity_usetarget_type_t useType;
            //! The useValue for delayed UseTarget.
            int32_t useValue;
        } useTarget;

        struct {
            //! The source entity that when SignalOut, created the DelayedSignalOut entity.
            edict_t *creatorEntity;
            //! A copy of the actual signal name.
            char name[ 256 ];
            //! For delayed signaling.
            std::vector<svg_signal_argument_t> arguments;
        } signalOut;
    } delayed;

    /**
    *   Physics Related:
    **/
    //! For move with parent entities.
    struct {
        //! Initial absolute origin of child.
        Vector3 absoluteOrigin;
        //! (Brush-) origin offset.
        Vector3 originOffset;
        //! Relative delta offset to the parent entity.
        Vector3 relativeDeltaOffset;

        //! Initial delta angles between parent and child.
        Vector3 spawnDeltaAngles;
        //! Initial angles set during spawn time.
        Vector3 spawnParentAttachAngles;

        //! The totalled velocity of parent and child.
        Vector3 totalVelocity;

        //! POinter to the parent we're moving with.
        edict_t *parentMoveEntity;

        //! A pointer to the first 'moveWith child' entity.
        //! The child entity will be pointing to the next in line, and so on.
        edict_t *moveNextEntity;
    } moveWith;
    //! Specified physics movetype.
    int32_t     movetype;
    //! Move velocity.
    Vector3     velocity;
    //! Angular(Move) Velocity.
    Vector3     avelocity;
    //! The entity's height above its 'origin', used to state where eyesight is determined.
    int32_t     viewheight;
    //! Categorized liquid entity state.
    mm_liquid_info_t liquidInfo;
    //! Categorized ground information.
    mm_ground_info_t groundInfo;
    //! [SpawnKey]: Weight(mass) of entity.
    int32_t     mass;
    //! [SpawnKey]: Per entity gravity multiplier (1.0 is normal) use for lowgrav artifact, flares.
    float       gravity;


    /**
    *   Pushers(MOVETYPE_PUSH/MOVETYPE_STOP) Physics:
    **/
    svg_pushmove_info_t pushMoveInfo;
    //! [SpawnKey]: Moving speed.
    float   speed;
    //! [SpawnKey]: Acceleration speed.
    float   accel;
    //! [SpawnKey]: Deceleration speed.
    float   decel;
    //! [SpawnKey]: Move axis orientation, defaults to Z axis.
    Vector3 movedir;
    //! Mover Default Start Position.
    Vector3 pos1;
    //! Mover Default Start Angles.
    Vector3 angles1;
    //! Mover Default End Position.
    Vector3 pos2;
    //! Mover Default End Angles.
    Vector3 angles2;
    //! Mover last origin.
    Vector3 lastOrigin;
    //! Mover last angles.
    Vector3 lastAngles;
    //! For func_train, next path to move to.
    edict_t *movetarget;


    /**
    *   NextThink AND Entity Callbacks:
    **/
    //! When to perform its next frame logic.
    sg_time_t   nextthink;

    //! Gives a chance to setup references to other entities etc.
    void        ( *postspawn )( edict_t *ent );

    //! Called before actually thinking.
    void        ( *prethink )( edict_t *ent );
    //! Called for thinking.
    void        ( *think )( edict_t *self );
    //! Called after thinking.
    void        ( *postthink )( edict_t *ent );

    //! Called when movement has been blocked.
    void        ( *blocked )( edict_t *self, edict_t *other );         // move to moveinfo?
    //! Called when the entity touches another entity.
    void        ( *touch )( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );

    //! Called to 'trigger' the entity.
    void        ( *use )( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
    //! Called when the entity is being 'Signalled', happens when another entity emits an OutSignal to it.
    void        ( *onsignalin )( edict_t *self, edict_t *other, edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );

    //! Called when it gets damaged.
    void        ( *pain )( edict_t *self, edict_t *other, float kick, int damage );
    //! Called to die.
    void        ( *die )( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );


    /**
    *   Entity Pointers:
    **/
    //! Will be nullptr or world if not currently angry at anyone.
    edict_t *enemy;
    //! Previous Enemy.
    edict_t *oldenemy;
    //! The next path spot to walk toward.If.enemy, ignore.movetarget. When an enemy is killed, the monster will try to return to it's path.
    edict_t *goalentity;

    //! Chain Entity.
    edict_t *chain;
    //! Team Chain.
    edict_t *teamchain;
    //! Team master.
    edict_t *teammaster;

    //! Trigger Activator.
    edict_t *activator;
    //! The entity that called upon the SignalOut/UseTarget
    edict_t *other;


    /**
    *   Light Data:
    **/
    //! [SpawnKey]: (Light-)Style.
    int32_t     style;          // also used as areaportal number
    //! [SpawnKey]: A custom lightstyle string to set at lightstyle index 'style'.
    const char *customLightStyle;	// It is as it says.

    /**
    *   Item Data:
    **/
    //! If not nullptr, will point to one of the items in the itemlist.
    const gitem_t *item;          // for bonus items

    /**
    *   Monster Data:
    **/
    //! How many degrees the yaw should rotate per frame in order to reach its 'ideal_yaw'.
    float   yaw_speed;
    //! Ideal yaw to face to.
    float   ideal_yaw;

    /**
    *   Player Noise/Trail:
    **/
    //! Pointer to noise entity.
    edict_t *mynoise;       // can go in client only
    edict_t *mynoise2;
    //! Noise indexes.
    int32_t noise_index;
    int32_t noise_index2;

    /**
    *   Sound Data:
    **/
    //! Volume, range 0.0 to 1.0
    float       volume;
    //! Attenuation.
    float       attenuation;
    sg_time_t   last_sound_time;

    /**
    *   Trigger(s) Data:
    **/
    //! [SpawnKey]: Message to display, usually center printed. (When triggers are triggered etc.)
    char *message;
    //! [SpawnKey]: Wait time, usually for movers as a time before engaging movement back to their original state.
    float   wait;
    //! [SpawnKey]: The delay(in seconds) to wait when triggered before triggering, or signalling the specified target.
    float   delay;
    //!
    float   random;


    /**
    *   Timers Data:
    **/
    sg_time_t   air_finished_time;
    sg_time_t   damage_debounce_time;
    sg_time_t   fly_sound_debounce_time;    // move to clientinfo
    sg_time_t   last_move_time;
    sg_time_t   touch_debounce_time;        // are all these legit?  do we need more/less of them?
    sg_time_t   pain_debounce_time;
    sg_time_t   show_hostile;
    //! Used for player trail.
    sg_time_t   trail_time;


    /**
    *   Various Data:
    **/
    //! Set when the entity gets hurt(SVG_TriggerDamage) and might be its cause of death.
    sg_means_of_death_t meansOfDeath;
    //! [SpawnKey]: Used for target_changelevel. Set as key/value.
    const char *map;

    //! [SpawnKey]: Damage entity will do.
    int32_t     dmg;
    //! Size of the radius where entities within will be damaged.
    int32_t     radius_dmg;
    float       dmg_radius;
    //! Light
    float		light;
    //! [SpawnKey]: For certain pushers/movers to use sounds or not. -1 usually is not, sometimes a positive value indices the set.
    int32_t     sounds;
    //! Count of ... whichever depends on entity.
    int32_t     count;

    /**
    *   Only used for g_turret.cpp - WID: Remove?:
    **/
    Vector3 move_origin;
    Vector3 move_angles;
};



/**
*
*
*
*   Entity Init/Alloc/Free:
*
*
*
**/
/**
*   @brief  (Re-)initialize an edict.
**/
void SVG_InitEdict( edict_t *e );
/**
*   @brief  Either finds a free edict, or allocates a new one.
*   @remark This function tries to avoid reusing an entity that was recently freed,
*           because it can cause the client to think the entity morphed into something
*           else instead of being removed and recreated, which can cause interpolated
*           angles and bad trails.
**/
edict_t *SVG_AllocateEdict( void );
/**
*   @brief  Marks the edict as free
**/
void SVG_FreeEdict( edict_t *e );



/**
*
*
*
*   Entity Field Finding:
*
*
*
**/
/**
*   @brief  Searches all active entities for the next one that holds
*           the matching string at fieldofs (use the FOFS_GENTITY() macro) in the structure.
*
*   @remark Searches beginning at the edict after from, or the beginning if NULL
*           NULL will be returned if the end of the list is reached.
**/
edict_t *SVG_Find( edict_t *from, const int32_t fieldofs, const char *match ); // WID: C++20: Added const.
/**
*   @brief  Similar to SVG_Find, but, returns entities that have origins within a spherical area.
**/
edict_t *SVG_FindWithinRadius( edict_t *from, const vec3_t org, const float rad );



/**
*
*
*
*   Entity (State-)Testing:
*
*
*
**/
/**
*   @brief  Returns true if, ent != nullptr, ent->inuse == true.
**/
static inline const bool SVG_IsActiveEntity( const edict_t *ent ) {
    // nullptr:
    if ( !ent ) {
        return false;
    }
    // Inactive:
    if ( !ent->inuse ) {
        return false;
    }
    // Active.
    return true;
}
/**
*   @brief  Returns true if the entity is active and has a client assigned to it.
**/
static inline const bool SVG_IsClientEntity( const edict_t *ent, const bool healthCheck = false ) {
    // Inactive Entity:
    if ( !SVG_IsActiveEntity( ent ) ) {
        return false;
    }
    // No Client:
    if ( !ent->client ) {
        return false;
    }

    // Health Check, require > 0
    if ( healthCheck && ent->health <= 0 ) {
        return false;
    }
    // Has Client.
    return true;
}
/**
*   @brief  Returns true if the entity is active and is an actual monster.
**/
static inline const bool SVG_IsMonsterEntity( const edict_t *ent/*, const bool healthCheck = false */ ) {
    // Inactive Entity:
    if ( !SVG_IsActiveEntity( ent ) ) {
        return false;
    }
    // Monster entity:
    if ( ( ent->svflags & SVF_MONSTER ) || ent->s.entityType == ET_MONSTER || ent->s.entityType == ET_MONSTER_CORPSE ) {
        return true;
    }
    // No monster entity.
    return false;
}
/**
*   @brief  Returns true if the entity is active(optional, true by default) and has a luaName set to it.
*   @param  mustBeActive    If true, it will also check for whether the entity is an active entity.
**/
static inline const bool SVG_IsValidLuaEntity( const edict_t *ent, const bool mustBeActive = true ) {
    // Inactive Entity:
    if ( mustBeActive && !SVG_IsActiveEntity( ent ) ) {
        return false;
    }
    // Has a luaName set:
    if ( ent && ent->luaProperties.luaName ) {
        return true;
    }
    // No Lua Entity.
    return false;
}



/**
*
*
*
*   Entity Body Queue:
*
*
*
**/
/**
*   @brief
**/
void SVG_InitBodyQue( void );
/**
*   @brief
**/
void SVG_CopyToBodyQue( edict_t *ent );


/**
*
*
*
*   Entity SpawnFlags:
*
*
*
**/
/**
*   @brief  Returns true if the entity has specified spawnFlags set.
**/
static inline const bool SVG_HasSpawnFlags( const edict_t *ent, const int32_t spawnFlags ) {
    return ( ent->spawnflags & spawnFlags ) != 0;
}



/**
*
*
*
*   Entity UseTargets:
* 
* 
* 
**/
/**
*   @brief  Returns true if the entity has the specified useTarget flags set.
**/
static inline const bool SVG_UseTarget_HasUseTargetFlags( const edict_t *ent, const entity_usetarget_flags_t useTargetFlags ) {
    return ( ( ent->useTarget.flags & useTargetFlags ) ) != 0 ? true : false;
}
/**
*   @brief  Returns true if the entity has the specified useTarget flags set.
**/
static inline const bool SVG_UseTarget_HasUseTargetState( const edict_t *ent, const entity_usetarget_state_t useTargetState ) {
    return ( ent->useTarget.state & useTargetState ) != 0 ? true : false;
}



/**
*
*
*
*   Visibility Testing:
*
*
*
**/
/**
*   @return True if the entity is visible to self, even if not infront ()
**/
const bool SVG_IsEntityVisible( edict_t *self, edict_t *other );
/**
*   @return True if the entity is in front (in sight) of self
**/
const bool SVG_IsEntityInFrontOf( edict_t *self, edict_t *other );
/**
*   @return True if the testOrigin point is in front of entity 'self'.
**/
const bool SVG_IsEntityInFrontOf( edict_t *self, const Vector3 &testOrigin );