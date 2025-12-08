# Creating Custom Temporary Entities

This guide explains how to add new temporary entity types to Q2RTXPerimental. Custom temp entities allow you to create unique visual effects beyond the built-in types.

## Overview

Adding a custom temp entity involves three main steps:

1. **Define the event type** in `sg_tempentity_events.h`
2. **Add server-side spawn code** (write network message)
3. **Add client-side processing** in `clg_temp_entities.cpp` (render effect)

## Step 1: Define the Event Type

### Location

`src/baseq2rtxp/sharedgame/sg_tempentity_events.h`

### Adding Your Event

```cpp
typedef enum temp_entity_event_e {
    TE_GUNSHOT,
    TE_BLOOD,
    // ... existing events ...
    TE_DEBUGTRAIL,
    
    // Add your custom events here
    TE_CUSTOM_ENERGY_BURST,      // Your new effect!
    TE_CUSTOM_PORTAL_SWIRL,      // Another custom effect
    
    TE_NUM_ENTITY_EVENTS         // Keep this last!
} temp_entity_event_t;
```

**Important Rules:**
- Add new events **before** `TE_NUM_ENTITY_EVENTS`
- Use descriptive names prefixed with `TE_`
- Add comments describing what the effect does

### Example: Energy Burst Effect

```cpp
typedef enum temp_entity_event_e {
    // ... existing events ...
    TE_DEBUGTRAIL,
    
    // Custom: Energy weapon impact burst
    // Parameters: origin (vec3), direction (vec3), color (byte)
    TE_CUSTOM_ENERGY_BURST,
    
    TE_NUM_ENTITY_EVENTS
} temp_entity_event_t;
```

## Step 2: Server-Side Spawning

### Create a Spawn Function

**Location:** Wherever you need to spawn the effect (e.g., weapon code, entity code)

```cpp
// In your weapon or entity file
void Spawn_EnergyBurst(const vec3_t &origin, const vec3_t &direction, 
                       byte color) {
    // 1. Write message type
    gi.WriteByte(svc_temp_entity);
    
    // 2. Write your custom event type
    gi.WriteByte(TE_CUSTOM_ENERGY_BURST);
    
    // 3. Write effect parameters
    gi.WritePosition(origin);
    gi.WriteDir(direction);
    gi.WriteByte(color);  // Custom color parameter
    
    // 4. Send to clients
    gi.multicast(origin, MULTICAST_PVS);
}
```

### Usage in Game Code

```cpp
// In weapon fire code
void svg_weapon_plasma_t::Fire() {
    // ... weapon firing logic ...
    
    // Spawn custom energy burst at impact point
    Spawn_EnergyBurst(impact_point, surface_normal, 0xd0);  // Green
}
```

### Parameter Guidelines

Choose parameters based on your effect's needs:

**Position Parameters:**
```cpp
gi.WritePosition(origin);        // Effect location
gi.WritePosition(start);         // Start point (for trails/beams)
gi.WritePosition(end);           // End point (for trails/beams)
```

**Direction Parameters:**
```cpp
gi.WriteDir(direction);          // Normalized direction vector
gi.WriteDir(surface_normal);     // Surface normal for impacts
```

**Additional Parameters:**
```cpp
gi.WriteByte(color);             // Color index (0-255)
gi.WriteByte(count);             // Particle count
gi.WriteByte(magnitude);         // Effect intensity
gi.WriteShort(duration);         // Effect duration in ms
```

## Step 3: Client-Side Processing

### Location

`src/baseq2rtxp/clgame/clg_temp_entities.cpp`

### Add to the Parser Switch

```cpp
void CLG_TemporaryEntities_Parse(void) {
    clg_explosion_t *ex;
    int r;

    switch (level.parsedMessage.events.tempEntity.type) {
        case TE_BLOOD:
            CLG_FX_BloodParticleEffect(
                level.parsedMessage.events.tempEntity.pos1,
                level.parsedMessage.events.tempEntity.dir,
                0xe8, 1000);
            break;
        
        // ... existing cases ...
        
        // Add your custom case
        case TE_CUSTOM_ENERGY_BURST:
            CLG_Handle_EnergyBurst();  // Your handler function
            break;
            
        default:
            Com_Error(ERR_DROP, "CLG_TemporaryEntities_Parse: bad type");
    }
}
```

