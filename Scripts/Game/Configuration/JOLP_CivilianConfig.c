//------------------------------------------------------------------------------------------------
//! Local reputation band. A place's villager attitude is chosen from its own score.
enum JOLP_ReputationStage
{
	VERY_LOW = 0,
	LOW = 1,
	MODERATE = 2,
	HIGH = 3,
	VERY_HIGH = 4
}

//------------------------------------------------------------------------------------------------
//! What a spawned villager will do. FRIENDLY is reserved and not rolled in v1.
enum JOLP_CivilianAttitude
{
	TERRORIST = 0,
	PARTISAN = 1,
	NEUTRAL = 2,
	FRIENDLY = 3
}

//------------------------------------------------------------------------------------------------
//! Foot villagers are the town's population. Cars are traffic and never move that count.
enum JOLP_CivilianKind
{
	FOOT = 0,
	VEHICLE = 1
}

//------------------------------------------------------------------------------------------------
//! One live footman or one car-and-driver trip.
class JOLP_CivilianLiveRecord : Managed
{
	EntityID m_GroupId;
	EntityID m_VehicleId;
	ref array<EntityID> m_aWaypointIds = new array<EntityID>();
	ref array<EntityID> m_aCharacterIds = new array<EntityID>();
	JOLP_CivilianKind m_eKind;
	JOLP_CivilianAttitude m_eAttitude;
	int m_iSourceLocationId = -1;
	int m_iTargetLocationId = -1;
	bool m_bSeenAgent;
	bool m_bSeated;
	bool m_bHostile;
	bool m_bDetonated;
	bool m_bDead;
	bool m_bSeeking;
	bool m_bChargesAttached;
	//! Told to go indoors. A later patrol refresh must leave them there.
	bool m_bSheltering;
	//! Player whose dialogue is holding them still. 0 when they are free to walk.
	int m_iTalkPlayerId;
	//! World time until a call-over expires. Negative when they were not called.
	float m_fSummonedUntil = -1;
	float m_fSpawnedAtWorldTime = -1;
}

//------------------------------------------------------------------------------------------------
//! A character or civilian car the reputation hooks can recognise.
class JOLP_TrackedAgent : Managed
{
	EntityID m_EntityId;
	int m_iLocationId = -1;
	bool m_bMilician;
	bool m_bVehicle;
	bool m_bSuppressReputation;
	//! Reinforcement / QRF on the road — hits must not call QRF for the donor town.
	bool m_bSuppressQRF;
	bool m_bInjuredApplied;
	bool m_bDestroyedApplied;
	JOLP_CivilianAttitude m_eAttitude;
}

//------------------------------------------------------------------------------------------------
//! Tunables for civilian counts, attitudes, traffic, and reputation.
[BaseContainerProps(configRoot: true)]
class JOLP_CivilianConfig : Managed
{
	[Attribute(defvalue: "2000", desc: "Server civilian tick interval in milliseconds")]
	int m_iTickIntervalMs;

	[Attribute(defvalue: "1.15", desc: "Despawn radius = activation radius × this")]
	float m_fDespawnMultiplier;

	[Attribute(defvalue: "3", desc: "Max foot villagers spawned per town per tick")]
	int m_iMaxFootSpawnsPerTick;

	[Attribute(defvalue: "8", desc: "Terrorist detonates when a player is this close (metres)")]
	float m_fTerroristTriggerRadius;

	[Attribute(defvalue: "11", desc: "Bomb vest appears when a player is this close (metres). Keep it a few metres beyond the detonation radius")]
	float m_fTerroristRevealRadius;

	[Attribute(defvalue: "2500", desc: "Civilian cars only drive to a town inside this range")]
	float m_fVehicleSearchRange;

	[Attribute(defvalue: "20000", desc: "Refund a villager if their body never appears (ms)")]
	int m_iAgentGraceMs;

	[Attribute(defvalue: "5", desc: "Foot villagers in a village before the mission percent")]
	int m_iVillageFoot;

	[Attribute(defvalue: "1", desc: "Concurrent civilian cars in a village before the mission percent")]
	int m_iVillageVehicles;

	[Attribute(defvalue: "10", desc: "Foot villagers in a town before the mission percent")]
	int m_iTownFoot;

	[Attribute(defvalue: "2", desc: "Concurrent civilian cars in a town before the mission percent")]
	int m_iTownVehicles;

	[Attribute(defvalue: "16", desc: "Foot villagers in a city before the mission percent")]
	int m_iCityFoot;

	[Attribute(defvalue: "3", desc: "Concurrent civilian cars in a city before the mission percent")]
	int m_iCityVehicles;

