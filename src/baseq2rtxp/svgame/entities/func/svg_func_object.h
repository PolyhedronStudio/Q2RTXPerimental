/********************************************************************
*
*
*	ServerGame: Brush Entity 'func_object'.
*
*
********************************************************************/
#pragma once



// Needed.
#include "svgame/entities/svg_base_edict.h"


/**
*
*
*   Object Entity Structure, a solid bmodel that will fall if its support is removed.
*   It does so by converting to MOVETYPE_TOSS
*
*
**/
struct svg_func_object_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_func_object_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_func_object_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_func_object_t() = default;


    /**
    *
    *	Define this as: "func_object" = svg_base_edict_t -> svg_func_object_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "func_object", svg_func_object_t, svg_base_edict_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_func_object_t::onSpawn
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
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_func_object_t, onSpawn );
    /**
    *   @brief  Post-Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_func_object_t, onPostSpawn );
    /**
    *   @brief  Thinking.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_object_t, onThink );
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_object_t, onThink_ReleaseObject );

    /**
    *   @brief  Blocked.
    **/
    //DECLARE_MEMBER_CALLBACK_BLOCKED( svg_func_object_t, onBlocked );

    /**
    *   @brief  Touched.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_func_object_t, onTouch );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_func_object_t, onUse );
    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_USE( svg_func_object_t, onUse_AreaPortal );
    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_PAIN( svg_func_object_t, onPain );
    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_DIE( svg_func_object_t, onDie );
    /**
    *   @brief  Signal Receiving:
    **/
    //DECLARE_MEMBER_CALLBACK_ON_SIGNALIN( svg_func_object_t, onSignalIn );


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
    /**
    *   Spawnflags:
    **/
    //! Object releases/drops after use triggered.
    static constexpr spawnflag_t SPAWNFLAG_RELEASE_AFTER_TRIGGER_USE = BIT( 0 );
    //! Object is hidden until use triggered.
	static constexpr spawnflag_t SPAWNFLAG_HIDDEN_UNTIL_TRIGGER_USE = BIT( 1 );
    //! Won't be triggered by monsters.
    static constexpr spawnflag_t SPAWNFLAG_NOMONSTER = BIT( 4 );
    //! Fires targets when damaged.
    //static constexpr spawnflag_t SPAWNFLAG_DAMAGE_ACTIVATES = BIT( 6 );
    //! Has animation.
    static constexpr spawnflag_t SPAWNFLAG_ANIMATED = BIT( 8 );
    //! Has animation.
    static constexpr spawnflag_t SPAWNFLAG_ANIMATED_FAST = BIT( 9 );
};