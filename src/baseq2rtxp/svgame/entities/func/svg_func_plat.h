/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_plat'.
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
*   Platform Entity Structure:
*
*
**/
struct svg_func_plat_trigger_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_func_plat_trigger_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_func_plat_trigger_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_func_plat_trigger_t() = default;


    /**
    *
    *	Define this as: "func_plat_trigger" = svg_base_edict_t -> svg_func_plat_trigger_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "func_plat_trigger", svg_func_plat_trigger_t, svg_base_edict_t,
        // typeInfoFlags:
        /*EdictTypeInfo::TypeInfoFlag_WorldSpawn |*/ EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_func_plat_trigger_t::onSpawn
    );

    #if 1
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
    //virtual const bool KeyValue( const cm_entity_t *keyValuePair, std::string &errorStr ) override;
    #endif // #if 0


    /**
    *
    *   Callback Member Functions:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_func_plat_trigger_t, onSpawn );
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_func_plat_trigger_t, onPostSpawn );
    /**
    *   @brief  Touched.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_func_plat_trigger_t, onTouch );



    /**
    *
    *   Member Functions:
    *
    **/



    /**
    *
    *   Member Variables:
    *
    **/
    //! An index to the entity number of the platform
	int32_t platformEntityNumber = -1;
};

/**
*
*
*   Platform Entity Structure:
*
*
**/
struct svg_func_plat_t : public svg_pushmove_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_func_plat_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_func_plat_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_func_plat_t() = default;


    /**
    *
    *	Define this as: "func_plat" = svg_base_edict_t -> svg_pushmove_edict_t -> svg_func_plat_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "func_plat", svg_func_plat_t, svg_pushmove_edict_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_func_plat_t::onSpawn
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
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_func_plat_t, onSpawn );
    /**
    *   @brief  Post-Spawn.
    **/
    //DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_func_plat_t, onPostSpawn );
    /**
    *   @brief  Thinking.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_plat_t, onThink );
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_plat_t, onThink_Idle );


    /**
    *   @brief  Blocked.
    **/
    DECLARE_MEMBER_CALLBACK_BLOCKED( svg_func_plat_t, onBlocked );

    ///**
    //*   @brief  Touched.
    //**/
    //DECLARE_MEMBER_CALLBACK_TOUCH( svg_func_plat_t, onTouch );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_func_plat_t, onUse );
    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_USE( svg_func_plat_t, onUse_AreaPortal );
    ///**
    //*   @brief
    //**/
    //DECLARE_MEMBER_CALLBACK_PAIN( svg_func_plat_t, onPain );
    ///**
    //*   @brief
    //**/
    //DECLARE_MEMBER_CALLBACK_DIE( svg_func_plat_t, onDie );
    ///**
    //*   @brief  Signal Receiving:
    //**/
    //DECLARE_MEMBER_CALLBACK_ON_SIGNALIN( svg_func_plat_t, onSignalIn );

    /**
    *   PushMoveInfo EndMoves:
    **/
    DECLARE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_plat_t, onPlatHitBottom );
    DECLARE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_plat_t, onPlatHitTop );

    /**
    *
    *   Member Functions:
    *
    **/
    /**
	*	@brief  Spawns a trigger inside the plat, at 
    *           PLAT_LOW_TRIGGER and/or PLAT_HIGH_TRIGGER
    *           when their spawnflags are set.
    **/
    void SpawnInsideTrigger( const bool isTop );

    /**
	*   @brief  Engage the platform to go up.
    **/
    void BeginUpMove();
    /**
    *   @brief  Engage the platform to go down.
    **/
    void BeginDownMove();


    /**
    *
    *   Member Variables:
    *
    **/
    //! An index to the entity number of the platform
    /**
    *   Spawnflags:
    **/
    //! Start Bottom
	static constexpr spawnflag_t SPAWNFLAG_START_BOTTOM = BIT( 0 );
    //! Plat High Trigger
	static constexpr spawnflag_t SPAWNFLAG_HIGH_TRIGGER = BIT( 1 );
    //! Plat Low Trigger
    static constexpr spawnflag_t SPAWNFLAG_LOW_TRIGGER = BIT( 2 );
    //! Plat Toggle
    static constexpr spawnflag_t SPAWNFLAG_TOGGLE = BIT( 3 );
    //! Plat Pain On Touch
    static constexpr spawnflag_t SPAWNFLAG_PAIN_ON_TOUCH = BIT( 4 ); // WID: TODO: Implement.
	//! Block Stops
	static constexpr spawnflag_t SPAWNFLAG_BLOCK_STOPS = BIT( 5 ); // WID: TODO: Implement.
    //! Animated
    static constexpr spawnflag_t SPAWNFLAG_ANIMATED = BIT( 6 );
    //! Animated Fast
    static constexpr spawnflag_t SPAWNFLAG_ANIMATED_FAST = BIT( 7 );


    /**
    *   For readability's sake:
    **/
    static constexpr svg_pushmove_state_t DOOR_STATE_OPENED = PUSHMOVE_STATE_TOP;
    static constexpr svg_pushmove_state_t DOOR_STATE_CLOSED = PUSHMOVE_STATE_BOTTOM;
    static constexpr svg_pushmove_state_t DOOR_STATE_MOVING_TO_OPENED_STATE = PUSHMOVE_STATE_MOVING_UP;
    static constexpr svg_pushmove_state_t DOOR_STATE_MOVING_TO_CLOSED_STATE = PUSHMOVE_STATE_MOVING_DOWN;
};