	[Attribute(defvalue: "0.4", desc: "Very low: ceiling share of a town's foot budget for terrorists. Each visit rolls from none up to this")]
	float m_fVeryLowTerroristShare;

	[Attribute(defvalue: "0.85", desc: "Very low: ceiling share of a town's foot budget for partisans. Each visit rolls from none up to this")]
	float m_fVeryLowPartisanShare;

	[Attribute(defvalue: "0.2", desc: "Low: ceiling share of a town's foot budget for terrorists. Each visit rolls from none up to this")]
	float m_fLowTerroristShare;

	[Attribute(defvalue: "0.4", desc: "Low: ceiling share of a town's foot budget for partisans. Each visit rolls from none up to this")]
	float m_fLowPartisanShare;

	[Attribute(defvalue: "0.25", desc: "Moderate: ceiling partisan share. Each visit rolls from none up to this. Terrorists are not spawned at this stage")]
	float m_fModeratePartisanShare;

	[Attribute(defvalue: "0", desc: "Starting reputation of each town, village, city, and hideout. 0 sits in the Low band")]
	float m_fStartingReputation;

	[Attribute(defvalue: "500", desc: "Metres around the player. A kill or injury also changes the closest reputation place inside this radius")]
	float m_fReputationRadius;

	[Attribute(defvalue: "-4", desc: "Reputation below this is Very low")]
	float m_fStageVeryLowBelow;

	[Attribute(defvalue: "4", desc: "Reputation below this (and at or above Very low) is Low")]
	float m_fStageLowBelow;

	[Attribute(defvalue: "12", desc: "Reputation below this (and at or above Low) is Moderate")]
	float m_fStageModerateBelow;

	[Attribute(defvalue: "25", desc: "Reputation below this (and at or above Moderate) is High. At or above is Very high")]
	float m_fStageHighBelow;

	[Attribute(defvalue: "0.1", desc: "Reputation gained for each milician who actually dies")]
	float m_fReputationPerMilicianKill;

	[Attribute(defvalue: "-0.5", desc: "Reputation when a player injures a civilian")]
	float m_fReputationDamaged;

	[Attribute(defvalue: "-1", desc: "Reputation when a player or an IED kills a civilian")]
	float m_fReputationDestroyed;

	[Attribute(defvalue: "0.2", desc: "Reputation removed at 08:00 for each man stored in a place's garrison")]
	float m_fReputationPerGarrisonMan;

	[Attribute(defvalue: "0.25", desc: "Reputation gained when an IED is disabled and does not explode")]
	float m_fReputationIEDDefused;

	[Attribute(defvalue: "-1", desc: "Reputation removed when a player dies inside a village, town, city, or hideout")]
	float m_fReputationPlayerKilled;

	[Attribute(defvalue: "0.25", desc: "After a player kills a civilian, this share of the neutrals in that place become partisans. At least one when any neutral is there")]
	float m_fKillRadicalizeShare;

	[Attribute(defvalue: "CIV", desc: "Faction key civilians wear until a partisan is provoked")]
	string m_sCivilianFactionKey;

	[Attribute(defvalue: "FIA", desc: "Faction key partisans switch to when local milician are in combat")]
	string m_sHostileFactionKey;

