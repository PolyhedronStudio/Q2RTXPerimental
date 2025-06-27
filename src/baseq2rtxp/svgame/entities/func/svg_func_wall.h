/********************************************************************
*
*
*	ServerGame: Brush Entity 'func_wall'.
*
*
********************************************************************/
#pragma once



// Needed.
#include "svgame/entities/svg_base_edict.h"


/**
*
*
*   A (brush entity-)wall that can be turned on and off, or toggled.
*
*
**/
struct svg_func_wall_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_func_wall_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_func_wall_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_func_wall_t() = default;


    /**
    *
    *	Define this as: "func_wall" = svg_base_edict_t -> svg_func_wall_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "func_wall", svg_func_wall_t, svg_base_edict_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_func_wall_t::onSpawn
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
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_func_wall_t, onSpawn );
    /**
    *   @brief  Post-Spawn.
    **/
    //DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_func_wall_t, onPostSpawn );
    /**
    *   @brief  Thinking.
    **/
    //DECLARE_MEMBER_CALLBACK_THINK( svg_func_wall_t, onThink );
    /**
    *   @brief  Blocked.
    **/
    //DECLARE_MEMBER_CALLBACK_BLOCKED( svg_func_wall_t, onBlocked );
    /**
    *   @brief  Touched.
    **/
    //DECLARE_MEMBER_CALLBACK_TOUCH( svg_func_wall_t, onTouch );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_func_wall_t, onUse );
    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_USE( svg_func_wall_t, onUse_AreaPortal );
    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_PAIN( svg_func_wall_t, onPain );
    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_DIE( svg_func_wall_t, onDie );
    /**
    *   @brief  Signal Receiving:
    **/
    DECLARE_MEMBER_CALLBACK_ON_SIGNALIN( svg_func_wall_t, onSignalIn );


    /**
    *
    *   Member Functions:
    *
    **/
    /**
    *   @brief  'Turns on'/'Shows' the wall.
    **/
    void TurnOn();
    /**
    *   @brief  'Turns off'/'Hides' the wall.
    **/
    void TurnOff( );
    /**
    *   @brief  Will toggle the wall's state between on and off.
    **/
    void Toggle( );



    /**
    *
    *   Member Variables:
    *
    **/
    /**
    *   Spawnflags:
    **/
    //! Spawn the wall and trigger at spawn.
    static constexpr spawnflag_t SPAWNFLAG_TRIGGER_SPAWN = BIT( 0 );
    //! Only valid for TRIGGER_SPAWN walls. This allows the wall to be turned on and off.
    static constexpr spawnflag_t SPAWNFLAG_TOGGLE = BIT( 1 );
    //! Only valid for TRIGGER_SPAWN walls. The wall will initially be present.
    static constexpr spawnflag_t SPAWNFLAG_START_ON = BIT( 2 );
    //! The wall will animate.
    static constexpr spawnflag_t SPAWNFLAG_ANIMATE = BIT( 3 );
    //! The wall will animate quickly.
    static constexpr spawnflag_t SPAWNFLAG_ANIMATE_FAST = BIT( 4 );
};