# API Reference - Entity Events

Entity events are used to trigger client-side effects (sounds, animations, particles) that are synchronized with server-side entity actions. They are defined in `src/baseq2rtxp/sharedgame/sg_entity_events.h`.

## Overview

Entity events are **networked actions** that occur relative to an entity's origin. Unlike temporary entities (which are independent effects), entity events are attached to existing entities and leverage the entity's position and orientation.

**Key Characteristics:**
- **Network Efficient**: Events piggyback on entity state updates
- **Position Relative**: Effects occur at the entity's origin
- **Time-Limited**: Each event is valid for `EVENT_VALID_MSEC` (150ms at 40hz tickrate)
- **Change Detection**: Event bits increment to distinguish repeated identical events

## Event System Mechanics

### Event Bits

```cpp
static constexpr int32_t EV_EVENT_BIT1 = BIT(8);  // 0x00000100
static constexpr int32_t EV_EVENT_BIT2 = BIT(9);  // 0x00000200
static constexpr int32_t EV_EVENT_BITS = (EV_EVENT_BIT1 | EV_EVENT_BIT2);
```

The top two bits of `entity_state_t->event` increment with each change, allowing clients to detect when the same event fires twice in succession.

### Getting Event Value

```cpp
// Extract the actual event ID from the event field
int32_t eventID = SG_GetEntityEventValue(ent->s.event);
```

### Event Validity Window

```cpp
static constexpr int32_t EVENT_VALID_MSEC = 150;  // 6 frames at 40hz
```

Events remain valid for 150ms, ensuring they survive network latency and packet loss while remaining responsive.

## Entity Event Types

### Footstep Events

**Location:** `sg_entity_events.h`

```cpp
EV_PLAYER_FOOTSTEP      // Player footstep sound
EV_OTHER_FOOTSTEP       // Other entity footstep sound
EV_FOOTSTEP_LADDER      // Ladder climbing footstep
```

**Usage:**
```cpp
// Server-side: Trigger footstep event during player movement
if (should_play_footstep) {
    player->s.event = EV_PLAYER_FOOTSTEP;
}
```

**Client-side Effect:**
- Plays appropriate footstep sound based on surface type
- May spawn small dust particle effects

### Water Events

```cpp
// Water entry events
EV_WATER_ENTER_FEET     // Feet touch water surface
EV_WATER_ENTER_WAIST    // Deep diving - waist enters water
EV_WATER_ENTER_HEAD     // Head goes underwater

// Water exit events
EV_WATER_LEAVE_FEET     // Feet leave water
EV_WATER_LEAVE_WAIST    // Waist leaves water
EV_WATER_LEAVE_HEAD     // Head leaves water
```

**Usage:**
```cpp
// Detect water level changes in player physics
if (old_waterlevel != new_waterlevel) {
    if (new_waterlevel == WATER_FEET) {
        player->s.event = EV_WATER_ENTER_FEET;
    } else if (new_waterlevel == WATER_WAIST) {
        player->s.event = EV_WATER_ENTER_WAIST;
    }
}
```

**Client-side Effects:**
- Splash sounds
- Water splash particles
- Ripple effects

### Jump and Fall Events

```cpp
EV_JUMP_UP          // Entity jumps
EV_JUMP_LAND        // Entity lands from a jump

EV_FALL_SHORT       // Short fall (minor impact)
EV_FALL_MEDIUM      // Medium fall (moderate impact)
EV_FALL_FAR         // Long fall (hard impact)
```

**Usage:**
```cpp
// Jump event
void svg_player_edict_t::Jump() {
    velocity[2] += 270;  // Jump velocity
    s.event = EV_JUMP_UP;
}

// Landing event based on fall distance
void svg_player_edict_t::Land(float fall_distance) {
    if (fall_distance < 100) {
        s.event = EV_FALL_SHORT;
    } else if (fall_distance < 300) {
        s.event = EV_FALL_MEDIUM;
    } else {
        s.event = EV_FALL_FAR;
    }
}
```

**Client-side Effects:**
- Jump: Grunt sound
- Landing: Impact sound (volume/pitch based on severity), camera shake

### Weapon Events

