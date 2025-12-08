# Creating Custom Entities Tutorial

This comprehensive tutorial walks you through creating custom entities in Q2RTXPerimental, from basic concepts to advanced features.

## Prerequisites

Before starting, ensure you:
- Have built Q2RTXPerimental successfully
- Understand C++ basics (classes, inheritance, virtual methods)
- Have read [Entity System Overview](Entity-System-Overview.md)
- Know how to use a map editor (TrenchBroom recommended)

## Tutorial Structure

We'll create three entities of increasing complexity:
1. **Simple Decorative Entity** - Static model with effects
2. **Interactive Button** - Player interaction and signals
3. **Animated Monster** - Full AI, animation, combat

## Part 1: Simple Decorative Entity

Let's create a glowing crystal that rotates and pulses.

### Step 1: Create Header File

Create `src/baseq2rtxp/svgame/entities/misc/svg_misc_crystal.h`:

```cpp
#pragma once

#include "svgame/entities/svg_base_edict.h"

/**
 * @brief A decorative glowing crystal
 * 
 * Spawns a rotating crystal model with pulsing light effect.
 * Demonstrates basic entity creation and thinking.
 */
class svg_misc_crystal_t : public svg_base_edict_t {
public:
    // Type registration
    DefineClass(
        "misc_crystal",              // Classname for maps
        svg_misc_crystal_t,          // This class
        svg_base_edict_t,            // Parent class
        EdictTypeInfo::TypeInfoFlag_GameSpawn
    );
    
    // Constructor/Destructor
    svg_misc_crystal_t() = default;
    virtual ~svg_misc_crystal_t() = default;
    
    // Virtual methods
    virtual void Spawn() override;
    virtual void Think() override;
    
private:
    // Custom properties
    float pulseSpeed = 2.0f;     // Cycles per second
    float baseLight = 200.0f;    // Base light intensity
    float pulseAmount = 100.0f;  // Light variation
    
    bool isPulsing = true;       // Enable/disable pulsing
};

// Register with spawn system
DECLARE_EDICT_SPAWN_INFO(misc_crystal, svg_misc_crystal_t);
```

### Step 2: Create Implementation File

Create `src/baseq2rtxp/svgame/entities/misc/svg_misc_crystal.cpp`:

```cpp
#include "svg_misc_crystal.h"
#include "svgame/svg_main.h"

// Register spawn function
DEFINE_EDICT_SPAWN_INFO(misc_crystal, svg_misc_crystal_t);

/**
 * @brief Spawn the crystal entity
 */
void svg_misc_crystal_t::Spawn() {
    // Call base class first
    svg_base_edict_t::Spawn();
    
    // Set entity properties
    s.entityType = ET_GENERAL;
    solid = SOLID_BBOX;  // Solid bounding box
    movetype = MOVETYPE_NONE;  // Doesn't move
    
    // Set bounding box (for collision)
    VectorSet(mins, -16, -16, 0);
    VectorSet(maxs, 16, 16, 48);
    
    // Read spawn parameters from entity dictionary
    // These come from key-value pairs in the map editor
    if (entityDictionary.contains("light")) {
        baseLight = std::stof(entityDictionary["light"]);
    }
    if (entityDictionary.contains("speed")) {
        pulseSpeed = std::stof(entityDictionary["speed"]);
    }
    if (entityDictionary.contains("amount")) {
        pulseAmount = std::stof(entityDictionary["amount"]);
    }
    
    // Set model
    const char *modelName = "models/objects/crystal/tris.md2";
    if (entityDictionary.contains("model")) {
        modelName = entityDictionary["model"].c_str();
    }
    gi.SetModel(edict, modelName);
    
    // Set visual effects
    s.effects |= EF_ROTATE;  // Rotate continuously
    s.renderfx |= RF_GLOW;   // Glow effect
    
    // Link into world (makes it solid and visible)
    gi.LinkEntity(edict);
    
    // Set up think callback for pulsing
    if (isPulsing) {
        SetThinkCallback(&svg_misc_crystal_t::Think);
        nextThinkTime = level.time + FRAMETIME;
    }
    
    gi.dprintf("misc_crystal spawned at (%.1f, %.1f, %.1f)\n",
               s.origin[0], s.origin[1], s.origin[2]);
}

/**
 * @brief Update pulsing effect
 */
void svg_misc_crystal_t::Think() {
    if (!isPulsing) {
        return;
    }
    
    // Calculate pulsing light intensity using sine wave
    float time = level.time.count() * pulseSpeed;
    float pulse = std::sin(time * M_PI * 2.0f);
    float currentLight = baseLight + (pulse * pulseAmount);
    
    // Update light_level for client
    light_level = currentLight;
    
    // Optional: Change visual effects based on intensity
    if (currentLight > baseLight) {
        s.renderfx |= RF_GLOW;
    } else {
        s.renderfx &= ~RF_GLOW;
    }
    
    // Schedule next think (every frame for smooth pulsing)
    nextThinkTime = level.time + FRAMETIME;
}
```

