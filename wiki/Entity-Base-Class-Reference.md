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

### 4. Game Properties and Status

**Health and Damage:**
```cpp
int health;                 // Current hit points
int max_health;             // Maximum hit points (for respawning/healing)
int armor;                  // Current armor points
int max_armor;              // Maximum armor
int gib_health;             // Health threshold for gibbing (usually -40)
entity_lifestatus_t lifeStatus;  // ALIVE, DEAD, GIBBED
entity_takedamage_t takedamage;  // Can entity take damage?

// Take damage values:
DAMAGE_NO       // Cannot be damaged
DAMAGE_YES      // Can take damage normally
DAMAGE_AIM      // Auto-aim targets this entity

// Life status:
LIFESTATUS_ALIVE    // Entity is alive
LIFESTATUS_DEAD     // Entity is dead
LIFESTATUS_GIBBED   // Entity was gibbed

// Example: Setting up damageable entity
health = 100;
max_health = 100;
takedamage = DAMAGE_YES;
lifeStatus = LIFESTATUS_ALIVE;

// Example: Check if entity should gib
if (damage > 50 && health < gib_health) {
    // Heavy damage and below gib threshold
    ThrowGibs(GIB_ORGANIC);
    lifeStatus = LIFESTATUS_GIBBED;
}

// Example: Invulnerable entity
takedamage = DAMAGE_NO;  // Cannot be damaged (door, wall, etc.)
```

**Entity Flags:**
```cpp
entity_flags_t flags;       // Generic entity flags (bit field)

// Common entity flags:
FL_FLY                 // Entity can fly
FL_SWIM                // Entity can swim
FL_IMMUNE_LASER        // Immune to lasers
FL_INWATER             // Currently in water
FL_GODMODE             // Cannot take damage
FL_NOTARGET            // Monsters ignore this entity
FL_IMMUNE_SLIME        // Immune to slime damage
FL_IMMUNE_LAVA         // Immune to lava damage
FL_PARTIALGROUND       // Not all corners on ground
FL_WATERJUMP           // Player jumping out of water
FL_TEAMSLAVE           // Part of a team (train)
FL_NO_KNOCKBACK        // Not affected by knockback
FL_POWER_ARMOR         // Has power armor
FL_RESPAWN             // Can respawn
FL_MECHANICAL          // Mechanical (immune to drowning)

// Example: Flying monster
flags |= FL_FLY;
movetype = MOVETYPE_FLY;

// Example: Mechanical entity (immune to drowning)
flags |= FL_MECHANICAL;

// Example: God mode player
flags |= FL_GODMODE;
takedamage = DAMAGE_NO;

// Example: Check if entity can be targeted by AI
if (target->flags & FL_NOTARGET) {
    // Don't attack this entity
    return;
}
```

**Server Flags:**
```cpp
int svflags;                // Server-specific flags

// Server flags:
SVF_NOCLIENT           // Don't send to clients (invisible)
SVF_DEADMONSTER        // Dead monster, treat specially
SVF_MONSTER            // Monster AI entity
SVF_PROJECTILE         // Projectile (special collision)
SVF_HULL               // Hull check entity

// Example: Invisible entity (server-side only logic)
svflags |= SVF_NOCLIENT;  // Clients won't see it

// Example: Monster entity
svflags |= SVF_MONSTER;
flags |= FL_FLY;  // Flying monster

// Example: Projectile
svflags |= SVF_PROJECTILE;
movetype = MOVETYPE_FLYMISSILE;
```

### 5. Timing and Scheduling

**Think Timing:**
```cpp
float nextthink;            // Server time to call Think() (0 = don't think)
float timestamp;            // General-purpose timestamp
QMTime freetime;            // Time entity was freed
QMTime eventTime;           // Event expiration time

// Example: Schedule think for next frame
nextthink = level.time + FRAMETIME;  // FRAMETIME = 0.025s (40 Hz)

// Example: Schedule think in 2 seconds
nextthink = level.time + 2.0f;

// Example: Stop thinking
nextthink = 0;

// Example: Cooldown timer
if (level.time < timestamp) {
    return;  // Still on cooldown
}
// Do action...
timestamp = level.time + 1.0f;  // 1 second cooldown

// Example: Delayed respawn
void Die(/*...*/) {
    SetThinkCallback(&svg_item_health_t::Respawn);
    nextthink = level.time + 30.0f;  // Respawn after 30 seconds
}
```