```cpp
EV_WEAPON_RELOAD             // Weapon reload
EV_WEAPON_PRIMARY_FIRE       // Primary fire
EV_WEAPON_SECONDARY_FIRE     // Secondary fire (alt-fire)
EV_WEAPON_HOLSTER_AND_DRAW   // Weapon switch animation
EV_WEAPON_DRAW               // Draw weapon
EV_WEAPON_HOLSTER            // Holster weapon
```

**Usage:**
```cpp
// Fire weapon
void svg_player_edict_t::FireWeapon() {
    s.event = EV_WEAPON_PRIMARY_FIRE;
    // ... weapon firing logic ...
}

// Reload
void svg_player_edict_t::Reload() {
    s.event = EV_WEAPON_RELOAD;
    weapon_frame = RELOAD_START_FRAME;
}
```

**Client-side Effects:**
- Muzzle flash
- Weapon animations
- Shell ejection particles
- Sound effects

**Note:** These events can be predicted client-side for responsive weapon feedback. See `PS_EV_MAX` for predictable event boundary.

### Teleport Events

```cpp
EV_PLAYER_LOGIN      // Player connects/spawns
EV_PLAYER_LOGOUT     // Player disconnects
EV_PLAYER_TELEPORT   // Player teleports
EV_OTHER_TELEPORT    // Other entity teleports
```

**Usage:**
```cpp
// Teleporter destination
void svg_misc_teleporter_dest_t::TeleportPlayer(svg_player_edict_t *player) {
    player->s.origin = this->s.origin;
    player->s.event = EV_PLAYER_TELEPORT;
}

// Monster teleporting
void svg_monster_base_t::TeleportTo(const vec3_t &destination) {
    s.origin = destination;
    s.event = EV_OTHER_TELEPORT;
}
```

**Client-side Effects:**
- Teleport particle effect (expanding ring)
- Teleport sound
- Flash of light

### Item Events

```cpp
EV_ITEM_RESPAWN      // Item respawns after being picked up
```

**Usage:**
```cpp
void svg_item_edict_t::Respawn() {
    solid = SOLID_TRIGGER;
    s.event = EV_ITEM_RESPAWN;
    nextthink = level.time + 10;  // Respawn after 10 seconds
}
```

**Client-side Effect:**
- Respawn particle effect (materializing)
- Respawn sound

### Sound Events

These are special "external" events that create temporary client-side entities for sound playback:

```cpp
EV_GENERAL_SOUND       // Entity-relative sound (standard attenuation)
EV_GENERAL_SOUND_EX    // Entity-relative sound with custom attenuation
EV_POSITIONED_SOUND    // Positioned sound at specific location
EV_GLOBAL_SOUND        // Non-attenuated sound (plays at full volume)
```

**Usage:**
```cpp
// Play sound on entity
void PlayEntitySound(svg_base_edict_t *ent, int soundindex, int channel) {
    // Setup sound event data...
    ent->s.event = EV_GENERAL_SOUND;
}

// Play positioned sound effect
void PlayPositionedSound(const vec3_t &origin, int soundindex) {
    // Create temporary entity with EV_POSITIONED_SOUND
    // Sound persists at that location
}

// Play global sound (heard by all players at full volume)
void PlayGlobalSound(int soundindex) {
    // Useful for level-wide announcements
}
```

## Predictable vs. Non-Predictable Events

### PS_EV_MAX Boundary

```cpp
PS_EV_MAX  // Maximum predictable player events
```

Events **before** `PS_EV_MAX` can be predicted client-side for responsiveness. Events **after** `PS_EV_MAX` are server-authoritative only.

**Predictable Events:**
- Footsteps
- Water splashes
- Jumps and landings
- Weapon events (fire, reload, switch)

**Non-Predictable Events:**
- Teleports (require server authority)
- Item respawns
- Login/logout effects

### Client Prediction Example

```cpp
// Client-side prediction code (in clgame)
void CLG_PredictMovement() {
    // Predict footsteps locally
    if (should_play_footstep()) {
        predictedPlayerState.event = EV_PLAYER_FOOTSTEP;
        // Play sound immediately for responsiveness
    }
    // Server will confirm or correct
}
```

## Setting Events Server-Side

### Basic Event Setting

```cpp
// Set event on entity
entity->s.event = EV_WEAPON_PRIMARY_FIRE;

// Event will be networked to clients in next update
```

### Event Change Detection

