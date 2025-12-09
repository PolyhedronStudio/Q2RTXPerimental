# Using Temporary Entities

This guide explains how to spawn and use temporary entities in your Q2RTXPerimental game code. Temporary entities are network-efficient, one-shot visual effects.

## Quick Start

### Basic Temp Entity Spawn

```cpp
// Include the temp entity header
#include "sharedgame/sg_tempentity_events.h"

void Fire_Bullet(const vec3_t &impact_point, const vec3_t &surface_normal) {
    // 1. Write the temp entity message type
    gi.WriteByte(svc_temp_entity);
    
    // 2. Write the specific temp entity event
    gi.WriteByte(TE_GUNSHOT);
    
    // 3. Write event-specific parameters
    gi.WritePosition(impact_point);
    gi.WriteDir(surface_normal);
    
    // 4. Send to clients that can see this location
    gi.multicast(impact_point, MULTICAST_PVS);
}
```

That's it! The client will receive this message and spawn the appropriate visual effect.

## The Message Writing API

### gi.WriteByte(value)

Writes a single byte (0-255) to the network message.

```cpp
gi.WriteByte(svc_temp_entity);  // Message type
gi.WriteByte(TE_GUNSHOT);       // Event type
gi.WriteByte(particle_count);    // Parameter
```

### gi.WritePosition(vec3_t)

Writes a 3D position with compression (16-bit per axis).

```cpp
vec3_t explosion_pos = {128.0f, 256.0f, 64.0f};
gi.WritePosition(explosion_pos);
```

**Precision:** ~1/8 unit (0.125 unit) accuracy

**Range:** -4096 to +4096 units per axis

### gi.WriteDir(vec3_t)

Writes a normalized direction vector (compressed to 1 byte).

```cpp
vec3_t surface_normal = {0.0f, 0.0f, 1.0f};  // Pointing up
gi.WriteDir(surface_normal);
```

**Note:** Direction must be normalized (length = 1.0) for proper encoding.

### gi.WriteShort(value)

Writes a 16-bit integer (-32768 to 32767).

```cpp
gi.WriteShort(particle_count);  // For effects that need counts > 255
```

### gi.WriteString(text)

Writes a null-terminated string.

```cpp
gi.WriteString("sound/weapons/shotgun.wav");
```

**Rarely used** for temp entities (more common for other message types).

## Multicast Functions

After writing a temp entity message, you must send it using `gi.multicast()`:

```cpp
void gi.multicast(const vec3_t &origin, multicast_t to);
```

### Multicast Types

#### MULTICAST_PVS (Most Common)

**Potentially Visible Set** - Send to clients that *might* see this location.

```cpp
gi.multicast(impact_point, MULTICAST_PVS);
```

**Use for:**
- Visual effects (sparks, blood, impacts)
- Effects with accompanying sounds that are < 512 units range
- Most temp entities

**Characteristics:**
- Based on BSP visibility data
- Very efficient
- Clients around corners won't receive (unless visible)

#### MULTICAST_PHS

**Potentially Hearable Set** - Send to clients that *might* hear this location.

```cpp
gi.multicast(explosion_origin, MULTICAST_PHS);
```

**Use for:**
- Loud explosions
- Sounds with > 512 unit attenuation
- Effects that can be heard through walls

**Characteristics:**
- Larger area than PVS
- Still efficient
- Better for audio-focused events

#### MULTICAST_ALL

**All Clients** - Send to every connected client.

```cpp
gi.multicast(vec3_origin, MULTICAST_ALL);
```

**Use for:**
- Global announcements
- Server-wide events
- Effects everyone should see regardless of location

**Warning:** Expensive! Use sparingly.

#### MULTICAST_ALL_R

**All Clients, Reliable** - Guaranteed delivery to all clients.

```cpp
gi.multicast(vec3_origin, MULTICAST_ALL_R);
```

**Use for:**
- Critical events that must be seen
- Game state changes

**Warning:** Very expensive! Almost never needed for temp entities.

## Common Temp Entity Patterns

### Bullet Impact