**Respawn Timing:**
```cpp
float delay;                // Delay before action
float wait;                 // Wait time between actions
float random;               // Random time variance

// Example: Trigger with random delay
wait = 2.0f;     // 2 second base delay
random = 1.0f;   // ±1 second randomness
float actual_delay = wait + crandom() * random;  // 1-3 seconds

// Example: Respawning item
delay = 30.0f;   // 30 second respawn
nextthink = level.time + delay;
```

### 6. Target System

**Target Names:**
```cpp
const char *targetname;     // This entity's name (for targeting)
const char *target;         // Entity to activate
const char *killtarget;     // Entity to kill
const char *team;           // Team name (for grouped entities)
const char *pathtarget;     // Path to follow
const char *deathtarget;    // Activate on death

// Example: Button that opens a door
// Button:
target = "door1";           // Activate entity named "door1"

// Door:
targetname = "door1";       // This door is named "door1"

// Example: Kill target
// Trigger that removes monsters:
killtarget = "wave1_monsters";

// Monsters:
targetname = "wave1_monsters";  // All monsters with this name will be killed

// Example: Team of func_trains
// Train 1:
targetname = "train";
team = "train_team";

// Train 2:
targetname = "train";
team = "train_team";       // Moves together with train 1
```

**Messages and Sounds:**
```cpp
const char *message;        // Message to display
const char *map;            // Map to load
const char *music;          // Music to play
int sounds;                 // Sound set index

// Example: Trigger that displays message
message = "You found a secret!";

// Example: Level change
map = "base2";  // Next map filename

// Example: Ambient sound
sounds = 1;     // Sound set (water, wind, etc.)
```

### 7. Entity Relationships

**Entity Pointers:**
```cpp
svg_base_edict_t *groundentity;     // Entity standing on
svg_base_edict_t *enemy;            // Current target (AI)
svg_base_edict_t *oldenemy;         // Previous target
svg_base_edict_t *movetarget;       // Movement destination
svg_base_edict_t *goalentity;       // AI goal
svg_base_edict_t *activator;        // Who activated this entity
svg_base_edict_t *teamchain;        // Next entity in team
svg_base_edict_t *teammaster;       // Team leader
svg_base_edict_t *owner;            // Who owns this (projectiles)
svg_base_edict_t *mynoise;          // Sound entity 1
svg_base_edict_t *mynoise2;         // Sound entity 2

// Example: Projectile ownership
void FireRocket() {
    auto *rocket = SVG_Spawn<svg_rocket_t>();
    rocket->owner = player;         // Player owns this rocket
    rocket->s.origin = muzzle_pos;
    rocket->velocity = forward * 650;
    // Rocket won't hit its owner
}

// Example: Monster AI
void Think() {
    if (enemy && CanSee(enemy)) {
        // Attack current enemy
        AttackEnemy();
    } else if (oldenemy && CanSee(oldenemy)) {
        // Return to previous target
        enemy = oldenemy;
        oldenemy = nullptr;
    } else {
        // No target, wander
        Wander();
    }
}

// Example: Path following
void FollowPath() {
    if (movetarget) {
        // Move toward path target
        vec3_t dir = movetarget->s.origin - s.origin;
        VectorNormalize(dir);
        velocity = dir * speed;
        
        // Check if reached
        if (VectorDistance(s.origin, movetarget->s.origin) < 32) {
            // Find next path target
            movetarget = FindNextPathTarget();
        }
    }
}

// Example: Team movement
void MoveTeam() {
    if (teamchain) {
        // Move all team members together
        svg_base_edict_t *member = teamchain;
        while (member) {
            member->s.origin = s.origin + member->team_offset;
            gi.linkentity(member);
            member = member->teamchain;
        }
    }
}
```

**Validation of Entity Pointers:**
Always validate entity pointers before use:

```cpp
// WRONG: Unsafe - entity might be freed
if (enemy) {
    Attack(enemy);  // CRASH if enemy was freed!
}

// CORRECT: Validate with linkcount
if (enemy && enemy->linkcount == enemy_linkcount) {
    Attack(enemy);  // Safe
} else {
    enemy = nullptr;  // Clear stale pointer
}

// CORRECT: Check inuse flag
if (enemy && enemy->inuse) {
    Attack(enemy);  // Safe
} else {
    enemy = nullptr;
}
```

### 8. AI Properties (for monsters)

```cpp
int enemy_vis_cache;        // Visibility cache flags
float last_sound_time;      // Time last made sound
int pain_debounce_time;     // Prevent pain spam
int air_finished_time;      // Drowning countdown
float search_time;          // Time spent searching
float attack_finished_time; // Attack cooldown

// AI state
int aiflags;                // AI behavior flags
float idle_time;            // Time spent idle
float pausetime;            // Pause before action
float attack_state;         // Current attack phase
```

