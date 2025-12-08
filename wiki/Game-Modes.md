# Game Modes

Q2RTXPerimental features an object-oriented game mode system that allows for different gameplay styles and custom mode creation. The system provides three core modes (Singleplayer, Cooperative, Deathmatch) and a flexible framework for creating additional modes.

**Primary Source Files:**
- `src/baseq2rtxp/sharedgame/sg_gamemode.h` - Base game mode interfaces
- `src/baseq2rtxp/svgame/gamemodes/svg_gm_basemode.h` - Server game mode base class
- `src/baseq2rtxp/svgame/gamemodes/svg_gm_singleplayer.{h,cpp}` - Singleplayer implementation
- `src/baseq2rtxp/svgame/gamemodes/svg_gm_cooperative.{h,cpp}` - Cooperative implementation
- `src/baseq2rtxp/svgame/gamemodes/svg_gm_deathmatch.{h,cpp}` - Deathmatch implementation

## Architecture Overview

### Two-Tier Class Hierarchy

The game mode system uses a two-tier inheritance hierarchy:

**1. Shared Game Base (`sg_gamemode_base_t`)**

Defined in `src/baseq2rtxp/sharedgame/sg_gamemode.h`:

```cpp
struct sg_gamemode_base_t {
    virtual ~sg_gamemode_base_t() = default;
    
    // Core identification
    virtual const sg_gamemode_type_t GetGameModeType() const = 0;
    virtual const char *GetGameModeName() const = 0;
    virtual const char *GetGameModeDescription() const = 0;
};
```

This base provides identification only - no gameplay logic. Both client and server game modules can access this interface.

**2. Server Game Extension (`svg_gamemode_t`)**

Defined in `src/baseq2rtxp/svgame/gamemodes/svg_gm_basemode.h`:

```cpp
struct svg_gamemode_t : public sg_gamemode_base_t {
    virtual ~svg_gamemode_t() = default;
    
    // Mode properties
    virtual const bool IsMultiplayer() const = 0;
    virtual const bool AllowSaveGames() const = 0;
    
    // Lifecycle hooks
    virtual void PrepareCVars() = 0;
    
    // Client lifecycle
    virtual const bool ClientConnect(svg_player_edict_t *ent, char *userinfo) = 0;
    virtual void ClientDisconnect(svg_player_edict_t *ent) = 0;
    virtual void ClientUserinfoChanged(svg_player_edict_t *ent, char *userinfo) = 0;
    virtual void ClientSpawnInBody(svg_player_edict_t *ent) = 0;
    virtual void ClientBegin(svg_player_edict_t *ent) = 0;
    
    // Frame hooks
    virtual void PreCheckGameRuleConditions() = 0;
    virtual void PostCheckGameRuleConditions() = 0;
    virtual void BeginServerFrame(svg_player_edict_t *ent) = 0;
    virtual void EndServerFrame(svg_player_edict_t *ent) = 0;
    
    // Gameplay hooks
    virtual void DamageEntity(svg_base_edict_t *targ, svg_base_edict_t *inflictor, 
                             svg_base_edict_t *attacker, const Vector3 &dir, 
                             Vector3 &point, const Vector3 &normal, 
                             const int32_t damage, const int32_t knockBack, 
                             const entity_damageflags_t damageFlags, 
                             const sg_means_of_death_t meansOfDeath) = 0;
    virtual void EntityKilled(svg_base_edict_t *targ, svg_base_edict_t *inflictor, 
                             svg_base_edict_t *attacker, int32_t damage, Vector3 *point);
    virtual const bool CanDamageEntityDirectly(svg_base_edict_t *targ, svg_base_edict_t *inflictor);
    
    // Spawn system
    virtual svg_base_edict_t *SelectSpawnPoint(svg_player_edict_t *ent, Vector3 &origin, Vector3 &angles);
    virtual void ExitLevel();
};
```

### Game Mode Type Enumeration

From `src/baseq2rtxp/sharedgame/sg_gamemode.h`:

```cpp
typedef enum sg_gamemode_type_e {
    GAMEMODE_TYPE_UNKNOWN = -1,
    
    GAMEMODE_TYPE_SINGLEPLAYER = 0,  // Solo campaign
    GAMEMODE_TYPE_COOPERATIVE = 1,    // Co-op adventure
    GAMEMODE_TYPE_DEATHMATCH = 2,     // PvP deathmatch
    
    GAMEMODE_TYPE_MAX,  // Total count
} sg_gamemode_type_t;
```

### Virtual Method Dispatch

The game mode system uses C++ virtual functions for polymorphic behavior:

```cpp
// Global game mode instance (server game)
extern svg_gamemode_t *game.mode;

// Mode-specific behavior via virtual dispatch
if (game.mode->AllowSaveGames()) {
    // Save game...
}

// Check mode type
if (game.mode->GetGameModeType() == GAMEMODE_TYPE_DEATHMATCH) {
    // DM-specific logic
}
```

## Core Game Modes

### 1. Singleplayer Mode

**Implementation:** `src/baseq2rtxp/svgame/gamemodes/svg_gm_singleplayer.cpp`

**Characteristics:**
- **Player Count:** 1 player only (maxclients=1)
- **Save Games:** Allowed (`AllowSaveGames() returns true`)
- **Monsters:** Always spawn
- **Items:** Never respawn (one-time pickups)
- **Friendly Fire:** Disabled
- **Spawn Points:** `info_player_start`

**CVar Configuration:**

From `PrepareCVars()` (lines 56-64):

```cpp
void svg_gamemode_singleplayer_t::PrepareCVars() {
    gi.cvar_forceset("coop", "0");
    gi.cvar_forceset("deathmatch", "0");
    gi.cvar_forceset("maxclients", "1");
}
```

**Client Connection:**

From `ClientConnect()` (lines 78-103):

```cpp
const bool svg_gamemode_singleplayer_t::ClientConnect(svg_player_edict_t *ent, char *userinfo) {
    // No password check in singleplayer
    
    // Assign client structure
    ent->client = &game.clients[g_edict_pool.NumberForEdict(ent) - 1];
    
    // Check if this is a new game or loadgame
    if (ent->inUse == false) {
        // Clear respawning variables
        SVG_Player_InitRespawnData(ent->client);
        
        // Initialize persistent data (unless auto-saved)
        if (!game.autosaved || !ent->client->pers.weapon) {
            SVG_Player_InitPersistantData(ent, ent->client);
        }
    }
    
    gi.bprintf(PRINT_HIGH, "%s spawned in a singleplayer campaign!\n", 
               ent->client->pers.netname);
    
    return true;  // Always allow connection
}
```

**Save/Load Handling:**

Singleplayer checks `game.autosaved` flag to determine if player should keep equipment:

```cpp
if (!game.autosaved || !ent->client->pers.weapon) {
    // Fresh start - initialize from scratch
    SVG_Player_InitPersistantData(ent, ent->client);
} else {
    // Loadgame - persistent data already restored
}
```

**Use Cases:**
- Story-driven campaigns
- Games with save/load progression
- Solo gameplay without multiplayer complications

### 2. Cooperative Mode

**Implementation:** `src/baseq2rtxp/svgame/gamemodes/svg_gm_cooperative.cpp`

**Characteristics:**
- **Player Count:** 2-4 players (default 4, configurable)
- **Save Games:** Only on dedicated servers
- **Monsters:** Spawn for all players
- **Items:** Respawn after delays (20s weapons, 15s ammo, 10s health)
- **Friendly Fire:** Configurable via `dmflags`
- **Respawn:** Players respawn on death, keep equipment
- **Spawn Points:** `info_player_coop`

**CVar Configuration:**

From `PrepareCVars()` (lines 56-65):

```cpp
void svg_gamemode_cooperative_t::PrepareCVars() {
    gi.cvar_forceset("coop", "1");
    gi.cvar_forceset("deathmatch", "0");
    
    // Enforce player count limits
    if (maxclients->integer <= 1 || maxclients->integer > 4) {
        gi.cvar_forceset("maxclients", "4");
    }
}
```

**Save Game Restriction:**

From `AllowSaveGames()` (lines 47-50):

```cpp
const bool svg_gamemode_cooperative_t::AllowSaveGames() const {
    // Only allow saves on dedicated servers
    return (dedicated != nullptr && dedicated->value != 0 ? true : false);
}
```

