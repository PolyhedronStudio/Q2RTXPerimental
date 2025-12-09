# Entity Save/Load System

Persistence and serialization of entities in Q2RTXPerimental for savegames.

## Overview

The save/load system allows game state to be saved to disk and restored later. Entities must implement save/load methods to persist properly.

**Location:** `src/baseq2rtxp/svgame/svg_save.cpp` and `svg_save.h`

## Implementing Save/Load

### Declare Methods

```cpp
class svg_custom_entity_t : public svg_base_edict_t {
public:
    void Save(game_write_context_t *ctx) override;
    void Load(game_read_context_t *ctx) override;
    
private:
    int custom_data;
    float custom_float;
    vec3_t custom_vector;
};
```

### Implement Save

```cpp
void svg_custom_entity_t::Save(game_write_context_t *ctx) {
    // ALWAYS call parent Save() first
    svg_base_edict_t::Save(ctx);
    
    // Save custom data
    SVG_Save_Write(ctx, &custom_data, sizeof(custom_data));
    SVG_Save_Write(ctx, &custom_float, sizeof(custom_float));
    SVG_Save_Write(ctx, &custom_vector, sizeof(custom_vector));
}
```

### Implement Load

```cpp
void svg_custom_entity_t::Load(game_read_context_t *ctx) {
    // ALWAYS call parent Load() first
    svg_base_edict_t::Load(ctx);
    
    // Load custom data IN SAME ORDER AS SAVE
    SVG_Save_Read(ctx, &custom_data, sizeof(custom_data));
    SVG_Save_Read(ctx, &custom_float, sizeof(custom_float));
    SVG_Save_Read(ctx, &custom_vector, sizeof(custom_vector));
}
```

## Save Descriptors

For complex data structures, use save descriptors:

```cpp
// Define save descriptor
static const save_field_t custom_fields[] = {
    {"custom_data", FOFS(custom_data), F_INT},
    {"custom_float", FOFS(custom_float), F_FLOAT},
    {"custom_vector", FOFS(custom_vector), F_VECTOR},
    {NULL}
};

void svg_custom_entity_t::Save(game_write_context_t *ctx) {
    svg_base_edict_t::Save(ctx);
    SVG_Save_WriteFields(ctx, this, custom_fields);
}

void svg_custom_entity_t::Load(game_read_context_t *ctx) {
    svg_base_edict_t::Load(ctx);
    SVG_Save_ReadFields(ctx, this, custom_fields);
}
```

## Field Types

```cpp
F_INT           // Integer
F_FLOAT         // Float
F_VECTOR        // vec3_t
F_STRING        // const char* (allocated)
F_EDICT         // svg_base_edict_t* (pointer)
F_CLIENT        // svg_client_t* (pointer)
F_ITEM          // gitem_t* (pointer)
F_IGNORE        // Skip this field
```

## Pointers and References

### Saving Entity Pointers

```cpp
void Save(game_write_context_t *ctx) {
    svg_base_edict_t::Save(ctx);
    
    // Save entity pointer as entity number
    int target_num = target_entity ? target_entity->s.number : -1;
    SVG_Save_Write(ctx, &target_num, sizeof(target_num));
}

void Load(game_read_context_t *ctx) {
    svg_base_edict_t::Load(ctx);
    
    // Load entity number and resolve pointer
    int target_num;
    SVG_Save_Read(ctx, &target_num, sizeof(target_num));
    target_entity = (target_num >= 0) ? &g_edicts[target_num] : nullptr;
}
```

### Saving Strings

```cpp
void Save(game_write_context_t *ctx) {
    svg_base_edict_t::Save(ctx);
    
    // Strings are automatically handled
    SVG_Save_WriteString(ctx, custom_string);
}

void Load(game_read_context_t *ctx) {
    svg_base_edict_t::Load(ctx);
    
    // Load string (allocated automatically)
    custom_string = SVG_Save_ReadString(ctx);
}
```

## What to Save

### ✅ Save These

- Entity state (position, angles, etc.)
- Custom variables (counters, timers, etc.)
- Entity relationships (target, enemy, owner)
- Gameplay state (health, ammo, score)
- Persistent flags

### ❌ Don't Save These

- Temporary calculations
- Cached values (recalculate on load)
- Callback function pointers (reset in Spawn)
- Client-only rendering data
- Physics traces (redo on next frame)

## Callback Persistence

**Callbacks ARE automatically saved and restored** by the save descriptor system. Q2RTXPerimental uses a function pointer registration system that:

1. **During startup**: All callback functions are registered with unique IDs
2. **During save**: Function pointers are written with their type and ID
3. **During load**: Function pointers are looked up and restored automatically

The `svg_base_edict_t` save descriptor includes all callbacks:

```cpp
// From svg_base_edict.cpp - callbacks are automatically saved/loaded
SAVE_DESCRIPTOR_DEFINE_FUNCPTR(svg_base_edict_t, spawnCallbackFuncPtr, 
                               SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_SPAWN),
SAVE_DESCRIPTOR_DEFINE_FUNCPTR(svg_base_edict_t, thinkCallbackFuncPtr,
                               SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_THINK),
SAVE_DESCRIPTOR_DEFINE_FUNCPTR(svg_base_edict_t, touchCallbackFuncPtr,
                               SD_FIELD_TYPE_FUNCTION, FPTR_SAVE_TYPE_TOUCH),
// ... all other callbacks
```

**You do NOT need to manually restore callbacks** - they are preserved automatically when you call the parent `Load()` method:

```cpp
void svg_custom_entity_t::Load(game_read_context_t *ctx) {
    // This automatically restores all callbacks
    svg_base_edict_t::Load(ctx);
    
    // Load your custom data
    SVG_Save_Read(ctx, &custom_data, sizeof(custom_data));
    
    // Callbacks are already restored - no manual setup needed!
}
```

## Complete Example

```cpp
class svg_rotating_light_t : public svg_base_edict_t {
public:
    void Spawn() override {
        rotation_speed = 45.0f;  // degrees/sec
        light_on = true;
        SetThinkCallback(&svg_rotating_light_t::Think);
        nextthink = level.time + FRAMETIME;
    }
    
    void Think() override {
        s.angles[YAW] += rotation_speed * FRAMETIME;
        if (s.angles[YAW] >= 360.0f) {
            s.angles[YAW] -= 360.0f;
        }
        gi.linkentity(edict);
        nextthink = level.time + FRAMETIME;
    }
    
    void Save(game_write_context_t *ctx) override {
        svg_base_edict_t::Save(ctx);
        SVG_Save_Write(ctx, &rotation_speed, sizeof(rotation_speed));
        SVG_Save_Write(ctx, &light_on, sizeof(light_on));
        // Callbacks are automatically saved by parent Save()
    }
    
    void Load(game_read_context_t *ctx) override {
        svg_base_edict_t::Load(ctx);
        SVG_Save_Read(ctx, &rotation_speed, sizeof(rotation_speed));
        SVG_Save_Read(ctx, &light_on, sizeof(light_on));
        // Callbacks are automatically restored by parent Load()
        // No manual callback setup needed!
    }
    
private:
    float rotation_speed;
    bool light_on;
};
```

## Testing Saves

```
// In console
save test_save
load test_save
```

**Verify:**
- Entity still exists
- Position/state correct
- Callbacks working
- Custom data preserved
- No crashes

## Common Issues

### Save/Load Order Mismatch

```cpp
// WRONG:
void Save() {
    Write(a);
    Write(b);
}
void Load() {
    Read(b);  // Order doesn't match!
    Read(a);
}

// CORRECT:
void Save() {
    Write(a);
    Write(b);
}
void Load() {
    Read(a);  // Same order
    Read(b);
}
```

### Forgetting Parent Save/Load

```cpp
// WRONG:
void Save(game_write_context_t *ctx) {
    // Missing: svg_base_edict_t::Save(ctx);
    Write(custom_data);
}

// CORRECT:
void Save(game_write_context_t *ctx) {
    svg_base_edict_t::Save(ctx);  // MUST call parent!
    Write(custom_data);
}
```

### Saving Pointers Directly

```cpp
// WRONG:
void Save() {
    Write(&target_entity);  // Don't save pointer address!
}

// CORRECT:
void Save() {
    int num = target_entity ? target_entity->s.number : -1;
    Write(&num);  // Save entity number
}
```

## Related Documentation

- [**Entity System Overview**](Entity-System-Overview.md)
- [**Entity Lifecycle**](Entity-Lifecycle.md)
- [**Entity Base Class Reference**](Entity-Base-Class-Reference.md)
- [**Server Game Module**](Server-Game-Module.md)

## Summary

The save/load system preserves game state:

- **Override Save/Load** in custom entities
- **Call parent methods** first (this automatically saves/loads callbacks)
- **Match save/load order** exactly
- **Don't save pointers** directly (save entity numbers)
- **Callbacks are automatic** - no manual restoration needed
- **Test thoroughly** to ensure state is preserved

Proper save/load implementation ensures players can save their progress and resume seamlessly.