### 9. Player-Specific Properties (svg_player_edict_t)

```cpp
svg_client_t *client;       // Client data (if player)

// Client data includes:
// - Player stats (health, ammo, score)
// - Inventory
// - Weapon state
// - View angles
// - Persistent info (name, skin)
// - Network state

// Example: Check if entity is a player
if (entity->client) {
    // This is a player
    int score = entity->client->resp.score;
}
```

## Virtual Callback Methods

All callbacks are virtual and can be overridden in derived classes. The callback system is type-safe and uses templated setter methods.

### Lifecycle Callbacks

#### PreSpawn()
```cpp
virtual void PreSpawn();

// Called: Before main Spawn() method
// Purpose: Early initialization, parse entity dictionary
// Use for: Reading custom map keys, early validation
```

**Example:**
```cpp
void svg_custom_trigger_t::PreSpawn() {
    // Call parent first
    svg_trigger_multiple_t::PreSpawn();
    
    // Parse custom keys from map
    if (const char *value = ED_GetString(entityDictionary, "custom_delay")) {
        custom_delay = atof(value);
    }
    
    // Validate spawn conditions
    if (custom_delay < 0) {
        gi.dprintf("WARNING: %s has negative delay!\n", classname);
        custom_delay = 0;
    }
}
```

#### Spawn()
```cpp
virtual void Spawn();

// Called: Main initialization phase
// Purpose: Set up entity properties, models, physics, callbacks
// Use for: Primary entity setup
```

**Example:**
```cpp
void svg_monster_soldier_t::Spawn() {
    // Call parent
    svg_monster_base_t::Spawn();
    
    // Set model and visuals
    gi.SetModel(edict, "models/monsters/soldier/tris.md2");
    s.skinnum = 0;
    s.frame = 0;
    
    // Set bounding box
    mins = {-16, -16, -24};
    maxs = {16, 16, 32};
    
    // Set physics
    movetype = MOVETYPE_STEP;
    solid = SOLID_BBOX;
    clipmask = MASK_MONSTERSOLID;
    
    // Set properties
    health = 50;
    max_health = 50;
    takedamage = DAMAGE_YES;
    mass = 200;
    
    // Set flags
    flags |= FL_FLY;  // Can fly/jump
    svflags |= SVF_MONSTER;
    
    // Set callbacks
    SetThinkCallback(&svg_monster_soldier_t::AI_Think);
    SetTouchCallback(&svg_monster_soldier_t::Touch);
    SetPainCallback(&svg_monster_soldier_t::Pain);
    SetDieCallback(&svg_monster_soldier_t::Die);
    
    // Link into world
    gi.linkentity(edict);
    
    // Start thinking
    nextthink = level.time + FRAMETIME;
}
```

#### PostSpawn()
```cpp
virtual void PostSpawn();

// Called: After main Spawn() completes
// Purpose: Finalization, establish entity relationships
// Use for: Finding target entities, team setup
```

**Example:**
```cpp
void svg_func_door_t::PostSpawn() {
    // Call parent
    svg_pushmove_edict_t::PostSpawn();
    
    // Find team members
    if (team) {
        svg_base_edict_t *master = this;
        svg_base_edict_t *ent = nullptr;
        
        // Find all entities with same team name
        while ((ent = SVG_Find(ent, FOFS(team), team)) != nullptr) {
            if (ent == this)
                continue;
            
            // Add to team chain
            ent->teammaster = master;
            ent->teamchain = master->teamchain;
            master->teamchain = ent;
        }
    }
    
    // Calculate move distances
    CalculateMoveDistance();
}
```

### Think Callbacks

#### PreThink()
```cpp
virtual void PreThink();

// Called: Before main Think()
// Purpose: Preparation, early state updates
// Frequency: Every frame (if nextthink set)
```

**Example:**
```cpp
void svg_player_edict_t::PreThink() {
    // Update oxygen/drowning
    CheckWaterLevel();
    CheckAirSupply();
    
    // Process powerup timers
    UpdatePowerups();
    
    // Update HUD stats
    UpdateHUD();
}
```

#### Think()
```cpp
virtual void Think();

// Called: Main entity logic
// Purpose: AI, state machines, primary behavior
// Frequency: When nextthink <= level.time
```