**Password Protection:**

From `ClientConnect()` (lines 83-121):

```cpp
const bool svg_gamemode_cooperative_t::ClientConnect(svg_player_edict_t *ent, char *userinfo) {
    // Check for spectator
    const char *value = Info_ValueForKey(userinfo, "spectator");
    if (*value && strcmp(value, "0")) {
        // Spectator password check
        if (*spectator_password->string &&
            strcmp(spectator_password->string, "none") &&
            strcmp(spectator_password->string, value)) {
            Info_SetValueForKey(userinfo, "rejmsg", 
                               "Spectator password required or incorrect.");
            return false;  // Refuse connection
        }
        
        // Count existing spectators
        int32_t numspec = 0;
        for (int32_t i = 0; i < maxclients->value; i++) {
            svg_base_edict_t *ed = g_edict_pool.EdictForNumber(i + 1);
            if (ed != nullptr && ed->inUse && ed->client->pers.spectator) {
                numspec++;
            }
        }
        
        // Check spectator limit
        if (numspec >= maxspectators->value) {
            Info_SetValueForKey(userinfo, "rejmsg", 
                               "Server spectator limit is full.");
            return false;
        }
    } else {
        // Regular player password check
        value = Info_ValueForKey(userinfo, "password");
        if (*password->string && strcmp(password->string, "none") &&
            strcmp(password->string, value)) {
            Info_SetValueForKey(userinfo, "rejmsg", 
                               "Password required or incorrect.");
            return false;
        }
    }
    
    // Connection allowed - initialize client
    ent->client = &game.clients[g_edict_pool.NumberForEdict(ent) - 1];
    
    if (ent->inUse == false) {
        SVG_Player_InitRespawnData(ent->client);
        if (!game.autosaved || !ent->client->pers.weapon) {
            SVG_Player_InitPersistantData(ent, ent->client);
        }
    }
    
    if (game.maxclients >= 1) {
        gi.bprintf(PRINT_HIGH, "%s connected\n", ent->client->pers.netname);
    }
    
    return true;
}
```

**Use Cases:**
- Multiplayer campaigns
- Persistent adventure with friends
- Passworded private servers

### 3. Deathmatch Mode

**Implementation:** `src/baseq2rtxp/svgame/gamemodes/svg_gm_deathmatch.cpp`

**Characteristics:**
- **Player Count:** 2-CLIENTNUM_RESERVED players (default 8)
- **Save Games:** Disabled (`AllowSaveGames() returns false`)
- **Monsters:** Do not spawn (`SPAWNFLAG_NOT_DEATHMATCH`)
- **Items:** Fast respawn (5s weapons, 10s ammo, 60s powerups)
- **Friendly Fire:** Always enabled
- **Respawn:** 5 second delay, any button or forced respawn
- **Scoring:** Frag-based with fraglimit
- **Spawn Points:** `info_player_deathmatch` (random selection)

**CVar Configuration:**

From `PrepareCVars()` (lines 55-67):

```cpp
void svg_gamemode_deathmatch_t::PrepareCVars() {
    gi.cvar_forceset("deathmatch", "1");
    gi.cvar_forceset("coop", "0");
    
    // Enforce player count limits
    if (maxclients->integer <= 1) {
        gi.cvar_forceset("maxclients", "8");
    } else if (maxclients->integer > CLIENTNUM_RESERVED) {
        gi.cvar_forceset("maxclients", std::to_string(CLIENTNUM_RESERVED).c_str());
    }
}
```

**Respawn Logic:**

From `BeginServerFrame()` (lines 85-150):

