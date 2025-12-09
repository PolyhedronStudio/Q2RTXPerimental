# Lua Scripting System

Map-specific scripting system for Q2RTXPerimental that enables dynamic gameplay interactions through Lua scripts.

**Location:** `src/baseq2rtxp/svgame/svg_lua.cpp` and `svg_lua.h`

## Overview

**Lua in Q2RTXPerimental is explicitly designed for MAP-SPECIFIC scripting.** It provides mappers with a powerful way to create dynamic, interactive maps without modifying the game's C++ code. Lua scripts are loaded per-map and can respond to entity signals, control entities, and manage map state.

### Key Design Principles

1. **Map-Centric**: Each map can have its own Lua script (`baseq2rtxp/maps/scripts/mapname.lua`)
2. **Signal I/O Integration**: Lua works with the Signal I/O system for entity communication
3. **State Persistence**: Map state variables are saved/loaded with savegames
4. **No Engine Modification Required**: Mappers can create complex interactions without C++ knowledge

## Map Script Structure

### Location and Naming

Map scripts must follow this naming convention:
```
baseq2rtxp/maps/scripts/[mapname].lua
```

For example, map `q2rtxp_dev_targetrange.bsp` uses script:
```
baseq2rtxp/maps/scripts/q2rtxp_dev_targetrange.lua
```

### Basic Script Template

```lua
----------------------------------------------------------------------
-- Map State Variables - Persisted across save/load
----------------------------------------------------------------------
mapStates = {
    customState = {
        counter = 0,
        isActive = false,
        highScore = 0
    }
}

----------------------------------------------------------------------
-- Map Media - Precached resources
----------------------------------------------------------------------
mapMedia = {
    sound = {}
}

----------------------------------------------------------------------
-- Map Callback Hooks
----------------------------------------------------------------------
function OnPrecacheMedia()
    -- Precache sounds, models, etc.
    mapMedia.sound.custom_sound = Media.PrecacheSound("sounds/custom.wav")
    return true
end

function OnBeginMap()
    -- Called when map starts
    return true
end

function OnExitMap()
    -- Called when map exits
    return true
end

----------------------------------------------------------------------
-- Client Callback Hooks
----------------------------------------------------------------------
function OnClientEnterLevel(clientEntity)
    -- Called when a player connects
    Core.DPrint("Player connected: " .. clientEntity.number .. "\n")
    return true
end

function OnClientExitLevel(clientEntity)
    -- Called when a player disconnects
    return true
end

----------------------------------------------------------------------
-- Frame Callback Hooks
----------------------------------------------------------------------
function OnBeginServerFrame()
    -- Called at start of each server frame
    return true
end

function OnEndServerFrame()
    -- Called at end of each server frame
    return true
end

function OnRunFrame(frameNumber)
    -- Called each frame with frame number
    return true
end
```

## Entity Signal I/O Integration

**This is the primary way Lua interacts with entities.** Entities with a `luaName` key in the map editor will have their signals routed to Lua callback functions.

### Setting Up Entity-Lua Bindings

In your map editor (e.g., TrenchBroom), add to an entity:
```
"luaName" "MyButton"
```

Then in your Lua script, define the callback:
```lua
function MyButton_OnSignalIn(self, signaller, activator, signalName, signalArguments)
    if signalName == "OnPressed" then
        Game.Print(PrintLevel.NOTICE, "Button was pressed!\n")
        -- Do something
    end
    return true
end
```

### Signal Callback Naming Convention

**Pattern:** `[luaName]_OnSignalIn`

The engine automatically calls `[luaName]_OnSignalIn` when an entity with that `luaName` receives a signal.

### Example: Button with Lua Response

**Map Entity:**
```
{
"classname" "func_button"
"targetname" "my_button"
"luaName" "DoorButton"
"target" "my_door"
}
```

