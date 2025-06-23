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
#include "svgame/svg_local.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "svgame/player/svg_player_hud.h"

#include "sharedgame/sg_means_of_death.h"
#include "sharedgame/sg_tempentity_events.h"

/*QUAKED target_temp_entity (1 0 0) (-8 -8 -8) (8 8 8)
Fire an origin based temp entity event to the clients.
"style"     type byte
*/
void Use_Target_Tent( svg_base_edict_t *ent, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    gi.WriteUint8(svc_temp_entity);
    gi.WriteUint8(ent->style);
    gi.WritePosition( ent->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    gi.multicast( ent->s.origin, MULTICAST_PVS, false );
}

void SP_target_temp_entity(svg_base_edict_t *ent)
{
    ent->SetUseCallback( Use_Target_Tent );
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
void Use_Target_Speaker( svg_base_edict_t *ent, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
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

void SP_target_speaker( svg_base_edict_t *ent ) {
    if ( !ent->noisePath.ptr || !ent->noisePath.count ) {
        gi.dprintf( "target_speaker with no noise set at %s\n", vtos( ent->s.origin ) );
        return;
    }
    std::string filePath = (const char *)ent->noisePath;
    if ( !strstr( (const char *)ent->noisePath, ".wav" ) ) {
        filePath += ".wav";
    }
    ent->noise_index = gi.soundindex(filePath.c_str());

    if (!ent->volume)
        ent->volume = 1.0f;

    if (!ent->attenuation)
        ent->attenuation = 1.0f;
    else if (ent->attenuation == -1)    // use -1 so 0 defaults to 1
        ent->attenuation = 0;

    // check for prestarted looping sound
    if (ent->spawnflags & 1)
        ent->s.sound = ent->noise_index;

    ent->SetUseCallback( Use_Target_Speaker );

    // Set entity type to ET_TARGET_SPEAKER.
    ent->s.entityType = ET_TARGET_SPEAKER;

    // must link the entity so we get areas and clusters so
    // the server can determine who to send updates to
    gi.linkentity(ent);
}

//==========================================================

/*QUAKED target_explosion (1 0 0) (-8 -8 -8) (8 8 8)
Spawns an explosion temporary entity when used.

"delay"     wait this long before going off
"dmg"       how much radius damage should be done, defaults to 0
*/
void target_explosion_explode(svg_base_edict_t *self)
{
    float       save;

    gi.WriteUint8(svc_temp_entity);
    gi.WriteUint8(TE_PLAIN_EXPLOSION);
    gi.WritePosition( self->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    gi.multicast( self->s.origin, MULTICAST_PHS, false );

    SVG_RadiusDamage(self, self->activator, self->dmg, NULL, self->dmg + 40, MEANS_OF_DEATH_EXPLOSIVE);

    save = self->delay;
    self->delay = 0;
    SVG_UseTargets(self, self->activator);
    self->delay = save;
}

void use_target_explosion( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    self->activator = activator;

    if (!self->delay) {
        target_explosion_explode(self);
        return;
    }

    self->SetThinkCallback( target_explosion_explode );
    self->nextthink = level.time + QMTime::FromSeconds( self->delay );
}

void SP_target_explosion(svg_base_edict_t *ent)
{
    ent->SetUseCallback( use_target_explosion );
    ent->svflags = SVF_NOCLIENT;
}


//==========================================================

/*QUAKED target_changelevel (1 0 0) (-8 -8 -8) (8 8 8)
Changes level to "map" when fired
*/
//void use_target_changelevel( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
//    if (level.intermissionFrameNumber)
//        return;     // already activated
//
//    if (!deathmatch->value && !coop->value) {
//        if ( g_edict_pool.EdictForNumber( 1 )->health <= 0)
//            return;
//    }
//
//    // if noexit, do a ton of damage to other
//    if (deathmatch->value && !((int)dmflags->value & DF_ALLOW_EXIT) && other != world) {
//        SVG_TriggerDamage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 10 * other->max_health, 1000, DAMAGE_NONE, MEANS_OF_DEATH_EXIT );
//        return;
//    }
//
//    // if multiplayer, let everyone know who hit the exit
//    if (deathmatch->value) {
//        if (activator && activator->client)
//            gi.bprintf(PRINT_HIGH, "%s exited the level.\n", activator->client->pers.netname);
//    }
//
//    // if going to a new unit, clear cross triggers
//    if (strchr(self->map.ptr, '*'))
//        game.serverflags &= ~(SFL_CROSS_TRIGGER_MASK);
//
//    SVG_HUD_BeginIntermission(self);
//}
//
//void SP_target_changelevel(svg_base_edict_t *ent)
//{
//    if (!ent->map) {
//        gi.dprintf("target_changelevel with no map at %s\n", vtos(ent->s.origin));
//        SVG_FreeEdict(ent);
//        return;
//    }
//
//    // ugly hack because *SOMEBODY* screwed up their map
//    if ((Q_stricmp(level.mapname, "fact1") == 0) && (Q_stricmp(ent->map, "fact3") == 0))
//        ent->map = "fact3$secret1";
//
//    ent->SetUseCallback( use_target_changelevel );
//    ent->svflags = SVF_NOCLIENT;
//}


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

void use_target_splash( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
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

void SP_target_splash(svg_base_edict_t *self)
{
    self->SetUseCallback( use_target_splash );
    SVG_Util_SetMoveDir(self->s.angles, self->movedir);

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
void ED_CallSpawn(svg_base_edict_t *ent);

void use_target_spawner( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    svg_base_edict_t *ent;

    ent = g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>();;
    ent->classname = self->targetNames.target;
    VectorCopy(self->s.origin, ent->s.origin);
    VectorCopy(self->s.angles, ent->s.angles);
    ent->DispatchSpawnCallback();//ED_CallSpawn(ent);
    gi.unlinkentity(ent);
    SVG_Util_KillBox(ent, false, MEANS_OF_DEATH_TELEFRAGGED );
    gi.linkentity(ent);
    if (self->speed)
        VectorCopy(self->movedir, ent->velocity);
}

void SP_target_spawner(svg_base_edict_t *self)
{
    self->SetUseCallback( use_target_spawner );
    self->svflags = SVF_NOCLIENT;
    if (self->speed) {
        SVG_Util_SetMoveDir(self->s.angles, self->movedir );
        VectorScale(self->movedir, self->speed, self->movedir);
    }
}

//==========================================================

/*QUAKED target_crosslevel_trigger (.5 .5 .5) (-8 -8 -8) (8 8 8) trigger1 trigger2 trigger3 trigger4 trigger5 trigger6 trigger7 trigger8
Once this trigger is touched/used, any trigger_crosslevel_target with the same trigger number is automatically used when a level is started within the same unit.  It is OK to check multiple triggers.  Message, delay, target, and targetNames.kill also work.
*/
void trigger_crosslevel_trigger_use( svg_base_edict_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    game.serverflags |= static_cast<crosslevel_target_flags_t>( self->spawnflags );
    SVG_FreeEdict(self);
}

void SP_target_crosslevel_trigger(svg_base_edict_t *self)
{
    self->svflags = SVF_NOCLIENT;
    self->SetUseCallback( trigger_crosslevel_trigger_use );
}

/*QUAKED target_crosslevel_target (.5 .5 .5) (-8 -8 -8) (8 8 8) trigger1 trigger2 trigger3 trigger4 trigger5 trigger6 trigger7 trigger8
Triggered by a trigger_crosslevel elsewhere within a unit.  If multiple triggers are checked, all must be true.  Delay, target and
targetNames.kill also work.

"delay"     delay before using targets if the trigger has been activated (default 1)
*/
void target_crosslevel_target_think(svg_base_edict_t *self)
{
    if (self->spawnflags == (game.serverflags & SFL_CROSS_TRIGGER_MASK & self->spawnflags)) {
        SVG_UseTargets(self, self);
        SVG_FreeEdict(self);
    }
}

void SP_target_crosslevel_target(svg_base_edict_t *self)
{
    if (! self->delay)
        self->delay = 1;
    self->svflags = SVF_NOCLIENT;

    self->SetThinkCallback( target_crosslevel_target_think );
    self->nextthink = level.time + QMTime::FromSeconds( self->delay );
}

//==========================================================
