# Entity Lifecycle

Understanding the entity lifecycle is crucial for creating custom entities in Q2RTXPerimental. This guide explains how entities are spawned, updated, and destroyed.

## Lifecycle Overview

```
Map Load → Spawn → Think Loop → Death → Removal
```

## Detailed Lifecycle Stages

### 1. Entity Dictionary Creation

**When:** During map load, before entity spawning

**What Happens:**
- Engine parses `.bsp` file entity lump
- Creates `cm_entity_t` dictionary for each entity
- Dictionary contains key-value pairs from map editor

**Example Dictionary:**
```
{
"classname" "monster_soldier"
"origin" "128 256 64"
"angle" "90"
"spawnflags" "1"
"target" "target_1"
}
```

**Code:**
```cpp
// Engine creates entity dictionary during map load
cm_entity_t *entities = CM_EntityDictionaries();
for (int i = 0; i < num_entities; i++) {
    cm_entity_t *ed = &entities[i];
    // Dictionary ready for spawning
}
```

### 2. Entity Allocation

**When:** During `SVG_InitEntities()` or dynamic spawning

**What Happens:**
- Server allocates entity slot from pool
- Calls C++ constructor
- Assigns entity number
- Stores entity dictionary reference

**Code:**
```cpp
// Allocate entity of specific type
auto *monster = SVG_Spawn<svg_monster_soldier_t>();
monster->s.number = entity_index;
monster->entityDictionary = ed;  // Store dictionary
```

### 3. Entity Spawning

**When:** Immediately after allocation

**Phases:**
1. **PreSpawn**: Setup phase
2. **Spawn**: Main initialization (virtual method)
3. **PostSpawn**: Finalization phase

#### PreSpawn Phase

```cpp
void svg_base_edict_t::DispatchPreSpawnCallback() {
    // Called before Spawn()
    // Setup basic properties
    if (PreSpawn) {
        PreSpawn(this);
    }
}
```

**Typical PreSpawn Actions:**
- Parse entity dictionary keys
- Set default values
- Initialize member variables

#### Spawn Phase

```cpp
void svg_monster_soldier_t::Spawn() {
    // Main spawning logic
    
    // Set model
    gi.SetModel(edict, "models/monsters/soldier/tris.md2");
    
    // Set bounding box
    mins = {-16, -16, -24};
    maxs = {16, 16, 32};
    
    // Set physics properties
    movetype = MOVETYPE_STEP;
    solid = SOLID_BBOX;
    
    // Set health
    health = 50;
    max_health = 50;
    
    // Set callbacks
    SetThinkCallback(&svg_monster_soldier_t::AI_Think);
    SetTouchCallback(&svg_monster_soldier_t::Touch);
    SetDieCallback(&svg_monster_soldier_t::Die);
    
    // Link into world
    gi.linkentity(edict);
    
    // Schedule first think
    nextthink = level.time + FRAMETIME;
}
```

**Typical Spawn Actions:**
- Load model/skin
- Set bounding box
- Configure physics (movetype, solid)
- Initialize health/armor
- Set callbacks (Think, Touch, Use, Die, etc.)
- Link entity into world
- Schedule first think

#### PostSpawn Phase

```cpp
void svg_base_edict_t::DispatchPostSpawnCallback() {
    // Called after Spawn()
    // Finalization
    if (PostSpawn) {
        PostSpawn(this);
    }
}
```

**Typical PostSpawn Actions:**
- Find target entities
- Establish entity relationships
- Verify spawn conditions
- Remove if spawn fails

### 4. Think Loop

**When:** Every frame while `nextthink <= level.time`

**Frequency:** Typically 40 Hz (every 0.025 seconds)

**Flow:**
```
PreThink → Think → PostThink → (repeat)
```

#### PreThink

```cpp
void svg_base_edict_t::DispatchPreThinkCallback() {
    // Before main think
    if (PreThink) {
        PreThink(this);
    }
}
```

**Use Cases:**
- Player input processing
- Prediction preparation

#### Think

```cpp
void svg_monster_soldier_t::Think() {
    // Main AI logic
    
    // Check if alive
    if (health <= 0) {
        return;  // Dead, no thinking
    }
    
    // AI decision making
    if (enemy && CanSeeEnemy()) {
        AttackEnemy();
    } else {
        PatrolArea();
    }
    
    // Schedule next think
    nextthink = level.time + FRAMETIME;
}
```

**Typical Think Actions:**
- AI decision making
- Physics simulation
- Animation updates
- Attack/defense logic
- State machine updates
- Schedule next think

#### PostThink

