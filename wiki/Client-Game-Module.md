# Client Game Module (clgame)

The Client Game Module (`clgame`) handles all client-side game logic, including prediction, interpolation, effects, and HUD rendering.

**Location:** `src/baseq2rtxp/clgame/`  
**Output:** `cgame.dll` (Windows) / `cgame.so` (Linux)

## Overview

The clgame module is responsible for:
- **Client-Side Prediction**: Predicting player movement locally
- **Entity Interpolation**: Smoothing entity movement between server snapshots
- **Temporary Entities**: Spawning short-lived visual effects
- **Visual Effects**: Particles, beams, explosions, trails
- **HUD Rendering**: Health, ammo, scores, messages
- **View Calculation**: Camera position, weapon positioning, view bobbing
- **Event Processing**: Handling entity events (footsteps, weapon sounds)
- **Local Entities**: Client-only entities (ejected shells, smoke)

**Key Point:** The client never makes authoritative gameplay decisions - it only predicts and presents. The server is always the authority.

## Module Structure

```
clgame/
├── effects/                    # Visual effect implementations
│   ├── clg_effect_steam.cpp
│   ├── clg_effect_bubbles.cpp
│   └── ...
├── hud/                        # HUD element implementations
│   ├── clg_hud_health.cpp
│   ├── clg_hud_ammo.cpp
│   ├── clg_hud_crosshair.cpp
│   └── ...
├── local_entities/             # Client-local entity types
│   ├── clg_local_shell.cpp    # Ejected shell casings
│   ├── clg_local_smoke.cpp    # Smoke puffs
│   └── ...
├── packet_entities/            # Server entity processing
│   └── clg_packet_entity_processing.cpp
├── temp_entities/              # Temp entity handlers
│   └── clg_temp_entity_handlers.cpp
├── clg_main.cpp                # Module entry point
├── clg_predict.cpp             # Client-side prediction
├── clg_packet_entities.cpp    # Entity interpolation
├── clg_temp_entities.cpp      # Temp entity spawning
├── clg_effects.cpp             # Effect systems
├── clg_view.cpp                # View calculation
├── clg_view_weapon.cpp         # Weapon view model
├── clg_hud.cpp                 # HUD rendering
├── clg_events.cpp              # Event handling
├── clg_local_entities.cpp      # Local entity management
└── ...
```

## Frame Loop

Every client frame (as fast as possible, capped by fps):

```cpp
void CLG_Frame(int msec) {
    // 1. Read messages from server
    CLG_ReadPackets();
    
    // 2. Process server snapshot
    if (new_snapshot_received) {
        CLG_ParseFrame(&cl.frame);
    }
    
    // 3. Predict player movement
    CLG_PredictMovement();
    
    // 4. Interpolate entities between snapshots
    CLG_InterpolateEntities();
    
    // 5. Update local entities (shells, smoke)
    CLG_UpdateLocalEntities(msec);
    
    // 6. Update particle effects
    CLG_UpdateParticles(msec);
    
    // 7. Calculate view
    CLG_CalcViewValues();
    
    // 8. Add entities to render scene
    CLG_AddEntities();
    
    // 9. Render HUD
    CLG_DrawHUD();
}
```

## Client-Side Prediction

### Why Prediction?

Network latency means client input takes time to reach server and results take time to return. Without prediction, controls would feel sluggish.

**With Prediction:**
1. Client sends input to server
2. Client **immediately** predicts result locally
3. Server processes input and sends result
4. Client compares prediction to result and corrects if needed

### Prediction Implementation

