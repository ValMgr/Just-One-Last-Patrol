//------------------------------------------------------------------------------------------------
//! Civilian dialogue. Choices are built from JOLP_CivilianTalk.GetChoices().
class JOLP_CivilianTalkDialog : SCR_ConfigurableDialogUi
{
	protected static const ResourceName PRESETS = "{664B13AF00000164}Configs/UI/JOLP_CivilianTalkDialog.conf";
	protected static const string PRESET_TAG = "civilian_talk";
	protected static const ResourceName BUTTON_LAYOUT = "{87037226B1A2064B}UI/layouts/WidgetLibrary/Buttons/WLib_NavigationButtonSuperSmall.layout";

	protected static JOLP_CivilianTalkDialog s_Instance;

	protected RplId m_SpeakerId;
	protected ref array<SCR_InputButtonComponent> m_aButtons;
	protected ref array<int> m_aChoiceIds;

	//------------------------------------------------------------------------------------------------
	static void Open(RplId speakerId)
	{
		JOLP_CivilianTalkDialog dialog = new JOLP_CivilianTalkDialog();
		dialog.m_SpeakerId = speakerId;

		SCR_ConfigurableDialogUi opened = SCR_ConfigurableDialogUi.CreateFromPreset(PRESETS, PRESET_TAG, dialog);
		if (!opened)
			return;

		s_Instance = JOLP_CivilianTalkDialog.Cast(opened);
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
		JOLP_CivilianTalkNetworkComponent network = JOLP_CivilianTalkNetworkComponent.Local();
		if (network)
			network.Release(m_SpeakerId);

		m_aButtons = null;
		m_aChoiceIds = null;

		if (s_Instance == this)
			s_Instance = null;

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnMenuOpen(SCR_ConfigurableDialogUiPreset preset)
	{
		SetMessage("They wait.");
		BuildChoices();

		JOLP_CivilianTalkNetworkComponent network = JOLP_CivilianTalkNetworkComponent.Local();
		if (network)
			network.Hold(m_SpeakerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildChoices()
	{
		Widget options = null;
		if (GetRootWidget())
			options = GetRootWidget().FindAnyWidget("Options");

		if (!options)
		{
			Print("[JOLP] Civilian talk dialog is missing the Options list", LogLevel.WARNING);
			return;
		}

		array<int> ids = new array<int>();
		array<string> labels = new array<string>();
		JOLP_CivilianTalk.GetChoices(ids, labels);

		m_aButtons = new array<SCR_InputButtonComponent>();
		m_aChoiceIds = new array<int>();

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		for (int i = 0; i < ids.Count(); i++)
		{
			if (i >= labels.Count())
				break;

			Widget widget = workspace.CreateWidgets(BUTTON_LAYOUT, options);
			if (!widget)
				continue;

			AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Stretch);
			AlignableSlot.SetPadding(widget, 0, 4, 0, 4);

			SCR_InputButtonComponent button = SCR_InputButtonComponent.Cast(widget.FindHandler(SCR_InputButtonComponent));
			if (!button)
				continue;

			button.SetLabel(labels[i]);
			button.m_OnActivated.Insert(OnChoice);
			m_aButtons.Insert(button);
			m_aChoiceIds.Insert(ids[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnChoice(SCR_InputButtonComponent button)
	{
		if (!m_aButtons || !m_aChoiceIds)
			return;

		for (int i = 0; i < m_aButtons.Count(); i++)
		{
			if (m_aButtons[i] != button)
				continue;

			if (i >= m_aChoiceIds.Count())
				return;

			JOLP_CivilianTalkNetworkComponent network = JOLP_CivilianTalkNetworkComponent.Local();
			if (!network)
			{
				SetMessage("They don't answer.");
				return;
			}

			SetMessage("...");
			network.Ask(m_aChoiceIds[i], m_SpeakerId);
			return;
		}
	}
}
