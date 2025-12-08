# Vanilla Quake 2 Weapon System

The Quake 2 weapon system handles player weapons, firing, ammunition, and weapon pickup. Understanding weapons is essential for combat gameplay and item systems.

**Primary Files:**
- `src/baseq2rtxp/svgame/svg_weapon.cpp` - Core weapon logic
- `src/baseq2rtxp/svgame/player/svg_player_weapon.cpp` - Player weapon management
- `src/baseq2rtxp/svgame/weapons/` - Individual weapon implementations

## Overview

Quake 2 weapons are item-based entities that players can pick up, equip, and fire. The weapon system handles:

1. **Weapon items** - Pickable entities in the world
2. **Weapon switching** - Changing between weapons
3. **Firing mechanics** - Primary/secondary fire
4. **Ammunition** - Ammo tracking and consumption
5. **Weapon animations** - View model animations
6. **Projectiles** - Spawning rockets, grenades, etc.
7. **Hitscan** - Instant-hit weapons (guns)

## Weapon Architecture

### Item System Integration

Weapons are **items** (`gitem_t`) that players carry in inventory:

```cpp
// Weapon as inventory item
typedef struct gitem_s {
    char        *classname;         // "weapon_shotgun"
    bool        (*pickup)(/*...*/); // Pickup function
    void        (*use)(/*...*/);    // Use/equip function
    void        (*drop)(/*...*/);   // Drop function
    void        (*weaponthink)(/*...*/);  // Weapon logic
    
    char        *pickup_sound;      // Sound when picked up
    char        *world_model;       // World model (3rd person)
    int         world_model_flags;  // EF_ROTATE, etc.
    char        *view_model;        // View model (1st person)
    
    char        *icon;              // HUD icon
    char        *pickup_name;       // "Shotgun"
    int         count_width;        // HUD ammo display width
    
    int         quantity;           // Ammo given on pickup
    char        *ammo;              // Ammo type used
    int         flags;              // IT_WEAPON, IT_AMMO, etc.
    int         weapmodel;          // Weapon model index
    
    // Sounds
    void        *info;              // Weapon-specific data
    int         tag;                // For networking
    
    char        *precaches;         // Assets to precache
} gitem_t;
```

### Weapon State (Client-Side)

Each player has weapon state in their `gclient_t`:

```cpp
// Player weapon state (ent->client)
struct {
    gitem_t     *pers.weapon;           // Current weapon
    gitem_t     *pers.lastweapon;       // Previous weapon
    gitem_t     *newweapon;             // Weapon to switch to
    
    int         pers.inventory[MAX_ITEMS];  // Item counts (ammo, weapons)
    
    // Weapon animation
    int         ps.gunframe;            // Current anim frame
    int         ps.gunindex;            // Weapon model index
    
    // Weapon timing
    int         weaponstate;            // WEAPON_READY, WEAPON_FIRING, etc.
    
} client_t;
```

### Weapon States

```cpp
// Weapon state machine
#define WEAPON_READY        0   // Idle, can fire
#define WEAPON_ACTIVATING   1   // Raising weapon
#define WEAPON_DROPPING     2   // Lowering weapon (switching)
#define WEAPON_FIRING       3   // Firing animation
```

## Weapon Pickup

### Weapon Item Entity

Weapons spawn as item entities in the world:

```cpp
void SP_weapon_shotgun(svg_base_edict_t *self) {
    // Spawn weapon pickup entity
    self->model = "models/weapons/g_shotg/tris.md2";
    self->classname = "weapon_shotgun";
    
    // Set as item
    gitem_t *item = FindItem("Shotgun");
    self->item = item;
    
    // Touch to pickup
    self->touch = Touch_Item;
    self->movetype = MOVETYPE_TOSS;
    self->solid = SOLID_TRIGGER;
    
    // Drop to floor
    SVG_DropToFloor(self);
}
```

### Pickup Function

When player touches weapon:

```cpp
bool Pickup_Weapon(svg_base_edict_t *ent, svg_base_edict_t *other) {
    int index = ITEM_INDEX(ent->item);
    
    // Already have this weapon?
    if (other->client->pers.inventory[index]) {
        // Just add ammo
        if (!(ent->spawnflags & DROPPED_ITEM)) {
            // Fresh spawn, give ammo
            Add_Ammo(other, ent->item->ammo, ent->item->quantity);
        }
        
        if (!(ent->spawnflags & DROPPED_ITEM)) {
            // Don't respawn
            return false;
        }
    } else {
        // New weapon, add to inventory
        other->client->pers.inventory[index] = 1;
        
        // Give ammo
        if (!(ent->spawnflags & DROPPED_ITEM)) {
            if (deathmatch->value) {
                // DM: less ammo
                if (ent->item->ammo) {
                    Add_Ammo(other, ent->item->ammo, ent->item->quantity);
                }
            } else {
                // SP: full ammo
                if (ent->item->ammo) {
                    Add_Ammo(other, ent->item->ammo, ent->item->quantity * 2);
                }
            }
        }
        
        // Auto-switch to new weapon
        if (!(ent->item->flags & IT_STAY_COOP) || 
            (ent->spawnflags & DROPPED_ITEM)) {
            other->client->newweapon = ent->item;
        }
    }
    
    return true;  // Remove pickup
}
```

## Weapon Switching

### Initiating Switch

When player selects new weapon:

```cpp
void Use_Weapon(svg_base_edict_t *ent, gitem_t *item) {
    // Already using this weapon?
    if (item == ent->client->pers.weapon) {
        return;
    }
    
    // Have this weapon?
    int index = ITEM_INDEX(item);
    if (!ent->client->pers.inventory[index]) {
        gi.cprintf(ent, PRINT_HIGH, "No weapon\n");
        return;
    }
    
    // Set new weapon
    ent->client->newweapon = item;
}
```

### Switch Process

Weapon switching happens over multiple frames:

```cpp
void ChangeWeapon(svg_base_edict_t *ent) {
    // No new weapon?
    if (!ent->client->newweapon) {
        return;
    }
    
    // Already changing?
    if (ent->client->weaponstate == WEAPON_DROPPING) {
        return;
    }
    
    // Start lowering current weapon
    ent->client->weaponstate = WEAPON_DROPPING;
    ent->client->ps.gunframe = 0;  // Start drop anim
}

void WeaponThink(svg_base_edict_t *ent) {
    if (ent->client->weaponstate == WEAPON_DROPPING) {
        // Advance drop animation
        ent->client->ps.gunframe++;
        
        if (ent->client->ps.gunframe >= 15) {
            // Drop complete, switch weapon
            ent->client->weaponstate = WEAPON_ACTIVATING;
            ent->client->pers.lastweapon = ent->client->pers.weapon;
            ent->client->pers.weapon = ent->client->newweapon;
            ent->client->newweapon = NULL;
            ent->client->ps.gunframe = 0;  // Start raise anim
            
            // Set weapon model
            if (ent->client->pers.weapon) {
                ent->client->ps.gunindex = 
                    gi.modelindex(ent->client->pers.weapon->view_model);
            }
        }
    } else if (ent->client->weaponstate == WEAPON_ACTIVATING) {
        // Advance raise animation
        ent->client->ps.gunframe++;
        
        if (ent->client->ps.gunframe >= 15) {
            // Raise complete, ready to fire
            ent->client->weaponstate = WEAPON_READY;
            ent->client->ps.gunframe = 0;
        }
    }
}
```

## Firing Mechanics

### Fire Button

Player presses fire button (attack):

```cpp
void ClientThink(svg_base_edict_t *ent, usercmd_t *ucmd) {
    // Check attack button
    if (ucmd->buttons & BUTTON_ATTACK) {
        if (!ent->client->latched_buttons & BUTTON_ATTACK) {
            // Just pressed attack
            ent->client->latched_buttons |= BUTTON_ATTACK;
            
            if (ent->client->weaponstate == WEAPON_READY) {
                // Fire weapon
                Weapon_Fire(ent);
            }
        }
    } else {
        // Released attack
        ent->client->latched_buttons &= ~BUTTON_ATTACK;
    }
}
```

### Weapon Fire Function

Each weapon has its own fire logic:

