# Entity System Overview

Q2RTXPerimental uses a modern C++ object-oriented entity system that builds upon Quake 2's entity foundation while adding type safety, inheritance, and enhanced features.

## Entity Architecture

### The Entity Hierarchy

All entities inherit from `svg_base_edict_t`:

```
svg_base_edict_t (Base class)
├── svg_worldspawn_edict_t (World entity)
├── svg_player_edict_t (Player entities)
├── svg_item_edict_t (Pickups: weapons, ammo, health, etc.)
├── svg_pushmove_edict_t (Moving brush entities)
│   ├── svg_func_door_t
│   ├── svg_func_plat_t
│   ├── svg_func_train_t
│   ├── svg_func_rotating_t
│   └── svg_func_button_t
├── Trigger Entities
│   ├── svg_trigger_multiple_t
│   ├── svg_trigger_once_t
│   ├── svg_trigger_hurt_t
│   ├── svg_trigger_push_t
│   └── svg_trigger_teleport_t
├── Monster Entities
│   ├── svg_monster_soldier_t
│   ├── svg_monster_tank_t
│   └── ...
├── Misc Entities
│   ├── svg_misc_teleporter_t
│   ├── svg_info_player_start_t
│   └── ...
└── Target Entities
    ├── svg_target_speaker_t
    ├── svg_target_explosion_t
    └── ...
```

### Why Object-Oriented?

**Vanilla Quake 2 Approach (C):**
```c
edict_t *ent = G_Spawn();
ent->classname = "func_door";
ent->think = door_think;
ent->use = door_use;
ent->blocked = door_blocked;
// Manual setup of all fields
```

**Q2RTXPerimental Approach (C++):**
```cpp
auto *door = SVG_Spawn<svg_func_door_t>();
door->Spawn();  // Virtual method handles all setup
// Think, Use, Blocked are virtual methods
```

**Benefits:**
- **Type Safety**: Compile-time checks prevent errors
- **Code Reuse**: Inherit common behavior from base classes
- **Maintainability**: Related code grouped in classes
- **Encapsulation**: Entity data and behavior together
- **Polymorphism**: Virtual methods allow customization

## The svg_base_edict_t Class

### Key Components

```cpp
class svg_base_edict_t {
public:
    // Networked state (sent to clients)
    entity_state_t s;
    
    // Server-only data
    bool inuse;                 // Is this entity active?
    int linkcount;              // For PVS updates
    
    // Physics properties
    int movetype;               // How entity moves
    int solid;                  // Collision type
    vec3_t mins, maxs;          // Bounding box
    vec3_t velocity;            // Movement speed
    vec3_t avelocity;           // Angular velocity
    
    // Entity relationships
    svg_base_edict_t *groundentity;  // What we're standing on
    svg_base_edict_t *enemy;         // AI: current target
    svg_base_edict_t *activator;     // Who triggered us
    
    // Game properties
    const char *classname;      // Entity type name
    int spawnflags;             // Flags from map editor
    int health;                 // Hit points
    int max_health;
    int takedamage;             // Can be damaged?
    int flags;                  // FL_* flags
    
    // Timing
    gametime_t nextThinkTime;   // When to call Think()
    
    // Spawn parameters
    cm_entity_t *entityDictionary;  // Key-value pairs from map
    
    // Virtual callback methods
    virtual void Spawn();
    virtual void Think();
    virtual void Touch(svg_base_edict_t *other, cm_plane_t *plane, cm_surface_t *surf);
    virtual void Use(svg_base_edict_t *other, svg_base_edict_t *activator, 
                     entity_usetarget_type_t useType, int32_t useValue);
    virtual void Pain(svg_base_edict_t *attacker, float kick, int damage);
    virtual void Die(svg_base_edict_t *inflictor, svg_base_edict_t *attacker, 
                     int damage, vec3_t point);
    virtual void Blocked(svg_base_edict_t *other);
    
    // Save/load support
    virtual const svg_save_descriptor_t *GetSaveDescriptor();
    
    // Signal I/O
    void EmitSignal(const char *signalName, ...);
    void RegisterSignalHandler(const char *signalName, SignalCallback callback);
    
    // UseTargets support
    virtual UseTargetHint GetUseTargetHint();
    virtual void OnUseTargetPressed(svg_base_edict_t *user);
    virtual void OnUseTargetReleased(svg_base_edict_t *user);
};
```