```cpp
void CLG_PredictMovement(void) {
    // Start from last acknowledged server position
    pmove_t pm = {};
    pm.s = cl.frame.playerstate.pmove;
    
    // Replay all unacknowledged commands
    for (int i = cl.netchan.outgoing_acknowledged + 1;
         i <= cl.netchan.outgoing_sequence; i++) {
        
        usercmd_t *cmd = &cl.cmds[i & CMD_MASK];
        
        pm.cmd = *cmd;
        pm.trace = CL_PMTrace;  // Collision function
        pm.pointcontents = CL_PMPointContents;
        
        // Run same movement code as server
        Pmove(&pm);  // Shared code from sharedgame/pmove/
    }
    
    // Use predicted position for rendering
    cl.predicted_origin = pm.s.origin;
    cl.predicted_angles = pm.s.viewangles;
    cl.predicted_velocity = pm.s.velocity;
    
    // Check for prediction errors
    if (VectorDistance(pm.s.origin, cl.frame.playerstate.pmove.origin) > 1.0f) {
        // Server corrected our prediction
        cl.prediction_error = cl.frame.playerstate.pmove.origin - pm.s.origin;
    }
}
```

**Key Points:**
- Uses **same movement code** as server (`Pmove` from sharedgame)
- Replays commands from last acknowledged to current
- Compares result to server's position
- Smoothly corrects errors over time

### Prediction Error Smoothing

```cpp
void CLG_SmoothPredictionError(void) {
    // Don't snap instantly to corrected position - smooth over time
    if (VectorLength(cl.prediction_error) > 0.1f) {
        // Reduce error by 1/8 each frame (exponential decay)
        VectorScale(cl.prediction_error, 0.875f, cl.prediction_error);
        
        // Add error to predicted origin for rendering
        cl.render_origin = cl.predicted_origin + cl.prediction_error;
    }
}
```

## Entity Interpolation

Servers send snapshots at ~40 Hz, but clients render at 60+ FPS. Interpolation smooths movement.

### Basic Interpolation

```cpp
void CLG_InterpolateEntity(centity_t *cent) {
    // Get current and previous snapshots
    entity_state_t *current = &cent->current;
    entity_state_t *prev = &cent->prev;
    
    // Calculate lerp fraction
    float lerp = cl.lerpfrac;  // 0.0 to 1.0 based on time
    
    // Interpolate position
    cent->lerp_origin = LerpVector(prev->origin, current->origin, lerp);
    
    // Interpolate angles (handle wrapping)
    cent->lerp_angles = LerpAngles(prev->angles, current->angles, lerp);
    
    // Interpolate animation frame
    if (current->frame != prev->frame) {
        cent->lerp_frame = prev->frame + lerp * (current->frame - prev->frame);
    }
}
```

### Extrapolation (Advanced)

For high latency, extrapolate slightly into the future:

```cpp
if (cl.lerpfrac > 1.0f) {
    // No new snapshot yet - extrapolate
    float extrap = cl.lerpfrac - 1.0f;
    
    // Estimate position based on velocity
    vec3_t velocity = (current->origin - prev->origin) / FRAMETIME.count();
    cent->lerp_origin = current->origin + velocity * extrap;
}
```

## Temporary Entities

Client spawns visual effects from server messages:

```cpp
void CLG_ParseTempEntity(void) {
    int type = MSG_ReadByte(&net_message);
    
    switch (type) {
        case TE_GUNSHOT:
            CLG_GunShotEffect();
            break;
            
        case TE_BLOOD:
            CLG_BloodEffect();
            break;
            
        case TE_EXPLOSION:
            CLG_ExplosionEffect();
            break;
            
        // ... handle all temp entity types ...
    }
}

void CLG_BloodEffect(void) {
    vec3_t origin = MSG_ReadPos(&net_message);
    vec3_t dir = MSG_ReadDir(&net_message);
    
    // Spawn blood particles
    for (int i = 0; i < 10; i++) {
        clg_particle_t *p = CLG_AllocParticle();
        p->type = PARTICLE_BLOOD;
        p->color = 0xe8;  // Red
        p->lifetime = 0.8f;
        p->org = origin + RandomVec() * 2;
        p->vel = dir * 60 + RandomVec() * 40;
        p->gravity = PARTICLE_GRAVITY * 2;
    }
    
    // Play splat sound
    S_StartSound(origin, -1, CHAN_AUTO, 
                 S_RegisterSound("misc/udeath.wav"), 0.5f, ATTN_NORM, 0);
}
```

