# Vanilla Quake 2 Physics System

The Quake 2 physics system handles movement, collision detection, gravity, and entity interactions. Understanding physics is crucial for entity behavior, collision, and game feel.

> **Note:** This page documents the **original Q2RTX** physics system from `/src/baseq2/`. For Q2RTXPerimental enhancements, see the main Entity and Physics documentation.

**Primary Files (Vanilla Q2RTX):**
- `src/baseq2/svgame/g_phys.cpp` - Server-side entity physics
- `src/baseq2/sharedgame/sg_pmove.h` - Player movement code
- `src/baseq2/svgame/m_move.cpp` - Monster AI movement

## Overview

Quake 2 physics operates on a **fixed timestep** (server framerate), typically 10Hz (100ms per frame) or higher. Each physics frame:

1. **Apply forces** (gravity, velocity)
2. **Move entities** based on movetype
3. **Detect collisions** with world and other entities
4. **Resolve collisions** (slide, bounce, stop)
5. **Call touch callbacks** for overlapping entities
6. **Update ground state** for entities

### Movement Types

Entities have different movement behaviors based on `movetype`:

```cpp
// Movement types (entity->movetype) - Original Quake 2
#define MOVETYPE_NONE           0   // No movement, locked position
#define MOVETYPE_NOCLIP         1   // No collision, flies through world
#define MOVETYPE_PUSH           2   // Push movers (doors, platforms)
#define MOVETYPE_STOP           3   // Stops on contact
#define MOVETYPE_WALK           4   // Player walking (gravity + ground)
#define MOVETYPE_STEP           5   // Monster walking (discrete steps)
#define MOVETYPE_FLY            6   // Free flight (gravity optional)
#define MOVETYPE_TOSS           7   // Ballistic trajectory (gravity)
#define MOVETYPE_FLYMISSILE     8   // Missile flight (no gravity)
#define MOVETYPE_BOUNCE         9   // Bounces off surfaces

// Q2RTXPerimental additions (not in vanilla Q2RTX):
// #define MOVETYPE_PENDULUM    10  // Swinging pendulum
// #define MOVETYPE_PHYSICS     11  // Full physics simulation
```

### Collision Types

Entities have different collision behaviors based on `solid`:

```cpp
// Solid types (entity->solid)
#define SOLID_NOT               0   // No collision (passes through)
#define SOLID_TRIGGER           1   // Touch triggers, no blocking
#define SOLID_BBOX              2   // Bounding box collision
#define SOLID_BSP               3   // BSP model collision (complex geometry)
```

## Movement Type Details

### MOVETYPE_NONE (0)

**No movement** - Entity is locked in place.

**Use cases:**
- Static decorations
- Temporary placeholder entities
- Entities with custom movement logic

**Behavior:**
- Velocity ignored
- Position never changes
- No collision detection
- No gravity

**Example:**
```cpp
void svg_decoration_t::Spawn() {
    movetype = MOVETYPE_NONE;
    solid = SOLID_NOT;
    // Static decoration, never moves
}
```

### MOVETYPE_NOCLIP (1)

**Free movement** - No collision with anything.

**Use cases:**
- Spectator mode
- Fly/noclip cheat
- Cinematic cameras
- Debug movement

**Behavior:**
- Moves through all geometry
- No collision detection
- Gravity not applied
- Velocity applied directly to position

**Physics step:**
```cpp
void SV_Physics_Noclip(svg_base_edict_t *ent) {
    // Simply add velocity to position
    VectorMA(ent->s.origin, FRAMETIME, ent->velocity, ent->s.origin);
    VectorMA(ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);
    gi.linkentity(ent);
}
```

### MOVETYPE_PUSH (2)

**Push movers** - Solid objects that push other entities.

**Use cases:**
- Doors (`func_door`)
- Platforms (`func_plat`)
- Trains (`func_train`)
- Rotating brushes (`func_rotating`)

**Behavior:**
- Pushes blocking entities
- Blocks player/monster movement
- Can crush entities if blocked
- Triggers touch callbacks on pushed entities