**Lua Script:**
```lua
function DoorButton_OnSignalIn(self, signaller, activator, signalName, signalArguments)
    if signalName == "OnPressed" then
        -- Get the door entity
        local doorEntity = Game.GetEntityForTargetName("my_door")
        
        -- Check door state
        local doorState = Game.GetPushMoverState(doorEntity)
        
        if doorState == PushMoveState.BOTTOM then
            -- Door is closed, open it
            Game.UseTarget(doorEntity, self, activator, EntityUseTargetType.ON, 1)
            Game.Print(PrintLevel.NOTICE, "Opening door...\n")
        elseif doorState == PushMoveState.TOP then
            -- Door is open, close it
            Game.UseTarget(doorEntity, self, activator, EntityUseTargetType.OFF, 0)
            Game.Print(PrintLevel.NOTICE, "Closing door...\n")
        else
            -- Door is moving
            Game.Print(PrintLevel.NOTICE, "Door is moving!\n")
        end
    end
    return true
end
```

### Signal Arguments

Signals can carry typed data:

```lua
function Target_OnSignalIn(self, signaller, activator, signalName, signalArguments)
    if signalName == "OnKilled" then
        -- Access argument by key
        local damage = signalArguments.damage
        local killerName = signalArguments.killer
        
        Game.Print(PrintLevel.NOTICE, 
            "Target killed by " .. killerName .. 
            " with " .. damage .. " damage!\n")
    end
    return true
end
```

## Lua API Reference

### Core Library

```lua
-- Print debug message
Core.DPrint("Debug message\n")

-- Get world time
local timeSeconds = Core.GetWorldTime(TimeType.SECONDS)
local timeFrames = Core.GetWorldTime(TimeType.FRAMES)
```

### Game Library

#### Entity Access

```lua
-- Get entity by targetname
local entity = Game.GetEntityForTargetName("door_main")

-- Get entity by luaName
local entity = Game.GetEntityForLuaName("MyDoor")

-- Get all entities with targetname (returns array)
local entities = Game.GetEntitiesForTargetName("light_torch")

-- Access entity state
entity.state.frame = 5
entity.state.effects = EntityEffects.ROTATE
```

#### Entity Control

```lua
-- Use target (activate entity)
Game.UseTarget(entity, sender, activator, EntityUseTargetType.ON, 1)

-- Use target with delay
-- (Uses Game.UseTargetDelay from utilities/entities.lua)

-- Send signal to entity
Game.SignalOut(entity, sender, activator, "CustomSignal", {
    damage = 100,
    type = "explosive"
})
```

#### Push Mover States

```lua
-- Get state of door/platform/train
local state = Game.GetPushMoverState(entity)

-- States:
-- PushMoveState.BOTTOM       -- At rest position
-- PushMoveState.TOP           -- At extended position
-- PushMoveState.MOVING_UP     -- Currently moving up
-- PushMoveState.MOVING_DOWN   -- Currently moving down
```

#### Player Information

```lua
-- Get player name
local name = Game.GetClientNameForEntity(playerEntity)

-- Print to chat
Game.Print(PrintLevel.NOTICE, "Message to all players\n")

-- Center print to specific player
Game.CenterPrint(playerEntity, "Message on screen")
```

### Media Library

```lua
-- Precache sound (in OnPrecacheMedia)
local soundIndex = Media.PrecacheSound("sounds/custom.wav")

-- Play sound
Media.Sound(
    entity,                      -- Entity to play from
    SoundChannel.VOICE,          -- Channel
    soundIndex,                  -- Sound index
    1.0,                        -- Volume (0.0-1.0)
    SoundAttenuation.NORMAL,    -- Attenuation
    0.0                         -- Time offset
)

-- Sound channels:
-- SoundChannel.AUTO
-- SoundChannel.WEAPON
-- SoundChannel.VOICE
-- SoundChannel.ITEM
-- SoundChannel.BODY

-- Sound attenuation:
-- SoundAttenuation.NONE     -- No falloff
-- SoundAttenuation.NORMAL   -- Normal falloff
-- SoundAttenuation.IDLE     -- Ambient sounds
-- SoundAttenuation.STATIC   -- Static sources
```

