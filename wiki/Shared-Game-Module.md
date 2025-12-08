# Shared Game Module (sharedgame)

The Shared Game Module contains code and data structures used by both client (`clgame`) and server (`svgame`) game modules.

**Location:** `src/baseq2rtxp/sharedgame/`  
**Output:** Static library linked into both `cgame.dll` and `game.dll`

## Overview

The sharedgame module provides:
- **Player Movement Code**: Identical physics on client and server for prediction
- **Entity Definitions**: Types, flags, events shared between modules
- **Game Mode Definitions**: Game mode constants and rules
- **Animation System**: Skeletal animation (SKM) support
- **Constants and Enums**: Shared data types to ensure consistency

## Why Shared Code?

### Client-Server Consistency

The most important reason is **client-side prediction**:

```cpp
// CLIENT SIDE (clgame)
void CLG_PredictMovement() {
    // Must use EXACT SAME code as server
    Pmove(&pm);  // From sharedgame/pmove/
}

// SERVER SIDE (svgame)
void SVG_ClientThink() {
    // Same function, same results
    Pmove(&pm);  // From sharedgame/pmove/
}
```

If client and server used different movement code, prediction would be wrong and constantly corrected.

### Avoiding Code Duplication

Without shared code:
```cpp
// BAD: Duplicated in clgame and svgame
#define EF_ROTATE BIT(1)  // In clgame
#define EF_ROTATE BIT(1)  // In svgame - must stay in sync!
```

With shared code:
```cpp
// GOOD: Defined once in sharedgame
#define EF_ROTATE BIT(1)  // Used by both
```

### Type Safety

Ensures client and server interpret data identically:
```cpp
// Shared enum ensures client and server agree
typedef enum {
    ET_GENERAL,
    ET_PLAYER,
    ET_MONSTER
} entity_type_t;
```

## Module Structure

```
sharedgame/
├── pmove/                      # Player movement code
│   ├── sg_pmove.cpp           # Core movement simulation
│   ├── sg_pmove_air.cpp       # Air movement
│   ├── sg_pmove_ground.cpp    # Ground movement
│   ├── sg_pmove_water.cpp     # Water movement
│   └── sg_pmove.h             # Movement interface
├── game_bindings/              # Lua bindings
│   └── sg_lua_bindings.cpp
├── sg_entity_types.h           # Entity type constants
├── sg_entity_flags.h           # Entity flag constants
├── sg_entity_events.h          # Entity event constants
├── sg_tempentity_events.h      # Temp entity event constants
├── sg_means_of_death.h         # Damage type constants
├── sg_muzzleflashes.h          # Muzzle flash constants
├── sg_gamemode.cpp/.h          # Game mode system
├── sg_skm.cpp/.h               # Skeletal animation
├── sg_skm_rootmotion.cpp/.h    # Root motion system
├── sg_usetarget_hints.cpp/.h   # UseTarget hint system
├── sg_time.h                   # Game time utilities
├── sg_shared.h                 # Shared definitions
└── ...
```

## Player Movement (pmove)

The most critical shared code - player physics simulation.

### Why pmove is Shared

**Problem Without Shared Movement:**
1. Client predicts player will move to position A
2. Server calculates player moves to position B (slightly different)
3. Client must snap to B (jarring/stuttering movement)

**Solution:**
- Client and server use **identical** movement code
- Prediction matches server almost perfectly
- Only network latency or packet loss causes corrections

### pmove Interface

```cpp
// In sharedgame/pmove/sg_pmove.h
typedef struct {
    // Input
    pmove_state_t   s;              // Current state
    usercmd_t       cmd;            // Player input
    
    // Collision functions (provided by caller)
    cm_trace_t      (*trace)(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
    int             (*pointcontents)(vec3_t point);
    
    // Output
    int             numtouch;       // Number of touched entities
    struct edict_s  *touchents[MAXTOUCH];  // Entities touched
    
    vec3_t          viewangles;     // View direction
    float           viewheight;     // Eye height
    vec3_t          mins, maxs;     // Bounding box
    
    int             groundentity;   // Entity standing on (-1 if none)
    int             watertype;      // Water type (if in water)
    int             waterlevel;     // 0=not in water, 1=feet, 2=waist, 3=head
    
    // Flags
    int             flags;          // PMF_* flags
} pmove_t;

// Main entry point
void Pmove(pmove_t *pm);
```