```cpp
void svg_gamemode_deathmatch_t::BeginServerFrame(svg_player_edict_t *ent) {
    // Skip if in intermission
    if (level.intermissionFrameNumber) {
        return;
    }
    
    svg_client_t *client = ent->client;
    
    // Handle spectator switching (5 second delay)
    if (client->pers.spectator != client->resp.spectator && 
        (level.time - client->respawn_time) >= 5_sec) {
        SVG_Client_RespawnSpectator(ent);
        return;
    }
    
    // Update use target tracing
    if (client->useTarget.tracedFrameNumber < level.frameNumber && 
        !client->resp.spectator) {
        SVG_Player_TraceForUseTarget(ent, client, false);
    } else {
        client->useTarget.tracedFrameNumber = level.frameNumber;
    }
    
    // Run weapon logic if not done by ClientThink
    if (client->weapon_thunk == false && !client->resp.spectator) {
        SVG_Player_Weapon_Think(ent, false);
    } else {
        client->weapon_thunk = false;
    }
    
    // Handle respawning after death
    if (ent->lifeStatus != LIFESTATUS_ALIVE) {
        if (level.time > client->respawn_time) {
            const int32_t buttonMask = BUTTON_PRIMARY_FIRE;
            
            // Respawn on button press or forced respawn
            if ((client->latched_buttons & buttonMask) || 
                ((int)dmflags->value & DF_FORCE_RESPAWN)) {
                SVG_Client_RespawnPlayer(ent);
                client->latched_buttons = BUTTON_NONE;
            }
        }
        return;
    }
}
```

**Intermission Exit:**

From `PreCheckGameRuleConditions()` (lines 74-80):

```cpp
void svg_gamemode_deathmatch_t::PreCheckGameRuleConditions() {
    // Exit intermission when time expires
    if (level.exitintermission) {
        ExitLevel();
        return;
    }
}
```

**Use Cases:**
- Player vs player combat
- Competitive multiplayer
- Fast-paced action without progression

## Mode Selection

### Command-Line Arguments

**Singleplayer:**
```bash
./q2rtxp +map base1
# OR explicitly:
./q2rtxp +set deathmatch 0 +set coop 0 +map base1
```

**Cooperative:**
```bash
./q2rtxp +set coop 1 +set maxclients 4 +map base1
./q2rtxp +set coop 1 +set maxclients 2 +set password mypass +map base2
```

**Deathmatch:**
```bash
./q2rtxp +set deathmatch 1 +map q2dm1
./q2rtxp +set deathmatch 1 +set maxclients 16 +set fraglimit 50 +map q2dm2
```

### Console Commands

```
// Switch to cooperative (requires map restart)
set coop 1
set deathmatch 0
map base1

// Switch to deathmatch
set deathmatch 1
set coop 0
map q2dm1
```

### Relevant CVars

**Mode Selection:**
- `deathmatch` (0/1) - Enable deathmatch mode
- `coop` (0/1) - Enable cooperative mode
- If both 0: Singleplayer
- If both 1: Cooperative takes precedence

**Player Management:**
- `maxclients` (1-CLIENTNUM_RESERVED) - Maximum player count
- `password` (string) - Server password for players
- `spectator_password` (string) - Password for spectators
- `maxspectators` (int) - Maximum spectator count

**Gameplay:**
- `dmflags` (int) - Deathmatch behavior flags
  - `DF_FORCE_RESPAWN` - Auto-respawn without button press
  - More flags defined in shared headers
- `fraglimit` (int) - Frag limit for DM
- `timelimit` (float) - Time limit in minutes

### Code-Based Mode Detection

```cpp
// Get current mode type
sg_gamemode_type_t modeType = game.mode->GetGameModeType();

// Check specific mode
if (modeType == GAMEMODE_TYPE_SINGLEPLAYER) {
    // SP-specific logic
} else if (modeType == GAMEMODE_TYPE_COOPERATIVE) {
    // Coop-specific logic
} else if (modeType == GAMEMODE_TYPE_DEATHMATCH) {
    // DM-specific logic
}

// Check if multiplayer
if (game.mode->IsMultiplayer()) {
    // MP logic (coop or DM)
}

// Check if saving allowed
if (game.mode->AllowSaveGames()) {
    // Enable save menu
}
```

## Creating Custom Game Modes

### Step 1: Define Game Mode Type

Add to `src/baseq2rtxp/sharedgame/sg_gamemode.h`:

```cpp
typedef enum sg_gamemode_type_e {
    GAMEMODE_TYPE_UNKNOWN = -1,
    GAMEMODE_TYPE_SINGLEPLAYER = 0,
    GAMEMODE_TYPE_COOPERATIVE = 1,
    GAMEMODE_TYPE_DEATHMATCH = 2,
    GAMEMODE_TYPE_CTF = 3,  // NEW: Capture the Flag
    GAMEMODE_TYPE_MAX,
} sg_gamemode_type_t;
```

