# UI Context Patterns

Complete guide for UI contexts, layout management, and JOLP_UIManagerComponent integration.

---

## Overview

JustOneLastPatrol UI system uses contexts that extend JOLP_UIContext. Each context manages a .layout file and its lifecycle. The JOLP_UIManagerComponent on the player entity handles context activation and management.

---

## Basic UI Context Pattern

### When to Use

Create a UI context when you need:
- A new screen/menu in the game
- Modal dialogs or overlays
- Persistent UI elements
- Interactive forms or interfaces

### Pattern Structure

```cpp
class JOLP_MyContext : JOLP_UIContext
{
    // Layout file (configured in Workbench)
    override ResourceName GetLayout()
    {
        return "{Path}/MyLayout.layout";
    }

    // Called when context is shown
    override void OnShow()
    {
        super.OnShow();

        // Initialize widgets
        // Bind data to UI
        // Set up event handlers
    }

    // Called when context is hidden
    override void OnHide()
    {
        super.OnHide();

        // Clean up
        // Unbind data
        // Remove event handlers
    }

    // Called every frame when visible
    override void OnUpdate(float timeSlice)
    {
        super.OnUpdate(timeSlice);

        // Update dynamic UI elements
    }
}
```

### Key Points

- **Extend JOLP_UIContext:** Base class for all JustOneLastPatrol contexts
- **GetLayout():** Returns path to .layout file
- **OnShow():** Initialize UI when context becomes visible
- **OnHide():** Clean up when context hidden
- **OnUpdate():** Called every frame for dynamic updates

---

## Activating UI Contexts

### Basic Activation

```cpp
void ShowMyUI()
{
    JOLP_UIManagerComponent ui = JOLP_Global.GetUI();
    if (!ui) return; // No UI manager (server?)

    // Get context instance
    JOLP_MyContext context = JOLP_MyContext.Cast(ui.GetContext(JOLP_MyContext));
    if (!context) return; // Context not registered

    // Set data before showing
    context.SetData(someData);

    // Show the context
    ui.ShowContext(JOLP_MyContext);
}
```

### With Data Passing

```cpp
class JOLP_ShopContext : JOLP_UIContext
{
    protected ref JOLP_ShopData m_ShopData;

    void SetShop(JOLP_ShopData shop)
    {
        m_ShopData = shop;
    }

    override void OnShow()
    {
        super.OnShow();

        // Use shop data to populate UI
        if (m_ShopData)
        {
            UpdateInventoryDisplay();
            UpdatePrices();
        }
    }
}

// Activating with data
void OpenShop(JOLP_ShopData shop)
{
    JOLP_UIManagerComponent ui = JOLP_Global.GetUI();
    if (!ui) return;

    JOLP_ShopContext context = JOLP_ShopContext.Cast(ui.GetContext(JOLP_ShopContext));
    if (!context) return;

    // Pass data to context
    context.SetShop(shop);

    // Show context
    ui.ShowContext(JOLP_ShopContext);
}
```

---

## Context Registration

### Registering Contexts

Contexts must be registered with JOLP_UIManagerComponent (typically on the player prefab in Workbench):

1. Open player prefab in Workbench
2. Find JOLP_UIManagerComponent
3. Add context class to registered contexts list
4. Save prefab

### Checking Registration

```cpp
JOLP_UIManagerComponent ui = JOLP_Global.GetUI();
if (!ui) return;

JOLP_MyContext context = JOLP_MyContext.Cast(ui.GetContext(JOLP_MyContext));
if (!context)
{
    Print("ERROR: JOLP_MyContext not registered with UI Manager!", LogLevel.ERROR);
    return;
}
```

---

## Widget Access and Binding

### Finding Widgets

```cpp
class JOLP_MyContext : JOLP_UIContext
{
    protected TextWidget m_TitleText;
    protected ButtonWidget m_ConfirmButton;
    protected ImageWidget m_IconImage;

    override void OnShow()
    {
        super.OnShow();

        // Find widgets by name (defined in .layout file)
        m_TitleText = TextWidget.Cast(m_Root.FindAnyWidget("TitleText"));
        m_ConfirmButton = ButtonWidget.Cast(m_Root.FindAnyWidget("ConfirmButton"));
        m_IconImage = ImageWidget.Cast(m_Root.FindAnyWidget("IconImage"));

        // Check widgets found
        if (!m_TitleText || !m_ConfirmButton || !m_IconImage)
        {
            Print("ERROR: Failed to find required widgets!", LogLevel.ERROR);
            return;
        }

        // Bind event handlers
        m_ConfirmButton.SetHandler(new SCR_ConfirmButtonHandler(this));

        // Set initial values
        m_TitleText.SetText("My Title");
        m_IconImage.LoadImageTexture(0, "{Path}/icon.edds");
    }
}
```

