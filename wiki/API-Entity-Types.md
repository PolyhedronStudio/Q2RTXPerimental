# API Reference - Entity Types

Entity types categorize entities for rendering, networking, and gameplay purposes. They are defined in `src/baseq2rtxp/sharedgame/sg_entity_types.h`.

## Overview

The `entityType` field of `entity_state_t` determines how the client renders and processes an entity. Different types have different rendering behaviors, collision prediction, and network priority.

## Entity Type Constants

### Engine-Defined Types

These are defined by the engine (in engine headers):

```cpp
// Basic engine types
ET_GENERAL              // Generic entity
ET_PLAYER               // Player entity (special networking)
ET_BEAM                 // Beam effect (lightning, grapple)
ET_MUZZLE_FLASH         // Weapon muzzle flash
```

### Game-Defined Types

**Location:** `src/baseq2rtxp/sharedgame/sg_entity_types.h`

```cpp
typedef enum sg_entity_type_e {
    // Player Types
    ET_PLAYER = ET_GAME_TYPES,      // Client player entity
    ET_PLAYER_CORPSE,                // Player corpse (ragdoll/gib)
    
    // Monster Types  
    ET_MONSTER,                      // NPC/Monster entity
    ET_MONSTER_CORPSE,               // Monster corpse
    
    // Other Types
    ET_PUSHER,                       // Moving brush entity (doors, platforms)
    ET_ITEM,                         // Pickup item
    
    // Effect Types
    ET_GIB,                          // Gib (body part)
    
    // Target Types
    ET_TARGET_SPEAKER,               // Speaker entity
    
    // Trigger Types
    ET_PUSH_TRIGGER,                 // Push trigger (created by pushers)
    ET_TELEPORT_TRIGGER,             // Teleporter trigger
    
    // Temporary Entity Events
    ET_TEMP_EVENT_ENTITY,            // Base for temp entity events
    
    ET_MAX_SHAREDGAME_TYPES          // Maximum types
} sg_entity_type_t;
```

## Entity Type Details

### ET_GENERAL

**Usage:** Generic entities that don't fit other categories

**Characteristics:**
- Default rendering behavior
- Standard networking priority
- Basic collision prediction

**Examples:**
- Decorative models
- Light sources
- Info entities

```cpp
void Spawn() {
    s.entityType = ET_GENERAL;
    gi.SetModel(edict, "models/objects/barrel/tris.md2");
}
```

### ET_PLAYER

**Usage:** Player-controlled characters

**Characteristics:**
- **High network priority** - Updated every frame
- Client-side prediction enabled
- Special animation handling
- View weapon rendering
- HUD updates

**Server-Side:**
```cpp
void svg_player_edict_t::Spawn() {
    s.entityType = ET_PLAYER;
    s.modelindex = gi.modelindex("players/male/tris.md2");
    // ... player setup ...
}
```

**Client-Side Effects:**
- Predicts movement locally
- Interpolates between server updates
- Renders view weapon
- Updates HUD

### ET_PLAYER_CORPSE

**Usage:** Dead player bodies

**Characteristics:**
- Lower network priority than live players
- No prediction
- Can be gibbed

```cpp
void svg_player_edict_t::Die(/*...*/) {
    s.entityType = ET_PLAYER_CORPSE;
    solid = SOLID_NOT;
    takedamage = DAMAGE_NO;
}
```

### ET_MONSTER

**Usage:** AI-controlled enemy entities

**Characteristics:**
- Interpolated on client
- Networked animations
- Can have client-side effects

```cpp
void svg_monster_soldier_t::Spawn() {
    s.entityType = ET_MONSTER;
    s.modelindex = gi.modelindex("models/monsters/soldier/tris.md2");
}
```

### ET_MONSTER_CORPSE

**Usage:** Dead monster bodies

**Characteristics:**
- Lower priority than live monsters
- Can fade out over time

```cpp
void svg_monster_soldier_t::Die(/*...*/) {
    s.entityType = ET_MONSTER_CORPSE;
    
    // Schedule removal
    SetThinkCallback(&svg_monster_soldier_t::RemoveCorpse);
    nextThinkTime = level.time + gametime_t::from_sec(30.0f);
}
```

