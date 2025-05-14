/********************************************************************
*
*
*	ServerGame: Trigger Entity 'svg_trigger_once'.
*
*
********************************************************************/
#pragma once


// Forward declare.
struct svg_trigger_multiple_t;

/***
*
*
*	trigger_once
*
*
***/
struct svg_trigger_once_t : public svg_trigger_multiple_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_trigger_once_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_trigger_once_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_trigger_once_t() = default;


    /**
    *
    *	Define this as: "trigger_once" = svg_base_edict -> svg_trigger_once_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "trigger_once", svg_trigger_once_t, svg_trigger_multiple_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_trigger_once_t::onSpawn
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
    #endif


    /**
    *
    *   Callback Member Functions:
    *
    **/
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_trigger_once_t, onSpawn );



    /**
    *
    *   Member Variables:
    *
    **/
    //! Will only be triggered once.
    static constexpr spawnflag_t SPAWNFLAG_PUSH_ONCE = 1;
    //! Will trigger when the entity is clipped(touching/inside) to brush.
    static constexpr spawnflag_t SPAWNFLAG_BRUSH_CLIP = 32;
};