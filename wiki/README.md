# Q2RTXPerimental Wiki Documentation

Welcome to the Q2RTXPerimental wiki documentation! This directory contains comprehensive guides for developing game mods using the Q2RTXPerimental engine.

## Documentation Structure

### 📑 Core Pages

| Page | Description |
|------|-------------|
| [**Home**](Home.md) | Main wiki page with navigation and overview |
| [**Getting Started**](Getting-Started.md) | Build instructions, setup, and first mod tutorial |

### 🎮 Vanilla Quake 2 Foundation

Understanding the original Quake 2 architecture is essential:

| Page | Description |
|------|-------------|
| [**Game Module Architecture**](Vanilla-Game-Module-Architecture.md) | Client-server separation, DLL interface, and module design |
| [**Entity System**](Vanilla-Entity-System.md) | How entities work in vanilla Quake 2 |

### 🏗️ Q2RTXPerimental Architecture

Modern enhancements and architecture:

| Page | Description |
|------|-------------|
| [**Architecture Overview**](Architecture-Overview.md) | High-level system design and module relationships |
| [**Server Game Module (svgame)**](Server-Game-Module.md) | Authoritative server-side game logic |
| [**Client Game Module (clgame)**](Client-Game-Module.md) | Client-side prediction, rendering, and effects |
| [**Shared Game Module (sharedgame)**](Shared-Game-Module.md) | Code shared between client and server |

### 🎯 Entity System

The heart of Q2RTXPerimental game logic:

| Page | Description |
|------|-------------|
| [**Entity System Overview**](Entity-System-Overview.md) | OOP entity architecture and lifecycle |
| [**Creating Custom Entities**](Creating-Custom-Entities.md) | Complete tutorial: decorative, interactive, and AI entities |

### ✨ Temporary Entity System

Efficient visual effects:

| Page | Description |
|------|-------------|
| [**Temp Entity Overview**](Temp-Entity-Overview.md) | What temp entities are, when to use them, and how to spawn them |

### 📖 API Reference

Quick reference documentation:

| Page | Description |
|------|-------------|
| [**Entity Types**](API-Entity-Types.md) | All entity type constants and their behavior |
| [**Entity Flags**](API-Entity-Flags.md) | Visual effects and rendering flags |

## Quick Navigation by Topic

### I want to...

**...understand the architecture**
1. Start with [Architecture Overview](Architecture-Overview.md)
2. Read about [Server Game Module](Server-Game-Module.md) for server-side logic
3. Learn about [Client Game Module](Client-Game-Module.md) for client-side rendering

**...create a custom entity**
1. Read [Entity System Overview](Entity-System-Overview.md) for concepts
2. Follow [Creating Custom Entities](Creating-Custom-Entities.md) tutorial
3. Reference [API - Entity Types](API-Entity-Types.md) and [API - Entity Flags](API-Entity-Flags.md)

**...add visual effects**
1. Learn about [Temporary Entity System](Temp-Entity-Overview.md)
2. Understand [Client Game Module](Client-Game-Module.md) for particle systems
3. Check effect examples in [Temp Entity Overview](Temp-Entity-Overview.md)

**...understand client-server networking**
1. Start with [Vanilla Game Module Architecture](Vanilla-Game-Module-Architecture.md)
2. Read [Client Game Module](Client-Game-Module.md) for prediction
3. Study [Shared Game Module](Shared-Game-Module.md) for pmove

**...build the engine and start coding**
1. Follow [Getting Started](Getting-Started.md)
2. Create your first entity with the [Creating Custom Entities](Creating-Custom-Entities.md) tutorial

## Documentation Status

### ✅ Complete
- Home and navigation
- Getting Started guide
- Vanilla Quake 2 foundation (2 pages)
- Q2RTXPerimental architecture (4 pages)
- Entity system guides (2 pages)
- Temporary entity documentation
- API references (2 pages)
- Custom entity creation tutorial

### 🚧 Planned
- Additional Vanilla Quake 2 topics (BSP, Networking, Physics, Weapons, AI)
- Entity lifecycle details
- Entity networking specifics
- Save/Load system
- Advanced topics (Signal I/O, UseTargets, Game Modes, Lua, Animation)
- Additional API references (Events, Spawn Flags, Means of Death)

## Contributing

If you find errors or want to contribute:
1. Open an issue on [GitHub](https://github.com/PolyhedronStudio/Q2RTXPerimental/issues)
2. Submit a pull request with fixes or additions
3. Join the [Discord community](https://discord.gg/6Qc6wfmFMR)

## Documentation Philosophy

Our documentation follows these principles:

1. **Example-Driven**: Every concept includes working code examples
2. **Progressive Complexity**: Start simple, build to advanced
3. **Cross-Referenced**: Related topics are linked together
4. **Practical Focus**: Emphasis on "how to do it" over theory
5. **Up-to-Date**: Reflects current codebase state

## Page Format

Each wiki page follows this structure:

1. **Overview**: What the page covers
2. **Core Concepts**: Fundamental ideas
3. **Code Examples**: Working, compilable code
4. **Practical Patterns**: Common use cases
5. **Best Practices**: Do's and don'ts
6. **Related Documentation**: Links to related pages

## Code Example Standards

All code examples in this wiki:
- ✅ Are syntactically correct C++
- ✅ Reference actual source files with paths
- ✅ Include context (where the code goes)
- ✅ Show both usage and implementation
- ✅ Follow Q2RTXPerimental coding style

## Getting Help

- 📚 **Documentation**: Start here in the wiki
- 💬 **Discord**: [PolyhedronStudio/Q2RTXPerimental](https://discord.gg/6Qc6wfmFMR)
- 🐛 **Issues**: [GitHub Issues](https://github.com/PolyhedronStudio/Q2RTXPerimental/issues)
- 🎮 **QuakeDev**: [QuakeDev Discord](https://discord.gg/csCBGXVUmv)

## Version Information

This documentation is maintained for the latest development version of Q2RTXPerimental. Check the repository's [changelog](../changelog.md) for recent changes.

---

**Ready to start?** Head to [Getting Started](Getting-Started.md) or jump straight into [Creating Custom Entities](Creating-Custom-Entities.md)!
