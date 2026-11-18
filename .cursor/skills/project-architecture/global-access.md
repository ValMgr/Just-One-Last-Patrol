# JOLP_Global Access Patterns

Complete guide for accessing managers and systems via JOLP_Global static class.

---

## Overview

JOLP_Global provides centralized static methods to access all manager singletons and key systems. Use JOLP_Global instead of calling GetInstance() directly for cleaner, more consistent code.

---

## Available Accessors

### Manager Access

```cpp
class JOLP_Global
{
    // Manager components
    static JOLP_TownManagerComponent GetTowns();
    static JOLP_BaseManagerComponent GetBases();
    static JOLP_FactionManagerComponent GetFactions();
    static JOLP_EconomyManagerComponent GetEconomy();
    static JOLP_CampManagerComponent GetCamps();
    // ... other managers ...

    // Player/Client systems
    static JOLP_PlayerManagerComponent GetPlayers();
    static JOLP_JustOneLastPatrolController GetController(); // Local client controller
    static JOLP_UIManagerComponent GetUI(); // Local UI manager

    // Server system (deprecated - use components on controller instead)
    static JOLP_JustOneLastPatrolServerComponent GetServer(); // Legacy

    // Game mode
    static JOLP_JustOneLastPatrolGameMode GetGameMode();
}
```

---

## Usage Patterns

### Accessing Managers

```cpp
void SomeMethod()
{
    // Get town manager
    JOLP_TownManagerComponent towns = JOLP_Global.GetTowns();
    if (!towns) return; // Manager not loaded

    // Use manager
    JOLP_TownController town = towns.GetTownById(15);
}
```

### Chaining Calls

```cpp
void GetTownName(int townId)
{
    JOLP_TownController town = JOLP_Global.GetTowns().GetTownById(townId);
    if (!town) return;

    return town.GetName();
}
```

### Always Check for Null

```cpp
void UpdateEconomy()
{
    JOLP_EconomyManagerComponent economy = JOLP_Global.GetEconomy();
    if (!economy) return; // Not loaded or doesn't exist

    economy.UpdatePrices();
}
```

---

## Client-Specific Access

### Getting Local Controller

```cpp
void ClientMethod()
{
    // Get locally controlled entity
    JOLP_JustOneLastPatrolController controller = JOLP_Global.GetController();
    if (!controller) return; // We're on dedicated server

    // Access components on controller
    JOLP_SomeServerComponent component = JOLP_SomeServerComponent.Cast(
        controller.FindComponent(JOLP_SomeServerComponent)
    );

    if (component)
    {
        component.DoClientTask();
    }
}
```

### Getting UI Manager

```cpp
void ShowUI()
{
    JOLP_UIManagerComponent ui = JOLP_Global.GetUI();
    if (!ui) return; // No UI (dedicated server)

    JOLP_SomeContext context = JOLP_SomeContext.Cast(ui.GetContext(JOLP_SomeContext));
    if (!context) return;

    ui.ShowContext(JOLP_SomeContext);
}
```

---

## Server-Specific Access

### Server-Only Managers

Most managers exist only on server:

```cpp
void ServerMethod()
{
    // These return null on client
    JOLP_AIManagerComponent ai = JOLP_Global.GetAI();
    if (!ai) return; // Client or not loaded

    ai.SpawnAIGroup();
}
```

### Checking Server vs Client

```cpp
void SomeMethod()
{
    if (Replication.IsServer())
    {
        // Server-side logic
        JOLP_Global.GetEconomy().ProcessTransactions();
    }
    else
    {
        // Client-side logic
        JOLP_Global.GetUI().UpdateDisplay();
    }
}
```

---

## vs Direct GetInstance()

### ❌ Don't Use GetInstance() Directly

```cpp
void OldWay()
{
    JOLP_TownManagerComponent towns = JOLP_TownManagerComponent.GetInstance();
    if (!towns) return;

    // Verbose, inconsistent
}
```

### ✅ Use JOLP_Global

```cpp
void NewWay()
{
    JOLP_TownManagerComponent towns = JOLP_Global.GetTowns();
    if (!towns) return;

    // Clean, consistent
}
```

**Benefits:**
- Shorter, cleaner code
- Consistent access pattern
- Easier to refactor
- Centralized access point

---

## Adding New Manager Accessors

### Step 1: Create Manager

```cpp
class JOLP_NewManagerComponent: JOLP_Component
{
    static JOLP_NewManagerComponent s_Instance;

    static JOLP_NewManagerComponent GetInstance()
    {
        if (!s_Instance)
        {
            BaseGameMode pGameMode = GetGame().GetGameMode();
            if (pGameMode)
            {
                s_Instance = JOLP_NewManagerComponent.Cast(
                    pGameMode.FindComponent(JOLP_NewManagerComponent)
                );
            }
        }
        return s_Instance;
    }
}
```

