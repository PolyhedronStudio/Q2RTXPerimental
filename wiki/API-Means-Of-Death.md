# API Reference - Means of Death

Means of Death (MOD) constants identify how an entity died or took damage. These values are used for death messages, statistics, and gameplay logic. They are defined in `src/baseq2rtxp/sharedgame/sg_means_of_death.h`.

## Overview

When an entity takes damage or dies, the `means_of_death` parameter identifies the cause. This information is used for:

- **Death Messages**: Displaying appropriate obituary messages to players
- **Statistics**: Tracking kills by weapon type
- **Gameplay Logic**: Different death types trigger different effects (gibs, corpses, respawn rules)
- **Achievements**: Tracking specific kill methods

## Means of Death Constants

**Location:** `src/baseq2rtxp/sharedgame/sg_means_of_death.h`

```cpp
typedef enum sg_means_of_death_e {
    MEANS_OF_DEATH_UNKNOWN = 0,
    
    // Weapon Deaths
    MEANS_OF_DEATH_HIT_FIGHTING,
    MEANS_OF_DEATH_HIT_PISTOL,
    MEANS_OF_DEATH_HIT_SMG,
    MEANS_OF_DEATH_HIT_RIFLE,
    MEANS_OF_DEATH_HIT_SNIPER,
    
    // Environmental Deaths
    MEANS_OF_DEATH_WATER,
    MEANS_OF_DEATH_SLIME,
    MEANS_OF_DEATH_LAVA,
    MEANS_OF_DEATH_CRUSHED,
    MEANS_OF_DEATH_TELEFRAGGED,
    
    // Self-Inflicted Deaths
    MEANS_OF_DEATH_SUICIDE,
    MEANS_OF_DEATH_FALLING,
    
    // Explosive Deaths
    MEANS_OF_DEATH_EXPLOSIVE,
    MEANS_OF_DEATH_EXPLODED_BARREL,
    
    // Energy Deaths
    MEANS_OF_DEATH_LASER,
    
    // Misc Deaths
    MEANS_OF_DEATH_SPLASH,
    MEANS_OF_DEATH_TRIGGER_HURT,
    MEANS_OF_DEATH_EXIT,
    MEANS_OF_DEATH_FRIENDLY_FIRE
} sg_means_of_death_t;
```

## Means of Death Categories

### Weapon Deaths

#### MEANS_OF_DEATH_HIT_FIGHTING

**Description:** Killed by fists, punches, or melee attacks

**Usage:**
```cpp
void svg_weapon_fists_t::Fire() {
    T_Damage(target, self, self, direction, point,
             vec3_zero(), damage, 0, 0, MEANS_OF_DEATH_HIT_FIGHTING);
}
```

**Death Message Examples:**
- "Player was punched by Enemy"
- "Player was beaten down by Enemy"

#### MEANS_OF_DEATH_HIT_PISTOL

**Description:** Killed by a pistol

**Usage:**
```cpp
void svg_weapon_pistol_t::Fire() {
    Fire_Bullet(start, forward, damage, kick, 
                hspread, vspread, MEANS_OF_DEATH_HIT_PISTOL);
}
```

**Death Message Examples:**
- "Player was shot by Enemy's pistol"
- "Player caught Enemy's bullet"

#### MEANS_OF_DEATH_HIT_SMG

**Description:** Killed by a submachine gun (SMG)

**Usage:**
```cpp
void svg_weapon_smg_t::Fire() {
    Fire_Bullet(start, forward, damage, kick,
                hspread, vspread, MEANS_OF_DEATH_HIT_SMG);
}
```

**Death Message Examples:**
- "Player was perforated by Enemy's SMG"
- "Player was riddled with bullets by Enemy"

#### MEANS_OF_DEATH_HIT_RIFLE

**Description:** Killed by an assault rifle

**Usage:**
```cpp
void svg_weapon_rifle_t::Fire() {
    Fire_Bullet(start, forward, damage, kick,
                hspread, vspread, MEANS_OF_DEATH_HIT_RIFLE);
}
```

**Death Message Examples:**
- "Player was gunned down by Enemy's rifle"
- "Player fell to Enemy's rifle fire"

#### MEANS_OF_DEATH_HIT_SNIPER

**Description:** Killed by a sniper rifle

**Usage:**
```cpp
void svg_weapon_sniper_t::Fire() {
    Fire_Bullet(start, forward, damage, kick,
                hspread, vspread, MEANS_OF_DEATH_HIT_SNIPER);
}
```

**Death Message Examples:**
- "Player was sniped by Enemy"
- "Enemy picked off Player from across the map"

