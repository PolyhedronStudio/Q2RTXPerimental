/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_button'.
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
struct svg_func_button_t : public svg_pushmove_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_func_button_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_func_button_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_func_button_t() = default;


    /**
    *
	*	Define this as: "func_button" = svg_base_edict_t -> svg_pushmove_edict_t -> svg_func_button_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "func_button", svg_func_button_t, svg_pushmove_edict_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_func_button_t::onSpawn
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
    *   TestDummy Callback Member Functions:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_func_button_t, onSpawn );
    /**
    *   @brief  Post-Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_func_button_t, onPostSpawn );
    /**
    *   @brief  Thinking.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_button_t, onThink_Return );
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_button_t, onThink_PressMove );
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_button_t, onThink_UnPressMove );

    /**
    *   @brief  Blocked.
    **/
    DECLARE_MEMBER_CALLBACK_BLOCKED( svg_func_button_t, onBlocked );

    /**
    *   @brief  Touched.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_func_button_t, onTouch );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_func_button_t, onUse );
    DECLARE_MEMBER_CALLBACK_USE( svg_func_button_t, onUse_UseTarget_Press );
    DECLARE_MEMBER_CALLBACK_USE( svg_func_button_t, onUse_UseTarget_Toggle );
    DECLARE_MEMBER_CALLBACK_USE( svg_func_button_t, onUse_UseTarget_Continuous_Press );

    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_USE( svg_func_button_t, onUse_AreaPortal );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_PAIN( svg_func_button_t, onPain );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_DIE( svg_func_button_t, onDie );
    /**
    *   @brief  Signal Receiving:
    **/
    DECLARE_MEMBER_CALLBACK_ON_SIGNALIN( svg_func_button_t, onSignalIn );

    /**
    *   PushMoveInfo EndMoves:
    **/
    DECLARE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_button_t, onUnPressEndMove );
    DECLARE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_button_t, onPressEndMove );

    /**
    *
    *   Member Functions:
    *
    **/
    ///**
    //*	@brief  Open or Close the door's area portal.
    //**/
    //void SetAreaPortal( const bool isOpen );


    /**
    *
    *   Member Variables:
    *
    **/
    /**
    *   These are the button frames, pressed/unpressed will toggle from pressed_0 to unpressed_0, leaving the
    *   pressed_1 and unpressed_1 as an animation frame in case they are set.
    *
    *   The first and second(if animating) frame for PRESSED state.
    *   svg_func_button_t::FRAME_PRESSED_0 -> This should be "+0texturename"
    *   svg_func_button_t::FRAME_PRESSED_1 -> This should be "+1texturename"
    *
    *   The first and second(if animating) frame for UNPRESSED state.
    *   svg_func_button_t::FRAME_UNPRESSED_0 -> This should be "+2texturename"
    *   svg_func_button_t::FRAME_UNPRESSED_1 -> This should be "+3texturename"
    **/
    static constexpr int32_t FRAME_PRESSED_0 = 0;
    static constexpr int32_t FRAME_PRESSED_1 = 1;
    static constexpr int32_t FRAME_UNPRESSED_0 = 2;
    static constexpr int32_t FRAME_UNPRESSED_1 = 3;
    /**
    *   Button SpawnFlags:
    **/
    //! Button starts pressed.
    static constexpr spawnflag_t SPAWNFLAG_START_PRESSED = BIT( 0 );
    //! Button can't be triggered by monsters.
    static constexpr spawnflag_t SPAWNFLAG_NO_MONSTERS = BIT( 1 );
    //! Button fires targets when touched.
    static constexpr spawnflag_t SPAWNFLAG_TOUCH_ACTIVATES = BIT( 2 );
    //! Button fires targets when damaged.
    static constexpr spawnflag_t SPAWNFLAG_DAMAGE_ACTIVATES = BIT( 3 );
    //! Button is Toggleable (Fires targets when toggled 'On').
    static constexpr spawnflag_t SPAWNFLAG_TOGGLEABLE = BIT( 4 );
    //! Button is locked from spawn, so it can't be used.
    static constexpr spawnflag_t SPAWNFLAG_LOCKED = BIT( 5 );
    //! Does not determine UseTarget Hints for display.
    static constexpr spawnflag_t SPAWNFLAG_DISABLE_USETARGET_HINTING = BIT( 6 );
    /**
    *   For readability's sake:
    **/
    static constexpr svg_pushmove_state_t STATE_PRESSED = PUSHMOVE_STATE_TOP;
    static constexpr svg_pushmove_state_t STATE_UNPRESSED = PUSHMOVE_STATE_BOTTOM;
    static constexpr svg_pushmove_state_t STATE_MOVING_TO_PRESSED_STATE = PUSHMOVE_STATE_MOVING_UP;
    static constexpr svg_pushmove_state_t STATE_MOVING_TO_UNPRESSED_STATE = PUSHMOVE_STATE_MOVING_DOWN;
};