### Widget Event Handlers

```cpp
class SCR_ConfirmButtonHandler : ScriptedWidgetEventHandler
{
    protected JOLP_MyContext m_Context;

    void SCR_ConfirmButtonHandler(JOLP_MyContext context)
    {
        m_Context = context;
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (button != 0) return false; // Left click only

        m_Context.OnConfirmClicked();
        return true;
    }
}

class JOLP_MyContext : JOLP_UIContext
{
    void OnConfirmClicked()
    {
        // Handle confirm button click
        ProcessConfirmation();

        // Close UI
        JOLP_UIManagerComponent ui = JOLP_Global.GetUI();
        if (ui) ui.HideContext(JOLP_MyContext);
    }
}
```

---

## Dynamic Widget Creation

### Creating Widgets at Runtime

```cpp
class JOLP_ListContext : JOLP_UIContext
{
    protected Widget m_ListContainer;
    protected ref array<Widget> m_ListItems;

    override void OnShow()
    {
        super.OnShow();

        m_ListContainer = m_Root.FindAnyWidget("ListContainer");
        if (!m_ListContainer) return;

        m_ListItems = new array<Widget>();

        // Create list items dynamically
        array<string> items = GetListData();
        foreach (string item : items)
        {
            CreateListItem(item);
        }
    }

    protected void CreateListItem(string text)
    {
        // Create widget from template layout
        Widget itemWidget = GetGame().GetWorkspace().CreateWidgets(
            "{Path}/ListItemTemplate.layout",
            m_ListContainer
        );

        if (!itemWidget) return;

        // Configure widget
        TextWidget textWidget = TextWidget.Cast(itemWidget.FindAnyWidget("ItemText"));
        if (textWidget)
        {
            textWidget.SetText(text);
        }

        m_ListItems.Insert(itemWidget);
    }

    override void OnHide()
    {
        // Clean up created widgets
        foreach (Widget item : m_ListItems)
        {
            item.RemoveFromHierarchy();
        }
        m_ListItems.Clear();

        super.OnHide();
    }
}
```

---

## Context Lifecycle Management

### Modal Contexts

```cpp
class JOLP_ModalContext : JOLP_UIContext
{
    protected JOLP_UIContext m_PreviousContext;

    void SetPreviousContext(JOLP_UIContext previous)
    {
        m_PreviousContext = previous;
    }

    void CloseModal()
    {
        JOLP_UIManagerComponent ui = JOLP_Global.GetUI();
        if (!ui) return;

        // Hide this modal
        ui.HideContext(JOLP_ModalContext);

        // Restore previous context if needed
        if (m_PreviousContext)
        {
            ui.ShowContext(m_PreviousContext.Type());
        }
    }
}
```

### Context Stack

```cpp
// Show new context, hiding current
void PushContext(typename contextType)
{
    JOLP_UIManagerComponent ui = JOLP_Global.GetUI();
    if (!ui) return;

    typename currentType = ui.GetActiveContextType();

    // Show new context (automatically hides previous)
    ui.ShowContext(contextType);

    // Store previous for back navigation
    StoreContextHistory(currentType);
}

// Return to previous context
void PopContext()
{
    JOLP_UIManagerComponent ui = JOLP_Global.GetUI();
    if (!ui) return;

    typename previousType = GetPreviousContextType();
    if (previousType)
    {
        ui.ShowContext(previousType);
    }
}
```

---

## Input Handling

### Keyboard Input

