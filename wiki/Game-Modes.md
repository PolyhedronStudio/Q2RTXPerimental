# Game Modes

Q2RTXPerimental supports multiple game modes with distinct rules, spawning behavior, and gameplay mechanics. Understanding the game mode system is essential for creating custom game modes or modifying existing ones.

**Primary Files:**
- `src/baseq2rtxp/sharedgame/sg_gamemode.h` - Game mode types and base interface
- `src/baseq2rtxp/sharedgame/sg_gamemode.cpp` - Shared game mode logic
- `src/baseq2rtxp/svgame/svg_gamemode.cpp` - Server-side game mode management
- `src/baseq2rtxp/svgame/gamemodes/` - Individual game mode implementations

## Overview

Q2RTXPerimental uses an **object-oriented game mode system** where each mode is a C++ class deriving from a base class. This allows for:

- **Mode-specific rules** - Each mode can override behavior
- **Clean separation** - Mode logic contained in dedicated classes
- **Easy extension** - Create new modes by deriving from base
- **Runtime selection** - Switch modes via command-line or console

### The Three Core Modes

```cpp
// Game mode types (sg_gamemode.h)
typedef enum sg_gamemode_type_e {
    GAMEMODE_TYPE_UNKNOWN = -1,
    
    GAMEMODE_TYPE_SINGLEPLAYER = 0,  // Solo adventure vs monsters
    GAMEMODE_TYPE_COOPERATIVE = 1,    // Co-op vs monsters
    GAMEMODE_TYPE_DEATHMATCH = 2,     // PvP combat
    
    GAMEMODE_TYPE_MAX,  // Total count
} sg_gamemode_type_t;
```

## Game Mode Architecture

### Base Class: sg_gamemode_base_t

All game modes inherit from this interface:

```cpp
// Base game mode interface (sharedgame/sg_gamemode.h)
struct sg_gamemode_base_t {
    virtual ~sg_gamemode_base_t() = default;
    
    // Mode identification
    virtual const sg_gamemode_type_t GetGameModeType() const = 0;
    virtual const char *GetGameModeName() const = 0;
    virtual const char *GetGameModeDescription() const = 0;
};
```

### Server Game Mode: svg_gamemode_t

Server-side extends the base with gameplay logic:

```cpp
// Server game mode class (svgame/gamemodes/svg_gm_basemode.h)
class svg_gamemode_t : public sg_gamemode_base_t {
public:
    virtual ~svg_gamemode_t() = default;
    
    // Mode initialization
    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
    
    // Spawn rules
    virtual bool AllowMonsterSpawn() const = 0;
    virtual bool AllowItemSpawn(gitem_t *item) const = 0;
    
    // Player management
    virtual void ClientBegin(svg_player_edict_t *player) = 0;
    virtual void ClientRespawn(svg_player_edict_t *player) = 0;
    virtual void ClientDeath(svg_player_edict_t *player, 
                            svg_base_edict_t *inflictor,
                            svg_base_edict_t *attacker) = 0;
    
    // Game rules
    virtual bool AllowSaveGames() const = 0;
    virtual bool AllowFriendlyFire() const = 0;
    virtual float GetItemRespawnTime(gitem_t *item) const = 0;
    
    // Scoring
    virtual void ScoreFrag(svg_player_edict_t *killer, 
                          svg_player_edict_t *victim) = 0;
};
```

## Singleplayer Mode

**Purpose:** Traditional solo Quake 2 campaign

**File:** `src/baseq2rtxp/svgame/gamemodes/svg_gm_singleplayer.h/.cpp`

### Key Features

