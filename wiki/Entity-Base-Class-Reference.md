# Entity Base Class Reference - svg_base_edict_t

Complete reference for `svg_base_edict_t`, the base class for all entities in Q2RTXPerimental. This is the most important class in the game code - every entity in the game world derives from it.

**Location:** `src/baseq2rtxp/svgame/entities/svg_base_edict.h` and `.cpp`

## Class Overview

`svg_base_edict_t` provides the foundation for all game entities. It inherits from `sv_shared_edict_t` and adds game-specific functionality including:

- **Entity state management** - Networked state synchronized to clients
- **Physics properties** - Movement, collision, and simulation
- **Callback system** - Virtual methods for entity behavior
- **Save/load support** - Persistence for savegames
- **Target system** - Entity targeting and activation
- **Signal I/O** - Advanced entity communication
- **Type information** - Runtime type identification and spawning

```cpp
struct svg_base_edict_t : public sv_shared_edict_t<svg_base_edict_t, svg_client_t> {
    // Base class for all game entities
    // Provides comprehensive entity functionality
    // Over 1000+ lines of member variables and methods
};
```

### Inheritance Hierarchy

```
sv_shared_edict_t<T, ClientType>
    ↓
svg_base_edict_t (Base for all game entities)
    ↓
    ├── svg_worldspawn_edict_t (World entity)
    ├── svg_player_edict_t (Players)
    ├── svg_monster_base_t (Monsters)
    │   ├── svg_monster_soldier_t
    │   ├── svg_monster_tank_t
    │   └── ...
    ├── svg_item_edict_t (Items)
    ├── svg_pushmove_edict_t (Moving brushes)
    │   ├── svg_func_door_t
    │   ├── svg_func_plat_t
    │   └── ...
    └── ... (hundreds more entity types)
```

## Key Member Variables

### 1. Networked State (entity_state_t s)

The `entity_state_t s` member contains all data that is **synchronized to clients** every frame. This is the ONLY data clients see.

```cpp
entity_state_t s;  // Sent to clients via delta compression
```

#### Critical entity_state_t Fields

**Position and Orientation:**
```cpp
s.origin          // vec3_t: World position (x, y, z in units)
s.angles          // vec3_t: Orientation (pitch, yaw, roll in degrees)
s.old_origin      // vec3_t: Previous position (for interpolation/trails)
```

**Visual Representation:**
```cpp
s.modelindex      // int: Primary model index (0 = invisible)
s.modelindex2     // int: Weapon/secondary model
s.modelindex3     // int: Third model (rare)
s.modelindex4     // int: Fourth model (very rare)
s.skinnum         // int: Skin/texture index
s.frame           // int: Animation frame number (0-198 typically)
```

**Visual Effects:**
```cpp
s.effects         // int: Visual effect flags
                  // EF_ROTATE      - Rotate slowly
                  // EF_GIB         - Leave blood trail
                  // EF_BLASTER     - Hyperblaster particle effect
                  // EF_ROCKET      // Rocket trail
                  // EF_GRENADE     // Grenade trail
                  // EF_HYPERBLASTER // Hyperblaster effect
                  // EF_BFG         // BFG effect
                  // EF_COLOR_SHELL // Colored shell
                  // EF_POWERSCREEN // Power screen glow
                  // EF_ANIM01-ANIM23 // Run frames 0-23 then stop
                  // EF_ANIM_ALL    // Run all frames cyclically
                  // EF_ANIM_ALLFAST // Run all frames fast
                  // EF_FLIES       // Attract flies
                  // EF_QUAD        // Quad damage shell
                  // EF_PENT        // Invulnerability shell
                  // EF_TELEPORTER  // Teleporter glow
                  // EF_FLAG1       // Red flag
                  // EF_FLAG2       // Blue flag
                  // EF_IONRIPPER   // Ion ripper effect
                  // EF_GREENGIB    // Green gib
                  // EF_BLUEHYPERBLASTER // Blue hyperblaster
                  // EF_SPHERETRANS // Sphere of transparency
                  // EF_TRACKERTRAIL // Tracker trail
                  // EF_TRACKER     // Tracker effect
                  // EF_DOUBLE      // Double damage
                  // EF_HALF_DAMAGE // Half damage

s.renderfx        // int: Rendering flags
                  // RF_MINLIGHT        - Always have some light
                  // RF_VIEWERMODEL     - Don't draw from own POV
                  // RF_WEAPONMODEL     - Draw as weapon
                  // RF_FULLBRIGHT      - Always full intensity
                  // RF_DEPTHHACK       - For view weapon
                  // RF_TRANSLUCENT     // 0.66 alpha
                  // RF_FRAMELERP       // Lerp between frames
                  // RF_BEAM            // Beam rendering
                  // RF_CUSTOMSKIN      // Custom skin
                  // RF_GLOW            // Pulse glow
                  // RF_SHELL_RED       // Red shell
                  // RF_SHELL_GREEN     // Green shell
                  // RF_SHELL_BLUE      // Blue shell
                  // RF_SHELL_DOUBLE    // Double damage shell
                  // RF_SHELL_HALF_DAM  // Half damage shell
                  // RF_NOSHADOW        // Don't cast shadow
                  // RF_FLARE           // Lens flare
```

