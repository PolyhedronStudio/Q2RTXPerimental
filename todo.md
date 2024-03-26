# Quake II RTXPerimental TODO Lists:
These are mainly my personal notes/ideas/interests, and do not per se reflect the actual changes to be made.

* [ ] Rid all places where ``coop`` and ``deathmatch`` cvars are checked by actual individual gamemode related counterparts.
* [ ] Add some way of having entity 'class' like type support. (At the least, eliminate a need for having such a large edict_t type that holds all sorts of object-type specific variables.)
* [ ] Add an entity that uses the humanoid ``test dummy`` model from **Mixamo** and **fully** operates at ``40hz``.
* [ ] In accordance of ridding ourselves of entire layout strings, implement the desired rendering for each game mode in the CLGame side.

* [ ] Modify the code so it does not generate ``hulls`` on the fly, but instead set them at ``LinkEntity`` time. 
	* [ ] Generate and store entity ``hulls`` in their server-side counterpart for later ``collision model`` rework.
* [ ] Then for tracing, retreives them from their **client/server** counterpart. 
In case of the **client** they will be regenerated in case of a ``new`` entity, or general ``bounds changes`` has occured for the **client** entity.
* [ ] Calculate the proper entity matrixes/quaternions during Link time.

* [ ] Add support for a proper "+/-use" command such as seen in **Half-Life**.
* [ ] Add an entity type that can have several ``hull`` varieties set to it, for testing purposes.