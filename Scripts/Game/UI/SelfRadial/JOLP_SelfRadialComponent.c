//------------------------------------------------------------------------------------------------
//! Same wheel visuals as the vanilla radial, but Show() is not dropped when the HUD visibility flags say no.
class JOLP_SelfRadialDisplay : SCR_RadialMenuDisplay
{
	//------------------------------------------------------------------------------------------------
	void BindCreatedRoot(IEntity owner)
	{
		if (!GetRootWidget())
			return;

		DisplayStartDraw(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void Show(bool show, float speed = UIConstants.FADE_RATE_INSTANT, EAnimationCurve curve = EAnimationCurve.LINEAR)
	{
		super.Show(show, speed, curve);

		Widget root = GetRootWidget();
		if (!root)
			return;

		root.SetVisible(show);
		if (show)
			root.SetOpacity(1);
	}
}

//------------------------------------------------------------------------------------------------
//! Own wheel. The shared one activates RadialMenuContext, which replaces mouse look and flips its Y axis.
class JOLP_SelfRadialMenu : SCR_RadialMenu
{
	//------------------------------------------------------------------------------------------------
	void Configure()
	{
		SCR_RadialMenuInputs inputs = new SCR_RadialMenuInputs();
		inputs.m_sContext = string.Empty;
		inputs.m_sBackAction = string.Empty;
		inputs.m_sPerformAction = string.Empty;
		inputs.m_fMouseSelectionTreshold = 80;
		inputs.m_bDynamicMouseTreshold = true;
		inputs.m_iContextDeactivationTime = 0;
		m_Inputs = inputs;
	}

	//------------------------------------------------------------------------------------------------
	override void Update(float timeSlice)
	{
		if (!IsOpened())
			return;

		OnUpdate(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	void ReleaseDisplay()
	{
		m_Display = null;
	}
}

//------------------------------------------------------------------------------------------------
//! Opens and closes our radial wheel from Ctrl+T.
[BaseContainerProps()]
class JOLP_SelfRadialController : SCR_RadialMenuController
{
	protected bool m_bWatchingClose;
	protected float m_fLastToggle = -1;
	protected ref JOLP_SelfRadialMenu m_Menu;
	protected SCR_SelectionMenuDisplay m_BorrowedDisplay;

	//------------------------------------------------------------------------------------------------
	void Prepare(IEntity owner)
	{
		m_Owner = owner;
		m_sEnableControl = true;

		m_Menu = new JOLP_SelfRadialMenu();
		m_Menu.Configure();
		m_RadialMenuToControl = m_Menu;

		SCR_RadialMenuControllerInputs controls = new SCR_RadialMenuControllerInputs();
		controls.m_sOpenAction = "JOLP_SelfRadial";
		controls.m_bCloseOnReleaseOpen = false;
		controls.m_bPerformOnClose = false;
		controls.m_bCloseOnPerform = true;
		controls.m_bOpenInRoot = true;
		controls.m_bPreventEmptyMenuOpen = true;
		controls.m_bShowWhileUnconcious = false;
		controls.m_bDeselectInCenter = true;
		controls.m_bUseLargeSize = true;
		controls.m_fCustomSize = -1;
		controls.m_bUseQuickActions = false;
		m_RMControls = controls;

		// A stuck shared wheel keeps RadialMenuContext up and remaps the mouse.
		SCR_RadialMenu stuck = SCR_RadialMenu.GetOpenedRadialMenu();
		if (stuck && stuck != m_Menu)
		{
			stuck.SetPreventSelectionContext(true);
			stuck.Close();
			if (!stuck.IsOpened())
				stuck.SetPreventSelectionContext(false);
		}
	}

	//------------------------------------------------------------------------------------------------
	void Update(float timeSlice)
	{
		if (m_Menu)
			m_Menu.Update(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	void ConfirmSelection()
	{
		if (!m_RadialMenu || !m_RadialMenu.IsOpened())
			return;

		SCR_SelectionMenuEntry entry = m_RadialMenu.GetSelectionEntry();
		if (!entry || !entry.IsEnabled())
			return;

		m_RadialMenu.PerformEntry(entry);
	}

	//------------------------------------------------------------------------------------------------
	void Shutdown()
	{
		if (GetGame() && IsMenuOpen())
			CloseMenu();

		ReleaseWheel();
	}

	//------------------------------------------------------------------------------------------------
	override void OnInputOpen()
	{
		if (!m_sEnableControl || !m_RMControls || !m_Menu)
			return;

		float now = 0;
		BaseWorld world = GetGame().GetWorld();
		if (world)
			now = world.GetWorldTime();

		if (m_fLastToggle >= 0 && now - m_fLastToggle < 250)
			return;

		m_fLastToggle = now;

		if (IsMenuOpen() && HasControl())
		{
			CloseMenu();
			return;
		}

		SCR_RadialMenu opened = SCR_RadialMenu.GetOpenedRadialMenu();
		if (opened && opened != m_Menu)
			opened.Close();

		if (IsCharacterBlocked())
		{
			Print("[JOLP] Self radial stayed closed: no conscious character.", LogLevel.WARNING);
			return;
		}

		array<ref SCR_SelectionMenuEntry> entries = {};
		BuildEntries(entries);
		if (entries.IsEmpty())
		{
			Print("[JOLP] Self radial stayed closed: no actions.", LogLevel.WARNING);
			return;
		}

		if (!HasControl())
			Control(m_Owner, m_Menu);

		if (!m_RadialMenu)
		{
			Print("[JOLP] Self radial stayed closed: wheel has no controller.", LogLevel.WARNING);
			return;
		}

		if (!BorrowDisplay())
		{
			ReleaseWheel();
			Print("[JOLP] Self radial stayed closed: HUD has no radial display.", LogLevel.WARNING);
			return;
		}

		m_RadialMenu.AddEntries(entries, true);
		if (m_RadialMenu.GetEntryCount() == 0)
		{
			ReleaseWheel();
			Print("[JOLP] Self radial stayed closed: entries were dropped.", LogLevel.WARNING);
			return;
		}

		WatchClose();
		m_RadialMenu.Open();

		if (!m_RadialMenu.IsOpened())
		{
			ReleaseWheel();
			Print("[JOLP] Self radial stayed closed: Open() did not take.", LogLevel.WARNING);
			return;
		}

		ForceDisplayVisible();
		Print("[JOLP] Self radial opened.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildEntries(notnull array<ref SCR_SelectionMenuEntry> entries)
	{
		entries.Clear();

		IEntity user = SCR_PlayerController.GetLocalControlledEntity();
		if (!user)
			return;

		array<ref JOLP_SelfRadialAction> actions = {};
		JOLP_SelfRadialCatalog.GetActions(actions);

		foreach (JOLP_SelfRadialAction action : actions)
		{
			if (!action || !action.CanShow(user))
				continue;

			JOLP_SelfRadialEntry entry = new JOLP_SelfRadialEntry();
			entry.Bind(action, user);
			entries.Insert(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsCharacterBlocked()
	{
		if (!m_RMControls || m_RMControls.m_bShowWhileUnconcious)
			return false;

		IEntity controlled = SCR_PlayerController.GetLocalControlledEntity();
		if (!controlled)
			return true;

		CharacterControllerComponent character = CharacterControllerComponent.Cast(controlled.FindComponent(CharacterControllerComponent));
		return character && character.IsUnconscious();
	}

	//------------------------------------------------------------------------------------------------
	protected void WatchClose()
	{
		if (m_bWatchingClose || !m_RadialMenu)
			return;

		m_RadialMenu.GetOnClose().Insert(OnWheelClosed);
		m_bWatchingClose = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UnwatchClose()
	{
		if (!m_bWatchingClose)
			return;

		if (m_RadialMenu)
			m_RadialMenu.GetOnClose().Remove(OnWheelClosed);

		m_bWatchingClose = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnWheelClosed(SCR_SelectionMenu menu)
	{
		ReleaseWheel();
	}

	//------------------------------------------------------------------------------------------------
	protected bool BorrowDisplay()
	{
		if (m_Menu && m_Menu.HasDisplay())
			return true;

		if (!m_Menu)
			return false;

		SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.Cast(m_Owner.FindComponent(SCR_HUDManagerComponent));
		if (!hud)
			hud = GetGame().GetHUDManager();

		if (!hud)
			return false;

		m_BorrowedDisplay = SCR_SelectionMenuDisplay.Cast(hud.FindInfoDisplay(JOLP_SelfRadialDisplay));
		if (!m_BorrowedDisplay)
			m_BorrowedDisplay = SCR_SelectionMenuDisplay.Cast(hud.FindInfoDisplay(SCR_RadialMenuDisplay));

		if (!m_BorrowedDisplay)
			return false;

		JOLP_SelfRadialDisplay ownDisplay = JOLP_SelfRadialDisplay.Cast(m_BorrowedDisplay);
		if (ownDisplay && !ownDisplay.GetRootWidget())
		{
			Widget root = hud.CreateLayout(ownDisplay.m_LayoutPath, EHudLayers.ALWAYS_TOP);
			if (root)
			{
				ownDisplay.SetRootWidget(root);
				ownDisplay.BindCreatedRoot(m_Owner);
			}
		}

		if (!m_BorrowedDisplay.GetRootWidget())
			return false;

		m_Menu.SetMenuDisplay(m_BorrowedDisplay);
		return m_Menu.HasDisplay();
	}

	//------------------------------------------------------------------------------------------------
	protected void ForceDisplayVisible()
	{
		if (!m_BorrowedDisplay)
			return;

		Widget root = m_BorrowedDisplay.GetRootWidget();
		if (!root)
			return;

		root.SetVisible(true);
		root.SetOpacity(1);

		Widget parent = root.GetParent();
		if (!parent)
			return;

		parent.SetVisible(true);
		parent.SetOpacity(1);
	}

	//------------------------------------------------------------------------------------------------
	protected void RestoreDisplay()
	{
		if (!m_BorrowedDisplay)
			return;

		SCR_RadialMenu globalMenu = SCR_RadialMenu.GlobalRadialMenu();
		if (globalMenu && globalMenu != m_Menu)
			globalMenu.SetMenuDisplay(m_BorrowedDisplay);

		m_BorrowedDisplay = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void ReleaseWheel()
	{
		UnwatchClose();

		if (m_RadialMenu)
		{
			SCR_RadialMenu menu = m_RadialMenu;
			menu.GetOnControllerChanged().Remove(OnMenuControllerChanged);
			menu.ClearEntries();
			StopControl();
		}

		if (m_Menu)
			m_Menu.ReleaseDisplay();

		RestoreDisplay();
	}
}

//------------------------------------------------------------------------------------------------
class JOLP_SelfRadialComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Local player controller. Ctrl+T toggles the self-interaction wheel.
//! JOLP_SelfRadial lives on JOLP_GameplayContext, which stays active during play.
class JOLP_SelfRadialComponent : ScriptComponent
{
	protected ref JOLP_SelfRadialController m_Controller;
	protected bool m_bBound;
	protected bool m_bActionDown;
	protected bool m_bConfirmDown;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		// Workbench play is a local server, so a dedicated-server skip never reaches the player.
		SetEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_bBound && GetGame() && GetGame().GetInputManager())
			GetGame().GetInputManager().RemoveActionListener("JOLP_SelfRadial", EActionTrigger.DOWN, OnSelfRadial);

		if (m_Controller)
			m_Controller.Shutdown();

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		PlayerController local = GetGame().GetPlayerController();
		if (!local)
			return;

		if (local != PlayerController.Cast(owner))
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		if (!m_bBound)
		{
			m_Controller = new JOLP_SelfRadialController();
			m_Controller.Prepare(owner);
			input.AddActionListener("JOLP_SelfRadial", EActionTrigger.DOWN, OnSelfRadial);
			m_bBound = true;
			Print("[JOLP] Self radial ready. Default bind is Ctrl+T.", LogLevel.NORMAL);
		}

		// Above VON (priority 110), which owns T. Overlay, so look and voice keep working.
		// The cursor context is only up while the wheel is open.
		if (m_Controller.IsMenuOpen())
			input.ActivateContext("JOLP_SelfRadialCursor", 250);
		else
			input.ActivateContext("JOLP_GameplayContext", 250);

		bool down = input.GetActionValue("JOLP_SelfRadial") > 0.5;
		if (down && !m_bActionDown)
			m_Controller.OnInputOpen();

		m_bActionDown = down;

		if (m_Controller.IsMenuOpen())
		{
			bool click = input.GetActionValue("MouseLeft") > 0.5;
			if (click && !m_bConfirmDown)
				m_Controller.ConfirmSelection();

			m_bConfirmDown = click;
		}
		else
		{
			m_bConfirmDown = false;
		}

		m_Controller.Update(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSelfRadial(float value, EActionTrigger reason, string actionName)
	{
		if (m_Controller)
			m_Controller.OnInputOpen();
	}
}
