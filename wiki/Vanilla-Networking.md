# Vanilla Networking

Understanding Quake 2's original networking model is essential for comprehending Q2RTXPerimental's enhancements.

> **Note:** This page documents the networking architecture used in both vanilla Q2RTX (`/src/baseq2/`) and Q2RTXPerimental (`/src/baseq2rtxp/`). The core protocol is shared; Q2RTXPerimental adds enhanced entity events and signals.

## Overview

Quake 2 uses a **client-server model** with **delta compression** for efficient entity state transmission.

## Core Concepts

### Client-Server Model

```
Server (Authoritative)
- Runs game logic
- Simulates physics
- Manages entities
- Validates actions
       ↓ State updates
Clients (Render only)
- Predict movement
- Interpolate entities
- Render visuals
- Send commands
```

### Delta Compression

Only **changes** are sent, not full state:

```
Frame 1: Entity at (100, 200, 64), angle 90°, frame 5
Frame 2: Entity at (102, 200, 64)  ← Only X changed, send 4 bytes

vs. sending full state every frame (28+ bytes)
```

**Bandwidth savings:** 85-95% reduction

## Packet Structure

### Client → Server (Usercmd)

Players send **user commands** every frame:

```c
typedef struct usercmd_s {
    byte msec;           // Time since last command
    byte buttons;        // Button states (attack, jump, etc.)
    short angles[3];     // View angles
    short forwardmove;   // Forward/backward
    short sidemove;      // Left/right
    short upmove;        // Jump/crouch
    byte impulse;        // Weapon change, etc.
    byte lightlevel;     // For powerup shells
} usercmd_t;
```

**Size:** 13 bytes
**Frequency:** 40 Hz (Q2RTXPerimental) or 10 Hz (vanilla)

### Server → Client (Snapshot)

Server sends **entity snapshots**:

```c
typedef struct entity_state_s {
    int number;          // Entity index
    vec3_t origin;       // Position
    vec3_t angles;       // Rotation
    vec3_t old_origin;   // For interpolation
    int modelindex;      // Model
    int frame;           // Animation frame
    int skinnum;         // Skin
    int effects;         // Visual effects
    int renderfx;        // Render flags
    int solid;           // Collision (for prediction)
    int sound;           // Looping sound
    int event;           // Entity event
} entity_state_t;
```

**Compressed with delta encoding**

## Snapshot System

### Server Snapshots

Server maintains **circular buffer** of recent game states:

```
Snapshot Buffer (32 frames):
[0] [1] [2] [3] ... [31]
 ↑
Current frame
```

When client acknowledges receiving frame N, server sends delta from frame N to current frame.

### Delta Encoding

```c
// Pseudocode for delta encoding
void WriteDelta(entity_state_t *from, entity_state_t *to) {
    byte flags = 0;
    
    // Check which fields changed
    if (to->origin != from->origin) flags |= U_ORIGIN;
    if (to->angles != from->angles) flags |= U_ANGLE;
    if (to->frame != from->frame) flags |= U_FRAME;
    // ... check all fields ...
    
    // Write flags
    MSG_WriteByte(flags);
    
    // Write only changed fields
    if (flags & U_ORIGIN) MSG_WritePos(to->origin);
    if (flags & U_ANGLE) MSG_WriteAngle(to->angles);
    if (flags & U_FRAME) MSG_WriteByte(to->frame);
    // ... write changed fields ...
}
```

## PVS (Potentially Visible Set)

Server only sends entities **potentially visible** to each client:

```
Map divided into BSP leaf nodes
Each leaf has PVS data - which other leaves are visible

Client in Leaf A:
- Send entities in Leaf A
- Send entities in leaves visible from A (PVS)
- DON'T send entities in non-visible leaves
```

**Benefits:**
- Reduced bandwidth
- Better performance
- Prevents wallhacks

**Example:**
```c
// Server determines what to send
byte *pvs = SV_FatPVS(client->edict->s.origin);

for (each entity) {
    if (SV_inPVS(entity->s.origin, pvs)) {
        // Send entity to client
        WriteEntityDelta(client, entity);
    }
}
```

## Client-Side Prediction

### Movement Prediction

Clients **predict their own movement** for instant response:

```c
void CL_PredictMovement(void) {
    // Start from last acknowledged server frame
    usercmd_t cmd = current_cmd;
    
    // Run local physics simulation
    pmove_t pm;
    pm.s = cl.predicted_state;
    pm.cmd = cmd;
    Pmove(&pm);  // Same physics as server
    
    // Use predicted position for rendering
    cl.predicted_origin = pm.s.origin;
}
```

**Benefits:**
- Instant response to input
- Smooth movement despite latency

**Drawbacks:**
- Prediction errors cause "rubber-banding"
- Must reconcile with server state

### Prediction Reconciliation

```c
void CL_CheckPredictionError(void) {
    // Compare predicted position to server position
    VectorSubtract(predicted_origin, server_origin, error);
    
    float error_dist = VectorLength(error);
    if (error_dist > 4.0) {
        // Prediction was wrong, snap to server position
        VectorCopy(server_origin, predicted_origin);
    } else if (error_dist > 0.5) {
        // Small error, smooth correction over next frame
        VectorMA(predicted_origin, -0.1, error, predicted_origin);
    }
}
```

