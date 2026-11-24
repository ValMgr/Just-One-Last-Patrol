//------------------------------------------------------------------------------------------------
//! Write a manual save point.
class JOLP_AdminSaveAction : JOLP_AdminAction
{
	//------------------------------------------------------------------------------------------------
	override string GetId()
	{
		return "save";
	}

	//------------------------------------------------------------------------------------------------
	override string GetLabel()
	{
		return "Save game";
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(JOLP_AdminDialog dialog)
	{
		JOLP_AdminNetworkComponent network = JOLP_AdminNetworkComponent.Local();
		if (!network)
		{
			if (dialog)
				dialog.SetMessage("Cannot reach the server.");

			return;
		}

		network.RequestSave();
	}
}