```cpp
void Weapon_Shotgun_Fire(svg_base_edict_t *ent) {
    // Check ammo
    int ammo_index = ITEM_INDEX(FindItem("Shells"));
    if (ent->client->pers.inventory[ammo_index] < 1) {
        // Out of ammo, click
        gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/noammo.wav"), 
                1, ATTN_NORM, 0);
        ent->client->ps.gunframe = 0;
        return;
    }
    
    // Consume ammo
    ent->client->pers.inventory[ammo_index]--;
    
    // Start firing animation
    ent->client->weaponstate = WEAPON_FIRING;
    ent->client->ps.gunframe = 1;
    
    // Fire projectiles/hitscan
    vec3_t forward, right, up;
    AngleVectors(ent->client->v_angle, forward, right, up);
    
    vec3_t start;
    G_ProjectSource(ent->s.origin, vec3_t{24, 8, -8}, 
                   forward, right, start);
    
    // Fire 12 pellets
    for (int i = 0; i < 12; i++) {
        vec3_t dir;
        VectorCopy(forward, dir);
        
        // Add spread
        dir[0] += crandom() * 0.04;
        dir[1] += crandom() * 0.04;
        dir[2] += crandom() * 0.04;
        
        // Hitscan
        fire_bullet(ent, start, dir, 4, 4, 0, 0, MOD_SHOTGUN);
    }
    
    // Muzzle flash
    gi.WriteByte(svc_muzzleflash);
    gi.WriteShort(ent - g_edicts);
    gi.WriteByte(MZ_SHOTGUN);
    gi.multicast(ent->s.origin, MULTICAST_PVS);
    
    // Kick
    ent->client->kick_angles[0] = -2;
}
```

## Projectile Weapons

### Rocket Launcher

Spawns physical projectile entity:

```cpp
void Weapon_RocketLauncher_Fire(svg_base_edict_t *ent) {
    // Check ammo
    int ammo_index = ITEM_INDEX(FindItem("Rockets"));
    if (ent->client->pers.inventory[ammo_index] < 1) {
        // No ammo
        gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/noammo.wav"),
                1, ATTN_NORM, 0);
        ent->client->ps.gunframe = 0;
        return;
    }
    
    // Consume ammo
    ent->client->pers.inventory[ammo_index]--;
    
    // Calculate start position
    vec3_t forward, right, up;
    AngleVectors(ent->client->v_angle, forward, right, up);
    
    vec3_t start;
    G_ProjectSource(ent->s.origin, vec3_t{8, 8, -8},
                   forward, right, start);
    
    // Spawn rocket
    fire_rocket(ent, start, forward, 100, 650, 120, 120);
    
    // Muzzle flash
    gi.WriteByte(svc_muzzleflash);
    gi.WriteShort(ent - g_edicts);
    gi.WriteByte(MZ_ROCKET);
    gi.multicast(ent->s.origin, MULTICAST_PVS);
    
    // Animation
    ent->client->ps.gunframe++;
    
    // Kick
    ent->client->kick_angles[0] = -2;
}

void fire_rocket(svg_base_edict_t *self, vec3_t start, vec3_t dir,
                int damage, int speed, float damage_radius, 
                int radius_damage) {
    // Create rocket entity
    svg_base_edict_t *rocket = G_Spawn();
    VectorCopy(start, rocket->s.origin);
    VectorCopy(dir, rocket->movedir);
    VectorNormalize(rocket->movedir);
    VectorScale(dir, speed, rocket->velocity);
    
    rocket->movetype = MOVETYPE_FLYMISSILE;
    rocket->clipMask = MASK_PROJECTILE;
    rocket->solid = SOLID_BBOX;
    rocket->s.effects |= EF_ROCKET;
    
    VectorClear(rocket->mins);
    VectorClear(rocket->maxs);
    rocket->s.modelindex = gi.modelindex("models/objects/rocket/tris.md2");
    rocket->owner = self;
    rocket->touch = rocket_touch;
    rocket->nextthink = level.time + 8_sec;  // Max flight time
    rocket->think = G_FreeEdict;
    rocket->dmg = damage;
    rocket->radius_dmg = radius_damage;
    rocket->dmg_radius = damage_radius;
    rocket->s.sound = gi.soundindex("weapons/rockfly.wav");
    rocket->classname = "rocket";
    
    gi.linkentity(rocket);
}

void rocket_touch(svg_base_edict_t *ent, svg_base_edict_t *other, ...) {
    // Explode on contact
    if (other == ent->owner) {
        return;  // Don't hit owner
    }
    
    if (other->takedamage) {
        // Direct hit
        T_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin,
                vec3_origin, ent->dmg, 0, 0, MOD_ROCKET);
    }
    
    // Radius damage
    T_RadiusDamage(ent, ent->owner, ent->radius_dmg, other,
                  ent->dmg_radius, MOD_R_SPLASH);
    
    // Explosion effect
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_EXPLOSION1);
    gi.WritePosition(ent->s.origin);
    gi.multicast(ent->s.origin, MULTICAST_PHS);
    
    // Remove rocket
    G_FreeEdict(ent);
}
```

