# Vanilla Quake 2 AI System

The Quake 2 AI system provides monster intelligence, pathfinding, target tracking, and combat behavior. Understanding AI is essential for creating convincing enemies and NPCs.

> **Note:** This page documents the **original Q2RTX** AI system from `/src/baseq2/`. For Q2RTXPerimental enhancements, see the Entity System documentation.

**Primary Files (Vanilla Q2RTX):**
- `src/baseq2/svgame/g_ai.cpp` - Core AI functions (target finding, movement, attacks)
- `src/baseq2/svgame/m_move.cpp` - Monster movement and animation
- `src/baseq2/svgame/m_*.cpp` - Individual monster implementations (soldier, tank, etc.)

## Overview

Quake 2 AI operates on a **finite state machine** model where monsters have different behaviors based on their current state:

1. **IDLE** - Standing around, looking for targets
2. **SEARCH** - Lost sight of enemy, searching
3. **COMBAT** - Actively fighting an enemy
4. **PAIN** - Reacting to damage
5. **DEATH** - Death animation and cleanup

### AI Think Cycle

Every monster runs AI logic each frame (or at intervals via `nextthink`):

```cpp
void monster_think(edict_t *self) {
    // 1. Check if enemy is still valid
    // 2. Determine visibility and range to enemy
    // 3. Choose appropriate action (attack, pursue, search, etc.)
    // 4. Execute action (move, fire weapon, play animation)
    // 5. Schedule next think
    self->nextthink = level.time + FRAMETIME;
}
```

## Core AI Functions

### ai_stand

**Purpose:** Standing around looking for targets

**Usage:** Idle monsters scan for players

```cpp
void ai_stand(edict_t *self, float dist) {
    // dist is for animation position adjustment
    
    // Look for targets
    if (FindTarget(self))
        return;  // Found enemy, switch to combat
    
    // Check if monster can hear something
    if (level.time > self->monsterinfo.pausetime)
        self->monsterinfo.stand(self);  // Play idle animation
}
```

**Key Features:**
- Periodically searches for players with `FindTarget()`
- Plays idle animations
- Responds to sounds (gunfire, footsteps)
- Can be alerted by nearby monsters

### ai_walk

**Purpose:** Patrolling and wandering

**Usage:** Monsters walking between waypoints

```cpp
void ai_walk(edict_t *self, float dist) {
    // Move forward at current facing
    M_walkmove(self, self->s.angles[YAW], dist);
    
    // Look for targets while walking
    if (FindTarget(self))
        return;  // Found enemy
    
    // Follow path if set
    if (self->movetarget) {
        // Move toward path_corner
        M_MoveToGoal(self, dist);
    }
}
```

**Key Features:**
- Moves specified distance each think
- Follows `path_corner` entities via `target` chain
- Continuously searches for enemies
- Used for patrol routes

### ai_run

**Purpose:** Chasing enemy

**Usage:** Monster pursuing player in combat

```cpp
void ai_run(edict_t *self, float dist) {
    vec3_t  v;
    edict_t *tempgoal, *save;
    edict_t *marker;
    float   d1, d2;
    trace_t tr;
    vec3_t  v_forward, v_right;
    float   left, center, right;
    vec3_t  left_target, right_target;
    
    // If no enemy, return to idle
    if (!self->enemy || !self->enemy->inuse) {
        self->monsterinfo.run(self);
        return;
    }
    
    // Check if enemy is visible
    if (visible(self, self->enemy)) {
        self->monsterinfo.search_time = level.time + 5;
    }
    
    // Move toward enemy
    if (self->monsterinfo.attack_state == AS_SLIDING) {
        // Sliding move attack (e.g., Tank charge)
        M_walkmove(self, self->ideal_yaw, dist);
    } else if (self->monsterinfo.aiflags & AI_STAND_GROUND) {
        // Stand and fight
        M_ChangeYaw(self);
    } else {
        // Chase enemy
        M_MoveToGoal(self, dist);
    }
    
    // Check if in attack range
    if (ai_checkattack(self, dist))
        return;  // Started attack
}
```