**Example:**
```cpp
void svg_monster_soldier_t::Think() {
    // Check if alive
    if (health <= 0) {
        return;
    }
    
    // AI state machine
    switch (ai_state) {
        case AI_STAND:
            AI_Stand();
            break;
        case AI_WALK:
            AI_Walk();
            break;
        case AI_RUN:
            AI_Run();
            break;
        case AI_ATTACK:
            AI_Attack();
            break;
    }
    
    // Update animation
    UpdateAnimation();
    
    // Schedule next think
    nextthink = level.time + FRAMETIME;
}
```

**Example: Door Think**
```cpp
void svg_func_door_t::Think() {
    // Update door movement
    if (moveinfo.state == STATE_UP) {
        // Door is opening
        if (level.time >= moveinfo.end_time) {
            // Finished opening
            moveinfo.state = STATE_TOP;
            Door_HitTop();
        } else {
            // Continue moving
            Move_Calc(moveinfo.end_origin, Door_HitTop);
        }
    } else if (moveinfo.state == STATE_DOWN) {
        // Door is closing
        if (level.time >= moveinfo.end_time) {
            // Finished closing
            moveinfo.state = STATE_BOTTOM;
            Door_HitBottom();
        } else {
            // Continue moving
            Move_Calc(moveinfo.start_origin, Door_HitBottom);
        }
    }
    
    // Schedule next think
    if (moveinfo.state == STATE_UP || moveinfo.state == STATE_DOWN) {
        nextthink = level.time + FRAMETIME;
    }
}
```

#### PostThink()
```cpp
virtual void PostThink();

// Called: After main Think()
// Purpose: Finalization, cleanup
// Frequency: Every frame (if nextthink set)
```

**Example:**
```cpp
void svg_player_edict_t::PostThink() {
    // Apply final velocity modifications
    ApplyFriction();
    
    // Update view position
    UpdateViewPosition();
    
    // Link entity with new position
    gi.linkentity(edict);
}
```

### Interaction Callbacks

#### Touch()
```cpp
virtual void Touch(svg_base_edict_t *other, 
                  const cm_plane_t *plane, 
                  cm_surface_t *surf);

// Called: When entities collide (solid == SOLID_TRIGGER or SOLID_BBOX)
// Parameters:
//   other: Entity that touched us
//   plane: Collision plane (normal, distance)
//   surf: Surface that was touched
```

**Example: Trigger Touch**
```cpp
void svg_trigger_multiple_t::Touch(svg_base_edict_t *other,
                                   const cm_plane_t *plane,
                                   cm_surface_t *surf) {
    // Check if player
    if (!other->client) {
        return;  // Only players activate
    }
    
    // Check cooldown
    if (level.time < timestamp) {
        return;  // Too soon
    }
    
    // Check activation count
    if (count > 0) {
        count--;
        if (count == 0) {
            // Used up all activations
            solid = SOLID_NOT;
            gi.linkentity(edict);
        }
    }
    
    // Activate targets
    UseTargets(other, other);
    
    // Set cooldown
    timestamp = level.time + wait;
}
```

**Example: Item Pickup**
```cpp
void svg_item_health_t::Touch(svg_base_edict_t *other,
                              const cm_plane_t *plane,
                              cm_surface_t *surf) {
    // Must be a player
    if (!other->client) {
        return;
    }
    
    // Check if already at max health
    if (other->health >= other->max_health) {
        return;
    }
    
    // Give health
    other->health += health_amount;
    if (other->health > other->max_health) {
        other->health = other->max_health;
    }
    
    // Play pickup sound
    gi.sound(other, CHAN_ITEM, gi.soundindex("items/health.wav"), 
             1, ATTN_NORM, 0);
    
    // Remove item temporarily
    solid = SOLID_NOT;
    svflags |= SVF_NOCLIENT;
    
    // Schedule respawn
    SetThinkCallback(&svg_item_health_t::Respawn);
    nextthink = level.time + respawn_time;
}
```

#### Use()
```cpp
virtual void Use(svg_base_edict_t *other, 
                svg_base_edict_t *activator,
                entity_usetarget_type_t useType,
                int32_t useValue);

// Called: When entity is activated (target system or player +use key)
// Parameters:
//   other: Entity that called Use (button, trigger)
//   activator: Original activator (usually player)
//   useType: ON, OFF, TOGGLE, SET
//   useValue: Optional value parameter
```

