# API Reference - Spawn Flags

Spawn flags are bit flags set in map editors (like TrenchBroom or NetRadiant) that control entity behavior at spawn time. They are defined throughout the codebase but primarily in `src/baseq2rtxp/svgame/entities/svg_base_edict.h` and individual entity headers.

## Overview

Spawn flags allow level designers to customize entity behavior without coding. They are set using checkboxes in map editors and are stored in the `spawnflags` member of `svg_base_edict_t`.

**Key Characteristics:**
- **Set at map load time**: Flags are read from map entity definitions
- **Bitmask values**: Multiple flags can be combined using bitwise OR
- **Read-only at runtime**: Typically not changed after spawn
- **Editor-friendly**: Exposed as checkboxes in map editors

## Universal Spawn Flags

These flags are available for **all entities** and defined in `src/baseq2rtxp/svgame/entities/svg_base_edict.h`:

### Game Mode Filters

```cpp
static constexpr spawnflag_t SPAWNFLAG_NOT_DEATHMATCH = BIT(19);  // Bit 19
static constexpr spawnflag_t SPAWNFLAG_NOT_COOP = BIT(20);        // Bit 20
```

**Purpose:** Exclude entities from specific game modes

**Usage in Maps:**
```
// In .map file
{
"classname" "weapon_shotgun"
"origin" "128 256 64"
"spawnflags" "524288"  // SPAWNFLAG_NOT_DEATHMATCH (2^19)
}
```

**Server-side Check:**
```cpp
void svg_base_edict_t::Spawn() {
    // Check if entity should spawn in current game mode
    if (game.deathmatch && (spawnflags & SPAWNFLAG_NOT_DEATHMATCH)) {
        SVG_FreeEdict(this);
        return;
    }
    
    if (game.coop && (spawnflags & SPAWNFLAG_NOT_COOP)) {
        SVG_FreeEdict(this);
        return;
    }
    
    // Continue spawning...
}
```

**Example Use Cases:**
- Health packs only in single-player/coop
- Extra ammo only in deathmatch
- Story-specific entities excluded from multiplayer

### Difficulty Filters

```cpp
static constexpr spawnflag_t SPAWNFLAG_NOT_EASY = BIT(21);    // Bit 21
static constexpr spawnflag_t SPAWNFLAG_NOT_MEDIUM = BIT(22);  // Bit 22
static constexpr spawnflag_t SPAWNFLAG_NOT_HARD = BIT(23);    // Bit 23
```

**Purpose:** Exclude entities from specific difficulty levels

**Usage:**
```cpp
void svg_monster_soldier_t::Spawn() {
    // Check difficulty level
    int skill_level = (int)g_skill->value;
    
    if (skill_level == 0 && (spawnflags & SPAWNFLAG_NOT_EASY)) {
        SVG_FreeEdict(this);
        return;
    }
    if (skill_level == 1 && (spawnflags & SPAWNFLAG_NOT_MEDIUM)) {
        SVG_FreeEdict(this);
        return;
    }
    if (skill_level >= 2 && (spawnflags & SPAWNFLAG_NOT_HARD)) {
        SVG_FreeEdict(this);
        return;
    }
    
    // Spawn monster...
}
```

**Example Use Cases:**
- Fewer monsters on easy difficulty
- Extra monsters on hard difficulty
- Powerful weapons only on higher difficulties

### UseTarget System Flags

```cpp
static constexpr spawnflag_t SPAWNFLAG_USETARGET_DISABLED = BIT(15);    // Bit 15
static constexpr spawnflag_t SPAWNFLAG_USETARGET_PRESSABLE = BIT(16);   // Bit 16
static constexpr spawnflag_t SPAWNFLAG_USETARGET_TOGGLEABLE = BIT(17);  // Bit 17
static constexpr spawnflag_t SPAWNFLAG_USETARGET_HOLDABLE = BIT(18);    // Bit 18
```

**Purpose:** Control interactive behavior with the use key

#### SPAWNFLAG_USETARGET_DISABLED

Temporarily disables use-key interaction for this entity.

```cpp
void svg_base_edict_t::CheckUseTarget(svg_player_edict_t *player) {
    if (spawnflags & SPAWNFLAG_USETARGET_DISABLED) {
        return;  // Cannot be used
    }
    
    // Allow use...
}
```

**Use Case:** Buttons or switches that start disabled and activate via trigger

#### SPAWNFLAG_USETARGET_PRESSABLE

Entity acts as a button - single press activates.

```cpp
void svg_func_button_t::Spawn() {
    spawnflags |= SPAWNFLAG_USETARGET_PRESSABLE;
    // Set up button behavior...
}
```

**Use Case:** Buttons, switches, levers