### Step 3: Add to Build System

Edit `src/baseq2rtxp/svgame/CMakeLists.txt` (or appropriate CMake file):

```cmake
set(SVGAME_SOURCES
    # ... existing files ...
    
    # Add our new files
    entities/misc/svg_misc_crystal.cpp
    entities/misc/svg_misc_crystal.h
    
    # ... rest of files ...
)
```

### Step 4: Build and Test

```bash
cd build
cmake --build . --config Release
```

### Step 5: Use in Map

In your map editor (TrenchBroom), create a new entity with:
- **classname**: `misc_crystal`
- **origin**: `0 0 0` (or desired position)
- **light**: `200` (optional, default is 200)
- **speed**: `2.0` (optional, pulses per second)
- **amount**: `100` (optional, pulse intensity)
- **model**: `models/objects/crystal/tris.md2` (optional)

## Part 2: Interactive Button Entity

Now let's create a button that players can press, which triggers other entities.

### Header File

Create `src/baseq2rtxp/svgame/entities/func/svg_func_button.h`:

```cpp
#pragma once

#include "svgame/entities/svg_base_edict.h"

/**
 * @brief Interactive button entity
 * 
 * Players can press the button (USE key) to trigger targets.
 * Demonstrates Use callback, UseTargets, and state management.
 */
class svg_func_button_t : public svg_base_edict_t {
public:
    DefineClass(
        "func_button",
        svg_func_button_t,
        svg_base_edict_t,
        EdictTypeInfo::TypeInfoFlag_GameSpawn
    );
    
    svg_func_button_t() = default;
    virtual ~svg_func_button_t() = default;
    
    // Virtual methods
    virtual void Spawn() override;
    virtual void Use(svg_base_edict_t *other, svg_base_edict_t *activator,
                     entity_usetarget_type_t useType, int32_t useValue) override;
    virtual void Think() override;
    
    // UseTargets support
    virtual UseTargetHint GetUseTargetHint() override {
        if (state == STATE_IDLE) {
            return UseTargetHint::PRESSABLE;  // Shows "Press E" hint
        }
        return UseTargetHint::NONE;
    }
    
    virtual void OnUseTargetPressed(svg_base_edict_t *user) override {
        Use(this, user, USETARGET_TYPE_PRESS, 0);
    }
    
private:
    // Button states
    enum ButtonState {
        STATE_IDLE,      // Waiting to be pressed
        STATE_PRESSED,   // Currently pressed
        STATE_RETURNING  // Returning to idle
    };
    
    ButtonState state = STATE_IDLE;
    
    // Properties
    float wait = 1.0f;         // Time before returning
    vec3_t startOrigin;        // Original position
    vec3_t pressedOrigin;      // Position when pressed
    float pressDistance = 4.0f; // How far button moves
    
    // Methods
    void StartPress();
    void FinishPress();
    void StartReturn();
    void FinishReturn();
};

DECLARE_EDICT_SPAWN_INFO(func_button, svg_func_button_t);
```

### Implementation File

Create `src/baseq2rtxp/svgame/entities/func/svg_func_button.cpp`:

```cpp
#include "svg_func_button.h"
#include "svgame/svg_main.h"

DEFINE_EDICT_SPAWN_INFO(func_button, svg_func_button_t);

void svg_func_button_t::Spawn() {
    svg_base_edict_t::Spawn();
    
    // Set up as brush entity
    s.entityType = ET_PUSHER;
    solid = SOLID_BSP;
    movetype = MOVETYPE_PUSH;
    
    // Load brush model from map
    gi.SetModel(edict, entityDictionary->model);
    
    // Read properties
    wait = GetEntityDictValue<float>("wait", 1.0f);
    pressDistance = GetEntityDictValue<float>("lip", 4.0f);
    
    // Store start position
    startOrigin = s.origin;
    
    // Calculate pressed position (moves in -normal direction)
    vec3_t moveDir = {0, 0, -1};  // Default: down
    if (entityDictionary.contains("angle")) {
        float angle = std::stof(entityDictionary["angle"]);
        AngleVectors(vec3_t{0, angle, 0}, moveDir, nullptr, nullptr);
    }
    pressedOrigin = startOrigin + (moveDir * pressDistance);
    
    // Set up sound
    if (entityDictionary.contains("sounds")) {
        int soundType = std::stoi(entityDictionary["sounds"]);
        // Load appropriate sound based on soundType
    }
    
    // Link into world
    gi.LinkEntity(edict);
    
    state = STATE_IDLE;
    
    gi.dprintf("func_button spawned, target='%s'\n",
               entityDictionary.contains("target") ? 
               entityDictionary["target"].c_str() : "(none)");
}

void svg_func_button_t::Use(svg_base_edict_t *other, svg_base_edict_t *activator,
                             entity_usetarget_type_t useType, int32_t useValue) {
    // Can only be pressed when idle
    if (state != STATE_IDLE) {
        return;
    }
    
    gi.dprintf("Button pressed by %s\n", 
               activator->client ? activator->client->pers.netname : activator->classname);
    
    // Start pressing
    StartPress();
    
    // Play activation sound
    gi.sound(edict, CHAN_VOICE, gi.soundindex("switches/butn2.wav"), 
             1.0f, ATTN_STATIC, 0);
    
    // Trigger targets
    UseTargets(activator);
    
    // Emit signal for advanced entity communication
    EmitSignal("OnPressed", this, activator);
}

void svg_func_button_t::StartPress() {
    state = STATE_PRESSED;
    
    // Move button to pressed position
    s.origin = pressedOrigin;
    gi.LinkEntity(edict);
    
    // Set up think to return after wait time
    SetThinkCallback(&svg_func_button_t::FinishPress);
    nextThinkTime = level.time + gametime_t::from_sec(wait);
}

void svg_func_button_t::FinishPress() {
    // Start returning to original position
    StartReturn();
}

void svg_func_button_t::StartReturn() {
    state = STATE_RETURNING;
    
    // Animate return (for smooth movement, use velocity)
    // For simplicity, we'll just snap back
    s.origin = startOrigin;
    gi.LinkEntity(edict);
    
    // Small delay before becoming usable again
    SetThinkCallback(&svg_func_button_t::FinishReturn);
    nextThinkTime = level.time + gametime_t::from_msec(100);
}

void svg_func_button_t::FinishReturn() {
    state = STATE_IDLE;
    // Ready to be pressed again
}

void svg_func_button_t::Think() {
    // Think callback will be set by state transition functions
    // No general think behavior needed
}
```

### Usage in Map

In your map editor:
- **classname**: `func_button`
- Create a brush and texture it
- **target**: `door1` (name of entity to trigger)
- **wait**: `1.0` (seconds before returning)
- **lip**: `4` (movement distance)
- **sounds**: `1` (optional, sound type)

## Part 3: Simple AI Monster

Finally, let's create a basic monster with AI.

### Header File (Simplified)

Create `src/baseq2rtxp/svgame/entities/monster/svg_monster_grunt.h`:

```cpp
#pragma once

#include "svgame/entities/svg_base_edict.h"

/**
 * @brief Basic grunt monster
 * 
 * Demonstrates AI, animation, and combat.
 * Patrols, detects enemies, attacks, and can be killed.
 */
class svg_monster_grunt_t : public svg_base_edict_t {
public:
    DefineClass(
        "monster_grunt",
        svg_monster_grunt_t,
        svg_base_edict_t,
        EdictTypeInfo::TypeInfoFlag_GameSpawn
    );
    
    svg_monster_grunt_t() = default;
    virtual ~svg_monster_grunt_t() = default;
    
    // Virtual methods
    virtual void Spawn() override;
    virtual void Think() override;
    virtual void Pain(svg_base_edict_t *attacker, float kick, int damage) override;
    virtual void Die(svg_base_edict_t *inflictor, svg_base_edict_t *attacker,
                     int damage, vec3_t point) override;
    
private:
    // AI states
    enum AIState {
        AI_IDLE,
        AI_PATROL,
        AI_CHASE,
        AI_ATTACK,
        AI_PAIN,
        AI_DEAD
    };
    
    AIState aiState = AI_IDLE;
    
    // Properties
    float sightRange = 1024.0f;
    float attackRange = 512.0f;
    float attackDamage = 10.0f;
    float attackDelay = 1.0f;
    
    gametime_t nextAttackTime;
    
    // AI methods
    void IdleAI();
    void ChaseAI();
    void AttackAI();
    
    bool FindEnemy();
    bool CanSeeEnemy();
    void FaceEnemy();
    void MoveToward(vec3_t target);
    void FireWeapon();
};

DECLARE_EDICT_SPAWN_INFO(monster_grunt, svg_monster_grunt_t);
```