**Special properties:**
- Always `SOLID_BSP` (BSP model)
- Has inline model (`"model" "*1"`)
- Moves on predefined path or rotation
- Can trigger `Blocked()` callback

**Example:**
```cpp
void svg_func_door_t::Spawn() {
    movetype = MOVETYPE_PUSH;
    solid = SOLID_BSP;
    
    gi.SetModel(edict, model);  // Set BSP model
    
    // Door will push entities when moving
    SetBlockedCallback(&svg_func_door_t::Blocked);
}

void svg_func_door_t::Blocked(svg_base_edict_t *other) {
    // Door hit something while moving
    if (other->takedamage) {
        SVG_InflictDamage(other, this, this, vec3_origin, 
                         other->s.origin, vec3_origin,
                         dmg, 0, 0, MOD_CRUSH);
    }
    
    // Reverse direction or stop
    GoReverse();
}
```

### MOVETYPE_WALK (4)

**Player walking** - Complex player movement with ground detection.

**Use cases:**
- Player entities only
- Includes walking, running, jumping, swimming, ladders

**Behavior:**
- Gravity applied when not on ground
- Ground friction when on ground
- Acceleration based on input
- Step up/down stairs automatically
- Water buoyancy
- Ladder climbing

**Handled by pmove (Player Movement) system:**
- Located in `src/baseq2rtxp/sharedgame/pmove/`
- Shared between client (prediction) and server (authoritative)
- Complex state machine for different movement modes

**Movement parameters:**
```cpp
// Default player movement parameters
static constexpr double pm_stop_speed = 100.;
static constexpr double pm_max_speed = 300.;
static constexpr double pm_jump_height = 270.;
static constexpr double pm_accelerate = 10.;
static constexpr double pm_air_accelerate = 1.;
static constexpr double pm_friction = 6.;
static constexpr double pm_water_speed = 400.;
static constexpr double pm_fly_speed = 400.;
```

**Player bounding boxes:**
```cpp
// Standing
static constexpr Vector3 PM_BBOX_STANDUP_MINS = {-16, -16, -36};
static constexpr Vector3 PM_BBOX_STANDUP_MAXS = {16, 16, 36};
static constexpr double  PM_VIEWHEIGHT_STANDUP = 30;

// Crouching
static constexpr Vector3 PM_BBOX_DUCKED_MINS = {-16, -16, -36};
static constexpr Vector3 PM_BBOX_DUCKED_MAXS = {16, 16, 8};
static constexpr double  PM_VIEWHEIGHT_DUCKED = 4;
```

**Stair stepping:**
```cpp
// Automatically step up/down stairs
static constexpr double PM_STEP_MIN_SIZE = 2.;   // Minimum step height
static constexpr double PM_STEP_MAX_SIZE = 18.;  // Maximum step height
static constexpr double PM_STEP_GROUND_DIST = 0.25;
```

### MOVETYPE_STEP (5)

**Monster walking** - Simplified ground-based movement for AI.

**Use cases:**
- Walking monsters (soldiers, guards, etc.)
- Ground-based NPCs
- Any AI that needs to walk on floors

**Behavior:**
- Gravity applied
- Stays on ground
- Steps up stairs (like MOVETYPE_WALK but simpler)
- No complex acceleration/friction
- Velocity applied directly

**Physics step:**
```cpp
void SV_Physics_Step(svg_base_edict_t *ent) {
    bool wasonground = (ent->groundInfo.entityNumber != ENTITYNUM_NONE);
    
    // Check velocity
    SV_CheckVelocity(ent);
    
    // Add gravity if not on ground or flying
    if (ent->groundInfo.entityNumber == ENTITYNUM_NONE && 
        !(ent->flags & FL_FLY) && !(ent->flags & FL_SWIM)) {
        SV_AddGravity(ent);
    }
    
    // Move
    SV_FlyMove(ent, FRAMETIME, ent->clipMask);
    gi.linkentity(ent);
    
    // Touch triggers
    SVG_TouchTriggers(ent);
    
    // Check if still on ground
    if (!ent->inUse)
        return;
    
    if (ent->groundInfo.entityNumber != ENTITYNUM_NONE) {
        if (!wasonground) {
            // Just landed
            if (ent->velocity[2] < -200) {
                // Play landing sound
            }
        }
    }
}
```