### Implement the Handler Function

```cpp
/**
 * @brief   Handles the custom energy burst temp entity effect
 **/
static void CLG_Handle_EnergyBurst() {
    // Get parameters from parsed message
    const vec3_t &origin = level.parsedMessage.events.tempEntity.pos1;
    const vec3_t &direction = level.parsedMessage.events.tempEntity.dir;
    const byte color = level.parsedMessage.events.tempEntity.color;
    
    // Create visual effect using particle system
    CLG_FX_EnergyBurstParticles(origin, direction, color);
    
    // Play sound effect
    clgi.S_StartSound(origin, 0, 0, 
                      precache.sfx.weapons.plasma_hit, 
                      1, ATTN_NORM, 0);
    
    // Optional: Create dynamic light
    CLG_AddDynamicLight(origin, 150, 0.2, 1.0, 0.2);  // Green light
}
```

## Creating Particle Effects

### Using Built-in Particle Functions

Q2RTXPerimental provides several particle effect functions:

```cpp
// Generic particle effect
void CLG_FX_ParticleEffect(const vec3_t &origin, const vec3_t &dir, 
                           int color, int count);

// Particle effect with custom behavior
void CLG_FX_ParticleEffect2(const vec3_t &origin, const vec3_t &dir, 
                            int color, int count);

// Blood particles (with gravity and collision)
void CLG_FX_BloodParticleEffect(const vec3_t &origin, const vec3_t &dir, 
                                int color, int count);

// Water splash
void CLG_FX_ParticleEffectWaterSplash(const vec3_t &origin, 
                                      const vec3_t &dir, 
                                      int color, int count);

// Bubble trail
void CLG_FX_BubbleTrail(const vec3_t &start, const vec3_t &end);
```

### Creating Custom Particle Effects

**Location:** `src/baseq2rtxp/clgame/clg_effects.cpp`

```cpp
/**
 * @brief   Custom energy burst particle effect
 **/
void CLG_FX_EnergyBurstParticles(const vec3_t &origin, 
                                  const vec3_t &direction,
                                  byte color) {
    // Spawn particle burst
    for (int i = 0; i < 30; i++) {
        cparticle_t *p = CLG_AllocParticle();
        if (!p)
            return;
        
        // Set particle type
        p->type = PARTICLE_SPARK;  // Or custom type
        
        // Set color
        p->color = color;
        p->alpha = 1.0f;
        
        // Position at origin with random offset
        p->origin[0] = origin[0] + crandom() * 5;
        p->origin[1] = origin[1] + crandom() * 5;
        p->origin[2] = origin[2] + crandom() * 5;
        
        // Velocity in burst pattern
        float speed = 100.0f + frandom() * 50.0f;
        p->velocity[0] = direction[0] * speed + crandom() * 30;
        p->velocity[1] = direction[1] * speed + crandom() * 30;
        p->velocity[2] = direction[2] * speed + crandom() * 30;
        
        // Acceleration (gravity, air resistance)
        p->accel[0] = 0;
        p->accel[1] = 0;
        p->accel[2] = -100;  // Gravity
        
        // Lifetime
        p->lifetime = 0.5f + frandom() * 0.3f;
        
        // Size
        p->size = 2.0f + frandom() * 1.0f;
        
        // Alpha fade
        p->alphavel = -1.0f / p->lifetime;
    }
}
```

## Complete Example: Portal Effect

### Step 1: Define Event

```cpp
// In sg_tempentity_events.h
typedef enum temp_entity_event_e {
    // ... existing events ...
    
    // Custom portal swirl effect
    // Parameters: origin (vec3), radius (byte), duration (byte)
    TE_CUSTOM_PORTAL_SWIRL,
    
    TE_NUM_ENTITY_EVENTS
} temp_entity_event_t;
```

### Step 2: Server Spawn Function

```cpp
// In your portal entity code
void svg_misc_portal_t::Spawn_PortalEffect() {
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_CUSTOM_PORTAL_SWIRL);
    gi.WritePosition(s.origin);
    gi.WriteByte(portal_radius);      // Custom: portal size
    gi.WriteByte(portal_duration);    // Custom: effect duration
    gi.multicast(s.origin, MULTICAST_PVS);
}

void svg_misc_portal_t::Think() {
    // Periodically spawn portal effect
    if (level.time > next_effect_time) {
        Spawn_PortalEffect();
        next_effect_time = level.time + 1.0f;  // Every second
    }
    
    nextthink = level.time + FRAMETIME;
}
```

