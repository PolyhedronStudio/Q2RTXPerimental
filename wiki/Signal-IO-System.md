# Signal I/O System

Advanced entity communication system for Q2RTXPerimental that extends beyond basic target/targetname relationships.

**Location:** `src/baseq2rtxp/svgame/svg_signalio.h` and `.cpp`

## Overview

Signal I/O provides **named signals with typed arguments** for entity communication. It's more flexible than UseTargets:

**UseTargets:** Simple on/off activation
**Signal I/O:** Send named signals with data (numbers, strings, booleans)

## Basic Concept

```
[Sender]                  [Receiver]
SendSignalOut("OnDamage", [Receiver]
  damage=50)       ----->  OnSignalIn("OnDamage", args)
                           {
                               damage = args["damage"];
                               // React to damage
                           }
```

## Sending Signals

### Simple Signal (No Arguments)

```cpp
void svg_trigger_multiple_t::Touch(svg_base_edict_t *other, /*...*/) {
    // Send signal with no arguments
    svg_signal_argument_array_t args;  // Empty
    SendSignalOut("OnTrigger", other, args);
}
```

### Signal with Arguments

```cpp
void svg_func_button_t::Use(/*...*/) {
    // Create arguments
    svg_signal_argument_array_t args;
    
    // Add integer argument
    svg_signal_argument_t arg1;
    arg1.type = SIGNAL_ARGUMENT_TYPE_NUMBER;
    arg1.key = "press_count";
    arg1.value.integer = press_count++;
    args.push_back(arg1);
    
    // Add boolean argument
    svg_signal_argument_t arg2;
    arg2.type = SIGNAL_ARGUMENT_TYPE_BOOLEAN;
    arg2.key = "is_locked";
    arg2.value.boolean = is_locked;
    args.push_back(arg2);
    
    // Send signal
    SendSignalOut("OnPressed", activator, args);
}
```

### Helper Macros (Simplified)

```cpp
void svg_monster_soldier_t::Die(/*...*/) {
    // Create arguments array
    svg_signal_argument_array_t args;
    
    // Add damage value
    svg_signal_argument_t dmg_arg;
    dmg_arg.type = SIGNAL_ARGUMENT_TYPE_NUMBER;
    dmg_arg.key = "damage";
    dmg_arg.value.integer = damage;
    args.push_back(dmg_arg);
    
    // Add attacker name
    svg_signal_argument_t name_arg;
    name_arg.type = SIGNAL_ARGUMENT_TYPE_STRING;
    name_arg.key = "killer";
    name_arg.value.str = attacker->classname;
    args.push_back(name_arg);
    
    // Send death signal
    SendSignalOut("OnDeath", attacker, args);
}
```

## Receiving Signals

### Implement OnSignalIn Callback

```cpp
void svg_custom_entity_t::OnSignalIn(svg_base_edict_t *other,
                                      svg_base_edict_t *activator,
                                      const char *signalName,
                                      const svg_signal_argument_array_t &args) {
    
    if (!Q_strcasecmp(signalName, "OnDamage")) {
        // Get damage from arguments
        int damage = SVG_SignalArguments_GetValue<int>(args, "damage", 0);
        
        // React to damage
        health -= damage;
        gi.dprintf("Entity took %d damage! Health: %d\n", damage, health);
    }
    else if (!Q_strcasecmp(signalName, "OnActivate")) {
        // Handle activation signal
        Activate();
    }
}
```

### Getting Argument Values

```cpp
// Get integer
int value = SVG_SignalArguments_GetValue<int>(args, "count", 0);

// Get float/double
double speed = SVG_SignalArguments_GetValue<double>(args, "speed", 1.0);

// Get string
const char *name = SVG_SignalArguments_GetValue<const char*>(args, "name", "default");

// Get boolean
bool enabled = SVG_SignalArguments_GetValue<bool>(args, "enabled", false);
```

## Argument Types

```cpp
typedef enum svg_signal_argument_type_e {
    SIGNAL_ARGUMENT_TYPE_NONE,      // Not set
    SIGNAL_ARGUMENT_TYPE_BOOLEAN,   // true/false
    SIGNAL_ARGUMENT_TYPE_NUMBER,    // Integer or double
    SIGNAL_ARGUMENT_TYPE_STRING,    // const char*
    SIGNAL_ARGUMENT_TYPE_NULLPTR,   // Null pointer
} svg_signal_argument_type_t;
```

## Practical Examples

### Damage Notification System

**Damageable entity:**
```cpp
void svg_breakable_t::Pain(svg_base_edict_t *attacker, /*...*/) {
    // Send damage signal
    svg_signal_argument_array_t args;
    
    svg_signal_argument_t arg;
    arg.type = SIGNAL_ARGUMENT_TYPE_NUMBER;
    arg.key = "remaining_health";
    arg.value.integer = health;
    args.push_back(arg);
    
    SendSignalOut("OnDamaged", attacker, args);
}
```