### MOVETYPE_FLY (6)

**Free flight** - Flying movement, optionally with gravity.

**Use cases:**
- Flying monsters (flyers, brain, etc.)
- Projectiles with steering
- Levitating entities

**Behavior:**
- No automatic ground checking
- Gravity optional (check `FL_FLY` flag)
- Velocity applied with collision detection
- Can fly anywhere in 3D space

**Example:**
```cpp
void svg_monster_flyer_t::Spawn() {
    movetype = MOVETYPE_FLY;
    solid = SOLID_BBOX;
    
    flags |= FL_FLY;  // Disable gravity
    
    mins = {-16, -16, 0};
    maxs = {16, 16, 32};
}
```

### MOVETYPE_TOSS (7)

**Ballistic trajectory** - Affected by gravity, bounces/stops on impact.

**Use cases:**
- Gibs (body parts)
- Thrown items
- Debris
- Item pickups
- Corpses

**Behavior:**
- Gravity always applied
- Flies through air
- Stops on ground contact
- Can come to rest
- Sets `groundentity` when resting

**Physics step:**
```cpp
void SV_Physics_Toss(svg_base_edict_t *ent) {
    // Check if on ground and at rest
    if (ent->groundInfo.entityNumber != ENTITYNUM_NONE) {
        // Check velocity
        if (VectorLength(ent->velocity) < 20 &&
            VectorLength(ent->avelocity) < 20) {
            // Come to rest
            VectorClear(ent->velocity);
            VectorClear(ent->avelocity);
            ent->nextthink = 0_ms;  // Stop thinking
            return;
        }
    }
    
    SV_CheckVelocity(ent);
    
    // Add gravity
    if (ent->movetype != MOVETYPE_FLY && 
        ent->movetype != MOVETYPE_FLYMISSILE) {
        SV_AddGravity(ent);
    }
    
    // Move
    VectorMA(ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);
    int move = SV_FlyMove(ent, FRAMETIME, ent->clipMask);
    gi.linkentity(ent);
    
    // Touch triggers
    SVG_TouchTriggers(ent);
}
```

**Example:**
```cpp
void svg_gib_t::Spawn() {
    movetype = MOVETYPE_TOSS;
    solid = SOLID_NOT;  // Pass through entities
    clipMask = MASK_SOLID;
    
    // Random velocity
    velocity[0] = 100 * crandom();
    velocity[1] = 100 * crandom();
    velocity[2] = 200 + 100 * crandom();
    
    // Random spin
    avelocity = {crandom() * 600, crandom() * 600, crandom() * 600};
    
    // Remove after 30 seconds
    think = &svg_gib_t::Remove;
    nextthink = level.time + 30_sec;
}
```

### MOVETYPE_FLYMISSILE (8)

**Missile flight** - Like FLY but ignores some collision types.

**Use cases:**
- Rockets
- Grenades in flight
- BFG projectiles
- Any high-speed projectile

**Behavior:**
- No gravity
- Clips only against solids and monsters
- Doesn't clip against other projectiles
- Fast movement with collision

**Special clipmask:**
```cpp
// Missiles clip against world and monsters, not other missiles
#define MASK_PROJECTILE  (CONTENTS_SOLID | CONTENTS_MONSTER | \
                         CONTENTS_WINDOW | CONTENTS_DEADMONSTER)
```

**Example:**
```cpp
void svg_projectile_rocket_t::Spawn() {
    movetype = MOVETYPE_FLYMISSILE;
    solid = SOLID_BBOX;
    clipMask = MASK_PROJECTILE;
    
    svflags |= SVF_PROJECTILE;
    
    mins = {-3, -3, -3};
    maxs = {3, 3, 3};
    
    velocity = forward * 650;  // Fast!
    
    SetTouchCallback(&svg_projectile_rocket_t::Touch);
}

void svg_projectile_rocket_t::Touch(svg_base_edict_t *other, ...) {
    // Explode on impact
    Explode();
}
```