### Step 2: Create Mode Header

Create `src/baseq2rtxp/svgame/gamemodes/svg_gm_ctf.h`:

```cpp
#pragma once

#include "svgame/gamemodes/svg_gm_basemode.h"

// Team enumeration
typedef enum {
    TEAM_NONE = 0,
    TEAM_RED = 1,
    TEAM_BLUE = 2,
} ctf_team_t;

// Flag states
typedef enum {
    FLAG_AT_BASE = 0,      // At home base
    FLAG_TAKEN = 1,        // Carried by player
    FLAG_DROPPED = 2,      // Dropped on ground
} ctf_flag_state_t;

/**
 * @brief Capture the Flag game mode
 */
struct svg_gamemode_ctf_t : public svg_gamemode_t {
    virtual ~svg_gamemode_ctf_t() = default;
    
    // Mode identification
    virtual const sg_gamemode_type_t GetGameModeType() const override {
        return GAMEMODE_TYPE_CTF;
    }
    virtual const char *GetGameModeName() const override {
        return "Capture the Flag";
    }
    virtual const char *GetGameModeDescription() const override {
        return "Two teams compete to capture the enemy's flag";
    }
    
    // Mode properties
    virtual const bool IsMultiplayer() const override { return true; }
    virtual const bool AllowSaveGames() const override { return false; }
    
    // Lifecycle hooks
    virtual void PrepareCVars() override;
    
    // Client lifecycle
    virtual const bool ClientConnect(svg_player_edict_t *ent, char *userinfo) override;
    virtual void ClientDisconnect(svg_player_edict_t *ent) override;
    virtual void ClientUserinfoChanged(svg_player_edict_t *ent, char *userinfo) override;
    virtual void ClientSpawnInBody(svg_player_edict_t *ent) override;
    virtual void ClientBegin(svg_player_edict_t *ent) override;
    
    // Frame hooks
    virtual void PreCheckGameRuleConditions() override;
    virtual void PostCheckGameRuleConditions() override;
    virtual void BeginServerFrame(svg_player_edict_t *ent) override;
    virtual void EndServerFrame(svg_player_edict_t *ent) override;
    
    // Gameplay hooks
    virtual void DamageEntity(svg_base_edict_t *targ, svg_base_edict_t *inflictor, 
                             svg_base_edict_t *attacker, const Vector3 &dir, 
                             Vector3 &point, const Vector3 &normal, 
                             const int32_t damage, const int32_t knockBack, 
                             const entity_damageflags_t damageFlags, 
                             const sg_means_of_death_t meansOfDeath) override;
    virtual void EntityKilled(svg_base_edict_t *targ, svg_base_edict_t *inflictor, 
                             svg_base_edict_t *attacker, int32_t damage, Vector3 *point) override;
    
    // Spawn system
    virtual svg_base_edict_t *SelectSpawnPoint(svg_player_edict_t *ent, Vector3 &origin, Vector3 &angles) override;
    
    // CTF-specific methods
    void AssignTeam(svg_player_edict_t *ent);
    void CaptureFlag(svg_player_edict_t *player, svg_base_edict_t *flag);
    void ReturnFlag(ctf_team_t team);
    void CheckWinCondition();
    ctf_team_t GetPlayerTeam(svg_player_edict_t *ent);
    
private:
    int32_t redScore;
    int32_t blueScore;
    svg_base_edict_t *redFlag;
    svg_base_edict_t *blueFlag;
};
```

### Step 3: Implement Mode Logic

Create `src/baseq2rtxp/svgame/gamemodes/svg_gm_ctf.cpp`:

```cpp
#include "svgame/svg_local.h"
#include "svgame/gamemodes/svg_gm_ctf.h"

void svg_gamemode_ctf_t::PrepareCVars() {
    gi.cvar_forceset("deathmatch", "0");
    gi.cvar_forceset("coop", "0");
    gi.cvar_forceset("ctf", "1");  // Custom CVar
    
    // Default 16 players for CTF
    if (maxclients->integer <= 1) {
        gi.cvar_forceset("maxclients", "16");
    }
}

const bool svg_gamemode_ctf_t::ClientConnect(svg_player_edict_t *ent, char *userinfo) {
    // Password check (similar to cooperative)
    const char *value = Info_ValueForKey(userinfo, "password");
    if (*password->string && strcmp(password->string, "none") &&
        strcmp(password->string, value)) {
        Info_SetValueForKey(userinfo, "rejmsg", "Password required or incorrect.");
        return false;
    }
    
    // Assign client structure
    ent->client = &game.clients[g_edict_pool.NumberForEdict(ent) - 1];
    
    // Initialize client data
    if (ent->inUse == false) {
        SVG_Player_InitRespawnData(ent->client);
        SVG_Player_InitPersistantData(ent, ent->client);
    }
    
    // Assign team
    AssignTeam(ent);
    
    gi.bprintf(PRINT_HIGH, "%s joined team %s\n", 
               ent->client->pers.netname,
               GetPlayerTeam(ent) == TEAM_RED ? "RED" : "BLUE");
    
    return true;
}

void svg_gamemode_ctf_t::ClientSpawnInBody(svg_player_edict_t *ent) {
    // Spawn at team-specific spawn point
    Vector3 origin, angles;
    svg_base_edict_t *spot = SelectSpawnPoint(ent, origin, angles);
    
    // Set team skin
    ctf_team_t team = GetPlayerTeam(ent);
    if (team == TEAM_RED) {
        strcpy(ent->client->pers.skin, "ctf_r");
    } else {
        strcpy(ent->client->pers.skin, "ctf_b");
    }
    
    // Standard spawn logic
    SVG_Player_Respawn(ent);
}

svg_base_edict_t *svg_gamemode_ctf_t::SelectSpawnPoint(svg_player_edict_t *ent, 
                                                       Vector3 &origin, Vector3 &angles) {
    ctf_team_t team = GetPlayerTeam(ent);
    const char *spotName = (team == TEAM_RED) ? "info_player_team1" : "info_player_team2";
    
    // Find team spawn point
    svg_base_edict_t *spot = nullptr;
    while ((spot = SVG_Entity_Find(spot, FOFS(classname), spotName)) != nullptr) {
        if (SVG_Spawn_IsGoodSpotForPlayer(spot, ent)) {
            origin = spot->s.origin;
            angles = spot->s.angles;
            return spot;
        }
    }
    
    // Fallback to deathmatch spawns
    return SVG_SelectRandomDeathmatchSpawnPoint(origin, angles);
}

void svg_gamemode_ctf_t::AssignTeam(svg_player_edict_t *ent) {
    // Count players on each team
    int32_t redCount = 0, blueCount = 0;
    for (int32_t i = 0; i < maxclients->value; i++) {
        svg_player_edict_t *other = (svg_player_edict_t*)g_edict_pool.EdictForNumber(i + 1);
        if (!other || !other->inUse || other == ent) continue;
        
        if (GetPlayerTeam(other) == TEAM_RED) redCount++;
        else if (GetPlayerTeam(other) == TEAM_BLUE) blueCount++;
    }
    
    // Assign to smaller team
    ent->client->pers.team = (redCount <= blueCount) ? TEAM_RED : TEAM_BLUE;
}

ctf_team_t svg_gamemode_ctf_t::GetPlayerTeam(svg_player_edict_t *ent) {
    return (ctf_team_t)ent->client->pers.team;
}

void svg_gamemode_ctf_t::CaptureFlag(svg_player_edict_t *player, svg_base_edict_t *flag) {
    ctf_team_t team = GetPlayerTeam(player);
    
    // Award points to capturing team
    if (team == TEAM_RED) {
        redScore++;
        gi.bprintf(PRINT_HIGH, "%s captured the BLUE flag! RED: %d BLUE: %d\n",
                   player->client->pers.netname, redScore, blueScore);
    } else {
        blueScore++;
        gi.bprintf(PRINT_HIGH, "%s captured the RED flag! RED: %d BLUE: %d\n",
                   player->client->pers.netname, redScore, blueScore);
    }
    
    // Award frag to player
    player->client->resp.score++;
    
    // Return flag to base
    ReturnFlag((team == TEAM_RED) ? TEAM_BLUE : TEAM_RED);
    
    // Check win condition
    CheckWinCondition();
}

void svg_gamemode_ctf_t::ReturnFlag(ctf_team_t team) {
    svg_base_edict_t *flag = (team == TEAM_RED) ? redFlag : blueFlag;
    if (!flag) return;
    
    // Return flag to base spawn
    // (Implementation would reset flag entity to home position)
}

void svg_gamemode_ctf_t::CheckWinCondition() {
    int32_t capturelimit = gi.cvar("capturelimit", "8", 0)->value;
    
    if (redScore >= capturelimit || blueScore >= capturelimit) {
        // End match
        level.intermissionFrameNumber = level.frameNumber;
        level.exitintermission = true;
        
        const char *winner = (redScore > blueScore) ? "RED" : "BLUE";
        gi.bprintf(PRINT_HIGH, "Team %s wins! Final Score: RED: %d BLUE: %d\n",
                   winner, redScore, blueScore);
    }
}

void svg_gamemode_ctf_t::PostCheckGameRuleConditions() {
    // Check win condition each frame
    CheckWinCondition();
}

// Implement remaining virtual methods...
// (Similar to deathmatch for most functionality)
```