**Health bar entity:**
```cpp
void svg_hud_healthbar_t::OnSignalIn(/*...*/, const char *signalName,
                                      const svg_signal_argument_array_t &args) {
    if (!Q_strcasecmp(signalName, "OnDamaged")) {
        int health = SVG_SignalArguments_GetValue<int>(args, "remaining_health", 100);
        UpdateHealthBar(health);  // Update visual display
    }
}
```

### Counter System

**Trigger counter:**
```cpp
class svg_trigger_counter_t : public svg_trigger_multiple_t {
    int count = 5;
    
    void Touch(svg_base_edict_t *other, /*...*/) override {
        count--;
        
        // Send count update
        svg_signal_argument_array_t args;
        svg_signal_argument_t arg;
        arg.type = SIGNAL_ARGUMENT_TYPE_NUMBER;
        arg.key = "remaining";
        arg.value.integer = count;
        args.push_back(arg);
        
        SendSignalOut("OnCountChanged", other, args);
        
        if (count <= 0) {
            SendSignalOut("OnCountReachedZero", other, {});
        }
    }
};
```

**Display entity:**
```cpp
void svg_display_counter_t::OnSignalIn(/*...*/, const char *signalName,
                                        const svg_signal_argument_array_t &args) {
    if (!Q_strcasecmp(signalName, "OnCountChanged")) {
        int remaining = SVG_SignalArguments_GetValue<int>(args, "remaining", 0);
        gi.centerprintf(nullptr, "%d remaining\n", remaining);
    }
    else if (!Q_strcasecmp(signalName, "OnCountReachedZero")) {
        gi.centerprintf(nullptr, "Counter reached zero!\n");
        UseTargets(nullptr, nullptr);  // Activate targets
    }
}
```

### Wave Spawner

**Spawner:**
```cpp
void svg_wave_spawner_t::SpawnWave() {
    current_wave++;
    
    // Spawn monsters...
    
    // Announce wave
    svg_signal_argument_array_t args;
    
    svg_signal_argument_t wave_arg;
    wave_arg.type = SIGNAL_ARGUMENT_TYPE_NUMBER;
    wave_arg.key = "wave_number";
    wave_arg.value.integer = current_wave;
    args.push_back(wave_arg);
    
    svg_signal_argument_t count_arg;
    count_arg.type = SIGNAL_ARGUMENT_TYPE_NUMBER;
    count_arg.key = "monster_count";
    count_arg.value.integer = monsters_spawned;
    args.push_back(count_arg);
    
    SendSignalOut("OnWaveStart", this, args);
}
```

**HUD receiver:**
```cpp
void svg_hud_waves_t::OnSignalIn(/*...*/, const char *signalName,
                                  const svg_signal_argument_array_t &args) {
    if (!Q_strcasecmp(signalName, "OnWaveStart")) {
        int wave = SVG_SignalArguments_GetValue<int>(args, "wave_number", 1);
        int count = SVG_SignalArguments_GetValue<int>(args, "monster_count", 0);
        
        gi.bprintf(PRINT_HIGH, "Wave %d: %d monsters!\n", wave, count);
    }
}
```

## Signal I/O vs. UseTargets

### When to Use UseTargets

✅ Simple on/off activation
✅ Map editor-friendly (target/targetname)
✅ Button presses, door triggers
✅ Standard entity activation

### When to Use Signal I/O

✅ Passing data between entities
✅ Complex state communication
✅ Game logic systems (counters, waves, etc.)
✅ Runtime entity relationships
✅ Lua scripting integration

## Map Editor Integration

Signal I/O can be set up in maps using entity keys:

```
{
"classname" "custom_counter"
"targetname" "counter1"
"signal_target" "display1"
"signal_name" "OnCountChanged"
}
```

**Code:**
```cpp
void svg_custom_counter_t::PostSpawn() override {
    // Get signal target from map
    const char *sig_target = ED_GetString(entityDictionary, "signal_target");
    const char *sig_name = ED_GetString(entityDictionary, "signal_name");
    
    if (sig_target && sig_name) {
        // Store for later use
        signal_target_name = sig_target;
        signal_name = sig_name;
    }
}

void svg_custom_counter_t::SendUpdate() {
    if (!signal_target_name)
        return;
    
    // Find target entity
    svg_base_edict_t *target = SVG_Find(nullptr, FOFS(targetname), 
                                         signal_target_name);
    if (target) {
        svg_signal_argument_array_t args;
        // ... add arguments ...
        
        target->OnSignalIn(this, this, signal_name, args);
    }
}
```