### MOVETYPE_BOUNCE (9)

**Bouncing** - Like TOSS but bounces off surfaces.

**Use cases:**
- Grenades
- Bouncing projectiles
- Bouncing debris

**Behavior:**
- Gravity applied
- Bounces off walls/floors
- Velocity reduced each bounce
- Eventually comes to rest

**Bounce coefficient:**
```cpp
// Velocity multiplier on bounce (0.0 = no bounce, 1.0 = perfect bounce)
#define BOUNCE_COEFFICIENT  0.5
```

**Example:**
```cpp
void svg_projectile_grenade_t::Spawn() {
    movetype = MOVETYPE_BOUNCE;
    solid = SOLID_BBOX;
    
    mins = {-3, -3, -3};
    maxs = {3, 3, 3};
    
    // Throw arc
    velocity = forward * 600 + vec3_t{0, 0, 200};
    
    // Spin
    avelocity = {300, 300, 300};
    
    // Explode after 2.5 seconds
    SetThinkCallback(&svg_projectile_grenade_t::Explode);
    nextthink = level.time + 2.5_sec;
    
    SetTouchCallback(&svg_projectile_grenade_t::Touch);
}

void svg_projectile_grenade_t::Touch(svg_base_edict_t *other, ...) {
    // Bounce off surfaces, don't explode yet
    // Explode only on timer or when touching monster
    if (other->takedamage) {
        Explode();
    }
}
```

## Gravity System

Gravity is applied each physics frame to entities that need it.

### Gravity Application

```cpp
void SV_AddGravity(svg_base_edict_t *ent) {
    // Get entity's gravity multiplier (default 1.0)
    float entGravity = (ent->gravity != 0) ? ent->gravity : 1.0f;
    
    // Apply gravity: velocity.z -= gravity * multiplier * frametime
    ent->velocity[2] -= entGravity * sv_gravity->value * FRAMETIME;
}
```

### Gravity Constants

```cpp
// World gravity (default 800 units/sec²)
cvar_t *sv_gravity;  // Default: 800

// Entity gravity multiplier (entity->gravity)
// 0.0 or 1.0 = Normal gravity
// 0.0 = Uses world gravity (1.0)
// 0.5 = Half gravity
// 2.0 = Double gravity
```

### Terminal Velocity

```cpp
// Maximum velocity (prevents infinite acceleration)
cvar_t *sv_maxvelocity;  // Default: 2000

void SV_CheckVelocity(svg_base_edict_t *ent) {
    // Clamp each axis
    for (int i = 0; i < 3; i++) {
        ent->velocity[i] = std::clamp(ent->velocity[i], 
                                     -sv_maxvelocity->value,
                                      sv_maxvelocity->value);
    }
}
```

### Gravity Example

```cpp
void svg_item_health_t::Spawn() {
    movetype = MOVETYPE_TOSS;
    solid = SOLID_TRIGGER;
    
    // Fall to ground with normal gravity
    gravity = 1.0f;
    
    // Drop item
    SVG_DropToFloor(edict);
}
```

## Collision Detection

Quake 2 uses **swept bounding box** collision detection.

### Trace Function

The core collision detection function:

```cpp
svg_trace_t SVG_Trace(
    vec3_t start,           // Start position
    vec3_t mins,            // Bounding box mins
    vec3_t maxs,            // Bounding box maxs
    vec3_t end,             // End position
    svg_base_edict_t *skip, // Entity to ignore
    cm_contents_t mask      // Content mask for collision
);
```

### Trace Result

```cpp
typedef struct svg_trace_s {
    bool        allsolid;       // Completely stuck in solid
    bool        startsolid;     // Started inside solid
    float       fraction;       // 0.0 to 1.0, how far trace went
    vec3_t      endpos;         // Final position
    cm_plane_t  plane;          // Surface plane hit
    cm_surface_t *surface;      // Surface properties hit
    cm_contents_t contents;     // Contents at end
    svg_base_edict_t *ent;      // Entity hit (NULL if world)
    int         entityNumber;   // Entity number hit
} svg_trace_t;
```

