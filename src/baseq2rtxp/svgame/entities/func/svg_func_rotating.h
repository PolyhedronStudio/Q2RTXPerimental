/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_rotating'.
*
*
********************************************************************/
#pragma once



/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_rotating_rotating'.
*
*
********************************************************************/
#pragma once


/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_rotating'.
*
*
********************************************************************/
#pragma once



// Needed.
#include "svgame/entities/svg_base_edict.h"
#include "svgame/entities/svg_pushmove_edict.h"



/**
*
*
*   Rotating Door Entity Structure:
*
*
**/
struct svg_func_rotating_t : public svg_pushmove_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_func_rotating_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_func_rotating_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_func_rotating_t() = default;


    /**
    *
    *	Define this as: "func_rotating" = svg_base_edict_t -> svg_pushmove_edict_t -> svg_func_door_t -> svg_func_dor_rotating_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "func_rotating", svg_func_rotating_t, svg_pushmove_edict_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_func_rotating_t::onSpawn
    );


    #if 0
    /**
    *
    *   Save Descriptor Fields:
    *
    **/
    //! Declare the save descriptor field handling function implementations.
    SVG_SAVE_DESCRIPTOR_FIELDS_DECLARE_IMPLEMENTATION();



    /**
    *
    *   Core:
    *
    **/
    /**
    *   Reconstructs the object, optionally retaining the entityDictionary.
    **/
    virtual void Reset( const bool retainDictionary = false ) override;

    /**
    *   @brief  Save the entity into a file using game_write_context.
    *   @note   Make sure to call the base parent class' Restore() function.
    **/
    virtual void Save( struct game_write_context_t *ctx ) override;
    /**
    *   @brief  Restore the entity from a loadgame read context.
    *   @note   Make sure to call the base parent class' Restore() function.
    **/
    virtual void Restore( struct game_read_context_t *ctx ) override;

    /**
    *   @brief  Called for each cm_entity_t key/value pair for this entity.
    *           If not handled, or unable to be handled by the derived entity type, it will return
    *           set errorStr and return false. True otherwise.
    **/
    virtual const bool KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) override;
    #endif // #if 0


    /**
    *
    *   Callback Member Functions:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_func_rotating_t, onSpawn );
    /**
    *   @brief  Touched.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_func_rotating_t, onTouch );
    /**
    *   @brief  Blocked.
    **/
    DECLARE_MEMBER_CALLBACK_BLOCKED( svg_func_rotating_t, onBlocked );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_func_rotating_t, onUse );
    /**
    *   @brief  Signal Receiving:
    **/
    DECLARE_MEMBER_CALLBACK_ON_SIGNALIN( svg_func_rotating_t, onSignalIn );


    /**
    *
    *   Member Functions:
    *
    **/
    /**
	*   @brief  Start acceleration.
    **/
	void StartAcceleration();
    /**
*   *   @brief  Start moving into the deceleration process.
    **/
	void StartDeceleration();
    /**
*   *   @brief  Togles moving into the acceleration/deceleration process.
    **/
    void ToggleAcceleration( const int32_t useValue );

    /**
	*   @brief  Process the acceleration.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_rotating_t, onThink_ProcessAcceleration );
    /**
	*   @brief  Process the deceleration.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_rotating_t, onThink_ProcessDeceleration );

    /**
	*   @brief  Start sound playback.
    **/
    void StartSoundPlayback();
    /**
	*   @brief  Stop sound playback.
    **/
    void EndSoundPlayback();

    /**
    *   @brief
    **/
    void LockState();
    /**
    *   @brief
    **/
    void UnLockState();


    /**
    *
    *   Member Variables:
    *
    **/


    /**
    *   Spawnflags:
    **/
    //! Starts ON.
    static constexpr spawnflag_t SPAWNFLAG_START_ON = BIT( 0 );
    //! Moves into reverse direction..
    static constexpr spawnflag_t SPAWNFLAG_START_REVERSE = BIT( 1 );
    //! Accelerate/Decelerate into velocity desired speed.
    static constexpr spawnflag_t SPAWNFLAG_ACCELERATED = BIT( 2 );
    //! Rotate around X axis.
    static constexpr spawnflag_t SPAWNFLAG_ROTATE_X = BIT( 3 );
    //! Rotate around Y axis.
    static constexpr spawnflag_t SPAWNFLAG_ROTATE_Y = BIT( 4 );
    //! Pain on Touch.
    static constexpr spawnflag_t SPAWNFLAG_PAIN_ON_TOUCH = BIT( 5 );
    //! Blocking object stops movement.
    static constexpr spawnflag_t SPAWNFLAG_BLOCK_STOPS = BIT( 6 );
    //! Brush has animations.
    static constexpr spawnflag_t SPAWNFLAG_ANIMATE_ALL = BIT( 7 );
    //! Brush has animations.
    static constexpr spawnflag_t SPAWNFLAG_ANIMATE_ALL_FAST = BIT( 8 );

    /**
    *   For readability's sake:
    **/
    static constexpr svg_pushmove_state_t ROTATING_STATE_DECELERATE_END = PUSHMOVE_STATE_TOP;
    static constexpr svg_pushmove_state_t ROTATING_STATE_ACCELERATE_END = PUSHMOVE_STATE_BOTTOM;
    static constexpr svg_pushmove_state_t ROTATING_STATE_MOVE_DECELERATING = PUSHMOVE_STATE_MOVING_UP;
    static constexpr svg_pushmove_state_t ROTATING_STATE_MOVE_ACCELERATING = PUSHMOVE_STATE_MOVING_DOWN;
};