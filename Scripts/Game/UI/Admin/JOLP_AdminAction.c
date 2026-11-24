//------------------------------------------------------------------------------------------------
//! One tool in the admin dialog.
//! To add a tool, subclass this and insert it from JOLP_AdminCatalog.GetActions().
//! Perform runs on the local player. Ask the server from there when the tool changes the game.
class JOLP_AdminAction
{
	//------------------------------------------------------------------------------------------------
	string GetId()
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	string GetLabel()
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Shown while this tool's children are on screen.
	string GetPageHint()
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	bool HasChildren()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	void GetChildren(notnull array<ref JOLP_AdminAction> children)
	{
		children.Clear();
	}

	//------------------------------------------------------------------------------------------------
	void Perform(JOLP_AdminDialog dialog)
	{
	}
}
