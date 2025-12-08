# Vanilla Quake 2 - Entity System Overview

This page explains how entities work in vanilla Quake 2, providing the foundation for understanding Q2RTXPerimental's enhanced entity system.

## What is an Entity?

An **entity** is any object in the game world that has a presence and behavior:
- Players and monsters
- Weapons and items
- Doors, elevators, and triggers
- Projectiles (rockets, grenades)
- Decorative objects (lights, gibs, particles)

Everything except for the static world geometry (BSP brushes) is an entity.

## The edict_t Structure

In vanilla Quake 2, entities are represented by the `edict_t` structure:

```cpp
typedef struct edict_s {
    entity_state_t  s;          // State sent to clients
    struct gclient_s *client;   // NULL if not a player
    
    qboolean    inuse;          // Is this entity slot active?
    int         linkcount;      // For PVS checking
    
    // Movement and physics
    link_t      area;           // Linked list for area grid
    int         num_clusters;   // For PVS
    int         clusternums[MAX_ENT_CLUSTERS];
    int         headnode;       // BSP node for collision
    
    int         areanum, areanum2;
    
    // Callbacks
    void        (*think)(edict_t *self);
    void        (*touch)(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
    void        (*use)(edict_t *self, edict_t *other, edict_t *activator);
    void        (*pain)(edict_t *self, edict_t *other, float kick, int damage);
    void        (*die)(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);
    void        (*blocked)(edict_t *self, edict_t *other);
    
    float       nextthink;      // When to call think()
    edict_t     *groundentity;  // Entity we're standing on
    
    // Physics properties
    int         movetype;       // MOVETYPE_NONE, MOVETYPE_WALK, etc.
    int         solid;          // SOLID_NOT, SOLID_BBOX, SOLID_BSP
    int         clipmask;       // Collision mask
    
    vec3_t      mins, maxs;     // Bounding box (local space)
    vec3_t      absmin, absmax; // Bounding box (world space)
    vec3_t      size;           // maxs - mins
    
    vec3_t      s.origin;       // Position in world
    vec3_t      s.angles;       // Rotation (pitch, yaw, roll)
    vec3_t      s.old_origin;   // For interpolation
    
    vec3_t      velocity;       // Units per second
    vec3_t      avelocity;      // Angular velocity
    
    // Game-specific data
    char        *classname;     // "monster_soldier", "weapon_shotgun"
    int         spawnflags;     // Flags from map editor
    float       timestamp;      // Generic timer
    char        *target;        // Target to trigger
    char        *targetname;    // Name for targeting
    char        *killtarget;    // Target to remove
    char        *team;          // Team name
    char        *message;       // Trigger message
    
    edict_t     *target_ent;    // Cached target entity
    edict_t     *chain;         // For linked lists
    edict_t     *enemy;         // AI: current enemy
    edict_t     *oldenemy;      // AI: previous enemy
    edict_t     *activator;     // Who triggered this entity
    edict_t     *groundentity;  // What we're standing on
    edict_t     *teamchain;     // Multi-part entity chain
    edict_t     *teammaster;    // Master of multi-part entity
    
    // Health and damage
    int         health;
    int         max_health;
    int         gib_health;     // Health at which to gib
    int         deadflag;       // Dead/dying state
    
    int         takedamage;     // DAMAGE_NO, DAMAGE_YES, DAMAGE_AIM
    int         dmg;            // Damage to inflict
    int         dmg_radius;     // Radius of splash damage
    
    // Sound
    int         sounds;         // Sound type/index
    int         noise_index;    // Precached sound index
    
    // Rendering
    float       light_level;    // For client-side lighting
    int         style;          // Light style
    
    // Item/weapon specific
    item_t      *item;          // Item definition
    
    // AI/monster specific  
    int         ideal_yaw;      // Desired facing
    float       yaw_speed;      // Turn rate
    
    // And many more fields...
} edict_t;
```

### Key Fields Explained

**inuse**: Indicates if this entity slot is occupied. The engine allocates a fixed array of edicts (usually 1024), and `inuse` marks which ones are active.

**think/nextthink**: The think callback is called when `level.time` reaches `nextthink`. This is how entities perform time-based actions.

**touch**: Called when another entity collides with this one.

**use**: Called when this entity is triggered (e.g., player presses a button).

