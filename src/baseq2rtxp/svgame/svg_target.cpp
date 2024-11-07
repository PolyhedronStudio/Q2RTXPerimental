/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "svg_local.h"

/*QUAKED target_temp_entity (1 0 0) (-8 -8 -8) (8 8 8)
Fire an origin based temp entity event to the clients.
"style"     type byte
*/
void Use_Target_Tent( edict_t *ent, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    gi.WriteUint8(svc_temp_entity);
    gi.WriteUint8(ent->style);
    gi.WritePosition( ent->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    gi.multicast( ent->s.origin, MULTICAST_PVS, false );
}

void SP_target_temp_entity(edict_t *ent)
{
    ent->use = Use_Target_Tent;
}


//==========================================================

//==========================================================

/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) looped-on looped-off reliable
"noise"     wav file to play
"attenuation"
-1 = none, send to whole level
1 = normal fighting sounds
2 = idle sound level
3 = ambient sound level
"volume"    0.0 to 1.0

Normal sounds play each time the target is used.  The reliable flag can be set for crucial voiceovers.

Looped sounds are always atten 3 / vol 1, and the use function toggles it on/off.
Multiple identical looping sounds will just increase volume without any speed cost.
*/
void Use_Target_Speaker( edict_t *ent, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    int     chan;

    if (ent->spawnflags & 3) {
        // looping sound toggles
        if (ent->s.sound)
            ent->s.sound = 0;   // turn it off
        else
            ent->s.sound = ent->noise_index;    // start it
    } else {
        // normal sound
        if (ent->spawnflags & 4)
            chan = CHAN_VOICE | CHAN_RELIABLE;
        else
            chan = CHAN_VOICE;
        // use a positioned_sound, because this entity won't normally be
        // sent to any clients because it is invisible
        gi.positioned_sound(ent->s.origin, ent, chan, ent->noise_index, ent->volume, ent->attenuation, 0);
    }
}

void SP_target_speaker(edict_t *ent)
{
    char    buffer[MAX_QPATH];

    if (!st.noise) {
        gi.dprintf("target_speaker with no noise set at %s\n", vtos(ent->s.origin));
        return;
    }
    if (!strstr(st.noise, ".wav"))
        Q_snprintf(buffer, sizeof(buffer), "%s.wav", st.noise);
    else
        Q_strlcpy(buffer, st.noise, sizeof(buffer));
    ent->noise_index = gi.soundindex(buffer);

    if (!ent->volume)
        ent->volume = 1.0f;

    if (!ent->attenuation)
        ent->attenuation = 1.0f;
    else if (ent->attenuation == -1)    // use -1 so 0 defaults to 1
        ent->attenuation = 0;

    // check for prestarted looping sound
    if (ent->spawnflags & 1)
        ent->s.sound = ent->noise_index;

    ent->use = Use_Target_Speaker;

    // Set entity type to ET_TARGET_SPEAKER.
    ent->s.entityType = ET_TARGET_SPEAKER;

    // must link the entity so we get areas and clusters so
    // the server can determine who to send updates to
    gi.linkentity(ent);
}

//==========================================================

/*QUAKED target_secret (1 0 1) (-8 -8 -8) (8 8 8)
Counts a secret found.
These are single use targets.
*/
void use_target_secret( edict_t *ent, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    gi.sound(ent, CHAN_VOICE, ent->noise_index, 1, ATTN_NORM, 0);

    level.found_secrets++;

    SVG_UseTargets(ent, activator);
    SVG_FreeEdict(ent);
}

void SP_target_secret(edict_t *ent)
{
    if (deathmatch->value) {
        // auto-remove for deathmatch
        SVG_FreeEdict(ent);
        return;
    }

    ent->use = use_target_secret;
    if (!st.noise)
        st.noise = "misc/secret.wav";
    ent->noise_index = gi.soundindex(st.noise);
    ent->svflags = SVF_NOCLIENT;
    level.total_secrets++;
    // map bug hack
    if (!Q_stricmp(level.mapname, "mine3") && ent->s.origin[0] == 280 && ent->s.origin[1] == -2048 && ent->s.origin[2] == -624)
        ent->message = const_cast<char*>("You have found a secret area."); // WID: C++20: Added cast.
}

