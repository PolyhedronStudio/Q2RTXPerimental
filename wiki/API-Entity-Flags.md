# API Reference - Entity Flags

Entity flags control special behaviors and rendering effects. They are defined in `src/baseq2rtxp/sharedgame/sg_entity_flags.h`.

## Overview

Entity flags (`entity_state_t::effects`) are bit flags sent to clients that control visual effects and entity behavior. They're applied using bitwise operations.

**Location:** `src/baseq2rtxp/sharedgame/sg_entity_flags.h`

## Flag Categories

### Rendering Effects

These flags control how entities are rendered on the client:

```cpp
#define EF_ROTATE           BIT(1)   // Rotate (bonus items)
#define EF_GIB              BIT(2)   // Leave a trail (blood)
#define EF_ANIM_CYCLE2_2HZ  BIT(3)   // Cycle frames 0-1 at 2 Hz
#define EF_ANIM01           BIT(4)   // Cycle frames 0-1 at 2 Hz
#define EF_ANIM23           BIT(5)   // Cycle frames 2-3 at 2 Hz
#define EF_ANIM_ALL         BIT(6)   // Cycle all frames at 2 Hz
#define EF_ANIM_ALLFAST     BIT(7)   // Cycle all frames at 10 Hz
```

### Power-Up Effects

These flags indicate active power-ups:

```cpp
#define EF_COLOR_SHELL      BIT(8)   // Colored shell effect
#define EF_QUAD             BIT(9)   // Quad damage glow
#define EF_PENT             BIT(10)  // Pentagram of protection
#define EF_DOUBLE           BIT(11)  // Double damage
#define EF_HALF_DAMAGE      BIT(12)  // Half damage taken
```

### Special Effects

```cpp
#define EF_TELEPORTER       BIT(13)  // Teleporter particle fountain
```

### Entity Event Targeting

```cpp
#define EF_ENTITY_EVENT_TARGET_OTHER  BIT(14)  // Event targets another entity
```

### Reserved/Unused

```cpp
#define EF_UNUSED_15 through EF_UNUSED_31  // Available for custom use
```

## Common Flag Usage

### Rotating Items

Make pickup items spin:

```cpp
void svg_item_health_t::Spawn() {
    s.effects |= EF_ROTATE;  // Item rotates continuously
    s.entityType = ET_ITEM;
}
```

**Client Behavior:** Item rotates around Z-axis at constant speed

### Animated Lights

Create blinking or animated lights:

```cpp
void svg_light_blink_t::Spawn() {
    s.effects |= EF_ANIM01;  // Blink between frames 0 and 1
}

void svg_light_strobe_t::Spawn() {
    s.effects |= EF_ANIM_ALLFAST;  // Fast animation through all frames
}
```

**Frame Cycling Speeds:**
- `EF_ANIM01`, `EF_ANIM23`, `EF_ANIM_ALL`: 2 Hz (2 cycles per second)
- `EF_ANIM_ALLFAST`: 10 Hz (10 cycles per second)

### Gib Trails

Make gibs leave blood trails:

```cpp
void SVG_ThrowGib(/*...*/) {
    gib->s.effects |= EF_GIB;  // Leaves blood particle trail
    gib->movetype = MOVETYPE_BOUNCE;
}
```

**Client Behavior:** Spawns blood particles along movement path

### Power-Up Glows

Indicate active power-ups on players:

```cpp
void svg_player_edict_t::PickupQuad() {
    s.effects |= EF_QUAD;  // Blue glow
    // Stacks with other effects
}

void svg_player_edict_t::PickupInvulnerability() {
    s.effects |= EF_PENT;  // Red glow
}
```

**Visual Effects:**
- `EF_QUAD`: Blue outer glow
- `EF_PENT`: Red spinning shell
- `EF_DOUBLE`: Green glow
- `EF_HALF_DAMAGE`: Yellow glow

**Note:** These can be combined (bitwise OR) for multiple simultaneous effects.

### Color Shells

Custom colored shells around entities:

```cpp
void svg_player_edict_t::ApplyColorShell(vec3_t color) {
    s.effects |= EF_COLOR_SHELL;
    s.renderfx = RF_SHELL_RED | RF_SHELL_GREEN;  // Yellow shell
}
```

**Requires:** Also setting `renderfx` flags for shell colors

### Teleporter Effect

Spawn particle fountain effect:

```cpp
void svg_misc_teleporter_t::Spawn() {
    s.effects |= EF_TELEPORTER;  // Upward particle stream
}
```

**Client Behavior:** Continuous upward particle fountain

## Combining Flags

Flags are combined using bitwise OR (`|`):

