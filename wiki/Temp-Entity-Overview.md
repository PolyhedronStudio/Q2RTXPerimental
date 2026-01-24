# Temporary Entity System

Temporary entities (temp entities) are short-lived visual effects that don't require persistent entity objects. They're essential for efficient visual feedback like bullet impacts, sparks, explosions, and blood splatter.

## What Are Temporary Entities?

**Temporary Entities** are transient visual effects spawned at a specific location for a brief moment. Unlike regular entities:

- **No entity_state_t**: Not part of the entity snapshot system
- **One-Time Events**: Sent once, not tracked or updated
- **Client-Side Only**: Server tells clients "spawn effect at X", clients handle rendering
- **Efficient**: No ongoing network traffic or server-side management

### When to Use Temp Entities

✅ **Use Temp Entities For:**
- Bullet impacts and ricochet sparks
- Blood splatter from damage
- Small explosions and particle bursts
- Environmental effects (steam, sparks, bubbles)
- Visual-only effects that last < 1 second

❌ **Don't Use Temp Entities For:**
- Projectiles that need physics (rockets, grenades) - use regular entities
- Effects that persist > 1 second - use local entities
- Anything that needs server-side logic - use regular entities
- Player-attached effects - use entity events

## Temp Entity Event Types

**Location:** `src/baseq2rtxp/sharedgame/sg_tempentity_events.h`

### Available Event Types

```cpp
typedef enum temp_entity_event_e {
    TE_GUNSHOT,             // Bullet impact (default surface)
    
    TE_BLOOD,               // Blood splatter (small)
    TE_MOREBLOOD,           // Blood splatter (large)
    
    TE_BUBBLETRAIL,         // Bubble trail underwater
    TE_BUBBLETRAIL2,        // Alternate bubble trail
    TE_SPLASH,              // Water splash (bullet hitting water)
    
    TE_STEAM,               // Steam puff
    TE_HEATBEAM_STEAM,      // Steam from heat beam
    
    TE_SPARKS,              // Generic sparks
    TE_HEATBEAM_SPARKS,     // Heat beam sparks
    TE_BULLET_SPARKS,       // Bullet ricochet sparks
    TE_ELECTRIC_SPARKS,     // Electrical sparks
    TE_LASER_SPARKS,        // Laser impact sparks
    TE_TUNNEL_SPARKS,       // Tunnel digging sparks
    TE_WELDING_SPARKS,      // Welding effect
    
    TE_FLAME,               // Flame burst
    
    TE_PLAIN_EXPLOSION,     // Generic explosion
    
    TE_TELEPORT_EFFECT,     // Teleporter effect
    
    TE_FLASHLIGHT,          // Flashlight beam
    TE_DEBUG_TRAIL,          // Debug visualization trail
    
    TE_NUM_ENTITY_EVENTS    // Total count
} temp_entity_event_t;
```

### Splash Types

For `TE_SPLASH` events:

```cpp
static constexpr int32_t SPLASH_UNKNOWN     = 0;
static constexpr int32_t SPLASH_SPARKS      = 1;  // Metal surface
static constexpr int32_t SPLASH_BLUE_WATER  = 2;  // Clean water
static constexpr int32_t SPLASH_BROWN_WATER = 3;  // Dirty water
static constexpr int32_t SPLASH_SLIME       = 4;  // Toxic slime
static constexpr int32_t SPLASH_LAVA        = 5;  // Molten lava
static constexpr int32_t SPLASH_BLOOD       = 6;  // Blood pool
```

## Spawning Temp Entities (Server-Side)

### Basic Pattern

```cpp
// 1. Write message type
gi.WriteByte(svc_temp_entity);

// 2. Write temp entity event type
gi.WriteByte(TE_GUNSHOT);

// 3. Write event-specific data
gi.WritePosition(origin);       // Where effect spawns
gi.WriteDir(direction);         // Optional: effect direction

// 4. Send to clients
gi.multicast(origin, MULTICAST_PVS);
```

### Multicast Modes

```cpp
MULTICAST_ALL       // Send to all clients (expensive!)
MULTICAST_PHS       // Send to clients in Potentially Hearable Set
MULTICAST_PVS       // Send to clients in Potentially Visible Set (most common)
MULTICAST_ALL_R     // Reliable to all clients
MULTICAST_PHS_R     // Reliable to PHS
MULTICAST_PVS_R     // Reliable to PVS
```

**Tip:** Use `MULTICAST_PVS` for visual effects - only clients who can see the location receive it.

