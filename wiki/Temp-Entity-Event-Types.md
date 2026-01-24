# Temp Entity Event Types Reference

Complete reference for all temporary entity event types in Q2RTXPerimental. Temporary entities are defined in `src/baseq2rtxp/sharedgame/sg_tempentity_events.h` and processed client-side in `src/baseq2rtxp/clgame/clg_temp_entities.cpp`.

## Event Type Enumeration

**Location:** `src/baseq2rtxp/sharedgame/sg_tempentity_events.h`

```cpp
typedef enum temp_entity_event_e {
    TE_GUNSHOT,
    TE_BLOOD,
    TE_MOREBLOOD,
    TE_BUBBLETRAIL,
    TE_BUBBLETRAIL2,
    TE_SPLASH,
    TE_STEAM,
    TE_HEATBEAM_STEAM,
    TE_SPARKS,
    TE_HEATBEAM_SPARKS,
    TE_BULLET_SPARKS,
    TE_ELECTRIC_SPARKS,
    TE_LASER_SPARKS,
    TE_TUNNEL_SPARKS,
    TE_WELDING_SPARKS,
    TE_FLAME,
    TE_PLAIN_EXPLOSION,
    TE_TELEPORT_EFFECT,
    TE_FLASHLIGHT,
    TE_DEBUG_TRAIL,
    TE_NUM_ENTITY_EVENTS
} temp_entity_event_t;
```

## Impact and Hit Effects

### TE_GUNSHOT

**Description:** Bullet impact on solid surfaces

**Parameters:**
- `origin` (vec3_t): Impact location
- `dir` (vec3_t): Surface normal direction