**Fraction values:**
- `1.0` = Reached end position (no hit)
- `0.0 to 0.99` = Hit something (endpos is impact point)
- `0.0` = Blocked at start position

### Content Masks

Content masks define what to collide with:

```cpp
// Common masks
#define MASK_ALL            (-1)        // Everything
#define MASK_SOLID          (CONTENTS_SOLID | CONTENTS_WINDOW)
#define MASK_PLAYERSOLID    (CONTENTS_SOLID | CONTENTS_WINDOW | \
                            CONTENTS_MONSTER | CONTENTS_PLAYER)
#define MASK_DEADSOLID      (CONTENTS_SOLID | CONTENTS_WINDOW | \
                            CONTENTS_PLAYER)
#define MASK_MONSTERSOLID   (CONTENTS_SOLID | CONTENTS_WINDOW | \
                            CONTENTS_MONSTER)
#define MASK_WATER          (CONTENTS_WATER | CONTENTS_LAVA | \
                            CONTENTS_SLIME)
#define MASK_OPAQUE         (CONTENTS_SOLID | CONTENTS_SLIME | \
                            CONTENTS_LAVA)
#define MASK_SHOT           (CONTENTS_SOLID | CONTENTS_MONSTER | \
                            CONTENTS_WINDOW | CONTENTS_DEADMONSTER)
#define MASK_PROJECTILE     (CONTENTS_SOLID | CONTENTS_MONSTER | \
                            CONTENTS_WINDOW | CONTENTS_DEADMONSTER)
```

### Clip Mask Selection

```cpp
const cm_contents_t SVG_GetClipMask(svg_base_edict_t *ent) {
    cm_contents_t mask = ent->clipMask;
    
    if (!mask) {
        // Default masks based on entity type
        if (ent->svFlags & SVF_PLAYER) {
            mask = MASK_PLAYERSOLID;
        } else if (ent->svFlags & SVF_MONSTER) {
            mask = MASK_MONSTERSOLID;
        } else if (ent->svFlags & SVF_PROJECTILE) {
            mask = MASK_PROJECTILE;
        } else {
            mask = MASK_SHOT;
        }
    }
    
    // Non-solid entities don't collide with players/monsters
    if (ent->solid == SOLID_NOT || ent->solid == SOLID_TRIGGER) {
        mask &= ~(CONTENTS_MONSTER | CONTENTS_PLAYER);
    }
    
    // Dead entities don't collide with players/monsters
    if ((ent->svFlags & (SVF_MONSTER | SVF_PLAYER)) && 
        (ent->svFlags & SVF_DEADENTITY)) {
        mask &= ~(CONTENTS_MONSTER | CONTENTS_PLAYER);
    }
    
    return mask;
}
```

### Collision Example

```cpp
// Check if path is clear
bool PathClear(vec3_t start, vec3_t end, svg_base_edict_t *self) {
    svg_trace_t trace = SVG_Trace(start, self->mins, self->maxs, 
                                   end, self, MASK_SOLID);
    return (trace.fraction == 1.0f);
}

// Find ground beneath entity
void FindGround(svg_base_edict_t *ent) {
    vec3_t end = ent->s.origin;
    end[2] -= 256;  // Trace down
    
    svg_trace_t trace = SVG_Trace(ent->s.origin, ent->mins, ent->maxs,
                                   end, ent, MASK_SOLID);
    
    if (trace.fraction < 1.0f) {
        // Found ground
        VectorCopy(trace.endpos, ent->s.origin);
        ent->groundInfo.entityNumber = trace.entityNumber;
        ent->groundInfo.entityLinkCount = trace.ent->linkCount;
    }
}
```

## Velocity Clipping (Sliding)

When entities hit surfaces, velocity is clipped to slide along them.

### ClipVelocity Function

