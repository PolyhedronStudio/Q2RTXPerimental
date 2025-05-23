/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_door'.
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
*   Door Entity Structure:
*
*
**/
struct svg_func_door_t : public svg_pushmove_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_func_door_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_func_door_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_func_door_t() = default;


    /**
    *
    *	Define this as: "func_door" = svg_base_edict_t -> svg_pushmove_edict_t -> svg_func_door_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "func_door", svg_func_door_t, svg_pushmove_edict_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_func_door_t::onSpawn
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
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_func_door_t, onSpawn );
    /**
    *   @brief  Post-Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_func_door_t, onPostSpawn );
    /**
    *   @brief  Thinking.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_door_t, onThink );
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_door_t, onThink_OpenMove );
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_door_t, onThink_CloseMove );

    /**
    *   @brief  Blocked.
    **/
    DECLARE_MEMBER_CALLBACK_BLOCKED( svg_func_door_t, onBlocked );

    /**
    *   @brief  Touched.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_func_door_t, onTouch );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_func_door_t, onUse );
    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_USE( svg_func_door_t, onUse_AreaPortal );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_PAIN( svg_func_door_t, onPain );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_DIE( svg_func_door_t, onDie );
    /**
	*   @brief  Signal Receiving:
    **/
	DECLARE_MEMBER_CALLBACK_ON_SIGNALIN( svg_func_door_t, onSignalIn );

    /**
    *   PushMoveInfo EndMoves:
    **/
    DECLARE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_door_t, onCloseEndMove );
    DECLARE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_door_t, onOpenEndMove );

    /**
    *
    *   Member Functions:
    *
    **/
    /**
    *	@brief  Open or Close the door's area portal.
    **/
    void SetAreaPortal( const bool isOpen );
    /**
    *   @brief  Setup default PushMoveInfo sounds.
    **/
    void SetupDefaultSounds();

    /**
    *
    *   Member Variables:
    *
    **/
    /**
    *   Spawnflags:
    **/
    //! Door starts opened.
    static constexpr spawnflag_t SPAWNFLAG_START_LOCKED = BIT( 0 );
    //! Door starts opened.
    static constexpr spawnflag_t SPAWNFLAG_START_OPEN = BIT( 1 );
    //! Door starts in reversed(its opposite) state.
    static constexpr spawnflag_t SPAWNFLAG_REVERSE = BIT( 2 );
    //! Door does damage when blocked, trying to 'crush' the entity blocking its path.
    static constexpr spawnflag_t SPAWNFLAG_CRUSHER = BIT( 3 );
    //! Door won't be triggered by monsters.
    static constexpr spawnflag_t SPAWNFLAG_NOMONSTER = BIT( 4 );
    //! Door fires targets when its touch area is triggered.
    static constexpr spawnflag_t SPAWNFLAG_TOUCH_AREA_TRIGGERED = BIT( 5 );
    //! Door fires targets when damaged.
    static constexpr spawnflag_t SPAWNFLAG_DAMAGE_ACTIVATES = BIT( 6 );
    //! Door does not automatically wait for returning to its initial(can be reversed) state.
    static constexpr spawnflag_t SPAWNFLAG_TOGGLE = BIT( 7 );
    //! Door has animation.
    static constexpr spawnflag_t SPAWNFLAG_ANIMATED = BIT( 8 );
    //! Door has animation.
    static constexpr spawnflag_t SPAWNFLAG_ANIMATED_FAST = BIT( 9 );
    //! Door moves into X axis instead of Z.
    static constexpr spawnflag_t SPAWNFLAG_X_AXIS = BIT( 10 );
    //! Door moves into Y axis instead of Z.
    static constexpr spawnflag_t SPAWNFLAG_Y_AXIS = BIT( 11 );
    //! Door always pushes into the negated direction of what the player is facing.
    static constexpr spawnflag_t SPAWNFLAG_BOTH_DIRECTIONS = BIT( 12 );


    /**
    *   For readability's sake:
    **/
    static constexpr svg_pushmove_state_t DOOR_STATE_OPENED = PUSHMOVE_STATE_TOP;
    static constexpr svg_pushmove_state_t DOOR_STATE_CLOSED = PUSHMOVE_STATE_BOTTOM;
    static constexpr svg_pushmove_state_t DOOR_STATE_MOVING_TO_OPENED_STATE = PUSHMOVE_STATE_MOVING_UP;
    static constexpr svg_pushmove_state_t DOOR_STATE_MOVING_TO_CLOSED_STATE = PUSHMOVE_STATE_MOVING_DOWN;
};