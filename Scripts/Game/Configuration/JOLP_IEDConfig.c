//------------------------------------------------------------------------------------------------
//! One planted RUK charge owned by JOLP_IEDManagerComponent.
class JOLP_IEDRecord : Managed
{
	EntityID m_EntityId;
	int m_iLocationId = -1;
	//! Stable location key. Session ids are reassigned and are not saved.
	string m_sLocationKey;
	vector m_vPosition;
	vector m_vAngles;
	string m_sPrefab;
	bool m_bDefused;
	bool m_bDetonated;
	//! World time when the charge was first seen destroyed. Negative until then.
	float m_fDetonatedAt = -1;
}

//------------------------------------------------------------------------------------------------
//! Tunables for planted RUK IEDs.
[BaseContainerProps(configRoot: true)]
class JOLP_IEDConfig : Managed
{
	[Attribute(defvalue: "3", desc: "Minimum charges rolled the first time a place is initialized")]
	int m_iInitialMin;

	[Attribute(defvalue: "8", desc: "Maximum charges rolled the first time a place is initialized")]
	int m_iInitialMax;

	[Attribute(defvalue: "0.6", desc: "Chance a Very low place gains one charge at 08:00")]
	float m_fChanceVeryLow;

	[Attribute(defvalue: "0.3", desc: "Chance a Low place gains one charge at 08:00")]
	float m_fChanceLow;

	[Attribute(defvalue: "0.1", desc: "Chance a Moderate place gains one charge at 08:00. High and Very high never gain one")]
	float m_fChanceModerate;

	[Attribute(defvalue: "40", desc: "Metres. A civilian death this close to a charge that just detonated is an IED kill")]
	float m_fKillRadius;

	[Attribute(defvalue: "{72BEEF40AF179763}Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Large.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Spawned when a charge detonates. This is the blast that kills. The RUK shell under it often does not", params: "et")]
	ResourceName m_rBlastPrefab;

	[Attribute(defvalue: "30", desc: "Minimum metres between charges of the same place")]
	float m_fMinSpacing;

	[Attribute(defvalue: "8", desc: "In-game hour when the daily garrison and IED check runs")]
	int m_iDailyHour;

	[Attribute(defvalue: "5000", desc: "How often the server checks the in-game clock, in milliseconds")]
	int m_iClockPollMs;

	[Attribute(desc: "RUK IED prefabs. One is picked at random per charge")]
	ref array<ResourceName> m_aPrefabs;

	[Attribute(defvalue: "2", desc: "Minimum trash props scattered around a charge")]
	int m_iTrashMin;

	[Attribute(defvalue: "4", desc: "Maximum trash props scattered around a charge")]
	int m_iTrashMax;

	[Attribute(defvalue: "20", desc: "Metres. A decoy rubbish pile is not placed this close to a charge")]
	float m_fDecoyClearance;

	[Attribute(defvalue: "16", desc: "Metres between decoy rubbish piles in the same place")]
	float m_fDecoySpacing;

	[Attribute(defvalue: "2", desc: "Minimum decoy rubbish piles in a village or hideout. Same props as a charge, with no charge")]
	int m_iDecoyVillageMin;

	[Attribute(defvalue: "4", desc: "Maximum decoy rubbish piles in a village or hideout")]
	int m_iDecoyVillageMax;

	[Attribute(defvalue: "4", desc: "Minimum decoy rubbish piles in a town")]
	int m_iDecoyTownMin;

	[Attribute(defvalue: "7", desc: "Maximum decoy rubbish piles in a town")]
	int m_iDecoyTownMax;

	[Attribute(defvalue: "8", desc: "Minimum decoy rubbish piles in a city")]
	int m_iDecoyCityMin;

	[Attribute(defvalue: "12", desc: "Maximum decoy rubbish piles in a city")]
	int m_iDecoyCityMax;

	[Attribute(desc: "Small props scattered around a charge so it does not sit alone on the road")]
	ref array<ResourceName> m_aTrashPrefabs;

	//------------------------------------------------------------------------------------------------
	int RollInitialCount()
	{
		int minCount = m_iInitialMin;
		int maxCount = m_iInitialMax;
		if (minCount < 0)
			minCount = 0;
		if (maxCount < minCount)
			maxCount = minCount;

		return Math.RandomInt(minCount, maxCount + 1);
	}

	//------------------------------------------------------------------------------------------------
	float ChanceForStage(JOLP_ReputationStage stage)
	{
		if (stage == JOLP_ReputationStage.VERY_LOW)
			return m_fChanceVeryLow;
		if (stage == JOLP_ReputationStage.LOW)
			return m_fChanceLow;
		if (stage == JOLP_ReputationStage.MODERATE)
			return m_fChanceModerate;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName RandomPrefab()
	{
		if (!m_aPrefabs || m_aPrefabs.IsEmpty())
			return ResourceName.Empty;

		return m_aPrefabs.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	int RollTrashCount()
	{
		int minCount = m_iTrashMin;
		int maxCount = m_iTrashMax;
		if (minCount < 0)
			minCount = 0;
		if (maxCount < minCount)
			maxCount = minCount;

		return Math.RandomInt(minCount, maxCount + 1);
	}

	//------------------------------------------------------------------------------------------------
	//! How many empty rubbish piles a place gets, so a heap is not a bomb marker.
	int RollDecoyCount(JOLP_SettlementSize size)
	{
		int minCount = m_iDecoyVillageMin;
		int maxCount = m_iDecoyVillageMax;
		if (size == JOLP_SettlementSize.CITY)
		{
			minCount = m_iDecoyCityMin;
			maxCount = m_iDecoyCityMax;
		}
		else if (size == JOLP_SettlementSize.TOWN)
		{
			minCount = m_iDecoyTownMin;
			maxCount = m_iDecoyTownMax;
		}

		if (minCount < 0)
			minCount = 0;
		if (maxCount < minCount)
			maxCount = minCount;

		return Math.RandomInt(minCount, maxCount + 1);
	}

	//------------------------------------------------------------------------------------------------
	ResourceName RandomTrash()
	{
		if (!m_aTrashPrefabs || m_aTrashPrefabs.IsEmpty())
			return ResourceName.Empty;

		return m_aTrashPrefabs.GetRandomElement();
	}
}