### Environmental Deaths

#### MEANS_OF_DEATH_WATER

**Description:** Drowned in water

**Usage:**
```cpp
void svg_player_edict_t::CheckWaterDamage() {
    if (waterlevel == WATER_HEAD && air_finished < level.time) {
        // Player drowned
        health -= 2;
        if (health <= 0) {
            T_Damage(this, world, world, vec3_zero(), s.origin,
                     vec3_zero(), 15, 0, DAMAGE_NO_ARMOR,
                     MEANS_OF_DEATH_WATER);
        }
    }
}
```

**Death Message Examples:**
- "Player drowned"
- "Player forgot to surface"
- "Player sleeps with the fishes"

#### MEANS_OF_DEATH_SLIME

**Description:** Killed by toxic slime

**Usage:**
```cpp
void svg_player_edict_t::CheckEnvironmentDamage() {
    if (waterlevel && (watertype & CONTENTS_SLIME)) {
        T_Damage(this, world, world, vec3_zero(), s.origin,
                 vec3_zero(), 4, 0, 0, MEANS_OF_DEATH_SLIME);
    }
}
```

**Death Message Examples:**
- "Player was dissolved by slime"
- "Player took a dip in toxic waste"
- "Player melted"

#### MEANS_OF_DEATH_LAVA

**Description:** Killed by lava

**Usage:**
```cpp
void svg_player_edict_t::CheckEnvironmentDamage() {
    if (waterlevel && (watertype & CONTENTS_LAVA)) {
        T_Damage(this, world, world, vec3_zero(), s.origin,
                 vec3_zero(), 10, 0, 0, MEANS_OF_DEATH_LAVA);
    }
}
```

**Death Message Examples:**
- "Player tried to swim in lava"
- "Player turned extra crispy"
- "Player became a crispy critter"

#### MEANS_OF_DEATH_CRUSHED

**Description:** Crushed by moving geometry (doors, platforms, crushers)

**Usage:**
```cpp
void svg_func_door_t::Blocked(svg_base_edict_t *other) {
    if (dmg) {
        T_Damage(other, this, this, vec3_zero(), other->s.origin,
                 vec3_zero(), dmg, 1, 0, MEANS_OF_DEATH_CRUSHED);
    }
}
```

**Death Message Examples:**
- "Player was squished"
- "Player was crushed"
- "Player became a pancake"

#### MEANS_OF_DEATH_TELEFRAGGED

**Description:** Killed by someone teleporting to your location

**Usage:**
```cpp
void TeleportPlayer(svg_player_edict_t *player, vec3_t dest) {
    // Check for existing players at destination
    svg_base_edict_t *existing = CheckTeleportDestination(dest);
    if (existing && existing->takedamage) {
        T_Damage(existing, player, player, vec3_zero(), dest,
                 vec3_zero(), 100000, 0, 0, MEANS_OF_DEATH_TELEFRAGGED);
    }
}
```

**Death Message Examples:**
- "Player was telefragged by Enemy"
- "Enemy materialized on top of Player"

### Self-Inflicted Deaths

#### MEANS_OF_DEATH_SUICIDE

**Description:** Intentional suicide (kill command, self-damage)

**Usage:**
```cpp
void Cmd_Kill_f(svg_player_edict_t *player) {
    if (player->solid == SOLID_NOT)
        return;  // Already dead
        
    player->flags &= ~FL_GODMODE;
    player->health = 0;
    
    T_Damage(player, world, world, vec3_zero(), player->s.origin,
             vec3_zero(), 100000, 0, 0, MEANS_OF_DEATH_SUICIDE);
}
```

**Death Message Examples:**
- "Player took the easy way out"
- "Player committed suicide"
- "Player gave up"

#### MEANS_OF_DEATH_FALLING

**Description:** Fell from a great height

**Usage:**
```cpp
void svg_player_edict_t::CheckFallDamage() {
    float fall_distance = oldvelocity[2] - velocity[2];
    
    if (fall_distance > FATAL_FALL_DISTANCE) {
        T_Damage(this, world, world, vec3_zero(), s.origin,
                 vec3_zero(), 100000, 0, 0, MEANS_OF_DEATH_FALLING);
    }
}
```

**Death Message Examples:**
- "Player cratered"
- "Player hit the ground too hard"
- "Player fell to their death"

### Explosive Deaths

#### MEANS_OF_DEATH_EXPLOSIVE

**Description:** Killed by explosive weapons (grenades, rockets, etc.)

