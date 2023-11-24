# Quake II RTXPerimental TODO (v0.0.1):

## Engine:
### CMake:
* [X] Do a rewrite of CMakeLists.txt's to be more ``structured`` and easily allow for enabling/disabling multiple types of game builds.
### Shared:
* [ ] Look for a library of sorts, to replace all C style maths with. Worst case scenario, hand-roll them, start off with taking ``qvec3`` from **Q2RE**
### Common/Collision Model:
* [ ] Modify the code so it does not generate ``hulls`` on the fly, but instead set them at ``LinkEntity`` time. Then for tracing, retreives them from their **client/server** counterpart. In case of the **client** they will be regenerated in case of a ``new`` entity, or general ``bounds changes`` has occured for the **client** entity.
### Net Code:
* [X] Rework ``MSG_`` calls to those which I had in Polyhedron so their names are more in line with ``type-strictness``.
* [X] Rework ``origin`` and ``angles`` to be transmitted as ``floats`` instead of ``shorts``.
* [X] Add ``packet fragmentation`` for ``svc_frame`` so it can support a higher entity count.
### Refresh(VKPT):
* [ ] Cpp-ify **VKPT** after all, this'll allow for more ``Cpp-ification``
* [ ] Allow support for ``RF_NOSHADOW``.
### Server:
* [ ] Generate and store entity ``hulls`` in their server-side counterpart for later ``collision model`` rework.
* [ ] Add a ``gamemode`` cvar and rid ourselves of ``deathmatch`` and ``coop`` cvars.

## Game Modules:
### Server Game:
* [ ] Add an entity type that can have several ``hull`` varieties set to it, for testing.
* [ ] Add an entity that uses the humanoid ``test dummy`` model from **Mixamo** and **fully** operates at ``40hz``.
* [ ] Add support for a proper "+/-use" command such as seen in **Half-Life**.
* [ ] Add proper ``gamemode`` support based on a ``gamemode`` cvar.
* [ ] Properly group functions and files accordingly, move into designated group subfolder if needed.
### Client Game:
* [ ] Nothing for now.
### Shared Game:
* [ ] Nothing for now.