# Quake II RTXPerimental Todo:

## Engine:
### Shared:
* [ ] Look for a library of sorts, to replace all C style maths with. Worst case scenario, hand-roll them, start off with taking ``qvec3`` from **Q2RE**
### Common/Collision Model:
* [ ] Modify the code so it does not generate ``hulls`` on the fly, but instead retreives them from their **client/server** counterpart. For the client they will have to be regenerated in case of a ``new`` entity, or general ``bounds changes`` have occured.
### Net Code:
* [ ] Rework ``MSG_`` calls to those which I had in Polyhedron so their names are more in line with ``type-strictness``.
* [ ] Rework ``origin`` and ``angles`` to be transmitted as ``floats`` instead of ``shorts``.
* [ ] Add ``packet fragmentation`` for ``svc_frame`` so it can support a higher entity count.
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
### Client Game:
* [ ] Nothing for now.
### Shared Game:
* [ ] Nothing for now.