#### SPAWNFLAG_USETARGET_TOGGLEABLE

Entity toggles between on/off states with each use.

```cpp
void svg_func_door_t::UseTarget_OnPress(svg_base_edict_t *other) {
    if (spawnflags & SPAWNFLAG_USETARGET_TOGGLEABLE) {
        if (moveinfo.state == STATE_TOP) {
            // Door is open, close it
            Close();
        } else if (moveinfo.state == STATE_BOTTOM) {
            // Door is closed, open it
            Open();
        }
    }
}
```

**Use Case:** Doors that toggle, lights that switch on/off

#### SPAWNFLAG_USETARGET_HOLDABLE

Entity responds to hold duration - triggers on press and release.

```cpp
void svg_func_train_t::UseTarget_OnPress(svg_base_edict_t *other) {
    if (spawnflags & SPAWNFLAG_USETARGET_HOLDABLE) {
        // Start moving
        StartMoving();
    }
}

void svg_func_train_t::UseTarget_OnRelease(svg_base_edict_t *other) {
    if (spawnflags & SPAWNFLAG_USETARGET_HOLDABLE) {
        // Stop moving
        StopMoving();
    }
}
```

**Use Case:** Elevators, cranes, hold-to-activate mechanisms

## Entity-Specific Spawn Flags

### Trigger Entities

#### trigger_multiple

**Location:** `src/baseq2rtxp/svgame/entities/trigger/svg_trigger_multiple.h`

```cpp
static constexpr spawnflag_t SPAWNFLAG_MONSTER = 1;       // Bit 0
static constexpr spawnflag_t SPAWNFLAG_NOT_PLAYER = 2;    // Bit 1
static constexpr spawnflag_t SPAWNFLAG_TRIGGERED = 4;     // Bit 2
static constexpr spawnflag_t SPAWNFLAG_BRUSH_CLIP = 32;   // Bit 5
```

**SPAWNFLAG_MONSTER:**
```cpp
void svg_trigger_multiple_t::Touch(svg_base_edict_t *other) {
    if (spawnflags & SPAWNFLAG_MONSTER) {
        if (!(other->svflags & SVF_MONSTER))
            return;  // Only monsters can trigger
    }
}
```

**SPAWNFLAG_NOT_PLAYER:**
```cpp
void svg_trigger_multiple_t::Touch(svg_base_edict_t *other) {
    if ((spawnflags & SPAWNFLAG_NOT_PLAYER) && other->client)
        return;  // Players cannot trigger this
}
```

**SPAWNFLAG_TRIGGERED:**
```cpp
void svg_trigger_multiple_t::Spawn() {
    if (spawnflags & SPAWNFLAG_TRIGGERED) {
        solid = SOLID_NOT;  // Start inactive
        // Requires Use() call to activate
    }
}
```

#### trigger_hurt

**Location:** `src/baseq2rtxp/svgame/entities/trigger/svg_trigger_hurt.h`

```cpp
static constexpr int32_t SPAWNFLAG_START_OFF = 1;         // Bit 0
static constexpr int32_t SPAWNFLAG_TOGGLE = 2;            // Bit 1
static constexpr int32_t SPAWNFLAG_SILENT = 4;            // Bit 2
static constexpr int32_t SPAWNFLAG_NO_PROTECTION = 8;     // Bit 3
static constexpr int32_t SPAWNFLAG_SLOW_HURT = 16;        // Bit 4
static constexpr int32_t SPAWNFLAG_BRUSH_CLIP = 32;       // Bit 5
```

**Examples:**
```cpp
// Start inactive
if (spawnflags & SPAWNFLAG_START_OFF) {
    solid = SOLID_NOT;
}

// Toggle on/off via Use()
if (spawnflags & SPAWNFLAG_TOGGLE) {
    // Allow toggling between active/inactive
}

// No pain sounds
if (spawnflags & SPAWNFLAG_SILENT) {
    // Don't play damage sounds
}

// Ignore armor
if (spawnflags & SPAWNFLAG_NO_PROTECTION) {
    damage_flags |= DAMAGE_NO_ARMOR;
}

// Damage once per second instead of 10 times per second
if (spawnflags & SPAWNFLAG_SLOW_HURT) {
    timestamp = level.time + 1.0;
}
```

#### trigger_push

**Location:** `src/baseq2rtxp/svgame/entities/trigger/svg_trigger_push.h`

```cpp
static constexpr spawnflag_t SPAWNFLAG_PUSH_ONCE = 1;     // Bit 0
static constexpr spawnflag_t SPAWNFLAG_BRUSH_CLIP = 32;   // Bit 5
```

