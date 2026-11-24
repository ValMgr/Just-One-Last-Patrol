//------------------------------------------------------------------------------------------------
//! Opens the admin dialog. Hidden unless this player has the same access as Game Master.
class JOLP_AdminRadialAction : JOLP_SelfRadialAction
{
	protected static const ResourceName ICON_SET = "{A4A692135635424E}UI/Imagesets/Commanding/Commanding_Icons.imageset";

	//------------------------------------------------------------------------------------------------
	override string GetId()
	{
		return "admin";
	}

	//------------------------------------------------------------------------------------------------
	override string GetLabel()
	{
		return "Admin";
	}

	//------------------------------------------------------------------------------------------------
	override string GetDescription()
	{
		return "Open admin tools";
	}

	//------------------------------------------------------------------------------------------------
	override void GetIcon(out ResourceName imageset, out string image)
	{
		imageset = ICON_SET;
		image = "defend";
	}

	//------------------------------------------------------------------------------------------------
	override bool CanShow(IEntity user)
	{
		if (!user)
			return false;

		JOLP_AdminNetworkComponent network = JOLP_AdminNetworkComponent.Local();
		if (!network)
			return false;

		return network.HasTools();
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(IEntity user)
	{
		GetGame().GetCallqueue().CallLater(JOLP_AdminDialog.Open, 0, false);
	}
}