**Usage:**
```cpp
void svg_weapon_grenade_t::Explode() {
    T_RadiusDamage(this, owner, damage, nullptr, radius,
                   MEANS_OF_DEATH_EXPLOSIVE);
}
```

**Death Message Examples:**
- "Player was blown up by Enemy's grenade"
- "Enemy fragged Player"
- "Player caught Enemy's rocket"

#### MEANS_OF_DEATH_EXPLODED_BARREL

**Description:** Killed by an exploding barrel

**Usage:**
```cpp
void svg_misc_barrel_t::Die(svg_base_edict_t *inflictor, 
                            svg_base_edict_t *attacker, 
                            int damage, const vec3_t &point) {
    // Explode and damage nearby entities
    T_RadiusDamage(this, attacker, explosion_damage, nullptr, 
                   explosion_radius, MEANS_OF_DEATH_EXPLODED_BARREL);
}
```

**Death Message Examples:**
- "Player blew up"
- "Player was caught in a barrel explosion"
- "Enemy's aim was explosive near Player"

### Energy Deaths

#### MEANS_OF_DEATH_LASER

**Description:** Killed by laser weapons

**Usage:**
```cpp
void svg_weapon_laser_t::Fire() {
    Fire_Laser(start, forward, damage, 
               MEANS_OF_DEATH_LASER);
}
```

**Death Message Examples:**
- "Player was lasered by Enemy"
- "Enemy's laser cut through Player"
- "Player was vaporized by Enemy"

### Miscellaneous Deaths

#### MEANS_OF_DEATH_SPLASH

**Description:** Killed by splash damage particles

**Usage:**
```cpp
void TE_Splash_Damage(const vec3_t &origin, int damage) {
    // Deal damage to entities near splash
    T_RadiusDamage(nullptr, world, damage, nullptr, radius,
                   MEANS_OF_DEATH_SPLASH);
}
```

**Death Message Examples:**
- "Player was hit by splash damage"
- "Player got splashed"

#### MEANS_OF_DEATH_TRIGGER_HURT

**Description:** Killed by a `trigger_hurt` entity

**Usage:**
```cpp
void svg_trigger_hurt_t::Touch(svg_base_edict_t *other) {
    if (other->takedamage) {
        T_Damage(other, this, this, vec3_zero(), other->s.origin,
                 vec3_zero(), dmg, 0, 0, MEANS_OF_DEATH_TRIGGER_HURT);
    }
}
```

**Death Message Examples:**
- "Player was in the wrong place"
- "Player discovered a painful trigger"

#### MEANS_OF_DEATH_EXIT

**Description:** Killed by trying to change levels when not allowed

**Usage:**
```cpp
void svg_target_changelevel_t::Use(svg_base_edict_t *other) {
    if (!game.deathmatch) {
        // Allow level change in single player/coop
        BeamOut(player);
    } else {
        // Kill player in deathmatch (shouldn't happen)
        T_Damage(player, this, this, vec3_zero(), player->s.origin,
                 vec3_zero(), 10000, 0, 0, MEANS_OF_DEATH_EXIT);
    }
}
```

**Death Message Examples:**
- "Player tried to exit"
- "Player attempted to leave the battle"

#### MEANS_OF_DEATH_FRIENDLY_FIRE

**Description:** Killed by a teammate (in team-based game modes)

**Usage:**
```cpp
void T_Damage(svg_base_edict_t *targ, svg_base_edict_t *inflictor,
              svg_base_edict_t *attacker, /*...*/) {
    // Check for friendly fire
    if (OnSameTeam(targ, attacker)) {
        if (!g_friendlyfire->value)
            return;  // Friendly fire disabled
            
        means_of_death = MEANS_OF_DEATH_FRIENDLY_FIRE;
    }
    // ... damage logic ...
}
```

**Death Message Examples:**
- "Player was killed by teammate Enemy"
- "Enemy should check their targets"
- "Player was a victim of friendly fire"

## Using Means of Death

### In Damage Functions

```cpp
// Basic damage with MOD
void T_Damage(svg_base_edict_t *targ,
              svg_base_edict_t *inflictor,
              svg_base_edict_t *attacker,
              const vec3_t &dir,
              const vec3_t &point,
              const vec3_t &normal,
              int damage,
              int knockback,
              int dflags,
              int means_of_death) {
    
    // Apply damage...
    targ->health -= take;
    
    // Check for death
    if (targ->health <= 0) {
        targ->means_of_death = means_of_death;
        targ->Die(inflictor, attacker, take, point);
    }
}
```

### In Death Handlers

