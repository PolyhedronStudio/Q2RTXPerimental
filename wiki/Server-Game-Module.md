# Server Game Module (svgame)

The Server Game Module (`svgame`) is the heart of Q2RTXPerimental's game logic, implementing the authoritative server-side simulation that all clients trust.

**Location:** `src/baseq2rtxp/svgame/`  
**Output:** `game.dll` (Windows) / `game.so` (Linux)

## Overview

The svgame module is responsible for:
- **Entity Management**: Creating, updating, and destroying all game entities
- **Physics Simulation**: Movement, collision detection, gravity
- **Combat System**: Damage calculation, hit detection, death handling
- **AI**: Monster behavior, pathfinding, decision making
- **Game Rules**: Scoring, win conditions, game modes
- **Persistence**: Saving and loading game state
- **Lua Integration**: Server-side scripting support

## Module Structure

```
svgame/
├── entities/                   # Entity class hierarchy
│   ├── svg_base_edict.h/.cpp  # Base entity class
│   ├── svg_player_edict.h/.cpp        # Player entities
│   ├── svg_item_edict.h/.cpp          # Item pickup entities
│   ├── svg_pushmove_edict.h/.cpp      # Moving brush base
│   ├── svg_worldspawn_edict.h/.cpp    # World entity
│   ├── func/                   # Brush entities
│   │   ├── svg_func_door.h/.cpp
│   │   ├── svg_func_plat.h/.cpp
│   │   ├── svg_func_train.h/.cpp
│   │   └── ...
│   ├── trigger/                # Trigger volumes
│   │   ├── svg_trigger_multiple.h/.cpp
│   │   ├── svg_trigger_hurt.h/.cpp
│   │   ├── svg_trigger_push.h/.cpp
│   │   └── ...
│   ├── misc/                   # Miscellaneous entities
│   ├── monster/                # Monster entities
│   ├── info/                   # Info entities (spawn points, etc.)
│   ├── light/                  # Light entities
│   └── target/                 # Target entities
├── gamemodes/                  # Game mode implementations
│   ├── svg_gamemode_coop.cpp
│   ├── svg_gamemode_deathmatch.cpp
│   └── ...
├── lua/                        # Lua scripting integration
│   ├── svg_lua.h/.cpp
│   └── svg_lua_gamelib.cpp
├── monsters/                   # AI implementations
│   ├── svg_ai.cpp              # General AI utilities
│   └── (monster-specific AI)
├── player/                     # Player-specific logic
│   ├── svg_player_client.cpp  # Client connection handling
│   ├── svg_player_hud.cpp     # HUD updates
│   └── svg_player_view.cpp    # View calculation
├── save/                       # Save/load system
│   ├── svg_save.h/.cpp
│   └── svg_save_pointers.cpp
├── weapons/                    # Weapon implementations
├── svg_main.cpp                # Module entry point (GetGameAPI)
├── svg_spawn.cpp               # Entity spawning from BSP
├── svg_edict_pool.cpp          # Entity pool management
├── svg_edicts.cpp              # Entity utilities
├── svg_physics.cpp             # Physics simulation
├── svg_combat.cpp              # Combat system
├── svg_clients.cpp             # Client management
├── svg_game_client.cpp         # Per-client game logic
├── svg_gamemode.cpp            # Game mode system
├── svg_signalio.cpp            # Signal I/O system
├── svg_usetargets.h            # UseTargets system
└── ...
```

## Module Initialization

### Entry Point

The engine loads `game.dll` and calls `GetGameAPI`:

```cpp
// In svg_main.cpp
game_export_t *GetGameAPI(game_import_t *import) {
    gi = *import;  // Store engine's function table
    
    globals.apiversion = GAME_API_VERSION;
    globals.Init = SVG_Init;
    globals.Shutdown = SVG_Shutdown;
    globals.SpawnEntities = SVG_SpawnEntities;
    globals.RunFrame = SVG_RunFrame;
    // ... set up all export functions ...
    
    return &globals;
}
```

### Initialization Sequence

```cpp
void SVG_Init(void) {
    gi.dprintf("Server Game Initialization\n");
    
    // Initialize subsystems
    SVG_InitItems();           // Register all items
    SVG_InitGameMode();        // Set up game mode
    SVG_InitLua();             // Initialize Lua VM
    
    // Register console commands
    SVG_RegisterCommands();
    
    // Set up game locals
    game.serverflags = 0;
    game.maxclients = (int)maxclients->value;
    game.maxentities = (int)maxentities->value;
}
```