### Grenade Launcher

Spawns bouncing grenade:

```cpp
void fire_grenade(svg_base_edict_t *self, vec3_t start, vec3_t aimdir,
                 int damage, int speed, float timer, float damage_radius) {
    // Create grenade
    svg_base_edict_t *grenade = G_Spawn();
    VectorCopy(start, grenade->s.origin);
    VectorScale(aimdir, speed, grenade->velocity);
    
    // Add arc
    grenade->velocity[2] += 200 + crandom() * 10.0;
    
    // Random spin
    VectorSet(grenade->avelocity, 300, 300, 300);
    
    grenade->movetype = MOVETYPE_BOUNCE;  // Bounce off surfaces
    grenade->clipMask = MASK_PROJECTILE;
    grenade->solid = SOLID_BBOX;
    grenade->s.effects |= EF_GRENADE;
    
    VectorClear(grenade->mins);
    VectorClear(grenade->maxs);
    grenade->s.modelindex = gi.modelindex("models/objects/grenade/tris.md2");
    grenade->owner = self;
    grenade->touch = Grenade_Touch;
    grenade->nextthink = level.time + timer;
    grenade->think = Grenade_Explode;
    grenade->dmg = damage;
    grenade->dmg_radius = damage_radius;
    grenade->classname = "grenade";
    
    gi.linkentity(grenade);
}

void Grenade_Touch(svg_base_edict_t *ent, svg_base_edict_t *other, ...) {
    // Bounce off surfaces
    if (other == ent->owner) {
        return;
    }
    
    if (other->takedamage) {
        // Hit monster, explode immediately
        Grenade_Explode(ent);
        return;
    }
    
    // Play bounce sound
    gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/grenlb1b.wav"),
            1, ATTN_NORM, 0);
}

void Grenade_Explode(svg_base_edict_t *ent) {
    // Radius damage
    T_RadiusDamage(ent, ent->owner, ent->dmg, NULL,
                  ent->dmg_radius, MOD_G_SPLASH);
    
    // Explosion effect
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_EXPLOSION1);
    gi.WritePosition(ent->s.origin);
    gi.multicast(ent->s.origin, MULTICAST_PHS);
    
    G_FreeEdict(ent);
}
```

## Hitscan Weapons

### Bullet Tracing

Instant-hit weapons use ray tracing:

```cpp
void fire_bullet(svg_base_edict_t *self, vec3_t start, vec3_t aimdir,
                int damage, int kick, int hspread, int vspread, int mod) {
    // Apply spread
    vec3_t dir;
    VectorCopy(aimdir, dir);
    
    dir[0] += crandom() * hspread;
    dir[1] += crandom() * vspread;
    dir[2] += crandom() * hspread;
    
    VectorNormalize(dir);
    
    // Trace ray
    vec3_t end;
    VectorMA(start, 8192, dir, end);
    
    svg_trace_t tr = gi.trace(start, NULL, NULL, end, self, MASK_SHOT);
    
    if (tr.fraction < 1.0) {
        // Hit something
        if (tr.ent->takedamage) {
            // Damage entity
            T_Damage(tr.ent, self, self, dir, tr.endpos, tr.plane.normal,
                    damage, kick, DAMAGE_BULLET, mod);
        } else {
            // Hit wall
            if (strncmp(tr.surface->name, "sky", 3) != 0) {
                // Not sky, show impact
                gi.WriteByte(svc_temp_entity);
                gi.WriteByte(TE_GUNSHOT);
                gi.WritePosition(tr.endpos);
                gi.WriteDir(tr.plane.normal);
                gi.multicast(tr.endpos, MULTICAST_PVS);
            }
        }
    }
    
    // Player noise (for AI)
    PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
}
```

## Ammunition System

### Ammo Types