**movetype**: Determines how the entity moves:
- `MOVETYPE_NONE` - Doesn't move
- `MOVETYPE_PUSH` - Can push other entities (doors, platforms)
- `MOVETYPE_WALK` - Ground-based movement (players, monsters)
- `MOVETYPE_FLY` - Ignores gravity (flying monsters)
- `MOVETYPE_TOSS` - Affected by gravity (gibs, shells)
- `MOVETYPE_BOUNCE` - Bounces when hitting surfaces (grenades)

**solid**: Determines collision behavior:
- `SOLID_NOT` - No collision (decorative, effects)
- `SOLID_TRIGGER` - Triggers touch events but doesn't block
- `SOLID_BBOX` - Uses bounding box for collision
- `SOLID_BSP` - Uses BSP model for collision (doors, platforms)

## Entity Lifecycle

### 1. Allocation

Entities are allocated from a fixed pool:

```cpp
edict_t *G_Spawn(void) {
    // Search for free entity slot
    for (i = maxclients->value + 1; i < globals.num_edicts; i++) {
        edict_t *e = &g_edicts[i];
        if (!e->inuse) {
            G_InitEdict(e);
            return e;
        }
    }
    
    // Allocate new slot if pool not full
    if (globals.num_edicts == game.maxentities) {
        gi.error("ED_Alloc: no free edicts");
    }
    
    edict_t *e = &g_edicts[globals.num_edicts++];
    G_InitEdict(e);
    return e;
}
```

**Key Points:**
- First slots are reserved for clients (players)
- Entity slots are reused when entities are freed
- There's a hard limit on total entities (usually 1024)

### 2. Spawning

When a map loads, entities are spawned from the BSP entity string:

```cpp
// Map entity string:
{
"classname" "weapon_shotgun"
"origin" "100 200 64"
"spawnflags" "1792"
}

// Spawning process:
void ED_CallSpawn(edict_t *ent) {
    spawn_t *s = FindSpawnFunction(ent->classname);
    if (!s) {
        gi.dprintf("ED_CallSpawn: unknown classname %s\n", ent->classname);
        G_FreeEdict(ent);
        return;
    }
    s->spawn(ent);  // Calls SP_weapon_shotgun(ent)
}
```

Spawn functions initialize the entity:

```cpp
void SP_weapon_shotgun(edict_t *ent) {
    gi.dprintf("SP_weapon_shotgun\n");
    
    // Don't spawn in deathmatch if flag set
    if (deathmatch->value && (ent->spawnflags & SPAWNFLAG_NOT_DEATHMATCH)) {
        G_FreeEdict(ent);
        return;
    }
    
    ent->classname = "weapon_shotgun";
    ent->item = FindItem("Shotgun");
    ent->movetype = MOVETYPE_TOSS;
    ent->solid = SOLID_TRIGGER;
    ent->touch = Touch_Item;
    
    gi.setmodel(ent, ent->item->world_model);
    
    if (ent->spawnflags & SPAWNFLAG_SUSPENDED) {
        ent->s.origin[2] += 16;
        ent->movetype = MOVETYPE_NONE;
    }
    
    gi.linkentity(ent);
}
```

### 3. Thinking

Entities perform actions through their think function:

```cpp
// Example: A light that flashes
void light_think(edict_t *self) {
    if (self->s.effects & EF_ANIM01) {
        self->s.effects &= ~EF_ANIM01;  // Turn off
    } else {
        self->s.effects |= EF_ANIM01;   // Turn on
    }
    
    self->nextthink = level.time + 0.5;  // Think again in 0.5 seconds
}
```

The game loop calls think functions:

```cpp
void G_RunFrame(void) {
    // ... player input processing ...
    
    // Run entity think functions
    for (i = 1; i < globals.num_edicts; i++) {
        edict_t *ent = &g_edicts[i];
        
        if (!ent->inuse)
            continue;
            
        // Time to think?
        if (ent->nextthink && ent->nextthink <= level.time) {
            ent->nextthink = 0;
            if (ent->think)
                ent->think(ent);
        }
    }
    
    // ... physics, collision detection ...
}
```

### 4. Touching

When entities collide, their touch functions are called:

```cpp
void Touch_Item(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf) {
    // Only players can pick up items
    if (!other->client)
        return;
        
    // Try to pick up the item
    if (!Pickup_Item(ent, other))
        return;  // Can't pick up (inventory full, etc.)
        
    // Play pickup sound
    gi.sound(other, CHAN_ITEM, gi.soundindex(ent->item->pickup_sound), 
             1, ATTN_NORM, 0);
             
    // Remove or respawn
    if (ent->spawnflags & SPAWNFLAG_ITEM_DROPPED) {
        G_FreeEdict(ent);  // Dropped items disappear
    } else {
        SetRespawn(ent, 30);  // Map items respawn after 30 seconds
    }
}
```

### 5. Using

Entities can be triggered by other entities:

```cpp
void door_use(edict_t *self, edict_t *other, edict_t *activator) {
    // Is door already moving?
    if (self->moveinfo.state == STATE_UP || 
        self->moveinfo.state == STATE_TOP) {
        return;  // Already open or opening
    }
    
    // Open the door
    self->moveinfo.state = STATE_UP;
    
    if (self->sounds)
        gi.sound(self, CHAN_NO_PHS_ADD+CHAN_VOICE, 
                 self->moveinfo.sound_start, 1, ATTN_NORM, 0);
                 
    Move_Calc(self, self->moveinfo.end_origin, door_hit_top);
}
```

Entities with a `targetname` can be triggered by entities targeting them:

```cpp
// In map: button targets door
{
"classname" "func_button"
"target" "door1"
}
{
"classname" "func_door"  
"targetname" "door1"
}

// When button is used:
void button_use(edict_t *self, edict_t *other, edict_t *activator) {
    // Activate targets
    G_UseTargets(self, activator);
}

void G_UseTargets(edict_t *ent, edict_t *activator) {
    // Find all entities with matching targetname
    edict_t *t = G_Find(NULL, FOFS(targetname), ent->target);
    while (t) {
        if (t->use)
            t->use(t, ent, activator);
        t = G_Find(t, FOFS(targetname), ent->target);
    }
}
```

### 6. Taking Damage

Entities can be damaged:

```cpp
void T_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker, 
              vec3_t dir, vec3_t point, vec3_t normal, int damage, 
              int knockback, int dflags, int mod) {
    
    // Can this entity take damage?
    if (!targ->takedamage)
        return;
        
    // Apply damage modifiers (armor, difficulty, etc.)
    damage = ModifyDamage(targ, attacker, damage, dflags);
    
    // Apply damage
    targ->health -= damage;
    
    // Call pain function
    if (targ->pain)
        targ->pain(targ, attacker, knockback, damage);
        
    // Died?
    if (targ->health <= 0) {
        Killed(targ, inflictor, attacker, damage, point);
    }
}

void player_die(edict_t *self, edict_t *inflictor, edict_t *attacker, 
                int damage, vec3_t point) {
    // Drop weapon
    Drop_Weapon(self);
    
    // Set death animation
    self->s.frame = FRAME_death01;
    self->takedamage = DAMAGE_NO;
    
    // Become a corpse
    self->movetype = MOVETYPE_TOSS;
    self->deadflag = DEAD_DEAD;
    
    // Respawn after delay
    self->think = player_respawn;
    self->nextthink = level.time + 3.0;
}
```

### 7. Freeing

Entities are freed when no longer needed:

```cpp
void G_FreeEdict(edict_t *ed) {
    // Unlink from world
    gi.unlinkentity(ed);
    
    // Clear callbacks to prevent dangling pointers
    memset(ed, 0, sizeof(*ed));
    ed->classname = "freed";
    ed->inuse = false;
    ed->freetime = level.time;
}
```

## Entity State vs. Entity Data

### entity_state_t (Networked)

The `s` field of edict_t is `entity_state_t` - data sent to clients:

```cpp
typedef struct entity_state_s {
    int     number;         // Entity index
    
    vec3_t  origin;         // Position
    vec3_t  angles;         // Rotation
    vec3_t  old_origin;     // For lerping
    
    int     modelindex;     // Model to render
    int     modelindex2, modelindex3, modelindex4;  // Additional models
    
    int     frame;          // Animation frame
    int     skinnum;        // Skin/texture
    
    uint32_t    effects;    // EF_* flags (rotating, teleport, etc.)
    int     renderfx;       // Rendering flags
    int     solid;          // Bounding box for prediction
    
    int     sound;          // Looping sound
    int     event;          // One-time event (muzzle flash, footstep)
} entity_state_t;
```

**Only this data is visible to clients!** If you want something to appear on screen, it must be in `entity_state_t`.

### Server-Only Data