### Entity State (entity_state_t)

The `s` field contains data sent to clients:

```cpp
typedef struct entity_state_s {
    int     number;             // Entity index
    
    vec3_t  origin;             // Position
    vec3_t  angles;             // Rotation (pitch, yaw, roll)
    vec3_t  old_origin;         // Previous position (for interpolation)
    
    int     modelindex;         // Model to render
    int     modelindex2, modelindex3, modelindex4;  // Additional models
    
    int     frame;              // Animation frame
    int     skinnum;            // Skin/texture variant
    
    uint32_t effects;           // EF_* rendering effects
    int     renderfx;           // RDF_* rendering flags
    int     solid;              // Bounding box (for prediction)
    
    int     sound;              // Looping sound index
    int     event;              // One-time event (EV_*)
} entity_state_t;
```

**Important:** Only data in `entity_state_t` is visible to clients. Server-only data like `health` or `enemy` is not sent.

## Entity Lifecycle

### 1. Allocation

Entities are allocated from a pool:

```cpp
template<typename T>
T* SVG_Spawn() {
    // Find free entity slot
    for (int i = maxclients + 1; i < globals.num_edicts; i++) {
        svg_base_edict_t *ent = GetEntityByIndex(i);
        if (!ent->inuse) {
            // Construct entity of type T in this slot
            ent = new (ent) T();  // Placement new
            ent->inuse = true;
            ent->edict = &g_edicts[i];  // Link to edict_t
            return static_cast<T*>(ent);
        }
    }
    
    // No free slots
    gi.error("SVG_Spawn: no free edicts");
    return nullptr;
}
```

### 2. Spawning

When a map loads or entity is created:

```cpp
void svg_func_door_t::Spawn() {
    // Call base class first
    svg_base_edict_t::Spawn();
    
    // Set entity type
    s.entityType = ET_PUSHER;
    
    // Set up physics
    solid = SOLID_BSP;
    movetype = MOVETYPE_PUSH;
    
    // Load model from BSP
    gi.SetModel(edict, entityDictionary->model);
    
    // Read spawn parameters
    speed = GetEntityDictValue<float>("speed", 100.0f);
    wait = GetEntityDictValue<float>("wait", 3.0f);
    dmg = GetEntityDictValue<int>("dmg", 2);
    
    // Calculate movement
    CalculateMoveDistance();
    
    // Set up callbacks
    SetUseCallback(&svg_func_door_t::DoorUse);
    SetBlockedCallback(&svg_func_door_t::DoorBlocked);
    SetThinkCallback(&svg_func_door_t::DoorThink);
    
    // Link into world (makes it solid and visible)
    gi.LinkEntity(edict);
}
```

### 3. Thinking

Entities perform actions periodically:

```cpp
void svg_misc_strobelight_t::Think() {
    // Toggle light effect
    if (s.effects & EF_ANIM01) {
        s.effects &= ~EF_ANIM01;  // Off
    } else {
        s.effects |= EF_ANIM01;   // On
    }
    
    // Schedule next think (0.5 seconds)
    nextThinkTime = level.time + gametime_t::from_sec(0.5f);
}
```

The engine calls think when `nextThinkTime` is reached:

```cpp
void SVG_RunEntities() {
    for (int i = 1; i < globals.num_edicts; i++) {
        svg_base_edict_t *ent = GetEntityByIndex(i);
        if (!ent->inuse)
            continue;
            
        if (ent->nextThinkTime && ent->nextThinkTime <= level.time) {
            ent->DispatchThinkCallback();  // Calls virtual Think()
        }
    }
}
```

### 4. Touching

When entities collide:

```cpp
void svg_item_health_t::Touch(svg_base_edict_t *other, cm_plane_t *plane, cm_surface_t *surf) {
    // Only players can pick up
    if (!other->client)
        return;
        
    // Already at max health?
    if (other->health >= other->max_health)
        return;
        
    // Heal player
    other->health = std::min(other->health + healAmount, other->max_health);
    
    // Play pickup sound
    gi.sound(other, CHAN_ITEM, gi.soundindex("items/health.wav"), 1, ATTN_NORM, 0);
    
    // Respawn after delay
    SetRespawn(30.0f);
}
```