**Example: Door Use**
```cpp
void svg_func_door_t::Use(svg_base_edict_t *other,
                          svg_base_edict_t *activator,
                          entity_usetarget_type_t useType,
                          int32_t useValue) {
    // Check if door is moving
    if (moveinfo.state == STATE_UP || moveinfo.state == STATE_DOWN) {
        return;  // Already moving
    }
    
    // Handle use type
    if (useType == ENTITY_USETARGET_TYPE_ON || 
        useType == ENTITY_USETARGET_TYPE_TOGGLE) {
        
        if (moveinfo.state == STATE_BOTTOM) {
            // Door is closed, open it
            Door_GoUp();
        } else if (moveinfo.state == STATE_TOP) {
            // Door is open, close it
            Door_GoDown();
        }
    } else if (useType == ENTITY_USETARGET_TYPE_OFF) {
        // Force close
        if (moveinfo.state == STATE_TOP) {
            Door_GoDown();
        }
    }
}
```

**Example: Button Use**
```cpp
void svg_func_button_t::Use(svg_base_edict_t *other,
                            svg_base_edict_t *activator,
                            entity_usetarget_type_t useType,
                            int32_t useValue) {
    // Check if already pressed
    if (moveinfo.state == STATE_UP || 
        moveinfo.state == STATE_DOWN) {
        return;
    }
    
    // Play button sound
    gi.sound(edict, CHAN_VOICE, moveinfo.sound_start, 1, ATTN_NORM, 0);
    
    // Move button
    Button_Move();
    
    // Fire targets
    UseTargets(activator, activator);
}
```

#### Blocked()
```cpp
virtual void Blocked(svg_base_edict_t *other);

// Called: When moving entity is blocked by another entity
// Purpose: Handle collision with obstacles
// Common for: Doors, platforms, trains
```

**Example: Door Blocked**
```cpp
void svg_func_door_t::Blocked(svg_base_edict_t *other) {
    // Damage the blocker
    if (dmg && other->takedamage) {
        T_Damage(other, this, this, vec3_zero(), other->s.origin,
                 vec3_zero(), dmg, 1, 0, MEANS_OF_DEATH_CRUSHED);
    }
    
    // Check door behavior
    if (spawnflags & SPAWNFLAG_CRUSHER) {
        // Crusher: Continue crushing
        return;
    }
    
    // Normal door: Reverse direction
    if (moveinfo.state == STATE_DOWN) {
        // Was closing, reopen
        Door_GoUp();
    } else if (moveinfo.state == STATE_UP) {
        // Was opening, close again
        Door_GoDown();
    }
}
```

**Example: Train Blocked**
```cpp
void svg_func_train_t::Blocked(svg_base_edict_t *other) {
    // Heavy damage to anything in the way
    if (other->takedamage) {
        T_Damage(other, this, this, vec3_zero(), other->s.origin,
                 vec3_zero(), 10000, 1, 0, MEANS_OF_DEATH_CRUSHED);
    }
    
    // Train doesn't stop for anything
}
```

#### Pain()
```cpp
virtual void Pain(svg_base_edict_t *other, 
                 float kick, 
                 int32_t damage,
                 entity_damageflags_t damageFlags);

// Called: When entity takes damage (but doesn't die)
// Parameters:
//   other: Attacker
//   kick: Knockback amount
//   damage: Damage taken
//   damageFlags: Damage type flags
```

**Example: Monster Pain**
```cpp
void svg_monster_soldier_t::Pain(svg_base_edict_t *other,
                                  float kick,
                                  int32_t damage,
                                  entity_damageflags_t damageFlags) {
    // Don't interrupt death animation
    if (lifeStatus == LIFESTATUS_DEAD) {
        return;
    }
    
    // Play pain sound
    int sound_index = (rand() % 3);  // 3 pain sounds
    gi.sound(edict, CHAN_VOICE, sound_pain[sound_index], 
             1, ATTN_NORM, 0);
    
    // Play pain animation
    if (damage < 10) {
        // Light pain
        s.frame = FRAME_pain1_01;
    } else {
        // Heavy pain
        s.frame = FRAME_pain2_01;
    }
    
    // If not alerted, become alerted to attacker
    if (!enemy) {
        enemy = other;
        FoundTarget();
    }
}
```

**Example: Player Pain**
```cpp
void svg_player_edict_t::Pain(svg_base_edict_t *other,
                               float kick,
                               int32_t damage,
                               entity_damageflags_t damageFlags) {
    // Play pain sound
    if (level.time > pain_debounce_time) {
        gi.sound(edict, CHAN_VOICE, gi.soundindex("*pain100_1.wav"),
                 1, ATTN_NORM, 0);
        pain_debounce_time = level.time + 0.7f;
    }
    
    // Apply view kick (screen shake)
    client->kick_angles[0] = kick * 0.5f;
    
    // Show damage indicators in HUD
    client->damage_blend[0] = 1.0f;  // Red
    client->damage_alpha = 0.5f;
}
```