### Client Usage

```cpp
// clgame/clg_predict.cpp
void CLG_PredictMovement() {
    pmove_t pm = {};
    
    // Set up pmove
    pm.s = cl.predicted_player.pmove;
    pm.cmd = cl.cmd;  // Latest input
    
    // Provide collision functions
    pm.trace = CL_PMTrace;
    pm.pointcontents = CL_PMPointContents;
    
    // Run movement
    Pmove(&pm);  // From sharedgame
    
    // Use results for rendering
    cl.predicted_origin = pm.s.origin;
    cl.predicted_velocity = pm.s.velocity;
}
```

### Server Usage

```cpp
// svgame/svg_clients.cpp
void SVG_ClientThink(svg_base_edict_t *ent, usercmd_t *cmd) {
    pmove_t pm = {};
    
    // Set up pmove
    pm.s = ent->client->ps.pmove;
    pm.cmd = *cmd;
    
    // Provide collision functions
    pm.trace = SVG_PMTrace;
    pm.pointcontents = gi.pointcontents;
    
    // Run movement (SAME CODE as client)
    Pmove(&pm);  // From sharedgame
    
    // Apply results
    ent->client->ps.pmove = pm.s;
    ent->s.origin = pm.s.origin;
    ent->velocity = pm.s.velocity;
}
```

**Key Point:** Both client and server call `Pmove()` with same inputs and get same outputs.

### pmove Implementation

```cpp
// Simplified pmove structure
void Pmove(pmove_t *pm) {
    // 1. Categorize position (ground, air, water)
    PM_CategorizePosition(pm);
    
    // 2. Check for jump
    if (pm->cmd.upmove && pm->groundentity != -1) {
        pm->s.velocity[2] = 270;  // Jump velocity
        pm->groundentity = -1;
    }
    
    // 3. Apply friction
    if (pm->groundentity != -1) {
        PM_Friction(pm);
    }
    
    // 4. Apply acceleration (movement input)
    PM_Accelerate(pm);
    
    // 5. Apply gravity
    if (pm->groundentity == -1) {
        pm->s.velocity[2] -= pm->s.gravity * FRAMETIME;
    }
    
    // 6. Move and slide against walls
    PM_StepSlideMove(pm);
    
    // 7. Update view height (crouching, stairs)
    PM_UpdateViewHeight(pm);
    
    // 8. Detect ground entity
    PM_GroundTrace(pm);
}
```

## Entity Constants

### Entity Types

**File:** `src/baseq2rtxp/sharedgame/sg_entity_types.h`

```cpp
typedef enum sg_entity_type_e {
    ET_PLAYER = ET_GAME_TYPES,
    ET_PLAYER_CORPSE,
    ET_MONSTER,
    ET_MONSTER_CORPSE,
    ET_PUSHER,
    ET_ITEM,
    ET_GIB,
    ET_TARGET_SPEAKER,
    ET_PUSH_TRIGGER,
    ET_TELEPORT_TRIGGER,
    ET_TEMP_ENTITY_EVENT,
    ET_MAX_SHAREDGAME_TYPES
} sg_entity_type_t;
```

**Usage:**
- Server sets: `ent->s.entityType = ET_MONSTER;`
- Client reads: `if (cent->current.entityType == ET_MONSTER)`

**See:** [Entity Types API Reference](API-Entity-Types.md)

### Entity Flags

**File:** `src/baseq2rtxp/sharedgame/sg_entity_flags.h`

```cpp
#define EF_ROTATE           BIT(1)   // Rotate (bonus items)
#define EF_GIB              BIT(2)   // Leave a trail
#define EF_ANIM01           BIT(4)   // Cycle frames 0-1 at 2 Hz
#define EF_QUAD             BIT(9)   // Quad damage glow
#define EF_PENT             BIT(10)  // Pentagram of protection
// ... etc
```

**See:** [Entity Flags API Reference](API-Entity-Flags.md)

### Entity Events

**File:** `src/baseq2rtxp/sharedgame/sg_entity_events.h`

