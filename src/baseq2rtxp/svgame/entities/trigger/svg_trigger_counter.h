/********************************************************************
*
*
*	ServerGame: Trigger Entity 'svg_trigger_counter'.
*
*
********************************************************************/
#pragma once


// Forward declare.
struct svg_trigger_multiple_t;


/***
*
*
*	trigger_counter
*
*
***/
struct svg_trigger_counter_t : public svg_trigger_multiple_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_trigger_counter_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_trigger_counter_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_trigger_counter_t() = default;


    /**
    *
    *	Define this as: "trigger_counter" = svg_base_edict -> svg_trigger_counter_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "trigger_counter", svg_trigger_counter_t, svg_trigger_multiple_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_trigger_counter_t::onSpawn
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
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_trigger_counter_t, onSpawn );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_trigger_counter_t, onUse );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_trigger_counter_t, onTouch );


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
public:
    //! Don't print sequence logic message
    static constexpr spawnflag_t SPAWNFLAG_NO_MESSAGE = BIT( 0 );
    //! Don't print sequence logic message
    static constexpr spawnflag_t SPAWNFLAG_TOUCHABLE = 64;
};