```cpp
class JOLP_MyContext : JOLP_UIContext
{
    override void OnShow()
    {
        super.OnShow();

        // Register for input events
        GetGame().GetInputManager().AddActionListener("MenuBack", EActionTrigger.DOWN, OnMenuBack);
    }

    override void OnHide()
    {
        // Unregister input events
        GetGame().GetInputManager().RemoveActionListener("MenuBack", EActionTrigger.DOWN, OnMenuBack);

        super.OnHide();
    }

    protected void OnMenuBack()
    {
        // Handle back/escape key
        CloseContext();
    }

    protected void CloseContext()
    {
        JOLP_UIManagerComponent ui = JOLP_Global.GetUI();
        if (ui) ui.HideContext(JOLP_MyContext);
    }
}
```

---

## Server Communication from UI

### Requesting Server Actions

```cpp
class JOLP_ShopContext : JOLP_UIContext
{
    void OnPurchaseClicked(int itemId)
    {
        // Get player's controller
        JOLP_JustOneLastPatrolController controller = JOLP_Global.GetController();
        if (!controller) return;

        // Get shop component on controller
        JOLP_ShopServerComponent shopComponent = JOLP_ShopServerComponent.Cast(
            controller.FindComponent(JOLP_ShopServerComponent)
        );
        if (!shopComponent) return;

        // Request server to process purchase
        shopComponent.RequestPurchase(itemId);

        // UI will be updated via RPC callback when server responds
    }
}

// In JOLP_ShopServerComponent on JOLP_JustOneLastPatrolController
void RequestPurchase(int itemId)
{
    if (Replication.IsServer())
    {
        RpcAsk_Purchase(itemId);
    }
    else
    {
        Rpc(RpcAsk_Purchase, itemId);
    }
}

[RplRpc(RplChannel.Reliable, RplRcver.Server)]
void RpcAsk_Purchase(int itemId)
{
    // Server validates and processes purchase
    // ...

    // Send result back to client
    Rpc(RpcDo_PurchaseResult, success, message);
}

[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
void RpcDo_PurchaseResult(bool success, string message)
{
    // Update UI with result
    JOLP_UIManagerComponent ui = JOLP_Global.GetUI();
    if (!ui) return;

    JOLP_ShopContext context = JOLP_ShopContext.Cast(ui.GetContext(JOLP_ShopContext));
    if (context)
    {
        context.OnPurchaseResult(success, message);
    }
}
```

---

## Best Practices

### ✅ DO:

- **Clean up widgets:** Remove dynamically created widgets in OnHide()
- **Check GetUI():** May return null on server
- **Check GetContext():** May return null if not registered
- **Set data before showing:** Call SetData() before ShowContext()
- **Unregister input:** Remove input listeners in OnHide()
- **Use strong refs:** For widget and data references
- **Validate server requests:** Never trust client-side data

### ❌ DON'T:

- **Store IEntity in context:** Store EntityID instead
- **Skip null checks:** Always verify UI manager and context exist
- **Forget to clean up:** Memory leaks from widgets and event handlers
- **Block main thread:** Keep OnUpdate() lightweight
- **Assume UI exists:** Check GetUI() on both client and server code
- **Trust client data:** Validate all user input on server

---

## Testing UI Contexts

### Manual Testing
1. Open UI context from game
2. Verify layout loads correctly
3. Test all interactive elements (buttons, inputs)
4. Test data display updates
5. Test closing/hiding context
6. Verify cleanup (no lingering widgets)

### Edge Cases
1. Test with null/invalid data
2. Test rapid open/close
3. Test multiple contexts stacking
4. Test on both host and dedicated server
5. Test with missing layout files
6. Test with missing widgets in layout

---

## Common Issues

### Issue: Context doesn't show
**Cause:** Not registered with JOLP_UIManagerComponent
**Fix:** Add context to player prefab's UI manager component

### Issue: Widgets null after FindAnyWidget
**Cause:** Widget names in code don't match .layout file
**Fix:** Verify widget names match exactly (case-sensitive)

### Issue: UI shows on dedicated server
**Cause:** Not checking if UI manager exists
**Fix:** Always check `if (!JOLP_Global.GetUI()) return;`

### Issue: Memory leak from widgets
**Cause:** Not removing dynamically created widgets
**Fix:** Call RemoveFromHierarchy() in OnHide()

---

## Related Resources

- See `component-patterns.md` for JOLP_UIManagerComponent architecture
- See `networking.md` for server communication from UI
- See `memory-management.md` for widget cleanup
- See main `SKILL.md` for overview
