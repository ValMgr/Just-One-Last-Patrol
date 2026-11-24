//------------------------------------------------------------------------------------------------
//! Lists this player's kits. A row shows the saved equipment and the supply difference.
class JOLP_LoadoutsDialog : SCR_ConfigurableDialogUi
{
	protected static const ResourceName PRESETS = "{664B13B0A1000865}Configs/UI/JOLP_LoadoutsDialog.conf";
	protected static const string PRESET_TAG = "loadouts";
	protected static const ResourceName BUTTON_LAYOUT = "{664B13B0A1000890}UI/Layouts/Loadout/LoadoutRow.layout";

	protected static JOLP_LoadoutsDialog s_Instance;

	protected RplId m_ArsenalId;
	protected TextWidget m_Detail;
	protected RichTextWidget m_Cost;
	protected ref array<SCR_ButtonBaseComponent> m_aButtons;
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
		s_Instance = dialog;

		SCR_ConfigurableDialogUi opened = SCR_ConfigurableDialogUi.CreateFromPreset(PRESETS, PRESET_TAG, dialog);
		if (!opened)
		{
			s_Instance = null;
			return;
		}

		s_Instance = JOLP_LoadoutsDialog.Cast(opened);
		if (!s_Instance)
			return;

		// The reply can arrive inside CreateFromPreset, before this returns.
		// Ask again once the list widget is in the tree.
		s_Instance.Refresh();
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
	static void DeliverDetail(bool ok, string message, string detail, string cost)
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
		s_Instance.SetCost(cost);
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
		s_Instance.SetCost("");
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
		m_Cost = null;
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
		{
			if (!packed.IsEmpty())
				SetMessage(packed);
			return;
		}

		if (m_aNames.IsEmpty())
		{
			SetDetail("You have no saved loadouts.");
			SetCost("");
			return;
		}

		for (int i = 0; i < m_aNames.Count(); i++)
		{
			Widget widget = workspace.CreateWidgets(BUTTON_LAYOUT, list);
			if (!widget)
				continue;

			AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Left);
			AlignableSlot.SetPadding(widget, 0, 2, 0, 2);

			SCR_ButtonComponent button = SCR_ButtonComponent.Cast(widget.FindHandler(SCR_ButtonComponent));
			if (!button)
				continue;

			SetRowLabel(button, m_aNames[i]);
			button.m_OnClicked.Insert(OnRow);
			m_aButtons.Insert(button);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRow(SCR_ButtonBaseComponent button)
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
			SetCost("");
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
				SetRowLabel(m_aButtons[i], "> " + m_aNames[i]);
			else
				SetRowLabel(m_aButtons[i], m_aNames[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SetDetail(string text)
	{
		Widget root = GetRootWidget();
		if (root)
			m_Detail = TextWidget.Cast(root.FindAnyWidget("Detail"));

		if (m_Detail)
		{
			m_Detail.SetText(text);
			return;
		}

		RichTextWidget rich = null;
		if (root)
			rich = RichTextWidget.Cast(root.FindAnyWidget("Detail"));

		if (rich)
		{
			rich.SetText(text);
			return;
		}

		SetMessage(text);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetRowLabel(SCR_ButtonBaseComponent button, string label)
	{
		if (!button)
			return;

		Widget root = button.GetRootWidget();
		if (!root)
			return;

		TextWidget name = TextWidget.Cast(root.FindAnyWidget("Name"));
		if (name)
			name.SetText(label);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetCost(string text)
	{
		Widget root = GetRootWidget();
		if (root)
			m_Cost = RichTextWidget.Cast(root.FindAnyWidget("Cost"));

		if (m_Cost)
			m_Cost.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearRows()
	{
		m_aButtons = new array<SCR_ButtonBaseComponent>();
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