## Frame Loop

Every server frame (25ms at 40Hz tick rate):

```cpp
void SVG_RunFrame(void) {
    level.frameNumber++;
    level.time += FRAMETIME;  // 25ms
    
    // 1. Process player input
    for (int i = 0; i < maxclients->value; i++) {
        edict_t *ent = &g_edicts[i + 1];
        if (!ent->inuse || !ent->client)
            continue;
            
        SVG_ClientThink(ent, &ent->client->ucmd);
    }
    
    // 2. Run entity think functions
    SVG_RunEntities();
    
    // 3. Check game rules
    SVG_CheckGameRules();
    
    // 4. Update Lua scripts
    if (lua_enabled->value)
        SVG_LuaRunFrame();
        
    // 5. Build client frames (snapshots)
    // (Done by engine after this function returns)
}
```

## Entity System

### Base Entity Class

All entities derive from `svg_base_edict_t`:

```cpp
// Simplified structure
struct svg_base_edict_t {
    // Networked state (sent to clients)
    entity_state_t s;
    
    // Server-only data
    int linkcount;
    bool inuse;
    
    // Physics
    int movetype;           // MOVETYPE_NONE, MOVETYPE_WALK, etc.
    int solid;              // SOLID_NOT, SOLID_BBOX, SOLID_BSP
    vec3_t mins, maxs;      // Bounding box
    vec3_t velocity;        // Movement speed
    
    // Game data
    const char *classname;
    int spawnflags;
    int health;
    int max_health;
    
    // Relationships
    svg_base_edict_t *groundentity;  // What we're standing on
    svg_base_edict_t *enemy;         // AI target
    
    // Timing
    gametime_t nextThinkTime;        // When to think next
    
    // Entity dictionary (spawn parameters from map)
    cm_entity_t *entityDictionary;
    
    // Virtual methods (callbacks)
    virtual void Spawn();
    virtual void Think();
    virtual void Touch(svg_base_edict_t *other, cm_plane_t *plane, cm_surface_t *surf);
    virtual void Use(svg_base_edict_t *other, svg_base_edict_t *activator);
    virtual void Pain(svg_base_edict_t *attacker, float kick, int damage);
    virtual void Die(svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int damage, vec3_t point);
    
    // Save/load support
    virtual const svg_save_descriptor_t *GetSaveDescriptor();
};
```

### Entity Spawning

Entities are spawned from the BSP entity string:

```cpp
void SVG_SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint) {
    // Parse entity string from BSP
    cm_entity_t *ed = nullptr;
    while ((ed = gi.CM_ParseEntity(entities, ed)) != nullptr) {
        // Get classname
        const char *classname = ed->classname;
        
        // Find spawn function
        const EdictTypeInfo *typeInfo = EdictTypeInfo::FindByClassName(classname);
        if (!typeInfo) {
            gi.dprintf("No spawn function for '%s'\n", classname);
            continue;
        }
        
        // Allocate entity
        svg_base_edict_t *ent = SVG_Spawn(typeInfo);
        
        // Store entity dictionary (spawn parameters)
        ent->entityDictionary = ed;
        
        // Parse key-value pairs
        ParseEntityFields(ent, ed);
        
        // Call spawn function
        ent->Spawn();
    }
    
    // Post-spawn initialization
    SVG_PostSpawnEntities();
}
```

Example entity spawn function:

```cpp
void svg_func_door_t::Spawn() {
    // Call base class
    svg_base_edict_t::Spawn();
    
    // Set up physics
    solid = SOLID_BSP;
    movetype = MOVETYPE_PUSH;
    
    // Set model from BSP
    gi.SetModel(edict, entityDictionary->model);
    
    // Read spawn parameters
    speed = GetEntityDictValue<float>("speed", 100.0f);
    wait = GetEntityDictValue<float>("wait", 3.0f);
    lip = GetEntityDictValue<float>("lip", 8.0f);
    
    // Calculate move distance
    CalculateMoveInfo();
    
    // Set up callbacks
    SetUseCallback(&svg_func_door_t::DoorUse);
    SetBlockedCallback(&svg_func_door_t::DoorBlocked);
    
    // Link into world
    gi.LinkEntity(edict);
}
```

### Entity Think Functions

Entities perform periodic actions through Think:

```cpp
class svg_misc_strobelight_t : public svg_base_edict_t {
public:
    void Spawn() override {
        svg_base_edict_t::Spawn();
        
        // Set up initial state
        s.entityType = ET_GENERAL;
        s.effects = EF_ANIM01;  // Start on
        
        // Schedule first think
        SetThinkCallback(&svg_misc_strobelight_t::StrobeThink);
        nextThinkTime = level.time + FRAMETIME;
        
        gi.LinkEntity(edict);
    }
    
    void StrobeThink() {
        // Toggle light
        if (s.effects & EF_ANIM01) {
            s.effects &= ~EF_ANIM01;  // Turn off
        } else {
            s.effects |= EF_ANIM01;   // Turn on
        }
        
        // Think again in 0.5 seconds
        nextThinkTime = level.time + gametime_t::from_sec(0.5f);
    }
};
```

The frame loop calls think functions:

```cpp
void SVG_RunEntities(void) {
    for (int i = 1; i < globals.num_edicts; i++) {
        svg_base_edict_t *ent = GetEntityByIndex(i);
        if (!ent->inuse)
            continue;
            
        // Time to think?
        if (ent->nextThinkTime && ent->nextThinkTime <= level.time) {
            ent->DispatchThinkCallback();
        }
    }
}
```

## Physics System

### Movement Types

```cpp
enum svg_movetype_t {
    MOVETYPE_NONE,          // No movement
    MOVETYPE_NOCLIP,        // No collision, moves freely
    MOVETYPE_PUSH,          // Pushes other entities (doors, platforms)
    MOVETYPE_STOP,          // Stops when blocked
    MOVETYPE_WALK,          // Ground-based with gravity (players)
    MOVETYPE_STEP,          // Steps over obstacles (monsters)
    MOVETYPE_ROOTMOTION,    // Animation-driven movement
    MOVETYPE_FLY,           // Ignores gravity
    MOVETYPE_TOSS,          // Affected by gravity (gibs, items)
    MOVETYPE_FLYMISSILE,    // Projectile with gravity
    MOVETYPE_BOUNCE         // Bounces off surfaces (grenades)
};
```

### Physics Simulation

```cpp
void SVG_Physics_Toss(svg_base_edict_t *ent) {
    // Apply gravity
    if (ent->movetype != MOVETYPE_FLY) {
        ent->velocity.z -= sv_gravity->value * FRAMETIME.count();
    }
    
    // Move entity
    vec3_t move = ent->velocity * FRAMETIME.count();
    cm_trace_t trace = SVG_Trace(ent->s.origin, ent->mins, ent->maxs, 
                                   ent->s.origin + move, ent, MASK_SOLID);
    
    if (trace.fraction < 1.0) {
        // Hit something
        if (ent->movetype == MOVETYPE_BOUNCE) {
            // Bounce off surface
            float backoff = DotProduct(ent->velocity, trace.plane.normal) * 1.5f;
            VectorMA(ent->velocity, -backoff, trace.plane.normal, ent->velocity);
        } else {
            // Stop on ground
            if (trace.plane.normal[2] > 0.7f) {
                ent->groundentity = trace.ent;
                ent->velocity = vec3_origin();
            }
        }
    }
    
    // Update position
    ent->s.origin = trace.endpos;
    gi.LinkEntity(ent->edict);
}
```

### Player Movement

Player movement uses the shared pmove code (for prediction):

```cpp
void SVG_ClientThink(svg_base_edict_t *ent, usercmd_t *ucmd) {
    // Set up player move
    pmove_t pm = {};
    pm.s = ent->client->ps.pmove;
    pm.cmd = *ucmd;
    pm.trace = SVG_PMTrace;  // Collision function
    pm.pointcontents = gi.pointcontents;
    
    // Run movement
    Pmove(&pm);  // Shared code in sharedgame/pmove/
    
    // Apply results
    ent->client->ps.pmove = pm.s;
    ent->s.origin = pm.s.origin;
    ent->velocity = pm.s.velocity;
    
    // Handle touch triggers, water events, etc.
    SVG_TouchTriggers(ent);
}
```

## Combat System

### Damage Dealing

