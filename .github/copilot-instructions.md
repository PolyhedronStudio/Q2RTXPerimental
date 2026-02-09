# Copilot Instructions

## General Guidelines
- Use 1TBS/K&R braces everywhere for new/modified code.
- Always use `nullptr` (avoid `NULL`).
- Prefer tabs for indentation; otherwise, use 4 spaces unless the file predominantly uses a different indent width. Match the immediate file’s indentation quirks.
- When implementing new systems, prefer enforcing drop limits both in A* edge validation and in path-following safety checks.
- When adjusting monster navigation/movement, always account for slopes and stairs (walkable surfaces, step-up/step-down) alongside cornering and center-origin hull issues.

## Code Style
- Follow consistent formatting rules across the project.
- Adhere to naming conventions as specified in the project documentation.
- Prefer Doxygen-style comments with tab-indented @tags (e.g., /**\n*\t@brief ...\n*\t@note ...\n**/) and section header blocks in .cpp files.
- Use explanatory // comments before small code blocks.

## Global/static variable documentation

- When defining a global (file-scope) or namespace-scope variable, document it with a `//!` comment on the line immediately preceding the variable.
- The `//!` comment must describe what the variable represents and/or what it is used for.
- If a `//!`-above style would significantly disrupt the surrounding formatting (rare), it is acceptable to use a nearby short block comment, but the preference is always `//!`.

Example:

```cpp
//! An actual descriptive name of what this variable is and/or might be used for.
int32_t descriptiveVariableName;
```

## Control-flow / condition intent comments

- For non-trivial `if` statements and `for`/`while` loops, add a short comment explaining **WHY** the condition exists and **WHAT** it is checking for.
- Prefer placing this comment immediately above the statement (or immediately above the controlling condition if wrapped across lines).

Example:

```cpp
//! Maximum amount of loop iterations.
int32_t maxLength = 0;
//! Iterate over the loop and explain here why/what it is up to.
for ( int32_t i = 0; i < maxLength; i++ ) {
	// ..
}
```

### Function-block comment style
- For blocks of code designated to a specified action inside a function, prefer a short starred comment block that explains the purpose of that code section. If the block appears inside a class declaration, keep it as a brief topic keyword section instead.
- Example usage inside a function:

```cpp
/**
*    There should generally be a comment explaining what is up if this is located within a function.
*    This explains the intent of the following code block and any important invariants.
**/
// ...code for the described action...
```
// Line-by-line / subsection comments
- For complex blocks that perform several steps (e.g., waypoint conversion, approach speed computation), add a short comment for each logical subsection or important line. This makes the code easier to scan and reason about.
- Example annotated block:

```cpp
// Convert next nav point from navmesh space to entity feet-origin space.
navPathProcess.GetNextPathPointEntitySpace( &nextNavPoint_feet );
// Compute vertical delta between nav point and our feet-origin.
float zDelta = std::fabs( nextNavPoint_feet.z - currentOrigin.z );
// Threshold under which we treat the waypoint as same step level
constexpr float zStepThreshold = 32.0f;
// Base movement speed used for frame velocity calculations.
double frameVelocity = 220.0;
// If the target is roughly on the same horizontal plane, compute slowdown.
if ( zDelta < zStepThreshold ) {
    // Vector from us to the nav waypoint.
    Vector3 toGoal = QM_Vector3Subtract( nextNavPoint_feet, currentOrigin );
    // Horizontal distance to the waypoint.
    float dist2D = std::sqrt( toGoal.x * toGoal.x + toGoal.y * toGoal.y );
    // Desired separation so we don't clip into the target.
    constexpr float desiredSeparation = 8.0f;
    // Distance remaining until desired separation.
    float approachDist = std::max( 0.0f, dist2D - desiredSeparation );
    // Range over which to slow down when approaching.
    constexpr float slowDownRange = 64.0f;
    // Scale speed between 0..1 based on approach distance.
    float speedScale = ( slowDownRange > 0.0f )
        ? QM_Clamp( approachDist / slowDownRange, 0.0f, 1.0f )
        : 1.0f;
    // Apply slowdown.
    frameVelocity *= speedScale;
    // If we're effectively at the target, stop.
    if ( approachDist <= 0.0f ) {
        frameVelocity = 0.0f;
    }
}
```

