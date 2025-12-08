# Vanilla Quake 2 - Game Module Architecture

Understanding vanilla Quake 2's architecture is essential before diving into Q2RTXPerimental's enhancements. This page covers the fundamental client-server separation and game module design that forms the foundation of the engine.

## Overview

Quake 2's architecture separates the engine from game logic through a DLL-based plugin system. This design allows for:
- **Modding without engine recompilation** - Game logic lives in separate DLLs
- **Clear separation of concerns** - Engine handles low-level systems, game DLLs handle gameplay
- **Network transparency** - The same game code works for single-player and multiplayer

## Client-Server Architecture

### The Three-Layer Model

Quake 2 uses a three-layer architecture even in single-player:

```
┌─────────────────────────────────────────┐
│         Client Application              │
│  (Rendering, Input, Sound, UI)          │
│         + Client Game DLL                │
│  (Prediction, Interpolation, Effects)   │
└─────────────────────────────────────────┘
                   ↕ (Network or Local Loop)
┌─────────────────────────────────────────┐
│         Server Application              │
│  (Physics, Networking, Level Management) │
│         + Server Game DLL                │
│  (Game Logic, AI, Combat, Entities)     │
└─────────────────────────────────────────┘
```

**Key Points:**
- The **client** is responsible for presentation - it renders what the player sees
- The **server** is the authority - it simulates the game world and sends updates
- In single-player, both run in the same process but maintain separation
- In multiplayer, they run on different machines

### Why This Separation?

1. **Network Play**: The server can run on a dedicated machine, serving multiple clients
2. **Anti-Cheat**: Server validates all actions; clients can't cheat by modifying local code
3. **Prediction**: Client can predict player movement for smooth gameplay despite latency
4. **Determinism**: Server runs the same simulation for all clients

## Game Module Interface

### The DLL Boundary

Game logic is separated into **two DLLs** (or shared libraries on Linux):

1. **Game DLL** (`game.dll` / `game.so`)
   - Server-side game logic
   - Entity management, physics, AI, combat
   - Authoritative game state

2. **Client Game DLL** (`cgame.dll` / `cgame.so`)  
   - Client-side effects and prediction
   - HUD rendering, particle effects
   - View bobbing, weapon positioning

### The Import/Export Pattern

Game modules communicate with the engine through function pointer tables:

```cpp
// Engine provides these functions TO the game
typedef struct {
    void (*bprintf)(int printlevel, char *fmt, ...);
    void (*dprintf)(char *fmt, ...);
    void (*cprintf)(edict_t *ent, int printlevel, char *fmt, ...);
    void (*centerprintf)(edict_t *ent, char *fmt, ...);
    void (*sound)(edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs);
    void (*positioned_sound)(vec3_t origin, edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs);
    // ... many more functions ...
} game_import_t;

// Game provides these functions TO the engine
typedef struct {
    int         apiversion;
    void        (*Init)(void);
    void        (*Shutdown)(void);
    void        (*SpawnEntities)(char *mapname, char *entities, char *spawnpoint);
    void        (*RunFrame)(void);
    // ... more functions ...
} game_export_t;
```

### Entry Point

The game DLL exports a single function that returns the export table:

```cpp
// In game DLL
game_export_t *GetGameAPI(game_import_t *import) {
    gi = *import;  // Store engine's import table
    
    globals.apiversion = GAME_API_VERSION;
    globals.Init = InitGame;
    globals.Shutdown = ShutdownGame;
    // ... set up all function pointers ...
    
    return &globals;
}
```

The engine:
1. Loads the game DLL
2. Finds the `GetGameAPI` function
3. Passes its import table to the game
4. Receives the game's export table
5. Calls game functions through the export table

## Server Game Module

The server game module (`game.dll`) is where most of the gameplay happens.

### Responsibilities

- **Entity Management**: Creating, updating, and destroying entities
- **Physics Simulation**: Movement, collision detection, gravity
- **Combat System**: Damage calculation, hit detection, death handling
- **AI**: Monster behavior, pathfinding, decision making
- **Game Rules**: Scoring, team management, win conditions
- **Persistence**: Saving and loading game state

### Frame Loop

Every server frame (typically 10 Hz in vanilla Quake 2, 40 Hz in Q2RTXPerimental):

```cpp
void G_RunFrame(void) {
    // 1. Process player commands (movement, shooting, etc.)
    for (each player) {
        ClientThink(player);
    }
    
    // 2. Run entity think functions
    for (each entity) {
        if (entity->nextthink <= level.time) {
            entity->think(entity);
        }
    }
    
    // 3. Run physics
    G_RunEntity(entity);  // For each entity
    
    // 4. Build and send snapshots to clients
    // (Done by engine, not game code)
}
```