#### Die()
```cpp
virtual void Die(svg_base_edict_t *inflictor, 
                svg_base_edict_t *attacker, 
                int32_t damage, 
                Vector3 *point);

// Called: When entity's health drops to 0 or below
// Parameters:
//   inflictor: Damage source (rocket, bullet, trigger)
//   attacker: Who caused the damage (player, monster)
//   damage: Final damage amount
//   point: Location of damage
```

**Example: Monster Death**
```cpp
void svg_monster_soldier_t::Die(svg_base_edict_t *inflictor,
                                 svg_base_edict_t *attacker,
                                 int32_t damage,
                                 Vector3 *point) {
    // Check for gibbing
    if (health < gib_health) {
        // Heavy damage, gib the corpse
        gi.sound(edict, CHAN_VOICE, gi.soundindex("misc/udeath.wav"),
                 1, ATTN_NORM, 0);
        ThrowGibs(damage, GIB_ORGANIC);
        lifeStatus = LIFESTATUS_GIBBED;
        SVG_FreeEdict(this);
        return;
    }
    
    // Normal death
    // Play death sound
    gi.sound(edict, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
    
    // Change to corpse
    s.entityType = ET_MONSTER_CORPSE;
    lifeStatus = LIFESTATUS_DEAD;
    
    // Play death animation
    int anim = (rand() % 3);  // 3 death animations
    if (anim == 0) {
        s.frame = FRAME_death1_01;
    } else if (anim == 1) {
        s.frame = FRAME_death2_01;
    } else {
        s.frame = FRAME_death3_01;
    }
    
    // Stop thinking
    SetThinkCallback(nullptr);
    nextthink = 0;
    
    // Can no longer take damage
    takedamage = DAMAGE_NO;
    
    // Become non-solid
    solid = SOLID_NOT;
    
    // Drop to ground
    movetype = MOVETYPE_TOSS;
    
    // Link changes
    gi.linkentity(edict);
    
    // Award points to killer
    if (attacker && attacker->client) {
        attacker->client->resp.score += 10;
    }
    
    // Remove corpse after 30 seconds
    SetThinkCallback(&svg_base_edict_t::SVG_FreeEdict_Think);
    nextthink = level.time + 30.0f;
}
```

## Setting Callbacks

Use type-safe setter methods to assign callback functions:

### Callback Setter Methods

```cpp
// All setter methods are templated for type safety
template<typename FuncPtrType>
FuncPtrType SetSpawnCallback(FuncPtrType funcPtr);

template<typename FuncPtrType>
FuncPtrType SetPostSpawnCallback(FuncPtrType funcPtr);

template<typename FuncPtrType>
FuncPtrType SetPreThinkCallback(FuncPtrType funcPtr);

template<typename FuncPtrType>
FuncPtrType SetThinkCallback(FuncPtrType funcPtr);

template<typename FuncPtrType>
FuncPtrType SetPostThinkCallback(FuncPtrType funcPtr);

template<typename FuncPtrType>
FuncPtrType SetBlockedCallback(FuncPtrType funcPtr);

template<typename FuncPtrType>
FuncPtrType SetTouchCallback(FuncPtrType funcPtr);

template<typename FuncPtrType>
FuncPtrType SetUseCallback(FuncPtrType funcPtr);

template<typename FuncPtrType>
FuncPtrType SetPainCallback(FuncPtrType funcPtr);

template<typename FuncPtrType>
FuncPtrType SetDieCallback(FuncPtrType funcPtr);

template<typename FuncPtrType>
FuncPtrType SetOnSignalInCallback(FuncPtrType funcPtr);
```

### Checking if Callbacks are Set

```cpp
bool HasSpawnCallback() const;
bool HasPostSpawnCallback() const;
bool HasPreThinkCallback() const;
bool HasThinkCallback() const;
bool HasPostThinkCallback() const;
bool HasBlockedCallback() const;
bool HasTouchCallback() const;
bool HasUseCallback() const;
bool HasPainCallback() const;
bool HasDieCallback() const;
bool HasOnSignalInCallback() const;

// Example: Check before calling
if (entity->HasThinkCallback()) {
    entity->DispatchThinkCallback();
}
```

### Type-Safe Callback Assignment