```cpp
// Item that rotates and has animation
s.effects = EF_ROTATE | EF_ANIM_ALL;

// Player with quad damage and invulnerability
s.effects = EF_QUAD | EF_PENT;

// Gib that leaves trail and rotates
s.effects = EF_GIB | EF_ROTATE;
```

## Setting and Clearing Flags

### Setting a Flag

```cpp
// Add flag (preserves other flags)
s.effects |= EF_ROTATE;

// Set multiple flags
s.effects |= (EF_ROTATE | EF_ANIM_ALL);
```

### Clearing a Flag

```cpp
// Remove flag (preserves other flags)
s.effects &= ~EF_ROTATE;

// Remove multiple flags
s.effects &= ~(EF_ROTATE | EF_ANIM_ALL);
```

### Toggling a Flag

```cpp
// Toggle on/off
s.effects ^= EF_ANIM01;

// Example: Blinking light
void Think() {
    s.effects ^= EF_ANIM01;  // Toggle between on and off
    nextThinkTime = level.time + gametime_t::from_sec(0.5f);
}
```

### Checking a Flag

```cpp
// Check if flag is set
if (s.effects & EF_ROTATE) {
    gi.dprintf("Entity is rotating\n");
}

// Check multiple flags (any)
if (s.effects & (EF_QUAD | EF_PENT)) {
    gi.dprintf("Has power-up\n");
}

// Check multiple flags (all)
if ((s.effects & (EF_QUAD | EF_PENT)) == (EF_QUAD | EF_PENT)) {
    gi.dprintf("Has both quad and pent\n");
}
```

## Client-Side Rendering

The client uses these flags to control rendering:

```cpp
// In client rendering code (clgame)
void CLG_AddEntity(centity_t *cent) {
    entity_t ent = {};
    
    // Check for rotation
    if (cent->current.effects & EF_ROTATE) {
        ent.angles[YAW] = anglemod(cl.time * 100);  // Rotate
    }
    
    // Check for animation
    if (cent->current.effects & EF_ANIM01) {
        int frame = (cl.time / 500) & 1;  // Alternate 0,1 at 2 Hz
        ent.frame = frame;
    }
    
    // Check for trails
    if (cent->current.effects & EF_GIB) {
        CLG_AddGibTrail(cent);  // Blood particles
    }
    
    // Check for power-up glows
    if (cent->current.effects & EF_QUAD) {
        CLG_AddQuadGlow(&ent);  // Blue glow
    }
    
    V_AddEntity(&ent);
}
```

## Practical Examples

### Bonus Item

```cpp
void svg_item_bonus_t::Spawn() {
    // Rotating, animated item
    s.effects = EF_ROTATE | EF_ANIM_ALLFAST;
    s.entityType = ET_ITEM;
    gi.SetModel(edict, "models/items/bonus/tris.md2");
}
```

**Result:** Item spins and cycles through animation frames rapidly

### Strobe Light

```cpp
class svg_light_strobe_t : public svg_base_edict_t {
    void Spawn() override {
        s.effects = EF_ANIM01;
        SetThinkCallback(&svg_light_strobe_t::StrobeThink);
        nextThinkTime = level.time + FRAMETIME;
    }
    
    void StrobeThink() {
        // Toggle light on/off
        s.effects ^= EF_ANIM01;
        nextThinkTime = level.time + gametime_t::from_sec(0.1f);
    }
};
```

**Result:** Light blinks rapidly (10 Hz)

### Power-Up Effects

```cpp
void svg_player_edict_t::UpdatePowerUps() {
    // Clear old power-up flags
    s.effects &= ~(EF_QUAD | EF_PENT | EF_DOUBLE | EF_HALF_DAMAGE);
    
    // Apply active power-ups
    if (client->quad_framenum > level.frameNumber) {
        s.effects |= EF_QUAD;
    }
    if (client->invincible_framenum > level.frameNumber) {
        s.effects |= EF_PENT;
    }
    if (client->double_framenum > level.frameNumber) {
        s.effects |= EF_DOUBLE;
    }
}
```

**Result:** Player glows based on active power-ups

### Blood Trail

```cpp
void svg_monster_soldier_t::Die(/*...*/) {
    // Gib if overkill
    if (health < gib_health) {
        for (int i = 0; i < 4; i++) {
            auto *gib = ThrowGib("models/objects/gibs/sm_meat/tris.md2", damage);
            gib->s.effects |= EF_GIB;  // Blood trail
        }
    }
}
```

**Result:** Flying gibs leave blood particle trails

### Teleporter Pad

```cpp
void svg_misc_teleporter_t::Spawn() {
    s.effects = EF_TELEPORTER;
    s.modelindex = gi.modelindex("models/objects/dmspot/tris.md2");
    
    // Also play looping sound
    s.sound = gi.soundindex("world/amb10.wav");
}
```

