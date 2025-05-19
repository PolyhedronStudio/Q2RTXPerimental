/********************************************************************
*
*
*	ServerGame: Base Pusher Entity.
*
*
********************************************************************/
#pragma once



/********************************************************************
*
*
*	ServerGame: Door Entity 'func_door'.
*
*
********************************************************************/
#pragma once

// Needed.
#include "svgame/entities/svg_base_edict.h"


/**
*
*
*   Door Entity Structure:
*
*
**/
struct svg_pushmove_edict_t : public svg_base_edict_t {
    /**
    *
    *	Construct/Destruct.
    *
    **/
    //! Constructor. 
    svg_pushmove_edict_t() = default;
    //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
    svg_pushmove_edict_t( const cm_entity_t *ed ) : Super( ed ) { };
    //! Destructor.
    virtual ~svg_pushmove_edict_t() = default;


    /**
    *
    *	Define this as: Abstract PushMove Edict = svg_base_edict -> svg_pushmove_edict_t
    *
    **/
    DefineAbstractClass(
        // classname:               classType:               superClassType:
        svg_pushmove_edict_t, svg_base_edict_t
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
    *   @brief  Thinking.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_MoveBegin );
    DECLARE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_MoveDone );
    DECLARE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_MoveFinal );

    /**
    *   @brief  Thinking.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink );
    DECLARE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_OpenMove );
    DECLARE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, onThink_CloseMove );

    #if 0
    /**
    *   @brief  Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_SPAWN( svg_pushmove_edict_t, onSpawn );
    /**
    *   @brief  Post-Spawn.
    **/
    DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_pushmove_edict_t, onPostSpawn );


    /**
    *   @brief  Blocked.
    **/
    DECLARE_MEMBER_CALLBACK_BLOCKED( svg_pushmove_edict_t, onBlocked );

    /**
    *   @brief  Touched.
    **/
    DECLARE_MEMBER_CALLBACK_TOUCH( svg_pushmove_edict_t, onTouch );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_USE( svg_pushmove_edict_t, onUse );
    /**
    *   @brief
    **/
    //DECLARE_MEMBER_CALLBACK_USE( svg_pushmove_edict_t, onUse_AreaPortal );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_PAIN( svg_pushmove_edict_t, onPain );
    /**
    *   @brief
    **/
    DECLARE_MEMBER_CALLBACK_DIE( svg_pushmove_edict_t, onDie );
    /**
    *   @brief  Signal Receiving:
    **/
    DECLARE_MEMBER_CALLBACK_ON_SIGNALIN( svg_pushmove_edict_t, onSignalIn );
    #endif
    /**
    *	@brief	Readjust speeds so that teamed movers start/end synchronized.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, SVG_PushMove_Think_CalculateMoveSpeed );
    /**
    *   @brief  The team has completed a frame of movement, so calculate
    *			the speed required for a move during the next game frame.
	*   @note   This is the VANILLA for 10hz movement acceleration procedure.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, SVG_PushMove_Think_AccelerateMove );
    /**
    *   @brief  The team has completed a frame of movement, so calculate
    *			the speed required for a move during the next game frame.
    *   @note   This is the RERELEASE for 40hz and higher movement acceleration procedure.
    **/
    DECLARE_MEMBER_CALLBACK_THINK( svg_pushmove_edict_t, SVG_PushMove_Think_AccelerateMoveNew );

    /**
    *   PushMoveInfo EndMove Stubs:
    **/
    //! Serves as Stub.
    DECLARE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_pushmove_edict_t, onCloseEndMove );
    //! Serves as Stub.
    DECLARE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_pushmove_edict_t, onOpenEndMove );


    /**
    *
    *   Member Functions:
    *
    **/
    /**
    *   @brief  Processes movement when at top speed, so there is no acceleration present anymore.
    **/
    void SVG_PushMove_MoveRegular( const Vector3 &destination, svg_pushmove_endcallback endMoveCallback );
    /**
    *   @brief  Processes movement accelerating to top speed, or to idle speed.
    **/
    void SVG_PushMove_MoveCalculate( const Vector3 &destination, svg_pushmove_endcallback endMoveCallback );

    /**
	*   @brief  Calculates the move speed for the next frame.
    **/
    void PushMove_CalculateAcceleratedMove( );
    /**
	*   @brief  Accelerates the move.
    **/
    void PushMove_Accelerate( );

    /**
    * @brief 
    **/
    const bool Think_AccelMove_MoveInfo( );
    /**
    *   @brief
    **/
    constexpr double AccelerationDistance( const double target, const double rate );


    /**
    *
    *   Member Variables:
    *
    **/
};