## Common Temp Entity Examples

### Bullet Impact

```cpp
void SVG_BulletImpact(vec3_t origin, vec3_t normal, int surfaceFlags) {
    // Don't show impact on sky
    if (surfaceFlags & CM_SURFACE_FLAG_SKY)
        return;
        
    // Spawn bullet sparks
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_BULLET_SPARKS);
    gi.WritePosition(origin);
    gi.WriteDir(normal);
    gi.multicast(origin, MULTICAST_PVS);
    
    // Also spawn decal (if on solid surface)
    if (!(surfaceFlags & CM_SURFACE_FLAG_WARP)) {
        SpawnBulletDecal(origin, normal);
    }
}
```

### Blood Splatter

```cpp
void SVG_BloodEffect(vec3_t origin, vec3_t dir, int damage) {
    temp_entity_event_t type;
    
    // More blood for higher damage
    if (damage < 10) {
        type = TE_BLOOD;
    } else {
        type = TE_MOREBLOOD;
    }
    
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(type);
    gi.WritePosition(origin);
    gi.WriteDir(dir);
    gi.multicast(origin, MULTICAST_PVS);
}
```

### Water Splash

```cpp
void SVG_WaterSplash(vec3_t origin, vec3_t dir, int count, int splashType) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_SPLASH);
    gi.WriteByte(count);            // Number of particles
    gi.WritePosition(origin);
    gi.WriteDir(dir);
    gi.WriteByte(splashType);       // SPLASH_BLUE_WATER, etc.
    gi.multicast(origin, MULTICAST_PVS);
}

// Usage
SVG_WaterSplash(waterSurface, vec3_up, 8, SPLASH_BLUE_WATER);
```

### Explosion

```cpp
void SVG_Explosion(vec3_t origin) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_PLAIN_EXPLOSION);
    gi.WritePosition(origin);
    gi.multicast(origin, MULTICAST_PHS);  // Use PHS for sound
    
    // Also spawn explosion sprite entity
    auto *explosion = SVG_Spawn<svg_misc_explosion_t>();
    explosion->s.origin = origin;
    explosion->Spawn();
}
```

### Sparks

```cpp
void SVG_SparkShower(vec3_t origin, vec3_t dir, int count) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_SPARKS);
    gi.WritePosition(origin);
    gi.WriteDir(dir);
    gi.WriteByte(count);  // Number of sparks
    gi.multicast(origin, MULTICAST_PVS);
}

// Electric sparks (different visual)
void SVG_ElectricSparks(vec3_t origin) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_ELECTRIC_SPARKS);
    gi.WritePosition(origin);
    gi.multicast(origin, MULTICAST_PVS);
}
```

### Steam

```cpp
void SVG_Steam(vec3_t origin, vec3_t dir, int count, int speed, 
               int noise, int basecount) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_STEAM);
    gi.WritePosition(origin);
    gi.WriteDir(dir);
    gi.WriteByte(count);       // Particle count
    gi.WriteByte(speed);       // Movement speed
    gi.WriteByte(noise);       // Random spread
    gi.WriteByte(basecount);   // Additional count modifier
    gi.multicast(origin, MULTICAST_PVS);
}

// Example: Steam vent
SVG_Steam(ventOrigin, vec3_up, 10, 50, 8, 0);
```

### Teleport Effect

```cpp
void SVG_TeleportEffect(vec3_t origin) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_TELEPORT_EFFECT);
    gi.WritePosition(origin);
    gi.multicast(origin, MULTICAST_PVS);
    
    // Also play teleport sound
    gi.sound(nullptr, CHAN_AUTO, gi.soundindex("world/teleport.wav"),
             1.0f, ATTN_NORM, 0);
}
```

## Client-Side Processing

**Location:** `src/baseq2rtxp/clgame/clg_temp_entities.cpp`

### Temp Entity Handler

```cpp
void CLG_ParseTempEntity(void) {
    int type = MSG_ReadByte(&net_message);
    
    switch (type) {
        case TE_GUNSHOT:
            CLG_GunShotEffect();
            break;
            
        case TE_BLOOD:
            CLG_BloodEffect(false);  // Small blood
            break;
            
        case TE_MOREBLOOD:
            CLG_BloodEffect(true);   // Large blood
            break;
            
        case TE_BULLET_SPARKS:
            CLG_BulletSparkEffect();
            break;
            
        case TE_SPLASH:
            CLG_SplashEffect();
            break;
            
        // ... handle all temp entity types ...
    }
}
```

