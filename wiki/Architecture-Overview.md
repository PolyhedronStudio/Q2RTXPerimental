# Q2RTXPerimental Architecture Overview

Q2RTXPerimental builds upon the Quake 2 foundation with a modernized C++ architecture, enhanced entity system, and improved separation of concerns. This page provides a high-level overview of the engine's design.

## Architectural Philosophy

Q2RTXPerimental maintains Quake 2's client-server separation while modernizing the implementation:

**Preserved from Vanilla:**
- Client-server architecture for network transparency
- DLL-based game modules for modding flexibility
- Entity-centric game world
- BSP-based level geometry

**Modernizations:**
- **C++ OOP**: Class hierarchies replace C structs and function pointers
- **Type Safety**: Strong typing and compile-time checks
- **Three-Module Design**: `clgame`, `svgame`, `sharedgame` with clear boundaries
- **Enhanced Systems**: Lua scripting, skeletal animation, signal I/O
- **Higher Tick Rate**: 40 Hz server update rate (vs. 20 Hz in vanilla)

## High-Level Architecture

```
┌────────────────────────────────────────────────────────────┐
│                     Q2RTXPerimental                        │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  ┌──────────────────┐              ┌──────────────────┐   │
│  │   Client Side    │              │   Server Side    │   │
│  ├──────────────────┤              ├──────────────────┤   │
│  │                  │              │                  │   │
│  │  Engine Client   │◄────────────►│  Engine Server   │   │
│  │  - Rendering     │   Network    │  - Networking    │   │
│  │  - Input         │   or         │  - Physics       │   │
│  │  - Sound         │   Loopback   │  - Collision     │   │
│  │  - UI            │              │  - Level Mgmt    │   │
│  │                  │              │                  │   │
│  ├──────────────────┤              ├──────────────────┤   │
│  │                  │              │                  │   │
│  │  clgame.dll      │              │  svgame.dll      │   │
│  │  (Client Game)   │              │  (Server Game)   │   │
│  │  - Prediction    │              │  - Entities      │   │
│  │  - Interpolation │              │  - AI            │   │
│  │  - Effects       │              │  - Combat        │   │
│  │  - HUD           │              │  - Physics       │   │
│  │                  │              │  - Lua Scripts   │   │
│  └──────────────────┘              └──────────────────┘   │
│           │                                  │             │
│           │        ┌──────────────────┐      │             │
│           └───────►│  sharedgame.lib  │◄─────┘             │
│                    │  (Shared Code)   │                    │
│                    │  - Entity Types  │                    │
│                    │  - Events        │                    │
│                    │  - Game Modes    │                    │
│                    │  - Player Move   │                    │
│                    │  - Animation     │                    │
│                    └──────────────────┘                    │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

## Module Breakdown

### Engine Core

**Location:** `src/client/`, `src/server/`, `src/common/`, `src/refresh/`

The engine provides low-level systems:
- **Rendering**: Vulkan-based path-traced renderer (`src/refresh/vkpt/`)
- **Networking**: Client-server communication, snapshot delta compression
- **Asset Loading**: BSP, models, textures, sounds
- **Input/Output**: Keyboard, mouse, gamepad, audio output
- **Console/Commands**: Command system, cvars, scripting
- **File System**: PAK/PKZ archive support

The engine is written in C and provides a C API to game modules.

### Server Game Module (svgame)

**Location:** `src/baseq2rtxp/svgame/`
**Output:** `game.dll` / `game.so`

The authoritative server-side game logic:

```
svgame/
├── entities/           # Entity class hierarchy
│   ├── svg_base_edict.h/.cpp    # Base entity class
│   ├── func/           # Brush entities (doors, platforms)
│   ├── trigger/        # Trigger entities
│   ├── misc/           # Miscellaneous entities
│   ├── monster/        # Monster entities
│   └── ...
├── gamemodes/          # Game mode implementations
├── lua/                # Lua scripting integration
├── monsters/           # AI implementations
├── player/             # Player-specific logic
├── save/               # Save/load system
├── svg_main.cpp        # Module entry point
├── svg_spawn.cpp       # Entity spawning
├── svg_physics.cpp     # Physics simulation
├── svg_combat.cpp      # Damage and combat
├── svg_ai.cpp          # AI utilities
└── ...
```

**Key Responsibilities:**
- Entity lifecycle management (spawn, think, die)
- Physics simulation and collision detection
- AI behavior and pathfinding
- Combat system (damage, health, death)
- Game rules and scoring
- Lua script execution
- Authoritative game state

**See:** [Server Game Module](Server-Game-Module.md)

### Client Game Module (clgame)

**Location:** `src/baseq2rtxp/clgame/`
**Output:** `cgame.dll` / `cgame.so`

Client-side presentation and prediction:

```
clgame/
├── effects/            # Visual effect implementations
├── hud/                # HUD element implementations
├── local_entities/     # Client-local entities
├── packet_entities/    # Server entity processing
├── temp_entities/      # Temporary entity handlers
├── clg_main.cpp        # Module entry point
├── clg_predict.cpp     # Client-side prediction
├── clg_packet_entities.cpp  # Entity interpolation
├── clg_temp_entities.cpp    # Temp entity spawning
├── clg_effects.cpp     # Effect systems
├── clg_view.cpp        # View calculation
├── clg_hud.cpp         # HUD rendering
└── ...
```

**Key Responsibilities:**
- Client-side prediction (player movement)
- Entity interpolation (smooth movement between snapshots)
- Temporary entities (particles, beams, effects)
- Visual effects (muzzle flashes, explosions, blood)
- HUD rendering (health, ammo, scores)
- View calculation (bobbing, kick, offsets)
- Event handling (footsteps, weapon sounds)

**See:** [Client Game Module](Client-Game-Module.md)

### Shared Game Module (sharedgame)

**Location:** `src/baseq2rtxp/sharedgame/`
**Output:** Static library linked into both `clgame` and `svgame`

Code and data structures shared between client and server:

```
sharedgame/
├── pmove/              # Player movement code
├── game_bindings/      # Lua bindings
├── sg_entity_types.h   # Entity type constants
├── sg_entity_flags.h   # Entity flag constants
├── sg_entity_events.h  # Entity event types
├── sg_tempentity_events.h  # Temp entity events
├── sg_gamemode.cpp     # Game mode definitions
├── sg_skm.cpp          # Skeletal animation system
├── sg_skm_rootmotion.cpp   # Root motion system
├── sg_usetarget_hints.cpp  # Use target hint system
└── ...
```

**Key Responsibilities:**
- Entity type definitions (used by both client and server)
- Entity event definitions (synchronized between client/server)
- Player movement code (client prediction needs same code as server)
- Game mode definitions
- Skeletal animation system
- Constants and enumerations

**Design Rationale:**
- **Prediction**: Client needs identical movement code to predict player position
- **Consistency**: Ensures client and server interpret data identically
- **DRY Principle**: Avoid duplicating code between modules
- **Type Safety**: Shared enums prevent client/server mismatch

**See:** [Shared Game Module](Shared-Game-Module.md)

## Data Flow

### Server → Client

Every server frame (25ms at 40Hz):

1. **Server Simulation**
   ```cpp
   SVG_RunFrame() {
       ProcessPlayerCommands();    // Apply player input
       RunEntityThink();           // Execute entity logic
       RunPhysics();               // Simulate movement/collision
       BuildSnapshots();           // Package state for clients
   }
   ```

2. **Snapshot Creation**
   - Server creates snapshot containing:
     - Player state (position, health, ammo, etc.)
     - Visible entities (in PVS - Potentially Visible Set)
     - Events (weapon fire, footsteps, etc.)
   - Delta compressed against client's last acknowledged snapshot

3. **Network Transmission**
   - Snapshot sent via UDP (unreliable with ack)
   - Reliable messages sent separately (important events)

4. **Client Reception**
   ```cpp
   CLG_ParseFrame(snapshot) {
       UpdatePlayerState();        // Update local player
       ParsePacketEntities();      // Update entity states
       ProcessEvents();            // Handle entity events
       ProcessTempEntities();      // Spawn temp effects
   }
   ```

5. **Client Rendering**
   ```cpp
   CLG_AddEntities() {
       InterpolateEntities();      // Smooth between snapshots
       PredictPlayerMovement();    // Predict ahead
       AddViewWeapon();            // Render weapon
       AddEntities();              // Add to render scene
   }
   ```

### Client → Server

Every client frame:

1. **Input Sampling**
   ```cpp
   CL_CreateCmd() {
       SampleInput();              // Read keyboard/mouse
       BuildUserCommand();         // Create usercmd_t
   }
   ```

2. **Command Transmission**
   - User command sent to server
   - Includes: angles, movement, buttons, impulse

3. **Server Processing**
   ```cpp
   SVG_ClientThink(client, cmd) {
       ValidateCommand(cmd);       // Anti-cheat checks
       PlayerMove(client, cmd);    // Apply movement
       ProcessButtons(cmd);        // Handle fire, use, etc.
   }
   ```

## Entity System Architecture

Q2RTXPerimental uses an OOP entity hierarchy:

```
svg_base_edict_t                    (Base class for all entities)
├── svg_player_edict_t              (Player entities)
├── svg_item_edict_t                (Pickup items)
├── svg_pushmove_edict_t            (Moving brush entities)
│   ├── svg_func_door_t
│   ├── svg_func_plat_t
│   └── svg_func_train_t
├── svg_worldspawn_edict_t          (World entity)
└── (Many entity types...)
    ├── Triggers (trigger_hurt, trigger_teleport)
    ├── Monsters (monster_soldier, monster_tank)
    ├── Misc (misc_teleporter, info_player_start)
    └── Targets (target_speaker, target_explosion)