```cpp
void SVG_Damage(svg_base_edict_t *targ, svg_base_edict_t *inflictor, 
                svg_base_edict_t *attacker, vec3_t dir, vec3_t point,
                vec3_t normal, int damage, int knockback, 
                entity_damageflags_t dflags, means_of_death_t mod) {
    
    // Can target take damage?
    if (!targ || !targ->takedamage)
        return;
        
    // Check immunity
    if (CheckDamageImmunity(targ, mod))
        return;
        
    // Apply armor
    if (targ->client && !(dflags & DAMAGE_NO_ARMOR)) {
        damage = ApplyArmorDamage(targ->client, damage);
    }
    
    // Apply damage
    targ->health -= damage;
    
    // Call pain callback
    if (targ->Pain) {
        targ->Pain(attacker, knockback, damage);
    }
    
    // Check for death
    if (targ->health <= 0) {
        SVG_Killed(targ, inflictor, attacker, damage, point, mod);
    }
}
```

### Hit Detection

```cpp
bool SVG_FireBullet(svg_base_edict_t *shooter, vec3_t start, vec3_t dir, 
                     int damage, int kick, int hspread, int vspread, means_of_death_t mod) {
    // Apply spread
    vec3_t forward, right, up;
    AngleVectors(dir, forward, right, up);
    
    vec3_t offset;
    offset[0] = forward[0] + random() * hspread;
    offset[1] = right[1] + random() * vspread;
    VectorNormalize(offset);
    
    // Trace bullet path
    vec3_t end = start + offset * 8192;  // Max bullet range
    cm_trace_t tr = SVG_Trace(start, vec3_origin, vec3_origin, end, 
                               shooter, MASK_SHOT);
    
    if (tr.fraction < 1.0) {
        // Hit something
        svg_base_edict_t *hit = tr.ent;
        
        // Spawn bullet impact effect
        if (!(tr.surface->flags & CM_SURFACE_FLAG_SKY)) {
            gi.WriteByte(svc_temp_entity);
            gi.WriteByte(TE_BULLET_SPARKS);
            gi.WritePosition(tr.endpos);
            gi.WriteDir(tr.plane.normal);
            gi.multicast(tr.endpos, MULTICAST_PVS);
        }
        
        // Apply damage
        if (hit->takedamage) {
            SVG_Damage(hit, shooter, shooter, dir, tr.endpos, tr.plane.normal,
                       damage, kick, DAMAGE_BULLET, mod);
            return true;
        }
    }
    
    return false;
}
```

## AI System

### Basic AI Structure

```cpp
class svg_monster_soldier_t : public svg_base_edict_t {
public:
    void Spawn() override {
        // Set up monster
        health = 30;
        gib_health = -30;
        mass = 100;
        
        solid = SOLID_BBOX;
        movetype = MOVETYPE_STEP;
        
        VectorSet(mins, -16, -16, -24);
        VectorSet(maxs, 16, 16, 32);
        
        // Set up AI
        SetThinkCallback(&svg_monster_soldier_t::IdleThink);
        nextThinkTime = level.time + FRAMETIME;
        
        gi.LinkEntity(edict);
    }
    
    void IdleThink() {
        // Look for enemies
        svg_base_edict_t *enemy = SVG_FindTarget(this);
        if (enemy) {
            this->enemy = enemy;
            SetThinkCallback(&svg_monster_soldier_t::CombatThink);
        }
        
        nextThinkTime = level.time + FRAMETIME;
    }
    
    void CombatThink() {
        // Lost sight of enemy?
        if (!SVG_CanSee(this, enemy)) {
            SetThinkCallback(&svg_monster_soldier_t::SearchThink);
            return;
        }
        
        // Face enemy
        SVG_AITurnToward(this, enemy->s.origin);
        
        // In attack range?
        float dist = VectorDistance(s.origin, enemy->s.origin);
        if (dist < 1000 && SVG_InFront(this, enemy)) {
            // Attack
            FireWeapon();
        } else {
            // Move closer
            SVG_AIMoveToward(this, enemy->s.origin);
        }
        
        nextThinkTime = level.time + FRAMETIME;
    }
    
    void Die(svg_base_edict_t *inflictor, svg_base_edict_t *attacker, 
             int damage, vec3_t point) override {
        // Play death animation
        s.frame = FRAME_death01;
        
        // Become a corpse
        solid = SOLID_NOT;
        movetype = MOVETYPE_TOSS;
        takedamage = DAMAGE_NO;
        
        // Schedule removal
        SetThinkCallback(&svg_monster_soldier_t::Dissolve);
        nextThinkTime = level.time + gametime_t::from_sec(30.0f);
    }
};
```