```cpp
void Fire_Bullet_Impact(const vec3_t &point, const vec3_t &normal, 
                         int surface_type) {
    gi.WriteByte(svc_temp_entity);
    
    // Choose effect based on surface
    if (surface_type == SURF_METAL) {
        gi.WriteByte(TE_BULLET_SPARKS);
    } else {
        gi.WriteByte(TE_GUNSHOT);
    }
    
    gi.WritePosition(point);
    gi.WriteDir(normal);
    gi.multicast(point, MULTICAST_PVS);
}
```

### Blood Splatter

```cpp
void Spawn_Blood(const vec3_t &origin, const vec3_t &dir, int damage) {
    gi.WriteByte(svc_temp_entity);
    
    // More blood for higher damage
    if (damage > 50) {
        gi.WriteByte(TE_MOREBLOOD);
    } else {
        gi.WriteByte(TE_BLOOD);
    }
    
    gi.WritePosition(origin);
    gi.WriteDir(dir);
    gi.multicast(origin, MULTICAST_PVS);
}
```

### Water Splash

```cpp
void Water_Splash(const vec3_t &origin, int liquid_type) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_SPLASH);
    
    // Particle count
    gi.WriteByte(8);
    
    gi.WritePosition(origin);
    gi.WriteDir(vec3_up);  // Splash upward
    
    // Liquid type (SPLASH_BLUE_WATER, SPLASH_SLIME, etc.)
    gi.WriteByte(liquid_type);
    
    gi.multicast(origin, MULTICAST_PVS);
}
```

### Explosion

```cpp
void Create_Explosion(const vec3_t &origin) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_PLAIN_EXPLOSION);
    gi.WritePosition(origin);
    
    // Use PHS for explosions so sound carries further
    gi.multicast(origin, MULTICAST_PHS);
}
```

### Spark Effects

```cpp
void Create_Sparks(const vec3_t &origin, const vec3_t &direction, 
                   spark_type_t type) {
    gi.WriteByte(svc_temp_entity);
    
    switch (type) {
        case SPARK_GENERIC:
            gi.WriteByte(TE_SPARKS);
            break;
        case SPARK_ELECTRIC:
            gi.WriteByte(TE_ELECTRIC_SPARKS);
            break;
        case SPARK_LASER:
            gi.WriteByte(TE_LASER_SPARKS);
            gi.WritePosition(origin);
            gi.WriteDir(direction);
            gi.WriteByte(0xd0);  // Green color
            gi.multicast(origin, MULTICAST_PVS);
            return;
    }
    
    gi.WritePosition(origin);
    gi.WriteDir(direction);
    gi.multicast(origin, MULTICAST_PVS);
}
```

### Steam Effect

```cpp
void Create_Steam_Puff(const vec3_t &origin, const vec3_t &direction) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_STEAM);
    gi.WritePosition(origin);
    gi.WriteDir(direction);
    
    // Steam parameters
    gi.WriteByte(0xe0);     // Color (white)
    gi.WriteShort(20);      // Particle count
    gi.WriteByte(8);        // Magnitude/intensity
    
    gi.multicast(origin, MULTICAST_PVS);
}
```

### Bubble Trail

```cpp
void Create_Bubble_Trail(const vec3_t &start, const vec3_t &end) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_BUBBLETRAIL);
    gi.WritePosition(start);
    gi.WritePosition(end);
    gi.multicast(start, MULTICAST_PVS);
}
```

### Teleport Effect

```cpp
void Spawn_Teleport_Effect(const vec3_t &origin) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_TELEPORT_EFFECT);
    gi.WritePosition(origin);
    gi.multicast(origin, MULTICAST_PVS);
}
```

## Advanced Techniques

### Conditional Effects Based on Game Settings

```cpp
void Spawn_Blood_Effect(const vec3_t &origin, const vec3_t &dir, int damage) {
    // Check if blood is enabled
    if (!g_blood->value) {
        return;  // Don't spawn blood effects
    }
    
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(damage > 50 ? TE_MOREBLOOD : TE_BLOOD);
    gi.WritePosition(origin);
    gi.WriteDir(dir);
    gi.multicast(origin, MULTICAST_PVS);
}
```

### Surface-Based Effects