### Implementation (Core Parts)

```cpp
#include "svg_monster_grunt.h"
#include "svgame/svg_main.h"

DEFINE_EDICT_SPAWN_INFO(monster_grunt, svg_monster_grunt_t);

void svg_monster_grunt_t::Spawn() {
    svg_base_edict_t::Spawn();
    
    // Set up as monster
    s.entityType = ET_MONSTER;
    solid = SOLID_BBOX;
    movetype = MOVETYPE_STEP;  // Can step over obstacles
    
    // Set bounding box
    VectorSet(mins, -16, -16, -24);
    VectorSet(maxs, 16, 16, 32);
    
    // Set health
    health = 100;
    max_health = 100;
    gib_health = -30;
    takedamage = DAMAGE_YES;
    
    // Load model
    gi.SetModel(edict, "models/monsters/grunt/tris.md2");
    
    // Set up AI
    aiState = AI_IDLE;
    
    // Link and start thinking
    gi.LinkEntity(edict);
    SetThinkCallback(&svg_monster_grunt_t::Think);
    nextThinkTime = level.time + FRAMETIME;
}

void svg_monster_grunt_t::Think() {
    // AI state machine
    switch (aiState) {
        case AI_IDLE:
            IdleAI();
            break;
        case AI_CHASE:
            ChaseAI();
            break;
        case AI_ATTACK:
            AttackAI();
            break;
    }
    
    // Continue thinking
    nextThinkTime = level.time + FRAMETIME;
}

void svg_monster_grunt_t::IdleAI() {
    // Look for enemies
    if (FindEnemy()) {
        aiState = AI_CHASE;
        gi.sound(edict, CHAN_VOICE, gi.soundindex("grunt/sight.wav"),
                 1.0f, ATTN_NORM, 0);
    }
}

void svg_monster_grunt_t::ChaseAI() {
    if (!enemy || !CanSeeEnemy()) {
        // Lost enemy
        enemy = nullptr;
        aiState = AI_IDLE;
        return;
    }
    
    // Face enemy
    FaceEnemy();
    
    // Check attack range
    float dist = VectorDistance(s.origin, enemy->s.origin);
    if (dist < attackRange) {
        aiState = AI_ATTACK;
        nextAttackTime = level.time;
    } else {
        // Move toward enemy
        MoveToward(enemy->s.origin);
    }
}

void svg_monster_grunt_t::AttackAI() {
    if (!enemy || !CanSeeEnemy()) {
        enemy = nullptr;
        aiState = AI_IDLE;
        return;
    }
    
    FaceEnemy();
    
    // Check if out of range
    float dist = VectorDistance(s.origin, enemy->s.origin);
    if (dist > attackRange * 1.5f) {
        aiState = AI_CHASE;
        return;
    }
    
    // Attack when delay expires
    if (level.time >= nextAttackTime) {
        FireWeapon();
        nextAttackTime = level.time + gametime_t::from_sec(attackDelay);
    }
}

bool svg_monster_grunt_t::FindEnemy() {
    // Simple enemy detection: find closest player in sight
    svg_base_edict_t *closest = nullptr;
    float closestDist = sightRange;
    
    for (int i = 1; i <= maxclients->value; i++) {
        svg_base_edict_t *player = GetEntityByIndex(i);
        if (!player->inuse || !player->client)
            continue;
            
        float dist = VectorDistance(s.origin, player->s.origin);
        if (dist < closestDist) {
            // Check line of sight
            cm_trace_t tr = SVG_Trace(s.origin, vec3_origin, vec3_origin,
                                       player->s.origin, this, MASK_OPAQUE);
            if (tr.fraction == 1.0f) {
                closest = player;
                closestDist = dist;
            }
        }
    }
    
    if (closest) {
        enemy = closest;
        return true;
    }
    return false;
}

void svg_monster_grunt_t::FireWeapon() {
    // Play attack animation
    s.frame = FRAME_attack01;  // Start attack animation
    
    // Play sound
    gi.sound(edict, CHAN_WEAPON, gi.soundindex("grunt/attack.wav"),
             1.0f, ATTN_NORM, 0);
    
    // Fire bullet
    vec3_t start = s.origin + vec3_t{0, 0, 24};  // Shoot from chest height
    vec3_t dir;
    VectorSubtract(enemy->s.origin, start, dir);
    VectorNormalize(dir);
    
    cm_trace_t tr = SVG_Trace(start, vec3_origin, vec3_origin,
                               start + dir * attackRange,
                               this, MASK_SHOT);
    
    if (tr.fraction < 1.0f && tr.ent == enemy) {
        // Hit enemy!
        SVG_Damage(enemy, this, this, dir, tr.endpos, tr.plane.normal,
                   attackDamage, 0, DAMAGE_BULLET, MOD_BLASTER);
    }
    
    // Spawn muzzle flash
    gi.WriteByte(svc_muzzleflash);
    gi.WriteShort(s.number);
    gi.WriteByte(MZ_BLASTER);
    gi.multicast(s.origin, MULTICAST_PVS);
}

void svg_monster_grunt_t::Pain(svg_base_edict_t *attacker, float kick, int damage) {
    // Play pain sound
    gi.sound(edict, CHAN_VOICE, gi.soundindex("grunt/pain.wav"),
             1.0f, ATTN_NORM, 0);
    
    // If we weren't aware of attacker, target them
    if (!enemy) {
        enemy = attacker;
        aiState = AI_CHASE;
    }
}

void svg_monster_grunt_t::Die(svg_base_edict_t *inflictor, svg_base_edict_t *attacker,
                               int damage, vec3_t point) {
    // Play death sound
    gi.sound(edict, CHAN_VOICE, gi.soundindex("grunt/death.wav"),
             1.0f, ATTN_NORM, 0);
    
    // Check for gibbing
    if (health < gib_health) {
        // Gib!
        ThrowGibs(damage);
        SVG_FreeEntity(this);
        return;
    }
    
    // Death animation
    s.frame = FRAME_death01;
    s.entityType = ET_MONSTER_CORPSE;
    
    // Become non-solid
    solid = SOLID_NOT;
    movetype = MOVETYPE_TOSS;
    takedamage = DAMAGE_NO;
    aiState = AI_DEAD;
    
    // Remove after 30 seconds
    SetThinkCallback(nullptr);
    nextThinkTime = gametime_t::zero();
    
    // Schedule corpse removal
    think_frame = level.frameNumber + (30 * 40);  // 30 seconds at 40 Hz
}
```

