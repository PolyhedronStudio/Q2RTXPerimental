# Quake II RTXPerimental TODO Lists:

## (v0.0.2):
### Engine:
#### General:
* [ ] Although we have 64 bit time in-game, the actual way how server frames, acks and requests are handled is usually still ``unsigned``. Convert this to ``int64_t/uint64_t``.
* [ ] Add and 'Wire'/'Netcode' the specific entity type an entity is.
* [ ] Currently svc_layout is sending 'large' strings, we can avoid doing so and just send minimal info now that
the Client Game is made aware of the Gamemode we're in._
* [ ] Create 'Spotlight' entities for the currently unused SPOTLIGHT features of the RTX renderer. (Ok, the client-side 'flashlight' makes use of it.)
#### CMake:
* [ ] Nothing for now.
#### Shared:
* [ ] Replace all C style maths with a library of sorts. Worst case scenario, hand-roll it.
#### Common/Collision Model:
* [ ] Nothing for now.
#### Net Code:
* [ ] Nothing for now.
#### Refresh(VKPT):
* [ ] ?? Cpp-ify **VKPT** after all, this'll allow for more ``Cpp-ification`` ??
* [ ] Allow support for ``RF_NOSHADOW``.
#### Server:
* [ ] Nothing for now.

### Game Modules:
#### Server Game:
* [ ] Rid all places where ``coop`` and ``deathmatch`` cvars are checked by actual individual gamemode related counterparts.
* [ ] Add some way of having entity 'class' like type support. (At the least, eliminate a need for having such a large edict_t type that holds all sorts of object-type specific variables.)
* [ ] Add an entity that uses the humanoid ``test dummy`` model from **Mixamo** and **fully** operates at ``40hz``.
#### Client Game:
* [ ] In accordance of ridding ourselves of entire layout strings, implement the desired rendering for each game mode in the CLGame side.
#### Shared Game:
* [ ] Improve PMove.

## (v0.0.3):
### Engine:
#### General:
* [ ] Nothing for now.
#### CMake:
* [ ] Nothing for now.
#### Shared:
* [ ] Nothing for now.
#### Common/Collision Model:
* [ ] Modify the code so it does not generate ``hulls`` on the fly, but instead set them at ``LinkEntity`` time. 
Then for tracing, retreives them from their **client/server** counterpart. 
In case of the **client** they will be regenerated in case of a ``new`` entity, or general ``bounds changes`` has occured for the **client** entity.
* [ ] Calculate the proper entity matrixes/quaternions during Link time.
#### Net Code:
* [ ] Nothing for now.
#### Refresh(VKPT):
* [ ] ?? Cpp-ify **VKPT** after all, this'll allow for more ``Cpp-ification`` ??
* [ ] Nothing for now.
#### Server:
* [ ] Generate and store entity ``hulls`` in their server-side counterpart for later ``collision model`` rework.

### Game Modules:
#### Server Game:
* [ ] Add support for a proper "+/-use" command such as seen in **Half-Life**.
* [ ] Add an entity type that can have several ``hull`` varieties set to it, for testing.
#### Client Game:
* [ ] Nothing for now.
#### Shared Game:
* [ ] Improve PMove.