```cpp
// Ammo items
gitem_t itemlist[] = {
    // ...
    {
        "item_shells",
        Pickup_Ammo,
        NULL, NULL,
        NULL,
        "misc/am_pkup.wav",
        "models/items/ammo/shells/medium/tris.md2", 0,
        NULL,
        "i_shells",
        "Shells",
        3, 10,
        NULL,
        IT_AMMO,
        0,
        NULL,
        AMMO_SHELLS,
        ""
    },
    {
        "item_bullets",
        Pickup_Ammo,
        NULL, NULL,
        NULL,
        "misc/am_pkup.wav",
        "models/items/ammo/bullets/medium/tris.md2", 0,
        NULL,
        "i_bullets",
        "Bullets",
        3, 50,
        NULL,
        IT_AMMO,
        0,
        NULL,
        AMMO_BULLETS,
        ""
    },
    // ... rockets, cells, grenades, slugs
};
```

### Ammo Management

```cpp
bool Add_Ammo(svg_base_edict_t *ent, gitem_t *item, int count) {
    if (!ent->client) {
        return false;
    }
    
    if (item->tag == AMMO_BULLETS) {
        max = ent->client->pers.max_bullets;
    } else if (item->tag == AMMO_SHELLS) {
        max = ent->client->pers.max_shells;
    } else if (item->tag == AMMO_ROCKETS) {
        max = ent->client->pers.max_rockets;
    } else if (item->tag == AMMO_GRENADES) {
        max = ent->client->pers.max_grenades;
    } else if (item->tag == AMMO_CELLS) {
        max = ent->client->pers.max_cells;
    } else if (item->tag == AMMO_SLUGS) {
        max = ent->client->pers.max_slugs;
    } else {
        return false;
    }
    
    int index = ITEM_INDEX(item);
    
    if (ent->client->pers.inventory[index] >= max) {
        return false;  // Already at max
    }
    
    ent->client->pers.inventory[index] += count;
    
    if (ent->client->pers.inventory[index] > max) {
        ent->client->pers.inventory[index] = max;
    }
    
    return true;
}
```

## Muzzle Flash

Visual effect when weapon fires:

```cpp
// Muzzle flash types
#define MZ_BLASTER          0
#define MZ_MACHINEGUN       1
#define MZ_SHOTGUN          2
#define MZ_CHAINGUN1        3
#define MZ_CHAINGUN2        4
#define MZ_CHAINGUN3        5
#define MZ_RAILGUN          6
#define MZ_ROCKET           7
#define MZ_GRENADE          8
#define MZ_LOGIN            9
#define MZ_LOGOUT           10
#define MZ_RESPAWN          11
#define MZ_BFG              12
#define MZ_SSHOTGUN         13
#define MZ_HYPERBLASTER     14
#define MZ_ITEMRESPAWN      15

// Send muzzle flash
void ShowGun(svg_base_edict_t *ent, int flash_number) {
    gi.WriteByte(svc_muzzleflash);
    gi.WriteShort(ent - g_edicts);
    gi.WriteByte(flash_number);
    gi.multicast(ent->s.origin, MULTICAST_PVS);
}
```

## Weapon Animation

View weapon animation frames:

```cpp
// Frame sequences
#define FRAME_ACTIVATE_FIRST    0
#define FRAME_ACTIVATE_LAST     14
#define FRAME_IDLE              15
#define FRAME_FIRE_FIRST        16
#define FRAME_FIRE_LAST         20
#define FRAME_DEACTIVATE_FIRST  21
#define FRAME_DEACTIVATE_LAST   35

void Weapon_Generic(svg_base_edict_t *ent, int FRAME_FIRE_FIRST,
                   int FRAME_FIRE_LAST, int FRAME_IDLE_LAST,
                   int *pause_frames, int *fire_frames,
                   void (*fire)(svg_base_edict_t *ent)) {
    int n;
    
    if (ent->client->weaponstate == WEAPON_DROPPING) {
        if (ent->client->ps.gunframe == FRAME_DEACTIVATE_LAST) {
            ChangeWeapon(ent);
            return;
        }
        ent->client->ps.gunframe++;
        return;
    }
    
    if (ent->client->weaponstate == WEAPON_ACTIVATING) {
        if (ent->client->ps.gunframe == FRAME_ACTIVATE_LAST) {
            ent->client->weaponstate = WEAPON_READY;
            ent->client->ps.gunframe = FRAME_IDLE;
            return;
        }
        ent->client->ps.gunframe++;
        return;
    }
    
    if ((ent->client->newweapon) && 
        (ent->client->weaponstate != WEAPON_FIRING)) {
        ent->client->weaponstate = WEAPON_DROPPING;
        ent->client->ps.gunframe = FRAME_DEACTIVATE_FIRST;
        return;
    }
    
    if (ent->client->weaponstate == WEAPON_READY) {
        // Check for fire
        if (ent->client->latched_buttons & BUTTON_ATTACK) {
            ent->client->latched_buttons &= ~BUTTON_ATTACK;
            ent->client->weaponstate = WEAPON_FIRING;
            ent->client->ps.gunframe = FRAME_FIRE_FIRST;
        } else {
            ent->client->ps.gunframe = FRAME_IDLE;
        }
        return;
    }
    
    if (ent->client->weaponstate == WEAPON_FIRING) {
        // Fire on specific frames
        for (n = 0; fire_frames[n]; n++) {
            if (ent->client->ps.gunframe == fire_frames[n]) {
                fire(ent);
                break;
            }
        }
        
        if (ent->client->ps.gunframe == FRAME_FIRE_LAST) {
            ent->client->weaponstate = WEAPON_READY;
            ent->client->ps.gunframe = FRAME_IDLE;
        } else {
            ent->client->ps.gunframe++;
        }
    }
}
```

