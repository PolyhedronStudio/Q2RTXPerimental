/********************************************************************
*
*
*	ServerGame: Trigger Entity 'svg_trigger_hurt'.
*
*
********************************************************************/
#pragma once



/***
*
*
*	trigger_hurt
*
*
***/
struct svg_trigger_hurt_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_trigger_hurt_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_trigger_hurt_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_trigger_hurt_t() = default;


    /**
    *
    *	Define this as: "trigger_hurt" = svg_base_edict -> svg_trigger_hurt_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:    classType:           superClassType:
        "trigger_hurt", svg_trigger_hurt_t, svg_base_edict_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_trigger_hurt_t::onSpawn
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
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_trigger_hurt_t, onSpawn );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_trigger_hurt_t, onUse );
    /**
    *   @brief  Touched.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_trigger_hurt_t, onTouch );



    /**
    *
    *   Member Functions:
    *
    **/
    // None.



    /**
    *
    *   Member Variables:
    *
    **/
    //! Starts off.
    static constexpr int32_t SPAWNFLAG_START_OFF = 1;
    //! Toggleable.
    static constexpr int32_t SPAWNFLAG_TOGGLE = 2;
    //! No moaning.
    static constexpr int32_t SPAWNFLAG_SILENT = 4;
    //! STDs.
    static constexpr int32_t SPAWNFLAG_NO_PROTECTION = 8;
    //! Slow hurting.
    static constexpr int32_t SPAWNFLAG_SLOW_HURT = 16;
    //! Strictly clip to brush.
    static constexpr int32_t SPAWNFLAG_BRUSH_CLIP = 32;
};