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
*   @brief  edict->flags
**/
enum entity_flags_t {
    FL_NONE = 0,
    FL_FLY = BIT( 1 ),
    FL_SWIM = BIT( 2 ), //! Implied immunity to drowining.
    FL_IMMUNE_LASER = BIT( 3 ),
    FL_INWATER = BIT( 4 ),
    FL_GODMODE = BIT( 5 ),
    FL_NOTARGET = BIT( 6 ),
    FL_DODGE = BIT( 7 ), //! Monster should try to dodge this.
    FL_IMMUNE_SLIME = BIT( 8 ),
    FL_IMMUNE_LAVA = BIT( 9 ),
    FL_PARTIALGROUND = BIT( 10 ),//! Not all corners are valid.
    FL_WATERJUMP = BIT( 11 ),//! Player jumping out of water.
    FL_TEAMSLAVE = BIT( 12 ),//! Not the first on the team.
    FL_NO_KNOCKBACK = BIT( 13 ),
    FL_RESPAWN = BIT( 14 ) //! Used for item respawning.
    //FL_POWER_ARMOR          = BIT( 15 ),//! Power armor (if any) is active
};
// Enumerator Type Bit Flags Support:
QENUM_BIT_FLAGS( entity_flags_t );


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
* 
* 
*   Server Game Entity Structure:
*
*
**/
/**
*   @brief  The server game entity structure. Still is a POD type.
*           Given template arguments are for the pointers that reside
*           within the 'shared' memory block.
* 
*           One could call this a different means of 'composition' here.
**/
struct svg_edict_t : public sv_shared_edict_t<svg_edict_t, svg_client_t> {
    /**
	*	Operator overloading so we can allocate the object using the Zone(Tag) memory.
	**/
    #if 1
    #ifndef _SHOBJECT_DEBUG_ZONE_TAG_MEMORY
	    //! New Operator Overload.
	    void *operator new( size_t size ) {
            // Prevent possible malloc from succeeding if size is 0.
            if ( size == 0 ) {
                size = 1;
            }
            //size += 1;
            // Allocate.
            if ( void *ptr = gi.TagMalloc( size, TAG_SVGAME_EDICTS ) ) {//TagAllocator::Malloc( size, TagAllocator::_zoneTag );
                // Debug about the allocation.
                gi.dprintf( "%s: Allocating %d bytes.\n", __func__, size );
                // Return the pointer.
                return ptr;
            }
            // Debug about the failure to allocate
            gi.dprintf( "%s: Failed allocationg %d bytes\n", __func__, size );
			// Throw an exception.
			//throw std::bad_alloc( "Failed to allocate memory" );
			return nullptr;
        }
	    //! Delete Operator Overload.
	    void operator delete( void *ptr ) {
            if ( ptr != nullptr ) {//TagAllocator::Free( ptr );
                // Debug about deallocation.
                gi.dprintf( "%s: Freeing %p in\n", __func__, ptr );
                // Deallocate.
                gi.TagFree( ptr );
                return;
            }
            // Debug about the failure to allocate
            gi.dprintf( "%s: (nullptr) %p\n", __func__, ptr );
	    }
	    //! New Operator Overload.
	    void *operator new[]( size_t size ) {
            // Prevent possible malloc from succeeding if size is 0.
            if ( size == 0 ) {
                size = 1;
            }
            //size += 1;
            // Allocate.
            if ( void *ptr = gi.TagMalloc( size, TAG_SVGAME_EDICTS ) ) {//TagAllocator::Malloc( size, TagAllocator::_zoneTag );
                // Debug about the allocation.
                gi.dprintf( "%s: Allocating %d bytes.\n", __func__, size );
                // Return the pointer.
                return ptr;
            }
            // Debug about the failure to allocate
            gi.dprintf( "%s: Failed allocating %d bytes\n", __func__, size );
            // Throw an exception.
            //throw std::bad_alloc( "Failed to allocate memory" );
            return nullptr;
	    }
	    //! Delete Operator Overload.
	    void operator delete[]( void *ptr ) {
            if ( ptr != nullptr ) {
                // Debug about deallocation.
                gi.dprintf( "%s: Freeing %p\n", __func__, ptr );
                // Deallocate.
                gi.TagFree( ptr ); // TagAllocator::Z_TagFree( ptr );
                return;
            }
            // Debug about the failure to allocate
            gi.dprintf( "%s: (nullptr) %p\n", __func__, ptr );
	    }
    #else
		//! New Operator Overload.
		void *operator new( size_t size, const char *file, int line ) {
            // Prevent possible malloc from succeeding if size is 0.
            if ( size == 0 ) {
                size = 1;
            }
            // Allocate.
            if ( void *ptr = gi.TagMalloc( size, TAG_SVGAME_EDICTS ) ) {//TagAllocator::Malloc( size, TagAllocator::_zoneTag );
                // Debug about the allocation.
                gi.dprintf( "%s: Allocating %d bytes in %s:%d\n", __func__, size, file, line );
                // Return the pointer.
                return ptr;
            }
            // Debug about the failure to allocate
            gi.dprintf( "%s: Failed allocationg %d bytes in %s:%d\n", __func__, size, file, line );
            // Throw an exception.
            //throw std::bad_alloc::bad_alloc( std::wstring( L"Failed to allocate memory" ) );
            return nullptr;		
        }
		//! Delete Operator Overload.
		void operator delete( void *ptr, const char *file, int line ) {
			// Deallocate.
            if ( ptr != nullptr ) {//TagAllocator::Free( ptr );
                gi.TagFree( ptr );
                // Debug about the deallocation.
                gi.dprintf( "%s: Freeing %p in %s:%d\n", __func__, ptr, file, line );
            }
		}
		//! New Operator Overload.
		void *operator new[]( size_t size, const char *file, int line ) {
            // Prevent possible malloc from succeeding if size is 0.
            if ( size == 0 ) {
                size = 1;
            }
            // Allocate.
            if ( void *ptr = gi.TagMalloc( size, TAG_SVGAME_EDICTS ) ) {//TagAllocator::Malloc( size, TagAllocator::_zoneTag );
                // Debug about the allocation.
                gi.dprintf( "%s: Allocating %d bytes in %s:%d\n", __func__, size, file, line );
                // Return the pointer.
                return ptr;
            }
            // Debug about the failure to allocate
            gi.dprintf( "%s: Failed allocationg %d bytes in %s:%d\n", __func__, size, file, line );
            return nullptr;
        }
		//! Delete Operator Overload.
		void operator delete[]( void *ptr, const char *file, int line ) {
			// Deallocate.
            if ( ptr != nullptr ) {
                gi.TagFree( ptr ); // TagAllocator::Z_TagFree( ptr );
                // Debug about the deallocation.
                gi.dprintf( "%s: Freeing %p in %s:%d\n", __func__, ptr, file, line );
            }
		}
	#endif
    #endif