```cpp
void Weapon_Impact_Effect(const vec3_t &point, const vec3_t &normal,
                          const trace_t &trace) {
    gi.WriteByte(svc_temp_entity);
    
    // Determine surface type from trace
    if (trace.surface && (trace.surface->flags & SURF_METAL)) {
        gi.WriteByte(TE_BULLET_SPARKS);
    } else if (trace.contents & CONTENTS_WATER) {
        gi.WriteByte(TE_SPLASH);
        gi.WriteByte(8);
        gi.WritePosition(point);
        gi.WriteDir(vec3_up);
        gi.WriteByte(SPLASH_BLUE_WATER);
        gi.multicast(point, MULTICAST_PVS);
        return;
    } else {
        gi.WriteByte(TE_GUNSHOT);
    }
    
    gi.WritePosition(point);
    gi.WriteDir(normal);
    gi.multicast(point, MULTICAST_PVS);
}
```

### Batching Multiple Effects

```cpp
void Shotgun_Impact(const vec3_t &origin, const vec3_t &dir) {
    // Spawn blood
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_MOREBLOOD);
    gi.WritePosition(origin);
    gi.WriteDir(dir);
    gi.multicast(origin, MULTICAST_PVS);
    
    // Spawn extra blood particles around impact
    for (int i = 0; i < 3; i++) {
        vec3_t offset;
        offset[0] = origin[0] + crandom() * 10;
        offset[1] = origin[1] + crandom() * 10;
        offset[2] = origin[2] + crandom() * 10;
        
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_BLOOD);
        gi.WritePosition(offset);
        gi.WriteDir(dir);
        gi.multicast(origin, MULTICAST_PVS);
    }
}
```

### Distance-Based Quality

```cpp
void Distant_Spark_Effect(const vec3_t &origin, const vec3_t &viewer_pos) {
    float distance = VectorDistance(origin, viewer_pos);
    
    // Skip effect if too far away
    if (distance > 2048.0f) {
        return;
    }
    
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_SPARKS);
    gi.WritePosition(origin);
    gi.WriteDir(vec3_up);
    gi.multicast(origin, MULTICAST_PVS);
}
```

## Helper Functions

### Creating Reusable Effect Functions

```cpp
// Generic temp entity spawner
void SVG_SpawnTempEntity(int te_type, const vec3_t &origin, 
                         const vec3_t &direction = vec3_origin) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(te_type);
    gi.WritePosition(origin);
    
    if (direction != vec3_origin) {
        gi.WriteDir(direction);
    }
    
    gi.multicast(origin, MULTICAST_PVS);
}

// Usage
SVG_SpawnTempEntity(TE_GUNSHOT, impact_point, surface_normal);
```

### Effect Manager Class

```cpp
class EffectManager {
public:
    static void BulletImpact(const vec3_t &pos, const vec3_t &normal, 
                             surf_type_t surface) {
        int effect = (surface == SURF_METAL) ? TE_BULLET_SPARKS : TE_GUNSHOT;
        SpawnEffect(effect, pos, normal);
    }
    
    static void Blood(const vec3_t &pos, const vec3_t &dir, int damage) {
        int effect = (damage > 50) ? TE_MOREBLOOD : TE_BLOOD;
        SpawnEffect(effect, pos, dir);
    }
    
private:
    static void SpawnEffect(int type, const vec3_t &pos, const vec3_t &dir) {
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(type);
        gi.WritePosition(pos);
        gi.WriteDir(dir);
        gi.multicast(pos, MULTICAST_PVS);
    }
};
```

## Common Mistakes

### ❌ Forgetting to Call multicast()

```cpp
// WRONG: Effect is written but never sent!
void BadExample() {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_GUNSHOT);
    gi.WritePosition(origin);
    gi.WriteDir(normal);
    // Missing: gi.multicast(origin, MULTICAST_PVS);
}
```

### ❌ Wrong Parameter Order

```cpp
// WRONG: Parameters in wrong order
gi.WriteByte(svc_temp_entity);
gi.WriteByte(TE_STEAM);
gi.WriteDir(direction);        // Direction before position - WRONG!
gi.WritePosition(origin);
gi.WriteByte(0xe0);
```

Check the [Temp Entity Event Types](Temp-Entity-Event-Types.md) reference for correct parameter order.

### ❌ Non-Normalized Direction Vector

