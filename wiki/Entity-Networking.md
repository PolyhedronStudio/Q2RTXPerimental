# Entity Networking

How entities are synchronized from server to clients in Q2RTXPerimental.

## Overview

Q2RTXPerimental uses a **client-server architecture** where:
- **Server** runs authoritative game logic
- **Clients** render what the server tells them

Entity state is transmitted via **delta compression** - only changes are sent.

## Entity State Structure

```cpp
typedef struct entity_state_s {
    int number;              // Entity index
    vec3_t origin;          // Position
    vec3_t angles;          // Orientation
    vec3_t old_origin;      // For lerping
    int modelindex;         // Model to render
    int modelindex2;        // Weapon model
    int modelindex3;        // Custom models
    int modelindex4;
    int frame;              // Animation frame
    int skinnum;            // Skin/texture
    int effects;            // Visual effects (EF_ROTATE, etc.)
    int renderfx;           // Rendering flags (RF_GLOW, etc.)
    int solid;              // Collision (for prediction)
    int sound;              // Looping sound
    int event;              // Entity event
} entity_state_t;
```

**What gets networked:**
- Position, angles, velocity
- Model, frame, skin
- Visual effects
- Events

**What DOESN'T get networked:**
- Entity health (except player)
- AI state
- Think callbacks
- Server-only logic

## Networking Priority

Entities have different update priorities:

### High Priority (Every Frame)

```cpp
ET_PLAYER               // Players - always sent
```

### Normal Priority (Delta Compressed)

```cpp
ET_MONSTER              // Monsters
ET_ITEM                 // Items
ET_PUSHER               // Moving platforms
```

### Low Priority (Rare Updates)

```cpp
ET_GENERAL              // Static entities
```

## Delta Compression

Only **changed fields** are sent:

```
Frame 1: origin={100, 200, 50}, angles={0, 90, 0}, frame=5
Frame 2: origin={100, 200, 52}  // Only origin changed, send 3 floats

Saves bandwidth vs. sending full state every frame!
```

## Client-Side Processing

### Receiving Entity Updates

Client receives entity snapshots from server:

```cpp
// Client-side (clgame)
void CLG_ParseEntityUpdate(int entnum, entity_state_t *state) {
    centity_t *cent = &cl_entities[entnum];
    
    // Store previous state for interpolation
    cent->prev = cent->current;
    cent->current = *state;
    
    // Interpolate between prev and current for smooth movement
}
```

### Interpolation

Clients interpolate between snapshots for smooth movement:

```cpp
// Render position between snapshots
vec3_t lerped_origin;
LerpVector(prev_origin, current_origin, lerp_fraction, lerped_origin);

// Render at interpolated position
```

**Lerp fraction** based on time between server updates (40 Hz = 25ms).

## Prediction

### Player Prediction

Clients predict their own movement locally for responsiveness:

```cpp
// Client predicts player movement before server confirms
void CLG_PredictMovement() {
    // Run same physics as server
    PM_Move(&pmove);
    
    // Use predicted position for rendering
    // Server will correct if prediction was wrong
}
```

**Benefits:**
- Instant response to input
- Smooth movement even with latency

**Drawbacks:**
- Must reconcile with server state
- Prediction errors cause "rubber-banding"

### Entity Prediction

Some entities (projectiles) can be predicted:

```cpp
// Client predicts projectile path
void CLG_PredictProjectile(centity_t *cent) {
    // Use velocity and gravity to predict next position
    predicted_origin = current_origin + velocity * dt;
    predicted_origin[2] -= 0.5 * gravity * dt * dt;
}
```

## PVS (Potentially Visible Set)

Server only sends entities **potentially visible** to each client:

```cpp
// Server determines PVS for player
byte *pvs = gi.inPVS(player->s.origin);

// Only send entities in PVS
for (each entity) {
    if (entity_in_pvs(entity, pvs)) {
        Send_Entity_Update(player, entity);
    }
}
```

**Benefits:**
- Reduced bandwidth
- Better performance
- Prevents cheating (can't see through walls)

## Entity Events

**One-shot events** synchronized with entity updates:

```cpp
// Server sets event
player->s.event = EV_PLAYER_FOOTSTEP;

// Client processes event
void CLG_EntityEvent(int event) {
    switch (event) {
        case EV_PLAYER_FOOTSTEP:
            Play_Footstep_Sound();
            break;
    }
}
```

Events use **incrementing bits** to detect repeated identical events.

## Bandwidth Optimization

### Techniques Used

1. **Delta Compression**: Only send changes
2. **PVS Culling**: Only send visible entities
3. **Priority System**: Important entities updated more often
4. **Event System**: One-shot effects don't need continuous updates

### Best Practices

```cpp
// GOOD: Only change networked state when needed
if (new_frame != s.frame) {
    s.frame = new_frame;  // Will be sent
}

// BAD: Changing every frame unnecessarily
s.frame++;  // Sent every frame even if same!
```

## Troubleshooting

### Entity Not Appearing

```cpp
// Check:
1. s.modelindex set? (gi.SetModel or gi.modelindex)
2. Entity linked? (gi.linkentity)
3. Entity in PVS? (clg_drawentities shows entities)
4. Correct entityType? (ET_GENERAL, ET_PLAYER, etc.)
```

### Entity Jittering

```cpp
// Likely causes:
1. Not linking after position change
2. Prediction errors
3. Network packet loss
4. Incorrect interpolation
```

### Entity Lag

```cpp
// Check:
1. Server framerate (should be 40 Hz)
2. Network latency (ping command)
3. PVS updates (entity rapidly entering/leaving PVS)
```

## Related Documentation

- [**Entity System Overview**](Entity-System-Overview.md) - Entity architecture
- [**Entity Lifecycle**](Entity-Lifecycle.md) - Entity lifetime
- [**Client Game Module**](Client-Game-Module.md) - Client-side processing
- [**Server Game Module**](Server-Game-Module.md) - Server-side logic
- [**API - Entity Events**](API-Entity-Events.md) - Entity event reference

## Summary

Entity networking in Q2RTXPerimental:

- **Server authoritative**: Server runs game logic
- **Delta compressed**: Only changes sent to clients
- **PVS culled**: Only visible entities sent
- **Interpolated**: Clients smooth movement between updates
- **Predicted**: Players and some entities predicted client-side
- **Event system**: One-shot effects synchronized efficiently

Only `entity_state_t` fields are networked. Server-only data (health, AI, callbacks) stays on server. This architecture provides smooth gameplay while minimizing bandwidth usage.