### Example: Bullet Sparks

```cpp
void CLG_BulletSparkEffect(void) {
    // Read data from message
    vec3_t origin = MSG_ReadPos(&net_message);
    vec3_t dir = MSG_ReadDir(&net_message);
    
    // Spawn particle effect
    for (int i = 0; i < 6; i++) {
        clg_particle_t *p = CLG_AllocParticle();
        if (!p)
            return;
            
        p->type = PARTICLE_SPARK;
        p->color = 0xe0 + (rand() & 7);  // Yellow-orange
        p->lifetime = 0.6f + frand() * 0.2f;
        
        // Randomize velocity
        p->vel[0] = dir[0] * 40 + crand() * 10;
        p->vel[1] = dir[1] * 40 + crand() * 10;
        p->vel[2] = dir[2] * 40 + crand() * 10;
        
        p->org = origin;
        p->gravity = PARTICLE_GRAVITY;
    }
    
    // Play ricochet sound
    S_StartSound(origin, -1, CHAN_AUTO, S_RegisterSound("weapons/ric1.wav"),
                 0.5f, ATTN_STATIC, 0);
}
```

### Example: Blood Effect

```cpp
void CLG_BloodEffect(bool large) {
    vec3_t origin = MSG_ReadPos(&net_message);
    vec3_t dir = MSG_ReadDir(&net_message);
    
    int count = large ? 20 : 10;
    
    for (int i = 0; i < count; i++) {
        clg_particle_t *p = CLG_AllocParticle();
        if (!p)
            return;
            
        p->type = PARTICLE_BLOOD;
        p->color = 0xe8;  // Red
        p->lifetime = 0.8f + frand() * 0.4f;
        p->scale = large ? 6.0f : 4.0f;
        
        // Spray in direction of hit
        p->vel[0] = dir[0] * 60 + crand() * 40;
        p->vel[1] = dir[1] * 60 + crand() * 40;
        p->vel[2] = dir[2] * 60 + crand() * 40;
        
        p->org = origin + vec3_t{crand(), crand(), crand()} * 2;
        p->gravity = PARTICLE_GRAVITY * 2;  // Falls faster
    }
    
    // Spawn blood decal
    CLG_SpawnDecal(origin, dir, DECAL_BLOOD);
}
```

## Practical Usage Patterns

### In Weapon Fire

```cpp
void svg_weapon_shotgun_t::Fire() {
    vec3_t start = owner->s.origin + viewHeight;
    vec3_t dir;
    AngleVectors(owner->client->v_angle, dir, nullptr, nullptr);
    
    // Fire multiple pellets
    for (int i = 0; i < 12; i++) {
        vec3_t spread = ApplySpread(dir, 500, 500);
        
        cm_trace_t tr = SVG_Trace(start, vec3_origin, vec3_origin,
                                   start + spread * 2048,
                                   owner, MASK_SHOT);
        
        if (tr.fraction < 1.0f) {
            // Hit something
            svg_base_edict_t *hit = tr.ent;
            
            // Damage
            if (hit->takedamage) {
                SVG_Damage(hit, owner, owner, dir, tr.endpos,
                           tr.plane.normal, damage, kick, 
                           DAMAGE_BULLET, MOD_SHOTGUN);
            }
            
            // Visual effect
            if (hit->takedamage) {
                // Blood on flesh
                gi.WriteByte(svc_temp_entity);
                gi.WriteByte(TE_BLOOD);
                gi.WritePosition(tr.endpos);
                gi.WriteDir(tr.plane.normal);
                gi.multicast(tr.endpos, MULTICAST_PVS);
            } else {
                // Sparks on hard surface
                gi.WriteByte(svc_temp_entity);
                gi.WriteByte(TE_BULLET_SPARKS);
                gi.WritePosition(tr.endpos);
                gi.WriteDir(tr.plane.normal);
                gi.multicast(tr.endpos, MULTICAST_PVS);
            }
        }
    }
}
```

### In Damage Calculation

```cpp
void SVG_Damage(svg_base_edict_t *targ, /*...*/) {
    // Apply damage...
    targ->health -= damage;
    
    // Blood effect if flesh
    if (targ->takedamage && damage > 0) {
        temp_entity_event_t bloodType = (damage > 20) ? TE_MOREBLOOD : TE_BLOOD;
        
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(bloodType);
        gi.WritePosition(point);
        gi.WriteDir(normal);
        gi.multicast(point, MULTICAST_PVS);
    }
    
    // Check for death...
}
```