```cpp
typedef enum sg_entity_events_e {
    EV_PLAYER_FOOTSTEP = EV_ENGINE_MAX,
    EV_OTHER_FOOTSTEP,
    EV_FOOTSTEP_LADDER,
    EV_WATER_ENTER_FEET,
    EV_JUMP_UP,
    EV_FALL_SHORT,
    EV_WEAPON_PRIMARY_FIRE,
    EV_PLAYER_TELEPORT,
    EV_ITEM_RESPAWN,
    // ... etc
    EV_GAME_MAX
} sg_entity_events_t;
```

**See:** [Entity Events API Reference](API-Entity-Events.md)

### Temporary Entity Events

**File:** `src/baseq2rtxp/sharedgame/sg_tempentity_events.h`

```cpp
typedef enum temp_entity_event_e {
    TE_GUNSHOT,
    TE_BLOOD,
    TE_MOREBLOOD,
    TE_SPLASH,
    TE_SPARKS,
    TE_BULLET_SPARKS,
    TE_PLAIN_EXPLOSION,
    TE_TELEPORT_EFFECT,
    // ... etc
    TE_NUM_ENTITY_EVENTS
} temp_entity_event_t;
```

**See:** [Temporary Entity System](Temp-Entity-Overview.md)

## Game Modes

**File:** `src/baseq2rtxp/sharedgame/sg_gamemode.cpp/.h`

Game mode definitions shared between modules:

```cpp
enum class GameMode {
    SinglePlayer,
    Cooperative,
    Deathmatch,
    TeamDeathmatch,
    CaptureTheFlag
};

// Get current game mode
GameMode SG_GetGameMode();

// Check game mode
bool SG_IsSinglePlayer();
bool SG_IsCooperative();
bool SG_IsDeathmatch();
```

**Usage:**

Server:
```cpp
// svgame
if (SG_IsDeathmatch()) {
    // Deathmatch-specific rules
    RespawnPlayer(player);
}
```

Client:
```cpp
// clgame
if (SG_IsCooperative()) {
    // Show team HUD
    DrawTeamStatus();
}
```

## Skeletal Animation System

**Files:** `src/baseq2rtxp/sharedgame/sg_skm.cpp/.h`

Shared skeletal animation support for IQM models:

```cpp
// Skeletal model structure
typedef struct {
    int num_bones;
    int num_frames;
    skm_bone_t *bones;
    skm_frame_t *frames;
} skm_model_t;

// Animation blending
void SG_SKM_BlendAnimations(skm_model_t *model,
                             int anim1, float weight1,
                             int anim2, float weight2,
                             skm_pose_t *out_pose);

// Root motion extraction
void SG_SKM_ExtractRootMotion(skm_model_t *model,
                               int frame_from, int frame_to,
                               vec3_t *delta_pos, vec3_t *delta_rot);
```

**Usage:**

Both client and server can query animation data:
```cpp
// Get animation length
float duration = SG_SKM_GetAnimationDuration(model, anim_run);

// Blend walk and run animations
SG_SKM_BlendAnimations(model, 
                        anim_walk, 1.0f - runSpeed,
                        anim_run, runSpeed,
                        &pose);
```

**See:** [Skeletal Animation](Skeletal-Animation.md), [Root Motion](Root-Motion-System.md)

## Root Motion System

**Files:** `src/baseq2rtxp/sharedgame/sg_skm_rootmotion.cpp/.h`

Extracts movement from animations:

```cpp
// Extract root motion between frames
void SG_ExtractRootMotion(skm_model_t *model, 
                          int frame_a, int frame_b,
                          vec3_t *delta_position,
                          quat_t *delta_rotation);

// Apply root motion to entity
void SG_ApplyRootMotion(svg_base_edict_t *ent,
                        vec3_t delta_position,
                        quat_t delta_rotation);
```

**Benefit:** Characters move exactly as animated, not floating or sliding.

## UseTarget Hints

**Files:** `src/baseq2rtxp/sharedgame/sg_usetarget_hints.cpp/.h`

Defines interaction hints shown to player:

```cpp
enum class UseTargetHint {
    NONE,           // Not usable
    PRESSABLE,      // "Press E"
    TOGGLEABLE,     // "Toggle"
    HOLDABLE,       // "Hold E"
    PICKUP          // "Pick up"
};

// Get hint string
const char *SG_GetUseTargetHintString(UseTargetHint hint);
```

**Server:** Entities specify their hint:
```cpp
UseTargetHint svg_func_button_t::GetUseTargetHint() {
    return UseTargetHint::PRESSABLE;
}
```