## Testing Your Entities

### Console Commands

```
// Spawn entity at player location
spawn misc_crystal
spawn func_button
spawn monster_grunt

// Give yourself items
give all
god

// Debug entity
ent_info <entity_number>
```

### Map Testing

1. Create a test map in TrenchBroom
2. Place your entities
3. Compile the map
4. Load in game: `map yourmap`

## Common Pitfalls

### 1. Forgetting to Call Base Class

```cpp
// BAD
void Spawn() {
    s.entityType = ET_MONSTER;
    // Missing base class call!
}

// GOOD
void Spawn() {
    svg_base_edict_t::Spawn();  // Always call first!
    s.entityType = ET_MONSTER;
}
```

### 2. Not Linking Entity

```cpp
// BAD
void Spawn() {
    s.origin = {100, 200, 50};
    // Entity not linked - won't appear!
}

// GOOD
void Spawn() {
    s.origin = {100, 200, 50};
    gi.LinkEntity(edict);  // Make it solid/visible
}
```

### 3. Wrong Movetype/Solid Combo

```cpp
// BAD: Trigger that blocks movement
solid = SOLID_BBOX;
movetype = MOVETYPE_NONE;

// GOOD: Trigger that doesn't block
solid = SOLID_TRIGGER;
movetype = MOVETYPE_NONE;
```

### 4. Think Without NextThink

```cpp
// BAD: Think never called again
void Think() {
    DoSomething();
    // Missing nextThinkTime!
}

// GOOD
void Think() {
    DoSomething();
    nextThinkTime = level.time + FRAMETIME;
}
```

## Next Steps

- [Entity Base Class Reference](Entity-Base-Class-Reference.md) - Complete API
- [Entity Lifecycle](Entity-Lifecycle.md) - Detailed lifecycle
- [Entity Networking](Entity-Networking.md) - How entities sync to clients
- [Signal I/O System](Signal-IO-System.md) - Advanced entity communication
- [Server Game Module](Server-Game-Module.md) - Server architecture

---

**Congratulations!** You've created your first custom entities. Now experiment and create your own unique gameplay!
