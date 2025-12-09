# UseTargets System

The UseTargets system allows entities to activate other entities through target/targetname relationships and player interaction.

**Location:** `src/baseq2rtxp/svgame/svg_usetargets.h` and player interaction code

## Overview

UseTargets enables:
- **Map entity targeting**: Buttons trigger doors, triggers activate spawners
- **Player interaction**: Use key (+use) to interact with entities
- **Entity chains**: One entity activates multiple others

## Target/Targetname System

### Basic Concept

```
[Button]             [Door]
target="door1" ----> targetname="door1"

Player presses button → Button activates door
```

### Map Editor Example

**Button entity:**
```
{
"classname" "func_button"
"target" "door1"
"origin" "128 256 64"
}
```

**Door entity:**
```
{
"classname" "func_door"
"targetname" "door1"
"origin" "256 256 64"
}
```

### Code Implementation

**Activating targets:**
```cpp
void svg_func_button_t::Use(svg_base_edict_t *other, 
                             svg_base_edict_t *activator, /*...*/) {
    // Button pressed, activate all entities with matching targetname
    UseTargets(activator, activator);
}
```

**UseTargets implementation:**
```cpp
void svg_base_edict_t::UseTargets(svg_base_edict_t *other,
                                   svg_base_edict_t *activator) {
    if (!target)
        return;  // No target set
    
    // Find all entities with matching targetname
    svg_base_edict_t *t = nullptr;
    while ((t = SVG_Find(t, FOFS(targetname), target)) != nullptr) {
        // Activate each matching entity
        t->Use(this, activator, ENTITY_USETARGET_TYPE_TOGGLE, 0);
    }
}
```

## Player Interaction

### Use Key (+use)

Players can interact with entities using the +use key (default: E).

**Making an entity interactable:**
```cpp
void svg_func_door_t::Spawn() {
    // ... door setup ...
    
    // Make door useable
    spawnflags |= SPAWNFLAG_USETARGET_PRESSABLE;
    
    // Set Use callback
    SetUseCallback(&svg_func_door_t::Use);
}

void svg_func_door_t::Use(svg_base_edict_t *other, 
                           svg_base_edict_t *activator, /*...*/) {
    // Player pressed use on door
    if (moveinfo.state == STATE_BOTTOM) {
        Door_GoUp();  // Open door
    } else if (moveinfo.state == STATE_TOP) {
        Door_GoDown();  // Close door
    }
}
```

### Use Target Types

```cpp
typedef enum entity_usetarget_type_e {
    ENTITY_USETARGET_TYPE_OFF,      // Turn off
    ENTITY_USETARGET_TYPE_ON,       // Turn on
    ENTITY_USETARGET_TYPE_SET,      // Set to specific value
    ENTITY_USETARGET_TYPE_TOGGLE    // Toggle on/off
} entity_usetarget_type_t;
```

**Usage:**
```cpp
void Use(svg_base_edict_t *other, svg_base_edict_t *activator,
         entity_usetarget_type_t useType, int32_t useValue) {
    
    switch (useType) {
        case ENTITY_USETARGET_TYPE_ON:
            TurnOn();
            break;
        case ENTITY_USETARGET_TYPE_OFF:
            TurnOff();
            break;
        case ENTITY_USETARGET_TYPE_TOGGLE:
            if (is_on) TurnOff(); else TurnOn();
            break;
    }
}
```

### Use Target Flags

**Spawn flags:**
```cpp
SPAWNFLAG_USETARGET_DISABLED    // Can't be used (temporarily)
SPAWNFLAG_USETARGET_PRESSABLE   // Single press activates
SPAWNFLAG_USETARGET_TOGGLEABLE  // Press toggles on/off
SPAWNFLAG_USETARGET_HOLDABLE    // Hold to activate continuously
```

**Example - Holdable button:**
```cpp
void svg_func_train_t::Spawn() {
    spawnflags |= SPAWNFLAG_USETARGET_HOLDABLE;
    SetUseCallback(&svg_func_train_t::Use);
}

void svg_func_train_t::Use(/*...*/) {
    // Called when player presses use
    StartMoving();
}

void svg_func_train_t::UseRelease(/*...*/) {
    // Called when player releases use
    StopMoving();
}
```

## Multiple Targets

One entity can activate multiple targets:

**Map:**
```
Button target="light1;light2;light3"

→ Activates light1, light2, AND light3
```

**Code:**
```cpp
void UseTargets(svg_base_edict_t *activator, svg_base_edict_t *other) {
    // Tokenize target string by semicolon
    const char *token = strtok(target, ";");
    while (token) {
        // Find entities with this targetname
        svg_base_edict_t *t = nullptr;
        while ((t = SVG_Find(t, FOFS(targetname), token)) != nullptr) {
            t->Use(this, activator, ENTITY_USETARGET_TYPE_TOGGLE, 0);
        }
        token = strtok(NULL, ";");
    }
}
```

## Kill Target

Special target that removes entities:

**Map:**
```
{
"classname" "trigger_multiple"
"killtarget" "monster1"
}
```

**Code:**
```cpp
void UseTargets(/*...*/) {
    // Kill targets
    if (killtarget) {
        svg_base_edict_t *t = nullptr;
        while ((t = SVG_Find(t, FOFS(targetname), killtarget)) != nullptr) {
            SVG_FreeEdict(t);  // Remove entity
        }
    }
    
    // Then activate normal targets
    if (target) {
        // ... normal target activation ...
    }
}
```

## Practical Examples

### Button Opens Door

```cpp
// Button
void svg_func_button_t::Use(/*...*/) {
    // Button pressed
    UseTargets(activator, activator);  // Activate door
}

// Door
void svg_func_door_t::Use(/*...*/) {
    // Triggered by button
    if (useType == ENTITY_USETARGET_TYPE_ON || 
        useType == ENTITY_USETARGET_TYPE_TOGGLE) {
        Door_GoUp();  // Open
    }
}
```

### Trigger Spawns Monsters

```cpp
// Trigger
void svg_trigger_multiple_t::Touch(svg_base_edict_t *other, /*...*/) {
    if (!other->client)
        return;
    
    UseTargets(other, other);  // Activate spawner
}

// Monster spawner
void svg_target_spawner_t::Use(/*...*/) {
    // Spawn monster
    auto *monster = SVG_Spawn<svg_monster_soldier_t>();
    monster->s.origin = s.origin;
    monster->Spawn();
}
```

### Use Hints

Display hints to players:

```cpp
// In player code
void Check_UseTarget_Hints(svg_player_edict_t *player) {
    // Trace forward from player
    trace_t tr = TraceForward(player, 64.0f);
    
    if (tr.ent && tr.ent->spawnflags & SPAWNFLAG_USETARGET_PRESSABLE) {
        // Show "Press E to use" hint
        player->client->ps.stats[STAT_USE_HINT] = CS_GENERAL + hint_index;
    }
}
```

## Best Practices

### Clear Naming

```cpp
// GOOD: Descriptive targetnames
"targetname" "security_door_1"
"targetname" "wave1_spawn_point"

// BAD: Generic names
"targetname" "door"
"targetname" "entity1"
```

### Validate Targets

```cpp
void PostSpawn() override {
    // Warn if target doesn't exist
    if (target && !SVG_Find(nullptr, FOFS(targetname), target)) {
        gi.dprintf("WARNING: Entity %s targets non-existent '%s'\n",
                   classname, target);
    }
}
```

### Prevent Spam

```cpp
void Use(/*...*/) {
    // Cooldown to prevent spamming
    if (level.time < use_timestamp) {
        return;  // Too soon
    }
    
    UseTargets(activator, activator);
    use_timestamp = level.time + 0.5f;  // 0.5 second cooldown
}
```

## Related Documentation

- [**Entity System Overview**](Entity-System-Overview.md)
- [**Creating Custom Entities**](Creating-Custom-Entities.md)
- [**Signal I/O System**](Signal-IO-System.md) - Advanced entity communication
- [**API - Spawn Flags**](API-Spawn-Flags.md) - UseTarget spawn flags

## Summary

The UseTargets system provides entity interaction:

- **target/targetname**: Entity targeting relationships
- **UseTargets()**: Activates all entities with matching targetname
- **Use callback**: Player interaction with +use key
- **Use types**: ON, OFF, TOGGLE, SET
- **Spawn flags**: PRESSABLE, TOGGLEABLE, HOLDABLE
- **killtarget**: Remove entities

This system is fundamental to creating interactive maps and gameplay in Q2RTXPerimental. Combine with the Signal I/O system for more advanced entity communication.