    /**
    *
    *
    * The following fields are only used locally in game, not by server.
    *
    *
    **/
    //! Used for projectile skip checks and in general for checking if the entity has happened to been respawned.
    int32_t spawn_count = 0;
    //! sv.time when the object was freed
    QMTime  freetime;
    //! Used for deferring client info so that disconnected, etc works
    QMTime  timestamp;

    //! [SpawnKey]: Entity classname key/value.
    svg_level_qstring_t classname;
    //! [SpawnKey]: Path to model.
    const char *model = nullptr;
    //! [SpawnKey]: Key Spawn Angle.
    float       angle = 0.f;          // set in qe3, -1 = up, -2 = down

    //! [SpawnKey]: Entity spawnflags key/value.
    spawnflag_t spawnflags = 0;
    //! Generic Entity flags.
    entity_flags_t flags = entity_flags_t::FL_NONE;


    /**
    *   Health/Body Status Conditions:
    **/
    //! [SpawnKey]: Current Health.
    int32_t     health = 0;
    //! [SpawnKey]: Maximum Health. (Usually used to reset health with in respawn scenarios.)
    int32_t     max_health = 0;
    int32_t     gib_health = 0;
    //! Officially dead, or still respawnable etc.
    entity_lifestatus_t     lifeStatus = entity_lifestatus_t::LIFESTATUS_ALIVE;
    //! To take damage or not.
    entity_takedamage_t     takedamage = entity_takedamage_t::DAMAGE_NO;

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
        //QMTime timeChanged;