// Example: Path-query function annotated with subsection and line-level comments
// Use this as the canonical example for QueryDirection-style functions.
```cpp
/**
 *	@brief	Query the next 3D movement direction while advancing 2D waypoints.
 *	@param	current_origin	Current agent feet-origin.
 *	@param	policy		Path-follow policy (waypoint radius, etc.).
 *	@param	out_dir3d	[out] Normalized 3D direction if available.
 *	@return	True if a valid direction was produced.
 **/
const bool svg_nav_path_process_t::QueryDirection3D( const Vector3 &current_origin, const svg_nav_path_policy_t &policy, Vector3 *out_dir3d ) {
    /**
    *    Validate output pointer and ensure we have a path to query.
    **/
    if ( !out_dir3d ) {
        return false;
    }
    if ( path.num_points <= 0 || !path.points ) {
        return false;
    }

    /**
    *    Transform caller origin to nav-center space using stored center offset.
    **/
    const Vector3 query_origin = QM_Vector3Add( current_origin, Vector3{ 0.0f, 0.0f, path_center_offset_z } );

    /**
    *    Query the movement direction and advance waypoints in 2D while
    *    returning a 3D direction for stairs/steps handling.
    **/
    Vector3 dir = {};
    int32_t idx = path_index;
    const bool ok = SVG_Nav_QueryMovementDirection_Advance2D_Output3D( &path, query_origin, policy.waypoint_radius, &idx, &dir );

    /**
    *    Sanity: if the nav query failed, bail out without altering state.
    **/
    if ( !ok ) {
        return false;
    }

    /**
    *    Commit advanced index so future queries continue along the path.
    **/
    path_index = idx;

    /**
    *    Validate the returned direction magnitude before normalizing to avoid
    *    numerical instability on near-zero vectors.
    **/
    const float len2 = ( dir.x * dir.x ) + ( dir.y * dir.y ) + ( dir.z * dir.z );
    if ( len2 <= ( 0.001f * 0.001f ) ) {
        return false;
    }

    /**
    *    Output normalized 3D direction.
    **/
    *out_dir3d = QM_Vector3Normalize( dir );
    return true;
}
```

## Mandatory commenting checklist (ENFORCED)

To avoid missing comments in future edits, every PR that modifies C++ code must ensure the following checklist is satisfied for each changed file. Reviewers should expect these items and CI/static checks may be added to enforce them.

- Every non-trivial function has a Doxygen-style header with at least @brief and @note/@param/@return where applicable.
- Within each function, every logical subsection (sanity checks, resource setup, major algorithm steps, state updates, I/O, early returns, safety checks) must be preceded by a short comment block (starred `/** ... **/` preferred) explaining intent.
- For complex multi-step subsections (pathfinding, physics/step handling, approach/speed computation), add line-by-line or per-statement comments for critical computations (see Line-by-line example above).
- For I/O-heavy code (load/save/serialization) and for loops that mutate structures, add a brief action comment above each important statement that changes state (allocations, reads/writes, index updates, frees, pointer swaps). If a line is not self-explanatory, it must be preceded by a short `//` comment.
- When refactoring an existing file, bring the whole file up to the standard for any function you touch. Do not only comment the lines you changed; ensure the entire function reads like a narrated set of actions.
- Small trivial statements (e.g., simple assignments) do not require individual comments unless their intent is non-obvious.
- Loops, early returns, and guards must have a short comment explaining their purpose and expected invariants.

Enforcement: any PR that modifies C++ files must include in its description a copy of the checklist annotated for each modified file (which function sections were added/changed and where comments were placed). CI may add automated checks to reject PRs that fail to include the required comment blocks.

### Template for new/modified functions (copy into PR description to show compliance):
```cpp
/**
*    @brief	Short description of function purpose.
*    @param	... (if applicable)
*    @return	... (if applicable)
*    @note	Any important notes or invariants.
**/
ReturnType FuncName( Params ) {
    /**
    *    Sanity checks / early returns.
    **/
    if ( ... ) {
        // Brief comment for why we return
        return ...;
    }

    /**
    *    Logical subsection A: what it does and why.
    **/
    // Line-level comment: explain key calculations
    auto v = ComputeSomething( ... );

    /**
    *    Logical subsection B: e.g., pathfinding, animation selection, physics step.
    **/
    // ...more commented steps...

    return result;
}
```
When in doubt: add a comment. It's preferable to be slightly verbose rather than leave intent unclear.

- Example brief topic inside a class declaration:

```cpp
/**
*    Movement handling.
*        - Handles stepping up/down stairs and slope corrections.
*        - Uses nav heuristics to avoid large drops.
**/
```

### Explicit Doxygen and sanity-check comments
- Add full Doxygen-style comment blocks for functions, using a tab after the leading `*` for tags and lines. Include any useful @brief, @param, @return, @note or @description entries. Be explicit even for simple functions — rather be slightly verbose than unclear.
- Also comment small/sanity-check code blocks inside functions (e.g., null checks, early returns) so future readers immediately see intent.
- Example function header with Doxygen tags and a commented sanity-check block:

```cpp
/**
*    I think you already do this but, add all the useful doxygens here like this, with a tabbed indent and a tab after that:
*    @brief	This is an example line.
*    @description	But you're intended to add any relevant and useful parameters, notes,
*                into this list here.
**/
bool ResetNumbersList( int *numList, int numCount ) {
    // Sanity check: ensure caller provided a valid list.
    if ( numList == nullptr ) {
        // Developer print the actual error we are facing.
        gi.dprintf( "%s: numList == (nullptr)!", __func__ );
        // We failed, thus return false.
        return false;
    }

    /**
    * We will iterate the list, resetting each entry one at a time
    * so that we do not stall the process.
    **/
    // Iterate over the numbers list.
    for ( int32_t i = 0; i < numCount; i++ ) {
        // Print the value as developer.
        gi.dprintf( "%s: (#%d)", __func__, numList[ i ] );
        // Reset the entry to 0.
        numList[ i ] = 0;
    }
    // Completed successfully, return true.
    return true;
}
```

