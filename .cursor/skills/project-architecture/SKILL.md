---
name: project-architecture
description: JustOneLastPatrol mod architecture patterns, naming conventions, and project structure
version: 1.0.0
---

# JustOneLastPatrol Architecture

Quick reference for JustOneLastPatrol mod-specific patterns and conventions. For detailed patterns, see resource files below.

---

## When to Use This Skill

Use this skill when:
- Creating new Manager or Controller components
- Understanding JustOneLastPatrol's architecture patterns
- Following project naming conventions
- Organizing code in the correct directories
- Accessing global systems via JOLP_Global
- Setting up new features or systems

---

## Quick Reference

### Manager Components
Singleton components on JOLP_JustOneLastPatrolGameMode managing entire systems. Use GetInstance() pattern with static s_Instance. Init() and PostGameStart() called manually by game mode.

**See:** `managers.md` for complete manager patterns

### Controller Components
Non-singleton components managing individual entities (bases, towns, camps). Register with manager in constructor. Multiple instances can exist simultaneously.

**See:** `controllers.md` for complete controller patterns

### JOLP_Global Access
Central static class providing easy access to all manager singletons. Use JOLP_Global.GetSomething() instead of calling GetInstance() directly. Cleaner and more consistent.

**See:** `global-access.md` for access patterns

### JOLP_JustOneLastPatrolController
The modular architecture for client-server communication, and since 2026-08-14 the ONLY one. Each player owns a controller entity carrying 17 specialized components, reached with `JOLP_ControllerComponent<T>.Get()`. The legacy comms monolith it replaced is deleted. Built-in progress tracking support.

**See:** `project-controller.md` for complete pattern

### File Organization
Scripts in Scripts/Game/, configs in Configs/, prefabs in Prefabs/. Specific subdirectories for Components, GameMode, Entities, Controllers, UI, UserActions.

**See:** `file-structure.md` for directory structure

### Coding Standards
JOLP_ class prefix, m_ member prefix, type prefixes (m_i, m_f, m_s, m_b, m_a, m_m). Doxygen-style comments. Getters/setters for protected members.

**See:** `coding-standards.md` for complete conventions

---

## Critical Conventions

- ✅ **JOLP_ prefix** - All JustOneLastPatrol classes start with JOLP_
- ✅ **Managers are singletons** - One instance per game mode
- ✅ **Controllers are instances** - Multiple instances per entity type
- ✅ **Use JOLP_Global** - For accessing managers (not direct GetInstance())
- ✅ **Use JustOneLastPatrolController** - For new client→server operations (not PlayerCommsComponent)
- ✅ **Register in constructor** - Controllers register with managers
- ✅ **Protected members** - Use getters/setters for external access
- ⚠️ **Init() not automatic** - Called manually by game mode or manager
- ✅ **Type prefixes** - m_i for int, m_f for float, m_s for string, etc.

---

## Architecture Hierarchy

```
JOLP_JustOneLastPatrolGameMode (entity)
├── JOLP_SomeManagerComponent (singleton)
│   ├── Manages multiple controllers
│   └── Global system state
└── JOLP_AnotherManagerComponent (singleton)
    └── Manages different system

Entity in World
└── JOLP_SomeControllerComponent (instance)
    ├── Manages this specific entity
    └── Registered with relevant manager
```

---

## Resource Files

Detailed documentation organized by concern:

1. **managers.md** - Singleton manager pattern, GetInstance, Init, PostGameStart
2. **controllers.md** - Instance controllers, registration, lifecycle
3. **project-controller.md** - NEW: Modular controller pattern for client-server operations
4. **global-access.md** - JOLP_Global patterns, accessing managers/systems
5. **file-structure.md** - Project directory organization and file placement
6. **coding-standards.md** - Naming conventions, documentation style, best practices

---

## Common Patterns

### Creating a Manager
1. Extend JOLP_Component
2. Add corresponding JOLP_ComponentClass
3. Implement static s_Instance and GetInstance()
4. Add Init() and PostGameStart() if needed
5. Place component on JOLP_JustOneLastPatrolGameMode prefab
6. Add accessor to JOLP_Global

### Creating a Controller
1. Extend JOLP_Component
2. Add corresponding JOLP_ComponentClass
3. Register with manager in constructor
4. Use protected members with getters/setters
5. Attach to entity prefab or spawn at runtime

### Accessing Systems
1. Use JOLP_Global.GetManager() for managers
2. Use JOLP_Global.GetController() for local controller
3. Use JOLP_Global.GetUI() for UI manager
4. Use JOLP_Global.GetPlayers() for player management
5. Always check for null before using

---

**Pattern:** Start here for quick reference, dive into resource files for implementation details.