```

**Base Class Features:**
- Virtual callback methods (Spawn, Think, Touch, Use, etc.)
- Entity state (`entity_state_t s`) for networking
- Save/load system with automatic serialization
- Signal I/O system for entity communication
- UseTargets system for interactive entities
- Entity dictionary for spawn parameters

**See:** [Entity System Overview](Entity-System-Overview.md)

## Key Enhancements Over Vanilla

### 1. Object-Oriented Entity System

**Vanilla:**
```c
edict_t *ent = G_Spawn();
ent->classname = "monster_soldier";
ent->think = soldier_think;
ent->touch = soldier_touch;
```

**Q2RTXPerimental:**
```cpp
auto *soldier = SVG_Spawn<svg_monster_soldier_t>();
soldier->Spawn();  // Calls virtual method
// Think, Touch, etc. are virtual methods
```

Benefits: Type safety, inheritance, virtual dispatch, less boilerplate

### 2. Enhanced Networking

- **Higher Tick Rate**: 40 Hz (vs. 20 Hz)
- **Improved Delta Compression**: More efficient bandwidth usage
- **Enhanced Prediction**: Better client-side prediction accuracy

### 3. Lua Scripting

Server-side Lua integration for game logic:

```lua
-- Define custom entity behavior in Lua
function OnEntitySpawn(entity)
    entity:SetHealth(100)
    entity:SetModel("models/custom.iqm")