```cpp
class svg_gamemode_singleplayer_t : public svg_gamemode_t {
public:
    const sg_gamemode_type_t GetGameModeType() const override {
        return GAMEMODE_TYPE_SINGLEPLAYER;
    }
    
    const char *GetGameModeName() const override {
        return "Singleplayer";
    }
    
    // Monsters always spawn
    bool AllowMonsterSpawn() const override {
        return true;
    }
    
    // Items never respawn (one-time pickups)
    float GetItemRespawnTime(gitem_t *item) const override {
        return 0.0f;  // Never respawn
    }
    
    // Allow game saving
    bool AllowSaveGames() const override {
        return true;
    }
    
    // No friendly fire (player can't damage self with explosions)
    bool AllowFriendlyFire() const override {
        return false;
    }
    
    // Spawn at info_player_start
    void ClientBegin(svg_player_edict_t *player) override {
        // Set up player for singleplayer
        player->client->pers.health = 100;
        player->client->pers.max_health = 100;
        
        // Give starting weapons
        InitClientPersistant(player->client);
        InitClientResp(player->client);
    }
};
```

### Singleplayer Rules

- **Monsters:** Always spawn (check `!(deathmatch->value)`)
- **Items:** Don't respawn after pickup
- **Death:** Loads last save or restarts level
- **Progression:** Linear level sequence via `target_changelevel`
- **Saving:** Allowed at any time
- **Difficulty:** `skill` cvar controls monster count/health

## Cooperative Mode

**Purpose:** Multiplayer adventure vs monsters

**File:** `src/baseq2rtxp/svgame/gamemodes/svg_gm_cooperative.h/.cpp`

### Key Features

```cpp
class svg_gamemode_cooperative_t : public svg_gamemode_t {
public:
    const sg_gamemode_type_t GetGameModeType() const override {
        return GAMEMODE_TYPE_COOPERATIVE;
    }
    
    const char *GetGameModeName() const override {
        return "Cooperative";
    }
    
    // Monsters spawn
    bool AllowMonsterSpawn() const override {
        return true;
    }
    
    // Items respawn after delay (for multiple players)
    float GetItemRespawnTime(gitem_t *item) const override {
        if (item->flags & IT_WEAPON)
            return 20.0f;  // Weapons respawn in 20 seconds
        if (item->flags & IT_AMMO)
            return 15.0f;  // Ammo respawns in 15 seconds
        if (item->flags & IT_HEALTH)
            return 20.0f;  // Health respawns in 20 seconds
        return 10.0f;  // Other items in 10 seconds
    }
    
    // Allow saving in coop (configurable)
    bool AllowSaveGames() const override {
        // Typically only on listen servers, not dedicated
        return !dedicated->value;
    }
    
    // Friendly fire controlled by cvar
    bool AllowFriendlyFire() const override {
        return dmflags->value & DF_NO_FRIENDLY_FIRE ? false : true;
    }
    
    // Respawn after death
    void ClientRespawn(svg_player_edict_t *player) override {
        // Respawn at coop spawn point
        SelectCoopSpawnPoint(player);
        
        // Keep weapons and ammo (don't lose progress)
        InitClientResp(player->client);
        PutClientInServer(player);
    }
    
    // Subtract frag for death (no score in coop)
    void ClientDeath(svg_player_edict_t *player,
                    svg_base_edict_t *inflictor,
                    svg_base_edict_t *attacker) override {
        player->client->pers.frags -= 1;  // Death penalty
    }
};
```

### Cooperative Rules

- **Monsters:** Spawn for all players to fight together
- **Items:** Respawn after time delay (shared between players)
- **Death:** Player respawns, keeps equipment
- **Spawn Points:** Use `info_player_coop` entities
- **Progression:** All players advance together through levels
- **Friendly Fire:** Configurable via `dmflags`
- **Saving:** Optional (usually disabled on dedicated servers)

## Deathmatch Mode

**Purpose:** Player vs player combat

**File:** `src/baseq2rtxp/svgame/gamemodes/svg_gm_deathmatch.h/.cpp`

### Key Features