//==========================================================

/*QUAKED target_goal (1 0 1) (-8 -8 -8) (8 8 8)
Counts a goal completed.
These are single use targets.
*/
void use_target_goal( edict_t *ent, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    gi.sound(ent, CHAN_VOICE, ent->noise_index, 1, ATTN_NORM, 0);

    level.found_goals++;

    if (level.found_goals == level.total_goals)
        gi.configstring(CS_CDTRACK, "0");

    SVG_UseTargets(ent, activator);
    SVG_FreeEdict(ent);
}

void SP_target_goal(edict_t *ent)
{
    if (deathmatch->value) {
        // auto-remove for deathmatch
        SVG_FreeEdict(ent);
        return;
    }

    ent->use = use_target_goal;
    if (!st.noise)
        st.noise = "misc/secret.wav";
    ent->noise_index = gi.soundindex(st.noise);
    ent->svflags = SVF_NOCLIENT;
    level.total_goals++;
}

//==========================================================


/*QUAKED target_explosion (1 0 0) (-8 -8 -8) (8 8 8)
Spawns an explosion temporary entity when used.

"delay"     wait this long before going off
"dmg"       how much radius damage should be done, defaults to 0
*/
void target_explosion_explode(edict_t *self)
{
    float       save;

    gi.WriteUint8(svc_temp_entity);
    gi.WriteUint8(TE_EXPLOSION1);
    gi.WritePosition( self->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    gi.multicast( self->s.origin, MULTICAST_PHS, false );

    SVG_RadiusDamage(self, self->activator, self->dmg, NULL, self->dmg + 40, MEANS_OF_DEATH_EXPLOSIVE);

    save = self->delay;
    self->delay = 0;
    SVG_UseTargets(self, self->activator);
    self->delay = save;
}

void use_target_explosion( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    self->activator = activator;

    if (!self->delay) {
        target_explosion_explode(self);
        return;
    }

    self->think = target_explosion_explode;
    self->nextthink = level.time + sg_time_t::from_sec( self->delay );
}

void SP_target_explosion(edict_t *ent)
{
    ent->use = use_target_explosion;
    ent->svflags = SVF_NOCLIENT;
}


//==========================================================

/*QUAKED target_changelevel (1 0 0) (-8 -8 -8) (8 8 8)
Changes level to "map" when fired
*/
void use_target_changelevel( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    if (level.intermission_framenum)
        return;     // already activated

    if (!deathmatch->value && !coop->value) {
        if (g_edicts[1].health <= 0)
            return;
    }

    // if noexit, do a ton of damage to other
    if (deathmatch->value && !((int)dmflags->value & DF_ALLOW_EXIT) && other != world) {
        SVG_TriggerDamage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 10 * other->max_health, 1000, 0, MEANS_OF_DEATH_EXIT );
        return;
    }

    // if multiplayer, let everyone know who hit the exit
    if (deathmatch->value) {
        if (activator && activator->client)
            gi.bprintf(PRINT_HIGH, "%s exited the level.\n", activator->client->pers.netname);
    }

    // if going to a new unit, clear cross triggers
    if (strchr(self->map, '*'))
        game.serverflags &= ~(SFL_CROSS_TRIGGER_MASK);

    SVG_HUD_BeginIntermission(self);
}

void SP_target_changelevel(edict_t *ent)
{
    if (!ent->map) {
        gi.dprintf("target_changelevel with no map at %s\n", vtos(ent->s.origin));
        SVG_FreeEdict(ent);
        return;
    }

    // ugly hack because *SOMEBODY* screwed up their map
    if ((Q_stricmp(level.mapname, "fact1") == 0) && (Q_stricmp(ent->map, "fact3") == 0))
        ent->map = "fact3$secret1";

    ent->use = use_target_changelevel;
    ent->svflags = SVF_NOCLIENT;
}