        //! Hint state information.
        //struct {
        //    //! Index, 0 = none, > 0 = in data array, < 0 is passed by hint command.
        //    int32_t index;
        //    //! Display flags for this entity's hint info.
        //    int32_t flags;
        //} hint;
        const sg_usetarget_hint_s *hintInfo;
    } useTarget = {};

    /**
    *   Target Name Fields:
    **/
    //! [SpawnKey]: Targetname of this entity.
    svg_level_qstring_t targetname;
    struct {
        //! [SpawnKey]: Name of the entity with a matching 'targetname' to (trigger-)use target.
        svg_level_qstring_t target;
        //! [SpawnKey]: Name of entity to kill if triggered.
        svg_level_qstring_t kill;
        //! [SpawnKey]: Name of the team this entity is on. (For movers.)
        svg_level_qstring_t team;
        //! [SpawnKey]: The path to traverse (For certain movers.)
        svg_level_qstring_t path;
        //! [SpawnKey]: The targetted entity to trigger when this entity dies.
        svg_level_qstring_t death;
        //! [SpawnKey]: Name of the entity with a matching 'targetname' to move along with. (PushMovers).
        svg_level_qstring_t movewith;
    } targetNames;

    /**
    *   Target Entities:
    **/
    struct {
        //! The active 'target' entity.
        svg_edict_t *target;
        //! The parent entity to move along with.
        svg_edict_t *movewith;
        //! Next child in the list of this 'movewith group' chain.
        //svg_edict_t *movewith_next;
    } targetEntities = {};


    /**
    *   Lua Properties:
    **/
    struct {
        //! [SpawnKey]: The name which its script methods are prepended by.
        svg_level_qstring_t luaName;
    } luaProperties;

    /**
    *   "Delay" entities, these are created when (UseTargets/SignalOut) with a
    *   special "delay" worldspawn key/value set.
    **/
    struct {
        //! For the SVG_UseTargets and its Lua companion.
        struct {
            //! The source entity that when UseTarget, created the DelayedUse entity.
            svg_edict_t *creatorEntity;
            //! The useType for delayed UseTarget.
            entity_usetarget_type_t useType;
            //! The useValue for delayed UseTarget.
            int32_t useValue;
        } useTarget;

        struct {
            //! The source entity that when SignalOut, created the DelayedSignalOut entity.
            svg_edict_t *creatorEntity;
            //! A copy of the actual signal name.
            char name[ 256 ];
            //! For delayed signaling.
            std::vector<svg_signal_argument_t> arguments;
        } signalOut;
    } delayed = {};

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
        svg_edict_t *parentMoveEntity;

        //! A pointer to the first 'moveWith child' entity.
        //! The child entity will be pointing to the next in line, and so on.
        svg_edict_t *moveNextEntity;
    } moveWith = {};

    //! Specified physics movetype.
    int32_t     movetype = 0;
    //! Move velocity.
    Vector3     velocity = QM_Vector3Zero();
    //! Angular(Move) Velocity.
    Vector3     avelocity = QM_Vector3Zero();
    //! The entity's height above its 'origin', used to state where eyesight is determined.
    int32_t     viewheight = 0;
    //! Categorized liquid entity state.
    mm_liquid_info_t liquidInfo = {};
    //! Categorized ground information.
    mm_ground_info_t groundInfo = {};
    //! [SpawnKey]: Weight(mass) of entity.
    int32_t     mass = 0;
    //! [SpawnKey]: Per entity gravity multiplier (1.0 is normal) use for lowgrav artifact, flares.
    float       gravity = 0.f;


    /**
    *   Pushers(MOVETYPE_PUSH/MOVETYPE_STOP) Physics:
    **/
    svg_pushmove_info_t pushMoveInfo = {};
    //! [SpawnKey]: Moving speed.
    float   speed = 0.f;
    //! [SpawnKey]: Acceleration speed.
    float   accel = 0.f;
    //! [SpawnKey]: Deceleration speed.
    float   decel = 0.f;
    //! [SpawnKey]: Move axis orientation, defaults to Z axis.
    Vector3 movedir = QM_Vector3Zero();
    //! Mover Default Start Position.
    Vector3 pos1 = QM_Vector3Zero();
    //! Mover Default Start Angles.
    Vector3 angles1 = QM_Vector3Zero();
    //! Mover Default End Position.
    Vector3 pos2 = QM_Vector3Zero();
    //! Mover Default End Angles.
    Vector3 angles2 = QM_Vector3Zero();
    //! Mover last origin.
    Vector3 lastOrigin = QM_Vector3Zero();
    //! Mover last angles.
    Vector3 lastAngles = QM_Vector3Zero();
    //! For func_train, next path to move to.
    svg_edict_t *movetarget = nullptr;


    /**
    *   NextThink AND Entity Callbacks:
    **/
    //! When to perform its next frame logic.
    QMTime   nextthink;

    //! Gives a chance to setup references to other entities etc.
    void        ( *postspawn )( svg_edict_t *ent ) = nullptr;

    //! Called before actually thinking.
    void        ( *prethink )( svg_edict_t *ent ) = nullptr;
    //! Called for thinking.
    void        ( *think )( svg_edict_t *self ) = nullptr;
    //! Called after thinking.
    void        ( *postthink )( svg_edict_t *ent ) = nullptr;

    //! Called when movement has been blocked.
    void        ( *blocked )( svg_edict_t *self, svg_edict_t *other ) = nullptr;         // move to moveinfo?
    //! Called when the entity touches another entity.
    void        ( *touch )( svg_edict_t *self, svg_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) = nullptr;

    //! Called to 'trigger' the entity.
    void        ( *use )( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) = nullptr;
    //! Called when the entity is being 'Signalled', happens when another entity emits an OutSignal to it.
    void        ( *onsignalin )( svg_edict_t *self, svg_edict_t *other, svg_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) = nullptr;

    //! Called when it gets damaged.
    void        ( *pain )( svg_edict_t *self, svg_edict_t *other, float kick, int damage ) = nullptr;
    //! Called to die.
    void        ( *die )( svg_edict_t *self, svg_edict_t *inflictor, svg_edict_t *attacker, int damage, vec3_t point ) = nullptr;


    /**
    *   Entity Pointers:
    **/
    //! Will be nullptr or world if not currently angry at anyone.
    svg_edict_t *enemy = nullptr;
    //! Previous Enemy.
    svg_edict_t *oldenemy = nullptr;
    //! The next path spot to walk toward.If.enemy, ignore.movetarget. When an enemy is killed, the monster will try to return to it's path.
    svg_edict_t *goalentity = nullptr;

    //! Chain Entity.
    svg_edict_t *chain = nullptr;
    //! Team Chain.
    svg_edict_t *teamchain = nullptr;
    //! Team master.
    svg_edict_t *teammaster = nullptr;

    //! Trigger Activator.
    svg_edict_t *activator = nullptr;
    //! The entity that called upon the SignalOut/UseTarget
    svg_edict_t *other = nullptr;


    /**
    *   Light Data:
    **/
    //! [SpawnKey]: (Light-)Style.
    int32_t     style = 0;          // also used as areaportal number
    //! [SpawnKey]: A custom lightstyle string to set at lightstyle index 'style'.
    const char *customLightStyle = nullptr;	// It is as it says.

    /**
    *   Item Data:
    **/
    //! If not nullptr, will point to one of the items in the itemlist.
    const gitem_t *item = nullptr;          // for bonus items

    /**
    *   Monster Data:
    **/
    //! How many degrees the yaw should rotate per frame in order to reach its 'ideal_yaw'.
    float   yaw_speed = 0.f;
    //! Ideal yaw to face to.
    float   ideal_yaw = 0.f;

    /**
    *   Player Noise/Trail:
    **/
    //! Pointer to noise entity.
    svg_edict_t *mynoise = nullptr;       // can go in client only
    svg_edict_t *mynoise2 = nullptr;
    //! Noise indexes.
    int32_t noise_index = 0;
    int32_t noise_index2 = 0;

    /**
    *   Sound Data:
    **/
    //! Volume, range 0.0 to 1.0
    float   volume = 0.f;
    //! Attenuation.
    float   attenuation = 0.f;
    //! Sound.
    QMTime  last_sound_time;

    /**
    *   Trigger(s) Data:
    **/
    //! [SpawnKey]: Message to display, usually center printed. (When triggers are triggered etc.)
    char *message = nullptr;
    //! [SpawnKey]: Wait time, usually for movers as a time before engaging movement back to their original state.
    float   wait = 0;
    //! [SpawnKey]: The delay(in seconds) to wait when triggered before triggering, or signalling the specified target.
    float   delay = 0.f;
    //!
    float   random = 0.f;


    /**
    *   Timers Data:
    **/
    QMTime   air_finished_time;
    QMTime   damage_debounce_time;
    QMTime   fly_sound_debounce_time;    // move to clientinfo
    QMTime   last_move_time;
    QMTime   touch_debounce_time;        // are all these legit?  do we need more/less of them?
    QMTime   pain_debounce_time;
    QMTime   show_hostile_time;
    //! Used for player trail.
    QMTime   trail_time;


    /**
    *   Various Data:
    **/
    //! Set when the entity gets hurt(SVG_TriggerDamage) and might be its cause of death.
    sg_means_of_death_t meansOfDeath = sg_means_of_death_t::MEANS_OF_DEATH_UNKNOWN;
    //! [SpawnKey]: Used for target_changelevel. Set as key/value.
    const char *map = nullptr;

    //! [SpawnKey]: Damage entity will do.
    int32_t     dmg = 0;
    //! Size of the radius where entities within will be damaged.
    int32_t     radius_dmg = 0;
    float       dmg_radius = 0.f;
    //! Light
    float		light = 0.f;
    //! [SpawnKey]: For certain pushers/movers to use sounds or not. -1 usually is not, sometimes a positive value indices the set.
    int32_t     sounds = 0;
    //! Count of ... whichever depends on entity.
    int32_t     count = 0;

    /**
    *   Only used for g_turret.cpp - WID: Remove?:
    **/
    Vector3 move_origin = QM_Vector3Zero();
    Vector3 move_angles = QM_Vector3Zero();
};

// This inherits from:
//
/**
*   @brief  The server game entity structure. Still is a POD type.
*           Given template arguments are for the pointers that reside
*           within the 'shared' memory block.
*
*           One could call this a different means of 'composition' here.
**/
//struct svg_edict_t : public sv_shared_edict_t<svg_edict_t, svg_client_t>
struct svg_player_edict_t : public svg_edict_t {
    int testVar = 100;
    Vector3 testVar2 = {};
};