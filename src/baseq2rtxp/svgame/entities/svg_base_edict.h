/********************************************************************
*
*
*	ServerGame: Edicts Entity Data
*	NameSpace: "".
*
*
********************************************************************/
#pragma once

// Enable to debug print the new/delete callback operation states.
//#define DEBUG_EDICT_ALLOCATORS 1

// Enable to debug the entity SetCallback functions.
// It'll trigger with specific information about what is wrong with it.
#if USE_DEBUG
    #define DEBUG_CALLBACK_ASSIGNMENTS 1 // Uncomment to disable.
#else
	// Prevent this from being defined in non-debug builds.
    //#define DEBUG_CALLBACK_ASSIGNMENTS 0 // Uncomment to disable.
#endif

// Forward declare.
struct svg_base_edict_t;

//! Include the TypeInfo system for linking a classname onto a C++ class type derived svg_edict_base_t.
#include "svgame/entities/typeinfo/svg_edict_typeinfo.h"
//! Needed for pushmove info.
#include "svgame/svg_pushmove_info.h"
//! Needed for the entity dictionary.
#include "svgame/svg_signalio.h"
//! Required for save descriptors.
#include "svgame/svg_save.h"
//! UseTargets
#include "svgame/svg_usetargets.h"

//! Macro for validating a callback function pointer at time of assignment.
#if ( defined( DEBUG_CALLBACK_ASSIGNMENTS ) && ( DEBUG_CALLBACK_ASSIGNMENTS == 1 ) )
    // Declared here for debug reasoning.
    const svg_save_descriptor_funcptr_error_t SVG_Save_DebugValidateCallbackFuncPtr( svg_base_edict_t *edict, void *p, svg_save_funcptr_type_t type, const std::string &functionName );
    #define DebugValidateCallbackFuncPtr(edict, p, type, functionName) SVG_Save_DebugValidateCallbackFuncPtr(edict, reinterpret_cast<void*>( p ), type, functionName)
//! Stub implementation:
//! This will always return true, so we can use it in release builds.
//! It will not do any validation.
#else
    #define DebugValidateCallbackFuncPtr(edict, p, type, functionName) (true);
#endif


/**
*   Implements a Base Copy reset of the svg_base_edict_t object, optionally retaining 
*   the original load time pointer to the entityDictionary. This allows for literally
*   restoring the entity to a freshly spawned world state.
*
*   Useful for let's say, a map with multiple matches/rounds, where you want to
*   reset all entities to their original spawn state without having to reload
*   the entire map.
**/
#define IMPLEMENT_EDICT_RESET_BY_COPY_ASSIGNMENT( BaseType, SelfType, retainDictionary ) \
    do { \
        /* Keep a copy of the base-class state so we can restore it */ \
        BaseType baseCopy = *static_cast<BaseType *>( this ); \
        /* Create a value-initialized temporary and assign it to this object. */ \
        static SelfType temp{}; \
        /* Retain the dictionary if necessary */ \
        if ( retainDictionary ) { \
            temp.entityDictionary = this->entityDictionary; \
		} \
        /* Assign to this object. This resets all derived members using their normal copy-assignment semantics. */ \
        *this = temp; \
        /* Now, restore base-class state produced by Base::Reset(...) */ \
        *static_cast<BaseType *>( this ) = baseCopy; \
    } while ( 0 )



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
*   @brief  Entity specific bit flags.
**/
enum entity_flags_t {
    FL_NONE = 0,
    FL_FLY = BIT( 1 ),
    //! Implied immunity to drowning.
    FL_SWIM = BIT( 2 ),
    FL_IMMUNE_LASER = BIT( 3 ),
    FL_INWATER = BIT( 4 ),
    FL_GODMODE = BIT( 5 ),
    FL_NOTARGET = BIT( 6 ),
    //! Monster should try to dodge this.
    FL_DODGE = BIT( 7 ),
    FL_IMMUNE_SLIME = BIT( 8 ),
    FL_IMMUNE_LAVA = BIT( 9 ),
    //! Not all corners are valid.
    FL_PARTIALGROUND = BIT( 10 ),
    //! Player jumping out of water.
    FL_WATERJUMP = BIT( 11 ),
    //! Not the first on the team.
    FL_TEAMSLAVE = BIT( 12 ),
    FL_NO_KNOCKBACK = BIT( 13 ),
    //! Used for item respawning.
    FL_RESPAWN = BIT( 14 )
    //! Power armor (if any) is active.
    //FL_POWER_ARMOR          = BIT( 15 ),
};
// Enumerator Type Bit Flags Support:
QENUM_BIT_FLAGS( entity_flags_t );