**Compile-time type checking:**
```cpp
void svg_custom_entity_t::Spawn() {
    // CORRECT: Type-safe, compiler verifies signature
    SetThinkCallback(&svg_custom_entity_t::CustomThink);
    
    // WRONG: Compiler error if signature doesn't match
    // SetThinkCallback(&svg_custom_entity_t::WrongSignature);
}

// Correct signature
void CustomThink() {
    // Think logic
}

// Wrong signature (would cause compile error)
void WrongSignature(int parameter) {  // Extra parameter!
    // Won't compile if used with SetThinkCallback
}
```

**Debug validation:**
In debug builds, callback setters validate that the function pointer exists in the class's method table, preventing invalid assignments.

### Setting Multiple Callbacks

```cpp
void svg_monster_soldier_t::Spawn() {
    // Set all needed callbacks at spawn time
    SetThinkCallback(&svg_monster_soldier_t::AI_Think);
    SetTouchCallback(&svg_monster_soldier_t::Touch);
    SetPainCallback(&svg_monster_soldier_t::Pain);
    SetDieCallback(&svg_monster_soldier_t::Die);
    SetBlockedCallback(&svg_monster_soldier_t::Blocked);
    
    nextthink = level.time + FRAMETIME;
}
```

### Changing Callbacks at Runtime

```cpp
void svg_monster_soldier_t::BecomeAngry() {
    // Switch to aggressive AI
    SetThinkCallback(&svg_monster_soldier_t::AI_Attack);
}

void svg_monster_soldier_t::BecomePacific() {
    // Switch to passive AI
    SetThinkCallback(&svg_monster_soldier_t::AI_Wander);
}
```

### Clearing Callbacks

```cpp
// Stop thinking
SetThinkCallback(nullptr);
nextthink = 0;

// Stop taking damage
SetPainCallback(nullptr);
SetDieCallback(nullptr);
takedamage = DAMAGE_NO;

// Disable touch
SetTouchCallback(nullptr);
solid = SOLID_NOT;
```

## Entity Dictionary Access

Access map entity key/value pairs from the entity dictionary:

```cpp
const cm_entity_t *entityDictionary;  // Parsed map entity
```

### Reading String Keys

```cpp
const char *value = ED_GetString(entityDictionary, "key_name");

// Example: Get custom map properties
if (const char *sound = ED_GetString(entityDictionary, "noise")) {
    sound_index = gi.soundindex(sound);
}

if (const char *msg = ED_GetString(entityDictionary, "message")) {
    message = msg;
}
```

### Reading Numeric Keys

```cpp
// Integer
int value = ED_GetInt(entityDictionary, "key_name", default_value);

// Float
float value = ED_GetFloat(entityDictionary, "key_name", default_value);

// Example: Get damage value
int damage = ED_GetInt(entityDictionary, "dmg", 5);  // Default 5

// Example: Get speed
float speed = ED_GetFloat(entityDictionary, "speed", 100.0f);
```

### Reading Vector Keys

```cpp
vec3_t value;
ED_GetVector(entityDictionary, "key_name", value);

// Example: Get custom offset
vec3_t offset;
ED_GetVector(entityDictionary, "offset", offset);
```

### Custom Key Parsing

```cpp
void svg_custom_spawner_t::PreSpawn() {
    svg_base_edict_t::PreSpawn();
    
    // Parse custom keys
    if (const char *val = ED_GetString(entityDictionary, "monster_type")) {
        if (!Q_strcasecmp(val, "soldier")) {
            monster_type = MONSTER_SOLDIER;
        } else if (!Q_strcasecmp(val, "tank")) {
            monster_type = MONSTER_TANK;
        }
    }
    
    spawn_count = ED_GetInt(entityDictionary, "count", 5);
    spawn_delay = ED_GetFloat(entityDictionary, "delay", 2.0f);
}
```

## Save/Load System

### Implementing Save/Load

```cpp
virtual void Save(game_write_context_t *ctx);
virtual void Restore(game_read_context_t *ctx);
```

**Save method:**
```cpp
void svg_custom_entity_t::Save(game_write_context_t *ctx) {
    // ALWAYS call parent first
    svg_base_edict_t::Save(ctx);
    
    // Save custom data
    SVG_Save_Write(ctx, &custom_int, sizeof(custom_int));
    SVG_Save_Write(ctx, &custom_float, sizeof(custom_float));
    SVG_Save_WriteString(ctx, custom_string);
}
```