### 5. Using (Interaction)

When triggered by another entity or player:

```cpp
void svg_func_button_t::Use(svg_base_edict_t *other, svg_base_edict_t *activator,
                            entity_usetarget_type_t useType, int32_t useValue) {
    if (state == STATE_PRESSED)
        return;  // Already pressed
        
    // Start pressing
    state = STATE_PRESSED;
    
    // Play sound
    if (sounds)
        gi.sound(edict, CHAN_VOICE, sounds, 1, ATTN_NORM, 0);
        
    // Trigger targets
    UseTargets(activator);
    
    // Reset after delay
    SetThinkCallback(&svg_func_button_t::ButtonReturn);
    nextThinkTime = level.time + gametime_t::from_sec(wait);
}
```

### 6. Taking Damage

When damaged:

```cpp
void svg_monster_soldier_t::Pain(svg_base_edict_t *attacker, float kick, int damage) {
    // Play pain sound
    gi.sound(edict, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);
    
    // Pain animation
    s.frame = FRAME_pain01;
    
    // Notice attacker if we weren't already fighting
    if (!enemy) {
        enemy = attacker;
        FoundTarget();
    }
}

void svg_monster_soldier_t::Die(svg_base_edict_t *inflictor, svg_base_edict_t *attacker,
                                 int damage, vec3_t point) {
    // Play death sound
    gi.sound(edict, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
    
    // Gib if overkill damage
    if (health < gib_health) {
        ThrowGibs();
        G_FreeEdict(this);
        return;
    }
    
    // Death animation
    s.frame = FRAME_death01;
    
    // Become a corpse
    takedamage = DAMAGE_NO;
    solid = SOLID_NOT;
    movetype = MOVETYPE_TOSS;
    
    // Remove after 30 seconds
    SetThinkCallback(&svg_monster_soldier_t::RemoveCorpse);
    nextThinkTime = level.time + gametime_t::from_sec(30.0f);
}
```

### 7. Freeing

When entity is no longer needed:

```cpp
void SVG_FreeEntity(svg_base_edict_t *ent) {
    // Unlink from world
    gi.UnlinkEntity(ent->edict);
    
    // Call destructor (virtual)
    ent->~svg_base_edict_t();
    
    // Mark slot as free
    ent->inuse = false;
    ent->freetime = level.time;
    ent->classname = "freed";
}
```

## Entity Registration System

### Declaring an Entity Type

```cpp
// In header file: svg_func_door.h
class svg_func_door_t : public svg_pushmove_edict_t {
public:
    DefineClass(
        "func_door",                    // Classname (used in maps)
        svg_func_door_t,                // Class type
        svg_pushmove_edict_t,           // Parent class
        EdictTypeInfo::TypeInfoFlag_GameSpawn  // Flags
    );
    
    void Spawn() override;
    void Use(/*...*/) override;
    // ... other virtual methods ...
};

// Register with spawn system
DECLARE_EDICT_SPAWN_INFO(func_door, svg_func_door_t);
```

```cpp
// In source file: svg_func_door.cpp
DEFINE_EDICT_SPAWN_INFO(func_door, svg_func_door_t);
```

This registration allows:
- Spawning from BSP entity strings (`classname "func_door"`)
- Runtime type identification
- Save/load system support
- Lua binding generation

### Spawning Registered Entities

```cpp
// From BSP entity string
// The engine finds "func_door" and calls:
svg_func_door_t *door = SVG_Spawn<svg_func_door_t>();
door->entityDictionary = ed;  // Spawn parameters
door->Spawn();

// Programmatic spawning
auto *explosion = SVG_Spawn<svg_misc_explosion_t>();
explosion->s.origin = explosionPos;
explosion->Spawn();
```

## Common Entity Patterns

### Time-Based Actions

```cpp
class svg_example_t : public svg_base_edict_t {
    void Spawn() override {
        // Start timer
        SetThinkCallback(&svg_example_t::DelayedAction);
        nextThinkTime = level.time + gametime_t::from_sec(5.0f);
    }
    
    void DelayedAction() {
        // Do something 5 seconds after spawn
        gi.dprintf("5 seconds passed!\n");
    }
};
```

