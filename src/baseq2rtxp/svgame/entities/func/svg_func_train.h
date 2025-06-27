/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_train'.
*
*
********************************************************************/
#pragma once


/**
*	@brief	For trigger_elevator.
**/
void train_resume( svg_base_edict_t *self );

/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_train'.
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
*   Train Trigger Entity Structure:
*
*
**/
#if 0
struct svg_func_train_trigger_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_func_train_trigger_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_func_train_trigger_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_func_train_trigger_t() = default;


    /**
    *
    *	Define this as: "func_train_trigger" = svg_base_edict_t -> svg_func_train_trigger_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "func_train_trigger", svg_func_train_trigger_t, svg_base_edict_t,
        // typeInfoFlags:
        /*EdictTypeInfo::TypeInfoFlag_WorldSpawn |*/ EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_func_train_trigger_t::onSpawn
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
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_func_train_trigger_t, onSpawn );
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_func_train_trigger_t, onPostSpawn );
    /**
    *   @brief  Touched.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_func_train_trigger_t, onTouch );



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
#endif


/**
*
*
*   Train Trigger Entity Structure:
*
*
**/
#if 0
struct svg_path_corner_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_path_corner_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_path_corner_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_path_corner_t() = default;


    /**
    *
    *	Define this as: "path_corner" = svg_base_edict_t -> svg_path_corner_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "path_corner", svg_path_corner_t, svg_base_edict_t,
        // typeInfoFlags:
        /*EdictTypeInfo::TypeInfoFlag_WorldSpawn |*/ EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_path_corner_t::onSpawn
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
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_path_corner_t, onSpawn );
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_path_corner_t, onPostSpawn );
    /**
    *   @brief  Touched.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_path_corner_t, onTouch );



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
#endif


/**
*
*
*   'Train' Entity Structure:
*
*
**/
struct svg_func_train_t : public svg_pushmove_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_func_train_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_func_train_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_func_train_t() = default;


    /**
    *
    *	Define this as: "func_train" = svg_base_edict_t -> svg_pushmove_edict_t -> svg_func_train_t
    *
    **/
    DefineWorldSpawnClass(
        // classname:               classType:               superClassType:
        "func_train", svg_func_train_t, svg_pushmove_edict_t,
        // typeInfoFlags:
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        // spawnFunc:
        svg_func_train_t::onSpawn
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
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_func_train_t, onSpawn );

    /**
    *   @brief  Post-Spawn.
    **/
    //DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_func_train_t, onPostSpawn );

    /**
    *   @brief  Thinking.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_train_t, onThink );
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_train_t, onThink_MoveToNextTarget );
    DECLARE_MEMBER_CALLBACK_THINK( svg_func_train_t, onThink_FindNextTarget );


    /**
    *   @brief  Blocked.
    **/
    DECLARE_MEMBER_CALLBACK_BLOCKED( svg_func_train_t, onBlocked );

    /**
    *   @brief  Touched.
    **/
    //DECLARE_MEMBER_CALLBACK_TOUCH( svg_func_train_t, onTouch );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_func_train_t, onUse );

    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_USE( svg_func_train_t, onUse_AreaPortal );
    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_PAIN( svg_func_train_t, onPain );
    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_DIE( svg_func_train_t, onDie );
    /**
    *   @brief  Signal Receiving:
    **/
    //DECLARE_MEMBER_CALLBACK_ON_SIGNALIN( svg_func_train_t, onSignalIn );

    /**
    *   PushMoveInfo EndMoves:
    **/
    /**
    *   @brief  Wait EndMove Callback.
    **/
    DECLARE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_train_t, onTrainWait );

    /**
    *
    *   Member Functions:
    *
    **/
    ///**
    //*	@brief  Spawns a trigger inside the plat, at
    //*           PLAT_LOW_TRIGGER and/or PLAT_HIGH_TRIGGER
    //*           when their spawnflags are set.
    //**/
    //void SpawnInsideTrigger( const bool isTop );

    /**
    *   @brief  Will resume/start the train's movement over its trajectory.
    **/
    void ResumeMovement();
    ///**
    //*   @brief  Engage the platform to go down.
    //**/
    //void BeginDownMove();


    /**
    *
    *   Member Variables:
    *
    **/
    /**
    *   Spawnflags:
    **/
    //! Has the train moving instantly after spawning.
    static constexpr spawnflag_t SPAWNFLAG_START_ON = BIT( 0 );
    //! Train movement can be toggled on/off.
    static constexpr spawnflag_t SPAWNFLAG_TOGGLE = BIT( 1 );
    //! The train will stop instead of destructing the blocking entity.
    static constexpr spawnflag_t SPAWNFLAG_BLOCK_STOPS = BIT( 2 );
    //! Fixes the offset of the pusher.
    static constexpr spawnflag_t SPAWNFLAG_FIX_OFFSET = BIT( 3 );
    //! Uses the pusher's origin brush set origin.
    static constexpr spawnflag_t SPAWNFLAG_USE_ORIGIN = BIT( 4 );
};