## Weapon Dropping

When player dies or manually drops weapon:

```cpp
void Drop_Weapon(svg_base_edict_t *ent, gitem_t *item) {
    int index = ITEM_INDEX(item);
    
    // Create dropped item
    svg_base_edict_t *dropped = Drop_Item(ent, item);
    
    // Remove from inventory
    ent->client->pers.inventory[index] = 0;
    ent->client->pers.weapon = NULL;
    
    // Mark as player-dropped
    dropped->spawnflags = DROPPED_ITEM | DROPPED_PLAYER_ITEM;
    
    // Toss forward
    vec3_t forward;
    AngleVectors(ent->client->v_angle, forward, NULL, NULL);
    VectorScale(forward, 100, dropped->velocity);
    dropped->velocity[2] = 300;
}
```

## Best Practices

### 1. Check Ammo Before Firing

```cpp
// Always check ammo
if (ent->client->pers.inventory[ammo_index] < ammo_cost) {
    // Play empty click sound
    gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/noammo.wav"),
            1, ATTN_NORM, 0);
    ent->client->ps.gunframe = 0;
    return;
}
```

### 2. Use Correct Projectile Movement Type

```cpp
// Fast projectiles: MOVETYPE_FLYMISSILE
rocket->movetype = MOVETYPE_FLYMISSILE;

// Bouncing projectiles: MOVETYPE_BOUNCE
grenade->movetype = MOVETYPE_BOUNCE;

// Ballistic arc: MOVETYPE_TOSS
thrown->movetype = MOVETYPE_TOSS;
```

### 3. Set Owner to Avoid Self-Damage

```cpp
// Set owner to prevent hitting self
projectile->owner = self;

// In touch callback
if (other == ent->owner) {
    return;  // Don't hit owner
}
```

### 4. Use Correct Damage Flags

```cpp
// Direct hit with knockback
T_Damage(target, inflictor, attacker, dir, point, normal,
        damage, kick, 0, MOD_ROCKET);

// Bullet damage (no armor penetration on some armor)
T_Damage(target, inflictor, attacker, dir, point, normal,
        damage, kick, DAMAGE_BULLET, MOD_MACHINEGUN);

// Radius damage (ignores armor)
T_Damage(target, inflictor, attacker, dir, point, normal,
        damage, kick, DAMAGE_RADIUS, MOD_R_SPLASH);
```

## Related Documentation

- [**Entity Lifecycle**](Entity-Lifecycle.md) - Projectile spawning
- [**Vanilla Physics**](Vanilla-Physics.md) - Projectile movement
- [**API Means of Death**](API-Means-Of-Death.md) - Damage types
- [**Temp Entity Events**](Temp-Entity-Event-Types.md) - Muzzle flash, impacts

## Summary

The Quake 2 weapon system provides:

- **Item-based weapons** integrated with inventory
- **Weapon switching** with raise/lower animations
- **Projectile spawning** for rockets, grenades, etc.
- **Hitscan tracing** for instant-hit weapons
- **Ammunition management** with different ammo types
- **Muzzle flash** and visual feedback
- **Weapon animations** synchronized with firing

Understanding weapons is essential for:
- Creating custom weapons
- Balancing combat
- Implementing special attacks
- Designing projectile behavior