**Key Features:**
- Continuously tracks enemy position
- Pathfinds around obstacles
- Checks attack range and initiates attacks
- Handles sliding attacks (charges)
- Respects AI_STAND_GROUND flag (stationary turrets)

### ai_charge

**Purpose:** Aggressive melee charge

**Usage:** Melee monsters charging player

```cpp
void ai_charge(edict_t *self, float dist) {
    // More aggressive version of ai_run
    // Used by melee monsters (dog, parasite)
    
    // Check visibility
    if (visible(self, self->enemy)) {
        // Directly charge at enemy
        vec3_t v;
        VectorSubtract(self->enemy->s.origin, self->s.origin, v);
        self->ideal_yaw = vectoyaw(v);
        
        // Move faster when charging
        M_walkmove(self, self->ideal_yaw, dist);
    } else {
        // Lost sight, use normal run behavior
        ai_run(self, dist);
    }
}
```

**Key Features:**
- Direct line charge at enemy
- Faster movement than normal run
- Falls back to ai_run if enemy not visible

### FindTarget

**Purpose:** Find valid player target

**Returns:** `true` if enemy found, `false` otherwise

```cpp
bool FindTarget(edict_t *self) {
    edict_t *client;
    bool    heardit;
    int     r;
    
    // Already has an enemy
    if (self->enemy && self->enemy->inuse && self->enemy->health > 0)
        return true;
    
    // Use level.sight_client (cycles between players)
    client = level.sight_client;
    if (!client)
        return false;  // No valid players
    
    // Check if player is targetable
    if (client->flags & FL_NOTARGET)
        return false;
    
    if (!visible(self, client))
        return false;  // Can't see player
    
    // Check if player is in front (180° FOV)
    if (!infront(self, client))
        return false;
    
    // Found valid target!
    self->enemy = client;
    FoundTarget(self);
    return true;
}
```

**Targeting Rules:**
1. Players with `notarget` cheat are ignored
2. Must have line of sight (not blocked by walls)
3. Must be in front of monster (180° field of view)
4. Dead players are ignored

### FoundTarget

**Purpose:** Called when enemy is first acquired

**Actions:**
- Sets enemy relationship
- Plays alert sound
- Alerts nearby monsters
- Switches to combat state

```cpp
void FoundTarget(edict_t *self) {
    // Play sight sound
    if (self->enemy->client) {
        if (self->enemy->flags & FL_DISGUISED)
            ; // Don't alert
        else
            gi.sound(self, CHAN_VOICE, self->monsterinfo.sound_sight, 1, ATTN_NORM, 0);
    }
    
    // Alert nearby monsters
    AlertAllies(self);
    
    // Remember last known position
    VectorCopy(self->enemy->s.origin, self->monsterinfo.last_sighting);
    self->monsterinfo.trail_time = level.time;
    
    // Switch to run/attack mode
    if (!self->combattarget) {
        HuntTarget(self);
        return;
    }
    
    // Trigger combat target (e.g., alarm)
    self->goalentity = self->movetarget = G_PickTarget(self->combattarget);
    if (!self->movetarget) {
        self->goalentity = self->movetarget = self->enemy;
        HuntTarget(self);
    }
}
```

## Enemy Tracking

### Visibility Caching

AI caches visibility to avoid expensive checks every frame:

```cpp
// In monster AI function
if (level.time > self->monsterinfo.idle_time) {
    // Time to recheck visibility
    self->monsterinfo.idle_time = level.time + 0.5;  // Check every 0.5 sec
    
    if (visible(self, self->enemy)) {
        enemy_vis = true;
        // Update last known position
        VectorCopy(self->enemy->s.origin, self->monsterinfo.last_sighting);
    } else {
        enemy_vis = false;
    }
}
```

### Range Detection

Monsters classify enemy distance for attack selection:

```cpp
#define RANGE_MELEE     0   // 0-80 units (melee attack)
#define RANGE_NEAR      1   // 80-500 units (close range weapons)
#define RANGE_MID       2   // 500-1000 units (medium range)
#define RANGE_FAR       3   // 1000+ units (long range/sniping)

int range(edict_t *self, edict_t *other) {
    vec3_t  v;
    float   len;
    
    VectorSubtract(self->s.origin, other->s.origin, v);
    len = VectorLength(v);
    
    if (len < 80)
        return RANGE_MELEE;
    if (len < 500)
        return RANGE_NEAR;
    if (len < 1000)
        return RANGE_MID;
    return RANGE_FAR;
}
```

### Angle to Enemy

Calculate angle to face enemy:

```cpp
float vectoyaw(vec3_t vec) {
    float yaw;
    
    if (vec[1] == 0 && vec[0] == 0)
        yaw = 0;
    else {
        yaw = (int)(atan2(vec[1], vec[0]) * 180 / M_PI);
        if (yaw < 0)
            yaw += 360;
    }
    
    return yaw;
}

// Usage
vec3_t v;
VectorSubtract(self->enemy->s.origin, self->s.origin, v);
self->ideal_yaw = vectoyaw(v);
M_ChangeYaw(self);  // Smoothly turn toward ideal_yaw
```

## Attack System

### ai_checkattack

**Purpose:** Decide if monster should attack

**Returns:** `true` if attack initiated

```cpp
bool ai_checkattack(edict_t *self, float dist) {
    vec3_t  temp;
    bool    hesDeadJim;
    
    // Dead enemies can't be attacked
    if (self->enemy->health > 0)
        hesDeadJim = false;
    else
        hesDeadJim = true;
    
    // Check if enemy is visible
    if (!visible(self, self->enemy)) {
        // Lost sight of enemy
        if ((level.time - self->monsterinfo.search_time) > 3) {
            // Been searching too long, go back to wandering
            M_SetActivity(self, ACT_SEARCH);
            return true;
        }
        return false;
    }
    
    // Get distance
    enemy_range = range(self, self->enemy);
    
    // Too far for current weapon?
    if (enemy_range == RANGE_FAR)
        return false;
    
    // Call monster-specific attack check
    if (self->monsterinfo.checkattack) {
        if (self->monsterinfo.checkattack(self))
            return true;  // Monster decided to attack
    }
    
    // In melee range?
    if (enemy_range == RANGE_MELEE) {
        if (self->monsterinfo.melee)
            self->monsterinfo.attack_state = AS_MELEE;
        else
            self->monsterinfo.attack_state = AS_MISSILE;
        return true;
    }
    
    // Random chance to attack based on range
    float chance;
    if (enemy_range == RANGE_MELEE)
        chance = 0.8;
    else if (enemy_range == RANGE_NEAR)
        chance = 0.4;
    else if (enemy_range == RANGE_MID)
        chance = 0.2;
    else
        chance = 0;
    
    if (random() < chance) {
        self->monsterinfo.attack_state = AS_MISSILE;
        return true;
    }
    
    return false;
}
```

### Attack States

```cpp
// Monster attack states (monsterinfo.attack_state)
#define AS_STRAIGHT     1   // Direct fire at enemy
#define AS_SLIDING      2   // Slide/charge attack (Tank)
#define AS_MELEE        3   // Melee attack
#define AS_MISSILE      4   // Ranged attack (rockets, bullets, etc.)
```

### Attack Execution

Monsters have callbacks for different attack types:

```cpp
// Monster structure (simplified)
typedef struct {
    void (*stand)(edict_t *self);
    void (*idle)(edict_t *self);
    void (*search)(edict_t *self);
    void (*walk)(edict_t *self);
    void (*run)(edict_t *self);
    void (*attack)(edict_t *self);    // General attack
    void (*melee)(edict_t *self);     // Melee attack
    void (*sight)(edict_t *self);     // Spotted enemy (sound)
    // ...
} monsterinfo_t;

// Example: Soldier fires his weapon
void soldier_fire(edict_t *self) {
    vec3_t  start;
    vec3_t  forward, right;
    vec3_t  aim;
    
    // Calculate firing position
    AngleVectors(self->s.angles, forward, right, NULL);
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ_BLASTER], 
                    forward, right, start);
    
    // Aim at enemy
    VectorCopy(self->enemy->s.origin, aim);
    aim[2] += self->enemy->viewheight;
    VectorSubtract(aim, start, forward);
    VectorNormalize(forward);
    
    // Fire blaster
    monster_fire_blaster(self, start, forward, 5, 500, MZ_BLASTER);
}
```