```cpp
class svg_gamemode_deathmatch_t : public svg_gamemode_t {
public:
    const sg_gamemode_type_t GetGameModeType() const override {
        return GAMEMODE_TYPE_DEATHMATCH;
    }
    
    const char *GetGameModeName() const override {
        return "Deathmatch";
    }
    
    // No monsters in deathmatch
    bool AllowMonsterSpawn() const override {
        return false;
    }
    
    // Fast item respawn for constant action
    float GetItemRespawnTime(gitem_t *item) const override {
        if (item->flags & IT_WEAPON)
            return 5.0f;   // Weapons respawn quickly
        if (item->flags & IT_AMMO)
            return 10.0f;  // Ammo respawns fast
        if (item->flags & IT_POWERUP)
            return 60.0f;  // Powerups take longer (Quad Damage, etc.)
        return 15.0f;  // Default respawn
    }
    
    // No saving in deathmatch
    bool AllowSaveGames() const override {
        return false;
    }
    
    // Always allow friendly fire (PvP mode)
    bool AllowFriendlyFire() const override {
        return true;
    }
    
    // Spawn at random deathmatch point
    void ClientRespawn(svg_player_edict_t *player) override {
        // Random spawn to avoid camping
        SelectRandomDeathmatchSpawnPoint(player);
        
        // Reset to default loadout
        InitClientResp(player->client);
        PutClientInServer(player);
    }
    
    // Award frag to killer
    void ScoreFrag(svg_player_edict_t *killer,
                   svg_player_edict_t *victim) override {
        if (killer == victim) {
            // Suicide - subtract frag
            victim->client->pers.frags -= 1;
        } else {
            // Kill - award frag
            killer->client->pers.frags += 1;
        }
        
        // Check for frag limit
        if (fraglimit->value > 0 &&
            killer->client->pers.frags >= fraglimit->value) {
            gi.bprintf(PRINT_HIGH, "%s reached the frag limit.\n",
                      killer->client->pers.netname);
            EndDMLevel();  // End match
        }
    }
};
```

### Deathmatch Rules

- **Monsters:** Never spawn (use `SPAWNFLAG_NOT_DEATHMATCH`)
- **Items:** Fast respawn for continuous combat
- **Death:** Quick respawn at random point
- **Spawn Points:** Use `info_player_deathmatch` entities
- **Scoring:** Frags for kills, -1 for suicide
- **Limits:** `fraglimit` and `timelimit` end match
- **Friendly Fire:** Always enabled (players fight each other)
- **Saving:** Disabled (match-based gameplay)

## Game Mode Selection

### Command-Line

Select mode when starting server:

```bash
# Singleplayer
./q2rtxp +map base1

# Cooperative
./q2rtxp +set coop 1 +map base1

# Deathmatch
./q2rtxp +set deathmatch 1 +map q2dm1
```

### Console

Change mode at runtime (requires level restart):

```
// Enable deathmatch
set deathmatch 1
map q2dm1

// Enable cooperative
set coop 1
set deathmatch 0
map base1

// Return to singleplayer
set coop 0
set deathmatch 0
map base1
```

### Code

Check current mode:

```cpp
// Get current game mode object
svg_gamemode_t *mode = game.mode;

// Check mode type
if (mode->GetGameModeType() == GAMEMODE_TYPE_SINGLEPLAYER) {
    // Singleplayer logic
}

// Check if multiplayer
if (SG_IsMultiplayerGameMode(mode->GetGameModeType())) {
    // Multiplayer-specific code
}

// Check specific mode
if (deathmatch->value) {
    // Deathmatch only
}
if (coop->value) {
    // Cooperative only
}
```

## Creating Custom Game Modes

### Step 1: Define Mode Type

Add new mode to enum in `sharedgame/sg_gamemode.h`:

```cpp
typedef enum sg_gamemode_type_e {
    GAMEMODE_TYPE_SINGLEPLAYER = 0,
    GAMEMODE_TYPE_COOPERATIVE = 1,
    GAMEMODE_TYPE_DEATHMATCH = 2,
    GAMEMODE_TYPE_CTF = 3,  // NEW: Capture the Flag
    
    GAMEMODE_TYPE_MAX,
} sg_gamemode_type_t;
```

### Step 2: Create Mode Class

Create header `svgame/gamemodes/svg_gm_ctf.h`:

```cpp
#pragma once

#include "svg_gm_basemode.h"

class svg_gamemode_ctf_t : public svg_gamemode_t {
public:
    virtual ~svg_gamemode_ctf_t() = default;
    
    // Identification
    const sg_gamemode_type_t GetGameModeType() const override {
        return GAMEMODE_TYPE_CTF;
    }
    
    const char *GetGameModeName() const override {
        return "Capture the Flag";
    }
    
    const char *GetGameModeDescription() const override {
        return "Capture the enemy flag and return it to your base";
    }
    
    // Initialization
    void Initialize() override;
    void Shutdown() override;
    
    // Spawn rules
    bool AllowMonsterSpawn() const override { return false; }
    bool AllowItemSpawn(gitem_t *item) const override { return true; }
    
    // Player management
    void ClientBegin(svg_player_edict_t *player) override;
    void ClientRespawn(svg_player_edict_t *player) override;
    void ClientDeath(svg_player_edict_t *player,
                    svg_base_edict_t *inflictor,
                    svg_base_edict_t *attacker) override;
    
    // Game rules
    bool AllowSaveGames() const override { return false; }
    bool AllowFriendlyFire() const override { return false; }
    float GetItemRespawnTime(gitem_t *item) const override { return 10.0f; }
    
    // Scoring
    void ScoreFrag(svg_player_edict_t *killer,
                   svg_player_edict_t *victim) override;
    
    // CTF-specific
    void ScoreCapture(svg_player_edict_t *player, int team);
    void ReturnFlag(svg_player_edict_t *player, int team);
};
```

### Step 3: Implement Mode Logic

Create implementation `svgame/gamemodes/svg_gm_ctf.cpp`:

```cpp
#include "svg_gm_ctf.h"

void svg_gamemode_ctf_t::Initialize() {
    // Set up CTF-specific game state
    gi.dprintf("Initializing Capture the Flag mode\n");
    
    // Reset team scores
    game.ctf_red_score = 0;
    game.ctf_blue_score = 0;
}

void svg_gamemode_ctf_t::ClientBegin(svg_player_edict_t *player) {
    // Assign player to team
    int team = PickTeam(player);
    player->client->pers.team = team;
    
    // Set skin based on team
    if (team == TEAM_RED)
        player->client->pers.skin = "red";
    else
        player->client->pers.skin = "blue";
    
    // Give starting equipment
    InitClientResp(player->client);
}

void svg_gamemode_ctf_t::ScoreCapture(svg_player_edict_t *player, int team) {
    // Award points for flag capture
    if (team == TEAM_RED) {
        game.ctf_red_score++;
        gi.bprintf(PRINT_HIGH, "%s captured the blue flag!\n",
                  player->client->pers.netname);
    } else {
        game.ctf_blue_score++;
        gi.bprintf(PRINT_HIGH, "%s captured the red flag!\n",
                  player->client->pers.netname);
    }
    
    // Check for capture limit
    if (capturelimit->value > 0) {
        if (game.ctf_red_score >= capturelimit->value ||
            game.ctf_blue_score >= capturelimit->value) {
            EndCTFMatch();
        }
    }
}
```

### Step 4: Register in Factory

Update `svgame/svg_gamemode.cpp`:

```cpp
svg_gamemode_t *SVG_AllocateGameModeInstance(const sg_gamemode_type_t gameModeType) {
    switch (gameModeType) {
    case GAMEMODE_TYPE_SINGLEPLAYER:
        return new svg_gamemode_singleplayer_t();
    case GAMEMODE_TYPE_COOPERATIVE:
        return new svg_gamemode_cooperative_t();
    case GAMEMODE_TYPE_DEATHMATCH:
        return new svg_gamemode_deathmatch_t();
    case GAMEMODE_TYPE_CTF:  // NEW
        return new svg_gamemode_ctf_t();
    default:
        return nullptr;
    }
}
```

### Step 5: Add Console Variable

Add cvar for mode selection:

```cpp
// In SVG_InitGame()
cvar_t *ctf = gi.cvar("ctf", "0", CVAR_SERVERINFO);

// Set mode based on cvars
if (ctf->value)
    gameModeType = GAMEMODE_TYPE_CTF;
else if (deathmatch->value)
    gameModeType = GAMEMODE_TYPE_DEATHMATCH;
else if (coop->value)
    gameModeType = GAMEMODE_TYPE_COOPERATIVE;
else
    gameModeType = GAMEMODE_TYPE_SINGLEPLAYER;
```