//==========================================================

/*QUAKED target_splash (1 0 0) (-8 -8 -8) (8 8 8)
Creates a particle splash effect when used.

Set "sounds" to one of the following:
  1) sparks
  2) blue water
  3) brown water
  4) slime
  5) lava
  6) blood

"count" how many pixels in the splash
"dmg"   if set, does a radius damage at this location when it splashes
        useful for lava/sparks
*/

void use_target_splash( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    gi.WriteUint8(svc_temp_entity);
    gi.WriteUint8(TE_SPLASH);
    gi.WriteUint8(self->count);
    gi.WritePosition( self->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    gi.WriteDir8(&self->movedir.x);
    gi.WriteUint8(self->sounds);
    gi.multicast( self->s.origin, MULTICAST_PVS, false );

    if (self->dmg)
        SVG_RadiusDamage(self, activator, self->dmg, NULL, self->dmg + 40, MEANS_OF_DEATH_SPLASH );
}

void SP_target_splash(edict_t *self)
{
    self->use = use_target_splash;
    SVG_SetMoveDir(self->s.angles, self->movedir);

    if (!self->count)
        self->count = 32;

    self->svflags = SVF_NOCLIENT;
}


//==========================================================

/*QUAKED target_spawner (1 0 0) (-8 -8 -8) (8 8 8)
Set target to the type of entity you want spawned.
Useful for spawning monsters and gibs in the factory levels.

For monsters:
    Set direction to the facing you want it to have.

For gibs:
    Set direction if you want it moving and
    speed how fast it should be moving otherwise it
    will just be dropped
*/
void ED_CallSpawn(edict_t *ent);

void use_target_spawner( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    edict_t *ent;

    ent = SVG_AllocateEdict();
    ent->classname = self->targetNames.target;
    VectorCopy(self->s.origin, ent->s.origin);
    VectorCopy(self->s.angles, ent->s.angles);
    ED_CallSpawn(ent);
    gi.unlinkentity(ent);
    KillBox(ent, false);
    gi.linkentity(ent);
    if (self->speed)
        VectorCopy(self->movedir, ent->velocity);
}

void SP_target_spawner(edict_t *self)
{
    self->use = use_target_spawner;
    self->svflags = SVF_NOCLIENT;
    if (self->speed) {
        SVG_SetMoveDir(self->s.angles, self->movedir );
        VectorScale(self->movedir, self->speed, self->movedir);
    }
}

//==========================================================

/*QUAKED target_blaster (1 0 0) (-8 -8 -8) (8 8 8) NOTRAIL NOEFFECTS
Fires a blaster bolt in the set direction when triggered.

dmg     default is 15
speed   default is 1000
*/

void use_target_blaster( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
#if 0
    int effect;

    if (self->spawnflags & 2)
        effect = 0;
    else if (self->spawnflags & 1)
        effect = EF_HYPERBLASTER;
    else
        effect = EF_BLASTER;
#endif

    //fire_blaster(self, self->s.origin, self->movedir, self->dmg, self->speed, EF_BLASTER, MOD_TARGET_BLASTER);
    gi.sound(self, CHAN_VOICE, self->noise_index, 1, ATTN_NORM, 0);
}

void SP_target_blaster(edict_t *self)
{
    self->use = use_target_blaster;
    SVG_SetMoveDir(self->s.angles, self->movedir );
    self->noise_index = gi.soundindex("weapons/laser2.wav");

    if (!self->dmg)
        self->dmg = 15;
    if (!self->speed)
        self->speed = 1000;

    self->svflags = SVF_NOCLIENT;
}


//==========================================================

/*QUAKED target_crosslevel_trigger (.5 .5 .5) (-8 -8 -8) (8 8 8) trigger1 trigger2 trigger3 trigger4 trigger5 trigger6 trigger7 trigger8
Once this trigger is touched/used, any trigger_crosslevel_target with the same trigger number is automatically used when a level is started within the same unit.  It is OK to check multiple triggers.  Message, delay, target, and targetNames.kill also work.
*/
void trigger_crosslevel_trigger_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    game.serverflags |= self->spawnflags;
    SVG_FreeEdict(self);
}