**See:** [Temporary Entity System](Temp-Entity-Overview.md)

## Visual Effects System

### Particle System

```cpp
typedef struct clg_particle_s {
    particle_type_t type;    // PARTICLE_BLOOD, PARTICLE_SMOKE, etc.
    vec3_t org;              // Current position
    vec3_t vel;              // Velocity
    vec3_t accel;            // Acceleration
    vec3_t color;            // RGB color
    float alpha;             // Transparency
    float scale;             // Size
    float lifetime;          // Remaining time
    float gravity;           // Gravity multiplier
    texture_t *texture;      // Particle texture
} clg_particle_t;

void CLG_UpdateParticles(int msec) {
    float dt = msec * 0.001f;
    
    for (int i = 0; i < cl.num_particles; i++) {
        clg_particle_t *p = &cl.particles[i];
        
        // Update lifetime
        p->lifetime -= dt;
        if (p->lifetime <= 0) {
            CLG_FreeParticle(p);
            continue;
        }
        
        // Apply gravity
        p->accel[2] = -p->gravity * 800.0f;
        
        // Update velocity
        p->vel += p->accel * dt;
        
        // Update position
        p->org += p->vel * dt;
        
        // Fade out
        p->alpha = p->lifetime / p->initial_lifetime;
    }
}
```

### Beam Effects

```cpp
void CLG_AddBeam(vec3_t start, vec3_t end, int model, float width, 
                 vec3_t color, float alpha) {
    beam_t *b = CLG_AllocBeam();
    b->start = start;
    b->end = end;
    b->model = model;
    b->width = width;
    b->color = color;
    b->alpha = alpha;
    b->lifetime = 0.1f;  // One frame
}

// Usage: Lightning bolt
CLG_AddBeam(gun_origin, hit_pos, gi.modelindex("sprites/lightning.sp2"),
            4.0f, {0.5f, 0.5f, 1.0f}, 1.0f);
```

### Dynamic Lights

```cpp
void CLG_AddDynamicLight(vec3_t origin, float radius, vec3_t color) {
    dlight_t *dl = CLG_AllocDLight();
    dl->origin = origin;
    dl->radius = radius;
    dl->color = color;
    dl->decay = 200.0f;  // Decay rate
    dl->lifetime = 0.1f;
}

// Usage: Muzzle flash light
CLG_AddDynamicLight(gun_origin, 200.0f, {1.0f, 0.8f, 0.3f});
```

## HUD System

### HUD Architecture

```cpp
void CLG_DrawHUD(void) {
    if (!cl.frame.valid)
        return;
        
    player_state_t *ps = &cl.frame.playerstate;
    
    // Draw HUD elements
    CLG_DrawHealth(ps);
    CLG_DrawAmmo(ps);
    CLG_DrawArmor(ps);
    CLG_DrawWeapon(ps);
    CLG_DrawCrosshair();
    CLG_DrawScore(ps);
    CLG_DrawMessages();
}
```

### HUD Elements

```cpp
void CLG_DrawHealth(player_state_t *ps) {
    int health = ps->stats[STAT_HEALTH];
    
    // Health bar background
    DrawRect(10, screenHeight - 50, 100, 30, {0, 0, 0, 0.5f});
    
    // Health bar (red to green based on value)
    float ratio = health / 100.0f;
    vec3_t color = LerpColor({1, 0, 0}, {0, 1, 0}, ratio);
    DrawRect(10, screenHeight - 50, 100 * ratio, 30, color);
    
    // Health number
    DrawString(10, screenHeight - 50, va("%d", health), {1, 1, 1});
}

void CLG_DrawAmmo(player_state_t *ps) {
    int ammo = ps->stats[STAT_AMMO];
    int ammo_icon = ps->stats[STAT_AMMO_ICON];
    
    if (ammo_icon && ammo >= 0) {
        // Draw ammo icon
        DrawPic(screenWidth - 80, screenHeight - 50, 
                cl.configstrings[CS_IMAGES + ammo_icon]);
        
        // Draw ammo count
        DrawString(screenWidth - 40, screenHeight - 50, 
                   va("%d", ammo), {1, 1, 1});
    }
}
```