## Pain and Death

### Pain Response

When damaged, monsters react with pain animation:

```cpp
void monster_pain(edict_t *self, edict_t *other, float kick, int damage) {
    float   r;
    
    // Already dead
    if (self->health < (self->gib_health + 1))
        return;
    
    // Pain sound cooldown (don't spam pain sounds)
    if (level.time < self->pain_debounce_time)
        return;
    
    self->pain_debounce_time = level.time + 3;  // 3 second cooldown
    
    // Play pain sound
    gi.sound(self, CHAN_VOICE, self->monsterinfo.sound_pain, 1, ATTN_NORM, 0);
    
    // Call monster-specific pain behavior
    if (self->monsterinfo.pain)
        self->monsterinfo.pain(self, other, kick, damage);
    
    // Set enemy if we don't have one
    if (!self->enemy && (other->client || other->svflags & SVF_MONSTER))
        self->enemy = other;
}
```

**Pain Features:**
- 3-second cooldown to avoid spam
- Sets attacker as enemy if no current enemy
- Calls monster-specific pain animation
- Can interrupt current action

### Death Sequence

Death handling with gibbing:

```cpp
void monster_die(edict_t *self, edict_t *inflictor, edict_t *attacker, 
                 int damage, vec3_t point) {
    int     n;
    
    // Check for gibbing (overkill damage)
    if (self->health <= self->gib_health) {
        // Gib death
        gi.sound(self, CHAN_VOICE, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);
        
        // Throw gibs
        for (n = 0; n < 2; n++)
            ThrowGib(self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
        for (n = 0; n < 4; n++)
            ThrowGib(self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
        
        ThrowHead(self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
        
        self->deadflag = DEAD_DEAD;
        return;
    }
    
    // Normal death
    if (self->deadflag == DEAD_DEAD)
        return;
    
    self->deadflag = DEAD_DEAD;
    self->takedamage = DAMAGE_YES;
    
    // Make entity a corpse
    self->movetype = MOVETYPE_TOSS;
    
    // Play death animation
    if (self->monsterinfo.die)
        self->monsterinfo.die(self, inflictor, attacker, damage, point);
}
```

**Death Features:**
- Gibbing when health drops below `gib_health`
- Corpse becomes MOVETYPE_TOSS (falls realistically)
- Triggers `target` and `killtarget`
- Awards frag to attacker in deathmatch

## Monster Animation

### Frame-Based Animation

Monsters use frame sequences defined in `mmove_t`:

```cpp
// Animation move definition
typedef struct {
    int         firstframe;         // Starting frame
    int         lastframe;          // Ending frame
    mframe_t    *frame;             // Per-frame actions
    void        (*endfunc)(edict_t *self);  // Called when sequence ends
} mmove_t;

// Per-frame definition
typedef struct {
    void        (*aifunc)(edict_t *self, float dist);  // AI function to call
    float       dist;               // Movement distance
    void        (*thinkfunc)(edict_t *self);  // Special action (fire, etc.)
} mframe_t;
```

### Example: Soldier Walk Animation

```cpp
mframe_t soldier_frames_walk[] = {
    { ai_walk, 3,  NULL },   // Frame 0: walk 3 units forward
    { ai_walk, 6,  NULL },   // Frame 1: walk 6 units
    { ai_walk, 2,  NULL },   // Frame 2: walk 2 units
    { ai_walk, 2,  NULL },
    { ai_walk, 2,  NULL },
    { ai_walk, 1,  NULL },
    { ai_walk, 6,  NULL },
    { ai_walk, 5,  NULL },
    { ai_walk, 3,  NULL },
    { ai_walk, -1, NULL },   // Frame 9: walk back 1 unit
};

mmove_t soldier_move_walk = {
    FRAME_walk101,          // First frame model index
    FRAME_walk110,          // Last frame model index
    soldier_frames_walk,    // Frame data
    NULL                    // No end function
};

void soldier_walk(edict_t *self) {
    self->monsterinfo.currentmove = &soldier_move_walk;
}
```

