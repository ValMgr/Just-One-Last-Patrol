//------------------------------------------------------------------------------------------------
//! Runtime contexts leave m_aConditions null. IsAvailable then crashes on array.Count.
class JOLP_SelfRadialHintContext : SCR_AvailableActionContext
{
	//------------------------------------------------------------------------------------------------
	void JOLP_SelfRadialHintContext()
	{
		m_bEnabled = true;
		m_sAction = "JOLP_SelfRadial";
		m_sName = "Self interaction";
		m_aConditions = {};
	}
}

//------------------------------------------------------------------------------------------------
//! Adds the self-interaction shortcut to the existing control-hint list.
modded class SCR_AvailableActionsDisplay
{
	protected ref JOLP_SelfRadialHintContext m_JOLPRadialHint;

	//------------------------------------------------------------------------------------------------
	protected override int GetAvailableActions(notnull SCR_AvailableActionsConditionData data, array<ref SCR_AvailableActionContext> inActions, out array<SCR_AvailableActionContext> outActions, float timeSlice)
	{
		int count = super.GetAvailableActions(data, inActions, outActions, timeSlice);

		if (!m_JOLPRadialHint)
			m_JOLPRadialHint = new JOLP_SelfRadialHintContext();

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager || !inputManager.IsActionActive(m_JOLPRadialHint.GetActionName()))
			return count;

		if (!m_JOLPRadialHint.IsAvailable(data, timeSlice))
			return count;

		outActions.Insert(m_JOLPRadialHint);
		return count + 1;
	}
}
