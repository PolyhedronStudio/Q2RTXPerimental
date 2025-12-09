# Skeletal Animation System

Q2RTXPerimental uses a skeletal animation system (SKM format) for complex animated models with hierarchical bone structures. This system provides smooth animation blending, bone manipulation, and root motion support.

**Primary Files:**
- `src/baseq2rtxp/sharedgame/sg_skm.h` - SKM format structures and API
- `src/baseq2rtxp/sharedgame/sg_skm.cpp` - Animation playback and blending
- `inc/shared/formats/format_skm.h` - SKM file format definitions

## Overview

**SKM (Skeletal Model)** is Quake 2's skeletal animation format, an enhancement over the original MD2 vertex animation format. SKM provides:

- **Hierarchical bones** - Parent-child bone relationships
- **Smooth blending** - Interpolation between animation frames
- **Bone manipulation** - Runtime bone transforms (IK, look-at, etc.)
- **Root motion** - Animation-driven entity movement
- **Efficient storage** - Bones transform vertices, not keyframed vertices

### SKM vs MD2

| Feature | MD2 (Vertex Animation) | SKM (Skeletal) |
|---------|----------------------|----------------|
| **Storage** | Keyframe all vertices | Keyframe bone rotations |
| **Blending** | Expensive (all verts) | Cheap (just bones) |
| **File Size** | Large | Smaller |
| **Flexibility** | Limited | High (IK, constraints) |
| **Root Motion** | No | Yes |

## SKM File Format

### Bone Hierarchy

Bones form a parent-child tree:

```
Root
├── Spine
│   ├── Chest
│   │   ├── LeftShoulder
│   │   │   ├── LeftElbow
│   │   │   │   └── LeftHand
│   │   ├── RightShoulder
│   │   │   ├── RightElbow
│   │   │   │   └── RightHand
│   │   └── Neck
│   │       └── Head
│   └── Pelvis
│       ├── LeftHip
│       │   ├── LeftKnee
│       │   │   └── LeftFoot
│       └── RightHip
│           ├── RightKnee
│               └── RightFoot
```

### Bone Structure

```cpp
// Bone definition (format_skm.h)
typedef struct {
    char        name[64];        // Bone name
    int         parent;          // Parent bone index (-1 for root)
    vec3_t      position;        // Local position relative to parent
    quat_t      rotation;        // Local rotation (quaternion)
} skm_bone_t;
```

### Animation Frame

Each animation frame contains bone transforms:

```cpp
// Animation frame (format_skm.h)
typedef struct {
    float       time;            // Time in seconds
    vec3_t      *positions;      // Per-bone positions [num_bones]
    quat_t      *rotations;      // Per-bone rotations [num_bones]
} skm_frame_t;
```

### Complete SKM Structure

```cpp
// SKM model file (format_skm.h)
typedef struct {
    int         num_bones;       // Total bone count
    skm_bone_t  *bones;          // Bone definitions
    
    int         num_anims;       // Animation count
    skm_anim_t  *anims;          // Animation definitions
    
    int         num_meshes;      // Mesh count
    skm_mesh_t  *meshes;         // Mesh geometry
} skm_model_t;

// Animation definition
typedef struct {
    char        name[64];        // Animation name "run", "attack", etc.
    int         first_frame;     // Starting frame index
    int         num_frames;      // Frame count
    float       framerate;       // Frames per second
    bool        looping;         // Loop animation?
} skm_anim_t;
```

## Animation Playback

### Playing an Animation

```cpp
// Entity with SKM model
class svg_animated_entity_t : public svg_base_edict_t {
    skm_anim_state_t anim_state;  // Animation playback state
    
    void PlayAnimation(const char *anim_name, bool loop = true) {
        // Find animation by name
        int anim_index = SKM_FindAnimation(model_handle, anim_name);
        if (anim_index < 0) {
            gi.dprintf("Animation '%s' not found\n", anim_name);
            return;
        }
        
        // Set animation
        anim_state.anim = anim_index;
        anim_state.frame = 0;
        anim_state.oldframe = 0;
        anim_state.backlerp = 0.0f;
        anim_state.looping = loop;
        anim_state.time = level.time;
        
        // Update entity state for networking
        s.frame = 0;
        s.old_frame = 0;
    }
};

// Example usage
void soldier_attack(svg_animated_entity_t *self) {
    self->PlayAnimation("attack", false);  // Play once
    self->nextthink = level.time + 0.5f;   // Fire weapon mid-animation
}
```