**Server-Side Usage:**
```cpp
void Fire_Bullet_Impact(const vec3_t &point, const vec3_t &normal) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_GUNSHOT);
    gi.WritePosition(point);
    gi.WriteDir(normal);
    gi.multicast(point, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- Gray/brown puff particles
- Impact smoke
- Dust cloud
- Surface-appropriate debris

**Use Cases:**
- Pistol impacts
- SMG impacts
- Any hitscan weapon hit
- Ricochet points

### TE_BULLET_SPARKS

**Description:** Bullet impact on metal surfaces (ricochet)

**Parameters:**
- `origin` (vec3_t): Impact location
- `dir` (vec3_t): Surface normal direction

**Server-Side Usage:**
```cpp
void Fire_Bullet_Metal_Impact(const vec3_t &point, const vec3_t &normal) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_BULLET_SPARKS);
    gi.WritePosition(point);
    gi.WriteDir(normal);
    gi.multicast(point, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- Bright yellow/orange sparks
- Metal ricochet sound
- Sparks bounce off surface
- Short-lived glow

**Use Cases:**
- Bullets hitting metal
- Ricochets off vehicles
- Hits on metal doors/walls
- Industrial environments

## Blood Effects

### TE_BLOOD

**Description:** Small blood splatter from minor damage

**Parameters:**
- `origin` (vec3_t): Damage location
- `dir` (vec3_t): Damage direction

**Server-Side Usage:**
```cpp
void T_Damage(svg_base_edict_t *targ, int damage, const vec3_t &point) {
    if (targ->svflags & SVF_MONSTER || targ->client) {
        // Spawn blood effect
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_BLOOD);
        gi.WritePosition(point);
        gi.WriteDir(damage_dir);
        gi.multicast(point, MULTICAST_PVS);
    }
}
```

**Client-Side Effect:**
- 10-20 red blood particles
- Particles spray in damage direction
- Blood droplets fall with gravity
- Short particle lifetime (~0.5s)

**Use Cases:**
- Light weapon damage
- Single bullet hits
- Minor monster damage
- Melee attacks

### TE_MOREBLOOD

**Description:** Large blood splatter from heavy damage

**Parameters:**
- `origin` (vec3_t): Damage location
- `dir` (vec3_t): Damage direction

**Server-Side Usage:**
```cpp
void T_Damage(svg_base_edict_t *targ, int damage, const vec3_t &point) {
    if (damage > 50) {
        // Heavy damage - more blood
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_MOREBLOOD);
        gi.WritePosition(point);
        gi.WriteDir(damage_dir);
        gi.multicast(point, MULTICAST_PVS);
    }
}
```

**Client-Side Effect:**
- 40-60 red blood particles (2-3x more than TE_BLOOD)
- Wider spray pattern
- Longer particle lifetime (~1s)
- More dramatic effect

**Use Cases:**
- Shotgun blasts
- Explosive damage
- High-damage weapons
- Fatal hits

## Water and Liquid Effects

### TE_SPLASH

**Description:** Splash effects for various liquid types

**Parameters:**
- `origin` (vec3_t): Splash location
- `dir` (vec3_t): Splash direction (usually up)
- `color` (byte): Splash type (SPLASH_* constant)
- `count` (byte): Number of particles

**Splash Type Constants:**
```cpp
static constexpr int32_t SPLASH_UNKNOWN     = 0;
static constexpr int32_t SPLASH_SPARKS      = 1;  // Metal/sparks
static constexpr int32_t SPLASH_BLUE_WATER  = 2;  // Clean water
static constexpr int32_t SPLASH_BROWN_WATER = 3;  // Dirty water
static constexpr int32_t SPLASH_SLIME       = 4;  // Green slime
static constexpr int32_t SPLASH_LAVA        = 5;  // Orange lava
static constexpr int32_t SPLASH_BLOOD       = 6;  // Red blood
```

**Server-Side Usage:**
```cpp
void Splash(const vec3_t &origin, const vec3_t &dir, int type, int count) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_SPLASH);
    gi.WriteByte(count);
    gi.WritePosition(origin);
    gi.WriteDir(dir);
    gi.WriteByte(type);
    gi.multicast(origin, MULTICAST_PVS);
}

// Example: Bullet hitting water
Splash(impact_point, vec3_up, SPLASH_BLUE_WATER, 8);
```

**Client-Side Effects by Type:**
- **SPLASH_BLUE_WATER**: Blue particles, water ripples, splash sound
- **SPLASH_BROWN_WATER**: Brown particles, muddy splash
- **SPLASH_SLIME**: Green particles, toxic splash sound
- **SPLASH_LAVA**: Orange/yellow particles, burning sound, fire effects
- **SPLASH_BLOOD**: Red particles, wet impact sound

**Use Cases:**
- Bullets hitting water
- Player/monster entering water
- Projectiles impacting liquid surfaces
- Environmental liquid effects

### TE_BUBBLETRAIL

**Description:** Underwater bubble trail

**Parameters:**
- `start` (vec3_t): Trail start position
- `end` (vec3_t): Trail end position

**Server-Side Usage:**
```cpp
void Create_BubbleTrail(const vec3_t &start, const vec3_t &end) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_BUBBLETRAIL);
    gi.WritePosition(start);
    gi.WritePosition(end);
    gi.multicast(start, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- Rising bubbles along path
- Bubbles accelerate upward
- Wobble animation
- Pop when reaching surface

**Use Cases:**
- Underwater projectile trails
- Underwater movement
- Submarine vehicle trails
- Diver air bubbles

### TE_BUBBLETRAIL2

**Description:** Alternate underwater bubble trail (typically denser)

**Parameters:**
- `start` (vec3_t): Trail start position
- `end` (vec3_t): Trail end position

**Server-Side Usage:**
```cpp
void Create_Dense_BubbleTrail(const vec3_t &start, const vec3_t &end) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_BUBBLETRAIL2);
    gi.WritePosition(start);
    gi.WritePosition(end);
    gi.multicast(start, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- More bubbles than TE_BUBBLETRAIL
- Faster rising animation
- Suitable for high-speed underwater movement

**Use Cases:**
- Fast underwater projectiles
- Underwater explosions
- Torpedo trails

## Spark Effects

### TE_SPARKS

**Description:** Generic white/yellow sparks

**Parameters:**
- `origin` (vec3_t): Spark origin
- `dir` (vec3_t): Spark direction

**Server-Side Usage:**
```cpp
void Create_Sparks(const vec3_t &pos, const vec3_t &dir) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_SPARKS);
    gi.WritePosition(pos);
    gi.WriteDir(dir);
    gi.multicast(pos, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- Yellow/white spark particles
- Emit in cone from direction
- Gravity-affected
- Short lifetime

**Use Cases:**
- General impacts
- Machinery damage
- Metal collisions
- Generic effects

### TE_ELECTRIC_SPARKS

**Description:** Blue/white electrical sparks

**Parameters:**
- `origin` (vec3_t): Spark origin
- `dir` (vec3_t): Primary direction

**Server-Side Usage:**
```cpp
void Create_Electric_Sparks(const vec3_t &pos, const vec3_t &dir) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_ELECTRIC_SPARKS);
    gi.WritePosition(pos);
    gi.WriteDir(dir);
    gi.multicast(pos, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- Blue/cyan spark particles
- Electric arc visual
- Crackling sound
- More erratic movement than regular sparks

**Use Cases:**
- Electrical damage
- EMP effects
- Short circuits
- Energy weapon impacts
- Damaged electronics

### TE_LASER_SPARKS

**Description:** Laser impact sparks (typically green/cyan)

**Parameters:**
- `origin` (vec3_t): Impact point
- `dir` (vec3_t): Surface normal
- `color` (byte): Particle color

**Server-Side Usage:**
```cpp
void Laser_Impact(const vec3_t &pos, const vec3_t &normal) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_LASER_SPARKS);
    gi.WritePosition(pos);
    gi.WriteDir(normal);
    gi.WriteByte(0xd0);  // Green color
    gi.multicast(pos, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- Colored spark particles (color parameter)
- Bright glow
- Energy vaporization effect
- Hissing sound

**Use Cases:**
- Laser weapon impacts
- Energy weapon hits
- Sci-fi effects
- Cutting lasers

### TE_WELDING_SPARKS

**Description:** Welding/cutting torch sparks

**Parameters:**
- `origin` (vec3_t): Welding point
- `dir` (vec3_t): Spark direction

**Server-Side Usage:**
```cpp
void Create_Welding_Effect(const vec3_t &pos, const vec3_t &dir) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_WELDING_SPARKS);
    gi.WritePosition(pos);
    gi.WriteDir(dir);
    gi.multicast(pos, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- Orange/white spark shower
- Continuous stream appearance
- Bright glow at origin
- Welding torch sound

**Use Cases:**
- Industrial machinery
- Repair effects
- Cutting through metal
- Construction scenes

### TE_TUNNEL_SPARKS

**Description:** Dirt/debris spray (tunnel digging effect)

**Parameters:**
- `origin` (vec3_t): Digging point
- `dir` (vec3_t): Spray direction
- `color` (byte): Debris color

**Server-Side Usage:**
```cpp
void Create_Tunnel_Debris(const vec3_t &pos, const vec3_t &dir, byte color) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_TUNNEL_SPARKS);
    gi.WritePosition(pos);
    gi.WriteDir(dir);
    gi.WriteByte(color);
    gi.multicast(pos, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- Brown/gray debris particles
- Rock chunks
- Dust cloud
- Drilling/digging sound

**Use Cases:**
- Tunnel boring
- Ground destruction
- Digging effects
- Excavation

### TE_HEATBEAM_SPARKS

**Description:** Heat beam weapon sparks

**Parameters:**
- `origin` (vec3_t): Beam impact
- `dir` (vec3_t): Surface normal

**Server-Side Usage:**
```cpp
void HeatBeam_Impact(const vec3_t &pos, const vec3_t &normal) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_HEATBEAM_SPARKS);
    gi.WritePosition(pos);
    gi.WriteDir(normal);
    gi.multicast(pos, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- Red/orange heat sparks
- Burning particles
- Thermal distortion
- Heat haze

**Use Cases:**
- Heat beam weapons (from mission packs)
- Thermal weapons
- Fire-based attacks

## Steam and Vapor

### TE_STEAM

**Description:** Steam/vapor puff

**Parameters:**
- `origin` (vec3_t): Steam origin
- `dir` (vec3_t): Steam direction
- `color` (byte): Steam color
- `count` (short): Number of particles
- `magnitude` (byte): Effect intensity

**Server-Side Usage:**
```cpp
void Create_Steam(const vec3_t &pos, const vec3_t &dir, int count) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_STEAM);
    gi.WritePosition(pos);
    gi.WriteDir(dir);
    gi.WriteByte(0xe0);      // White color
    gi.WriteShort(count);    // Particle count
    gi.WriteByte(10);        // Magnitude
    gi.multicast(pos, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- White/gray vapor particles
- Rising motion
- Dissipates over time
- Steam hiss sound

**Use Cases:**
- Pipe bursts
- Geysers
- Damaged machinery
- Hot surfaces
- Environmental ambience

### TE_HEATBEAM_STEAM

**Description:** Steam from heat beam impacts

**Parameters:**
- `origin` (vec3_t): Impact point
- `dir` (vec3_t): Surface normal
- `color` (byte): Steam tint

**Server-Side Usage:**
```cpp
void HeatBeam_Steam(const vec3_t &pos, const vec3_t &normal) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_HEATBEAM_STEAM);
    gi.WritePosition(pos);
    gi.WriteDir(normal);
    gi.WriteByte(0xe0);
    gi.multicast(pos, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- Orange-tinted steam
- Rapid vaporization effect
- Sizzling sound

**Use Cases:**
- Heat beam hitting water
- Thermal weapon impacts
- Flash boiling effects

## Fire and Explosions

### TE_FLAME

**Description:** Flame burst effect

**Parameters:**
- `origin` (vec3_t): Flame origin
- `dir` (vec3_t): Flame direction

**Server-Side Usage:**
```cpp
void Create_Flame(const vec3_t &pos, const vec3_t &dir) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_FLAME);
    gi.WritePosition(pos);
    gi.WriteDir(dir);
    gi.multicast(pos, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- Orange/yellow flame particles
- Upward movement
- Flickering
- Fire crackling sound

**Use Cases:**
- Flamethrower
- Fire traps
- Burning objects
- Ignition effects

### TE_PLAIN_EXPLOSION

**Description:** Generic explosion effect

**Parameters:**
- `origin` (vec3_t): Explosion center

**Server-Side Usage:**
```cpp
void Create_Explosion(const vec3_t &pos) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_PLAIN_EXPLOSION);
    gi.WritePosition(pos);
    gi.multicast(pos, MULTICAST_PHS);  // Use PHS for wider area
}
```

**Client-Side Effect:**
- Expanding fireball
- Smoke cloud
- Shockwave ring
- Explosion sound
- Screen shake (if close)

**Use Cases:**
- Grenade explosions
- Barrel explosions
- Rocket impacts
- Generic explosions

## Special Effects

### TE_TELEPORT_EFFECT

**Description:** Teleportation effect

**Parameters:**
- `origin` (vec3_t): Teleport location

**Server-Side Usage:**
```cpp
void Create_Teleport_Effect(const vec3_t &pos) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_TELEPORT_EFFECT);
    gi.WritePosition(pos);
    gi.multicast(pos, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- Expanding ring particles
- Upward particle spiral
- Blue/white glow
- Teleport sound
- Energy shimmer

**Use Cases:**
- Teleporter destinations
- Monster spawns
- Beam-in effects
- Portal exits

### TE_FLASHLIGHT

**Description:** Flashlight beam visualization

**Parameters:**
- `origin` (vec3_t): Light origin
- `dir` (vec3_t): Beam direction

**Server-Side Usage:**
```cpp
void Create_Flashlight_Beam(const vec3_t &pos, const vec3_t &dir) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_FLASHLIGHT);
    gi.WritePosition(pos);
    gi.WriteDir(dir);
    gi.multicast(pos, MULTICAST_PVS);
}
```

**Client-Side Effect:**
- Volumetric light beam
- Dust particles in beam
- Light cone

**Use Cases:**
- Flashlight effects
- Search lights
- Spotlights
- Light beams

### TE_DEBUG_TRAIL

**Description:** Debug visualization trail

**Parameters:**
- `start` (vec3_t): Trail start
- `end` (vec3_t): Trail end

**Server-Side Usage:**
```cpp
void Draw_Debug_Trail(const vec3_t &start, const vec3_t &end) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_DEBUG_TRAIL);
    gi.WritePosition(start);
    gi.WritePosition(end);
    gi.multicast(start, MULTICAST_ALL);
}
```

**Client-Side Effect:**
- Green particle trail
- Persistent for several seconds
- Visible through walls (developer mode)

**Use Cases:**
- AI pathfinding visualization
- Projectile trajectory debugging
- Movement path tracking
- Developer tools

## Best Practices

### Choosing the Right Event Type

```cpp
// GOOD: Specific event for the situation
if (surface_type == SURF_METAL) {
    SpawnTempEntity(TE_BULLET_SPARKS, impact_point, normal);
} else {
    SpawnTempEntity(TE_GUNSHOT, impact_point, normal);
}

// BAD: Always using generic effect
SpawnTempEntity(TE_SPARKS, impact_point, normal);
```

### Performance Considerations

```cpp
// GOOD: Only send to clients that can see it
gi.multicast(origin, MULTICAST_PVS);

// BAD: Send to all clients (expensive)
gi.multicast(origin, MULTICAST_ALL);
```

### Multicast Types

- `MULTICAST_PVS`: Send to clients that can potentially see this location (most common)
- `MULTICAST_PHS`: Send to clients that can potentially hear this location (for sounds/explosions)
- `MULTICAST_ALL`: Send to all clients (rarely needed)
- `MULTICAST_ALL_R`: Send to all clients reliably (even more rare)

## Related Documentation

- [**Temp Entity Overview**](Temp-Entity-Overview.md) - System overview and when to use temp entities
- [**Using Temp Entities**](Using-Temp-Entities.md) - How to spawn temp entities from code
- [**Custom Temp Entities**](Custom-Temp-Entities.md) - Adding new temp entity types
- [**Client Game Module**](Client-Game-Module.md) - Client-side temp entity processing
- [**API - Entity Events**](API-Entity-Events.md) - Entity events vs. temp entities

## Summary

Q2RTXPerimental provides **20 temp entity event types** for visual effects:

- **Impact Effects**: Gunshots, bullet sparks, blood
- **Water/Liquid**: Splashes, bubbles, various liquid types
- **Sparks**: Generic, electric, laser, welding, tunnel
- **Vapor**: Steam effects, heat beam steam
- **Fire/Explosions**: Flames, plain explosions
- **Special**: Teleports, flashlights, debug trails

Each event type has specific parameters and produces distinct client-side visual/audio effects. Choose the appropriate event type for your use case and use proper multicast types for network efficiency.