/**
*   @brief  edict->movetype values
**/
enum svg_movetype_t {
    //! Never moves. (Ignores velocity etc.)
    MOVETYPE_NONE = 0,
    //! Origin and angles are updated, but there is no interaction to world and other entities
    //! because we are "not clipping".
    MOVETYPE_NOCLIP,

	//! No clip to world, push on box contact.
    MOVETYPE_PUSH,
	//! No clip to world, stops on box contact.
    MOVETYPE_STOP,

    //! For player(client) entities. Player movement.
    MOVETYPE_WALK,

    //! Gravity, special edge handling.
    MOVETYPE_STEP,
    //! Gravity, animation root motion influences velocities.
    MOVETYPE_ROOTMOTION,
	//! No gravity, but still applies velocity and acceleration.
    MOVETYPE_FLY,
	//! For thrown entities. Influenced by gravity and velocity.
    MOVETYPE_TOSS,
	//! For projectiles. Influenced by gravity and velocity.
    MOVETYPE_FLYMISSILE,
	//! For bouncing entities. Influenced by gravity and velocity.
    MOVETYPE_BOUNCE
};



/**
*
*
*   Callback Function Ptrs:
*
*
**/
struct svg_base_edict_t;

//! Gives a chance to setup references to other entities etc.
using svg_edict_callback_spawn_fptr = void ( * )( svg_base_edict_t *ent );
//! Gives a chance to setup references to other entities etc.
using svg_edict_callback_postspawn_fptr = void ( * )( svg_base_edict_t *ent );

//! Called before actually thinking.
using svg_edict_callback_prethink_fptr = void ( * )( svg_base_edict_t *ent );
//! Called for thinking.
using svg_edict_callback_think_fptr = void ( * )( svg_base_edict_t *self );
//! Called after thinking.
using svg_edict_callback_postthink_fptr = void ( * )( svg_base_edict_t *self );

//! Called when movement has been blocked.
using svg_edict_callback_blocked_fptr = void ( * )( svg_base_edict_t *self, svg_base_edict_t *other );         // move to moveinfo?
//! Called when the entity touches another entity.
using svg_edict_callback_touch_fptr = void ( * )( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );

//! Called to 'trigger' the entity.
using svg_edict_callback_use_fptr = void ( * )( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
//! Called when the entity is being 'Signalled', happens when another entity emits an OutSignal to it.
using svg_edict_callback_onsignalin_fptr = void ( * )( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );

//! Called when it gets damaged.
using svg_edict_callback_pain_fptr = void ( * )( svg_base_edict_t *self, svg_base_edict_t *other, const float kick, const int damage, const entity_damageflags_t damageFlags );
//! Called to die.
using svg_edict_callback_die_fptr = void ( * )( svg_base_edict_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point );



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
struct svg_base_edict_t : public sv_shared_edict_t<svg_base_edict_t, svg_client_t> {
    //! Constructor. 
    svg_base_edict_t() = default;
    //! Destructor.
    virtual ~svg_base_edict_t() = default;

    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_base_edict_t( const cm_entity_t *ed ) : sv_shared_edict_t< svg_base_edict_t, svg_client_t >( ed ) {};

    // The root class of inheritance based game entity typeinfo inheritance.
    DefineTopRootClass( 
        // classname:       classType:        superClassType:
        "svg_base_edict_t", svg_base_edict_t, sv_shared_edict_t, 
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_GameSpawn
    );


    /**
    * 
    * 
	*	Operator overloading so we can allocate the object using the Zone(Tag) memory.
    * 
    * 
	**/
    #if 0
    //! New Operator Overload.
    void *operator new( size_t size, void *ptr ) {
        // Prevent possible malloc from succeeding if size is 0.
        if ( size == 0 ) {
            size = 0;
        }
        // Allocate.
        if ( ptr = gi.TagMalloc( size, TAG_SVGAME_EDICTS ) ) {//TagAllocator::Malloc( size, TagAllocator::_zoneTag );
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
    //! New Operator Overload.
    void operator delete( void *ptr, size_t size ) {
        if ( ptr != nullptr ) {//TagAllocator::Free( ptr );
            // Debug about deallocation.
            gi.dprintf( "%s: Freeing %p\n", __func__, ptr );
            // Deallocate.
            gi.TagFree( ptr );
            ptr = nullptr;
            return;
        }
        // Debug about the failure to allocate
        gi.dprintf( "%s: (nullptr) %p\n", __func__, ptr );
        // Throw an exception.
        //throw std::bad_alloc( "Failed to allocate memory" );
        return;
    }
    #endif
    //! New Operator Overload.
	void *operator new( size_t size ) {
        // Prevent possible malloc from succeeding if size is 0.
        if ( size == 0 ) {
            size = 0;
        }
        // Allocate.
        if ( void *ptr = gi.TagMalloc( size, TAG_SVGAME_EDICTS ) ) {//TagAllocator::Malloc( size, TagAllocator::_zoneTag );
            // Debug about the allocation.
            #if DEBUG_EDICT_ALLOCATORS
            gi.dprintf( "%s: Allocating %d bytes.\n", __func__, size );
            #endif
            // Return the pointer.
            return ptr;
        }
        #if DEBUG_EDICT_ALLOCATORS
        // Debug about the failure to allocate
        gi.dprintf( "%s: Failed allocationg %d bytes\n", __func__, size );
        #endif
		// Throw an exception.
		//throw std::bad_alloc( "Failed to allocate memory" );
		return nullptr;
    }
	//! Delete Operator Overload.
	void operator delete( void *ptr ) {
        if ( ptr != nullptr ) {//TagAllocator::Free( ptr );
            #if DEBUG_EDICT_ALLOCATORS
            // Debug about deallocation.
            gi.dprintf( "%s: Freeing %p\n", __func__, ptr );
            #endif
            // Deallocate.
            gi.TagFree( ptr );
            return;
        }
        ptr = nullptr;
        #if DEBUG_EDICT_ALLOCATORS
        // Debug about the failure to allocate
        gi.dprintf( "%s: (nullptr) %p\n", __func__, ptr );
        #endif
	}



    /**
    *
    *
    *   Callback Dispatching:
    *
    *
    **/
    /**
    *   @brief  Calls the 'spawn' callback that is configured for this entity.
    **/
    virtual void DispatchSpawnCallback( );
    /**
    *   @brief  Calls the 'postspawn' callback that is configured for this entity.
    **/
    virtual void DispatchPostSpawnCallback( );
    /**
    *   @brief  Calls the 'prethink' callback that is configured for this entity.
    **/
    virtual void DispatchPreThinkCallback( );
    /**
    *   @brief  Calls the 'think' callback that is configured for this entity.
    **/
    virtual void DispatchThinkCallback( );
    /**
    *   @brief  Calls the 'postthink' callback that is configured for this entity.
    **/
    virtual void DispatchPostThinkCallback( );
    /**
    *   @brief  Calls the 'blocked' callback that is configured for this entity.
    **/
    virtual void DispatchBlockedCallback( svg_base_edict_t *other );
    /**
    *   @brief  Calls the 'touch' callback that is configured for this entity.
    **/
    virtual void DispatchTouchCallback( svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
    /**
    *   @brief  Calls the 'use' callback that is configured for this entity.
    **/
    virtual void DispatchUseCallback( svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue );
    /**
    *   @brief  Calls the 'onsignalin' callback that is configured for this entity.
    **/
    virtual void DispatchOnSignalInCallback( svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments );
    /**
    *   @brief  Calls the 'pain' callback that is configured for this entity.
    **/
    virtual void DispatchPainCallback( svg_base_edict_t *other, const float kick, const int32_t damage, const entity_damageflags_t damageFlags );
    /**
    *   @brief  Calls the 'die' callback that is configured for this entity.
    **/
    virtual void DispatchDieCallback( svg_base_edict_t *inflictor, svg_base_edict_t *attacker, const int32_t damage, Vector3 *point );



    /**
    *
    *
    *   Core:
    *
    *
    **/
    /**
	*   Reconstructs the object, zero-ing out its members, and optionally 
	*   retaining the entityDictionary. 
    * 
	*   Sometimes we want to keep the entityDictionary intact so we can 
    *   spawn from it. (This is used for saving/loading.)
    **/
    virtual void Reset( const bool retainDictionary = false ) override;

    /**
	*   @brief  Used for savegaming the entity. Each derived entity type
	*           that needs to be saved should implement this function.
    * 
    *   @note   Make sure to call the base parent class' Save() function.
    **/
	virtual void Save( struct game_write_context_t *ctx );
    /**
    *   @brief  Used for loadgaming the entity. Each derived entity type
    *           that needs to be loaded should implement this function.
    *
    *   @note   Make sure to call the base parent class' Restore() function.
    **/
    virtual void Restore( struct game_read_context_t *ctx );

    /**
    *   @brief  Called for each cm_entity_t key/value pair for this entity.
    *           If not handled, or unable to be handled by the derived entity type, it will return
    *           set errorStr and return false. True otherwise.
    **/
    virtual const bool KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr );



    /**
    *
    *
    *   @brief  Use these to simplify setting a callback function pointer to anything such as,
    *           static void some_entity_class::die_callback(svg_not_base_entity *self) {
    *           }
	*           The methods will perform a reinterpret cast to the correct type. And in debug
    *           mode check if the function pointer exists in the table of ptrs.
    *
    *
    **/
    /**
    *   @brief  For properly setting the 'spawn' callback method function pointer.
    **/
    template<typename FuncPtrType>
    inline FuncPtrType SetSpawnCallback( FuncPtrType funcPtr ) {
		// Debug validate the function pointer.
        DebugValidateCallbackFuncPtr( this, funcPtr, FPTR_SAVE_TYPE_SPAWN, __func__ );
		// Set and return the function pointer.
        spawnCallbackFuncPtr = reinterpret_cast<svg_edict_callback_spawn_fptr>( funcPtr );
        return funcPtr;
    }
	/**
	*   @return Returns true if the spawn callback function pointer is set.
	**/
	inline const bool HasSpawnCallback() const {
		return ( spawnCallbackFuncPtr != nullptr );
	}
    
    /**
    *   @brief  For properly setting the 'postspawn' callback method function pointer.
    **/
    template<typename FuncPtrType>
    inline FuncPtrType SetPostSpawnCallback( FuncPtrType funcPtr ) {
        // Debug validate the function pointer.
        DebugValidateCallbackFuncPtr( this, funcPtr, FPTR_SAVE_TYPE_POSTSPAWN, __func__ );
        // Set and return the function pointer.
        postSpawnCallbackFuncPtr = reinterpret_cast<svg_edict_callback_postspawn_fptr>( funcPtr );
        return funcPtr;
    }
    /**
    *   @return Returns true if the post spawn callback function pointer is set.
    **/
    inline const bool HasPostSpawnCallback() const {
        return ( postSpawnCallbackFuncPtr != nullptr );
    }
    
    /**
    *   @brief  For properly setting the 'prethink' callback method function pointer.
    **/
    template<typename FuncPtrType>
    inline FuncPtrType SetPreThinkCallback( FuncPtrType funcPtr ) {
        // Debug validate the function pointer.
        DebugValidateCallbackFuncPtr( this, funcPtr, FPTR_SAVE_TYPE_PRETHINK, __func__ );
        // Set and return the function pointer.
        preThinkCallbackFuncPtr = reinterpret_cast<svg_edict_callback_prethink_fptr>( funcPtr );
        return funcPtr;
    }
    /**
    *   @return Returns true if the prethink callback function pointer is set.
    **/
    inline const bool HasPreThinkCallback() const {
        return ( preThinkCallbackFuncPtr != nullptr );
    }
    
    /**
    *   @brief  For properly setting the 'think' callback method function pointer.
    **/
    template<typename FuncPtrType>
    inline FuncPtrType SetThinkCallback( FuncPtrType funcPtr ) {
        // Debug validate the function pointer.
        DebugValidateCallbackFuncPtr( this, funcPtr, FPTR_SAVE_TYPE_THINK, __func__ );
        // Set and return the function pointer.
        thinkCallbackFuncPtr = reinterpret_cast<svg_edict_callback_think_fptr>( funcPtr );
        return funcPtr;
    }
    /**
    *   @return Returns true if the think callback function pointer is set.
    **/
    inline const bool HasThinkCallback() const {
        return ( thinkCallbackFuncPtr != nullptr );
    }
    
    /**
    *   @brief  For properly setting the 'postthink' callback method function pointer.
    **/
    template<typename FuncPtrType>
    inline FuncPtrType SetPostThinkCallback( FuncPtrType funcPtr ) {
        // Debug validate the function pointer.
        DebugValidateCallbackFuncPtr( this, funcPtr, FPTR_SAVE_TYPE_POSTTHINK, __func__ );
        // Set and return the function pointer.
        postThinkCallbackFuncPtr = reinterpret_cast<svg_edict_callback_postthink_fptr>( funcPtr );
        return funcPtr;
    }
    /**
    *   @return Returns true if the post think callback function pointer is set.
    **/
    inline const bool HasPostThinkCallback() const {
        return ( postThinkCallbackFuncPtr != nullptr );
    }

    /**
    *   @brief  For properly setting the 'blocked' callback method function pointer.
    **/
    template<typename FuncPtrType>
    inline FuncPtrType SetBlockedCallback( FuncPtrType funcPtr ) {
        // Debug validate the function pointer.
        DebugValidateCallbackFuncPtr( this, funcPtr, FPTR_SAVE_TYPE_BLOCKED, __func__ );
        // Set and return the function pointer.
        blockedCallbackFuncPtr = reinterpret_cast<svg_edict_callback_blocked_fptr>( funcPtr );
        return funcPtr;
    }
    /**
    *   @return Returns true if the blocked callback function pointer is set.
    **/
    inline const bool HasBlockedCallback() const {
        return ( blockedCallbackFuncPtr != nullptr );
    }

    /**
    *   @brief  For properly setting the 'touch' callback method function pointer.
    **/
    template<typename FuncPtrType>
    inline FuncPtrType SetTouchCallback( FuncPtrType funcPtr ) {
        // Debug validate the function pointer.
        DebugValidateCallbackFuncPtr( this, funcPtr, FPTR_SAVE_TYPE_TOUCH, __func__ );
        // Set and return the function pointer.
        touchCallbackFuncPtr = reinterpret_cast<svg_edict_callback_touch_fptr>( funcPtr );
        return funcPtr;
    }
    /**
    *   @return Returns true if the touch callback function pointer is set.
    **/
    inline const bool HasTouchCallback() const {
        return ( touchCallbackFuncPtr != nullptr );
    }

    /**
    *   @brief  For properly setting the 'use' callback method function pointer.
    **/
    template<typename FuncPtrType>
    inline FuncPtrType SetUseCallback( FuncPtrType funcPtr ) {
        // Debug validate the function pointer.
        DebugValidateCallbackFuncPtr( this, funcPtr, FPTR_SAVE_TYPE_USE, __func__ );
        // Set and return the function pointer.
        useCallbackFuncPtr = reinterpret_cast<svg_edict_callback_use_fptr>( funcPtr );
        return funcPtr;
    }
    /**
    *   @return Returns true if the use callback function pointer is set.
    **/
    inline const bool HasUseCallback() const {
        return ( useCallbackFuncPtr != nullptr );
    }

    /**
    *   @brief  For properly setting the 'onsignalin' callback method function pointer.
    **/
    template<typename FuncPtrType>
    inline FuncPtrType SetOnSignalInCallback( FuncPtrType funcPtr ) {
        // Debug validate the function pointer.
        DebugValidateCallbackFuncPtr( this, funcPtr, FPTR_SAVE_TYPE_ONSIGNALIN, __func__ );
        // Set and return the function pointer.
        onSignalInCallbackFuncPtr = reinterpret_cast<svg_edict_callback_onsignalin_fptr>( funcPtr );
        return funcPtr;
    }
    /**
    *   @return Returns true if the on-signal-in callback function pointer is set.
    **/
    inline const bool HasOnSignalInCallback() const {
        return ( onSignalInCallbackFuncPtr != nullptr );
    }

    /**
    *   @brief  For properly setting the 'pain' callback method function pointer.
    **/
    template<typename FuncPtrType>
    inline FuncPtrType SetPainCallback( FuncPtrType funcPtr ) {
        // Debug validate the function pointer.
        DebugValidateCallbackFuncPtr( this, funcPtr, FPTR_SAVE_TYPE_PAIN, __func__ );
        // Set and return the function pointer.
        painCallbackFuncPtr = reinterpret_cast<svg_edict_callback_pain_fptr>( funcPtr );
        return funcPtr;
    }
    /**
    *   @return Returns true if the pain callback function pointer is set.
    **/
    inline const bool HasPainCallback() const {
        return ( painCallbackFuncPtr != nullptr );
    }

    /**
    *   @brief  For properly setting the 'die' callback method function pointer.
    **/
    template<typename FuncPtrType>
    inline FuncPtrType SetDieCallback( FuncPtrType funcPtr ) {
        // Debug validate the function pointer.
        DebugValidateCallbackFuncPtr( this, funcPtr, FPTR_SAVE_TYPE_DIE, __func__ );
        // Set and return the function pointer.
        dieCallbackFuncPtr = reinterpret_cast<svg_edict_callback_die_fptr>( funcPtr );
        return funcPtr;
    }
    /**
    *   @return Returns true if the use callback function pointer is set.
    **/
    inline const bool HasDieCallback() const {
        return ( dieCallbackFuncPtr != nullptr );
    }



    /**
    *
    * 
    *   TypeInfo Related:
    *
    * 
    **/
    //! For each derived edict type, this field can be added to and
    //! instanced in the edict type's own .cpp file.
    //! 
    //! The game will take care of recursively writing the neccessary
    //! fields in order of the edict_t inheritance hierachy.
    static svg_save_descriptor_field_t saveDescriptorFields[];
    
    /**
	*   @brief Retrieves a pointer to the save descriptor fields.
	*   @return A pointer to a structure of type `svg_save_descriptor_field_t` representing the save descriptor fields.
	**/
    virtual svg_save_descriptor_field_t *GetSaveDescriptorFields();
    /**
	*   @return The number of save descriptor fields.
    **/
    virtual int32_t GetSaveDescriptorFieldsCount();
    /**
	*   @return A pointer to the save descriptor field with the given name.
    **/
    virtual svg_save_descriptor_field_t *GetSaveDescriptorField( const char *name );



    /**
    *
    *   Callbacks(defaults):
    *
    **/
    /**
	*   @brief  A default spawn implementation function.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_base_edict_t, onSpawn );
    /**
    *   @brief  PostSpawn Stub.
    **/
    DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_base_edict_t, onPostSpawn );
    /**
    *   @brief  Thinking Stubs.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_base_edict_t, onPreThink );
    DECLARE_MEMBER_CALLBACK_THINK( svg_base_edict_t, onThink );
    DECLARE_MEMBER_CALLBACK_THINK( svg_base_edict_t, onPostThink );
    /**
    *   @brief  Blocked Stub.
    **/
    DECLARE_MEMBER_CALLBACK_BLOCKED( svg_base_edict_t, onBlocked );
    /**
    *   @brief  Touched Stub.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_base_edict_t, onTouch );
    /**
    *   @brief  Use Stub.
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_base_edict_t, onUse );
    /**
    *   @brief  Pain Stub.
    **/
    DECLARE_MEMBER_CALLBACK_PAIN( svg_base_edict_t, onPain );
    /**
    *   @brief  Die Stub.
    **/
    DECLARE_MEMBER_CALLBACK_DIE( svg_base_edict_t, onDie );
    /**
    *   @brief  Signal Receiving Stub.
    **/
    DECLARE_MEMBER_CALLBACK_ON_SIGNALIN( svg_base_edict_t, onSignalIn );


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
    QMTime  freetime = 0_ms;
    //! Used for deferring client info so that disconnected, etc works
    QMTime  timestamp = 0_ms;
    //! If true, entity will never be freed, only unlinked(thus, removed as in, no collision, no transmission).
    bool    neverFreeOnlyUnlink = false;

    //! [SpawnKey]: Entity spawnflags key/value.
    spawnflag_t spawnflags = 0;
    //! Generic Entity flags.
    entity_flags_t flags = entity_flags_t::FL_NONE;

    //! [SpawnKey]: Entity classname key/value.
    svg_level_qstring_t classname = nullptr;
    //! [SpawnKey]: Path to model.
    svg_level_qstring_t model = nullptr;
    //! [SpawnKey]: Key Spawn Angle.
    float       angle = 0.f;          // set in qe3, -1 = up, -2 = down


    /**
    *   Entity Event Properties:
    **/
    //! Events will be cleared EVENT_VALID_MSEC after set.
    QMTime  eventTime = 0_ms;
	//! If true, entity will be freed once the event is done.
	bool	freeAfterEvent = false;
	//! If true, entity will be unlinked(thus, removed as in, no collision, no transmission) once the event is done.
    bool    unlinkAfterEvent = false;


    /**
    *   Health/Body Status Conditions:
    **/
    //! [SpawnKey]: Current Health.
    int32_t     health = 0;
	//! [SpawnKey]: Current Armor.
	int32_t     armor = 0;
    //! [SpawnKey]: Maximum Health. (Usually used to reset health with in respawn scenarios.)
    int32_t     max_health = 0;
	//! [SpawnKey]: Maximum Armor. (Usually used to reset armor with in respawn scenarios.)
	int32_t     max_armor = 0;
	//! Gib Health, used to determine if the entity is dead or not.
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
        entity_usetarget_flags_t flags = ENTITY_USETARGET_FLAG_NONE;
        //! The use target state.
        entity_usetarget_state_t state = ENTITY_USETARGET_STATE_DEFAULT;
        //! The time at which this entity was last (+usetarget) activated.
        //QMTime timeChanged;

        //! Hint state information.
        //struct {
        //    //! Index, 0 = none, > 0 = in data array, < 0 is passed by hint command.
        //    int32_t index;
        //    //! Display flags for this entity's hint info.
        //    int32_t flags;
        //} hint;
        const sg_usetarget_hint_t *hintInfo = nullptr;
    } useTarget = {};


    /**
    *   Target Name Fields:
    **/
    //! [SpawnKey]: Targetname of this entity.
    svg_level_qstring_t targetname = nullptr;
    struct {
        //! [SpawnKey]: Name of the entity with a matching 'targetname' to (trigger-)use target.
        svg_level_qstring_t target = nullptr;
        //! [SpawnKey]: Name of entity to kill if triggered.
        svg_level_qstring_t kill = nullptr;
        //! [SpawnKey]: Name of the team this entity is on. (For movers.)
        svg_level_qstring_t team = nullptr;
        //! [SpawnKey]: The path to traverse (For movers, laser targets.)
        svg_level_qstring_t path = nullptr;
        //! [SpawnKey]: The targetted entity to trigger when this entity dies.
        svg_level_qstring_t death = nullptr;
        //! [SpawnKey]: Name of the entity with a matching 'targetname' to move along with. (PushMovers).
        svg_level_qstring_t movewith = nullptr;
    } targetNames;


    /**
    *   Target Entities:
    **/
    struct {
        //! The active 'target' entity.
        svg_base_edict_t *target = nullptr;
        //! The parent entity to move along with.
        svg_base_edict_t *movewith = nullptr;
        //! Next child in the list of this 'movewith group' chain.
        //svg_base_edict_t *movewith_next;
    } targetEntities = {};


    /**
    *   Lua Properties:
    **/
    struct {
        //! [SpawnKey]: The name which its script methods are prepended by.
        svg_level_qstring_t luaName = nullptr;
    } luaProperties;


    /**
    *   "Delay" entities, these are created when (UseTargets/SignalOut) with a
    *   special "delay" worldspawn key/value set.
    **/
    struct {
        //! For the SVG_UseTargets and its Lua companion.
        struct {
            //! The source entity that when UseTarget, created the DelayedUse entity.
            svg_base_edict_t *creatorEntity = nullptr;
            //! The useType for delayed UseTarget.
            entity_usetarget_type_t useType;
            //! The useValue for delayed UseTarget.
            int32_t useValue;
        } useTarget;

        struct {
            //! The source entity that when SignalOut, created the DelayedSignalOut entity.
            svg_base_edict_t *creatorEntity = nullptr;
            //! A copy of the actual signal name.
            char name[ 256 ];
            //! For delayed signaling.
            std::vector<svg_signal_argument_t> arguments;
			// <Q2RTXP>: TODO: Implement saving and restoring of these arguments.
            //sg_qtag_memory_t<svg_signal_argument_t, TAG_SVGAME_LEVEL> arguments = sg_qtag_memory_t<svg_signal_argument_t, TAG_SVGAME_LEVEL>::sg_qtag_memory_t( nullptr, 0 );
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
        svg_base_edict_t *parentMoveEntity;

        //! A pointer to the first 'moveWith child' entity.
        //! The child entity will be pointing to the next in line, and so on.
        svg_base_edict_t *moveNextEntity;
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
    liquid_info_t liquidInfo = {};
    //! Categorized ground information.
    ground_info_t groundInfo = { .entityNumber = ENTITYNUM_NONE };
    //! The directional gravity vector.
	Vector3     gravityVector = QM_Vector3Gravity();
    //! [SpawnKey]: Weight(mass) of entity.
    int32_t     mass = 0;
    //! [SpawnKey]: Per entity gravity multiplier (1.0 is normal) use for lowgrav artifact, flares.
    float       gravity = 0.f;
    //! [SpawnKey]: For movers.
    //const char *noise;
    //! [SpawnKey]: For triggers.
    QMTime      pausetime = 0_ms;


    /**
    *   Pushers(MOVETYPE_PUSH/MOVETYPE_STOP) Physics:
    **/
    svg_pushmove_info_t pushMoveInfo = {};

    //! [SpawnKey]: PushMover Lip Distance.
    float   lip     = 0.f;
    //! [SpawnKey]: PushMover Lip Distance.
    float   distance    = 0.f;
    //! [SpawnKey]: PushMover Lip Distance.
    float   height  = 0.f;

    //! [SpawnKey]: Moving speed.
    float   speed   = 0.f;
    //! [SpawnKey]: Acceleration speed.
    float   accel   = 0.f;
    //! [SpawnKey]: Deceleration speed.
    float   decel   = 0.f;

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
    svg_base_edict_t *movetarget = nullptr;


    /**
    *   NextThink AND Entity Callbacks:
    **/
    //! When to perform its next frame logic.
    QMTime   nextthink = 0_ms;

    svg_edict_callback_spawn_fptr spawnCallbackFuncPtr = nullptr;
    //! Gives a chance to setup references to other entities etc.
	svg_edict_callback_postspawn_fptr postSpawnCallbackFuncPtr = nullptr;
    //! Called before actually thinking.
    svg_edict_callback_prethink_fptr preThinkCallbackFuncPtr = nullptr;
    //! Called for thinking.
    svg_edict_callback_think_fptr thinkCallbackFuncPtr = nullptr;
    //! Called after thinking.
    svg_edict_callback_postthink_fptr postThinkCallbackFuncPtr = nullptr;

    //! Called when movement has been blocked.
    svg_edict_callback_blocked_fptr blockedCallbackFuncPtr = nullptr;         // move to moveinfo?
    //! Called when the entity touches another entity.
    svg_edict_callback_touch_fptr touchCallbackFuncPtr = nullptr;

    //! Called to 'trigger' the entity.
    svg_edict_callback_use_fptr useCallbackFuncPtr = nullptr;
    //! Called when the entity is being 'Signalled', happens when another entity emits an OutSignal to it.
    svg_edict_callback_onsignalin_fptr onSignalInCallbackFuncPtr = nullptr;

    //! Called when it gets damaged.
    svg_edict_callback_pain_fptr painCallbackFuncPtr = nullptr;
    //! Called to die.
    svg_edict_callback_die_fptr dieCallbackFuncPtr = nullptr;


    /**
    *   Entity Pointers:
    **/
    //! Will be nullptr or world if not currently angry at anyone.
    svg_base_edict_t *enemy = nullptr;
    //! Previous Enemy.
    svg_base_edict_t *oldenemy = nullptr;
    //! The next path spot to walk toward.If.enemy, ignore.movetarget. When an enemy is killed, the monster will try to return to it's path.
    svg_base_edict_t *goalentity = nullptr;

    //! Chain Entity.
    svg_base_edict_t *chain = nullptr;
    //! Team Chain.
    svg_base_edict_t *teamchain = nullptr;
    //! Team master.
    svg_base_edict_t *teammaster = nullptr;

    //! Entity that activated the triggering.
    svg_base_edict_t *activator = nullptr;
    //! The entity that called upon the SignalOut/UseTarget
    svg_base_edict_t *other = nullptr;


    /**
    *   Light Data:
    **/
    //! [SpawnKey]: (Light-)Style.
    int32_t     style = 0;          // also used as areaportal number
    //! [SpawnKey]: A custom lightstyle string to set at lightstyle index 'style'.
    svg_level_qstring_t customLightStyle = nullptr;	// It is as it says.

    /**
    *   Item Data:
    **/
    //! If not nullptr, will point to one of the items in the itemlist.
    const gitem_t *item = nullptr;          // for bonus items
    //! [SpawnKey]: For items.
    svg_level_qstring_t itemName = nullptr;

    /**
    *   Monster Data:
    **/
    //! How many degrees the yaw should rotate per frame in order to reach its 'ideal_yaw'.
    float   yaw_speed = 0.f;
    //! Ideal yaw to face to.
    float   ideal_yaw = 0.f;

    /**
    *   (Player-)Noise/Trail:
    **/
	//! [SpawnKey]: The path to the noise audio to use.
    svg_level_qstring_t noisePath = nullptr;
    //! Pointer to noise entity.
    svg_base_edict_t *mynoise = nullptr;       // can go in client only
    svg_base_edict_t *mynoise2 = nullptr;
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
    QMTime  last_sound_time = 0_ms;

    /**
    *   Trigger(s) Data:
    **/
    //! [SpawnKey]: Message to display, usually center printed. (When triggers are triggered etc.)
    svg_level_qstring_t message = nullptr;
    //! [SpawnKey]: Wait time, usually for movers as a time before engaging movement back to their original state.
    float   wait = 0;
    //! [SpawnKey]: The delay(in seconds) to wait when triggered before triggering, or signalling the specified target.
    float   delay = 0.f;
    //!
    float   random = 0.f;


    /**
    *   Timers Data:
    **/
    QMTime   air_finished_time = 0_ms;
    QMTime   damage_debounce_time = 0_ms;
    QMTime   fly_sound_debounce_time = 0_ms;    // move to clientinfo
    QMTime   last_move_time = 0_ms;
    QMTime   touch_debounce_time = 0_ms;        // are all these legit?  do we need more/less of them?
    QMTime   pain_debounce_time = 0_ms;
    QMTime   show_hostile_time = 0_ms;
    //! Registers the time of the frame we died in.
	QMTime   death_time = 0_ms;
    //! Used for player trail.
    QMTime   trail_time = 0_ms;


    /**
    *   Various Data:
    **/
    //! Set when the entity gets hurt(SVG_DamageEntity) and might be its cause of death.
    sg_means_of_death_t meansOfDeath = static_cast<sg_means_of_death_t>(0);
    //! [SpawnKey]: Used for target_changelevel. Set as key/value.
    svg_level_qstring_t map = nullptr;
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