**Load/Restore method:**
```cpp
void svg_custom_entity_t::Restore(game_read_context_t *ctx) {
    // ALWAYS call parent first - this automatically restores callbacks!
    svg_base_edict_t::Restore(ctx);
    
    // Load custom data IN SAME ORDER
    SVG_Save_Read(ctx, &custom_int, sizeof(custom_int));
    SVG_Save_Read(ctx, &custom_float, sizeof(custom_float));
    custom_string = SVG_Save_ReadString(ctx);
    
    // Callbacks are automatically restored by parent Restore()
    // No manual callback setup needed!
}
```

**How Callback Persistence Works:**

Q2RTXPerimental uses an automatic function pointer registration system:

1. **Registration**: All callbacks are registered at startup with unique IDs
2. **Saving**: Function pointers are written with their type and ID to save file
3. **Loading**: Function pointers are looked up by ID and restored automatically

The `svg_base_edict_t` save descriptor includes all callback pointers:

```cpp
// From svg_base_edict.cpp - automatically handled
SAVE_DESCRIPTOR_DEFINE_FUNCPTR(svg_base_edict_t, spawnCallbackFuncPtr, 
                               SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_SPAWN),
SAVE_DESCRIPTOR_DEFINE_FUNCPTR(svg_base_edict_t, thinkCallbackFuncPtr,
                               SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_THINK),
// ... all callbacks automatically saved/loaded
```

### Save Descriptor System

For complex entities, use save descriptors:

```cpp
// Define fields to save
static svg_save_descriptor_field_t saveDescriptorFields[] = {
    {"custom_int", FOFS(custom_int), F_INT},
    {"custom_float", FOFS(custom_float), F_FLOAT},
    {"custom_vector", FOFS(custom_vector), F_VECTOR},
    {"custom_string", FOFS(custom_string), F_STRING},
    {"custom_entity", FOFS(custom_entity), F_EDICT},
    {nullptr}  // Terminator
};

// Implement GetSaveDescriptorFields
svg_save_descriptor_field_t *svg_custom_entity_t::GetSaveDescriptorFields() {
    return saveDescriptorFields;
}
```

## UseTargets System

Activate entities with matching targetname:

```cpp
void UseTargets(svg_base_edict_t *other, svg_base_edict_t *activator);
```

**Example:**
```cpp
void svg_func_button_t::Use(/*...*/) {
    // Activate all entities with matching targetname
    UseTargets(activator, activator);
}
```

**Target types:**
```cpp
target      // Entity to activate
killtarget  // Entity to remove
deathtarget // Activate on death
```

## Signal I/O System

Advanced entity communication with typed arguments:

```cpp
void SendSignalOut(const char *signalName,
                   svg_base_edict_t *activator,
                   const svg_signal_argument_array_t &args);

virtual void OnSignalIn(svg_base_edict_t *other,
                       svg_base_edict_t *activator,
                       const char *signalName,
                       const svg_signal_argument_array_t &args);
```

**Example: Sending signal**
```cpp
void svg_spawner_t::SpawnWave() {
    // Create arguments
    svg_signal_argument_array_t args;
    
    svg_signal_argument_t wave_arg;
    wave_arg.type = SIGNAL_ARGUMENT_TYPE_NUMBER;
    wave_arg.key = "wave_number";
    wave_arg.value.integer = current_wave;
    args.push_back(wave_arg);
    
    // Send signal
    SendSignalOut("OnWaveStart", this, args);
}
```

**Example: Receiving signal**
```cpp
void svg_hud_t::OnSignalIn(/*...*/, const char *signalName,
                           const svg_signal_argument_array_t &args) {
    if (!Q_strcasecmp(signalName, "OnWaveStart")) {
        int wave = SVG_SignalArguments_GetValue<int>(args, "wave_number", 1);
        DisplayMessage("Wave %d starting!", wave);
    }
}
```

## Type Information System

Runtime type identification and spawning:

```cpp
// Define type info in class
DefineTopRootClass(
    "svg_base_edict_t",    // classname
    svg_base_edict_t,      // class type
    sv_shared_edict_t,     // parent type
    EdictTypeInfo::TypeInfoFlag_GameSpawn  // flags
);
```

**Derived class type info:**
```cpp
DefineClass(
    "monster_soldier",      // classname (map editor)
    svg_monster_soldier_t,  // class type
    svg_monster_base_t,     // parent type
    EdictTypeInfo::TypeInfoFlag_GameSpawn  // can spawn from map
);
```

**Type-safe spawning:**
```cpp
// Spawn specific type
auto *monster = SVG_Spawn<svg_monster_soldier_t>();
monster->Spawn();

// Spawn by classname (from map)
svg_base_edict_t *ent = SVG_SpawnByClassname("monster_soldier");
if (ent) {
    ent->Spawn();
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
