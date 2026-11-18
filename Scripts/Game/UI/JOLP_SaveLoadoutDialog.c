//------------------------------------------------------------------------------------------------
//! Asks for a name, then the server snapshots what the player is wearing.
class JOLP_SaveLoadoutDialog : SCR_ConfigurableDialogUi
{
	protected static const ResourceName PRESETS = "{664B13B0A1000860}Configs/UI/JOLP_SaveLoadoutDialog.conf";
	protected static const string PRESET_TAG = "save_loadout";

	protected static JOLP_SaveLoadoutDialog s_Instance;

	protected RplId m_ArsenalId;
	protected EditBoxWidget m_NameInput;
	protected bool m_bWaiting;

	//------------------------------------------------------------------------------------------------
	static void Open(RplId arsenalId)
	{
		if (s_Instance)
			return;

		JOLP_SaveLoadoutDialog dialog = new JOLP_SaveLoadoutDialog();
		dialog.m_ArsenalId = arsenalId;

		SCR_ConfigurableDialogUi opened = SCR_ConfigurableDialogUi.CreateFromPreset(PRESETS, PRESET_TAG, dialog);
		if (!opened)
			return;

		s_Instance = JOLP_SaveLoadoutDialog.Cast(opened);
	}

	//------------------------------------------------------------------------------------------------
	static void Deliver(bool saved, string message)
	{
		if (!s_Instance)
			return;

		if (saved)
		{
			s_Instance.Close();
			return;
		}

		s_Instance.m_bWaiting = false;
		if (!message.IsEmpty())
			s_Instance.SetMessage(message);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		m_NameInput = null;
		m_bWaiting = false;

		if (s_Instance == this)
			s_Instance = null;

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnMenuOpen(SCR_ConfigurableDialogUiPreset preset)
	{
		Widget root = GetRootWidget();
		if (root)
			m_NameInput = EditBoxWidget.Cast(root.FindAnyWidget("NameInput"));

		if (!m_NameInput)
		{
			Print("[JOLP] Save loadout dialog is missing NameInput", LogLevel.WARNING);
			return;
		}

		m_NameInput.SetVirtualKeyboardTitle("Loadout name");

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace)
			workspace.SetFocusedWidget(m_NameInput);

		m_NameInput.ActivateWriteMode();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnConfirm()
	{
		if (m_bWaiting)
			return;

		string name = string.Empty;
		if (m_NameInput)
			name = m_NameInput.GetText();

		name = name.Trim();
		if (name.IsEmpty())
		{
			SetMessage("Enter a name.");
			return;
		}

		if (name.Length() > JOLP_LoadoutManagerComponent.MAX_NAME_LENGTH)
		{
			SetMessage("That name is too long.");
			return;
		}

		JOLP_LoadoutNetworkComponent network = JOLP_LoadoutNetworkComponent.Local();
		if (!network)
		{
			SetMessage("Cannot reach the server.");
			return;
		}

		m_bWaiting = true;
		SetMessage("Saving...");
		network.AskSave(m_ArsenalId, name);
	}
}