### Entity Think Functions

Entities in Quake 2 use a callback-based system:

```cpp
// Example: A door that opens when triggered
void door_use(edict_t *self, edict_t *other, edict_t *activator) {
    self->moveinfo.state = STATE_UP;  // Start opening
    self->think = door_go_up;         // Set think function
    self->nextthink = level.time + FRAMETIME;
}

void door_go_up(edict_t *self) {
    Move_Calc(self, self->moveinfo.end_origin, door_hit_top);
}

void door_hit_top(edict_t *self) {
    if (self->wait >= 0) {
        self->think = door_go_down;
        self->nextthink = level.time + self->wait;
    }
}
```

**Key Concepts:**
- Each entity has a `think` function pointer
- `nextthink` specifies when to call it (in game time)
- Think functions can chain by setting new think/nextthink

## Client Game Module

The client game module (`cgame.dll`) handles presentation and prediction.

### Responsibilities

- **Prediction**: Predicting player movement before server confirms
- **Interpolation**: Smoothing entity positions between server snapshots
- **Effects**: Particles, temporary entities, muzzle flashes
- **HUD**: Drawing health, ammo, scores, etc.
- **View**: Camera angles, weapon positioning, view bobbing

### Frame Loop

Every client frame (as fast as possible, limited by FPS):

```cpp
void CL_ClientFrame(int msec) {
    // 1. Predict player movement
    CL_PredictMovement();
    
    // 2. Interpolate entity positions
    CL_AddEntities();
    
    // 3. Add temporary effects
    CL_AddTempEntities();
    
    // 4. Update view
    CL_CalcViewValues();
    
    // 5. Draw HUD
    SCR_DrawStats();
}
```

### Prediction System

To hide network latency, the client predicts local player movement:

```cpp
// Client predicts multiple frames ahead
void CL_PredictMovement(void) {
    // Start from last acknowledged server position
    pm.s = cl.frame.playerstate.pmove;
    
    // Replay all unacknowledged player commands
    for (int i = last_ack; i < current_cmd; i++) {
        PlayerMove(&pm, cmd[i]);  // Same movement code as server
    }
    
    // Use predicted position for rendering
    cl.predicted_origin = pm.s.origin;
}
```

**Why This Works:**
- Client and server use **identical movement code**
- Client can predict accurately because physics is deterministic
- When server update arrives, client corrects any drift

### Temporary Entities

Some visual effects don't need persistent entities:

```cpp
// Server tells client: "Play a blood splat effect at this location"
gi.WriteByte(svc_temp_entity);
gi.WriteByte(TE_BLOOD);
gi.WritePosition(origin);
gi.WriteDir(direction);
gi.multicast(origin, MULTICAST_PVS);

// Client receives this and spawns a temporary particle effect
// No entity is created, no updates are sent
```

## Communication Between Client and Server

### Server → Client: Snapshots

The server sends **snapshots** to clients at regular intervals:

```cpp
// Server builds a snapshot
snapshot_t {
    int         serverTime;          // Current server time
    playerstate_t playerstate;        // Local player state
    int         num_entities;         // Number of entities
    entity_state_t entities[MAX_ENTITIES];  // All visible entities
};
```

**Snapshot Contents:**
- Player state (health, ammo, position, velocity, etc.)
- All entities visible to the player (in PVS - Potentially Visible Set)
- Game state (scores, time remaining, etc.)

### Client → Server: Commands

Clients send **user commands** to the server:

```cpp
usercmd_t {
    byte    msec;           // Duration of this command
    short   angles[3];      // View angles
    short   forwardmove;    // Forward/back input
    short   sidemove;       // Left/right input  
    short   upmove;         // Jump/crouch
    byte    buttons;        // Attack, use, etc.
    byte    impulse;        // Weapon switch, etc.
};
```

The server:
1. Receives the command
2. Validates it (anti-cheat)
3. Applies it to the player's movement
4. Simulates the frame
5. Sends resulting snapshot back to client

## Quake 2 Data Structures

### Entity (edict_t)

The fundamental game object:

```cpp
struct edict_t {
    entity_state_t  s;          // Network-visible state
    struct gclient_s *client;   // NULL if not a player
    
    qboolean    inuse;          // Entity is active
    int         linkcount;      // For unlinking/relinking
    
    // Callbacks
    void        (*think)(edict_t *self);
    void        (*touch)(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
    void        (*use)(edict_t *self, edict_t *other, edict_t *activator);
    void        (*pain)(edict_t *self, edict_t *other, float kick, int damage);
    void        (*die)(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);
    
    // Physics
    float       freetime;       // Time when entity is freed
    int         movetype;       // MOVETYPE_*
    int         solid;          // SOLID_*
    
    vec3_t      mins, maxs;     // Bounding box
    vec3_t      absmin, absmax; // World space bounds
    vec3_t      size;           // maxs - mins
    
    // Movement
    vec3_t      origin;
    vec3_t      velocity;
    vec3_t      avelocity;      // Angular velocity
    
    // Game-specific fields
    char        *classname;
    int         spawnflags;
    float       nextthink;
    edict_t     *groundentity;
    // ... many more fields ...
};
```

### Player State (player_state_t)

State specific to players that's sent in snapshots:

```cpp
typedef struct player_state_s {
    pmove_state_t   pmove;      // Movement state (position, velocity)
    
    vec3_t          viewangles; // View direction
    vec3_t          viewoffset;  // Eye position offset
    vec3_t          kick_angles; // Weapon kick
    
    vec3_t          gunangles;  // Weapon model angles
    vec3_t          gunoffset;  // Weapon model offset
    int             gunindex;   // Weapon model index
    int             gunframe;   // Weapon animation frame
    
    // Player stats (visible in HUD)
    short           stats[MAX_STATS];  // Health, ammo, etc.
    
    // Rendering effects
    int             rdflags;    // RDF_*
    // ... more fields ...
} player_state_t;
```

## Networking Model

### Delta Compression

To save bandwidth, only changes are sent:

```cpp
// Instead of sending full entity state every frame:
entity_state_t old_state;  // Last acknowledged state
entity_state_t new_state;  // Current state

// Send delta:
MSG_WriteDeltaEntity(&old_state, &new_state, msg);

// This only sends fields that changed between old and new
```

### Reliable vs. Unreliable

- **Unreliable**: Entity positions, snapshots (if lost, next snapshot will fix it)
- **Reliable**: Important events (player death, item pickup) - resent until acknowledged

### Lag Compensation

The server performs lag compensation for hit detection:

```cpp
// When client shoots:
// 1. Client sends command with their current view time
// 2. Server rewinds entity positions to that time
// 3. Server performs hit detection at rewound positions
// 4. Server applies damage to present-time entities
```

This ensures players hit what they see, despite network latency.

## Map Loading and Entity Spawning

### BSP File

Maps are compiled into `.bsp` files containing:
- Geometry (brushes)
- Lighting data
- Entity spawn string

### Entity String

The BSP contains an entity string in a key-value format:

```
{
"classname" "worldspawn"
"message" "My Level"
}
{
"classname" "info_player_start"
"origin" "100 200 50"
"angle" "90"
}
{
"classname" "monster_soldier"
"origin" "500 300 64"
"angle" "180"
"spawnflags" "1"
}
```

### Spawning Process

When a map loads:

```cpp
void SpawnEntities(char *mapname, char *entities, char *spawnpoint) {
    // 1. Parse entity string
    char *token = COM_Parse(&entities);
    while (token) {
        // 2. Find entity by classname
        spawn_t *spawn = FindSpawnFunction(classname);
        
        // 3. Allocate edict
        edict_t *ent = G_Spawn();
        
        // 4. Parse key-value pairs
        // "origin" "100 200 50" -> ent->s.origin = {100, 200, 50}
        
        // 5. Call spawn function
        spawn->spawn(ent);  // E.g., SP_monster_soldier(ent)
    }
}
```

## Relevance to Q2RTXPerimental

Q2RTXPerimental builds upon this architecture:

**Preserved:**
- Client-server separation
- DLL-based game modules  
- Entity-based game world
- Snapshot networking

**Enhanced:**
- C++ OOP entity system (instead of C callbacks)
- Three game modules: `clgame`, `svgame`, `sharedgame`
- Modern class-based entity hierarchy
- Enhanced networking with higher tick rate (40 Hz)
- Improved prediction and interpolation

Understanding vanilla Quake 2's architecture helps you:
- Know why certain design decisions were made
- Understand where Q2RTXPerimental improves on the foundation
- Debug issues by understanding the underlying systems
- Work with legacy code or documentation

---

**Next Topics:**
- [Vanilla Entity System](Vanilla-Entity-System.md) - How entities work in vanilla Quake 2
- [Vanilla Networking](Vanilla-Networking.md) - Deep dive into client-server communication
- [Architecture Overview](Architecture-Overview.md) - Q2RTXPerimental's enhanced architecture