	[Attribute(defvalue: "{664B13AF00000070}Prefabs/Groups/JOLP_Group_Civilian.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "One civilian, will not fight", params: "et")]
	ResourceName m_rFootGroupPrefab;

	[Attribute(defvalue: "{664B13AF00000074}Prefabs/Groups/JOLP_Group_CivilianPartisan.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "One civilian who looks unarmed until they join a fight", params: "et")]
	ResourceName m_rPartisanGroupPrefab;

	[Attribute(defvalue: "{72BEEF40AF179763}Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Large.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Spawned when a terrorist detonates", params: "et")]
	ResourceName m_rExplosionPrefab;

	[Attribute(defvalue: "{664B13AF00000090}Prefabs/Items/JOLP_BombVest.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "MiddleEastInsurgents bomb vest, worn once a player is inside the reveal radius", params: "et")]
	ResourceName m_rBomberChargePrefab;

	[Attribute(defvalue: "{664B13AF000000A2}Prefabs/Sounds/JOLP_SuicideScream.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Played at the bomber when the vest appears", params: "et")]
	ResourceName m_rSuicideSoundPrefab;

	[Attribute(defvalue: "{FA5C25BF66A53DCF}Prefabs/Weapons/Rifles/AK74/Rifle_AK74.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Weapon given to partisans", params: "et")]
	ResourceName m_rPartisanWeaponPrefab;

	[Attribute(defvalue: "{0A84AA5A3884176F}Prefabs/Weapons/Magazines/Magazine_545x39_AK_30rnd_Last_5Tracer.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Magazine given to partisans", params: "et")]
	ResourceName m_rPartisanMagazinePrefab;

	[Attribute(defvalue: "2", desc: "Magazines inserted with the partisan weapon")]
	int m_iPartisanMagazines;

	[Attribute(defvalue: "{22A875E30470BD4F}Prefabs/AI/Waypoints/AIWaypoint_Patrol.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Patrol waypoint", params: "et")]
	ResourceName m_rPatrolWaypointPrefab;

	[Attribute(defvalue: "{35BD6541CBB8AC08}Prefabs/AI/Waypoints/AIWaypoint_Cycle.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Cycle waypoint", params: "et")]
	ResourceName m_rCycleWaypointPrefab;

	[Attribute(defvalue: "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Move waypoint used by civilian cars", params: "et")]
	ResourceName m_rMoveWaypointPrefab;

	[Attribute(defvalue: "4", desc: "Metres. Speak is refused when the player is farther than this from the civilian")]
	float m_fTalkDistance;

	[Attribute(defvalue: "45", desc: "Metres. Hide order covers foot civilians inside this radius of the speaker")]
	float m_fShelterRadius;

	[Attribute(defvalue: "80", desc: "Metres. Search this far from a civilian for a building door")]
	float m_fShelterSearchRadius;

	[Attribute(defvalue: "4000", desc: "Metres. An enemy squad counts as nearby for a civilian tip inside this range")]
	float m_fIntelRange;

	[Attribute(desc: "Civilian car prefabs. One is picked at random per trip")]
	ref array<ResourceName> m_aVehiclePrefabs;

	//------------------------------------------------------------------------------------------------
	int FootBudgetForSize(JOLP_SettlementSize size)
	{
		switch (size)
		{
			case JOLP_SettlementSize.VILLAGE:
				return Math.Max(0, m_iVillageFoot);
			case JOLP_SettlementSize.CITY:
				return Math.Max(0, m_iCityFoot);
		}

		return Math.Max(0, m_iTownFoot);
	}

	//------------------------------------------------------------------------------------------------
	int VehicleSlotsForSize(JOLP_SettlementSize size)
	{
		switch (size)
		{
			case JOLP_SettlementSize.VILLAGE:
				return Math.Max(0, m_iVillageVehicles);
			case JOLP_SettlementSize.CITY:
				return Math.Max(0, m_iCityVehicles);
		}

		return Math.Max(0, m_iTownVehicles);
	}

	//------------------------------------------------------------------------------------------------
	static int ScaleCount(int baseCount, int percent)
	{
		if (baseCount <= 0 || percent <= 0)
			return 0;

		float scaled = baseCount * (percent / 100.0);
		int rounded = Math.Round(scaled);
		if (rounded < 0)
			return 0;

		return rounded;
	}

	//------------------------------------------------------------------------------------------------
	void GetAttitudeShares(JOLP_ReputationStage stage, out float terroristShare, out float partisanShare)
	{
		terroristShare = 0;
		partisanShare = 0;

		switch (stage)
		{
			case JOLP_ReputationStage.VERY_LOW:
				terroristShare = m_fVeryLowTerroristShare;
				partisanShare = m_fVeryLowPartisanShare;
				break;
			case JOLP_ReputationStage.LOW:
				terroristShare = m_fLowTerroristShare;
				partisanShare = m_fLowPartisanShare;
				break;
			case JOLP_ReputationStage.MODERATE:
				partisanShare = m_fModeratePartisanShare;
				break;
		}

		if (terroristShare < 0)
			terroristShare = 0;
		if (partisanShare < 0)
			partisanShare = 0;
	}

	//------------------------------------------------------------------------------------------------
	JOLP_ReputationStage StageForReputation(float reputation)
	{
		if (reputation < m_fStageVeryLowBelow)
			return JOLP_ReputationStage.VERY_LOW;
		if (reputation < m_fStageLowBelow)
			return JOLP_ReputationStage.LOW;
		if (reputation < m_fStageModerateBelow)
			return JOLP_ReputationStage.MODERATE;
		if (reputation < m_fStageHighBelow)
			return JOLP_ReputationStage.HIGH;

		return JOLP_ReputationStage.VERY_HIGH;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName RandomVehiclePrefab()
	{
		if (!m_aVehiclePrefabs || m_aVehiclePrefabs.IsEmpty())
			return ResourceName.Empty;

		return m_aVehiclePrefabs.GetRandomElement();
	}
}
