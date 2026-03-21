# Navigation Rewrite Module (`svgame/nav2`)

This folder is the canonical destination for the staged navigation rewrite.

- Place all new navigation rewrite code here.
- Prefer engine-native `gi` APIs, tagged memory ownership, and `SVG_RAIIObject`.
- Keep data structures stable-ID friendly for worker snapshots and serialization.

## Memory Ownership and RAII Conventions

Nav2 must follow the existing svgame ownership model rather than introducing an independent allocator scheme.

### Preferred ownership rules
- Use `SVG_RAIIObject` or `nav2_owner_t<T>` for owned helper objects and object-like runtime state that benefits from deterministic teardown.
- Use `gi.TagMallocz` for zero-initialized tagged buffers such as query state blocks, section descriptors, and builder-local scratch that is represented as raw storage rather than objects.
- Use `gi.TagReMalloc` only for genuinely growable tagged buffers that need to preserve contents across resizes.
- Use `gi.TagFree` or `gi.TagFreeP` only for tagged allocations created through the tagged allocator path.
- Use `gi.FS_LoadFile` only for engine filesystem blobs and always release those buffers with `gi.FS_FreeFile`.

### Lifetime guidance
- Long-lived nav assets and subsystem helper objects should use the default svgame tag and prefer RAII owners when the allocation is object-like.
- Query-lifetime and worker-slice scratch should prefer zero-initialized tagged allocations or RAII-owned helper objects and must be released deterministically on cancellation, restart, or shutdown.
- Serialization buffers should use tagged allocation when nav2 owns the memory, but filesystem-loaded blobs must remain under `FS_FreeFile` ownership.
- Benchmark and debug scratch should stay lightweight, tagged, and easy to discard without relying on broad tag resets.

### Avoid
- Avoid raw `new` / `delete` ownership patterns in nav2 code.
- Avoid freeing `FS_LoadFile` buffers with `TagFree`.
- Avoid freeing tagged buffers with `FS_FreeFile`.
- Avoid using `gi.FreeTags` as ordinary control flow cleanup for nav2 subsystems unless the entire tag lifetime is intentionally ending.

### Helper layer
- Use `nav2_memory.h/.cpp` for small helper wrappers that make tag-vs-filesystem ownership explicit at the call site.
- Keep helper usage pragmatic: the goal is clearer ownership, not a second memory-management system.