### Step 2: Add to JOLP_Global

```cpp
// In JOLP_Global.c
class JOLP_Global
{
    // ... existing accessors ...

    static JOLP_NewManagerComponent GetNewManager()
    {
        return JOLP_NewManagerComponent.GetInstance();
    }
}
```

### Step 3: Use Everywhere

```cpp
void UseNewManager()
{
    JOLP_NewManagerComponent manager = JOLP_Global.GetNewManager();
    if (!manager) return;

    manager.DoSomething();
}
```

---

## Common Access Patterns

### Pattern: Get and Use Manager

```cpp
void ProcessTowns()
{
    JOLP_TownManagerComponent towns = JOLP_Global.GetTowns();
    if (!towns) return;

    array<JOLP_TownController> allTowns = towns.GetAllTowns();
    foreach (JOLP_TownController town : allTowns)
    {
        ProcessTown(town);
    }
}
```

### Pattern: Cross-Manager Communication

```cpp
void TransferResources(int fromTownId, int toTownId, int amount)
{
    // Get both managers
    JOLP_TownManagerComponent towns = JOLP_Global.GetTowns();
    JOLP_EconomyManagerComponent economy = JOLP_Global.GetEconomy();

    if (!towns || !economy) return;

    // Get towns
    JOLP_TownController fromTown = towns.GetTownById(fromTownId);
    JOLP_TownController toTown = towns.GetTownById(toTownId);

    if (!fromTown || !toTown) return;

    // Process transfer via economy
    economy.TransferResources(fromTown, toTown, amount);
}
```

### Pattern: Client Request to Server

```cpp
void ClientRequestAction()
{
    // Get local controller
    JOLP_JustOneLastPatrolController controller = JOLP_Global.GetController();
    if (!controller) return; // We're on server

    // Get component on controller
    JOLP_ServerRequestComponent requests = JOLP_ServerRequestComponent.Cast(
        controller.FindComponent(JOLP_ServerRequestComponent)
    );

    if (!requests) return;

    // Send request
    requests.RequestAction();
}
```

---

## Best Practices

### ✅ DO:

- **Always use JOLP_Global:** For accessing managers
- **Check for null:** Every time, managers may not exist
- **Short variable names:** `towns`, `economy`, etc.
- **Early return:** If manager null, return early
- **Add new accessors:** When creating new managers
- **Document managers:** What they manage, when available

### ❌ DON'T:

- **Call GetInstance() directly:** Use JOLP_Global instead
- **Assume manager exists:** Always check for null
- **Cache manager references:** Get when needed, managers are singletons
- **Skip null checks:** Will crash if manager doesn't exist
- **Use on wrong side:** Some managers server-only, UI client-only

---

## Null Check Strategies

### Strategy 1: Early Return

```cpp
void Method()
{
    JOLP_TownManagerComponent towns = JOLP_Global.GetTowns();
    if (!towns) return;

    // Use towns
}
```

### Strategy 2: Guard Pattern

```cpp
void Method()
{
    JOLP_TownManagerComponent towns = JOLP_Global.GetTowns();
    if (towns)
    {
        // Use towns
    }
}
```

### Strategy 3: Multiple Managers

```cpp
void Method()
{
    JOLP_TownManagerComponent towns = JOLP_Global.GetTowns();
    JOLP_EconomyManagerComponent economy = JOLP_Global.GetEconomy();

    if (!towns || !economy) return;

    // Use both
}
```

---

## Testing Global Access

### Verify Manager Available

```cpp
void TestManagerAccess()
{
    JOLP_TownManagerComponent towns = JOLP_Global.GetTowns();
    if (!towns)
    {
        Print("ERROR: Town manager not available!", LogLevel.ERROR);
        return;
    }

    Print("Town manager available");
}
```

### Verify Client-Side Access

```cpp
void TestClientAccess()
{
    // Should return null on dedicated server
    JOLP_UIManagerComponent ui = JOLP_Global.GetUI();

    if (Replication.IsServer() && !Replication.IsRunning())
    {
        // Dedicated server - UI should be null
        if (ui)
        {
            Print("ERROR: UI exists on dedicated server!", LogLevel.ERROR);
        }
    }
    else
    {
        // Client or host - UI should exist
        if (!ui)
        {
            Print("ERROR: UI missing on client!", LogLevel.ERROR);
        }
    }
}
```

---

## Related Resources

- See `managers.md` for manager pattern
- See `controllers.md` for controller pattern
- See `enforcescript-patterns/component-patterns.md` for base patterns
- See main `SKILL.md` for overview