### Crosshair

```cpp
void CLG_DrawCrosshair(void) {
    if (!cl_crosshair->value)
        return;
        
    int x = screenWidth / 2;
    int y = screenHeight / 2;
    
    // Load crosshair texture
    texture_t *crosshair = R_RegisterPic("ch1");
    
    // Draw centered
    DrawPic(x - 8, y - 8, crosshair);
    
    // Optional: Change color based on target
    if (PlayerLookingAtEnemy()) {
        DrawPic(x - 8, y - 8, crosshair, {1, 0, 0});  // Red
    }
}
```

## View Calculation

### Camera Position

```cpp
void CLG_CalcViewValues(void) {
    player_state_t *ps = &cl.predicted_player;
    
    // Base view position (eye height)
    cl.refdef.vieworg = ps->pmove.origin;
    cl.refdef.vieworg[2] += ps->viewheight;
    
    // Add view offset (ducking, stairs)
    cl.refdef.vieworg += ps->viewoffset;
    
    // Add view bobbing
    CLG_AddViewBob(&cl.refdef.vieworg, ps);
    
    // Add kick angles (weapon recoil)
    cl.refdef.viewangles = ps->viewangles + ps->kick_angles;
    
    // Add view roll (strafing)
    CLG_AddViewRoll(&cl.refdef.viewangles, ps);
}
```

### View Bobbing

```cpp
void CLG_AddViewBob(vec3_t *vieworg, player_state_t *ps) {
    if (!(ps->pmove.flags & PMF_DUCKED)) {
        // Calculate bob cycle
        float bobcycle = cl.time * 0.01f;
        float bobvalue = sin(bobcycle * M_PI * 2) * 1.0f;
        
        // Apply bob to view
        (*vieworg)[2] += bobvalue;
    }
}
```

### Weapon View Model

```cpp
void CLG_AddViewWeapon(void) {
    player_state_t *ps = &cl.predicted_player;
    
    if (!ps->stats[STAT_GUN_INDEX])
        return;  // No weapon
        
    entity_t gun = {};
    
    // Weapon model
    gun.model = cl.model_draw[ps->stats[STAT_GUN_INDEX]];
    gun.frame = ps->stats[STAT_GUN_FRAME];
    
    // Position relative to view
    gun.origin = cl.refdef.vieworg;
    gun.origin += cl.refdef.viewforward * ps->gunoffset[0];
    gun.origin += cl.refdef.viewright * ps->gunoffset[1];
    gun.origin += cl.refdef.viewup * ps->gunoffset[2];
    
    // Angles
    gun.angles = cl.refdef.viewangles;
    gun.angles += ps->gunangles;
    
    // Add weapon bob
    CLG_AddWeaponBob(&gun, ps);
    
    V_AddEntity(&gun);
}
```

## Entity Events

Entity events are one-time occurrences attached to entities:

```cpp
void CLG_ParseEntityEvents(centity_t *cent) {
    entity_state_t *s = &cent->current;
    
    // Extract event
    int event = s->event & ~EV_EVENT_BITS;
    
    if (event == 0)
        return;
        
    switch (event) {
        case EV_PLAYER_FOOTSTEP:
            CLG_PlayFootstep(cent);
            break;
            
        case EV_WEAPON_PRIMARY_FIRE:
            CLG_WeaponFireEffect(cent);
            break;
            
        case EV_ITEM_RESPAWN:
            CLG_ItemRespawnEffect(cent);
            break;
            
        // ... handle all events ...
    }
}

void CLG_PlayFootstep(centity_t *cent) {
    // Determine surface type
    int contents = CL_PMPointContents(cent->current.origin);
    
    const char *sound;
    if (contents & CONTENTS_WATER) {
        sound = "player/step_water.wav";
    } else if (contents & CONTENTS_SLIME) {
        sound = "player/step_slime.wav";
    } else {
        sound = "player/step.wav";
    }
    
    S_StartSound(cent->current.origin, cent->current.number, 
                 CHAN_BODY, S_RegisterSound(sound), 1.0f, ATTN_NORM, 0);
}
```