void SP_target_crosslevel_trigger(edict_t *self)
{
    self->svflags = SVF_NOCLIENT;
    self->use = trigger_crosslevel_trigger_use;
}

/*QUAKED target_crosslevel_target (.5 .5 .5) (-8 -8 -8) (8 8 8) trigger1 trigger2 trigger3 trigger4 trigger5 trigger6 trigger7 trigger8
Triggered by a trigger_crosslevel elsewhere within a unit.  If multiple triggers are checked, all must be true.  Delay, target and
targetNames.kill also work.

"delay"     delay before using targets if the trigger has been activated (default 1)
*/
void target_crosslevel_target_think(edict_t *self)
{
    if (self->spawnflags == (game.serverflags & SFL_CROSS_TRIGGER_MASK & self->spawnflags)) {
        SVG_UseTargets(self, self);
        SVG_FreeEdict(self);
    }
}

void SP_target_crosslevel_target(edict_t *self)
{
    if (! self->delay)
        self->delay = 1;
    self->svflags = SVF_NOCLIENT;

    self->think = target_crosslevel_target_think;
    self->nextthink = level.time + sg_time_t::from_sec( self->delay );
}

//==========================================================

/*QUAKED target_laser (0 .5 .8) (-8 -8 -8) (8 8 8) START_ON RED GREEN BLUE YELLOW ORANGE FAT
When triggered, fires a laser.  You can either set a target
or a direction.
*/

void target_laser_think(edict_t *self)
{
    edict_t *ignore;
    vec3_t  start;
    vec3_t  end;
    trace_t tr;
    vec3_t  point;
    vec3_t  last_movedir;
    int     count;

    if (self->spawnflags & 0x80000000)
        count = 8;
    else
        count = 4;

    if (self->enemy) {
        VectorCopy(self->movedir, last_movedir);
        VectorMA(self->enemy->absmin, 0.5f, self->enemy->size, point);
        VectorSubtract(point, self->s.origin, self->movedir);
        self->movedir = QM_Vector3Normalize(self->movedir);
        if (!VectorCompare(self->movedir, last_movedir))
            self->spawnflags |= 0x80000000;
    }

    ignore = self;
    VectorCopy(self->s.origin, start);
    VectorMA(start, 2048, self->movedir, end);
    while (1) {
        tr = gi.trace( start, NULL, NULL, end, ignore, ( CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_DEADMONSTER ) );

        if (!tr.ent)
            break;

        // hurt it if we can
        if ((tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER))
            SVG_TriggerDamage(tr.ent, self, self->activator, &self->movedir.x, tr.endpos, vec3_origin, self->dmg, 1, DAMAGE_ENERGY, MEANS_OF_DEATH_LASER );

        // if we hit something that's not a monster or player or is immune to lasers, we're done
        if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client)) {
            if (self->spawnflags & 0x80000000) {
                self->spawnflags &= ~0x80000000;
                gi.WriteUint8(svc_temp_entity);
                gi.WriteUint8(TE_LASER_SPARKS);
                gi.WriteUint8(count);
                gi.WritePosition( tr.endpos, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
                gi.WriteDir8(tr.plane.normal);
                gi.WriteUint8(self->s.skinnum);
                gi.multicast( tr.endpos, MULTICAST_PVS, false );
            }
            break;
        }

        ignore = tr.ent;
        VectorCopy(tr.endpos, start);
    }

    VectorCopy(tr.endpos, self->s.old_origin);

    self->nextthink = level.time + FRAME_TIME_S;
}

void target_laser_on(edict_t *self)
{
    if (!self->activator)
        self->activator = self;
    self->spawnflags |= 0x80000001;
    self->svflags &= ~SVF_NOCLIENT;
    target_laser_think(self);
}