**Client:** Displays appropriate message:
```cpp
UseTargetHint hint = GetTargetedEntityHint();
DrawString(screenWidth/2, screenHeight/2 + 20,
           SG_GetUseTargetHintString(hint));
```

## Means of Death

**File:** `src/baseq2rtxp/sharedgame/sg_means_of_death.h`

Damage type constants:

```cpp
typedef enum means_of_death_e {
    MOD_UNKNOWN,
    MOD_BLASTER,
    MOD_SHOTGUN,
    MOD_MACHINEGUN,
    MOD_ROCKET,
    MOD_GRENADE,
    MOD_GRENADE_SPLASH,
    MOD_FALLING,
    MOD_SUICIDE,
    MOD_TELEFRAG,
    MOD_WATER,
    MOD_SLIME,
    MOD_LAVA,
    MOD_CRUSH,
    MOD_TRIGGER_HURT,
    // ... etc
} means_of_death_t;
```

**Usage:**

Server:
```cpp
SVG_Damage(target, inflictor, attacker, /*...*/, MOD_ROCKET);
```

Client (death messages):
```cpp
const char *GetDeathMessage(means_of_death_t mod) {
    switch (mod) {
        case MOD_ROCKET:
            return "was blown up";
        case MOD_FALLING:
            return "fell to their death";
        // ... etc
    }
}
```

## Time Utilities

**File:** `src/baseq2rtxp/sharedgame/sg_time.h`

Shared game time type:

```cpp
class gametime_t {
public:
    static gametime_t from_ms(int64_t ms);
    static gametime_t from_sec(float sec);
    static gametime_t zero();
    
    int64_t count() const;          // Milliseconds
    float seconds() const;          // Seconds
    
    // Comparison operators
    bool operator<(gametime_t other) const;
    bool operator<=(gametime_t other) const;
    // ... etc
};
```

**Benefits:**
- Type-safe time representation
- Prevents mixing milliseconds and seconds
- Consistent between client and server

## Best Practices

### When to Add to Sharedgame

✅ **Add to sharedgame when:**
- Both client and server need the code
- Code must be identical on both sides (e.g., physics)
- Constants/enums need to match exactly

❌ **Don't add to sharedgame when:**
- Only server needs it (AI, combat logic)
- Only client needs it (HUD, particles)
- Behavior can differ between sides

### Conditional Compilation

Sometimes shared code needs slight differences:

```cpp
// In sharedgame
#ifdef SERVER
    // Server-specific variant
    void SomeFunction() {
        // Use server-specific gi.dprintf
        gi.dprintf("Server version\n");
    }
#else
    // Client-specific variant
    void SomeFunction() {
        // Use client-specific Com_Printf
        Com_Printf("Client version\n");
    }
#endif
```

**Use sparingly** - defeats purpose of shared code!

### Testing Shared Code

Ensure consistency:

```cpp
// Test that client and server get same result
#ifdef _DEBUG
void TestPmove() {
    pmove_t pm_client = {...};
    pmove_t pm_server = {...};
    
    // Run on both
    Pmove(&pm_client);
    Pmove(&pm_server);
    
    // Results must match
    assert(VectorCompare(pm_client.s.origin, pm_server.s.origin));
    assert(VectorCompare(pm_client.s.velocity, pm_server.s.velocity));
}
#endif
```

## Performance Considerations

### Shared Code is Compiled Twice

Sharedgame code is compiled into both client and server DLLs:
- Increases total binary size
- Worth it for consistency and reduced bugs

### Inlining

Small shared functions should be inline:

```cpp
// In sg_time.h
inline gametime_t gametime_t::from_sec(float sec) {
    return gametime_t(static_cast<int64_t>(sec * 1000));
}
```

Avoids function call overhead in performance-critical code.

## Related Documentation

- [Server Game Module](Server-Game-Module.md) - Server-side game logic
- [Client Game Module](Client-Game-Module.md) - Client-side rendering
- [Entity Types API](API-Entity-Types.md) - Entity type constants
- [Entity Flags API](API-Entity-Flags.md) - Entity flag constants
- [Entity Events API](API-Entity-Events.md) - Entity event constants

---

**Key Takeaway:** Sharedgame ensures client and server speak the same language, critical for prediction, networking, and consistency.