Most edict_t fields are server-only:
- `health` - Clients don't need to know enemy health
- `enemy` - AI state is server-side
- `think` - Callbacks can't be sent over network
- `velocity` - Often server-only (though sometimes inferred client-side)

This separation:
- Reduces bandwidth (only essential data is sent)
- Prevents cheating (can't see enemy health, AI plans, etc.)
- Maintains server authority (client can't modify server state)

## Entity Relationships

### Targeting

Entities can trigger other entities through the target/targetname system:

```cpp
// Trigger entity
ent->target = "door1";

// Target entity  
ent->targetname = "door1";

// When trigger activates:
G_UseTargets(trigger, activator);  // Finds and activates "door1"
```

### Chaining

Multi-part entities (e.g., trains) use chains:

```cpp
// Master entity
train->teammaster = train;
train->teamchain = part1;

// Chain parts
part1->teammaster = train;
part1->teamchain = part2;

part2->teammaster = train;
part2->teamchain = NULL;

// Moving the master moves all chained parts
```

### Ground Entity

Entities track what they're standing on:

```cpp
// Set by physics system
player->groundentity = floor_entity;

// Used for:
// - Jump detection (can only jump when grounded)
// - Friction (grounded entities have ground friction)
// - Moving platforms (inherit velocity from groundentity)
```

### AI Relationships

Monsters track targets and allies:

```cpp
monster->enemy = player;           // Current attack target
monster->movetarget = path_corner; // Patrol destination
monster->goalentity = player;      // Overall goal
monster->oldenemy = NULL;          // Previous enemy (for returning to patrol)
```

## Common Entity Patterns

### Triggerable Entity

```cpp
void SP_trigger_example(edict_t *self) {
    self->use = trigger_example_use;
    self->svflags |= SVF_NOCLIENT;  // Invisible
    gi.linkentity(self);
}

void trigger_example_use(edict_t *self, edict_t *other, edict_t *activator) {
    // Do something when triggered
    gi.dprintf("Triggered by %s\n", activator->client->pers.netname);
    
    // Trigger targets
    G_UseTargets(self, activator);
}
```

### Thinking Entity

```cpp
void SP_rotating_light(edict_t *self) {
    self->think = rotating_light_think;
    self->nextthink = level.time + FRAMETIME;
    gi.linkentity(self);
}

void rotating_light_think(edict_t *self) {
    self->s.angles[YAW] += 5;  // Rotate 5 degrees
    if (self->s.angles[YAW] >= 360)
        self->s.angles[YAW] -= 360;
        
    self->nextthink = level.time + FRAMETIME;  // Think every frame
}
```

### Touch Trigger

```cpp
void SP_trigger_hurt(edict_t *self) {
    self->solid = SOLID_TRIGGER;
    self->movetype = MOVETYPE_NONE;
    self->touch = hurt_touch;
    
    gi.setmodel(self, self->model);  // Use brush model from map
    gi.linkentity(self);
}

void hurt_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf) {
    // Only damage players
    if (!other->client)
        return;
        
    // Damage
    T_Damage(other, self, self, vec3_origin, other->s.origin, 
             vec3_origin, self->dmg, 0, 0, MOD_TRIGGER_HURT);
}
```

## Differences from Q2RTXPerimental

Q2RTXPerimental modernizes this system:

**Vanilla Quake 2:**
- C-style structs with function pointers
- Callback-based behavior
- Manual memory management
- All entity data in one flat structure

**Q2RTXPerimental:**
- C++ classes with virtual methods
- OOP inheritance hierarchy (`svg_base_edict_t` base class)
- Automatic memory management (smart pointers where appropriate)
- Separated concerns (base class, derived classes for specific entity types)
- Type-safe entity system with compile-time checks
- Enhanced save/load with automatic serialization

**Example Comparison:**

Vanilla:
```c
void door_use(edict_t *self, edict_t *other, edict_t *activator) {
    // ...
}

ent->use = door_use;
```

Q2RTXPerimental:
```cpp
class svg_func_door_t : public svg_base_edict_t {
    virtual void Use(svg_base_edict_t *other, svg_base_edict_t *activator) override {
        // ...
    }
};
```

---

**Next Topics:**
- [Vanilla Physics](Vanilla-Physics.md) - Movement and collision detection
- [Vanilla Networking](Vanilla-Networking.md) - How entity state is synchronized
- [Entity System Overview](Entity-System-Overview.md) - Q2RTXPerimental's enhanced entity system
