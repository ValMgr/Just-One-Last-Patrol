//------------------------------------------------------------------------------------------------
//! Tunables for milician proximity spawn / despawn and role prefabs.
[BaseContainerProps(configRoot: true)]
class JOLP_MilicianGarrisonConfig : Managed
{
	[Attribute(defvalue: "2000", desc: "Server proximity tick interval in milliseconds")]
	int m_iTickIntervalMs;

	[Attribute(defvalue: "1.15", desc: "Despawn radius = activation radius × this (hysteresis)")]
	float m_fDespawnMultiplier;

	[Attribute(defvalue: "200", desc: "A group only spawns or despawns when every living player is at least this far from it (metres). Stops a squad appearing or vanishing on a player standing in the spawn area")]
	float m_fPlayerClearance;

	[Attribute(defvalue: "15", desc: "Random horizontal scatter when placing groups around a location")]
	float m_fSpawnScatterRadius;

	[Attribute(defvalue: "120000", desc: "After reinforcement arrives, wait this many ms then absorb into target stored")]
	int m_iReinforceArriveDespawnMs;

	[Attribute(defvalue: "1800000", desc: "Default hideout reinforcement interval (30 minutes)")]
	int m_iHideoutReinforceIntervalMs;

	[Attribute(defvalue: "2500", desc: "Default search range when hideouts pick a town to reinforce")]
	float m_fHideoutReinforceSearchRange;

	[Attribute(defvalue: "2000", desc: "Minimum distance between promoted hideouts. One hideout is placed in each map quarter at this distance; any still needed to reach the mission count may use half of it")]
	float m_fHideoutMinSeparation;

	[Attribute(defvalue: "2", desc: "How many nearest operational hideouts keep a reinforcement group on the road")]
	int m_iHideoutActiveReinforcers;

	[Attribute(defvalue: "4", desc: "Hideouts keep at least this many stored men for defense when sending reinforcements")]
	int m_iHideoutDefenseReserve;

	[Attribute(defvalue: "40", desc: "Stored garrison when a town is promoted to hideout")]
	int m_iHideoutInitialGarrison;

	[Attribute(defvalue: "60", desc: "Hideout stored garrison soft cap (regen stops here)")]
	int m_iHideoutMaxGarrison;

	[Attribute(defvalue: "6", desc: "Men regenerated on each hideout reinforce tick before sending a wave")]
	int m_iHideoutRegenPerTick;

	[Attribute(defvalue: "0.5", desc: "Hideout wave chance to send a vehicle instead of a foot squad")]
	float m_fHideoutVehicleChance;

	[Attribute(defvalue: "0.35", desc: "When a hideout wave is a vehicle, chance it is armed (light or heavy) instead of a transport")]
	float m_fHideoutArmedVehicleChance;

	[Attribute(defvalue: "2", desc: "Transports stocked at a town")]
	int m_iTownTransports;

	[Attribute(defvalue: "1", desc: "Light vehicles stocked at a town")]
	int m_iTownLights;

	[Attribute(defvalue: "1", desc: "Transports stocked at a city")]
	int m_iCityTransports;

	[Attribute(defvalue: "2", desc: "Heavies stocked when a place becomes a hideout. Replaces whatever that place had")]
	int m_iHideoutHeavies;

	[Attribute(defvalue: "2500", desc: "Search range when an attacked settlement calls a neighbor for QRF")]
	float m_fQRFSearchRange;

	[Attribute(defvalue: "30000", desc: "Delay after the first player hit before a neighbor dispatches QRF (ms)")]
	int m_iQRFReactionDelayMs;

	[Attribute(defvalue: "480000", desc: "Cooldown after a QRF squad is sent to a settlement (ms)")]
	int m_iQRFCooldownMs;

	[Attribute(defvalue: "60000", desc: "Cooldown when an attacked settlement finds no QRF donor (ms)")]
	int m_iQRFRetryCooldownMs;

	[Attribute(defvalue: "4", desc: "Donor keeps at least this many stored men when sending QRF")]
	int m_iQRFDefenseReserve;

