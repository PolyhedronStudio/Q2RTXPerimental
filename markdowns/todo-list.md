# TODO:

## Version 0.0.7
### qstd:
#### containers:
    - [ ] Containers:
        - [ ] `string`
        - [ ] `vector`
        - [ ] `(forward-)list`
        - [ ] `circular_buffer`
#### Maths:
    - [ ] Maths:
        - [ ] `Matrix3x3`
        - [ ] `Matrix3x2`
        - [ ] `Matrix4x4`
        - [ ] `DualQuaternion`

---
### Refresh(GL):
#### Replace:
    - [ ] Perhaps replace entire renderer with yQuake2 or so?
#### Formats:
    - [ ] Add IQM support, external lightmaps?

---
### SGame:

    - [ ] Move items list and functionalities down here so we can have CLGame and SVGame both be aware of the same items.
#### GameMode Ideas:
    - [ ] Implement Team DM entirely.
    - [ ] Implement Capture The Beav entirely.
        - This might also be a tag kind of game mode, as long as it features Beav being the main star.
    - [ ] Implement a spin-off of the Bomb Defusal scenario of CS.
    - [ ] Implement a spin-off of the hostage rescue in CS.
        - It'd be neat if one has the ability to play as a hostage himself. Where he can only pick up weapons from dead players, and when he does decide to weaponize, he won't be an extra point for the rescue team when they get to the rescue zone.

## Version 0.0.6
### Technically:
    - [ ] AreaPortal and func_door in Makkon map are buggy again? Wtf?
        - [ ] On that note, crawl in the tunnel in func_rotating map, the rotators etc hide. Wtf?
    - [ ] REQUIRES ASSET/PIPELINE MODIFICATIONS:
        - [ ] Allow for setting hitbox capsules by hand in iqmtools.

---
### Core:
#### CollisionModel:
    - [ ] Add `brushID` trace support so we can identify the actual brush being hit.
        - A `brushID` of `0` equals No Brush Hit.
        - A positive `brushID` equals for example `1` is BSP `brushList[brushID - 1]`. This might also be an inline-model BSP brush index belonging to the trace its entity.
        - A negative `brushID` equals for example `1` is Entity `GetEntityHull(entities[brushID - 1])`
#### Skeletal Models:
    - [ ] Add support for .skc files to define specific bone ID/name related features
    and hitboxing.
    
---
### Client:
#### Various:
    - [ ] Move scoreboard to clgame.
        - [ ] Some cmd handling is still in the Client.
        - [X] Patch up scoreboard cmd.

---
### Refresh(VKPT):
#### Validation Layer:
    - [ ] Fix the remaining validation error about resource leakage when shutting down the game.

---
### Server:
#### Entity Collision Hulls:
    - [ ] Store an identity, and transformed hull for each `server_entity_t` which are updated when linking the entity.


---
### CLGame:
#### Gameplay Feature Necessities:
    - [X] Implement Mute/Unmute button functionality of the scoreboard menu.
    - [ ] Implement state managed and received from the SVGame. (Muted or not)
    - [ ] Implement all necessary functions for the remaining effects that need to be ported as event entities.
---
### SGame:
    - [ ] Move items list and functionalities down here so we can have CLGame and SVGame both be aware of the same items.

---
### SVGame:
#### !!COOL!! Gameplay Feature:
    - [X] HitBoxes treatment:
        - [X] Allow 
    - [ ] Implement all necessary functions for the remaining effects that need to be ported as event entities.

#### !!Necessary!! Gameplay Feature:
    - [X] Patch up scoreboard cmd.
    - [ ] Implement state managed and sent to the CLGame for scoreboard data.
    - [ ] Implement Vote Kick/Ban for clients, where it takes at least a certain percentage(estimate, I have no clue yet) required to agree with a vote.

#### GameMode Ideas:
    - [ ] Implement DeathMatch entirely.

#### Structurally:
    - [ ] Remove necessities for "deathmatch" cvar and "coop" cvar.
    - [ ] ..






```
---
---
Just some space here, to separate lol, cba to search markdown tag for that atm.
---
---
```





## Version 0.0.7
### Client:
- [ ] Move scoreboard to clgame.
    - [ ] Some cmd handling is still in the Client.
    - [X] Patch up scoreboard cmd.
---
### Server:
- [ ] ...
---
### CLGame:
#### Gameplay Feature Necessities:
- [X] Implement Mute/Unmute button functionality of the scoreboard menu.
- [ ] Implement state managed and received from the SVGame. (Muted or not)
- [ ] (See SVGame Todo): Implement a box entity, which can be picked up and carried around by the player.
    - Requires SVGame work also.

---
### SVGame:
#### !!COOL!! Gameplay Feature:
- [ ] HitBoxes treatment:
    - [ ] Allow 

#### !!Necessary!! Gameplay Feature:
- [X] Patch up scoreboard cmd.
- [ ] Implement state managed and sent to the CLGame for scoreboard data.
- [ ] Implement Vote Kick/Ban for clients, where it takes at least a certain percentage(estimate, I have no clue yet) required to agree with a vote.
---
### SGame:


#### Structurally:
- [ ] ..

#### Technically:
- [ ] Implement Recast/Detour libraries for AI.
- [ ] Implement a box entity, which can be picked up and carried around by the player. Likely requires CLGame work also.