**Result:** Pad has upward particle stream and ambient sound

## Custom Effects Using Unused Flags

You can use unused flags for custom effects:

```cpp
// Define custom flag
#define EF_CUSTOM_GLOW  EF_UNUSED_15

// Server-side
void svg_custom_entity_t::Spawn() {
    s.effects |= EF_CUSTOM_GLOW;
}

// Client-side (in clgame)
void CLG_AddEntity(centity_t *cent) {
    if (cent->current.effects & EF_CUSTOM_GLOW) {
        CLG_AddCustomGlowEffect(cent);  // Your custom rendering
    }
}
```

**Note:** Requires modifying both server and client code

## Performance Considerations

### Flag Overhead

Each flag set adds minimal overhead:
- Flags are part of entity state (always sent)
- Setting/clearing flags is bitwise operation (very fast)
- Client-side checks are also bitwise (very fast)

### Effect Complexity

Some effects are more expensive than others:

**Low Cost:**
- `EF_ROTATE` - Simple angle calculation
- `EF_ANIM01` - Frame index calculation

**Medium Cost:**
- `EF_GIB` - Spawns particles periodically
- `EF_TELEPORTER` - Continuous particle spawning

**High Cost:**
- `EF_QUAD`, `EF_PENT` - Additional rendering passes for glows
- `EF_COLOR_SHELL` - Outline rendering

**Tip:** Don't combine too many expensive effects on many entities

## Debugging

### Console Commands

```
// Show entity effects
ent_info <entitynum>

// Output includes:
// Entity 5: monster_soldier
//   effects: EF_ROTATE | EF_GIB
```

### Debug Visualization

```cpp
#ifdef _DEBUG
void DebugEntityEffects(svg_base_edict_t *ent) {
    if (ent->s.effects & EF_ROTATE)
        gi.dprintf("  EF_ROTATE\n");
    if (ent->s.effects & EF_GIB)
        gi.dprintf("  EF_GIB\n");
    if (ent->s.effects & EF_QUAD)
        gi.dprintf("  EF_QUAD\n");
    // ... check all flags ...
}
#endif
```

### Client-Side Debug

```cpp
// In client game (clgame)
if (cl_show_effects->value) {
    DrawText(va("Effects: 0x%04X", cent->current.effects));
}
```

## Common Mistakes

### Forgetting to Set Model

```cpp
// BAD: Effects without model
s.effects = EF_ROTATE;
// Nothing to rotate!

// GOOD: Set model first
gi.SetModel(edict, "models/items/health/tris.md2");
s.effects = EF_ROTATE;
```

### Overwriting Instead of Adding

```cpp
// BAD: Overwrites existing effects
s.effects = EF_QUAD;  // Loses EF_ROTATE if it was set

// GOOD: Add to existing effects
s.effects |= EF_QUAD;  // Preserves other flags
```

### Wrong Animation Frames

```cpp
// BAD: Model doesn't have enough frames
s.effects = EF_ANIM_ALL;  // But model only has 2 frames

// GOOD: Use appropriate animation
if (modelFrameCount <= 2) {
    s.effects = EF_ANIM01;  // Only use frames 0-1
}
```

## Best Practices

### Clear Before Setting

```cpp
// When completely changing effect state
s.effects = 0;  // Clear all
s.effects = EF_ROTATE | EF_ANIM_ALL;  // Set new state
```

### Use Constants for Clarity

```cpp
// GOOD: Clear and maintainable
static constexpr uint32_t POWERUP_FLAGS = EF_QUAD | EF_PENT | EF_DOUBLE;

void ClearPowerUps() {
    s.effects &= ~POWERUP_FLAGS;
}
```

### Document Custom Effects

```cpp
// If using unused flags, document them
// EF_UNUSED_15: Custom entity glow effect
// EF_UNUSED_16: Custom pulsing effect
#define EF_CUSTOM_GLOW   EF_UNUSED_15
#define EF_CUSTOM_PULSE  EF_UNUSED_16
```

## Related Documentation

- [Entity Types](API-Entity-Types.md) - Entity type constants
- [Entity Events](API-Entity-Events.md) - Entity event system
- [Entity System Overview](Entity-System-Overview.md) - Entity architecture
- [Client Game Module](Client-Game-Module.md) - Client-side rendering

---

**See Also:**
- `src/baseq2rtxp/sharedgame/sg_entity_flags.h` - Flag definitions
- `src/baseq2rtxp/clgame/clg_entities.cpp` - Client rendering code
- `src/baseq2rtxp/svgame/svg_spawn.cpp` - Server entity spawning