	[Attribute(defvalue: "{664B13AF00000300}Prefabs/Props/Military/JOLP_HideoutSite.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Hideout camp. The ammo stack is what players must destroy", params: "et")]
	ResourceName m_rHideoutHQPrefab;

	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Static weapons placed around a hideout so the garrison can man them. One of each is placed", params: "et", category: "Hideout")]
	ref array<ResourceName> m_aHideoutNestPrefabs;

	[Attribute(defvalue: "{664B13AF00000316}Prefabs/Props/Military/JOLP_HideoutRoadblock.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Barrier placed on a road near the hideout camp, inside the settlement", params: "et", category: "Hideout")]
	ResourceName m_rHideoutRoadblockPrefab;

	[Attribute(defvalue: "600000", desc: "Max lifetime of a patrol before it is stored back if still active (ms)")]
	int m_iPatrolMaxDurationMs;

	[Attribute(defvalue: "USSR", desc: "Faction key milician wear. USSR is the OPFOR side, so they stay distinct from FIA partisans")]
	string m_sFactionKey;

	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Sentry groups. One prefab is picked at random per spawn", params: "et", category: "Group Prefab")]
	ref array<ResourceName> m_aSentryGroupPrefabs;

	[Attribute(defvalue: "2", desc: "Smaller sentry. A sentry fields this many men or one more. When only that larger count is left in storage, they all leave together so one man is not left alone")]
	int m_iSentryAgents;

	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Patrol groups. One prefab is picked at random per spawn", params: "et", category: "Group Prefab")]
	ref array<ResourceName> m_aPatrolGroupPrefabs;

	[Attribute(defvalue: "5", desc: "Patrol expected agent count (accounting)")]
	int m_iPatrolAgents;

	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Checkpoint infantry groups. One prefab is picked at random per spawn", params: "et", category: "Group Prefab")]
	ref array<ResourceName> m_aCheckpointGroupPrefabs;

	[Attribute(defvalue: "4", desc: "Checkpoint expected agent count (accounting)")]
	int m_iCheckpointAgents;

	[Attribute(defvalue: "{7C85836D444E3797}Prefabs/Compositions/Slotted/SlotRoadMedium/Checkpoint_M_USSR_01.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Checkpoint fortification composition", params: "et")]
	ResourceName m_rCheckpointCompositionPrefab;

	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Reinforcement infantry groups. One prefab is picked at random per spawn", params: "et", category: "Group Prefab")]
	ref array<ResourceName> m_aReinforceGroupPrefabs;

	[Attribute(defvalue: "6", desc: "Reinforcement expected agent count (accounting)")]
	int m_iReinforceAgents;

	[Attribute(defvalue: "0.15", desc: "Chance a town or a city rolls one sniper when its garrison first wakes. Later refills do not roll again")]
	float m_fSniperChance;

	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Sniper groups. One prefab is picked at random per spawn. Members are placed on a roof, not on the ground", params: "et", category: "Group Prefab")]
	ref array<ResourceName> m_aSniperGroupPrefabs;