**SPAWNFLAG_PUSH_ONCE:**
```cpp
void svg_trigger_push_t::Touch(svg_base_edict_t *other) {
    // Apply push velocity
    other->velocity = push_velocity;
    
    if (spawnflags & SPAWNFLAG_PUSH_ONCE) {
        // Remove after first use
        SVG_FreeEdict(this);
    }
}
```

#### trigger_gravity

**Location:** `src/baseq2rtxp/svgame/entities/trigger/svg_trigger_gravity.h`

```cpp
static constexpr spawnflag_t SPAWNFLAG_TOGGLE = 8;           // Bit 3
static constexpr spawnflag_t SPAWNFLAG_START_OFF = 16;       // Bit 4
static constexpr spawnflag_t SPAWNFLAG_BRUSH_CLIP = 32;      // Bit 5
```

### Light Entities

#### light / light_spotlight

**Location:** `src/baseq2rtxp/svgame/entities/light/svg_light_light.h`

```cpp
static constexpr spawnflag_t SPAWNFLAG_START_OFF = 1;  // Bit 0
```

**Usage:**
```cpp
void svg_light_light_t::Spawn() {
    if (spawnflags & SPAWNFLAG_START_OFF) {
        s.renderfx |= RF_SHELL_RED;  // Light starts off
    } else {
        s.renderfx |= RF_SHELL_GREEN;  // Light starts on
    }
}

void svg_light_light_t::Use(svg_base_edict_t *other) {
    // Toggle light on/off
    if (s.renderfx & RF_SHELL_GREEN) {
        s.renderfx &= ~RF_SHELL_GREEN;
        s.renderfx |= RF_SHELL_RED;
    } else {
        s.renderfx &= ~RF_SHELL_RED;
        s.renderfx |= RF_SHELL_GREEN;
    }
}
```

### Target Entities

#### target_laser

**Location:** `src/baseq2rtxp/svgame/entities/target/svg_target_laser.h`

```cpp
static constexpr spawnflag_t SPAWNFLAG_START_ON = BIT(0);      // Bit 0
static constexpr spawnflag_t SPAWNFLAG_COLOR_RED = BIT(1);     // Bit 1
static constexpr spawnflag_t SPAWNFLAG_COLOR_GREEN = BIT(2);   // Bit 2
static constexpr spawnflag_t SPAWNFLAG_COLOR_BLUE = BIT(3);    // Bit 3
static constexpr spawnflag_t SPAWNFLAG_COLOR_YELLOW = BIT(4);  // Bit 4
static constexpr spawnflag_t SPAWNFLAG_COLOR_ORANGE = BIT(5);  // Bit 5
static constexpr spawnflag_t SPAWNFLAG_THICK = BIT(6);         // Bit 6
```

**Color Selection:**
```cpp
void svg_target_laser_t::Spawn() {
    if (spawnflags & SPAWNFLAG_COLOR_RED) {
        s.skinnum = 0xf2f2f0f0;
    } else if (spawnflags & SPAWNFLAG_COLOR_GREEN) {
        s.skinnum = 0xd0d1d2d3;
    } else if (spawnflags & SPAWNFLAG_COLOR_BLUE) {
        s.skinnum = 0xf3f3f1f1;
    } else if (spawnflags & SPAWNFLAG_COLOR_YELLOW) {
        s.skinnum = 0xdcdddedf;
    } else if (spawnflags & SPAWNFLAG_COLOR_ORANGE) {
        s.skinnum = 0xe0e1e2e3;
    }
    
    if (spawnflags & SPAWNFLAG_THICK) {
        s.frame = 16;  // Thicker laser beam
    } else {
        s.frame = 4;   // Normal laser beam
    }
}
```

### Counter Entity

#### trigger_counter

**Location:** `src/baseq2rtxp/svgame/entities/trigger/svg_trigger_counter.h`

```cpp
static constexpr spawnflag_t SPAWNFLAG_NO_MESSAGE = BIT(0);  // Bit 0
static constexpr spawnflag_t SPAWNFLAG_TOUCHABLE = 64;       // Bit 6
```

**SPAWNFLAG_NO_MESSAGE:**
```cpp
void svg_trigger_counter_t::Use(svg_base_edict_t *activator) {
    count--;
    
    if (count > 0 && !(spawnflags & SPAWNFLAG_NO_MESSAGE)) {
        // Display counter message
        gi.centerprintf(activator, "%i more to go...", count);
    }
}
```

## Working with Spawn Flags

### Checking Spawn Flags

```cpp
// Check single flag
if (spawnflags & SPAWNFLAG_NOT_DEATHMATCH) {
    // Entity excluded from deathmatch
}

// Check multiple flags (any)
if (spawnflags & (SPAWNFLAG_NOT_EASY | SPAWNFLAG_NOT_MEDIUM)) {
    // Entity excluded from easy OR medium
}

// Check multiple flags (all)
if ((spawnflags & (FLAG_A | FLAG_B)) == (FLAG_A | FLAG_B)) {
    // Entity has BOTH flags set
}
```