### Animation State Changes

```cpp
void M_SetActivity(edict_t *self, int activity) {
    switch (activity) {
        case ACT_STAND:
            self->monsterinfo.stand(self);
            break;
        case ACT_WALK:
            self->monsterinfo.walk(self);
            break;
        case ACT_RUN:
            self->monsterinfo.run(self);
            break;
        case ACT_ATTACK:
            self->monsterinfo.attack(self);
            break;
        // ...
    }
}
```

## Pathfinding

### Simple Waypoint System

Monsters follow `path_corner` entities:

```cpp
// Map entity example:
// {
//   "classname" "path_corner"
//   "targetname" "patrol1"
//   "target" "patrol2"
// }
// {
//   "classname" "path_corner"
//   "targetname" "patrol2"
//   "target" "patrol1"
// }

void ai_walk(edict_t *self, float dist) {
    if (self->movetarget) {
        // Moving toward waypoint
        if (M_MoveToGoal(self, dist)) {
            // Reached waypoint
            self->goalentity = self->movetarget = G_PickTarget(self->movetarget->target);
            
            // Wait at waypoint if specified
            if (self->movetarget->wait > 0) {
                self->monsterinfo.pausetime = level.time + self->movetarget->wait;
                self->monsterinfo.stand(self);
            }
        }
    }
}
```

### Obstacle Avoidance

Simple left/right checks when blocked:

```cpp
bool M_walkmove(edict_t *ent, float yaw, float dist) {
    vec3_t  move;
    
    // Try moving forward
    yaw = yaw * M_PI * 2 / 360;
    move[0] = cos(yaw) * dist;
    move[1] = sin(yaw) * dist;
    move[2] = 0;
    
    if (SV_movestep(ent, move, true))
        return true;  // Success
    
    // Blocked! Try strafing
    if (random() < 0.5) {
        // Try left
        yaw -= M_PI / 4;
        move[0] = cos(yaw) * dist;
        move[1] = sin(yaw) * dist;
        if (SV_movestep(ent, move, true))
            return true;
        
        // Try right instead
        yaw += M_PI / 2;
        move[0] = cos(yaw) * dist;
        move[1] = sin(yaw) * dist;
        return SV_movestep(ent, move, true);
    }
    
    return false;  // Completely stuck
}
```

## Monster Flags

### AI Flags

```cpp
// Monster AI behavior flags (monsterinfo.aiflags)
#define AI_STAND_GROUND     0x00000001  // Never move (turrets)
#define AI_TEMP_STAND_GROUND 0x00000002 // Temporarily stationary
#define AI_SOUND_TARGET     0x00000004  // Moving toward a sound
#define AI_LOST_SIGHT       0x00000008  // Can't see enemy
#define AI_PURSUIT_LAST_SEEN 0x00000010 // Going to last known pos
#define AI_PURSUE_NEXT      0x00000020  // Following predicted path
#define AI_PURSUE_TEMP      0x00000040  // Temp pursuit
#define AI_HOLD_FRAME       0x00000080  // Don't advance animation
#define AI_GOOD_GUY         0x00000100  // Friendly NPC (won't attack player)
#define AI_BRUTAL           0x00000200  // Extra aggressive
#define AI_NOSTEP           0x00000400  // Can't step up ledges
#define AI_DUCKED           0x00000800  // Currently ducked
#define AI_COMBAT_POINT     0x00001000  // Moving to combat point
#define AI_MEDIC            0x00002000  // Medic behavior (heals others)
#define AI_RESURRECTING     0x00004000  // Currently reviving corpse
```

### Example: Turret (Stationary)

```cpp
void turret_spawn(edict_t *self) {
    // Set as stationary turret
    self->monsterinfo.aiflags |= AI_STAND_GROUND;
    
    // Never move from spawn position
    self->movetype = MOVETYPE_NONE;
    
    // Can only rotate to face enemy
    self->monsterinfo.run = turret_run;  // Just turns toward enemy
}
```