## Complete Example: Target Range

This example shows a complete interactive target range with score tracking:

```lua
----------------------------------------------------------------------
-- Map State
----------------------------------------------------------------------
mapStates = {
    targetRange = {
        highScore = 0,
        targetsAlive = 0,
        roundActive = false,
        timeStarted = 0
    }
}

mapMedia = {
    sound = {}
}

----------------------------------------------------------------------
-- Target Signal Handler
----------------------------------------------------------------------
function Target_OnSignalIn(self, signaller, activator, signalName, signalArguments)
    if signalName == "OnKilled" then
        -- Decrement targets alive
        mapStates.targetRange.targetsAlive = mapStates.targetRange.targetsAlive - 1
        
        -- Get killer name
        local killerName = Game.GetClientNameForEntity(activator)
        
        -- Check if round complete
        if mapStates.targetRange.targetsAlive <= 0 then
            -- Calculate time
            local timeTaken = Core.GetWorldTime(TimeType.SECONDS) - 
                            mapStates.targetRange.timeStarted
            
            -- Check for high score
            if timeTaken < mapStates.targetRange.highScore or 
               mapStates.targetRange.highScore == 0 then
                mapStates.targetRange.highScore = timeTaken
                Game.Print(PrintLevel.NOTICE, 
                    "New high score by " .. killerName .. ": " .. 
                    timeTaken .. " seconds!\n")
            end
            
            -- Round complete
            mapStates.targetRange.roundActive = false
        else
            -- Notify of kill
            Game.Print(PrintLevel.NOTICE, 
                killerName .. " killed a target! " .. 
                mapStates.targetRange.targetsAlive .. " remaining!\n")
        end
    end
    return true
end

----------------------------------------------------------------------
-- Start Button Signal Handler
----------------------------------------------------------------------
function StartButton_OnSignalIn(self, signaller, activator, signalName, signalArguments)
    if signalName == "OnPressed" then
        if mapStates.targetRange.roundActive then
            Game.Print(PrintLevel.NOTICE, "Round already active!\n")
        else
            -- Start new round
            mapStates.targetRange.targetsAlive = 4
            mapStates.targetRange.roundActive = true
            mapStates.targetRange.timeStarted = Core.GetWorldTime(TimeType.SECONDS)
            
            Game.Print(PrintLevel.NOTICE, "Round started! Kill all targets!\n")
            
            -- Reset all targets
            Game.SignalOut(Game.GetEntityForTargetName("target_1"), 
                         self, activator, "Reset", {})
            Game.SignalOut(Game.GetEntityForTargetName("target_2"), 
                         self, activator, "Reset", {})
            Game.SignalOut(Game.GetEntityForTargetName("target_3"), 
                         self, activator, "Reset", {})
            Game.SignalOut(Game.GetEntityForTargetName("target_4"), 
                         self, activator, "Reset", {})
        end
    end
    return true
end

----------------------------------------------------------------------
-- Map Hooks
----------------------------------------------------------------------
function OnPrecacheMedia()
    mapMedia.sound = {
        target_hit = Media.PrecacheSound("maps/targetrange/hit.wav"),
        round_start = Media.PrecacheSound("maps/targetrange/start.wav")
    }
    return true
end

function OnBeginMap()
    mapStates.targetRange.highScore = 0
    mapStates.targetRange.targetsAlive = 0
    mapStates.targetRange.roundActive = false
    return true
end
```

## Map State Persistence

**Map state is automatically saved/loaded with savegames.**

### State Variables

Define in `mapStates` table:
```lua
mapStates = {
    customState = {
        counter = 0,
        playerScores = {},
        puzzleSolved = false
    }
}
```

These variables persist across:
- Quick saves
- Manual saves
- Level transitions (for same map)

### What Gets Saved

- ✅ Numbers (integers, floats)
- ✅ Booleans
- ✅ Strings
- ✅ Tables (nested structures)
- ❌ Functions
- ❌ Entity references (use targetname strings instead)