## Best Practices

### Use Descriptive Signal Names

```cpp
// GOOD: Clear intent
SendSignalOut("OnDoorOpened", /*...*/);
SendSignalOut("OnPlayerKilled", /*...*/);
SendSignalOut("OnWaveCompleted", /*...*/);

// BAD: Vague names
SendSignalOut("Event1", /*...*/);
SendSignalOut("Thing", /*...*/);
```

### Validate Arguments

```cpp
void OnSignalIn(/*...*/, const char *signalName,
                const svg_signal_argument_array_t &args) {
    if (!Q_strcasecmp(signalName, "OnDamage")) {
        int damage = SVG_SignalArguments_GetValue<int>(args, "damage", 0);
        
        // Validate
        if (damage < 0) {
            gi.dprintf("WARNING: Negative damage value!\n");
            damage = 0;
        }
        
        ApplyDamage(damage);
    }
}
```

### Document Signals

```cpp
/**
 * Signals this entity can send:
 * - "OnActivated": Sent when entity activates
 * - "OnDeactivated": Sent when entity deactivates
 * - "OnDamaged": Sent when taking damage
 *   Arguments:
 *     - "damage" (int): Amount of damage taken
 *     - "attacker" (string): Attacker's classname
 */
class svg_custom_entity_t : public svg_base_edict_t {
    // ...
};
```

## Lua Scripting Integration

**Signal I/O is tightly integrated with Lua for map-specific gameplay.** This is one of the primary use cases for Signal I/O.

### Purpose of Lua + Signal I/O

**Lua in Q2RTXPerimental exists explicitly for maps**, allowing mappers to create dynamic, interactive experiences by coupling Lua scripts with the Signal I/O system. This enables:

- Map-specific puzzles and events
- Custom game modes per map
- Interactive objects with complex behavior
- Score tracking and state management

### Setting Up Lua Signal Handlers

In your map editor, give entities a `luaName` key:

```
{
"classname" "func_button"
"targetname" "puzzle_button"
"luaName" "PuzzleButton"
}
```

In your map script (`baseq2rtxp/maps/scripts/yourmap.lua`):

```lua
function PuzzleButton_OnSignalIn(self, signaller, activator, signalName, signalArguments)
    if signalName == "OnPressed" then
        -- Handle button press
        Game.Print(PrintLevel.NOTICE, "Puzzle button pressed!\n")
        
        -- Send signal to other entities
        local door = Game.GetEntityForTargetName("puzzle_door")
        Game.SignalOut(door, self, activator, "Unlock", {})
    end
    return true  -- Always return true
end
```

### Lua Signal Arguments

Access signal arguments in Lua callbacks:

```lua
function Target_OnSignalIn(self, signaller, activator, signalName, signalArguments)
    if signalName == "OnDamaged" then
        -- Arguments are accessed as table fields
        local damage = signalArguments.damage
        local attackerName = signalArguments.attacker
        
        Game.Print(PrintLevel.NOTICE, 
            "Target took " .. damage .. " damage from " .. attackerName .. "!\n")
        
        -- Update map state
        mapStates.targetRange.totalDamage = 
            mapStates.targetRange.totalDamage + damage
    end
    return true
end
```

### Complete Lua Example: Interactive Door System

**Map Entities:**
```
{
"classname" "func_button"
"targetname" "door_button"
"luaName" "SecurityButton"
"target" "security_door"
}

{
"classname" "func_door"
"targetname" "security_door"
"luaName" "SecurityDoor"
"speed" "100"
}
```