```cpp
void svg_player_edict_t::Die(svg_base_edict_t *inflictor,
                             svg_base_edict_t *attacker,
                             int damage,
                             const vec3_t &point) {
    // Check means of death for special handling
    switch (means_of_death) {
        case MEANS_OF_DEATH_FALLING:
            // No corpse, just gib
            ThrowGibs(damage, GIB_ORGANIC);
            break;
            
        case MEANS_OF_DEATH_LAVA:
        case MEANS_OF_DEATH_SLIME:
            // Dissolve corpse
            ThrowGibs(damage, GIB_ORGANIC);
            break;
            
        case MEANS_OF_DEATH_EXPLOSIVE:
            if (damage > 50) {
                ThrowGibs(damage, GIB_ORGANIC);
            } else {
                // Normal death, leave corpse
                BecomeCorpse();
            }
            break;
            
        default:
            // Standard death
            BecomeCorpse();
            break;
    }
    
    // Broadcast obituary
    BroadcastDeathMessage(this, attacker, means_of_death);
}
```

### Generating Death Messages

```cpp
void BroadcastDeathMessage(svg_player_edict_t *victim,
                          svg_base_edict_t *attacker,
                          int means_of_death) {
    const char *message = nullptr;
    
    if (attacker == victim) {
        // Self-inflicted
        switch (means_of_death) {
            case MEANS_OF_DEATH_SUICIDE:
                message = " took the easy way out";
                break;
            case MEANS_OF_DEATH_FALLING:
                message = " cratered";
                break;
            // ... other self-inflicted deaths ...
        }
    } else {
        // Killed by another entity
        switch (means_of_death) {
            case MEANS_OF_DEATH_HIT_PISTOL:
                message = " was shot by ";
                break;
            case MEANS_OF_DEATH_EXPLOSIVE:
                message = " was blown up by ";
                break;
            // ... other deaths ...
        }
    }
    
    // Send to all clients
    gi.bprintf(PRINT_MEDIUM, "%s%s%s\n", 
               victim->client->pers.netname,
               message,
               attacker->client->pers.netname);
}
```

## Best Practices

### Choosing the Right MOD

```cpp
// GOOD: Specific MOD for weapon type
void FireWeapon() {
    Fire_Bullet(/*...*/, MEANS_OF_DEATH_HIT_RIFLE);
}

// BAD: Generic MOD
void FireWeapon() {
    Fire_Bullet(/*...*/, MEANS_OF_DEATH_UNKNOWN);
}
```

### Tracking Kill Statistics

```cpp
void OnKill(svg_player_edict_t *killer, 
            svg_player_edict_t *victim, 
            int means_of_death) {
    // Track weapon kills
    switch (means_of_death) {
        case MEANS_OF_DEATH_HIT_PISTOL:
            killer->stats.pistol_kills++;
            break;
        case MEANS_OF_DEATH_HIT_SNIPER:
            killer->stats.sniper_kills++;
            break;
        // ... track other weapon types ...
    }
}
```

### Custom Death Effects

```cpp
void CustomDeathEffect(svg_base_edict_t *entity, int means_of_death) {
    switch (means_of_death) {
        case MEANS_OF_DEATH_LAVA:
            // Spawn burning effect
            SpawnTempEntity(TE_FLAME, entity->s.origin);
            break;
            
        case MEANS_OF_DEATH_LASER:
            // Vaporize effect
            SpawnTempEntity(TE_ELECTRIC_SPARKS, entity->s.origin);
            break;
            
        case MEANS_OF_DEATH_TELEFRAGGED:
            // Telefrag explosion
            SpawnTempEntity(TE_PLAIN_EXPLOSION, entity->s.origin);
            break;
    }
}
```

## Related Documentation

- [**Entity System Overview**](Entity-System-Overview.md) - Entity damage and death
- [**Temp Entity Overview**](Temp-Entity-Overview.md) - Death effect particles
- [**Server Game Module**](Server-Game-Module.md) - Combat system
- [**API - Entity Events**](API-Entity-Events.md) - Death-related events
- [**Creating Custom Entities**](Creating-Custom-Entities.md) - Implementing damage handlers

## Summary

Means of Death constants identify how entities died or took damage. They are used for:

- Generating appropriate death messages
- Tracking kill statistics by weapon/method
- Triggering death-specific effects (gibs, corpses, particles)
- Implementing friendly fire detection
- Creating custom death behaviors

Always use specific MOD constants (e.g., `MEANS_OF_DEATH_HIT_RIFLE`) rather than generic ones (`MEANS_OF_DEATH_UNKNOWN`) for better gameplay feedback and statistics tracking.