	[Attribute(defvalue: "1", desc: "Sniper expected agent count (accounting)")]
	int m_iSniperAgents;

	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Unarmed reinforcement vehicles. One prefab is picked at random per spawn", params: "et", category: "Vehicles")]
	ref array<ResourceName> m_aTransportVehiclePrefabs;

	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Armed cars and BRDMs. One prefab is picked at random per spawn", params: "et", category: "Vehicles")]
	ref array<ResourceName> m_aLightVehiclePrefabs;

	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "BTRs. One prefab is picked at random per spawn", params: "et", category: "Vehicles")]
	ref array<ResourceName> m_aHeavyVehiclePrefabs;

	[Attribute(defvalue: "2", desc: "Crew spawned with a transport")]
	int m_iTransportCrew;

	[Attribute(defvalue: "2", desc: "Crew spawned with a light vehicle")]
	int m_iLightCrew;

	[Attribute(defvalue: "3", desc: "Crew spawned with a heavy vehicle")]
	int m_iHeavyCrew;

	[Attribute(defvalue: "{9FFEF10757E742EB}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Crew.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Crewman used in light and heavy vehicles", params: "et")]
	ResourceName m_rCrewmanPrefab;

	[Attribute(defvalue: "{DCB41B3746FDD1BE}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Rifleman.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Driver used in unarmed transports", params: "et")]
	ResourceName m_rDriverPrefab;

	[Attribute(defvalue: "{664B13AF00000190}Prefabs/Groups/JOLP_Group_MilicianCrew.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Empty group the vehicle crew is added to", params: "et")]
	ResourceName m_rCrewGroupPrefab;

	[Attribute(defvalue: "{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Defend waypoint prefab", params: "et")]
	ResourceName m_rDefendWaypointPrefab;

	[Attribute(defvalue: "{22A875E30470BD4F}Prefabs/AI/Waypoints/AIWaypoint_Patrol.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Patrol waypoint prefab", params: "et")]
	ResourceName m_rPatrolWaypointPrefab;

	[Attribute(defvalue: "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Move waypoint prefab", params: "et")]
	ResourceName m_rMoveWaypointPrefab;

	[Attribute(defvalue: "{35BD6541CBB8AC08}Prefabs/AI/Waypoints/AIWaypoint_Cycle.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Cycle waypoint prefab", params: "et")]
	ResourceName m_rCycleWaypointPrefab;

	[Attribute(defvalue: "{B3E7B8DC2BAB8ACC}Prefabs/AI/Waypoints/AIWaypoint_SearchAndDestroy.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Search-and-destroy waypoint. QRF uses it on a player milician have seen", params: "et")]
	ResourceName m_rSearchAndDestroyWaypointPrefab;

	[Attribute(defvalue: "{C40316EE26846CAB}Prefabs/AI/Waypoints/AIWaypoint_GetOut.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Dismount waypoint issued before a QRF search", params: "et")]
	ResourceName m_rGetOutWaypointPrefab;

	[Attribute(defvalue: "{D1FFE458E8AC4BDB}Prefabs/Weapons/Mortars/2B14/Mortar_2B14.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Tube spawned at an OP when a moderate or better place starts a fight", params: "et", category: "Mortar")]
	ResourceName m_rMortarPrefab;

	[Attribute(defvalue: "{2351FA1249150AE6}Prefabs/Props/Military/Arsenal/AmmoBoxes/USSR/AmmoBoxArsenal_Mortar_USSR.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Shell source placed beside the tube. The crew draws HE from it", params: "et", category: "Mortar")]
	ResourceName m_rMortarAmmoBoxPrefab;

	[Attribute(defvalue: "{664B13AF00000220}Prefabs/Characters/Factions/IND/MEI/Character_MEI_MortarCrew.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Crew character. MEI uniform with USSR mortar backpack and binoculars. Empty uses the driver prefab", params: "et", category: "Mortar")]
	ResourceName m_rMortarCrewPrefab;

	[Attribute(defvalue: "2", desc: "Crew on the tube. Not drawn from the OP garrison", category: "Mortar")]
	int m_iMortarCrew;

	[Attribute(defvalue: "150", desc: "First ranging shot lands about this far from the last known player position", category: "Mortar")]
	float m_fMortarRangingFirst;

	[Attribute(defvalue: "100", desc: "Second ranging shot, issued after the first shell has landed", category: "Mortar")]
	float m_fMortarRangingSecond;

	[Attribute(defvalue: "50", desc: "Third ranging shot", category: "Mortar")]
	float m_fMortarRangingThird;

	[Attribute(defvalue: "20", desc: "Random metres added or removed from a ranging offset", category: "Mortar")]
	float m_fMortarAimJitter;

	[Attribute(defvalue: "60000", desc: "Wait after the first ranging shell before the second (ms)", category: "Mortar")]
	int m_iMortarGapAfterFirstMs;

	[Attribute(defvalue: "30000", desc: "Wait after the second ranging shell before the third (ms)", category: "Mortar")]
	int m_iMortarGapAfterSecondMs;

	[Attribute(defvalue: "30000", desc: "Wait after the third ranging shell before the first salvo (ms)", category: "Mortar")]
	int m_iMortarGapAfterThirdMs;

	[Attribute(defvalue: "5", desc: "Shells in one fire-for-effect salvo. The loader's own pace separates them", category: "Mortar")]
	int m_iMortarRoundsPerSalvo;

	[Attribute(defvalue: "5", desc: "Salvos after the three ranging shots. The battery then despawns", category: "Mortar")]
	int m_iMortarSalvoCount;

	[Attribute(defvalue: "180000", desc: "Wait after a salvo finishes before the next one (ms)", category: "Mortar")]
	int m_iMortarSalvoIntervalMs;

	[Attribute(defvalue: "900000", desc: "After the battery despawns, this OP waits this long before another (ms)", category: "Mortar")]
	int m_iMortarCooldownMs;

	[Attribute(defvalue: "150", desc: "2B14 will not be called when the aim point is closer than this", category: "Mortar")]
	float m_fMortarMinRange;

	[Attribute(defvalue: "3000", desc: "2B14 will not be called when the aim point is farther than this", category: "Mortar")]
	float m_fMortarMaxRange;

	[Attribute(defvalue: "480000", desc: "A fire order that never finishes ends the mission (ms)", category: "Mortar")]
	int m_iMortarOrderTimeoutMs;

	//------------------------------------------------------------------------------------------------
	ResourceName RandomPrefab(array<ResourceName> prefabs)
	{
		if (!prefabs || prefabs.IsEmpty())
			return ResourceName.Empty;

		array<ResourceName> choices = {};
		foreach (ResourceName prefab : prefabs)
		{
			if (!prefab.IsEmpty())
				choices.Insert(prefab);
		}

		if (choices.IsEmpty())
			return ResourceName.Empty;

		return choices.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	bool VehicleCarriesSquad(JOLP_VehicleClass kind)
	{
		return kind == JOLP_VehicleClass.TRANSPORT || kind == JOLP_VehicleClass.HEAVY;
	}

	//------------------------------------------------------------------------------------------------
	int CrewCount(JOLP_VehicleClass kind)
	{
		int count = 2;
		if (kind == JOLP_VehicleClass.TRANSPORT)
			count = m_iTransportCrew;
		else if (kind == JOLP_VehicleClass.LIGHT)
			count = m_iLightCrew;
		else if (kind == JOLP_VehicleClass.HEAVY)
			count = m_iHeavyCrew;

		if (count < 1)
			count = 1;

		return count;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName CrewCharacterPrefab(JOLP_VehicleClass kind)
	{
		if (kind == JOLP_VehicleClass.LIGHT || kind == JOLP_VehicleClass.HEAVY)
		{
			if (!m_rCrewmanPrefab.IsEmpty())
				return m_rCrewmanPrefab;
		}

		return m_rDriverPrefab;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName VehiclePrefab(JOLP_VehicleClass kind)
	{
		if (kind == JOLP_VehicleClass.TRANSPORT)
			return RandomPrefab(m_aTransportVehiclePrefabs);
		if (kind == JOLP_VehicleClass.LIGHT)
			return RandomPrefab(m_aLightVehiclePrefabs);
		if (kind == JOLP_VehicleClass.HEAVY)
			return RandomPrefab(m_aHeavyVehiclePrefabs);

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Village and OP stay empty. Hideout replaces any previous stock.
	void ApplyDefaultVehicleStock(notnull JOLP_LocationData data)
	{
		int transport = 0;
		int light = 0;
		int heavy = 0;

		if (data.IsHideout())
		{
			heavy = m_iHideoutHeavies;
		}
		else if (!data.IsOP())
		{
			if (data.m_eSettlementSize == JOLP_SettlementSize.CITY)
			{
				transport = m_iCityTransports;
			}
			else if (data.m_eSettlementSize == JOLP_SettlementSize.TOWN)
			{
				transport = m_iTownTransports;
				light = m_iTownLights;
			}
		}

		data.SetVehicleStock(JOLP_VehicleClass.TRANSPORT, transport);
		data.SetVehicleStock(JOLP_VehicleClass.LIGHT, light);
		data.SetVehicleStock(JOLP_VehicleClass.HEAVY, heavy);
	}
}