**Entity Identification:**
```cpp
s.number          // int: Entity index (0-MAX_EDICTS)
s.entityType      // int: Entity type for rendering
                  // ET_GENERAL      - Generic entity
                  // ET_PLAYER       - Player entity
                  // ET_PLAYER_CORPSE - Dead player
                  // ET_MONSTER      - Monster/NPC
                  // ET_MONSTER_CORPSE - Dead monster
                  // ET_PUSHER       - Moving brush
                  // ET_ITEM         - Pickup item
                  // ET_GIB          - Gib/body part
                  // ET_TARGET_SPEAKER - Speaker
                  // ET_PUSH_TRIGGER - Push trigger
                  // ET_TELEPORT_TRIGGER - Teleport trigger
                  // ET_TEMP_ENTITY_EVENT - Temp entity
```

**Audio and Events:**
```cpp
s.sound           // int: Looping sound index
s.event           // int: One-shot entity event (footsteps, gunshots, etc.)
                  // Top 2 bits increment to detect repeated events
                  // Lower bits are actual event type
```

**Collision (for prediction):**
```cpp
s.solid           // int: Bounding box encoding (for client prediction)
                  // Encodes mins/maxs for client-side collision
```

**Example: Setting entity state for a monster**
```cpp
void svg_monster_soldier_t::Spawn() {
    // Set visual representation
    s.modelindex = gi.modelindex("models/monsters/soldier/tris.md2");
    s.skinnum = 0;
    s.frame = 0;
    
    // Set position
    s.origin = spawn_origin;
    s.angles = spawn_angles;
    
    // Set entity type for proper client rendering
    s.entityType = ET_MONSTER;
    
    // Set visual effects
    s.effects = 0;  // No special effects
    s.renderfx = RF_FRAMELERP;  // Smooth animation interpolation
    
    // Set collision for client prediction
    s.solid = SOLID_BBOX;  // Bounding box collision
    
    // Entity index assigned by engine
    // s.number already set
}

### 2. Server-Only Core Data

These fields exist **only on the server** and are never sent to clients:

```cpp
bool inuse;                  // Is this entity slot active?
int linkcount;               // Incremented each time entity is linked
                            // Used to validate ground entity pointers
const char *classname;       // Entity class name ("monster_soldier", "weapon_shotgun")
int spawnflags;              // Flags from map editor (bit field)
const cm_entity_t *entityDictionary;  // Parsed map entity key/value pairs
```

**inuse Flag:**
The `inuse` flag determines if an entity slot is occupied:

```cpp
// Check if entity is active
if (!entity->inuse) {
    return;  // Entity slot is free
}

// Free an entity
entity->inuse = false;  // Marks slot as available
```

**linkcount:**
Used to validate entity pointers (prevents stale pointer bugs):

```cpp
// Store reference to ground entity
groundentity = floor_entity;
groundentity_linkcount = floor_entity->linkcount;