```cpp
void svg_base_edict_t::DispatchPostThinkCallback() {
    // After main think
    if (PostThink) {
        PostThink(this);
    }
}
```

**Use Cases:**
- Finalize player state
- Update view position
- Apply final adjustments

### 5. Interaction Callbacks

These callbacks can occur at any time during the entity's life:

#### Touch

**When:** Entity collides with another entity

```cpp
void svg_trigger_multiple_t::Touch(svg_base_edict_t *other, 
                                    const cm_plane_t *plane, 
                                    cm_surface_t *surf) {
    // Check if player touched trigger
    if (!other->client) {
        return;  // Not a player
    }
    
    // Check cooldown
    if (timestamp > level.time) {
        return;  // Too soon
    }
    
    // Trigger activation
    UseTargets(other, other);
    
    // Set cooldown
    timestamp = level.time + wait;
}
```

#### Use

**When:** Player presses use key on entity

```cpp
void svg_func_button_t::Use(svg_base_edict_t *other, 
                             svg_base_edict_t *activator,
                             entity_usetarget_type_t useType,
                             int32_t useValue) {
    // Button activation
    if (health > 0 && !activator->client) {
        return;  // Only players can use
    }
    
    // Play button sound
    gi.sound(edict, CHAN_VOICE, moveinfo.sound_start, 1, ATTN_NORM, 0);
    
    // Move button
    Button_Move();
    
    // Fire targets
    UseTargets(activator, activator);
}
```

#### Pain