### Step 3: Client Handler

```cpp
// In clg_temp_entities.cpp

// Add to switch statement
case TE_CUSTOM_PORTAL_SWIRL:
    CLG_Handle_PortalSwirl();
    break;

// Implement handler
static void CLG_Handle_PortalSwirl() {
    const vec3_t &origin = level.parsedMessage.events.tempEntity.pos1;
    const byte radius = level.parsedMessage.events.tempEntity.color;
    const byte duration = level.parsedMessage.events.tempEntity.count;
    
    // Create swirling particle effect
    CLG_FX_PortalSwirlParticles(origin, radius, duration);
    
    // Portal sound (ambient hum)
    clgi.S_StartSound(origin, 0, 0, 
                      precache.sfx.misc.portal_hum, 
                      0.5f, ATTN_IDLE, 0);
    
    // Glowing light
    CLG_AddDynamicLight(origin, radius * 2, 
                        0.5, 0.2, 1.0);  // Purple light
}
```

### Step 4: Portal Particle Effect

```cpp
// In clg_effects.cpp
void CLG_FX_PortalSwirlParticles(const vec3_t &origin, 
                                  byte radius, byte duration) {
    float r = (float)radius;
    
    // Create spiral of particles
    for (int i = 0; i < 40; i++) {
        cparticle_t *p = CLG_AllocParticle();
        if (!p)
            return;
        
        p->type = PARTICLE_GLOW;
        p->color = 0xd0 + (i % 16);  // Purple/blue gradient
        p->alpha = 0.7f;
        
        // Spiral pattern
        float angle = (float)i / 40.0f * M_PI * 4.0f;
        float height = (float)i / 40.0f * r * 2.0f;
        
        p->origin[0] = origin[0] + cos(angle) * r;
        p->origin[1] = origin[1] + sin(angle) * r;
        p->origin[2] = origin[2] - r + height;
        
        // Swirl velocity
        p->velocity[0] = -sin(angle) * 50.0f;
        p->velocity[1] = cos(angle) * 50.0f;
        p->velocity[2] = 20.0f;
        
        p->accel[0] = 0;
        p->accel[1] = 0;
        p->accel[2] = 0;
        
        p->lifetime = (float)duration / 10.0f;
        p->size = 3.0f;
        p->alphavel = -p->alpha / p->lifetime;
    }
}
```

## Advanced: Beam Effects

For continuous beam effects (lightning, lasers), use beam entities:

```cpp
void CLG_Handle_CustomLaserBeam() {
    const vec3_t &start = level.parsedMessage.events.tempEntity.pos1;
    const vec3_t &end = level.parsedMessage.events.tempEntity.pos2;
    const byte color = level.parsedMessage.events.tempEntity.color;
    
    // Create beam entity
    centity_t *beam = CLG_AllocBeam();
    if (!beam)
        return;
    
    beam->type = BEAM_LASER;
    VectorCopy(start, beam->start);
    VectorCopy(end, beam->end);
    beam->color = color;
    beam->width = 4.0f;
    beam->alpha = 1.0f;
    beam->lifetime = 0.1f;  // Brief flash
    
    // Beam sound
    clgi.S_StartSound(start, 0, 0, 
                      precache.sfx.weapons.laser_fire,
                      1, ATTN_NORM, 0);
}
```

## Testing Your Custom Temp Entity

### Console Test Command

Create a debug command to test your effect:

```cpp
// In your server game code
void Cmd_TestPortal_f(edict_t *ent) {
    trace_t tr;
    vec3_t forward, start, end;
    
    // Trace forward from player
    AngleVectors(ent->client->v_angle, forward, NULL, NULL);
    VectorCopy(ent->s.origin, start);
    start[2] += ent->viewheight;
    VectorMA(start, 2048, forward, end);
    
    tr = gi.trace(start, NULL, NULL, end, ent, MASK_SOLID);
    
    // Spawn portal effect at hit point
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_CUSTOM_PORTAL_SWIRL);
    gi.WritePosition(tr.endpos);
    gi.WriteByte(50);   // radius
    gi.WriteByte(10);   // duration
    gi.multicast(tr.endpos, MULTICAST_PVS);
    
    gi.cprintf(ent, PRINT_HIGH, "Portal effect spawned\n");
}
```