// Later, check if reference is still valid
if (groundentity && groundentity->linkcount == groundentity_linkcount) {
    // Pointer is still valid
} else {
    // Entity was freed and reallocated, clear reference
    groundentity = nullptr;
}
```

**classname:**
String identifier for entity type:

```cpp
classname = "monster_soldier";   // Monster
classname = "weapon_shotgun";    // Weapon
classname = "func_door";         // Door
classname = "info_player_start"; // Spawn point
```

Used for:
- Debugging/logging
- Entity searching (`SVG_Find`)
- Map editor compatibility
- Type identification

**spawnflags:**
Bit field of flags set in map editor:

```cpp
// Universal spawn flags (all entities)
SPAWNFLAG_NOT_DEATHMATCH (bit 19) // Don't spawn in deathmatch
SPAWNFLAG_NOT_COOP (bit 20)       // Don't spawn in coop
SPAWNFLAG_NOT_EASY (bit 21)       // Don't spawn on easy
SPAWNFLAG_NOT_MEDIUM (bit 22)     // Don't spawn on medium
SPAWNFLAG_NOT_HARD (bit 23)       // Don't spawn on hard

// Entity-specific flags (bits 0-18, 24-31)
// Example for func_door:
SPAWNFLAG_START_OPEN (bit 0)      // Door starts open
SPAWNFLAG_REVERSE (bit 1)         // Reverse move direction
SPAWNFLAG_CRUSHER (bit 2)         // Crushes entities
SPAWNFLAG_TOGGLE (bit 5)          // Wait for retrigger
```

**Example: Checking spawn flags**
```cpp
void svg_monster_soldier_t::Spawn() {
    // Check difficulty
    int skill = (int)g_skill->value;
    if (skill == 0 && (spawnflags & SPAWNFLAG_NOT_EASY)) {
        SVG_FreeEdict(this);  // Don't spawn on easy
        return;
    }
    
    // Check game mode
    if (game.deathmatch && (spawnflags & SPAWNFLAG_NOT_DEATHMATCH)) {
        SVG_FreeEdict(this);  // Don't spawn in deathmatch
        return;
    }
    
    // Continue spawning...
}
```

### 3. Physics Properties

Complete physics simulation parameters:

**Movement Type:**
```cpp
int movetype;                // How entity moves

// Movement types:
MOVETYPE_NONE       // No movement (static)
MOVETYPE_NOCLIP     // No collision (flies through walls)
MOVETYPE_PUSH       // Push other entities (trains, plats)
MOVETYPE_STOP       // Push until hitting obstacle
MOVETYPE_WALK       // Walking (ground movement with gravity)
MOVETYPE_STEP       // Stepping (monsters, players with stairs)
MOVETYPE_FLY        // Flying (full 3D movement with gravity)
MOVETYPE_TOSS       // Tossed physics (affected by gravity, bounces)
MOVETYPE_FLYMISSILE // Flying missile (not affected by gravity)
MOVETYPE_BOUNCE     // Bounces off surfaces (grenades)
MOVETYPE_WALLBOUNCE // Bounces, but without gravity (plasma)
```

**Example: Setting movement type**
```cpp
// Static entity (light, decorative object)
movetype = MOVETYPE_NONE;

// Projectile (rocket, grenade)
movetype = MOVETYPE_BOUNCE;  // Bounces and falls

// Monster (walks on ground, climbs stairs)
movetype = MOVETYPE_STEP;

// Flying monster (flies in 3D space)
movetype = MOVETYPE_FLY;

// Physics item (health pack bounces when spawned)
movetype = MOVETYPE_TOSS;
```

**Collision Type:**
```cpp
int solid;                   // Collision behavior

// Collision types:
SOLID_NOT        // No collision (pass through everything)
SOLID_TRIGGER    // Touch trigger (calls Touch callback, no blocking)
SOLID_BBOX       // Bounding box collision (blocks movement)
SOLID_BSP        // BSP model collision (func_door, func_wall)
```

**Bounding Box:**
```cpp
vec3_t mins, maxs;          // Bounding box (relative to origin)
vec3_t absmin, absmax;      // Absolute bounds in world space (calculated)
vec3_t size;                // maxs - mins (calculated)