## Interpolation

### Entity Interpolation

Clients interpolate between snapshots for smooth movement:

```
Server sends snapshots at 10 Hz (every 100ms)

Client renders at 60 Hz (every 16ms)

Need to interpolate between snapshots:
Frame at 0ms, Frame at 100ms
↓
Render at 16ms: Lerp 16% between frames
Render at 33ms: Lerp 33% between frames
Render at 50ms: Lerp 50% between frames
...
```

**Implementation:**
```c
void CL_AddEntity(centity_t *cent) {
    float lerp = (cl.time - cent->prev.servertime) / 
                 (cent->current.servertime - cent->prev.servertime);
    
    // Interpolate position
    vec3_t render_origin;
    LerpVector(cent->prev.origin, cent->current.origin, 
               lerp, render_origin);
    
    // Interpolate angles
    vec3_t render_angles;
    LerpAngles(cent->prev.angles, cent->current.angles,
               lerp, render_angles);
    
    // Render at interpolated position
    AddEntityToScene(render_origin, render_angles, /*...*/);
}
```

## Reliable vs. Unreliable Messages

### Unreliable (Most common)

**Characteristics:**
- Fast, low overhead
- No delivery guarantee
- Sent via UDP

**Use for:**
- Entity positions (resent next frame anyway)
- Movement commands (new command coming)
- Temp entities (one-shot effects)

### Reliable

**Characteristics:**
- Guaranteed delivery
- Acknowledged by receiver
- Higher overhead

**Use for:**
- Player connection
- Server commands
- Configuration changes
- Game state changes

## Network Optimizations

### 1. Entity Priorities

More important entities updated more frequently:

```c
// Players: Every frame
if (ent->client) {
    Send_Entity(ent);
}

// Monsters: Every frame if moving, else every 4 frames
else if (ent->svflags & SVF_MONSTER) {
    if (ent->velocity != 0 || frame_count % 4 == 0) {
        Send_Entity(ent);
    }
}

// Static entities: Only when state changes
else {
    if (ent->state_changed) {
        Send_Entity(ent);
    }
}
```

### 2. Event System

One-shot effects use **events** instead of entity updates:

```c
// BAD: Create temp entity, update for 10 frames, remove
// Cost: 10 entity updates

// GOOD: Send one-time event
ent->s.event = EV_PLAYER_TELEPORT;
// Cost: 1 byte in entity snapshot
```

### 3. Sound Attenuation

Distant sounds sent less frequently or not at all:

```c
float dist = Distance(sound_origin, player_origin);

if (dist < 512) {
    Send_Sound();  // Always send
} else if (dist < 1024 && frame_count % 2 == 0) {
    Send_Sound();  // Send every other frame
} else if (dist < 2048 && frame_count % 4 == 0) {
    Send_Sound();  // Send every 4 frames
}
// else: Don't send (too far away)
```

## Lag Compensation

### Client-Side

**Extrapolation:** Predict forward when packet is late
```c
if (packet_late) {
    // Continue moving in last known direction
    predicted_pos += velocity * time_late;
}
```

**Interpolation:** Smooth between old and new states
```c
// Render 100ms in the past to have stable snapshots
render_time = current_time - 100ms;
```

### Server-Side

**Lag Compensation for Hit Detection:**
```c
// Rewind entity positions to when player fired
void Rewind_Entities(float player_ping) {
    for (each entity) {
        // Move entity back in time based on ping
        entity->temp_pos = entity->position_at(now - player_ping);
    }
    
    // Check hit detection with rewound positions
    bool hit = CheckHit(player_aim, rewound_positions);
    
    // Restore current positions
    Restore_Entity_Positions();
}
```

## Network CVars

Key console variables:

```
cl_maxfps        // Client frame rate (60-125)
rate             // Max bytes/sec from server (25000)
cl_predict       // Enable client prediction (1)
cl_shownet       // Show network debug info (0-3)
```

## Differences in Q2RTXPerimental

| Vanilla | Q2RTXPerimental |
|---------|-----------------|
| 10 Hz tickrate | 40 Hz tickrate |
| Lower precision | Higher precision |
| Basic prediction | Enhanced prediction |
| Original PVS | Optimized PVS |

## Related Documentation

- [**Entity Networking**](Entity-Networking.md) - Q2RTXPerimental networking
- [**Vanilla Game Module Architecture**](Vanilla-Game-Module-Architecture.md) - Module structure
- [**Client Game Module**](Client-Game-Module.md) - Client-side processing
- [**Server Game Module**](Server-Game-Module.md) - Server-side logic

## Summary

Vanilla Quake 2 networking:

- **Client-server model**: Server authoritative
- **Delta compression**: Only send changes (85-95% bandwidth reduction)
- **PVS culling**: Only send visible entities
- **Client prediction**: Predict movement for instant response
- **Interpolation**: Smooth movement between snapshots
- **10 Hz tickrate**: 100ms per server frame
- **Lag compensation**: Rewind for hit detection

This efficient networking model enabled smooth multiplayer gameplay over 1990s dial-up connections and remains effective today with Q2RTXPerimental's enhancements.
