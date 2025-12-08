# Entity Base Class Reference - svg_base_edict_t

Complete reference for `svg_base_edict_t`, the base class for all entities in Q2RTXPerimental.

**Location:** `src/baseq2rtxp/svgame/entities/svg_base_edict.h` and `.cpp`

## Class Overview

`svg_base_edict_t` provides the foundation for all game entities. It inherits from `sv_shared_edict_t` and adds game-specific functionality.

```cpp
struct svg_base_edict_t : public sv_shared_edict_t<svg_base_edict_t, svg_client_t> {
    // Base class for all game entities
};
```

## Key Member Variables

### Networked State

```cpp
entity_state_t s;  // Sent to clients every frame
```

**Important fields:**
- `s.origin` - Entity position in world
- `s.angles` - Entity orientation
- `s.modelindex` - Model to render
- `s.frame` - Animation frame
- `s.entityType` - Entity type (ET_GENERAL, ET_PLAYER, etc.)
- `s.event` - Entity event for client effects
- `s.effects` - Visual effects flags (EF_ROTATE, EF_GIB, etc.)
- `s.renderfx` - Rendering flags (RF_SHELL_RED, RF_GLOW, etc.)

### Server-Only Data

```cpp
bool inuse;                  // Is entity slot active?
int linkcount;               // For PVS updates
const char *classname;       // Entity class name ("monster_soldier")
int spawnflags;              // Flags from map editor
```

### Physics Properties

```cpp
int movetype;                // MOVETYPE_NONE, MOVETYPE_WALK, etc.
int solid;                   // SOLID_NOT, SOLID_BBOX, SOLID_BSP
vec3_t mins, maxs;          // Bounding box
vec3_t absmin, absmax;      // Absolute bounding box (world space)
vec3_t size;                // maxs - mins
vec3_t velocity;            // Current velocity
vec3_t avelocity;           // Angular velocity
float mass;                 // For physics
```

### Game Properties

```cpp
int health;                 // Hit points
int max_health;             // Maximum health
int takedamage;             // DAMAGE_NO, DAMAGE_YES, DAMAGE_AIM
int flags;                  // FL_FLY, FL_SWIM, FL_GODMODE, etc.
int svflags;                // SVF_MONSTER, SVF_NOCLIENT, etc.
```

### Entity Relationships

```cpp
svg_base_edict_t *groundentity;    // What entity is standing on
svg_base_edict_t *enemy;           // AI: current target
svg_base_edict_t *movetarget;      // AI: movement target
svg_base_edict_t *activator;       // Who activated this entity
svg_base_edict_t *owner;           // Who owns this entity (projectiles)
```

### Timing

```cpp
float nextthink;            // Next time to call Think()
float timestamp;            // General purpose timestamp
```

### Target System

```cpp
const char *target;         // Target name to activate
const char *targetname;     // This entity's name
const char *killtarget;     // Entity to kill on activation
const char *message;        // Message to display
```

## Virtual Callback Methods

All callbacks are virtual and can be overridden in derived classes.

### Spawn Callbacks

```cpp
virtual void PreSpawn();     // Before main spawn
virtual void Spawn();        // Main spawn logic
virtual void PostSpawn();    // After spawn finalization
```

### Think Callbacks

```cpp
virtual void PreThink();     // Before main think
virtual void Think();        // Main logic update
virtual void PostThink();    // After think
```

### Interaction Callbacks

```cpp
// Entity collision
virtual void Touch(svg_base_edict_t *other, 
                  const cm_plane_t *plane, 
                  cm_surface_t *surf);

// Player uses entity
virtual void Use(svg_base_edict_t *other, 
                svg_base_edict_t *activator,
                entity_usetarget_type_t useType,
                int32_t useValue);

// Moving entity blocked
virtual void Blocked(svg_base_edict_t *other);

// Entity takes damage
virtual void Pain(svg_base_edict_t *other, 
                 float kick, 
                 int32_t damage,
                 entity_damageflags_t damageFlags);

// Entity dies
virtual void Die(svg_base_edict_t *inflictor, 
                svg_base_edict_t *attacker, 
                int32_t damage, 
                Vector3 *point);
```

### Signal System Callback