```cpp
int SVG_Physics_ClipVelocity(Vector3 &in, vec3_t normal, 
                              Vector3 &out, double overbounce) {
    int blocked = 0;
    
    // Determine blocking type
    if (normal[2] > 0) {
        blocked |= 1;  // Floor
    }
    if (normal[2] == 0) {
        blocked |= 2;  // Wall/step
    }
    
    // Calculate backoff
    double backoff = DotProduct(in, normal) * overbounce;
    
    // Apply clipping
    for (int i = 0; i < 3; i++) {
        double change = normal[i] * backoff;
        out[i] = in[i] - change;
        
        // Stop small movements
        if (out[i] > -0.1 && out[i] < 0.1) {
            out[i] = 0;
        }
    }
    
    return blocked;
}
```

**Overbounce values:**
- `1.0` = Slide along surface (no bounce)
- `1.5` = Slight bounce
- `2.0` = Full bounce

### FlyMove Algorithm

The core movement algorithm that handles sliding along multiple planes:

```cpp
int SV_FlyMove(svg_base_edict_t *ent, float time, cm_contents_t mask) {
    vec3_t planes[MAX_CLIP_PLANES];  // Max 5 planes
    int numplanes = 0;
    int blocked = 0;
    float time_left = time;
    
    vec3_t original_velocity;
    VectorCopy(ent->velocity, original_velocity);
    
    // Try up to 4 bumps
    for (int bumpcount = 0; bumpcount < 4; bumpcount++) {
        // Calculate end position
        vec3_t end;
        VectorMA(ent->s.origin, time_left, ent->velocity, end);
        
        // Trace movement
        svg_trace_t trace = SVG_Trace(ent->s.origin, ent->mins, ent->maxs,
                                      end, ent, mask);
        
        if (trace.allsolid) {
            // Stuck in solid
            VectorClear(ent->velocity);
            return 3;  // Blocked by floor and wall
        }
        
        if (trace.fraction > 0) {
            // Made some progress
            VectorCopy(trace.endpos, ent->s.origin);
            numplanes = 0;
        }
        
        if (trace.fraction == 1.0f) {
            break;  // Moved entire distance
        }
        
        // Hit something
        time_left -= time_left * trace.fraction;
        
        // Record plane
        if (numplanes >= MAX_CLIP_PLANES) {
            VectorClear(ent->velocity);
            return 3;
        }
        
        VectorCopy(trace.plane.normal, planes[numplanes]);
        numplanes++;
        
        // Modify velocity to slide along planes
        int i;
        for (i = 0; i < numplanes; i++) {
            SVG_Physics_ClipVelocity(original_velocity, planes[i],
                                     ent->velocity, 1.0);
            
            // Check if velocity is okay for all planes
            int j;
            for (j = 0; j < numplanes; j++) {
                if (j != i) {
                    if (DotProduct(ent->velocity, planes[j]) < 0) {
                        break;  // Not okay
                    }
                }
            }
            
            if (j == numplanes) {
                break;  // Velocity is okay
            }
        }
        
        if (i == numplanes) {
            // Go along crease (intersection of two planes)
            if (numplanes != 2) {
                VectorClear(ent->velocity);
                return 7;  // Blocked
            }
            
            // Calculate crease direction
            vec3_t dir;
            CrossProduct(planes[0], planes[1], dir);
            float d = DotProduct(dir, ent->velocity);
            VectorScale(dir, d, ent->velocity);
        }
        
        // If velocity is against original direction, stop
        if (DotProduct(ent->velocity, original_velocity) <= 0) {
            VectorClear(ent->velocity);
            return blocked;
        }
    }
    
    return blocked;
}
```

**FlyMove handles:**
- Sliding along walls
- Sliding along floors
- Navigating corners (creases between planes)
- Getting stuck in geometry

## Ground Detection

Entities track what they're standing on.

### Ground Entity

```cpp
// Entity ground state (entity->groundInfo)
struct {
    int entityNumber;       // Entity number we're standing on
    int entityLinkCount;    // Link count for validation
} groundInfo;
```

**Special values:**
- `entityNumber == ENTITYNUM_NONE` = Not on ground (in air)
- `entityNumber == 0` = On world geometry
- `entityNumber > 0` = On another entity

