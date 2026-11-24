//------------------------------------------------------------------------------------------------
//! Admin tools. The rows are built from JOLP_AdminCatalog. A tool with children replaces the list.
class JOLP_AdminDialog : SCR_ConfigurableDialogUi
{
	protected static const ResourceName PRESETS = "{664B13B0A1000760}Configs/UI/JOLP_AdminDialog.conf";
	protected static const string PRESET_TAG = "admin";
	protected static const ResourceName BUTTON_LAYOUT = "{87037226B1A2064B}UI/layouts/WidgetLibrary/Buttons/WLib_NavigationButtonSuperSmall.layout";

	protected static JOLP_AdminDialog s_Instance;

	protected ref array<ref JOLP_AdminAction> m_aRoot;
	protected ref array<ref JOLP_AdminAction> m_aStack;
	protected ref array<ref JOLP_AdminAction> m_aPage;
	protected ref array<SCR_InputButtonComponent> m_aButtons;
	protected SCR_InputButtonComponent m_BackButton;

	//------------------------------------------------------------------------------------------------
	static void Open()
	{
		if (s_Instance)
			return;

		JOLP_AdminDialog dialog = new JOLP_AdminDialog();
		SCR_ConfigurableDialogUi opened = SCR_ConfigurableDialogUi.CreateFromPreset(PRESETS, PRESET_TAG, dialog);
		if (!opened)
			return;

		s_Instance = JOLP_AdminDialog.Cast(opened);
	}

	//------------------------------------------------------------------------------------------------
	static void Deliver(string text)
	{
		if (!s_Instance || text.IsEmpty())
			return;

		s_Instance.SetMessage(text);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		ReleaseRows();
		m_aRoot = null;
		m_aStack = null;
		m_aPage = null;

		if (s_Instance == this)
			s_Instance = null;

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnMenuOpen(SCR_ConfigurableDialogUiPreset preset)
	{
		m_aRoot = new array<ref JOLP_AdminAction>();
		m_aStack = new array<ref JOLP_AdminAction>();
		JOLP_AdminCatalog.GetActions(m_aRoot);
		ShowPage();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowPage()
	{
		ReleaseRows();

		ref array<ref JOLP_AdminAction> source = null;
		JOLP_AdminAction parent = CurrentParent();
		if (parent)
		{
			source = new array<ref JOLP_AdminAction>();
			parent.GetChildren(source);
			SetMessage(parent.GetPageHint());
		}
		else
		{
			source = m_aRoot;
			SetMessage("Choose a tool.");
		}

		Widget options = null;
		if (GetRootWidget())
			options = GetRootWidget().FindAnyWidget("Options");

		if (!options)
		{
			Print("[JOLP] Admin dialog is missing the Options list", LogLevel.WARNING);
			return;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_aPage = new array<ref JOLP_AdminAction>();
		m_aButtons = new array<SCR_InputButtonComponent>();

		if (parent)
			m_BackButton = CreateButton(workspace, options, "Back", true);

		if (!source)
			return;

		foreach (JOLP_AdminAction action : source)
		{
			if (!action)
				continue;

			SCR_InputButtonComponent button = CreateButton(workspace, options, action.GetLabel(), false);
			if (!button)
				continue;

			m_aButtons.Insert(button);
			m_aPage.Insert(action);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_InputButtonComponent CreateButton(WorkspaceWidget workspace, Widget parent, string label, bool back)
	{
		Widget widget = workspace.CreateWidgets(BUTTON_LAYOUT, parent);
		if (!widget)
			return null;

		AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(widget, 0, 4, 0, 4);

		SCR_InputButtonComponent button = SCR_InputButtonComponent.Cast(widget.FindHandler(SCR_InputButtonComponent));
		if (!button)
			return null;

		button.SetLabel(label);
		if (back)
			button.m_OnActivated.Insert(OnBack);
		else
			button.m_OnActivated.Insert(OnChoice);

		return button;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnChoice(SCR_InputButtonComponent button)
	{
		if (!m_aButtons || !m_aPage)
			return;

		for (int i = 0; i < m_aButtons.Count(); i++)
		{
			if (m_aButtons[i] != button)
				continue;

			if (i >= m_aPage.Count())
				return;

			JOLP_AdminAction action = m_aPage[i];
			if (!action)
				return;

			if (action.HasChildren())
			{
				m_aStack.Insert(action);
				ShowPage();
				return;
			}

			action.Perform(this);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBack(SCR_InputButtonComponent button)
	{
		if (!m_aStack || m_aStack.IsEmpty())
		{
			ShowPage();
			return;
		}

		m_aStack.Remove(m_aStack.Count() - 1);
		ShowPage();
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_AdminAction CurrentParent()
	{
		if (!m_aStack || m_aStack.IsEmpty())
			return null;

		return m_aStack[m_aStack.Count() - 1];
	}

	//------------------------------------------------------------------------------------------------
	protected void ReleaseRows()
	{
		if (m_aButtons)
		{
			foreach (SCR_InputButtonComponent button : m_aButtons)
			{
				if (button)
					button.m_OnActivated.Remove(OnChoice);
			}
		}

		if (m_BackButton)
			m_BackButton.m_OnActivated.Remove(OnBack);

		m_aButtons = null;
		m_BackButton = null;

		Widget options = null;
		if (GetRootWidget())
			options = GetRootWidget().FindAnyWidget("Options");

		if (!options)
			return;

		Widget child = options.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}
	}
}