### ET_PUSHER

**Usage:** Moving brush entities (doors, platforms, trains)

**Characteristics:**
- **Prediction on client** - Clients predict pusher movement
- Uses BSP models
- Can push other entities
- Creates push triggers for detection

```cpp
void svg_func_door_t::Spawn() {
    s.entityType = ET_PUSHER;
    solid = SOLID_BSP;
    movetype = MOVETYPE_PUSH;
    gi.SetModel(edict, model);
}
```

**Client Behavior:**
- Predicts door/platform position
- Smooth interpolation between states
- Handles client-side physics interaction

### ET_ITEM

**Usage:** Pickup items (weapons, ammo, health, armor)

**Characteristics:**
- Respawn effects
- Bobbing animation (client-side)
- Pickup detection

```cpp
void svg_item_health_t::Spawn() {
    s.entityType = ET_ITEM;
    s.modelindex = gi.modelindex(item->world_model);
    s.effects |= EF_ROTATE;  // Spinning effect
}
```

**Client Behavior:**
- Rotates/bobs automatically
- Spawns particles when collected
- Plays pickup sound

### ET_GIB

**Usage:** Giblets (body parts from explosions)

**Characteristics:**
- Physics-based (toss/bounce)
- Short lifetime
- Blood trail effect

```cpp
void SVG_ThrowGib(svg_base_edict_t *self, const char *gibname, int damage) {
    auto *gib = SVG_Spawn<svg_misc_gib_t>();
    gib->s.entityType = ET_GIB;
    gib->s.origin = self->s.origin;
    gib->velocity = RandomVector() * 300;
    gib->movetype = MOVETYPE_BOUNCE;
    gi.SetModel(gib->edict, gibname);
}
```

### ET_TARGET_SPEAKER

**Usage:** Ambient sound emitters

**Characteristics:**
- Plays looping or one-shot sounds
- Attenuation based on distance

```cpp
void svg_target_speaker_t::Spawn() {
    s.entityType = ET_TARGET_SPEAKER;
    s.sound = gi.soundindex(noise);
}
```

### ET_PUSH_TRIGGER / ET_TELEPORT_TRIGGER

**Usage:** Special trigger volumes

**Characteristics:**
- Created automatically by pusher/teleporter entities
- Invisible
- Solid trigger volume

```cpp
void svg_func_door_t::CreateTrigger() {
    auto *trigger = SVG_Spawn<svg_trigger_generic_t>();
    trigger->s.entityType = ET_PUSH_TRIGGER;
    trigger->solid = SOLID_TRIGGER;
    // ... setup trigger bounds ...
}
```

### ET_TEMP_EVENT_ENTITY

**Usage:** One-time visual effects

**Characteristics:**
- Not a persistent entity
- Used for temp entity events (impacts, explosions, etc.)
- Calculated as: `ET_TEMP_EVENT_ENTITY + eventNum`

```cpp
// This is handled automatically by temp entity system
// You typically don't set this manually
```

**See:** [Temporary Entity System](Temp-Entity-Overview.md)

## Setting Entity Types

### In Spawn Function

```cpp
void svg_example_entity_t::Spawn() {
    svg_base_edict_t::Spawn();
    
    // Set appropriate type
    s.entityType = ET_GENERAL;  // or ET_ITEM, ET_MONSTER, etc.
    
    // ... rest of spawn logic ...
}
```

### Changing At Runtime

```cpp
void svg_player_edict_t::Die(/*...*/) {
    // Change from living player to corpse
    s.entityType = ET_PLAYER_CORPSE;
    
    // Update other properties
    solid = SOLID_NOT;
    movetype = MOVETYPE_TOSS;
}
```

## Network Implications

### Update Priority

Entities are prioritized for networking based on type:

1. **High Priority**: `ET_PLAYER` - Updated every frame
2. **Medium Priority**: `ET_MONSTER`, `ET_PUSHER` - Updated frequently
3. **Low Priority**: `ET_ITEM`, `ET_GENERAL` - Updated when changed
4. **Lowest Priority**: `ET_PLAYER_CORPSE`, `ET_MONSTER_CORPSE` - Updated rarely

