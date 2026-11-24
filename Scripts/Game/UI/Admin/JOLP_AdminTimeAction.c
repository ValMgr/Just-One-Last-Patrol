//------------------------------------------------------------------------------------------------
//! One forward step of the in-game clock.
class JOLP_AdminAdvanceTimeAction : JOLP_AdminAction
{
	protected int m_iHours;

	//------------------------------------------------------------------------------------------------
	void JOLP_AdminAdvanceTimeAction(int hours)
	{
		m_iHours = hours;
	}

	//------------------------------------------------------------------------------------------------
	override string GetId()
	{
		return "time_" + m_iHours.ToString();
	}

	//------------------------------------------------------------------------------------------------
	override string GetLabel()
	{
		if (m_iHours == 1)
			return "+1 hour";

		return string.Format("+%1 hours", m_iHours);
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

		network.RequestAdvanceHours(m_iHours);
	}
}

//------------------------------------------------------------------------------------------------
//! Opens the clock steps. The steps do not pay supply income for the skipped time.
class JOLP_AdminTimeAction : JOLP_AdminAction
{
	//------------------------------------------------------------------------------------------------
	override string GetId()
	{
		return "time";
	}

	//------------------------------------------------------------------------------------------------
	override string GetLabel()
	{
		return "Change time";
	}

	//------------------------------------------------------------------------------------------------
	override string GetPageHint()
	{
		return "Advance the clock. Supply production is unchanged.";
	}

	//------------------------------------------------------------------------------------------------
	override bool HasChildren()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void GetChildren(notnull array<ref JOLP_AdminAction> children)
	{
		children.Clear();
		children.Insert(new JOLP_AdminAdvanceTimeAction(1));
		children.Insert(new JOLP_AdminAdvanceTimeAction(3));
		children.Insert(new JOLP_AdminAdvanceTimeAction(6));
		children.Insert(new JOLP_AdminAdvanceTimeAction(12));
	}
}