The engine automatically increments event bits when you set a new event or set the same event again:

```cpp
// First fire
entity->s.event = EV_WEAPON_PRIMARY_FIRE;  // Bits: 00

// Second fire (same event, but bits change)
entity->s.event = EV_WEAPON_PRIMARY_FIRE;  // Bits: 01

// Third fire
entity->s.event = EV_WEAPON_PRIMARY_FIRE;  // Bits: 10
```

Clients detect the bit change and know to replay the effect.

### Event Timing

```cpp
// Events are sent with entity updates
// They remain active for EVENT_VALID_MSEC (150ms)

void Think() {
    // Set event
    s.event = EV_WEAPON_PRIMARY_FIRE;
    
    // Event is automatically cleared after EVENT_VALID_MSEC
    // No manual clearing needed
}
```

## Processing Events Client-Side

Events are processed in `src/baseq2rtxp/clgame/clg_events.cpp`:

```cpp
void CLG_ProcessEntityEvent(centity_t *cent, int event) {
    int eventValue = SG_GetEntityEventValue(event);
    
    switch (eventValue) {
        case EV_PLAYER_FOOTSTEP:
            CLG_PlayFootstepSound(cent);
            break;
            
        case EV_WEAPON_PRIMARY_FIRE:
            CLG_PlayMuzzleFlash(cent);
            CLG_PlayFireSound(cent);
            break;
            
        case EV_PLAYER_TELEPORT:
            CLG_TeleportEffect(cent->current.origin);
            break;
            
        // ... handle other events ...
    }
}
```

## Best Practices

### When to Use Entity Events

✅ **Use entity events for:**
- Effects tied to entity actions (weapon fire, footsteps)
- Player feedback (jumps, landings, water splashes)
- Synchronized audio-visual effects
- Effects that need entity position/orientation

❌ **Don't use entity events for:**
- Effects at arbitrary positions (use temp entities instead)
- Long-duration effects (use regular entities)
- Effects that need to persist beyond 150ms

### Performance Considerations

```cpp
// GOOD: Efficient - event piggybacks on entity update
void FireWeapon() {
    s.event = EV_WEAPON_PRIMARY_FIRE;
    // Muzzle flash and sound handled client-side
}

// BAD: Inefficient - creates separate network message
void FireWeapon() {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_GUNSHOT);
    gi.WritePosition(muzzle_origin);
    gi.multicast(muzzle_origin, MULTICAST_PHS);
    // Less efficient than entity event
}
```

### Event Chaining

```cpp
// AVOID: Don't set multiple events in one frame
// Only the last event will be seen
void BadExample() {
    s.event = EV_WEAPON_RELOAD;      // Lost!
    s.event = EV_WEAPON_PRIMARY_FIRE; // Only this is sent
}

// CORRECT: Space events across frames
void GoodExample() {
    if (reloading) {
        s.event = EV_WEAPON_RELOAD;
    } else if (firing) {
        s.event = EV_WEAPON_PRIMARY_FIRE;
    }
}
```

## Event String Names

For debugging purposes, event names are available:

```cpp
extern const char *sg_event_string_names[EV_GAME_MAX];

// Usage
Com_Printf("Entity event: %s\n", sg_event_string_names[eventValue]);
```

## Related Documentation

- [**Entity System Overview**](Entity-System-Overview.md) - Entity architecture
- [**Temp Entity Overview**](Temp-Entity-Overview.md) - Temporary entity system comparison
- [**Entity Networking**](Entity-Networking.md) - How entity state is networked
- [**Client Game Module**](Client-Game-Module.md) - Client-side event processing
- [**API - Entity Types**](API-Entity-Types.md) - Entity type reference
- [**API - Entity Flags**](API-Entity-Flags.md) - Entity flag reference

## Summary

Entity events provide a network-efficient way to trigger client-side effects synchronized with server-side entity actions. They are:

- **Attached to entities**: Events occur at entity origins
- **Network efficient**: Piggyback on entity state updates
- **Time-limited**: Valid for 150ms
- **Change detected**: Bit flags distinguish repeated events
- **Partially predictable**: Some events can be predicted client-side

Use entity events for entity-relative effects like footsteps, weapon fire, and teleports. For independent positional effects, use [temporary entities](Temp-Entity-Overview.md) instead.
