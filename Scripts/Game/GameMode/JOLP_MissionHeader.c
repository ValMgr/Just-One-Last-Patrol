//------------------------------------------------------------------------------------------------
//! Scenario header. Hosts civilian / milician mission params read at game start.
class JOLP_MissionHeader : SCR_MissionHeader
{
	[Attribute("100", UIWidgets.Slider, "Percent of the configured civilian population to spawn", "0 200 5", category: "Civilians")]
	int m_iCivilianPopulationPercent;

	[Attribute("5", UIWidgets.Slider, "How many village-sized settlements become milician hideouts", "0 20 1", category: "Milician")]
	int m_iHideoutCount;

	//------------------------------------------------------------------------------------------------
	//! \return 0–200. Missing or foreign headers fall back to 100.
	static int GetCivilianPopulationPercent()
	{
		if (!GetGame())
			return 100;

		JOLP_MissionHeader header = JOLP_MissionHeader.Cast(GetGame().GetMissionHeader());
		if (!header)
			return 100;

		int percent = header.m_iCivilianPopulationPercent;
		if (percent < 0)
			return 0;
		if (percent > 200)
			return 200;

		return percent;
	}

	//------------------------------------------------------------------------------------------------
	//! \return hideout count (≥ 0). Missing headers fall back to 5.
	static int GetHideoutCount()
	{
		if (!GetGame())
			return 5;

		JOLP_MissionHeader header = JOLP_MissionHeader.Cast(GetGame().GetMissionHeader());
		if (!header)
			return 5;

		if (header.m_iHideoutCount < 0)
			return 0;

		return header.m_iHideoutCount;
	}
}