### In Environmental Effects

```cpp
class svg_misc_steam_t : public svg_base_edict_t {
    void Think() override {
        // Emit steam puff
        vec3_t dir = {0, 0, 1};  // Upward
        
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_STEAM);
        gi.WritePosition(s.origin);
        gi.WriteDir(dir);
        gi.WriteByte(8);   // count
        gi.WriteByte(50);  // speed
        gi.WriteByte(8);   // noise
        gi.WriteByte(0);   // basecount
        gi.multicast(s.origin, MULTICAST_PVS);
        
        // Think again in 0.5 seconds
        nextThinkTime = level.time + gametime_t::from_sec(0.5f);
    }
};
```

## Creating Custom Temp Entities

To add a new temp entity type:

### 1. Add Event Type

In `src/baseq2rtxp/sharedgame/sg_tempentity_events.h`:

```cpp
typedef enum temp_entity_event_e {
    // ... existing types ...
    
    TE_CUSTOM_EFFECT,      // Your new effect
    
    TE_NUM_ENTITY_EVENTS
} temp_entity_event_t;
```

### 2. Server-Side Spawning

```cpp
void SVG_CustomEffect(vec3_t origin, vec3_t color, int intensity) {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_CUSTOM_EFFECT);
    gi.WritePosition(origin);
    gi.WriteByte(color[0] * 255);
    gi.WriteByte(color[1] * 255);
    gi.WriteByte(color[2] * 255);
    gi.WriteByte(intensity);
    gi.multicast(origin, MULTICAST_PVS);
}
```

### 3. Client-Side Handler

In `src/baseq2rtxp/clgame/clg_temp_entities.cpp`:

```cpp
void CLG_ParseTempEntity(void) {
    int type = MSG_ReadByte(&net_message);
    
    switch (type) {
        // ... existing cases ...
        
        case TE_CUSTOM_EFFECT:
            CLG_CustomEffect();
            break;
    }
}

void CLG_CustomEffect(void) {
    vec3_t origin = MSG_ReadPos(&net_message);
    vec3_t color = {
        MSG_ReadByte(&net_message) / 255.0f,
        MSG_ReadByte(&net_message) / 255.0f,
        MSG_ReadByte(&net_message) / 255.0f
    };
    int intensity = MSG_ReadByte(&net_message);
    
    // Spawn particles, lights, sounds, etc.
    for (int i = 0; i < intensity; i++) {
        clg_particle_t *p = CLG_AllocParticle();
        // ... configure particle ...
    }
}
```

## Performance Tips

### Bandwidth Consideration

```cpp
// BAD: Spawns temp entity every frame
void Think() {
    SpawnSparks(s.origin);
    nextThinkTime = level.time + FRAMETIME;  // 40 Hz
}

// GOOD: Spawns periodically
void Think() {
    SpawnSparks(s.origin);
    nextThinkTime = level.time + gametime_t::from_sec(0.25f);  // 4 Hz
}
```

### PVS Culling

Always use `MULTICAST_PVS` for visual effects:

```cpp
// Automatically culled to visible clients
gi.multicast(origin, MULTICAST_PVS);
```

### Particle Budget

Don't spawn too many particles:

```cpp
// Limit particle count
int maxParticles = min(damageAmount / 5, 20);
for (int i = 0; i < maxParticles; i++) {
    SpawnBloodParticle();
}
```

## Debugging

### Console Variables

```
cl_show_temp_entities 1     // Show temp entity spawns
cl_temp_entity_debug 1      // Print temp entity info
r_drawparticles 1           // Show particles (default)
```

### Debug Visualization

```cpp
#ifdef _DEBUG
void SVG_DebugTempEntity(const char *name, vec3_t origin) {
    gi.dprintf("TE: %s at (%.1f, %.1f, %.1f)\n", 
               name, origin[0], origin[1], origin[2]);
}
#endif

// Usage
SVG_DebugTempEntity("BLOOD", hitPos);
gi.WriteByte(svc_temp_entity);
gi.WriteByte(TE_BLOOD);
// ...
```

## Next Steps

- [Entity Events](API-Entity-Events.md) - Learn about persistent entity events
- [Client Game Module](Client-Game-Module.md) - Understand client-side effects
- [Creating Custom Entities](Creating-Custom-Entities.md) - When to use regular entities

---

**Previous:** [Entity System Overview](Entity-System-Overview.md)  
**See Also:** [API Reference - Temp Entity Events](API-Temp-Entity-Events.md)