end

function OnEntityThink(entity)
    -- Custom AI logic
end
```

**See:** [Lua Scripting](Lua-Scripting.md)

### 4. Skeletal Animation System

Support for skeletal (SKM) models with:
- Bone-based animation
- Animation blending
- Root motion (animation-driven movement)
- IQM format support

**See:** [Skeletal Animation](Skeletal-Animation.md), [Root Motion](Root-Motion-System.md)

### 5. Signal I/O System

Entity communication through signals:

```cpp
// Entity A emits signal
EmitSignal("door_opened", this);

// Entity B listens for signal
RegisterSignalHandler("door_opened", &MyEntity::OnDoorOpened);
```

**See:** [Signal I/O System](Signal-IO-System.md)

### 6. UseTargets System

Enhanced player interaction with entities:

```cpp
class svg_func_button_t : public svg_base_edict_t {
    UseTargetHint GetUseTargetHint() override {
        return UseTargetHint::PRESSABLE;  // Shows "Press E" hint
    }
    
    void OnUseTargetPressed(svg_base_edict_t *user) override {
        // Handle button press
    }
};
```

**See:** [UseTargets System](UseTargets-System.md)

## Performance Considerations

### Entity Pooling

Entities are allocated from a fixed pool:
- Fast allocation/deallocation (no malloc/free)
- Cache-friendly (contiguous memory)
- Deterministic performance

### Think Optimization

Not all entities think every frame:
- Entities set `nextThinkTime` for when they need to think
- Engine only calls think on entities that need it
- Reduces CPU load for idle entities

### Networking Optimization

- PVS culling: Only send entities visible to player
- Delta compression: Only send changed fields
- Priority system: Important entities updated more frequently

### Spatial Partitioning

- Area grid for fast collision queries
- BSP tree for visibility determination
- Frustum culling for rendering

## Development Workflow

### 1. Creating a New Entity

1. Create header/cpp in `src/baseq2rtxp/svgame/entities/`
2. Derive from `svg_base_edict_t`
3. Implement virtual methods (Spawn, Think, etc.)
4. Register with spawn system using `DECLARE_EDICT_SPAWN_INFO`
5. Add to CMakeLists.txt

### 2. Adding Client-Side Effects

1. Create effect handler in `src/baseq2rtxp/clgame/effects/`
2. Register with temp entity system or entity event handler
3. Implement rendering/particle logic

### 3. Sharing Data Between Client/Server

1. Add to `src/baseq2rtxp/sharedgame/`
2. Use `#ifdef SERVER` / `#ifdef CLIENT` if needed
3. Ensure both modules can access the code

### 4. Hot Reloading

During development:
```
+set developer 1
# Make code changes
# Rebuild
game_restart  # Reload game DLLs without restarting engine
```

## Thread Safety

Q2RTXPerimental game modules are **single-threaded**:
- Game logic runs on main thread
- No synchronization needed for game code
- Engine may use threads for rendering, but game modules don't see this

**Important:** Don't use threads in game module code without careful synchronization.

## Next Steps

Now that you understand the architecture, dive deeper into specific modules:

- [Server Game Module](Server-Game-Module.md) - Detailed svgame documentation
- [Client Game Module](Client-Game-Module.md) - Detailed clgame documentation
- [Shared Game Module](Shared-Game-Module.md) - Detailed sharedgame documentation
- [Entity System Overview](Entity-System-Overview.md) - Entity architecture in depth
- [Engine Integration](Engine-Integration.md) - How game modules communicate with engine

---

**Previous:** [Vanilla Game Module Architecture](Vanilla-Game-Module-Architecture.md)