### Frame Interpolation

Smooth animation between keyframes:

```cpp
void UpdateAnimation(svg_animated_entity_t *self, float delta_time) {
    skm_anim_state_t *state = &self->anim_state;
    skm_anim_t *anim = &model->anims[state->anim];
    
    // Advance time
    state->time += delta_time;
    
    // Calculate frame
    float frame_time = state->time * anim->framerate;
    int current_frame = (int)frame_time;
    
    // Interpolation factor (0.0 to 1.0)
    float lerp = frame_time - current_frame;
    
    // Handle looping
    if (current_frame >= anim->num_frames) {
        if (anim->looping) {
            current_frame = current_frame % anim->num_frames;
        } else {
            current_frame = anim->num_frames - 1;
            lerp = 0.0f;  // Stay on last frame
        }
    }
    
    // Update state
    state->frame = current_frame;
    state->oldframe = (current_frame - 1 + anim->num_frames) % anim->num_frames;
    state->backlerp = 1.0f - lerp;
    
    // Network to clients
    self->s.frame = current_frame + anim->first_frame;
    self->s.old_frame = state->oldframe + anim->first_frame;
}
```

## Animation Blending

Blend between two animations:

```cpp
// Blend from current animation to new animation
void BlendToAnimation(svg_animated_entity_t *self, const char *anim_name, 
                      float blend_time = 0.2f) {
    int new_anim = SKM_FindAnimation(model_handle, anim_name);
    if (new_anim < 0)
        return;
    
    // Store blend info
    self->blend_from_anim = self->anim_state.anim;
    self->blend_from_frame = self->anim_state.frame;
    self->blend_to_anim = new_anim;
    self->blend_duration = blend_time;
    self->blend_time = 0.0f;
    self->blending = true;
}

void UpdateBlend(svg_animated_entity_t *self, float delta_time) {
    if (!self->blending)
        return;
    
    self->blend_time += delta_time;
    float blend_factor = self->blend_time / self->blend_duration;
    
    if (blend_factor >= 1.0f) {
        // Blend complete
        self->blending = false;
        self->anim_state.anim = self->blend_to_anim;
        self->anim_state.frame = 0;
    } else {
        // Interpolate between animations
        // (Rendering system handles actual blending)
        self->blend_alpha = blend_factor;
    }
}
```

## Bone Manipulation

### Getting Bone Transform

```cpp
// Get current world-space transform of a bone
bool GetBoneTransform(svg_animated_entity_t *self, const char *bone_name,
                      vec3_t out_pos, vec3_t out_angles) {
    int bone_index = SKM_FindBone(self->model_handle, bone_name);
    if (bone_index < 0)
        return false;
    
    // Get bone transform from animation
    matrix3x4_t bone_matrix;
    SKM_GetBoneMatrix(self->model_handle, &self->anim_state, 
                      bone_index, bone_matrix);
    
    // Convert to world space
    matrix3x4_t world_matrix;
    Matrix3x4_Concat(world_matrix, self->s.origin, self->s.angles, bone_matrix);
    
    // Extract position and angles
    Matrix3x4_GetOrigin(world_matrix, out_pos);
    Matrix3x4_ToAngles(world_matrix, out_angles);
    
    return true;
}
```

### Example: Weapon Attachment

```cpp
void UpdateWeaponPosition(svg_animated_entity_t *self) {
    vec3_t hand_pos, hand_angles;
    
    // Get right hand bone position
    if (GetBoneTransform(self, "RightHand", hand_pos, hand_angles)) {
        // Position weapon at hand
        if (self->weapon_entity) {
            VectorCopy(hand_pos, self->weapon_entity->s.origin);
            VectorCopy(hand_angles, self->weapon_entity->s.angles);
            gi.linkentity(self->weapon_entity);
        }
    }
}
```

## Animation Events

Trigger gameplay events at specific frames:

```cpp
// Animation event definition
typedef struct {
    int         frame;           // Frame to trigger on
    const char  *event_name;     // Event identifier
    const char  *parameter;      // Optional parameter
} anim_event_t;

// Example: Attack animation with hit detection
anim_event_t soldier_attack_events[] = {
    { 5,  "fire_weapon", NULL },      // Fire gun on frame 5
    { 10, "footstep", "left" },       // Left footstep
    { 20, "footstep", "right" },      // Right footstep
};

void UpdateAnimationEvents(svg_animated_entity_t *self) {
    int current_frame = self->anim_state.frame;
    
    // Check if we crossed an event frame
    if (current_frame != self->last_event_frame) {
        for (int i = 0; i < num_events; i++) {
            if (soldier_attack_events[i].frame == current_frame) {
                // Trigger event
                HandleAnimEvent(self, &soldier_attack_events[i]);
            }
        }
        self->last_event_frame = current_frame;
    }
}

void HandleAnimEvent(svg_animated_entity_t *self, anim_event_t *event) {
    if (strcmp(event->event_name, "fire_weapon") == 0) {
        soldier_fire(self);
    } else if (strcmp(event->event_name, "footstep") == 0) {
        PlayFootstepSound(self, event->parameter);
    }
}
```

## Integration with Entities

### Entity Setup

```cpp
void svg_animated_monster_t::Spawn() {
    // Load SKM model
    gi.SetModel(edict, "models/monsters/soldier/soldier.skm");
    
    // Initialize animation state
    anim_state.anim = SKM_FindAnimation(model_handle, "idle");
    anim_state.frame = 0;
    anim_state.looping = true;
    anim_state.time = 0;
    
    // Set entity properties
    mins = {-16, -16, -24};
    maxs = {16, 16, 32};
    movetype = MOVETYPE_STEP;
    solid = SOLID_BBOX;
    
    gi.linkentity(edict);
}
```

### Think Function

```cpp
void svg_animated_monster_t::Think() {
    // Update animation
    float delta = FRAMETIME;
    UpdateAnimation(this, delta);
    
    // Update bone-attached entities (weapons, etc.)
    UpdateWeaponPosition(this);
    
    // AI logic...
    AI_Run(this, 8);
    
    nextthink = level.time + FRAMETIME;
}
```

## Common Animations

Standard animations for characters:

- **idle** - Standing still
- **walk** - Walking forward
- **run** - Running forward
- **attack** - Attack/fire weapon
- **pain** - Damage reaction
- **death** - Death sequence
- **jump** - Jumping
- **crouch** - Crouching
- **crouch_walk** - Crouched movement

## Best Practices

### Animation Naming

Use consistent naming conventions:

```
idle
walk
run
run_backwards
strafe_left
strafe_right
attack_melee
attack_ranged
pain_light
pain_heavy
death_forward
death_back
```

### Frame Rate

Typical frame rates:
- **30 FPS** - Normal animation
- **60 FPS** - High-quality smooth animation
- **15 FPS** - Low-detail background characters

### Looping vs One-Shot

- **Looping**: idle, walk, run (continuous)
- **One-shot**: attack, pain, death (play once, then idle)

### Performance

```cpp
// Cache bone indices (don't search by name every frame)
class svg_animated_entity_t {
    int cached_hand_bone;
    
    void CacheBones() {
        cached_hand_bone = SKM_FindBone(model_handle, "RightHand");
    }
    
    void UpdateWeapon() {
        if (cached_hand_bone >= 0) {
            // Use cached index
            SKM_GetBoneMatrix(model_handle, &anim_state, 
                             cached_hand_bone, bone_matrix);
        }
    }
};
```

## Summary

Q2RTXPerimental's skeletal animation system provides:

- **Hierarchical bones** - Parent-child relationships
- **Smooth interpolation** - Frame blending
- **Animation blending** - Transition between animations
- **Bone manipulation** - Attach weapons, IK, constraints
- **Animation events** - Trigger gameplay at keyframes
- **Root motion** - Animation-driven movement (see Root-Motion-System.md)

**Key Files:**
- `sharedgame/sg_skm.h/.cpp` - Animation playback
- `inc/shared/formats/format_skm.h` - File format
- `sharedgame/sg_skm_rootmotion.h/.cpp` - Root motion extraction

**For root motion** (animation-driven entity movement), see Root-Motion-System.md.