### Setting Spawn Flags at Runtime

```cpp
// Add a flag
spawnflags |= SPAWNFLAG_START_OFF;

// Remove a flag
spawnflags &= ~SPAWNFLAG_START_OFF;

// Toggle a flag
spawnflags ^= SPAWNFLAG_TOGGLE;
```

**Note:** Typically spawn flags are read-only after spawn and shouldn't be modified at runtime unless you have a specific reason.

### Custom Entity Spawn Flags

When creating custom entities, define spawn flags as class-specific constants:

```cpp
// In your custom entity header
class svg_custom_trigger_t : public svg_trigger_multiple_t {
public:
    // Custom spawn flags
    static constexpr spawnflag_t SPAWNFLAG_REVERSE = BIT(10);
    static constexpr spawnflag_t SPAWNFLAG_RANDOM = BIT(11);
    
    void Spawn() override {
        if (spawnflags & SPAWNFLAG_REVERSE) {
            // Custom behavior
        }
        
        if (spawnflags & SPAWNFLAG_RANDOM) {
            // More custom behavior
        }
        
        // Call parent spawn
        svg_trigger_multiple_t::Spawn();
    }
};
```

**Bit Usage Guidelines:**
- Bits 0-14: Entity-specific flags
- Bit 15-18: UseTarget system (universal)
- Bits 19-23: Game mode/difficulty filters (universal)
- Bits 24-31: Available for custom use

## Map Editor Integration

### TrenchBroom FGD Example

```fgd
@PointClass base(Targetname) = trigger_hurt : "Hurt Trigger"
[
    spawnflags(flags) = 
    [
        1 : "Start Off" : 0
        2 : "Toggle" : 0
        4 : "Silent" : 0
        8 : "No Protection" : 0
        16 : "Slow Hurt" : 0
    ]
    dmg(integer) : "Damage" : 5
    wait(string) : "Wait" : "1"
]
```

### NetRadiant Entity Definition

```
<point name="trigger_hurt" color="1 .5 .5" box="-8 -8 -8 8 8 8">
Hurts entities that touch it.
-------- KEYS --------
dmg : damage to deal (default 5)
-------- SPAWNFLAGS --------
1 : START_OFF - trigger starts disabled
2 : TOGGLE - can be toggled on/off
4 : SILENT - no damage sounds
8 : NO_PROTECTION - ignores armor
16 : SLOW_HURT - damage once per second
</point>
```

## Best Practices

### Design Time Configuration

✅ **Use spawn flags for:**
- Game mode filtering (deathmatch vs. coop)
- Difficulty scaling
- Initial state (on/off, open/closed)
- Behavior variations (toggle vs. momentary)
- Visual variations (colors, sizes)

❌ **Don't use spawn flags for:**
- Runtime state changes (use member variables)
- Complex logic (use entity keys/properties)
- Data that changes frequently

### Naming Conventions

```cpp
// GOOD: Clear, descriptive names
static constexpr spawnflag_t SPAWNFLAG_START_OFF = BIT(0);
static constexpr spawnflag_t SPAWNFLAG_TOGGLE = BIT(1);

// BAD: Cryptic names
static constexpr spawnflag_t SF1 = BIT(0);
static constexpr spawnflag_t FLAG_A = BIT(1);
```

### Documentation

Always document spawn flags in:
1. **Code comments** - What the flag does
2. **Entity headers** - Grouped with related flags
3. **Map editor definitions** - FGD/DEF files
4. **Wiki pages** - Usage examples

## Related Documentation

- [**Entity System Overview**](Entity-System-Overview.md) - Entity architecture
- [**Creating Custom Entities**](Creating-Custom-Entities.md) - Implementing spawn flags
- [**UseTargets System**](UseTargets-System.md) - UseTarget spawn flags in detail
- [**Server Game Module**](Server-Game-Module.md) - Entity spawning system
- [**API - Entity Types**](API-Entity-Types.md) - Entity type reference
- [**API - Entity Flags**](API-Entity-Flags.md) - Runtime entity flags (different from spawn flags)

## Summary

Spawn flags are **design-time bit flags** that control entity behavior:

- **Set in map editors** using checkboxes
- **Read at spawn time** from map entity data
- **Bitmask values** allow multiple flags per entity
- **Universal flags** (bits 15-23) work on all entities
- **Entity-specific flags** (bits 0-14, 24-31) for custom behavior

Use spawn flags to give level designers control over entity behavior without requiring code changes. Keep flags simple, well-documented, and exposed in map editor definitions.