### State Machines

```cpp
enum DoorState {
    STATE_CLOSED,
    STATE_OPENING,
    STATE_OPEN,
    STATE_CLOSING
};

class svg_func_door_t : public svg_base_edict_t {
    DoorState state = STATE_CLOSED;
    
    void Use(/*...*/) override {
        switch (state) {
            case STATE_CLOSED:
                StartOpening();
                break;
            case STATE_OPEN:
                StartClosing();
                break;
            // Ignore if moving
        }
    }
    
    void StartOpening() {
        state = STATE_OPENING;
        SetThinkCallback(&svg_func_door_t::FinishOpening);
        nextThinkTime = level.time + openDuration;
    }
    
    void FinishOpening() {
        state = STATE_OPEN;
        SetThinkCallback(&svg_func_door_t::StartClosing);
        nextThinkTime = level.time + gametime_t::from_sec(wait);
    }
};
```

### Multi-Part Entities

```cpp
// Train with multiple cars
class svg_func_train_t : public svg_base_edict_t {
    void Spawn() override {
        // This is the master
        teammaster = this;
        
        // Find and link all train cars
        svg_base_edict_t *part = nullptr;
        while ((part = SVG_Find(part, "targetname", target)) != nullptr) {
            part->teammaster = this;
            part->teamchain = teamchain;
            teamchain = part;
        }
    }
    
    void Think() override {
        // Move master
        s.origin += velocity * FRAMETIME.count();
        
        // Move all chained parts
        for (auto *part = teamchain; part; part = part->teamchain) {
            part->s.origin = s.origin + part->s.old_origin;  // Relative offset
        }
    }
};
```

## Performance Considerations

### Entity Pool Limits

```cpp
#define MAX_EDICTS 1024  // Default limit

// First N slots reserved for clients
// Remaining slots for game entities
```

**Tips:**
- Reuse entities when possible (e.g., gibs, temp particles)
- Free entities when done (corpses after 30s)
- Use temporary entities for short-lived effects

### Think Optimization

Don't think every frame unless necessary:

```cpp
// BAD: Thinks every frame
void Think() {
    CheckSomething();
    nextThinkTime = level.time + FRAMETIME;  // 25ms
}

// GOOD: Thinks only when needed
void Think() {
    CheckSomething();
    nextThinkTime = level.time + gametime_t::from_sec(1.0f);  // 1 second
}
```

### Collision Optimization

```cpp
// Set appropriate solid type
solid = SOLID_NOT;      // No collision (decorative)
solid = SOLID_TRIGGER;  // Touch only, doesn't block
solid = SOLID_BBOX;     // Bounding box collision
solid = SOLID_BSP;      // Expensive, use for brush entities only
```

## Debugging Entities

### Console Commands

```
give <itemname>         Give item to player
spawn <classname>       Spawn entity at player location
kill <classname>        Remove all entities of type
ent_list                List all active entities
ent_info <index>        Show entity details
ent_remove <index>      Remove specific entity
```

### Debug Visualization

```cpp
// Draw bounding box
void svg_base_edict_t::DebugDraw() {
    gi.debug_draw_box(s.origin + mins, s.origin + maxs, {1, 0, 0});
}

// Draw think time
if (developer->value) {
    float timeLeft = (nextThinkTime - level.time).count();
    gi.dprintf("Entity %d thinks in %.2fs\n", s.number, timeLeft);
}
```

## Next Steps

- [Entity Base Class Reference](Entity-Base-Class-Reference.md) - Complete API documentation
- [Creating Custom Entities](Creating-Custom-entities.md) - Step-by-step tutorial
- [Entity Lifecycle](Entity-Lifecycle.md) - Detailed lifecycle management
- [Entity Networking](Entity-Networking.md) - How entities sync to clients
- [Entity Save/Load](Entity-Save-Load.md) - Persistence system

---

**Previous:** [Architecture Overview](Architecture-Overview.md)  
**Next:** [Creating Custom Entities](Creating-Custom-Entities.md)