Register the command:

```cpp
void SVG_InitGame() {
    // ... other init code ...
    gi.AddCommand("testportal", Cmd_TestPortal_f);
}
```

Test in console:

```
testportal
```

## Best Practices

### ✅ Do's

**Clear Naming:**
```cpp
TE_CUSTOM_ENERGY_BURST     // Good: Descriptive
TE_CUSTOM_PORTAL_SWIRL     // Good: Clear purpose
```

**Document Parameters:**
```cpp
// Custom energy burst effect
// Parameters:
//   - origin (vec3): Burst center
//   - direction (vec3): Burst direction
//   - color (byte): Particle color (0-255)
TE_CUSTOM_ENERGY_BURST,
```

**Efficient Particle Counts:**
```cpp
// Good: Reasonable particle count
for (int i = 0; i < 30; i++) {
    // Spawn particle
}

// Bad: Too many particles
for (int i = 0; i < 1000; i++) {
    // Will lag!
}
```

### ❌ Don'ts

**Don't Break Existing Events:**
```cpp
// WRONG: Inserting in the middle
typedef enum temp_entity_event_e {
    TE_GUNSHOT,
    TE_CUSTOM_NEW,        // Breaks network compatibility!
    TE_BLOOD,
    // ...
```

**Don't Use Too Many Parameters:**
```cpp
// WRONG: Too many parameters = complex and slow
gi.WriteByte(svc_temp_entity);
gi.WriteByte(TE_CUSTOM_COMPLEX);
gi.WritePosition(pos1);
gi.WritePosition(pos2);
gi.WritePosition(pos3);
gi.WriteByte(param1);
gi.WriteByte(param2);
gi.WriteByte(param3);
gi.WriteShort(param4);
// ... 10 more parameters ...
```

**Don't Forget Error Handling:**
```cpp
// WRONG: No null check
cparticle_t *p = CLG_AllocParticle();
p->type = PARTICLE_SPARK;  // Crash if p is NULL!

// CORRECT: Check for null
cparticle_t *p = CLG_AllocParticle();
if (!p)
    return;
p->type = PARTICLE_SPARK;
```

## Debugging

### Enable Developer Mode

```
developer 1
```

### Add Debug Prints

```cpp
void CLG_Handle_EnergyBurst() {
    Com_Printf("Energy burst at (%f, %f, %f)\n",
               level.parsedMessage.events.tempEntity.pos1[0],
               level.parsedMessage.events.tempEntity.pos1[1],
               level.parsedMessage.events.tempEntity.pos1[2]);
    
    // ... effect code ...
}
```

### Use TE_DEBUGTRAIL for Visualization

```cpp
// Visualize effect bounds
gi.WriteByte(svc_temp_entity);
gi.WriteByte(TE_DEBUGTRAIL);
gi.WritePosition(origin);
gi.WritePosition(target_pos);
gi.multicast(origin, MULTICAST_ALL);
```

## Related Documentation

- [**Temp Entity Overview**](Temp-Entity-Overview.md) - System overview
- [**Temp Entity Event Types**](Temp-Entity-Event-Types.md) - Built-in event reference
- [**Using Temp Entities**](Using-Temp-Entities.md) - How to spawn temp entities
- [**Client Game Module**](Client-Game-Module.md) - Client-side rendering
- [**Server Game Module**](Server-Game-Module.md) - Server-side game logic

## Summary

To create a custom temp entity:

1. **Define** the event type in `sg_tempentity_events.h`
2. **Add server spawn code** to write the network message
3. **Add client handler** in `clg_temp_entities.cpp` to render the effect
4. **Create particle effects** using the particle system
5. **Test** using console commands

**Key Points:**
- Add new events before `TE_NUM_ENTITY_EVENTS`
- Keep parameter counts reasonable (3-5 is ideal)
- Check for null when allocating particles/beams
- Use appropriate multicast types (MULTICAST_PVS for most)
- Test thoroughly with different network conditions

Custom temp entities let you create unique visual effects that match your game's aesthetic while remaining network-efficient!
