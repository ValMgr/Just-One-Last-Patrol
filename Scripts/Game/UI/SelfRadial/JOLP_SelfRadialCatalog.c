//------------------------------------------------------------------------------------------------
//! The self-interaction list. Add a line here when a new action should appear on the wheel.
class JOLP_SelfRadialCatalog
{
	//------------------------------------------------------------------------------------------------
	static void GetActions(notnull array<ref JOLP_SelfRadialAction> actions)
	{
		actions.Clear();
		actions.Insert(new JOLP_CallCiviliansRadialAction());
	}
}