void target_laser_off(edict_t *self)
{
    self->spawnflags &= ~1;
    self->svflags |= SVF_NOCLIENT;
    self->nextthink = 0_ms;
}

void target_laser_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    self->activator = activator;
    if (self->spawnflags & 1)
        target_laser_off(self);
    else
        target_laser_on(self);
}

void target_laser_start(edict_t *self)
{
    edict_t *ent;

    self->movetype = MOVETYPE_NONE;
    self->solid = SOLID_NOT;
    self->s.entityType = ET_BEAM;
    self->s.renderfx |= RF_BEAM | RF_TRANSLUCENT;
    self->s.modelindex = 1;         // must be non-zero

    // set the beam diameter
    if (self->spawnflags & 64)
        self->s.frame = 16;
    else
        self->s.frame = 4;

    // set the color
    if (self->spawnflags & 2)
        self->s.skinnum = 0xf2f2f0f0;
    else if (self->spawnflags & 4)
        self->s.skinnum = 0xd0d1d2d3;
    else if (self->spawnflags & 8)
        self->s.skinnum = 0xf3f3f1f1;
    else if (self->spawnflags & 16)
        self->s.skinnum = 0xdcdddedf;
    else if (self->spawnflags & 32)
        self->s.skinnum = 0xe0e1e2e3;

    if (!self->enemy) {
        if (self->targetNames.target) {
            ent = SVG_Find(NULL, FOFS(targetname), self->targetNames.target);
            if (!ent)
                gi.dprintf("%s at %s: %s is a bad target\n", self->classname, vtos(self->s.origin), self->targetNames.target);
            self->enemy = ent;
        } else {
            SVG_SetMoveDir(self->s.angles, self->movedir );
        }
    }
    self->use = target_laser_use;
    self->think = target_laser_think;

    if (!self->dmg)
        self->dmg = 1;

    VectorSet(self->mins, -8, -8, -8);
    VectorSet(self->maxs, 8, 8, 8);
    gi.linkentity(self);

    if (self->spawnflags & 1)
        target_laser_on(self);
    else
        target_laser_off(self);
}

void SP_target_laser(edict_t *self)
{
    // let everything else get spawned before we start firing
    self->think = target_laser_start;
    self->nextthink = level.time + 1_sec;
}

//==========================================================

/*QUAKED target_lightramp (0 .5 .8) (-8 -8 -8) (8 8 8) TOGGLE
speed       How many seconds the ramping will take
message     two letters; starting lightlevel and ending lightlevel
*/

void target_lightramp_think(edict_t *self)
{
    char    style[2];

	style[ 0 ] = (char)( 'a' + self->movedir[ 0 ] + ( ( level.time - self->timestamp ) / gi.frame_time_s ).seconds( ) * self->movedir[ 2 ] );
    style[1] = 0;
    gi.configstring(CS_LIGHTS + self->enemy->style, style);

	if ( ( level.time - self->timestamp ).seconds( ) < self->speed ) {
		self->nextthink = level.time + FRAME_TIME_S;
	} else if (self->spawnflags & 1) {
        char    temp;

        temp = self->movedir[0];
        self->movedir[0] = self->movedir[1];
        self->movedir[1] = temp;
        self->movedir[2] *= -1;
    }
}

void target_lightramp_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    if (!self->enemy) {
        edict_t     *e;

        // check all the targets
        e = NULL;
        while (1) {
            e = SVG_Find(e, FOFS(targetname), self->targetNames.target);
            if (!e)
                break;
            if (strcmp(e->classname, "light") != 0) {
                gi.dprintf("%s at %s ", self->classname, vtos(self->s.origin));
                gi.dprintf("target %s (%s at %s) is not a light\n", self->targetNames.target, e->classname, vtos(e->s.origin));
            } else {
                self->enemy = e;
            }
        }

        if (!self->enemy) {
            gi.dprintf("%s target %s not found at %s\n", self->classname, self->targetNames.target, vtos(self->s.origin));
            SVG_FreeEdict(self);
            return;
        }
    }

    self->timestamp = level.time;
    target_lightramp_think(self);
}