### Emphasize commenting intent for small helper sections
- It's better to be slightly too obvious than unclear. Small helpers and sections that perform a single logical action should have a short comment block describing intent, inputs, and any invariants. Example:

```cpp
/**
*    @brief	ember helper: Face a horizontal target smoothly by blending between a hint direction and the exact target direction.
*/
void svg_monster_testdummy_t::FaceTargetHorizontal( const Vector3 &directionHint, const Vector3 &targetPoint, float blendFactor, float yawSpeed ) {
    // Initially initialize to current yaw. (It will slerp so what we eventually face targetPoint ).
    Vector3 target = targetPoint;
    // Project to feet
    target.z = targetPoint.z;
    // Compute direction to target.
    Vector3 toTarget = QM_Vector3Subtract( target, currentOrigin );
    // Project to horizontal plane.
    const float dist2D = std::sqrt( toTarget.x * toTarget.x + toTarget.y * toTarget.y );
    // Face only if we have a valid horizontal direction to face.
    if ( dist2D > 0.001f ) {
        // Determine blended facing direction.
        Vector3 faceDir = QM_Vector3SlerpLocal( directionHint, QM_Vector3Normalize( toTarget ), blendFactor );
        // Update the yaw properties.
        ideal_yaw = QM_Vector3ToYaw( faceDir );
        yaw_speed = yawSpeed;
        // Finally face the ideal yaw.
        SVG_MMove_FaceIdealYaw( this, ideal_yaw, yaw_speed );
    }
}
```
- Use spaced parameter style in function declarations, e.g., `void Func( const Type &param, const int32_t x, const bool flag = false );`
- For header (or rare TU) class declarations, use `svg_func_plat_trigger_t` as the canonical minimal example: sectioned Doxygen blocks for Construct/Destruct, DefineWorldSpawnClass, Save Descriptor Fields, Core (Reset/Save/Restore), Callback Member Functions, Member Functions, Member Variables; with small //! member comments and the established indentation/spacing.
- Member functions can have Doxygen comments, and member variables can also be documented.

Here is an example header file with a class declaration:
```cpp
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
```

And here is an example of a .cpp file with section headers:
```cpp
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
// Includes needed.
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"
#include "svgame/svg_entity_events.h"

// Entity includes.
#include "svgame/entities/svg_base_edict.h"
#include "sharedgame/sg_entities.h"

// CoPilot style includes.
#include "shared/copilot_cppfile_styles.h"


/**
*
*
*
*	Static Member Initialization and Method Definitions:
*
*
*
**/
/**
*	@brief	This is a reference...
*			New lines go like this...
**/
svg_example_copilot_class::staticExampleMember = 42; // Static member initialization outside of class definition.



/**
*
*
*
*	Catagory Description and stating it is their function implementations:
*
*
*
**/
/**
*	@brief	An example method. We can place more text here and either keep it on this line but ultimately we
*			break down to a second line because horizontal scrolling sucks.
**/
void svg_example_copilot_class::SetCurrentHealth( const int32_t newCurrentHealth, const health_set_flags flags ) {
	// Example code here.
	if ( ( flags & HEALTH_SET_FLAG_NO_OVERHEAL ) != 0 && ( newCurrentHealth > 100 ) ) {
		currentHealth = 100;
	} else {
		currentHealth = newCurrentHealth;
	}

	if ( ( flags & HEALTH_SET_FLAG_NOTIFY ) != 0 ) {
		Com_Error( ERR_DISCONNECT, "%s's health is now %d.\n", displayName.c_str(), currentHealth );
		return;
	}

	// Just a random extra style example for reference.
	const uint64_t exampleBitmask = COPILOT_STYLE_EXAMPLE | BIT_ULL( 44 );
	if ( ( exampleBitmask & COPILOT_STYLE_EXAMPLE ) != 0 ) {
		// Do something here. Like print a debug message.
		gi.dprintf( "%s: Example bitmask flag is set!\n", __func__ );

		// Iterate over all the bits in the bitmask. 
		// Debug printing as we go which bits are precisely set.
		for ( int32_t i = 0; i < 64; i++ ) {
			// Check if bit i is set.
			if ( ( exampleBitmask & ( 1ULL << i ) ) != 0 ) {
				// Debug print the bit that is set.
				gi.dprintf( "%s: Bit %d is set in exampleBitmask.\n", __func__, i );
			}
		}
	}
}
/**
*	@brief	Get current Health.
*	@note	To @CoPilot, these functions are sometimes inlined in the class, or merely declared inside of it and defined outside of it in a general translation unit.
**/
const int32_t svg_example_copilot_class::GetCurrentHealth() const { // <-- Note the const at the end for const correctness. Only apply if necessary.
	return currentHealth;
}
#endif
```
