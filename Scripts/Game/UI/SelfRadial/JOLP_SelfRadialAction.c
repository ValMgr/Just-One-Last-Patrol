//------------------------------------------------------------------------------------------------
//! One slice on the self-interaction wheel.
//! To add an action, subclass this and insert it from JOLP_SelfRadialCatalog.GetActions().
//! Perform runs on the local player. Call the server from there when the action has to be shared.
class JOLP_SelfRadialAction
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
	string GetDescription()
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	void GetIcon(out ResourceName imageset, out string image)
	{
		imageset = "";
		image = string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! False hides the slice.
	bool CanShow(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! False keeps the slice visible and blocks the click.
	bool CanPerform(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Perform(IEntity user)
	{
	}
}