```cpp
// WRONG: Direction not normalized
vec3_t dir = {100.0f, 50.0f, 25.0f};  // Length >> 1.0
gi.WriteDir(dir);  // Will encode incorrectly!

// CORRECT: Normalize first
VectorNormalize(dir);
gi.WriteDir(dir);  // Now correct
```

### ❌ Using MULTICAST_ALL for Everything

```cpp
// WRONG: Expensive and unnecessary
gi.multicast(origin, MULTICAST_ALL);

// CORRECT: Use PVS for most effects
gi.multicast(origin, MULTICAST_PVS);
```

### ❌ Spawning Too Many Effects

```cpp
// WRONG: Can cause network congestion
void BadShrapnel() {
    for (int i = 0; i < 100; i++) {  // 100 temp entities!
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_SPARKS);
        // ... more writes ...
        gi.multicast(origin, MULTICAST_PVS);
    }
}

// CORRECT: Reasonable effect count
void GoodShrapnel() {
    for (int i = 0; i < 5; i++) {  // 5-10 is usually enough
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_SPARKS);
        // ... more writes ...
        gi.multicast(origin, MULTICAST_PVS);
    }
}
```

## Performance Guidelines

### Temp Entity Budget

**Per frame, per visible area:**
- **Good:** 0-10 temp entities
- **Acceptable:** 10-30 temp entities
- **Excessive:** 30+ temp entities

### When to Throttle

```cpp
// Throttle rapid-fire temp entities
float last_spark_time = 0;

void Create_Continuous_Sparks(const vec3_t &origin) {
    float current_time = level.time;
    
    // Only spawn every 0.1 seconds (10 Hz)
    if (current_time - last_spark_time < 0.1f) {
        return;
    }
    
    last_spark_time = current_time;
    
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_SPARKS);
    gi.WritePosition(origin);
    gi.WriteDir(vec3_up);
    gi.multicast(origin, MULTICAST_PVS);
}
```

### Network Efficiency

```cpp
// GOOD: One multicast per effect
void EfficientEffect() {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_GUNSHOT);
    gi.WritePosition(pos);
    gi.WriteDir(normal);
    gi.multicast(pos, MULTICAST_PVS);  // Sends to ~5-15 clients
}

// BAD: Multiple unicasts (don't do this!)
void InefficientEffect() {
    for (each client in visible_clients) {
        gi.unicast(client, false);  // Sends separately to each client!
    }
}
```

## Debugging Temp Entities

### Enable Developer Console

```
developer 1
```

### Console Commands

```
// Show temp entity spawns
set g_debug_temp_entities 1

// Show PVS/PHS areas
r_drawpvs 1
```

### Debug Trail

```cpp
void Debug_Projectile_Path(const vec3_t &start, const vec3_t &end) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_DEBUGTRAIL);
    gi.WritePosition(start);
    gi.WritePosition(end);
    gi.multicast(start, MULTICAST_ALL);  // Make sure everyone sees it
}
```

## Related Documentation

- [**Temp Entity Overview**](Temp-Entity-Overview.md) - When to use temp entities
- [**Temp Entity Event Types**](Temp-Entity-Event-Types.md) - Complete event reference
- [**Custom Temp Entities**](Custom-Temp-Entities.md) - Creating new temp entity types
- [**Client Game Module**](Client-Game-Module.md) - How clients process temp entities
- [**Server Game Module**](Server-Game-Module.md) - Server-side game logic
- [**API - Entity Events**](API-Entity-Events.md) - Entity events vs. temp entities

## Summary

To use temporary entities:

1. **Write message type**: `gi.WriteByte(svc_temp_entity)`
2. **Write event type**: `gi.WriteByte(TE_GUNSHOT)` (or other event)
3. **Write parameters**: Position, direction, etc. (event-specific)
4. **Send to clients**: `gi.multicast(origin, MULTICAST_PVS)` (or other multicast type)

**Key Points:**
- Always call `multicast()` after writing the effect
- Use `MULTICAST_PVS` for most effects (efficient)
- Normalize direction vectors before writing
- Don't spawn too many effects (< 30 per frame per area)
- Check the [Event Types](Temp-Entity-Event-Types.md) reference for parameter order

Temporary entities are the most efficient way to create short-lived visual effects in Q2RTXPerimental!