### Ground Validation

```cpp
// Check if ground entity is still valid
bool ValidateGround(svg_base_edict_t *ent) {
    if (ent->groundInfo.entityNumber == ENTITYNUM_NONE) {
        return false;  // Not on ground
    }
    
    svg_base_edict_t *ground = g_edict_pool.EdictForNumber(
        ent->groundInfo.entityNumber);
    
    // Check if entity still exists and hasn't moved
    if (!ground->inUse || 
        ground->linkCount != ent->groundInfo.entityLinkCount) {
        ent->groundInfo.entityNumber = ENTITYNUM_NONE;
        return false;
    }
    
    return true;
}
```

### Ground Contact Detection

```cpp
// Detect if entity is on ground
void CheckGround(svg_base_edict_t *ent) {
    // Trace slightly down
    vec3_t end = ent->s.origin;
    end[2] -= 1;  // 1 unit down
    
    svg_trace_t trace = SVG_Trace(ent->s.origin, ent->mins, ent->maxs,
                                   end, ent, MASK_SOLID);
    
    if (trace.plane.normal[2] < 0.7f) {
        // Too steep, not ground
        ent->groundInfo.entityNumber = ENTITYNUM_NONE;
        return;
    }
    
    // Standing on something
    if (trace.ent && trace.ent->solid == SOLID_BSP) {
        ent->groundInfo.entityNumber = trace.entityNumber;
        ent->groundInfo.entityLinkCount = trace.ent->linkCount;
    } else if (trace.entityNumber == 0) {
        // World
        ent->groundInfo.entityNumber = 0;
    }
}
```

## Water Physics

Special physics for water volumes.

### Waterlevel

```cpp
// Entity waterlevel (entity->waterlevel)
#define WATERLEVEL_NONE     0   // Not in water
#define WATERLEVEL_FEET     1   // Feet in water
#define WATERLEVEL_WAIST    2   // Waist deep
#define WATERLEVEL_UNDER    3   // Fully submerged

// Entity watertype (entity->watertype)
// CONTENTS_WATER, CONTENTS_SLIME, or CONTENTS_LAVA
```

### Water Detection

```cpp
void CheckWaterLevel(svg_base_edict_t *ent) {
    vec3_t point = ent->s.origin;
    
    // Check feet
    point[2] = ent->s.origin[2] + ent->mins[2] + 1;
    cm_contents_t cont = gi.pointcontents(point);
    
    if (!(cont & MASK_WATER)) {
        ent->waterlevel = 0;
        ent->watertype = 0;
        return;
    }
    
    ent->watertype = cont;
    ent->waterlevel = 1;
    
    // Check waist
    point[2] = ent->s.origin[2];
    cont = gi.pointcontents(point);
    if (!(cont & MASK_WATER)) {
        return;
    }
    
    ent->waterlevel = 2;
    
    // Check head
    point[2] = ent->s.origin[2] + ent->maxs[2] - 1;
    cont = gi.pointcontents(point);
    if (cont & MASK_WATER) {
        ent->waterlevel = 3;
    }
}
```

### Water Effects

**Buoyancy:**
```cpp
if (ent->waterlevel && !(ent->flags & FL_SWIM)) {
    // Apply upward force (buoyancy)
    ent->velocity[2] += 10;
}
```

**Drowning:**
```cpp
if (ent->waterlevel == 3 && ent->watertype & CONTENTS_WATER) {
    // Player underwater
    ent->air_finished -= FRAMETIME;
    
    if (ent->air_finished < level.time) {
        // Drowning damage
        SVG_InflictDamage(ent, world, world, vec3_origin,
                         ent->s.origin, vec3_origin,
                         2, 0, DAMAGE_NO_ARMOR, MOD_WATER);
    }
} else {
    // Restore air
    ent->air_finished = level.time + 12_sec;
}
```