### Step 4: Register in Factory

Edit the game mode allocation function (typically in `svg_gamemode.cpp`):

```cpp
svg_gamemode_t *SVG_AllocateGameModeInstance(sg_gamemode_type_t modeType) {
    switch (modeType) {
        case GAMEMODE_TYPE_SINGLEPLAYER:
            return new svg_gamemode_singleplayer_t();
        case GAMEMODE_TYPE_COOPERATIVE:
            return new svg_gamemode_cooperative_t();
        case GAMEMODE_TYPE_DEATHMATCH:
            return new svg_gamemode_deathmatch_t();
        case GAMEMODE_TYPE_CTF:  // NEW
            return new svg_gamemode_ctf_t();
        default:
            return new svg_gamemode_singleplayer_t();  // Default fallback
    }
}
```

### Step 5: Add CVar Support

Update mode detection to check for `ctf` cvar:

```cpp
sg_gamemode_type_t SG_GetRequestedGameModeType() {
    if (gi.cvar("deathmatch", "0", 0)->value != 0) {
        return GAMEMODE_TYPE_DEATHMATCH;
    } else if (gi.cvar("coop", "0", 0)->value != 0) {
        return GAMEMODE_TYPE_COOPERATIVE;
    } else if (gi.cvar("ctf", "0", 0)->value != 0) {  // NEW
        return GAMEMODE_TYPE_CTF;
    } else {
        return GAMEMODE_TYPE_SINGLEPLAYER;
    }
}
```

## Mode-Specific Features

### Entity Spawning

Entities check spawn flags to determine if they should spawn in each mode:

```cpp
// From entity spawn logic
if ((ent->spawnflags & SPAWNFLAG_NOT_DEATHMATCH) && 
    game.mode->GetGameModeType() == GAMEMODE_TYPE_DEATHMATCH) {
    SVG_FreeEdict(ent);  // Don't spawn in deathmatch
    return;
}

if ((ent->spawnflags & SPAWNFLAG_NOT_COOP) && 
    game.mode->GetGameModeType() == GAMEMODE_TYPE_COOPERATIVE) {
    SVG_FreeEdict(ent);  // Don't spawn in coop
    return;
}

// Difficulty checks for SP/Coop
if (game.mode->GetGameModeType() != GAMEMODE_TYPE_DEATHMATCH) {
    if ((ent->spawnflags & SPAWNFLAG_NOT_EASY) && skill->value == 0) {
        SVG_FreeEdict(ent);
        return;
    }
    // Similar for MEDIUM, HARD, DEATHMATCH ONLY
}
```

### Item Respawn

Different modes have different item respawn times:

```cpp
float GetItemRespawnTime(gitem_t *item) {
    sg_gamemode_type_t mode = game.mode->GetGameModeType();
    
    if (mode == GAMEMODE_TYPE_SINGLEPLAYER) {
        return 0;  // Never respawn
    } else if (mode == GAMEMODE_TYPE_COOPERATIVE) {
        if (item->flags & IT_WEAPON) return 20.0f;
        if (item->flags & IT_AMMO) return 15.0f;
        if (item->flags & IT_HEALTH) return 10.0f;
        return 20.0f;
    } else if (mode == GAMEMODE_TYPE_DEATHMATCH) {
        if (item->flags & IT_WEAPON) return 5.0f;
        if (item->flags & IT_AMMO) return 10.0f;
        if (item->flags & IT_POWERUP) return 60.0f;
        return 5.0f;
    }
    
    return 0;
}
```

### Friendly Fire

Controlled by game mode and dmflags:

```cpp
bool CanDamageTeammate(svg_player_edict_t *attacker, svg_player_edict_t *target) {
    sg_gamemode_type_t mode = game.mode->GetGameModeType();
    
    if (mode == GAMEMODE_TYPE_DEATHMATCH) {
        return true;  // Always allow in DM
    } else if (mode == GAMEMODE_TYPE_COOPERATIVE) {
        // Check dmflags for friendly fire setting
        return ((int)dmflags->value & DF_FRIENDLY_FIRE) != 0;
    }
    
    return false;  // Singleplayer has no teammates
}
```

## Best Practices

### 1. Always Check Mode Type

Don't assume a mode - always check explicitly:

```cpp
// GOOD
if (game.mode->GetGameModeType() == GAMEMODE_TYPE_DEATHMATCH) {
    // DM logic
}

// BAD - assumes non-DM is singleplayer
if (game.mode->GetGameModeType() != GAMEMODE_TYPE_DEATHMATCH) {
    // Could be coop OR singleplayer!
}
```

### 2. Use Virtual Methods for Extensibility

Override virtual methods rather than checking mode types everywhere:

```cpp
// GOOD - override in each mode class
void svg_gamemode_deathmatch_t::EntityKilled(...) {
    // DM-specific death handling
    AwardFrags(attacker, target);
}

// BAD - mode checks everywhere
void EntityKilled(...) {
    if (game.mode->GetGameModeType() == GAMEMODE_TYPE_DEATHMATCH) {
        AwardFrags(attacker, target);
    }
    // Scattered logic
}
```

### 3. Initialize CVars in PrepareCVars

Set all mode-specific CVars in one place:

```cpp
void MyMode::PrepareCVars() {
    gi.cvar_forceset("mymode", "1");
    gi.cvar_forceset("maxclients", "8");
    // etc.
}
```

### 4. Validate Mode Type

Use the validation function:

```cpp
if (!SG_IsValidGameModeType(modeType)) {
    // Handle invalid mode
    modeType = GAMEMODE_TYPE_SINGLEPLAYER;
}
```

### 5. Handle Password Checks in ClientConnect

Implement password protection in ClientConnect, not elsewhere:

```cpp
const bool MyMode::ClientConnect(svg_player_edict_t *ent, char *userinfo) {
    // Password validation first
    const char *pass = Info_ValueForKey(userinfo, "password");
    if (*password->string && strcmp(password->string, pass)) {
        Info_SetValueForKey(userinfo, "rejmsg", "Incorrect password");
        return false;
    }
    
    // Then proceed with connection
    // ...
}
```

## Summary

The Q2RTXPerimental game mode system provides:

- **Object-oriented architecture** with virtual method dispatch
- **Three core modes** (SP, Coop, DM) with distinct behaviors
- **Extensible framework** for custom modes
- **Clean separation** between shared and server game logic
- **CVar-based** mode selection and configuration

**Key Virtual Methods:**
- `PrepareCVars()` - Initialize mode settings
- `ClientConnect()` - Handle connection, passwords, team assignment
- `ClientSpawnInBody()` - Spawn/respawn players
- `BeginServerFrame()` / `EndServerFrame()` - Per-frame logic
- `DamageEntity()` - Mode-specific damage handling
- `SelectSpawnPoint()` - Mode-specific spawn points

**Implementation Files:**
- `sg_gamemode.h` - Base interface (shared)
- `svg_gm_basemode.h` - Server game interface
- `svg_gm_{mode}.{h,cpp}` - Mode implementations

For advanced customization, derive from `svg_gamemode_t` and override the virtual methods to implement custom game rules, scoring systems, team management, and gameplay mechanics.