## Utility Libraries

Create reusable code in `baseq2rtxp/maps/scripts/utilities/`:

### Example: entities.lua

```lua
-- utilities/entities.lua
local entities = {}

function entities:UseTargetDelay(entity, sender, activator, useType, useValue, delay)
    -- Create delayed use target
    -- Implementation details...
end

function entities:SignalOutDelay(entity, sender, activator, signalName, args, delay)
    -- Create delayed signal
    -- Implementation details...
end

return entities
```

### Using Utilities

```lua
local entities = require("utilities/entities")

function MyButton_OnSignalIn(self, signaller, activator, signalName, signalArguments)
    -- Use utility function
    entities:UseTargetDelay(
        Game.GetEntityForTargetName("door"), 
        self, activator, 
        EntityUseTargetType.ON, 1, 
        2.0  -- 2 second delay
    )
end
```

## Best Practices

### 1. Use luaName for Interactivity

Any entity that needs Lua behavior should have a `luaName`:
```
"luaName" "PuzzleDoor"
"targetname" "door_puzzle"
```

### 2. Keep State in mapStates

```lua
-- GOOD: Persistent state
mapStates.puzzle = {
    solved = false,
    attempts = 0
}

-- BAD: Local variables (not saved)
local puzzleSolved = false  -- Lost on save/load!
```

### 3. Return true from Callbacks

Always return `true` from callback functions:
```lua
function MyEntity_OnSignalIn(self, signaller, activator, signalName, signalArguments)
    -- Handle signal
    return true  -- Important!
end
```

### 4. Check Entity Validity

```lua
local entity = Game.GetEntityForTargetName("door")
if entity then
    -- Entity exists, use it
    Game.UseTarget(entity, self, activator, EntityUseTargetType.ON, 1)
else
    Core.DPrint("Error: door entity not found!\n")
end
```

### 5. Use Signal I/O Over Direct Control

```lua
-- GOOD: Use signals for communication
Game.SignalOut(targetEntity, self, activator, "Activate", {})

-- LESS GOOD: Direct state manipulation
targetEntity.state.frame = 5  -- Works but less flexible
```

## Debugging

### Enable Debug Output

In `svg_lua.h`:
```cpp
#define LUA_DEBUG_OUTPUT 1
```

### Debug Printing

```lua
Core.DPrint("Debug: Variable value = " .. tostring(value) .. "\n")
```

### Common Issues

**Signal not received:**
- Check entity has `luaName` key in map
- Verify callback function name: `[luaName]_OnSignalIn`
- Ensure function returns `true`

**State not persisting:**
- Variables must be in `mapStates` table
- Don't use local variables for persistent data

**Entity not found:**
- Verify `targetname` in map matches Lua code
- Check entity exists in map
- Ensure map is compiled correctly

## Integration with C++ Entities

Lua works alongside C++ entity classes:

1. **C++ entities** send signals (e.g., `func_door` sends "OnOpen", "OnClose")
2. **Lua scripts** receive signals via `[luaName]_OnSignalIn`
3. **Lua scripts** control entities via `Game.UseTarget()` and `Game.SignalOut()`

This separation allows:
- Complex entity behavior in C++ (doors, monsters, physics)
- Map-specific logic in Lua (puzzles, scores, events)
- No engine recompilation for map-specific features

## Related Documentation

- [**Signal I/O System**](Signal-IO-System.md) - Entity communication
- [**UseTargets System**](UseTargets-System.md) - Entity targeting
- [**Entity System Overview**](Entity-System-Overview.md) - Entity architecture

## Summary

Lua scripting in Q2RTXPerimental:

- **Purpose**: Map-specific interactive gameplay
- **Integration**: Works through Signal I/O system
- **Scope**: Per-map scripts with persistent state
- **API**: Access entities, send signals, play media
- **Workflow**: Map entities → Lua callbacks → Entity control

This system enables rich, interactive maps without modifying the game engine.
