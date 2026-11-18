//------------------------------------------------------------------------------------------------
//! Asks the player for the name of the FOB the command truck will become.
class JOLP_FOBNameDialog : SCR_ConfigurableDialogUi
{
	protected static const ResourceName PRESETS = "{664B13B0A1000720}Configs/UI/JOLP_FOBNameDialog.conf";
	protected static const string PRESET_TAG = "fob_name";

	protected static JOLP_FOBNameDialog s_Instance;

	protected RplId m_VehicleId;
	protected EditBoxWidget m_NameInput;
	protected bool m_bWaiting;

	//------------------------------------------------------------------------------------------------
	static void Open(RplId vehicleId)
	{
		if (s_Instance)
			return;

		JOLP_FOBNameDialog dialog = new JOLP_FOBNameDialog();
		dialog.m_VehicleId = vehicleId;

		SCR_ConfigurableDialogUi opened = SCR_ConfigurableDialogUi.CreateFromPreset(PRESETS, PRESET_TAG, dialog);
		if (!opened)
			return;

		s_Instance = JOLP_FOBNameDialog.Cast(opened);
	}

	//------------------------------------------------------------------------------------------------
	static void Deliver(bool established, string message)
	{
		if (!s_Instance)
			return;

		if (established)
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
			Print("[JOLP] FOB name dialog is missing NameInput", LogLevel.WARNING);
			return;
		}

		m_NameInput.SetVirtualKeyboardTitle("FOB name");

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

		if (name.Length() > JOLP_FOBDeployNetworkComponent.MAX_NAME_LENGTH)
		{
			SetMessage("That name is too long.");
			return;
		}

		JOLP_FOBDeployNetworkComponent network = JOLP_FOBDeployNetworkComponent.Local();
		if (!network)
		{
			SetMessage("Cannot reach the server.");
			return;
		}

		m_bWaiting = true;
		SetMessage("Establishing...");
		network.AskDeploy(m_VehicleId, name);
	}
}