**Damage in lava/slime:**
```cpp
if (ent->waterlevel && (ent->watertype & (CONTENTS_LAVA | CONTENTS_SLIME))) {
    if (ent->damage_time < level.time) {
        ent->damage_time = level.time + 0.2_sec;
        
        int dmg = (ent->watertype & CONTENTS_LAVA) ? 10 : 4;
        SVG_InflictDamage(ent, world, world, vec3_origin,
                         ent->s.origin, vec3_origin,
                         dmg, 0, 0, MOD_LAVA);
    }
}
```

## Touch Triggers

Entities can trigger other entities by touching them.

### Touch Callback

```cpp
void svg_trigger_hurt_t::Touch(svg_base_edict_t *other, ...) {
    if (!other->takedamage) {
        return;
    }
    
    if (level.time < other->pain_debounce_time) {
        return;  // Cooldown
    }
    
    other->pain_debounce_time = level.time + 1_sec;
    
    SVG_InflictDamage(other, this, this, vec3_origin,
                     other->s.origin, vec3_origin,
                     dmg, 0, 0, MOD_TRIGGER_HURT);
}
```

### Touch Trigger Detection

```cpp
void SVG_TouchTriggers(svg_base_edict_t *ent) {
    // Don't touch triggers if dead
    if (ent->health <= 0) {
        return;
    }
    
    // Find all entities touching
    int num = gi.BoxEdicts(ent->absmin, ent->absmax, 
                          touch_list, MAX_EDICTS, AREA_TRIGGERS);
    
    for (int i = 0; i < num; i++) {
        svg_base_edict_t *hit = touch_list[i];
        
        if (!hit->HasTouchCallback()) {
            continue;
        }
        
        if (hit->solid != SOLID_TRIGGER) {
            continue;
        }
        
        // Call touch
        hit->DispatchTouchCallback(ent, nullptr, nullptr);
    }
}
```

## Physics Best Practices

### 1. Choose Correct Movetype

```cpp
// GOOD: Door uses PUSH (solid, blocks movement)
door->movetype = MOVETYPE_PUSH;
door->solid = SOLID_BSP;

// GOOD: Flying monster uses FLY
flyer->movetype = MOVETYPE_FLY;
flyer->flags |= FL_FLY;  // Disable gravity

// GOOD: Item uses TOSS (falls to ground)
item->movetype = MOVETYPE_TOSS;
item->solid = SOLID_TRIGGER;  // Trigger on touch
```

### 2. Set Appropriate Clipmask

```cpp
// Monster clips against world and players
monster->clipMask = MASK_MONSTERSOLID;

// Projectile clips against world and monsters
projectile->clipMask = MASK_PROJECTILE;
projectile->svflags |= SVF_PROJECTILE;
```

### 3. Validate Ground Entity

```cpp
// Always validate before using
if (ent->groundInfo.entityNumber != ENTITYNUM_NONE) {
    svg_base_edict_t *ground = g_edict_pool.EdictForNumber(
        ent->groundInfo.entityNumber);
    
    if (ground->inUse && 
        ground->linkCount == ent->groundInfo.entityLinkCount) {
        // Ground entity is valid
    } else {
        ent->groundInfo.entityNumber = ENTITYNUM_NONE;
    }
}
```

### 4. Link Entity After Movement

```cpp
// ALWAYS link after changing position
VectorCopy(newPos, ent->s.origin);
gi.linkentity(ent);  // Update spatial hash
```

## Related Documentation

- [**Entity Lifecycle**](Entity-Lifecycle.md) - Entity spawn and update
- [**Entity Base Class Reference**](Entity-Base-Class-Reference.md) - Physics properties
- [**Vanilla BSP Format**](Vanilla-BSP-Format.md) - Collision geometry
- [**Vanilla Networking**](Vanilla-Networking.md) - State synchronization

## Summary

Quake 2 physics provides:

- **11 movement types** for different behaviors
- **Gravity system** with per-entity multipliers
- **Swept collision detection** with sliding
- **Ground detection** and validation
- **Water physics** with buoyancy and damage
- **Touch triggers** for entity interactions

Understanding physics is essential for:
- Implementing entity movement
- Creating projectiles and items
- Building interactive world objects
- Designing gameplay mechanics