**When:** Entity takes damage (but doesn't die)

```cpp
void svg_monster_soldier_t::Pain(svg_base_edict_t *other, 
                                  float kick, 
                                  int32_t damage,
                                  entity_damageflags_t damageFlags) {
    // Play pain sound
    gi.sound(edict, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);
    
    // Play pain animation
    SetAnimation(ANIM_PAIN);
    
    // If not alerted, become alerted
    if (!enemy) {
        enemy = other;
    }
}
```

#### Blocked

**When:** Moving entity is blocked by another entity

```cpp
void svg_func_door_t::Blocked(svg_base_edict_t *other) {
    // Damage blocker
    if (dmg) {
        T_Damage(other, this, this, vec3_zero(), other->s.origin,
                 vec3_zero(), dmg, 1, 0, MEANS_OF_DEATH_CRUSHED);
    }
    
    // Reverse door direction
    if (moveinfo.state == STATE_DOWN) {
        Door_GoUp(moveinfo.start_origin);
    } else {
        Door_GoDown();
    }
}
```

### 6. Death

**When:** Entity's health drops to 0 or below

**Process:**
```
Damage → Health <= 0 → Die Callback → Cleanup
```

```cpp
void svg_monster_soldier_t::Die(svg_base_edict_t *inflictor, 
                                 svg_base_edict_t *attacker, 
                                 int32_t damage, 
                                 Vector3 *point) {
    // Already dead?
    if (health <= GIB_HEALTH) {
        // Gib the corpse
        ThrowGibs(damage, GIB_ORGANIC);
        return;
    }
    
    // Play death sound
    gi.sound(edict, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
    
    // Change entity type to corpse
    s.entityType = ET_MONSTER_CORPSE;
    
    // Play death animation
    SetAnimation(ANIM_DEATH);
    
    // Stop thinking
    SetThinkCallback(nullptr);
    nextthink = 0;
    
    // Can no longer take damage
    takedamage = DAMAGE_NO;
    
    // Can walk through
    solid = SOLID_NOT;
    
    // Drop to floor
    movetype = MOVETYPE_TOSS;
    
    // Link changes
    gi.linkentity(edict);
}
```

**Typical Death Actions:**
- Play death sound
- Play death animation
- Change entity type (to corpse)
- Stop thinking
- Disable damage
- Change physics (become non-solid, toss physics)
- Spawn gibs (if extreme damage)
- Award points to killer

### 7. Removal

**When:** Entity is no longer needed

**Methods:**

#### Immediate Removal

```cpp
void Remove_Entity() {
    SVG_FreeEdict(entity);  // Immediately free
}
```

**Use Cases:**
- Trigger_once after activation
- Projectiles after impact
- Temporary spawned entities

#### Delayed Removal

```cpp
void svg_monster_soldier_t::Die(/*...*/) {
    // ... death logic ...
    
    // Remove corpse after 30 seconds
    SetThinkCallback(&svg_base_edict_t::SVG_FreeEdict_Think);
    nextthink = level.time + 30.0f;
}
```

**Use Cases:**
- Corpses (fade out after time)
- Temporary items
- Timed effects

#### Conditional Removal

```cpp
void Think() {
    // Check if off-screen and old
    if (level.time - spawn_time > 60.0f && !IsVisible()) {
        SVG_FreeEdict(this);
        return;
    }
    
    nextthink = level.time + 1.0f;
}
```

## Lifecycle State Diagram

```
┌─────────────┐
│ Map Loading │
└──────┬──────┘
       │
       v
┌─────────────────┐
│ Allocate Entity │
└────────┬────────┘
         │
         v
┌────────────────┐
│ PreSpawn()     │
└────────┬───────┘
         │
         v
┌────────────────┐
│ Spawn()        │  ← Main initialization
└────────┬───────┘
         │
         v
┌────────────────┐
│ PostSpawn()    │
└────────┬───────┘
         │
         v
    ┌────────────────┐
    │  Think Loop    │ ← Active gameplay
    │ ┌────────────┐ │
    │ │PreThink()  │ │
    │ │Think()     │ │◄──── Repeats every frame
    │ │PostThink() │ │      (while nextthink set)
    │ └────────────┘ │
    └────┬───────────┘
         │
         │ (callbacks during life)
         │
         v
┌────────────────────┐
│ Touch/Use/Pain/... │ ← Interaction events
└────────┬───────────┘
         │
         v
   ┌─────────────┐
   │ health <= 0?│
   └──────┬──────┘
          │ yes
          v
   ┌─────────────┐
   │   Die()     │ ← Death handler
   └──────┬──────┘
          │
          v
   ┌─────────────┐
   │  Remove     │ ← Cleanup
   └─────────────┘
```

## Best Practices

### ✅ Proper Initialization

```cpp
void svg_custom_entity_t::Spawn() {
    // ALWAYS call parent Spawn() first
    svg_base_edict_t::Spawn();
    
    // Set required fields
    classname = "custom_entity";
    health = 100;
    
    // ALWAYS link after changing position/size
    gi.linkentity(edict);
    
    // ALWAYS set nextthink if entity needs to think
    nextthink = level.time + FRAMETIME;
}
```

### ✅ Safe Think Implementation

```cpp
void Think() {
    // Check validity
    if (!inuse || health <= 0) {
        return;
    }
    
    // Do work...
    
    // ALWAYS reschedule if thinking should continue
    nextthink = level.time + FRAMETIME;
}
```

### ✅ Clean Death Handler

```cpp
void Die(/*...*/) {
    // Stop future thinks
    SetThinkCallback(nullptr);
    nextthink = 0;
    
    // Disable damage
    takedamage = DAMAGE_NO;
    
    // Change physics
    solid = SOLID_NOT;
    movetype = MOVETYPE_NONE;
    
    // Update visuals
    s.entityType = ET_CORPSE;
    
    // Link changes
    gi.linkentity(edict);
}
```

### ❌ Common Mistakes

**Forgetting to link:**
```cpp
// WRONG: Position changed but not linked
s.origin = new_position;
// Clients won't see the change!

// CORRECT:
s.origin = new_position;
gi.linkentity(edict);
```

**Not rescheduling think:**
```cpp
// WRONG: Think() called once then never again
void Think() {
    DoWork();
    // Missing: nextthink = level.time + FRAMETIME;
}
```

**Accessing freed entity:**
```cpp
// WRONG: Using entity after freeing
SVG_FreeEdict(target);
target->health = 0;  // CRASH! Entity is freed!
```

## Related Documentation

- [**Entity System Overview**](Entity-System-Overview.md) - Entity architecture
- [**Entity Base Class Reference**](Entity-Base-Class-Reference.md) - svg_base_edict_t details
- [**Creating Custom Entities**](Creating-Custom-Entities.md) - Step-by-step tutorial
- [**Entity Networking**](Entity-Networking.md) - How entity state is synchronized
- [**Server Game Module**](Server-Game-Module.md) - Server-side systems

## Summary

The entity lifecycle in Q2RTXPerimental follows this flow:

1. **Dictionary Creation**: Map load creates key-value pairs
2. **Allocation**: Entity slot allocated from pool
3. **Spawning**: PreSpawn → Spawn → PostSpawn
4. **Think Loop**: PreThink → Think → PostThink (repeats)
5. **Interaction**: Touch, Use, Pain, Blocked callbacks
6. **Death**: Die callback when health <= 0
7. **Removal**: Immediate or delayed cleanup

**Key Points:**
- Always call parent Spawn() first
- Always link after position/size changes
- Always reschedule nextthink if thinking should continue
- Stop thinking and disable damage in Die()
- Never access freed entities

Understanding this lifecycle is essential for creating robust custom entities that behave correctly throughout their existence in the game world.