```cpp
virtual void OnSignalIn(svg_base_edict_t *other, 
                        svg_base_edict_t *activator,
                        const char *signalName,
                        const svg_signal_argument_array_t &args);
```

## Setting Callbacks

Use type-safe setter methods:

```cpp
void svg_monster_soldier_t::Spawn() {
    // Set callback methods (type-checked at compile time)
    SetThinkCallback(&svg_monster_soldier_t::AI_Think);
    SetTouchCallback(&svg_monster_soldier_t::Touch);
    SetPainCallback(&svg_monster_soldier_t::Pain);
    SetDieCallback(&svg_monster_soldier_t::Die);
}
```

## Entity Dictionary

```cpp
const cm_entity_t *entityDictionary;  // Parsed map entity data
```

Access map editor properties:

```cpp
void Spawn() {
    // Get custom key from map
    if (const char *value = ED_GetString(entityDictionary, "custom_key")) {
        // Use custom value
    }
}
```

## Save/Load System

```cpp
virtual void Save(game_write_context_t *ctx);
virtual void Load(game_read_context_t *ctx);
```

Implement for persistent entities:

```cpp
void svg_custom_entity_t::Save(game_write_context_t *ctx) {
    // Call parent save first
    svg_base_edict_t::Save(ctx);
    
    // Save custom data
    SVG_Save_Write(ctx, &custom_data, sizeof(custom_data));
}
```

## UseTargets System

```cpp
void UseTargets(svg_base_edict_t *other, 
                svg_base_edict_t *activator);
```

Activate target entities:

```cpp
void svg_func_button_t::Use(/*...*/) {
    // Fire all entities with matching targetname
    UseTargets(activator, activator);
}
```

## Signal I/O System

```cpp
void SendSignalOut(const char *signalName, 
                   svg_base_edict_t *activator,
                   const svg_signal_argument_array_t &args);
```

Send signals to other entities:

```cpp
void Think() {
    // Send signal with arguments
    svg_signal_argument_array_t args;
    args[0] = 42;  // Integer argument
    SendSignalOut("OnActivate", this, args);
}
```

## Common Patterns

### Basic Entity Spawn

```cpp
void svg_item_health_t::Spawn() {
    // Set model
    gi.SetModel(edict, "models/items/health/tris.md2");
    
    // Set size
    mins = {-16, -16, 0};
    maxs = {16, 16, 32};
    
    // Set physics
    movetype = MOVETYPE_TOSS;
    solid = SOLID_TRIGGER;
    
    // Set callbacks
    SetTouchCallback(&svg_item_health_t::Touch);
    
    // Link into world
    gi.linkentity(edict);
}
```

### Entity with Think Loop

```cpp
void svg_rotating_t::Spawn() {
    // ... setup ...
    
    // Start thinking
    SetThinkCallback(&svg_rotating_t::Think);
    nextthink = level.time + FRAMETIME;
}

void svg_rotating_t::Think() {
    // Rotate
    s.angles[YAW] += rotation_speed * FRAMETIME;
    
    // Link updated position
    gi.linkentity(edict);
    
    // Continue thinking
    nextthink = level.time + FRAMETIME;
}
```

### Entity with Target

```cpp
void svg_trigger_multiple_t::Touch(svg_base_edict_t *other, /*...*/) {
    // Check if player
    if (!other->client)
        return;
    
    // Fire targets
    UseTargets(other, other);
}
```

## Related Documentation

- [**Entity System Overview**](Entity-System-Overview.md) - Architecture
- [**Entity Lifecycle**](Entity-Lifecycle.md) - Spawn, think, die
- [**Creating Custom Entities**](Creating-Custom-Entities.md) - Tutorial
- [**Signal I/O System**](Signal-IO-System.md) - Entity communication
- [**UseTargets System**](UseTargets-System.md) - Target activation

## Summary

`svg_base_edict_t` is the foundation for all Q2RTXPerimental entities:

- **Networked state** (`entity_state_t s`) synchronized to clients
- **Virtual callbacks** for spawn, think, touch, use, pain, die
- **Physics properties** for movement and collision
- **Target system** for entity interactions
- **Signal I/O** for advanced communication
- **Save/load support** for game persistence

Derive from `svg_base_edict_t` and override virtual methods to create custom entities with unique behaviors.