**Lua Script (yourmap.lua):**
```lua
----------------------------------------------------------------------
-- Map State
----------------------------------------------------------------------
mapStates = {
    securityDoor = {
        locked = true,
        accessCode = "1234",
        failedAttempts = 0
    }
}

----------------------------------------------------------------------
-- Button Signal Handler
----------------------------------------------------------------------
function SecurityButton_OnSignalIn(self, signaller, activator, signalName, signalArguments)
    if signalName == "OnPressed" then
        local doorEntity = Game.GetEntityForLuaName("SecurityDoor")
        
        if mapStates.securityDoor.locked then
            -- Door is locked
            Game.CenterPrint(activator, "Security door is LOCKED")
            
            mapStates.securityDoor.failedAttempts = 
                mapStates.securityDoor.failedAttempts + 1
            
            if mapStates.securityDoor.failedAttempts >= 3 then
                -- Trigger alarm
                Game.SignalOut(
                    Game.GetEntityForTargetName("alarm_system"),
                    self, activator, "Trigger", 
                    { reason = "too_many_attempts" }
                )
            end
        else
            -- Door is unlocked, toggle it
            local doorState = Game.GetPushMoverState(doorEntity)
            
            if doorState == PushMoveState.BOTTOM then
                Game.UseTarget(doorEntity, self, activator, 
                             EntityUseTargetType.ON, 1)
                Game.Print(PrintLevel.NOTICE, "Security door opening...\n")
            elseif doorState == PushMoveState.TOP then
                Game.UseTarget(doorEntity, self, activator, 
                             EntityUseTargetType.OFF, 0)
                Game.Print(PrintLevel.NOTICE, "Security door closing...\n")
            end
        end
    end
    return true
end

----------------------------------------------------------------------
-- Door Signal Handler
----------------------------------------------------------------------
function SecurityDoor_OnSignalIn(self, signaller, activator, signalName, signalArguments)
    if signalName == "Unlock" then
        mapStates.securityDoor.locked = false
        mapStates.securityDoor.failedAttempts = 0
        
        -- Change door appearance
        self.state.skinnum = 1  -- Unlocked skin
        
        Game.Print(PrintLevel.NOTICE, "Security door UNLOCKED!\n")
        
        -- Play unlock sound
        Media.Sound(self, SoundChannel.VOICE, 
                   mapMedia.sound.door_unlock, 
                   1.0, SoundAttenuation.NORMAL, 0.0)
    elseif signalName == "Lock" then
        mapStates.securityDoor.locked = true
        self.state.skinnum = 0  -- Locked skin
        
        Game.Print(PrintLevel.NOTICE, "Security door LOCKED!\n")
    end
    return true
end

----------------------------------------------------------------------
-- Map Hooks
----------------------------------------------------------------------
mapMedia = {
    sound = {}
}

function OnPrecacheMedia()
    mapMedia.sound = {
        door_unlock = Media.PrecacheSound("doors/security_unlock.wav")
    }
    return true
end
```

### Sending Signals from Lua

**Send signals to entities:**
```lua
function Button_OnSignalIn(self, signaller, activator, signalName, signalArguments)
    if signalName == "OnPressed" then
        -- Get target entity
        local targetEntity = Game.GetEntityForTargetName("my_target")
        
        -- Send signal with arguments
        Game.SignalOut(
            targetEntity,           -- Target entity
            self,                   -- Sender
            activator,              -- Activator
            "CustomSignal",         -- Signal name
            {                       -- Arguments table
                value = 42,
                message = "Hello",
                enabled = true
            }
        )
    end
    return true
end
```

### C++ Entity Signals → Lua

C++ entities can send signals that Lua receives:

**C++ Code:**
```cpp
void svg_func_door_t::OnFullyOpened() {
    // Send signal that Lua can receive
    svg_signal_argument_array_t args;
    SendSignalOut("OnOpened", this, args);
}
```

**Lua Handler:**
```lua
-- Set luaName="MyDoor" in map editor
function MyDoor_OnSignalIn(self, signaller, activator, signalName, signalArguments)
    if signalName == "OnOpened" then
        -- Door finished opening
        Game.Print(PrintLevel.NOTICE, "Door is now open!\n")
        
        -- Trigger next event
        local nextButton = Game.GetEntityForTargetName("button_next")
        Game.SignalOut(nextButton, self, activator, "Enable", {})
    end
    return true
end
```

### Real-World Example: Target Range with Scoring

See the complete example in [**Lua Scripting**](Lua-Scripting.md#complete-example-target-range) showing:

- Target entities sending "OnKilled" signals to Lua
- Lua tracking score and remaining targets
- Lua sending signals back to entities for visual feedback
- Map state persistence across save/load

### Advantages of Lua + Signal I/O

1. **No engine recompilation** - Mappers can add complex behavior
2. **Per-map customization** - Each map has its own script
3. **State persistence** - Map state saved/loaded automatically
4. **Bidirectional** - C++ ↔ Lua communication
5. **Data passing** - Not just on/off, but rich data

## Related Documentation

- [**Lua Scripting**](Lua-Scripting.md) - Complete Lua documentation and examples
- [**UseTargets System**](UseTargets-System.md) - Simple entity targeting
- [**Entity System Overview**](Entity-System-Overview.md) - Entity architecture
- [**Entity Base Class Reference**](Entity-Base-Class-Reference.md) - OnSignalIn callback

## Summary

Signal I/O provides advanced entity communication:

- **Named signals**: Descriptive signal names ("OnDamaged", "OnActivate")
- **Typed arguments**: Pass booleans, numbers, strings
- **Flexible**: Not limited to on/off like UseTargets
- **Runtime**: Create entity relationships dynamically
- **Lua friendly**: Easy integration with Lua scripts

Use Signal I/O for complex entity interactions that require passing data. For simple activation, UseTargets is sufficient and more map-editor friendly.