**See:** [Entity Events](API-Entity-Events.md)

## Local Entities

Client-only entities that don't come from server:

```cpp
void CLG_EjectShell(vec3_t origin, vec3_t velocity, int shellType) {
    local_entity_t *le = CLG_AllocLocalEntity();
    
    le->type = LE_SHELL;
    le->origin = origin;
    le->velocity = velocity;
    le->avelocity = RandomVec() * 360;  // Random spin
    le->lifetime = 2.0f;
    
    le->model = shellType == SHELL_SHOTGUN ? 
                cl.model_draw[cl_mod_shotgun_shell] :
                cl.model_draw[cl_mod_machinegun_shell];
}

void CLG_UpdateLocalEntities(int msec) {
    float dt = msec * 0.001f;
    
    for (int i = 0; i < cl.num_local_entities; i++) {
        local_entity_t *le = &cl.local_entities[i];
        
        // Update lifetime
        le->lifetime -= dt;
        if (le->lifetime <= 0) {
            CLG_FreeLocalEntity(le);
            continue;
        }
        
        // Apply physics
        le->velocity[2] -= 800.0f * dt;  // Gravity
        le->origin += le->velocity * dt;
        le->angles += le->avelocity * dt;
        
        // Collision check
        cm_trace_t tr = CL_Trace(le->prev_origin, le->origin);
        if (tr.fraction < 1.0f) {
            // Bounce
            float backoff = DotProduct(le->velocity, tr.plane.normal) * 1.5f;
            VectorMA(le->velocity, -backoff, tr.plane.normal, le->velocity);
            le->velocity *= 0.5f;  // Energy loss
            
            // Play clink sound
            S_StartSound(tr.endpos, -1, CHAN_AUTO, 
                        S_RegisterSound("weapons/shellhit.wav"),
                        0.3f, ATTN_STATIC, 0);
        }
    }
}
```

## Performance Optimization

### Particle Culling

```cpp
// Don't update particles outside view frustum
if (!R_CullBox(p->org - p->scale, p->org + p->scale)) {
    CLG_UpdateParticle(p, dt);
}
```

### LOD for Effects

```cpp
// Reduce particle count based on distance
float dist = VectorDistance(p->org, cl.refdef.vieworg);
if (dist > 1000.0f) {
    // Far away - reduce quality
    if (rand() % 2 == 0) {
        continue;  // Skip this particle
    }
}
```

## Debugging

### Visualization

```cpp
if (cl_show_prediction->value) {
    // Draw predicted position
    DrawBox(cl.predicted_origin - 16, cl.predicted_origin + 16, {0, 1, 0});
    
    // Draw server position
    DrawBox(cl.frame.playerstate.pmove.origin - 16,
            cl.frame.playerstate.pmove.origin + 16, {1, 0, 0});
}

if (cl_show_interpolation->value) {
    for (each entity) {
        DrawLine(cent->prev.origin, cent->current.origin, {1, 1, 0});
    }
}
```

## Related Documentation

- [Entity System Overview](Entity-System-Overview.md) - Entity architecture
- [Temp Entity System](Temp-Entity-Overview.md) - Temporary entities
- [Entity Events](API-Entity-Events.md) - Entity event system
- [Server Game Module](Server-Game-Module.md) - Server-side counterpart
- [Shared Game Module](Shared-Game-Module.md) - Shared code (pmove, etc.)

---

**Key Takeaway:** The client game module is all about presentation and prediction. It makes the game feel responsive despite network latency, but never makes authoritative decisions.
