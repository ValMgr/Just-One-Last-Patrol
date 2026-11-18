//------------------------------------------------------------------------------------------------
//! One catalog action placed on the vanilla radial wheel.
class JOLP_SelfRadialEntry : SCR_SelectionMenuEntry
{
	protected static const float REPEAT_BLOCK_MS = 100;

	protected ref JOLP_SelfRadialAction m_Action;
	protected float m_fLastPerform = -1;

	//------------------------------------------------------------------------------------------------
	void Bind(notnull JOLP_SelfRadialAction action, IEntity user)
	{
		m_Action = action;
		SetId(action.GetId());
		SetName(action.GetLabel());
		SetDescription(action.GetDescription());

		ResourceName imageset;
		string image;
		action.GetIcon(imageset, image);
		if (!image.IsEmpty())
			SetIcon(imageset, image);

		Enable(user && action.CanPerform(user));
	}

	//------------------------------------------------------------------------------------------------
	override void SetEntryComponent(SCR_SelectionMenuEntryComponent entryComponent)
	{
		super.SetEntryComponent(entryComponent);

		if (!m_EntryComponent)
			return;

		// The base click runs the entry and leaves the wheel open. Send the click through the menu so it closes.
		m_EntryComponent.GetOnClick().Remove(OnEntryClick);
		m_EntryComponent.GetOnClick().Insert(OnSliceClicked);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSliceClicked(SCR_SelectionMenuEntryComponent entryComponent)
	{
		if (!IsEnabled())
			return;

		SCR_RadialMenu menu = SCR_RadialMenu.GetOpenedRadialMenu();
		if (!menu)
			return;

		menu.PerformEntry(this);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPerform()
	{
		if (!m_Action)
			return;

		BaseWorld world = GetGame().GetWorld();
		float now = 0;
		if (world)
			now = world.GetWorldTime();

		if (m_fLastPerform >= 0 && now - m_fLastPerform < REPEAT_BLOCK_MS)
			return;

		IEntity user = SCR_PlayerController.GetLocalControlledEntity();
		if (!user || !m_Action.CanPerform(user))
			return;

		m_fLastPerform = now;
		m_Action.Perform(user);
	}
}