## Mode-Specific Entity Spawning

Entities can spawn/not spawn based on mode:

```cpp
void SP_monster_soldier(svg_base_edict_t *self) {
    // Check spawn flags
    if (deathmatch->value) {
        // Monster has SPAWNFLAG_NOT_DEATHMATCH?
        if (self->spawnflags & SPAWNFLAG_NOT_DEATHMATCH) {
            SVG_FreeEdict(self);
            return;
        }
    }
    
    if (coop->value) {
        // Monster has SPAWNFLAG_NOT_COOP?
        if (self->spawnflags & SPAWNFLAG_NOT_COOP) {
            SVG_FreeEdict(self);
            return;
        }
    }
    
    // Spawn the monster
    self->Spawn();
}
```

### Spawn Flag Constants

```cpp
// Universal spawn flags (all entities)
#define SPAWNFLAG_NOT_EASY          0x00000100  // Don't spawn on easy
#define SPAWNFLAG_NOT_MEDIUM        0x00000200  // Don't spawn on medium
#define SPAWNFLAG_NOT_HARD          0x00000400  // Don't spawn on hard
#define SPAWNFLAG_NOT_DEATHMATCH    0x00000800  // Don't spawn in DM
#define SPAWNFLAG_NOT_COOP          0x00001000  // Don't spawn in coop
```

## Best Practices

### Mode-Aware Code

Always check mode before applying rules:

```cpp
// Check if monsters should spawn
if (game.mode->AllowMonsterSpawn()) {
    SpawnMonster(self);
}

// Check item respawn time
float respawn_time = game.mode->GetItemRespawnTime(item);
if (respawn_time > 0) {
    item->nextthink = level.time + respawn_time;
}

// Check if friendly fire allowed
if (game.mode->AllowFriendlyFire() || attacker != victim) {
    T_Damage(victim, inflictor, attacker, dir, point, 
             normal, damage, knockback, dflags, mod);
}
```

### Mode Transitions

Handle mode changes cleanly:

```cpp
void ChangeGameMode(sg_gamemode_type_t newMode) {
    // Shutdown current mode
    if (game.mode) {
        game.mode->Shutdown();
        delete game.mode;
    }
    
    // Allocate new mode
    game.mode = SVG_AllocateGameModeInstance(newMode);
    
    // Initialize new mode
    if (game.mode) {
        game.mode->Initialize();
    }
    
    // Restart level to apply new rules
    gi.AddCommandString("map_restart\n");
}
```

### Team Management (for team modes)

```cpp
int PickTeam(svg_player_edict_t *player) {
    // Count players per team
    int red_count = 0, blue_count = 0;
    
    for (int i = 1; i <= game.maxclients; i++) {
        svg_player_edict_t *ent = &g_edicts[i];
        if (!ent->inuse)
            continue;
        
        if (ent->client->pers.team == TEAM_RED)
            red_count++;
        else if (ent->client->pers.team == TEAM_BLUE)
            blue_count++;
    }
    
    // Assign to smaller team
    if (red_count < blue_count)
        return TEAM_RED;
    else if (blue_count < red_count)
        return TEAM_BLUE;
    else
        return (rand() % 2) ? TEAM_RED : TEAM_BLUE;
}
```

## Summary

Q2RTXPerimental's game mode system provides:

- **Object-oriented design** - Clean, extensible mode classes
- **Three core modes** - Singleplayer, Cooperative, Deathmatch
- **Customizable rules** - Per-mode spawn, respawn, scoring
- **Easy extension** - Create new modes by deriving from base
- **Mode-aware entities** - Spawn flags control mode-specific behavior
- **Runtime selection** - Switch modes via console/command-line

**Key Files:**
- `sharedgame/sg_gamemode.h` - Mode types and base class
- `svgame/gamemodes/svg_gm_basemode.h` - Server mode base
- `svgame/gamemodes/svg_gm_*.cpp` - Individual mode implementations
- `svgame/svg_gamemode.cpp` - Mode factory and management

**For more information**, see the Entity System documentation for spawn flag usage and level progression via `target_changelevel`.
