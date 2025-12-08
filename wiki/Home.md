# Q2RTXPerimental Wiki

Welcome to the **Q2RTXPerimental** game development documentation! This wiki is designed to help developers create game mods using the Q2RTXPerimental engine, a modernized evolution of Quake II RTX.

## What is Q2RTXPerimental?

Q2RTXPerimental is an experimental fork of Quake II RTX that modernizes the classic Quake II engine with:
- Path-traced ray tracing graphics
- Modern C++ codebase
- Enhanced entity system with OOP architecture
- Lua scripting integration
- Skeletal animation system with root motion
- Signal I/O system for entity communication
- Advanced game mode support

## Quick Navigation

### 🚀 Getting Started
- [**Getting Started**](Getting-Started.md) - Build instructions and development environment setup
- [**Creating Your First Mod**](Getting-Started.md#creating-your-first-mod) - Step-by-step guide to creating a simple mod

### 📚 Core Documentation

#### Vanilla Quake 2 Fundamentals
Understanding the foundation is crucial before diving into Q2RTXPerimental enhancements:
- [**Game Module Architecture**](Vanilla-Game-Module-Architecture.md) - Client/server separation and DLL interface
- [**Entity System Overview**](Vanilla-Entity-System.md) - How entities work in vanilla Quake 2
- [**BSP Map Format**](Vanilla-BSP-Format.md) - Map structure and entity placement
- [**Networking Fundamentals**](Vanilla-Networking.md) - Client-server model, snapshots, and state updates
- [**Physics and Movement**](Vanilla-Physics.md) - Movement, collision detection, and simulation
- [**Weapon System**](Vanilla-Weapons.md) - How weapons are implemented
- [**AI System**](Vanilla-AI.md) - Monster AI and pathfinding basics

#### Q2RTXPerimental Architecture
Learn about the enhanced architecture and new systems:
- [**Architecture Overview**](Architecture-Overview.md) - High-level overview of Q2RTXPerimental design
- [**Client Game (clgame)**](Client-Game-Module.md) - Client-side prediction, rendering, and effects
- [**Server Game (svgame)**](Server-Game-Module.md) - Authoritative server logic, physics, and AI
- [**Shared Game (sharedgame)**](Shared-Game-Module.md) - Code shared between client and server
- [**Engine Integration**](Engine-Integration.md) - How game modules communicate with the engine

### 🎮 Entity System

The heart of Q2RTXPerimental game logic:
- [**Entity System Overview**](Entity-System-Overview.md) - Understanding the entity architecture
- [**svg_base_edict_t Reference**](Entity-Base-Class-Reference.md) - Complete reference for the base entity class
- [**Creating Custom Entities**](Creating-Custom-Entities.md) - Step-by-step tutorial with examples
- [**Entity Lifecycle**](Entity-Lifecycle.md) - Spawn, think, die, and respawn
- [**Entity Networking**](Entity-Networking.md) - How entities are synchronized to clients
- [**Save/Load System**](Entity-Save-Load.md) - Persistence and serialization

### ✨ Temporary Entity System

Efficient visual effects without entity overhead:
- [**Temp Entity Overview**](Temp-Entity-Overview.md) - What are temp entities and when to use them
- [**Event Types Reference**](Temp-Entity-Event-Types.md) - Complete list of all temp entity events
- [**Using Temp Entities**](Using-Temp-Entities.md) - How to spawn temp entities from your code
- [**Custom Temp Entities**](Custom-Temp-Entities.md) - Adding new temporary entity types

### 🔧 Advanced Topics

Deep dives into specialized systems:
- [**Signal I/O System**](Signal-IO-System.md) - Entity communication and event dispatching
- [**UseTargets System**](UseTargets-System.md) - Interactive entities and player interaction
- [**Game Modes**](Game-Modes.md) - Custom game mode development
- [**Lua Scripting**](Lua-Scripting.md) - Extending gameplay with Lua
- [**Skeletal Animation**](Skeletal-Animation.md) - SKM animation system
- [**Root Motion**](Root-Motion-System.md) - Animation-driven movement

### 📖 API Reference

Quick reference for constants, enums, and flags:
- [**Entity Types**](API-Entity-Types.md) - All entity type constants
- [**Entity Flags**](API-Entity-Flags.md) - Entity flag reference
- [**Entity Events**](API-Entity-Events.md) - Entity event system
- [**Spawn Flags**](API-Spawn-Flags.md) - Map editor spawn flags
- [**Means of Death**](API-Means-Of-Death.md) - Damage type constants

## Contributing to Documentation

This wiki is maintained to reflect the current state of the Q2RTXPerimental codebase. If you find errors or outdated information, please:
1. Open an issue on the [GitHub repository](https://github.com/PolyhedronStudio/Q2RTXPerimental)
2. Submit a pull request with corrections
3. Join the [Discord community](https://discord.gg/6Qc6wfmFMR) for discussions

## Additional Resources

- [Q2RTXPerimental GitHub Repository](https://github.com/PolyhedronStudio/Q2RTXPerimental)
- [Q2RTXPerimental Discord Server](https://discord.gg/6Qc6wfmFMR)
- [QuakeDev Discord](https://discord.gg/csCBGXVUmv)
- [Original Quake II Source Code](https://github.com/id-Software/Quake-2)
- [Quake II RTX](https://github.com/NVIDIA/Q2RTX)

## Version Information

This documentation is maintained for the latest development version of Q2RTXPerimental. Features and APIs may change as the engine evolves. Check the [changelog](../changelog.md) for recent updates.

---

**Ready to start?** Head over to [Getting Started](Getting-Started.md) to set up your development environment!