### Bandwidth Optimization

```cpp
// Use appropriate type to control update frequency
if (isImportant) {
    s.entityType = ET_MONSTER;  // Frequent updates
} else {
    s.entityType = ET_GENERAL;  // Only when changed
}
```

## Client-Side Behavior

### Prediction

Only certain types are predicted on the client:

- ✅ `ET_PLAYER` - Full prediction
- ✅ `ET_PUSHER` - Predicted movement
- ❌ `ET_MONSTER` - Interpolated only
- ❌ `ET_ITEM` - Interpolated only

### Interpolation

All entity types are interpolated between server snapshots:

```cpp
// Client-side (automatic)
void CLG_InterpolateEntities() {
    for (each entity) {
        // Lerp position between old and new snapshot
        entity.renderOrigin = Lerp(entity.prev_origin, entity.curr_origin, lerpFraction);
    }
}
```

### Special Rendering

```cpp
// Client determines rendering based on type
void CLG_AddEntity(centity_t *cent) {
    switch (cent->current.entityType) {
        case ET_PLAYER:
            CLG_AddPlayerEntity(cent);  // Special player rendering
            break;
            
        case ET_ITEM:
            CLG_AddItemEntity(cent);    // Bobbing, rotation
            break;
            
        case ET_PUSHER:
            CLG_AddBrushEntity(cent);   // BSP model
            break;
            
        default:
            CLG_AddGenericEntity(cent); // Standard model
            break;
    }
}
```

## Debugging

### Viewing Entity Types

```
// Console command to show entity info
ent_list

// Output:
// 0: worldspawn (ET_GENERAL)
// 1: player (ET_PLAYER)
// 2: monster_soldier (ET_MONSTER)
// 3: item_health (ET_ITEM)
```

### Debug Visualization

```cpp
#ifdef _DEBUG
const char* GetEntityTypeName(int type) {
    switch (type) {
        case ET_GENERAL: return "ET_GENERAL";
        case ET_PLAYER: return "ET_PLAYER";
        case ET_MONSTER: return "ET_MONSTER";
        case ET_ITEM: return "ET_ITEM";
        // ... all types ...
        default: return "ET_UNKNOWN";
    }
}

void DebugEntity(svg_base_edict_t *ent) {
    gi.dprintf("Entity %d: %s (type: %s)\n",
               ent->s.number,
               ent->classname,
               GetEntityTypeName(ent->s.entityType));
}
#endif
```

## Best Practices

### Choose Appropriate Type

```cpp
// GOOD: Use specific type for specific purpose
s.entityType = ET_ITEM;      // For pickup items
s.entityType = ET_MONSTER;   // For AI enemies
s.entityType = ET_PUSHER;    // For moving brushes

// BAD: Generic for everything
s.entityType = ET_GENERAL;   // Missing optimizations
```

### Update Type When State Changes

```cpp
void Respawn() {
    // Item respawning
    s.entityType = ET_ITEM;
    s.effects |= EF_ROTATE;
    solid = SOLID_TRIGGER;
}

void Collected() {
    // Item collected - becomes invisible temporarily
    s.entityType = ET_GENERAL;  // Lower priority
    s.effects = 0;
    solid = SOLID_NOT;
}
```

### Consistent With Model Type

```cpp
// GOOD: Type matches model
s.entityType = ET_PLAYER;
s.modelindex = gi.modelindex("players/male/tris.md2");

// BAD: Mismatch causes issues
s.entityType = ET_ITEM;
s.modelindex = gi.modelindex("players/male/tris.md2");  // Wrong!
```

## Related Documentation

- [Entity Flags](API-Entity-Flags.md) - Entity flag constants
- [Entity Events](API-Entity-Events.md) - Entity event system
- [Entity System Overview](Entity-System-Overview.md) - Entity architecture
- [Temp Entity System](Temp-Entity-Overview.md) - Temporary entities

---

**See Also:**
- `src/baseq2rtxp/sharedgame/sg_entity_types.h` - Entity type definitions
- `src/baseq2rtxp/clgame/clg_entities.cpp` - Client entity rendering
- `src/baseq2rtxp/svgame/svg_spawn.cpp` - Server entity spawning