## Signal I/O System

Entities can communicate through signals:

```cpp
// Entity emits a signal
void svg_func_button_t::Press() {
    // Emit signal to targets
    EmitSignal("OnPressed", this, activator, {
        {"button_id", id},
        {"press_time", level.time.count()}
    });
}

// Another entity listens for signal
class svg_func_door_t : public svg_base_edict_t {
    void Spawn() override {
        // Register signal handler
        RegisterSignalHandler("OnPressed", &svg_func_door_t::OnButtonPressed);
    }
    
    void OnButtonPressed(svg_base_edict_t *sender, svg_base_edict_t *activator,
                         const svg_signal_argument_array_t &args) {
        // Open door when button pressed
        StartOpening();
    }
};
```

See: [Signal I/O System](Signal-IO-System.md)

## Lua Integration

Server-side Lua scripting:

```cpp
// Load and run Lua script
void SVG_LuaLoadScript(const char *filename) {
    lua_State *L = svg_lua.state;
    
    if (luaL_dofile(L, filename) != 0) {
        gi.dprintf("Lua error: %s\n", lua_tostring(L, -1));
        return;
    }
}

// Call Lua function from C++
void SVG_CallLuaEntityThink(svg_base_edict_t *ent) {
    lua_State *L = svg_lua.state;
    
    // Get entity's think function
    lua_getglobal(L, "EntityThink");
    if (lua_isfunction(L, -1)) {
        // Push entity as argument
        lua_pushlightuserdata(L, ent);
        
        // Call function
        if (lua_pcall(L, 1, 0, 0) != 0) {
            gi.dprintf("Lua error in EntityThink: %s\n", lua_tostring(L, -1));
        }
    }
}
```

Lua script example:

```lua
-- scripts/custom_entity.lua
function EntitySpawn(entity)
    entity:SetHealth(100)
    entity:SetModel("models/custom.iqm")
    print("Custom entity spawned!")
end

function EntityThink(entity)
    -- Custom AI logic
    local enemy = entity:FindEnemy()
    if enemy then
        entity:AttackEnemy(enemy)
    end
end
```

See: [Lua Scripting](Lua-Scripting.md)

## Save/Load System

Entities can be saved and loaded:

```cpp
// Define save descriptor for entity
const svg_save_descriptor_t svg_func_door_t::saveDescriptor = {
    FIELD_INT(state),
    FIELD_FLOAT(speed),
    FIELD_FLOAT(wait),
    FIELD_VEC3(moveDir),
    FIELD_VEC3(startOrigin),
    FIELD_VEC3(endOrigin),
    FIELD_END
};

// Save entity
void SVG_SaveEntity(svg_base_edict_t *ent, FILE *f) {
    const svg_save_descriptor_t *desc = ent->GetSaveDescriptor();
    SVG_WriteFields(f, ent, desc);
}

// Load entity
void SVG_LoadEntity(svg_base_edict_t *ent, FILE *f) {
    const svg_save_descriptor_t *desc = ent->GetSaveDescriptor();
    SVG_ReadFields(f, ent, desc);
}
```

See: [Entity Save/Load System](Entity-Save-Load.md)

## Key Files Reference

| File | Purpose |
|------|---------|
| `svg_main.cpp` | Module entry point, exports GetGameAPI |
| `svg_spawn.cpp` | Entity spawning from BSP |
| `svg_edict_pool.cpp` | Entity allocation/deallocation |
| `svg_physics.cpp` | Physics simulation |
| `svg_combat.cpp` | Damage and combat |
| `svg_clients.cpp` | Client connection management |
| `svg_game_client.cpp` | Per-client game logic |
| `svg_ai.cpp` | AI utilities |
| `svg_gamemode.cpp` | Game mode system |
| `entities/svg_base_edict.h/.cpp` | Base entity class |

## Next Steps

- [Entity System Overview](Entity-System-Overview.md) - Deep dive into entity architecture
- [Creating Custom Entities](Creating-Custom-Entities.md) - Tutorial for making entities
- [Entity Base Class Reference](Entity-Base-Class-Reference.md) - Complete svg_base_edict_t API
- [Client Game Module](Client-Game-Module.md) - Learn about the client-side module
- [Lua Scripting](Lua-Scripting.md) - Extend gameplay with Lua

---

**Previous:** [Architecture Overview](Architecture-Overview.md)
