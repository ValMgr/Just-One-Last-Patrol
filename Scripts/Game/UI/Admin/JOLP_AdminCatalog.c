//------------------------------------------------------------------------------------------------
//! The admin tool list. Add a line here when a new tool should appear in the dialog.
class JOLP_AdminCatalog
{
	//------------------------------------------------------------------------------------------------
	static void GetActions(notnull array<ref JOLP_AdminAction> actions)
	{
		actions.Clear();
		actions.Insert(new JOLP_AdminSaveAction());
		actions.Insert(new JOLP_AdminTimeAction());
	}
}
