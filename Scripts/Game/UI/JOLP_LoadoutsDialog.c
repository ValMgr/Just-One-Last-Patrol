//------------------------------------------------------------------------------------------------
//! Lists this player's kits. A row shows the saved equipment and the supply difference.
class JOLP_LoadoutsDialog : SCR_ConfigurableDialogUi
{
	protected static const ResourceName PRESETS = "{664B13B0A1000865}Configs/UI/JOLP_LoadoutsDialog.conf";
	protected static const string PRESET_TAG = "loadouts";
	protected static const ResourceName BUTTON_LAYOUT = "{87037226B1A2064B}UI/layouts/WidgetLibrary/Buttons/WLib_NavigationButtonSuperSmall.layout";

	protected static JOLP_LoadoutsDialog s_Instance;

	protected RplId m_ArsenalId;
	protected TextWidget m_Detail;
	protected ref array<SCR_InputButtonComponent> m_aButtons;
	protected ref array<string> m_aNames;
	protected string m_sSelected;
	protected bool m_bBusy;

	//------------------------------------------------------------------------------------------------
	static void Open(RplId arsenalId)
	{
		if (s_Instance)
			return;

		JOLP_LoadoutsDialog dialog = new JOLP_LoadoutsDialog();
		dialog.m_ArsenalId = arsenalId;

		SCR_ConfigurableDialogUi opened = SCR_ConfigurableDialogUi.CreateFromPreset(PRESETS, PRESET_TAG, dialog);
		if (!opened)
			return;

		s_Instance = JOLP_LoadoutsDialog.Cast(opened);
	}

	//------------------------------------------------------------------------------------------------
	static void DeliverList(bool ok, string message, string names)
	{
		if (!s_Instance)
			return;

		s_Instance.m_bBusy = false;
		if (!ok)
		{
			s_Instance.SetMessage(message);
			return;
		}

		s_Instance.ShowNames(names);
	}

	//------------------------------------------------------------------------------------------------
	static void DeliverDetail(bool ok, string message, string detail)
	{
		if (!s_Instance)
			return;

		if (!ok)
		{
			s_Instance.SetMessage(message);
			return;
		}

		s_Instance.SetMessage("");
		s_Instance.SetDetail(detail);
	}

	//------------------------------------------------------------------------------------------------
	static void DeliverDelete(bool deleted, string message)
	{
		if (!s_Instance)
			return;

		s_Instance.m_bBusy = false;
		if (!deleted)
		{
			if (!message.IsEmpty())
				s_Instance.SetMessage(message);
			return;
		}

		s_Instance.m_sSelected = string.Empty;
		s_Instance.SetDetail("Select a loadout.");
		s_Instance.Refresh();
	}

	//------------------------------------------------------------------------------------------------
	static void DeliverApply(bool applied, string message)
	{
		if (!s_Instance)
			return;

		s_Instance.m_bBusy = false;
		if (applied)
		{
			s_Instance.Close();
			return;
		}

		if (!message.IsEmpty())
			s_Instance.SetMessage(message);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		m_Detail = null;
		m_aButtons = null;
		m_aNames = null;
		m_bBusy = false;

		if (s_Instance == this)
			s_Instance = null;

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnMenuOpen(SCR_ConfigurableDialogUiPreset preset)
	{
		Widget root = GetRootWidget();
		if (root)
			m_Detail = TextWidget.Cast(root.FindAnyWidget("Detail"));

		SetDetail("Select a loadout.");
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnButtonPressed(SCR_InputButtonComponent button)
	{
		string tag = GetButtonTag(button);
		if (tag == "load")
		{
			OnLoad();
			return;
		}

		if (tag == "delete")
		{
			OnDelete();
			return;
		}

		super.OnButtonPressed(button);
	}

	//------------------------------------------------------------------------------------------------
	protected void Refresh()
	{
		JOLP_LoadoutNetworkComponent network = JOLP_LoadoutNetworkComponent.Local();
		if (!network)
		{
			SetMessage("Cannot reach the server.");
			return;
		}

		network.AskList(m_ArsenalId);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowNames(string packed)
	{
		ClearRows();

		if (!packed.IsEmpty())
			packed.Split("\n", m_aNames, true);

		Widget list = ListWidget();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!list || !workspace)
			return;

		if (m_aNames.IsEmpty())
		{
			SetDetail("You have no saved loadouts.");
			return;
		}

		for (int i = 0; i < m_aNames.Count(); i++)
		{
			Widget widget = workspace.CreateWidgets(BUTTON_LAYOUT, list);
			if (!widget)
				continue;

			AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Stretch);
			AlignableSlot.SetPadding(widget, 0, 4, 0, 4);

			SCR_InputButtonComponent button = SCR_InputButtonComponent.Cast(widget.FindHandler(SCR_InputButtonComponent));
			if (!button)
				continue;

			button.SetLabel(m_aNames[i]);
			button.m_OnActivated.Insert(OnRow);
			m_aButtons.Insert(button);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRow(SCR_InputButtonComponent button)
	{
		if (m_bBusy || !m_aButtons || !m_aNames)
			return;

		for (int i = 0; i < m_aButtons.Count(); i++)
		{
			if (m_aButtons[i] != button)
				continue;

			if (i >= m_aNames.Count())
				return;

			m_sSelected = m_aNames[i];
			MarkSelected();

			JOLP_LoadoutNetworkComponent network = JOLP_LoadoutNetworkComponent.Local();
			if (!network)
			{
				SetMessage("Cannot reach the server.");
				return;
			}

			SetDetail("Checking price...");
			network.AskDetail(m_ArsenalId, m_sSelected);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnLoad()
	{
		if (m_bBusy)
			return;

		if (m_sSelected.IsEmpty())
		{
			SetMessage("Select a loadout.");
			return;
		}

		JOLP_LoadoutNetworkComponent network = JOLP_LoadoutNetworkComponent.Local();
		if (!network)
		{
			SetMessage("Cannot reach the server.");
			return;
		}

		m_bBusy = true;
		SetMessage("Equipping...");
		network.AskApply(m_ArsenalId, m_sSelected);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDelete()
	{
		if (m_bBusy)
			return;

		if (m_sSelected.IsEmpty())
		{
			SetMessage("Select a loadout.");
			return;
		}

		JOLP_LoadoutNetworkComponent network = JOLP_LoadoutNetworkComponent.Local();
		if (!network)
		{
			SetMessage("Cannot reach the server.");
			return;
		}

		m_bBusy = true;
		SetMessage("Deleting...");
		network.AskDelete(m_ArsenalId, m_sSelected);
	}

	//------------------------------------------------------------------------------------------------
	protected void MarkSelected()
	{
		if (!m_aButtons || !m_aNames)
			return;

		for (int i = 0; i < m_aButtons.Count(); i++)
		{
			if (i >= m_aNames.Count() || !m_aButtons[i])
				continue;

			if (m_aNames[i] == m_sSelected)
				m_aButtons[i].SetLabel("> " + m_aNames[i]);
			else
				m_aButtons[i].SetLabel(m_aNames[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SetDetail(string text)
	{
		if (m_Detail)
			m_Detail.SetText(text);
		else
			SetMessage(text);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearRows()
	{
		m_aButtons = new array<SCR_InputButtonComponent>();
		m_aNames = new array<string>();

		Widget list = ListWidget();
		if (!list)
			return;

		Widget child = list.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected Widget ListWidget()
	{
		Widget root = GetRootWidget();
		if (!root)
			return null;

		return root.FindAnyWidget("LoadoutList");
	}
}
