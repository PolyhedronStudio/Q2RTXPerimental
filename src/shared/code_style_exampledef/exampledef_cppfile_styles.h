//! This is where GitHub CoPilot can analyse header file style from.
#ifdef STYLE_TYPE_FOR_COPILOT
/********************************************************************
*
*
*	ServerGame: Small Description, e.g Entities: "func_door"
*
*   Brief and/or long description depending on the complexity of our needs.
*
*
********************************************************************/
#pragma once



/**
*   Optional Debug or other Defines/ConstExpr/Macros go here usually.
**/
#define _DEBUG_SVG_COPILOT_EXAMPLE 1



/**
*	@brief	This is a reference...
*			New lines go like this...
**/
class svg_example_copilot_class {
public:
    /**
    *	Constructor/Deconstructor:
    **/
    svg_example_copilot_class() = default;
    ~svg_example_copilot_class() = default;

    /**
    *
    *
    *	Some SubCatagory of Member Functions/Variables
    *
    *
    **/
    //! Static member, declared here defined outside.
	static const int32_t staticExampleMember;

    //! This is the name of the player
    std::string displayName = "Player";
    //! The health of the player
    int32_t currentHealth = 100;
    //! Current armor of the player
    int32_t currentArmor = 100;
    //! Some Vector.
    Vector3 position = { 0.f, 0.f, 0.f };

    //! Some constexpr
    static constexpr int32_t COPILOT_STYLE_EXAMPLE = BIT( 0 );

    /**
    *
    *
    *	Some SubCatagory of Member Functions/Variables
    *
    *
    **/
    /**
    *	@brief	An example enum type definition within the class.
    **/
    enum health_set_flags {
        //! Set health with no flags.
        HEALTH_SET_FLAG_NONE = 0,
        //! Do not allow overheal beyond max health.
        HEALTH_SET_FLAG_NO_OVERHEAL = 1 << 0,
        //! Notify the player of health change.
        HEALTH_SET_FLAG_NOTIFY = 1 << 1
    };

    /**
    *	@brief	An example method. We can place more text here and either keep it on this line but ultimately we
    *			break down to a second line because horizontal scrolling sucks.
    **/
    void SetCurrentHealth( const int32_t newCurrentHealth, const health_set_flags flags );
    /**
    *	@brief	Get current Health.
    *	@note	To @CoPilot, these functions are sometimes inlined in the class, or merely declared inside of it and defined outside of it in a general translation unit.
    **/
    const int32_t GetCurrentHealth() const; // <-- Note the const at the end for const correctness. Only apply if necessary.
    
    /**
    *	@brief	This is a function belonging to the SubCatagory but related to something else than Health.
    *			Instead it is about armor!
    **/
    inline void SetCurrentArmor( const int32_t newCurrentArmor ) {
        currentArmor = newCurrentArmor;
    }
    /**
    *	@brief	Get current Armor.
    **/
    inline const int32_t GetCurrentArmor() const { // <-- Note the const at the end for const correctness. Only apply if necessary.
        return currentArmor;
    }
};
#endif