void SP_target_lightramp(edict_t *self)
{
    if (!self->message || strlen(self->message) != 2 || self->message[0] < 'a' || self->message[0] > 'z' || self->message[1] < 'a' || self->message[1] > 'z' || self->message[0] == self->message[1]) {
        gi.dprintf("target_lightramp has bad ramp (%s) at %s\n", self->message, vtos(self->s.origin));
        SVG_FreeEdict(self);
        return;
    }

    if (deathmatch->value) {
        SVG_FreeEdict(self);
        return;
    }

    if (!self->targetNames.target) {
        gi.dprintf("%s with no target at %s\n", self->classname, vtos(self->s.origin));
        SVG_FreeEdict(self);
        return;
    }

    self->svflags |= SVF_NOCLIENT;
    self->use = target_lightramp_use;
    self->think = target_lightramp_think;

    self->movedir[0] = self->message[0] - 'a';
    self->movedir[1] = self->message[1] - 'a';
    self->movedir[2] = (self->movedir[1] - self->movedir[0]) / self->speed;
}

//==========================================================

/*QUAKED target_earthquake (1 0 0) (-8 -8 -8) (8 8 8)
When triggered, this initiates a level-wide earthquake.
All players and monsters are affected.
"speed"     severity of the quake (default:200)
"count"     duration of the quake (default:5)
*/

void target_earthquake_think(edict_t *self)
{
    int     i;
    edict_t *e;

    if (self->last_move_time < level.time) {
        gi.positioned_sound(self->s.origin, self, CHAN_AUTO, self->noise_index, 1.0f, ATTN_NONE, 0);
		self->last_move_time = level.time + 6.5_sec;
    }

    for ( i = 1, e = g_edicts + i; i < globals.num_edicts; i++, e++ ) {
        if ( !e->inuse )
            continue;
        if ( !e->client )
            break;

        e->client->viewMove.quakeTime = level.time + 1000_ms;
    }

    if ( level.time < self->timestamp )
        self->nextthink = level.time + 10_hz;
}

void target_earthquake_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
	//self->timestamp = level.time + sg_time_t::from_sec( self->count );
	//self->nextthink = level.time + FRAME_TIME_S;
	//self->last_move_time = 0_ms;
 //   self->activator = activator;
	if ( self->spawnflags & 8 /*.has( SPAWNFLAGS_EARTHQUAKE_ONE_SHOT )*/ ) {
		uint32_t i;
		edict_t *e;

		for ( i = 1, e = g_edicts + i; i < globals.num_edicts; i++, e++ ) {
			if ( !e->inuse )
				continue;
			if ( !e->client )
				break;

			e->client->viewMove.damagePitch = -self->speed * 0.1f;
			e->client->viewMove.damageTime = level.time + DAMAGE_TIME( );
		}

		return;
	}

	self->timestamp = level.time + sg_time_t::from_sec( self->count );

	if ( self->spawnflags & 2 /*SPAWNFLAGS_EARTHQUAKE_TOGGLE*/ ) {
		if ( self->style )
			self->nextthink = 0_ms;
		else
			self->nextthink = level.time + FRAME_TIME_S;

		self->style = !self->style;
	} else {
		self->nextthink = level.time + FRAME_TIME_S;
		self->last_move_time = 0_ms;
	}

	self->activator = activator;
}

void SP_target_earthquake(edict_t *self)
{
    if (!self->targetname)
        gi.dprintf("untargeted %s at %s\n", self->classname, vtos(self->s.origin));

    if (!self->count)
        self->count = 5;

    if (!self->speed)
        self->speed = 200;

    self->svflags |= SVF_NOCLIENT;
    self->think = target_earthquake_think;
    self->use = target_earthquake_use;

    self->noise_index = gi.soundindex("world/quake.wav");
}