## Alerting Nearby Monsters

When one monster spots a player, nearby monsters are alerted:

```cpp
void AlertAllies(edict_t *self) {
    edict_t *e;
    int     i;
    
    // Alert all monsters within 512 units
    for (i = 1; i < globals.num_edicts; i++) {
        e = &g_edicts[i];
        
        if (!e->inuse)
            continue;
        if (!(e->svflags & SVF_MONSTER))
            continue;
        if (e == self)
            continue;
        if (!e->monsterinfo.aiflags & AI_SOUND_TARGET)
            continue;
        
        // Check distance
        float dist = VectorDistance(self->s.origin, e->s.origin);
        if (dist > 512)
            continue;
        
        // Alert this monster
        if (!e->enemy) {
            e->enemy = self->enemy;
            FoundTarget(e);
        }
    }
}
```

## Best Practices

### Performance Optimization

```cpp
// Don't check visibility every frame
if (level.time > self->monsterinfo.idle_time) {
    self->monsterinfo.idle_time = level.time + 0.5;  // Check every 0.5 sec
    enemy_vis = visible(self, self->enemy);
}

// Use cached result for rest of frame
if (enemy_vis) {
    // Can see enemy
}
```

### Enemy Validation

Always validate enemy pointer:

```cpp
if (self->enemy && self->enemy->inuse && self->enemy->health > 0) {
    // Enemy is valid
} else {
    self->enemy = NULL;  // Clear invalid enemy
}
```

### Animation Timing

Synchronize attacks with animation frames:

```cpp
mframe_t gunner_frames_attack[] = {
    { ai_charge, 0,  NULL },
    { ai_charge, 0,  NULL },
    { ai_charge, 0,  gunner_fire },  // Fire on frame 2
    { ai_charge, 0,  NULL },
    { ai_charge, 0,  NULL },
};
```

## Example: Complete Monster AI

### Soldier Think Function

```cpp
void soldier_think(edict_t *self) {
    // Validate enemy
    if (self->enemy && (!self->enemy->inuse || self->enemy->health <= 0))
        self->enemy = NULL;
    
    // Determine current behavior
    if (!self->enemy) {
        // No enemy - idle or patrol
        if (self->movetarget)
            ai_walk(self, 8);  // Follow patrol route
        else
            ai_stand(self, 0);  // Stand and look around
    } else {
        // Have enemy - combat mode
        float dist = VectorDistance(self->s.origin, self->enemy->s.origin);
        
        if (visible(self, self->enemy)) {
            // Can see enemy
            if (dist < 80) {
                // Melee range - kick
                self->monsterinfo.attack_state = AS_MELEE;
                soldier_attack(self);
            } else if (dist < 1000) {
                // Weapon range - shoot
                if (ai_checkattack(self, dist)) {
                    soldier_attack(self);
                } else {
                    // Get closer
                    ai_run(self, 8);
                }
            } else {
                // Too far - chase
                ai_run(self, 10);
            }
        } else {
            // Can't see enemy - search last known position
            ai_run(self, 8);
        }
    }
    
    // Schedule next think
    self->nextthink = level.time + FRAMETIME;
}
```

## Summary

The vanilla Q2RTX AI system provides:

- **State-based behavior** - Stand, walk, run, attack, pain, death
- **Target acquisition** - `FindTarget()` with visibility and FOV checks
- **Range-based combat** - Melee, near, mid, far attack selection
- **Simple pathfinding** - Waypoint following via `path_corner` entities
- **Animation integration** - Frame-based movement and attack timing
- **Alert system** - Monsters notify nearby allies
- **Pain reactions** - Damage causes pain animation with cooldown
- **Death and gibbing** - Normal death or gib on overkill

**Key Files to Study:**
- `src/baseq2/svgame/g_ai.cpp` - Core AI functions
- `src/baseq2/svgame/m_soldier.cpp` - Example monster implementation
- `src/baseq2/svgame/m_move.cpp` - Movement and animation system

**For Q2RTXPerimental enhancements**, see the Entity System documentation which covers the C++ class-based monster system with enhanced behaviors.