// Example: Player bounding box
mins = {-16, -16, -24};     // 32x32 base, 32 units tall
maxs = {16, 16, 32};

// Example: Monster soldier
mins = {-16, -16, -24};
maxs = {16, 16, 32};

// Example: Health pack
mins = {-16, -16, 0};
maxs = {16, 16, 16};

// Example: Large monster (tank)
mins = {-32, -32, -16};
maxs = {32, 32, 72};
```

**Velocity and Acceleration:**
```cpp
vec3_t velocity;            // Linear velocity (units/second)
vec3_t avelocity;           // Angular velocity (degrees/second)
float mass;                 // Mass for physics (typically 200)
float gravity;              // Gravity multiplier (1.0 = normal, 0.0 = none)

// Example: Toss a grenade
velocity = forward * 600;   // Throw forward at 600 units/sec
velocity[2] = 200;          // Add upward velocity
avelocity = {0, 300, 0};    // Spin around yaw axis

// Example: Flying monster
velocity = direction * 250; // Fly at 250 units/sec
avelocity = vec3_zero;      // No rotation

// Example: Rocket
velocity = forward * 650;   // Fast projectile
avelocity = vec3_zero;      // Doesn't spin
movetype = MOVETYPE_FLYMISSILE;  // Not affected by gravity
```

**Ground Entity:**
```cpp
svg_base_edict_t *groundentity;      // Entity we're standing on
int groundentity_linkcount;          // Validation for groundentity

// Example: Check if on ground
if (groundentity) {
    // On ground (can jump)
} else {
    // In air (can't jump until landing)
}

// Example: Standing on moving platform
if (groundentity && groundentity->movetype == MOVETYPE_PUSH) {
    // Move with platform
    s.origin += groundentity->velocity * FRAMETIME;
}
```

**Clip Mask:**
```cpp
int clipmask;               // What to collide with (bit mask)

// Clip masks:
MASK_ALL           // Everything
MASK_SOLID         // Solid objects (walls, floors)
MASK_PLAYERSOLID   // Solid + monsters
MASK_DEADSOLID     // Solid + players (for corpses)
MASK_MONSTERSOLID  // Solid + players + monsters
MASK_WATER         // Water volumes
MASK_OPAQUE        // Blocks line of sight
MASK_SHOT          // Hitscan weapons
MASK_CURRENT       // Water currents

// Example: Trace for player
trace_t tr = gi.trace(start, mins, maxs, end, 
                      player, MASK_PLAYERSOLID);

// Example: Trace for monster
trace_t tr = gi.trace(start, mins, maxs, end,
                      monster, MASK_MONSTERSOLID);

// Example: Check line of sight
trace_t tr = gi.trace(eye_pos, NULL, NULL, target_pos,
                      self, MASK_OPAQUE);
if (tr.fraction == 1.0f) {
    // Clear line of sight
}
```

**Waterlevel and Watertype:**
```cpp
int waterlevel;             // 0=not in water, 1=feet, 2=waist, 3=head
int watertype;              // Type of liquid (CONTENTS_*)

// Water types:
CONTENTS_WATER     // Regular water
CONTENTS_SLIME     // Toxic slime (damages)
CONTENTS_LAVA      // Lava (heavy damage)

// Example: Check drowning
if (waterlevel == 3) {  // Head underwater
    if (air_finished < level.time) {
        // Drown damage
        health -= 2;
        if (health <= 0) {
            T_Damage(this, world, world, vec3_zero(), s.origin,
                     vec3_zero(), 15, 0, DAMAGE_NO_ARMOR,
                     MEANS_OF_DEATH_WATER);
        }
    }
}

// Example: Swimming speed
if (waterlevel >= 2) {
    // Slow movement in water
    velocity *= 0.5f;
}

// Example: Slime damage
if (waterlevel && (watertype & CONTENTS_SLIME)) {
    T_Damage(this, world, world, vec3_zero(), s.origin,
             vec3_zero(), 4, 0, 0, MEANS_OF_DEATH_SLIME);
}
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
