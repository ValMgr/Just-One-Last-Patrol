class JOLP_MilicianManagerComponentClass : JOLP_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Server-side milician spawn/despawn, role packages, hideout reinforcement, and settlement QRF.
//! Lives on a Manager.layer world entity (editor-editable prefabs), not on the game mode.
class JOLP_MilicianManagerComponent : JOLP_Component
{
	static JOLP_MilicianManagerComponent s_Instance;

	[Attribute(desc: "Milician garrison tunables and role prefabs (edit on Manager.layer entity)")]
	ref JOLP_MilicianGarrisonConfig m_Config;

	[Attribute(desc: "Combat aim for milician. Same file as the civilian manager: Configs/AI/AISkill.conf")]
	ref JOLP_AISkillConfig m_SkillConfig;

	protected bool m_bTickRunning;
	protected ref array<ref JOLP_LiveGroupRecord> m_aLiveGroups;
	protected ref JOLP_MortarSupport m_MortarSupport;
	//! The garrison that appears when a place wakes up. Later spawns during a fight use the far rim.
	protected bool m_bOpeningSpawn;

	//------------------------------------------------------------------------------------------------
	static JOLP_MilicianManagerComponent GetInstance()
	{
		if (s_Instance)
			return s_Instance;

		BaseGameMode gameMode = GetGame().GetGameMode();
		if (gameMode)
		{
			s_Instance = JOLP_MilicianManagerComponent.Cast(gameMode.FindComponent(JOLP_MilicianManagerComponent));
			if (s_Instance)
				return s_Instance;
		}

		// World-authored manager entity (Manager.layer).
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, FindMilicianManagerEntity, null, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool FindMilicianManagerEntity(IEntity entity)
	{
		if (!entity || s_Instance)
			return true;

		JOLP_MilicianManagerComponent component = JOLP_MilicianManagerComponent.Cast(
			entity.FindComponent(JOLP_MilicianManagerComponent));
		if (component)
			s_Instance = component;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		if (JOLP_Component.IsPreviewInstance(owner))
			return;

		s_Instance = this;
		if (!m_Config)
			m_Config = new JOLP_MilicianGarrisonConfig();
		if (!m_aLiveGroups)
			m_aLiveGroups = new array<ref JOLP_LiveGroupRecord>();
	}

	//------------------------------------------------------------------------------------------------
	void Init(IEntity owner)
	{
		s_Instance = this;
		if (!m_Config)
			m_Config = new JOLP_MilicianGarrisonConfig();
		if (!m_aLiveGroups)
			m_aLiveGroups = new array<ref JOLP_LiveGroupRecord>();

		Print("[JOLP] MilicianManager Init", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	void PostGameStart()
	{
		if (!Replication.IsServer())
			return;

		if (m_bTickRunning)
			return;

		int interval = 2000;
		if (m_Config && m_Config.m_iTickIntervalMs > 0)
			interval = m_Config.m_iTickIntervalMs;

		GetGame().GetCallqueue().CallLater(ProximityTick, interval, true);
		m_bTickRunning = true;

		Print(string.Format("[JOLP] MilicianManager PostGameStart: tick every %1 ms", interval), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Living agents that were paid for out of this place's garrison. Free waves and QRF are not included.
	int GetFieldedGarrison(int locationId)
	{
		if (!m_aLiveGroups)
			return 0;

		int total = 0;
		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!record || record.m_iSourceLocationId != locationId)
				continue;

			if (record.m_bQRF || record.m_bFreeReinforcement || record.m_iChargedAgents <= 0)
				continue;

			SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
			if (!group)
				continue;

			total += JOLP_WorldUtils.CountLivingAgents(group);
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Hulls currently in the world that will return to this place's stock if they survive.
	int GetFieldedVehicles(int locationId, JOLP_VehicleClass kind)
	{
		if (!m_aLiveGroups || kind == JOLP_VehicleClass.NONE)
			return 0;

		int total = 0;
		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!record || record.m_iSourceLocationId != locationId || record.m_eVehicleClass != kind)
				continue;

			if (!record.m_VehicleId)
				continue;

			IEntity vehicle = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
			if (!vehicle || JOLP_VehicleCrew.IsHullDestroyed(vehicle))
				continue;

			total++;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	JOLP_MilicianGarrisonConfig GetConfig()
	{
		if (!m_Config)
			m_Config = new JOLP_MilicianGarrisonConfig();

		return m_Config;
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_AISkillConfig GetSkillConfig()
	{
		if (!m_SkillConfig)
			m_SkillConfig = new JOLP_AISkillConfig();

		return m_SkillConfig;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickMortarSupport()
	{
		if (!m_MortarSupport)
			m_MortarSupport = new JOLP_MortarSupport();

		m_MortarSupport.Tick(this, GetConfig(), GetSkillConfig().m_eMilicianSkill);
	}

	//------------------------------------------------------------------------------------------------
	protected void ProximityTick()
	{
		if (!Replication.IsServer())
			return;

		UpdateLivePackages();
		TickMortarSupport();
		TickHideoutReinforcements();
		TickQRFRequests();

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		float despawnMultiplier = 1.15;
		if (m_Config && m_Config.m_fDespawnMultiplier > 1.0)
			despawnMultiplier = m_Config.m_fDespawnMultiplier;

		foreach (JOLP_LocationData data : locations.GetAllLocations())
		{
			if (!data)
				continue;

			// Player FOB covers this settlement — despawn local packs and never re-activate.
			if (data.IsSettlement() && data.IsSuppressedByFOB())
			{
				if (data.m_bActive)
					DeactivateLocation(data);
				continue;
			}

			if (data.IsHideout() && data.m_bHideoutDestroyed)
			{
				if (data.m_bActive)
					DeactivateLocation(data);

				DespawnHideoutLeftovers(data);
				continue;
			}

			if (!data.m_bActive)
			{
				if (data.m_iGarrisonStored <= 0 && !data.HasVehicleStock())
					continue;

				if (JOLP_WorldUtils.PlayerInRange(data.m_vPosition, data.m_fActivationRadius))
					ActivateLocation(data);
			}
			else
			{
				float despawnRadius = data.m_fActivationRadius * despawnMultiplier;
				if (JOLP_WorldUtils.AllPlayersBeyondRange(data.m_vPosition, despawnRadius))
					DeactivateLocation(data);
				else
					RefillLocationIfNeeded(data);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ActivateLocation(notnull JOLP_LocationData data)
	{
		data.m_bActive = true;
		m_bOpeningSpawn = true;
		SpawnPlannedPackages(data);
		m_bOpeningSpawn = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void RefillLocationIfNeeded(notnull JOLP_LocationData data)
	{
		int live = CountLocalLiveAgents(data.m_iId);
		int target = GetDesiredLiveCount(data, live);
		bool needInfantry = live < target && data.m_iGarrisonStored > 0;
		bool needVehicles = data.HasVehicleStock() && (live < target || (data.m_iGarrisonStored <= 0 && live <= 0));
		if (needInfantry || needVehicles)
			SpawnPlannedPackages(data);
	}

	//------------------------------------------------------------------------------------------------
	protected int GetDesiredLiveCount(notnull JOLP_LocationData data, int liveLocal)
	{
		if (data.m_iMaxActiveOverride >= 0)
			return data.m_iMaxActiveOverride;

		float ratio = data.m_fActiveGarrisonRatio;
		if (ratio <= 0)
			ratio = 0.65;

		int pool = data.m_iGarrisonStored + liveLocal;
		return Math.Round(pool * ratio + 0.499); // ceil-ish without float→int loss
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnPlannedPackages(notnull JOLP_LocationData data)
	{
		int live = CountLocalLiveAgents(data.m_iId);
		int target = GetDesiredLiveCount(data, live);
		int need = target - live;
		if (need > 0 && data.m_iGarrisonStored > 0)
		{
			if (data.IsHideout())
				SpawnHideoutDefense(data, need);
			else
				SpawnSettlementPackages(data, need);
		}

		live = CountLocalLiveAgents(data.m_iId);
		int remaining = target - live;
		bool noInfantryLeft = data.m_iGarrisonStored <= 0 && live <= 0;
		if ((remaining > 0 || noInfantryLeft) && data.HasVehicleStock())
			SpawnStoredDefenseVehicles(data);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnHideoutDefense(notnull JOLP_LocationData data, int need)
	{
		if (!data.m_bAllowSentry)
			return;

		while (need > 0 && data.m_iGarrisonStored > 0)
		{
			int cost = GetRoleCost(JOLP_MilicianGroupRole.SENTRY);
			if (cost > data.m_iGarrisonStored || cost > need)
				break;

			int charged = SpawnRolePackage(data, JOLP_MilicianGroupRole.SENTRY, -1);
			if (charged <= 0)
				break;

			need -= charged;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnSettlementPackages(notnull JOLP_LocationData data, int need)
	{
		int storedSnapshot = data.m_iGarrisonStored + CountLocalLiveAgents(data.m_iId);

		// Once, when the place wakes. A refill tick must not keep rolling until a sniper appears.
		if (m_bOpeningSpawn)
			need = need - TryOpeningSniper(data, need);

		int guard = 0;
		while (need > 0 && data.m_iGarrisonStored > 0 && guard < 8)
		{
			guard++;
			int charged = SpawnRandomPackage(data, storedSnapshot, need);
			if (charged <= 0)
				break;

			need -= charged;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One sharpshooter in a town or a city, and only on the wake-up that rolled him. 0 when the roll misses, the budget is short, or no roof will hold him.
	protected int TryOpeningSniper(notnull JOLP_LocationData data, int need)
	{
		if (data.IsHideout())
			return 0;

		if (data.m_eSettlementSize != JOLP_SettlementSize.CITY && data.m_eSettlementSize != JOLP_SettlementSize.TOWN)
			return 0;

		JOLP_MilicianGarrisonConfig config = GetConfig();
		if (!config || config.m_fSniperChance <= 0)
			return 0;

		if (Math.RandomFloat01() >= config.m_fSniperChance)
			return 0;

		if (HasLiveSniper(data.m_iId))
			return 0;

		if (!RoleFits(JOLP_MilicianGroupRole.SNIPER, data, need))
			return 0;

		return TryAddRole(data, JOLP_MilicianGroupRole.SNIPER, need, true, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasLiveSniper(int locationId)
	{
		if (!m_aLiveGroups)
			return false;

		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!record || record.m_iSourceLocationId != locationId)
				continue;

			if (record.m_eRole == JOLP_MilicianGroupRole.SNIPER)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! One affordable role, chosen at random, so towns are not always a stack of sentries.
	protected int SpawnRandomPackage(notnull JOLP_LocationData data, int storedSnapshot, int need)
	{
		array<JOLP_MilicianGroupRole> roles = new array<JOLP_MilicianGroupRole>();
		if (data.m_bAllowSentry && RoleFits(JOLP_MilicianGroupRole.SENTRY, data, need))
			roles.Insert(JOLP_MilicianGroupRole.SENTRY);
		if (data.m_bAllowPatrol && storedSnapshot >= 10 && RoleFits(JOLP_MilicianGroupRole.PATROL, data, need))
			roles.Insert(JOLP_MilicianGroupRole.PATROL);
		if (data.m_bAllowCheckpoint && storedSnapshot >= 18 && RoleFits(JOLP_MilicianGroupRole.CHECKPOINT, data, need))
			roles.Insert(JOLP_MilicianGroupRole.CHECKPOINT);
		if (data.m_bAllowReinforcement && storedSnapshot >= 15 && !HasLiveReinforcement(data.m_iId) && RoleFits(JOLP_MilicianGroupRole.REINFORCEMENT, data, need))
			roles.Insert(JOLP_MilicianGroupRole.REINFORCEMENT);

		while (!roles.IsEmpty())
		{
			int index = Math.RandomInt(0, roles.Count());
			JOLP_MilicianGroupRole role = roles[index];
			roles.Remove(index);

			if (role == JOLP_MilicianGroupRole.REINFORCEMENT)
			{
				JOLP_LocationData target = JOLP_Global.GetLocations().FindWeakestSettlement(data.m_vPosition, 2500, data.m_iId);
				if (!target)
					continue;

				int charged = TryAddRole(data, role, need, true, target.m_iId);
				if (charged > 0)
					return charged;

				continue;
			}

			int chargedLocal = TryAddRole(data, role, need, true, 0);
			if (chargedLocal > 0)
				return chargedLocal;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected bool RoleFits(JOLP_MilicianGroupRole role, notnull JOLP_LocationData data, int need)
	{
		int cost = GetRoleCost(role);
		return cost > 0 && cost <= data.m_iGarrisonStored && cost <= need;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasLiveReinforcement(int locationId)
	{
		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!record)
				continue;

			if (record.m_iSourceLocationId == locationId && record.m_eRole == JOLP_MilicianGroupRole.REINFORCEMENT)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Agents charged (0 if nothing spawned).
	protected int TryAddRole(notnull JOLP_LocationData data, JOLP_MilicianGroupRole role, int need, bool allowed, int reinforceTargetId)
	{
		if (!allowed || need <= 0)
			return 0;

		int cost = GetRoleCost(role);
		if (cost <= 0 || cost > data.m_iGarrisonStored || cost > need)
			return 0;

		return SpawnRolePackage(data, role, reinforceTargetId);
	}

	//------------------------------------------------------------------------------------------------
	protected int GetRoleCost(JOLP_MilicianGroupRole role)
	{
		JOLP_MilicianGarrisonConfig config = GetConfig();
		switch (role)
		{
			case JOLP_MilicianGroupRole.SENTRY:
				return Math.Max(1, config.m_iSentryAgents);
			case JOLP_MilicianGroupRole.PATROL:
				return Math.Max(1, config.m_iPatrolAgents);
			case JOLP_MilicianGroupRole.CHECKPOINT:
				return Math.Max(1, config.m_iCheckpointAgents);
			case JOLP_MilicianGroupRole.REINFORCEMENT:
				return Math.Max(1, config.m_iReinforceAgents);
			case JOLP_MilicianGroupRole.SNIPER:
				return Math.Max(1, config.m_iSniperAgents);
		}

		return 4;
	}

	//------------------------------------------------------------------------------------------------
	//! Two or three men. The whole remainder goes out together when splitting it would leave one man in storage.
	protected int SentryHeadcount(notnull JOLP_LocationData data)
	{
		int small = GetRoleCost(JOLP_MilicianGroupRole.SENTRY);
		if (small < 2)
			small = 2;

		int large = small + 1;
		int stored = data.m_iGarrisonStored;
		if (stored < small)
			return 0;

		if (stored <= large)
			return stored;

		int size = small;
		if (Math.RandomInt(0, 2) == 1)
			size = large;

		if (stored - size == 1)
			size = small;

		return size;
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName GetRoleGroupPrefab(JOLP_MilicianGroupRole role)
	{
		JOLP_MilicianGarrisonConfig config = GetConfig();
		switch (role)
		{
			case JOLP_MilicianGroupRole.SENTRY:
				return config.RandomPrefab(config.m_aSentryGroupPrefabs);
			case JOLP_MilicianGroupRole.PATROL:
				return config.RandomPrefab(config.m_aPatrolGroupPrefabs);
			case JOLP_MilicianGroupRole.CHECKPOINT:
				return config.RandomPrefab(config.m_aCheckpointGroupPrefabs);
			case JOLP_MilicianGroupRole.REINFORCEMENT:
				return config.RandomPrefab(config.m_aReinforceGroupPrefabs);
			case JOLP_MilicianGroupRole.SNIPER:
				return config.RandomPrefab(config.m_aSniperGroupPrefabs);
		}

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Group prefabs are FIA. Force OPFOR so partisans (FIA) and milician stay distinct.
	protected void ApplyMilicianFaction(SCR_AIGroup group, IEntity character = null)
	{
		JOLP_MilicianGarrisonConfig config = GetConfig();
		if (!config || config.m_sFactionKey.IsEmpty())
			return;

		if (group)
			JOLP_CivilianOutfitter.SetGroupFaction(group, config.m_sFactionKey);

		if (character)
			JOLP_CivilianOutfitter.SetCharacterFaction(character, config.m_sFactionKey);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Agents accounted for the package, or 0 on failure. chargeGarrison false leaves the source pool untouched.
	//! forcedClass is the hideout roll. Other reinforcements pick transport, then light, then heavy.
	protected int SpawnRolePackage(notnull JOLP_LocationData data, JOLP_MilicianGroupRole role, int reinforceTargetId, bool qrf = false, bool chargeGarrison = true, JOLP_VehicleClass forcedClass = JOLP_VehicleClass.NONE, bool useForcedClass = false)
	{
		int cost = GetRoleCost(role);
		if (role == JOLP_MilicianGroupRole.SENTRY)
		{
			cost = SentryHeadcount(data);
			if (cost <= 0)
				return 0;
		}

		JOLP_VehicleClass vehicleClass = JOLP_VehicleClass.NONE;
		if (role == JOLP_MilicianGroupRole.REINFORCEMENT)
		{
			if (useForcedClass)
				vehicleClass = forcedClass;
			else
				vehicleClass = PickReinforcementClass(data, chargeGarrison);

			// A light vehicle has no passenger squad, so it does not spend infantry.
			if (vehicleClass != JOLP_VehicleClass.NONE && !GetConfig().VehicleCarriesSquad(vehicleClass))
			{
				int sent = SpawnCrewVehicle(data, vehicleClass, reinforceTargetId, qrf, false);
				if (sent > 0)
					return sent;

				vehicleClass = JOLP_VehicleClass.NONE;
			}
		}

		if (chargeGarrison && cost > data.m_iGarrisonStored)
			return 0;

		ResourceName groupPrefab = GetRoleGroupPrefab(role);
		if (groupPrefab.IsEmpty())
		{
			Print(string.Format("[JOLP] Missing group prefab for role %1", typename.EnumToString(JOLP_MilicianGroupRole, role)), LogLevel.WARNING);
			return 0;
		}

		// The mission list may hold the sniper character instead of a group. A character is not an SCR_AIGroup, so the cast below fails and nothing is fielded.
		ResourceName memberPrefab = "";
		if (role == JOLP_MilicianGroupRole.SNIPER && !IsAIGroupPrefab(groupPrefab))
		{
			memberPrefab = groupPrefab;
			groupPrefab = "{0C9C86FCE951AF19}Prefabs/Groups/INDFOR/AmbientPatrols/Group_FIA_LoneWolfSniper_NotSpawned.et";
		}

		JOLP_MilicianGarrisonConfig config = GetConfig();
		float area = data.m_fActivationRadius;
		if (area < 40)
			area = 80;

		vector spawnPos = data.m_vPosition;
		vector spawnAngles = "0 0 0";
		EntityID compositionId;
		array<vector> occupied = CollectGroupOrigins(data.m_iId);
		float minSeparation = Math.Clamp(area * 0.18, 35, 100);
		int nestIndex = -1;

		bool roadSpawn = false;
		vector vehiclePos;
		vector vehicleAngles;
		array<vector> nearbyPlayers = {};
		bool hideFromPlayers = HideSpawnFromPlayers(data, nearbyPlayers);
		bool placed = false;
		if (role == JOLP_MilicianGroupRole.SNIPER)
		{
			array<vector> players = {};
			float clearance = PlayerClearance();
			JOLP_WorldUtils.CollectLivingPlayerPositions(data.m_vPosition, area + clearance, players);
			if (!JOLP_WorldUtils.FindElevatedStand(data.m_vPosition, area, occupied, 4, players, clearance, spawnPos))
			{
				Print(string.Format("[JOLP] No roof or tower for a sniper at '%1'", data.m_sName), LogLevel.NORMAL);
				return 0;
			}

			placed = true;
		}

		if (!placed && role == JOLP_MilicianGroupRole.SENTRY && data.IsHideout())
		{
			nestIndex = data.ClaimHideoutNest();
			if (nestIndex >= 0)
			{
				JOLP_HideoutPiece nest = data.GetHideoutPiece(nestIndex);
				spawnPos = NestStandPosition(nest);
				spawnAngles = nest.m_vAngles;
				placed = true;
			}
		}

		if (!placed && role == JOLP_MilicianGroupRole.CHECKPOINT)
		{
			if (hideFromPlayers)
				roadSpawn = JOLP_WorldUtils.FindRoadSpawnFarthestFrom(data.m_vPosition, area, occupied, minSeparation, nearbyPlayers, false, spawnPos, spawnAngles);
			else
				roadSpawn = JOLP_WorldUtils.FindSpreadRoadSpawn(data.m_vPosition, area, occupied, minSeparation, spawnPos, spawnAngles);

			if (!roadSpawn)
				spawnPos = PickLocalInfantrySpawn(data, area, occupied, minSeparation);
		}
		else if (!placed && role == JOLP_MilicianGroupRole.REINFORCEMENT)
		{
			// A vehicle only leaves from a stretch the body actually fits on. Otherwise the squad walks.
			float reach = area;
			if (reach < 80)
				reach = 80;

			roadSpawn = FindVehicleDeparture(data, reinforceTargetId, qrf, vehiclePos, vehicleAngles);
			if (roadSpawn)
			{
				spawnPos = JOLP_WorldUtils.CrewStandPosition(vehiclePos, vehicleAngles, 18);
				spawnAngles = vehicleAngles;
			}
			else
			{
				spawnPos = PickLocalInfantrySpawn(data, area, occupied, minSeparation);
				vector unusedPos;
				vector unusedAngles;
				if (!JOLP_WorldUtils.FindNearestRoadSpawn(data.m_vPosition, reach, unusedPos, unusedAngles))
					Print(string.Format("[JOLP] '%1' has no road — reinforcement goes on foot", data.m_sName), LogLevel.NORMAL);
				else
					Print(string.Format("[JOLP] '%1' has no clear stretch of road — reinforcement goes on foot", data.m_sName), LogLevel.NORMAL);
			}
		}
		else if (!placed)
		{
			spawnPos = PickLocalInfantrySpawn(data, area, occupied, minSeparation);
		}

		vector appearAt = spawnPos;
		if (roadSpawn)
			appearAt = vehiclePos;

		bool clear = PositionClearOfPlayers(appearAt);
		if (clear && roadSpawn)
			clear = PositionClearOfPlayers(spawnPos);

		if (!clear)
		{
			if (role == JOLP_MilicianGroupRole.SNIPER)
				return 0;

			if (nestIndex >= 0)
			{
				data.ReleaseHideoutNest(nestIndex);
				nestIndex = -1;
			}

			roadSpawn = false;
			vector away;
			if (!PickSpawnAwayFromPlayers(data.m_vPosition, area, occupied, minSeparation, away))
				return 0;

			spawnPos = away;
		}

		if (role == JOLP_MilicianGroupRole.CHECKPOINT && !config.m_rCheckpointCompositionPrefab.IsEmpty())
		{
			IEntity composition = JOLP_WorldUtils.SpawnEntityPrefab(config.m_rCheckpointCompositionPrefab, spawnPos, spawnAngles, true);
			if (composition)
				compositionId = composition.GetID();
		}

		bool manualMembers = role == JOLP_MilicianGroupRole.SENTRY || role == JOLP_MilicianGroupRole.SNIPER;
		if (manualMembers)
			SCR_AIGroup.IgnoreSpawning(true);

		IEntity groupEnt = JOLP_WorldUtils.SpawnEntityPrefab(groupPrefab, spawnPos, spawnAngles, true);
		if (manualMembers)
			SCR_AIGroup.IgnoreSpawning(false);

		SCR_AIGroup group = SCR_AIGroup.Cast(groupEnt);
		if (!group)
		{
			JOLP_WorldUtils.DeleteEntitySafe(groupEnt);
			if (compositionId)
				JOLP_WorldUtils.DeleteEntitySafe(GetGame().GetWorld().FindEntityByID(compositionId));
			if (nestIndex >= 0)
				data.ReleaseHideoutNest(nestIndex);
			Print(string.Format("[JOLP] Failed to spawn %1 at '%2'", typename.EnumToString(JOLP_MilicianGroupRole, role), data.m_sName), LogLevel.WARNING);
			return 0;
		}

		ApplyMilicianFaction(group);
		bool snapMembers = role != JOLP_MilicianGroupRole.SNIPER;
		if (manualMembers && FillGroupMembers(group, groupPrefab, spawnPos, spawnAngles, cost, snapMembers, memberPrefab) <= 0)
		{
			JOLP_WorldUtils.DeleteEntitySafe(group);
			if (compositionId)
				JOLP_WorldUtils.DeleteEntitySafe(GetGame().GetWorld().FindEntityByID(compositionId));
			if (nestIndex >= 0)
				data.ReleaseHideoutNest(nestIndex);
			Print(string.Format("[JOLP] Failed to spawn %1 at '%2'", typename.EnumToString(JOLP_MilicianGroupRole, role), data.m_sName), LogLevel.WARNING);
			return 0;
		}

		JOLP_LiveGroupRecord record = new JOLP_LiveGroupRecord();
		record.m_eRole = role;
		record.m_GroupId = group.GetID();
		record.m_CompositionId = compositionId;
		record.m_iSourceLocationId = data.m_iId;
		record.m_iTargetLocationId = reinforceTargetId;
		record.m_bQRF = qrf && role == JOLP_MilicianGroupRole.REINFORCEMENT;
		record.m_bFreeReinforcement = !chargeGarrison && role == JOLP_MilicianGroupRole.REINFORCEMENT;
		record.m_bExemptFromProximityDespawn = (role == JOLP_MilicianGroupRole.REINFORCEMENT);
		record.m_iNestPieceIndex = nestIndex;
		record.m_fSpawnedAtWorldTime = GetGame().GetWorld().GetWorldTime();
		record.m_iChargedAgents = cost;

		int agents = JOLP_WorldUtils.CountLivingAgents(group);
		if (agents > 0)
			record.m_iChargedAgents = Math.Min(cost, agents);

		if (chargeGarrison)
			data.m_iGarrisonStored = data.m_iGarrisonStored - record.m_iChargedAgents;

		if (role != JOLP_MilicianGroupRole.REINFORCEMENT)
			AssignBehaviour(data, group, record);

		if (role == JOLP_MilicianGroupRole.REINFORCEMENT && config.VehicleCarriesSquad(vehicleClass) && roadSpawn)
		{
			if (data.TakeVehicleStock(vehicleClass))
			{
				if (!AttachCrewedVehicle(record, vehicleClass, vehiclePos, vehicleAngles, group))
					data.AddVehicleStock(vehicleClass, 1);
			}
		}

		if (role == JOLP_MilicianGroupRole.REINFORCEMENT)
			AssignBehaviour(data, group, record);

		m_aLiveGroups.Insert(record);
		group.GetOnAgentAdded().Insert(OnMilicianAgentAdded);
		RegisterExistingMilicianAgents(group);
		if (record.m_CrewGroupId && record.m_CrewGroupId != record.m_GroupId)
		{
			SCR_AIGroup crew = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_CrewGroupId));
			if (crew)
			{
				crew.GetOnAgentAdded().Insert(OnMilicianAgentAdded);
				RegisterExistingMilicianAgents(crew);
			}
		}

		Print(string.Format("[JOLP] Spawned %1 at '%2' (charged %3, stored %4)",
			typename.EnumToString(JOLP_MilicianGroupRole, role),
			data.m_sName,
			record.m_iChargedAgents,
			data.m_iGarrisonStored), LogLevel.NORMAL);

		return record.m_iChargedAgents;
	}

	//------------------------------------------------------------------------------------------------
	protected void AssignBehaviour(notnull JOLP_LocationData data, notnull SCR_AIGroup group, notnull JOLP_LiveGroupRecord record)
	{
		JOLP_MilicianGarrisonConfig config = GetConfig();
		float area = data.m_fActivationRadius;
		if (area < 40)
			area = 80;

		switch (record.m_eRole)
		{
			case JOLP_MilicianGroupRole.SENTRY:
			{
				JOLP_HideoutPiece nest = data.GetHideoutPiece(record.m_iNestPieceIndex);
				if (nest)
					JOLP_MilicianWaypointHelper.AssignDefend(group, record, config.m_rDefendWaypointPrefab, nest.m_vPosition);
				else
					AssignRandomLocalMission(data, group, record, area, 0.45);
				break;
			}
			case JOLP_MilicianGroupRole.SNIPER:
			{
				// No waypoint. A defend order paths on the ground navmesh and walks him off the tower.
				HoldSniper(group);
				break;
			}
			case JOLP_MilicianGroupRole.PATROL:
			{
				array<vector> occupied = CollectGroupOrigins(data.m_iId);
				vector center = PickSpreadPoint(data.m_vPosition, area * 0.75, occupied, Math.Clamp(area * 0.2, 40, 120));
				float radius = Math.RandomFloat(Math.Max(40, area * 0.2), Math.Max(70, area * 0.7));
				int points = Math.RandomInt(3, 6);
				JOLP_MilicianWaypointHelper.AssignCyclePatrol(group, record, config.m_rPatrolWaypointPrefab, config.m_rCycleWaypointPrefab, center, radius, points, Math.RandomFloat(0, Math.PI2), 0.45);
				break;
			}
			case JOLP_MilicianGroupRole.CHECKPOINT:
			{
				JOLP_MilicianWaypointHelper.AssignDefend(group, record, config.m_rDefendWaypointPrefab, group.GetOrigin());
				break;
			}
			case JOLP_MilicianGroupRole.REINFORCEMENT:
			{
				JOLP_LocationData target = JOLP_Global.GetLocations().GetLocation(record.m_iTargetLocationId);
				vector dest = data.m_vPosition;
				if (target)
					dest = ReinforcementMoveDestination(target, record.m_bQRF);

				// The crew drives. A move order on the passengers makes them dismount and walk.
				SCR_AIGroup driver = group;
				if (record.m_CrewGroupId)
				{
					SCR_AIGroup crew = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_CrewGroupId));
					if (crew)
						driver = crew;
				}

				JOLP_MilicianWaypointHelper.AssignMoveTo(driver, record, config.m_rMoveWaypointPrefab, dest);

				if (record.m_bQRF && target)
				{
					vector contact;
					float now = GetGame().GetWorld().GetWorldTime();
					if (FreshPlayerContact(target, now, contact))
					{
						record.m_vQRFHuntPosition = contact;
						record.m_fQRFHuntOrderedAt = now;
					}
				}
				break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A road the vehicle can actually stop on inside the target. An off-road point in town is how a convoy ends up on a roof.
	protected vector ReinforcementDestination(notnull JOLP_LocationData target)
	{
		float reach = target.m_fActivationRadius;
		if (reach < 120)
			reach = 120;

		vector roadPos;
		vector roadAngles;
		if (JOLP_WorldUtils.FindVehicleRoadSpawn(target.m_vPosition, reach, null, 0, roadPos, roadAngles))
			return roadPos;

		if (JOLP_WorldUtils.FindNearestRoadSpawn(target.m_vPosition, reach, roadPos, roadAngles))
			return roadPos;

		return JOLP_WorldUtils.OffsetOnGround(target.m_vPosition, Math.Max(20, target.m_fActivationRadius * 0.25));
	}

	//------------------------------------------------------------------------------------------------
	//! QRF leaves toward the sighting when one is already known. Otherwise the town road.
	protected vector ReinforcementMoveDestination(notnull JOLP_LocationData target, bool qrf)
	{
		if (qrf)
		{
			vector contact;
			if (FreshPlayerContact(target, GetGame().GetWorld().GetWorldTime(), contact))
				return RoadNearContact(contact);
		}

		return ReinforcementDestination(target);
	}

	//------------------------------------------------------------------------------------------------
	protected vector RoadNearContact(vector contact)
	{
		vector roadPos;
		vector roadAngles;
		if (JOLP_WorldUtils.FindVehicleRoadSpawn(contact, 160, null, 0, roadPos, roadAngles))
			return SnapSurface(roadPos);

		if (JOLP_WorldUtils.FindNearestRoadSpawn(contact, 160, roadPos, roadAngles))
			return SnapSurface(roadPos);

		return SnapSurface(contact);
	}

	//------------------------------------------------------------------------------------------------
	protected vector SnapSurface(vector position)
	{
		BaseWorld world = GetGame().GetWorld();
		if (world)
			position[1] = world.GetSurfaceY(position[0], position[2]);

		return position;
	}

	//------------------------------------------------------------------------------------------------
	//! Behind the gun, toward the camp, so the pair does not appear inside the sandbags.
	protected vector NestStandPosition(notnull JOLP_HideoutPiece nest)
	{
		float yawRad = nest.m_vAngles[0] * Math.DEG2RAD;
		vector forward = Vector(Math.Sin(yawRad), 0, Math.Cos(yawRad));
		vector pos = nest.m_vPosition - (forward * 4);
		return SnapSurface(pos);
	}

	//------------------------------------------------------------------------------------------------
	//! One soldier of the pair takes the static weapon. The other keeps the defend order.
	//! The tripod is not always ready on the frame the soldier appears, so a miss is tried again.
	protected void TrySeatNestGunner(notnull JOLP_LiveGroupRecord record, IEntity body)
	{
		if (record.m_bNestGunnerSeated || record.m_iNestPieceIndex < 0)
			return;

		if (!body)
			body = FirstNestBody(record);

		if (!body)
			return;

		bool seated = false;
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		JOLP_LocationData data = null;
		if (locations)
			data = locations.GetLocation(record.m_iSourceLocationId);

		JOLP_HideoutPiece nest = null;
		if (data)
			nest = data.GetHideoutPiece(record.m_iNestPieceIndex);

		IEntity turret = null;
		if (nest)
			turret = JOLP_WorldUtils.FindTurretEntity(GetGame().GetWorld().FindEntityByID(nest.m_EntityId));

		SCR_CompartmentAccessComponent access = null;
		if (turret)
			access = SCR_CompartmentAccessComponent.Cast(body.FindComponent(SCR_CompartmentAccessComponent));

		if (access && access.MoveInVehicle(turret, ECompartmentType.TURRET))
			seated = true;

		if (seated)
		{
			record.m_bNestGunnerSeated = true;
			return;
		}

		if (record.m_iNestSeatAttempts >= 8)
			return;

		record.m_iNestSeatAttempts++;
		GetGame().GetCallqueue().CallLater(RetryNestGunner, 500, false, record.m_iSourceLocationId, record.m_iNestPieceIndex);
	}

	//------------------------------------------------------------------------------------------------
	protected void RetryNestGunner(int locationId, int nestIndex)
	{
		if (!m_aLiveGroups)
			return;

		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!record || record.m_iSourceLocationId != locationId || record.m_iNestPieceIndex != nestIndex)
				continue;

			TrySeatNestGunner(record, null);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FirstNestBody(notnull JOLP_LiveGroupRecord record)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
		if (!group)
			return null;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity body = agent.GetControlledEntity();
			if (body)
				return body;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Crew stay in the driver and gunner seats for the life of the hull. Passengers ride until drop-off.
	protected void TrySeatReinforcement(notnull JOLP_LiveGroupRecord record)
	{
		if (!record.m_VehicleId)
			return;

		if (record.m_eRole != JOLP_MilicianGroupRole.REINFORCEMENT && record.m_eRole != JOLP_MilicianGroupRole.VEHICLE)
			return;

		IEntity vehicle = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
		if (!vehicle || JOLP_VehicleCrew.IsHullDestroyed(vehicle))
			return;

		SCR_AIGroup crew = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_CrewGroupId));
		if (!crew)
			crew = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));

		JOLP_VehicleCrew.Seat(crew, vehicle);

		if (!record.m_bCarriesSquad || record.m_bArrived)
			return;

		if (record.m_CrewGroupId == record.m_GroupId)
			return;

		SCR_AIGroup passengers = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
		JOLP_VehicleCrew.SeatCargo(passengers, vehicle);
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_VehicleClass PickReinforcementClass(notnull JOLP_LocationData data, bool chargeGarrison)
	{
		int cost = GetRoleCost(JOLP_MilicianGroupRole.REINFORCEMENT);
		bool canPay = !chargeGarrison || data.m_iGarrisonStored >= cost;
		if (canPay && data.GetVehicleStock(JOLP_VehicleClass.TRANSPORT) > 0)
			return JOLP_VehicleClass.TRANSPORT;

		if (data.GetVehicleStock(JOLP_VehicleClass.LIGHT) > 0)
			return JOLP_VehicleClass.LIGHT;

		if (canPay && data.GetVehicleStock(JOLP_VehicleClass.HEAVY) > 0)
			return JOLP_VehicleClass.HEAVY;

		return JOLP_VehicleClass.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Foot when the roll says so, or when the rolled kind is not in stock.
	protected JOLP_VehicleClass RollHideoutVehicle(notnull JOLP_LocationData data)
	{
		JOLP_MilicianGarrisonConfig config = GetConfig();
		float vehicleChance = config.m_fHideoutVehicleChance;
		if (vehicleChance <= 0 || Math.RandomFloat01() >= vehicleChance)
			return JOLP_VehicleClass.NONE;

		float armedChance = config.m_fHideoutArmedVehicleChance;
		if (armedChance < 0)
			armedChance = 0;

		if (Math.RandomFloat01() >= armedChance)
		{
			if (data.GetVehicleStock(JOLP_VehicleClass.TRANSPORT) > 0)
				return JOLP_VehicleClass.TRANSPORT;

			return JOLP_VehicleClass.NONE;
		}

		JOLP_VehicleClass first = JOLP_VehicleClass.LIGHT;
		JOLP_VehicleClass second = JOLP_VehicleClass.HEAVY;
		if (Math.RandomFloat01() < 0.5)
		{
			first = JOLP_VehicleClass.HEAVY;
			second = JOLP_VehicleClass.LIGHT;
		}

		if (data.GetVehicleStock(first) > 0)
			return first;

		if (data.GetVehicleStock(second) > 0)
			return second;

		return JOLP_VehicleClass.NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected bool FindVehicleDeparture(notnull JOLP_LocationData data, int targetLocationId, bool qrf, out vector position, out vector angles)
	{
		float reach = data.m_fActivationRadius;
		if (reach < 80)
			reach = 80;

		array<vector> occupied = {};
		AppendVehicleOriginsNear(data.m_vPosition, reach + 20, occupied);
		JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
		if (civilians)
			civilians.AppendVehicleOriginsNear(data.m_vPosition, reach + 20, occupied);

		vector destination = data.m_vPosition;
		JOLP_LocationData target = JOLP_Global.GetLocations().GetLocation(targetLocationId);
		if (target)
			destination = ReinforcementMoveDestination(target, qrf);

		// Defense has no trip. The nearest road is the one the player just entered on, so aim across the camp.
		bool found = false;
		array<vector> nearbyPlayers = {};
		bool localDefense = !qrf && targetLocationId == data.m_iId;
		if (localDefense)
			found = FindDefenseSpawnOppositePlayers(data, reach, occupied, nearbyPlayers, position, angles);

		// The rim, on the side facing the trip, so a roadblock at the camp is already behind them.
		if (!found)
			found = JOLP_WorldUtils.FindRoadSpawnOnEdge(data.m_vPosition, reach, destination, occupied, 16, position, angles);

		if (!found && !localDefense && HideSpawnFromPlayers(data, nearbyPlayers))
			found = JOLP_WorldUtils.FindRoadSpawnFarthestFrom(data.m_vPosition, reach, occupied, 16, nearbyPlayers, true, position, angles);

		if (!found)
			found = JOLP_WorldUtils.FindVehicleRoadSpawn(data.m_vPosition, reach, occupied, 16, position, angles);

		if (!found)
			return false;

		if (!PositionClearOfPlayers(position))
		{
			array<vector> players = {};
			float search = reach + PlayerClearance();
			JOLP_WorldUtils.CollectLivingPlayerPositions(data.m_vPosition, search, players);
			found = JOLP_WorldUtils.FindRoadSpawnFarthestFrom(data.m_vPosition, reach, occupied, 16, players, true, position, angles);
			if (!found || !PositionClearOfPlayers(position))
				return false;
		}

		JOLP_WorldUtils.FaceYawToward(angles, position, destination);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle-fit road on the far side of the players already inside the radius.
	//! The farthest carriageway is used when that side has no clear stretch.
	//! False when nobody is inside, or no vehicle-fit road exists.
	protected bool FindDefenseSpawnOppositePlayers(notnull JOLP_LocationData data, float reach, array<vector> occupied, notnull array<vector> players, out vector position, out vector angles)
	{
		position = data.m_vPosition;
		angles = "0 0 0";

		if (JOLP_WorldUtils.CollectLivingPlayerPositions(data.m_vPosition, reach, players) <= 0)
			return false;

		vector nearest = players[0];
		float nearestDist = JOLP_WorldUtils.DistanceFlat(data.m_vPosition, nearest);
		for (int i = 1; i < players.Count(); i++)
		{
			float dist = JOLP_WorldUtils.DistanceFlat(data.m_vPosition, players[i]);
			if (dist >= nearestDist)
				continue;

			nearestDist = dist;
			nearest = players[i];
		}

		vector away = data.m_vPosition - nearest;
		away[1] = 0;
		if (away.Length() >= 1)
		{
			away.Normalize();
			vector opposite = data.m_vPosition + (away * reach);
			vector edgePos;
			vector edgeAngles;
			if (JOLP_WorldUtils.FindRoadSpawnOnEdge(data.m_vPosition, reach, opposite, occupied, 16, edgePos, edgeAngles))
			{
				vector dir = edgePos - data.m_vPosition;
				dir[1] = 0;
				if (dir.Length() >= 1)
				{
					dir.Normalize();
					if (vector.Dot(dir, away) > 0)
					{
						position = edgePos;
						angles = edgeAngles;
						return true;
					}
				}
			}
		}

		return JOLP_WorldUtils.FindRoadSpawnFarthestFrom(data.m_vPosition, reach, occupied, 16, players, true, position, angles);
	}

	//------------------------------------------------------------------------------------------------
	protected bool AttachCrewedVehicle(notnull JOLP_LiveGroupRecord record, JOLP_VehicleClass kind, vector vehiclePos, vector vehicleAngles, SCR_AIGroup passengers)
	{
		JOLP_MilicianGarrisonConfig config = GetConfig();
		ResourceName prefab = config.VehiclePrefab(kind);
		if (prefab.IsEmpty())
			return false;

		IEntity vehicle = JOLP_WorldUtils.SpawnEntityPrefab(prefab, vehiclePos, vehicleAngles, true);
		if (!vehicle)
			return false;

		JOLP_WorldUtils.SettleSpawnedVehicle(vehicle);
		JOLP_CivilianOutfitter.SetCharacterFaction(vehicle, config.m_sFactionKey);

		vector crewPos = JOLP_WorldUtils.CrewStandPosition(vehiclePos, vehicleAngles, 8);
		SCR_AIGroup crew = JOLP_VehicleCrew.Spawn(config, kind, crewPos, vehicleAngles);
		if (!crew)
		{
			JOLP_WorldUtils.DeleteEntitySafe(vehicle);
			return false;
		}

		ApplyMilicianFaction(crew);
		record.m_VehicleId = vehicle.GetID();
		record.m_CrewGroupId = crew.GetID();
		record.m_eVehicleClass = kind;
		record.m_bCarriesSquad = passengers != null;
		JOLP_VehicleCrew.Seat(crew, vehicle);
		if (passengers)
			JOLP_VehicleCrew.SeatCargo(passengers, vehicle);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void SubscribeCrew(notnull JOLP_LiveGroupRecord record)
	{
		SCR_AIGroup crew = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_CrewGroupId));
		if (!crew)
			return;

		crew.GetOnAgentAdded().Insert(OnMilicianAgentAdded);
		RegisterExistingMilicianAgents(crew);
	}

	//------------------------------------------------------------------------------------------------
	//! Crew only. defense true fields the hull in this place. Otherwise it drives to the target.
	protected int SpawnCrewVehicle(notnull JOLP_LocationData data, JOLP_VehicleClass kind, int targetLocationId, bool qrf, bool defense)
	{
		if (!data.TakeVehicleStock(kind))
			return 0;

		int aim = targetLocationId;
		if (defense)
			aim = data.m_iId;

		vector vehiclePos;
		vector vehicleAngles;
		if (!FindVehicleDeparture(data, aim, qrf && !defense, vehiclePos, vehicleAngles))
		{
			data.AddVehicleStock(kind, 1);
			return 0;
		}

		JOLP_LiveGroupRecord record = new JOLP_LiveGroupRecord();
		record.m_eVehicleClass = kind;
		record.m_iSourceLocationId = data.m_iId;
		record.m_iTargetLocationId = aim;
		record.m_fSpawnedAtWorldTime = GetGame().GetWorld().GetWorldTime();
		record.m_iChargedAgents = 0;

		if (!AttachCrewedVehicle(record, kind, vehiclePos, vehicleAngles, null))
		{
			data.AddVehicleStock(kind, 1);
			return 0;
		}

		record.m_GroupId = record.m_CrewGroupId;
		m_aLiveGroups.Insert(record);
		SubscribeCrew(record);

		if (defense)
		{
			record.m_eRole = JOLP_MilicianGroupRole.VEHICLE;
			record.m_bExemptFromProximityDespawn = false;
			OrderVehiclePostArrival(record, data);
			Print(string.Format("[JOLP] '%1' fielded a %2 for defense", data.m_sName, typename.EnumToString(JOLP_VehicleClass, kind)), LogLevel.NORMAL);
			return 1;
		}

		record.m_eRole = JOLP_MilicianGroupRole.REINFORCEMENT;
		record.m_bQRF = qrf;
		record.m_bExemptFromProximityDespawn = true;
		SCR_AIGroup crew = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_CrewGroupId));
		if (crew)
			AssignBehaviour(data, crew, record);

		Print(string.Format("[JOLP] '%1' sent a %2 toward a reinforcement", data.m_sName, typename.EnumToString(JOLP_VehicleClass, kind)), LogLevel.NORMAL);
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnStoredDefenseVehicles(notnull JOLP_LocationData data)
	{
		SpawnDefenseClass(data, JOLP_VehicleClass.TRANSPORT);
		SpawnDefenseClass(data, JOLP_VehicleClass.LIGHT);
		SpawnDefenseClass(data, JOLP_VehicleClass.HEAVY);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnDefenseClass(notnull JOLP_LocationData data, JOLP_VehicleClass kind)
	{
		int guard = 0;
		while (data.GetVehicleStock(kind) > 0 && guard < 8)
		{
			guard++;
			if (SpawnCrewVehicle(data, kind, data.m_iId, false, true) <= 0)
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_AIGroup DriverGroup(notnull JOLP_LiveGroupRecord record, SCR_AIGroup fallback)
	{
		if (record.m_CrewGroupId)
		{
			SCR_AIGroup crew = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_CrewGroupId));
			if (crew)
				return crew;
		}

		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	//! Redirect the crew. The passengers stay seated until the settlement drop-off.
	protected void UpdateVehicleQRFDrive(notnull SCR_AIGroup driver, notnull JOLP_LiveGroupRecord record, notnull JOLP_LocationData target, float now)
	{
		vector contact;
		if (!FreshPlayerContact(target, now, contact))
			return;

		float distance = JOLP_WorldUtils.DistanceFlat(QRFPosition(record, driver), contact);
		if (distance > 90)
			RedirectQRFMove(driver, record, target, contact, now);
	}

	//------------------------------------------------------------------------------------------------
	protected void FinishVehicleArrival(notnull JOLP_LiveGroupRecord record, notnull JOLP_LocationData target, SCR_AIGroup passengers, bool joinFight = false)
	{
		SCR_AIGroup crew = DriverGroup(record, passengers);
		if (crew)
			JOLP_MilicianWaypointHelper.ClearGroupWaypoints(crew, record);

		vector playerPos = vector.Zero;
		bool havePlayer = false;
		if (joinFight)
			havePlayer = JOLP_WorldUtils.NearestLivingPlayerPosition(target.m_vPosition, playerPos);

		if (record.m_bCarriesSquad && passengers && record.m_CrewGroupId != passengers.GetID())
		{
			vector dropAt = target.m_vPosition;
			if (record.m_VehicleId)
			{
				IEntity vehicle = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
				if (vehicle)
					dropAt = vehicle.GetOrigin();
			}

			record.m_GroupId = record.m_CrewGroupId;
			record.m_bCarriesSquad = false;
			HandOffPassengers(passengers, target, dropAt, joinFight && havePlayer, playerPos);
		}
		else
		{
			record.m_GroupId = record.m_CrewGroupId;
			record.m_bCarriesSquad = false;
		}

		record.m_iSourceLocationId = target.m_iId;
		record.m_iTargetLocationId = target.m_iId;
		record.m_eRole = JOLP_MilicianGroupRole.VEHICLE;
		record.m_bExemptFromProximityDespawn = false;
		record.m_bFreeReinforcement = false;
		record.m_bQRF = false;
		record.m_iChargedAgents = 0;

		if (joinFight && havePlayer)
		{
			record.m_bCommittedFight = true;
			SCR_AIGroup gunner = crew;
			if (!gunner)
				gunner = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));

			if (gunner)
				OrderGroupToPlayer(gunner, record, playerPos, false);

			Print(string.Format("[JOLP] Vehicle crew at '%1' moving on the nearest player", target.m_sName), LogLevel.NORMAL);
			return;
		}

		OrderVehiclePostArrival(record, target);
		Print(string.Format("[JOLP] Vehicle crew staying at '%1'", target.m_sName), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void HandOffPassengers(notnull SCR_AIGroup passengers, notnull JOLP_LocationData target, vector dropAt, bool joinFight, vector playerPos)
	{
		JOLP_LiveGroupRecord foot = new JOLP_LiveGroupRecord();
		foot.m_eRole = JOLP_MilicianGroupRole.PATROL;
		if (joinFight)
			foot.m_eRole = JOLP_MilicianGroupRole.SENTRY;

		foot.m_GroupId = passengers.GetID();
		foot.m_iSourceLocationId = target.m_iId;
		foot.m_fSpawnedAtWorldTime = GetGame().GetWorld().GetWorldTime();
		foot.m_iChargedAgents = JOLP_WorldUtils.CountLivingAgents(passengers);
		foot.m_bExemptFromProximityDespawn = false;
		m_aLiveGroups.Insert(foot);

		if (joinFight)
		{
			OrderGroupToPlayer(passengers, foot, playerPos, true);
			Print(string.Format("[JOLP] Squad at '%1' moving on the nearest player", target.m_sName), LogLevel.NORMAL);
			return;
		}

		JOLP_MilicianGarrisonConfig config = GetConfig();
		if (!config.m_rGetOutWaypointPrefab.IsEmpty())
			JOLP_MilicianWaypointHelper.AssignMoveTo(passengers, foot, config.m_rGetOutWaypointPrefab, dropAt);

		float area = target.m_fActivationRadius;
		if (area < 40)
			area = 80;

		AssignRandomLocalMission(target, passengers, foot, area, 0.55);
		Print(string.Format("[JOLP] Squad dropped at '%1'", target.m_sName), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateVehicleOrders(notnull JOLP_LiveGroupRecord record, notnull JOLP_LocationData home, float now)
	{
		vector contact;
		bool spotted = FreshPlayerContact(home, now, contact);
		if (record.m_bVehicleOrdersSet)
		{
			if (!spotted || record.m_bQRFHunting)
			{
				if (!spotted || JOLP_WorldUtils.DistanceFlat(record.m_vQRFHuntPosition, contact) < 40)
					return;
			}
		}

		OrderVehiclePostArrival(record, home);
	}

	//------------------------------------------------------------------------------------------------
	protected void OrderVehiclePostArrival(notnull JOLP_LiveGroupRecord record, notnull JOLP_LocationData home)
	{
		SCR_AIGroup crew = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_CrewGroupId));
		if (!crew)
			crew = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));

		if (!crew)
			return;

		JOLP_MilicianWaypointHelper.ClearGroupWaypoints(crew, record);
		JOLP_MilicianGarrisonConfig config = GetConfig();
		vector contact;
		bool spotted = FreshPlayerContact(home, GetGame().GetWorld().GetWorldTime(), contact);
		if (spotted && !config.m_rSearchAndDestroyWaypointPrefab.IsEmpty())
		{
			JOLP_MilicianWaypointHelper.AssignSearchAndDestroy(crew, record, config.m_rSearchAndDestroyWaypointPrefab, SnapSurface(contact), 80, 300000);
			record.m_bQRFHunting = true;
			record.m_vQRFHuntPosition = contact;
		}
		else
		{
			float radius = home.m_fActivationRadius;
			if (radius < 80)
				radius = 80;

			JOLP_MilicianWaypointHelper.AssignRoadPatrol(crew, record, config.m_rPatrolWaypointPrefab, config.m_rCycleWaypointPrefab, home.m_vPosition, radius);
			record.m_bQRFHunting = false;
		}

		record.m_bVehicleOrdersSet = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Defend a random post, or patrol a random loop, somewhere inside the location.
	protected void AssignRandomLocalMission(notnull JOLP_LocationData data, notnull SCR_AIGroup group, notnull JOLP_LiveGroupRecord record, float area, float maxRadiusShare)
	{
		JOLP_MilicianGarrisonConfig config = GetConfig();
		float roll = Math.RandomFloat01();
		array<vector> occupied = CollectGroupOrigins(data.m_iId);

		if (roll < 0.4)
		{
			vector post = PickSpreadPoint(data.m_vPosition, area * 0.9, occupied, Math.Clamp(area * 0.15, 30, 90));
			JOLP_MilicianWaypointHelper.AssignDefend(group, record, config.m_rDefendWaypointPrefab, post);
			return;
		}

		vector center = PickSpreadPoint(data.m_vPosition, area * 0.8, occupied, Math.Clamp(area * 0.15, 30, 90));
		float radius = Math.RandomFloat(Math.Max(25, area * 0.12), Math.Max(45, area * maxRadiusShare));
		int points = Math.RandomInt(3, 6);
		JOLP_MilicianWaypointHelper.AssignCyclePatrol(group, record, config.m_rPatrolWaypointPrefab, config.m_rCycleWaypointPrefab, center, radius, points, Math.RandomFloat(0, Math.PI2), 0.4);
	}

	//------------------------------------------------------------------------------------------------
	//! Extra troops spawned after the fight has started. The opening garrison keeps its old scatter.
	protected bool HideSpawnFromPlayers(notnull JOLP_LocationData data, notnull array<vector> players)
	{
		if (m_bOpeningSpawn || !data.m_bActive || !FightHasStarted(data))
			return false;

		return JOLP_WorldUtils.CollectLivingPlayerPositions(data.m_vPosition, PlayerPresenceRadius(data), players) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Shots have been fired, or milician here have already seen a player.
	protected bool FightHasStarted(notnull JOLP_LocationData data)
	{
		if (IsLocationInCombat(data.m_iId))
			return true;

		vector contact;
		return FreshPlayerContact(data, GetGame().GetWorld().GetWorldTime(), contact);
	}

	//------------------------------------------------------------------------------------------------
	protected float PlayerPresenceRadius(notnull JOLP_LocationData data)
	{
		float radius = data.m_fActivationRadius * GetDespawnMultiplier();
		if (radius < data.m_fActivationRadius)
			radius = data.m_fActivationRadius;

		if (radius < 40)
			radius = 40;

		return radius;
	}

	//------------------------------------------------------------------------------------------------
	//! Spread point. The far rim is used only for troops added after the fight has started.
	protected vector PickLocalInfantrySpawn(notnull JOLP_LocationData data, float radius, array<vector> occupied, float minSeparation)
	{
		array<vector> players = {};
		if (HideSpawnFromPlayers(data, players))
		{
			vector edge;
			if (PickOppositeEdge(data.m_vPosition, radius, occupied, minSeparation, players, edge))
				return edge;
		}

		return PickSpreadPoint(data.m_vPosition, radius, occupied, minSeparation);
	}

	//------------------------------------------------------------------------------------------------
	//! Point on the outer ring farthest from the players, so a squad does not appear in front of them.
	protected bool PickOppositeEdge(vector center, float radius, array<vector> occupied, float minSeparation, array<vector> players, out vector position)
	{
		position = center;
		if (radius < 30)
			radius = 30;

		if (!players || players.IsEmpty())
			return false;

		vector best = center;
		float bestScore = -1;
		bool found = false;
		for (int ring = 0; ring < 2; ring++)
		{
			float dist = radius * 0.78;
			if (ring > 0)
				dist = radius * 0.92;

			for (int i = 0; i < 12; i++)
			{
				float angle = (i / (float)12) * Math.PI2;
				vector pos = center + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);
				pos = SnapSurface(pos);

				float fromPlayers = JOLP_WorldUtils.NearestFlatDistance(pos, players);
				float score = fromPlayers;
				if (JOLP_WorldUtils.Clearance(pos, occupied) < minSeparation)
					score = fromPlayers * 0.25;

				if (score <= bestScore)
					continue;

				bestScore = score;
				best = pos;
				found = true;
			}
		}

		position = best;
		return found;
	}

	//------------------------------------------------------------------------------------------------
	protected vector PickSpreadPoint(vector center, float radius, array<vector> occupied, float minSeparation)
	{
		if (radius < 10)
			radius = 10;

		vector best = JOLP_WorldUtils.OffsetOnGround(center, radius);
		float bestClearance = JOLP_WorldUtils.Clearance(best, occupied);

		for (int i = 0; i < 9; i++)
		{
			vector pos = JOLP_WorldUtils.OffsetOnGround(center, radius * Math.RandomFloat(0.15, 1));
			float clearance = JOLP_WorldUtils.Clearance(pos, occupied);
			if (clearance >= minSeparation)
				return pos;

			if (clearance > bestClearance)
			{
				bestClearance = clearance;
				best = pos;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected float PlayerClearance()
	{
		JOLP_MilicianGarrisonConfig config = GetConfig();
		if (config && config.m_fPlayerClearance > 0)
			return config.m_fPlayerClearance;

		return 200;
	}

	//------------------------------------------------------------------------------------------------
	protected bool PositionClearOfPlayers(vector pos)
	{
		return JOLP_WorldUtils.ClearOfLivingPlayers(pos, PlayerClearance());
	}

	//------------------------------------------------------------------------------------------------
	//! A living group the player can see stays in the world. An already-gone package can be removed.
	protected bool DespawnBlockedByPlayer(notnull JOLP_LiveGroupRecord record)
	{
		vector position = LiveGroupPosition(record);
		if (position[0] == 0 && position[1] == 0 && position[2] == 0)
			return false;

		return !PositionClearOfPlayers(position);
	}

	//------------------------------------------------------------------------------------------------
	//! A point in the area at least PlayerClearance from every living player.
	protected bool PickSpawnAwayFromPlayers(vector center, float radius, array<vector> occupied, float minSeparation, out vector position)
	{
		position = center;
		if (radius < 10)
			radius = 10;

		bool found = false;
		vector best = center;
		float bestSep = -1;
		for (int ring = 0; ring < 3; ring++)
		{
			float dist = radius * 0.6;
			if (ring == 1)
				dist = radius * 0.8;
			if (ring == 2)
				dist = radius;

			float shift = ring * 0.17;
			for (int i = 0; i < 12; i++)
			{
				float angle = (i / (float)12) * Math.PI2 + shift;
				vector pos = center + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);
				pos = SnapSurface(pos);
				if (!PositionClearOfPlayers(pos))
					continue;

				float sep = JOLP_WorldUtils.Clearance(pos, occupied);
				if (!found || sep > bestSep)
				{
					best = pos;
					bestSep = sep;
					found = true;
				}

				if (sep < minSeparation)
					continue;

				position = pos;
				return true;
			}
		}

		if (!found)
			return false;

		position = best;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The sentry prefab is not left to spawn its authored pair. Members are created here so the count can be 2 or 3.
	//! snapToSurface false keeps a roof perch. GetSurfaceY would put that body under the building.
	protected int FillGroupMembers(notnull SCR_AIGroup group, ResourceName groupPrefab, vector origin, vector angles, int count, bool snapToSurface = true, ResourceName memberPrefab = "")
	{
		if (count <= 0)
			return 0;

		array<ResourceName> slots = {};
		if (!memberPrefab.IsEmpty())
			slots.Insert(memberPrefab);
		else
			ReadGroupMemberPrefabs(groupPrefab, slots);
		if (slots.IsEmpty())
		{
			ResourceName fallback = GetConfig().m_rDriverPrefab;
			if (!fallback.IsEmpty())
				slots.Insert(fallback);
		}

		if (slots.IsEmpty())
			return 0;

		int slotIndex = 0;
		int spawned = 0;
		for (int i = 0; i < count; i++)
		{
			vector pos = origin;
			if (snapToSurface)
			{
				float side = i - (count * 0.5);
				pos = origin + Vector(side * 0.9, 0, 0);
				pos = SnapSurface(pos);
			}

			IEntity body = JOLP_WorldUtils.SpawnEntityPrefab(slots[slotIndex], pos, angles, true);
			slotIndex++;
			if (slotIndex >= slots.Count())
				slotIndex = 0;

			if (!body)
				continue;

			if (!group.AddAIEntityToGroup(body))
			{
				JOLP_WorldUtils.DeleteEntitySafe(body);
				continue;
			}

			spawned++;
		}

		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAIGroupPrefab(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return false;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return false;

		BaseResourceObject resourceObject = resource.GetResource();
		if (!resourceObject)
			return false;

		IEntitySource source = resourceObject.ToEntitySource();
		if (!source)
			return false;

		return source.GetClassName() == "SCR_AIGroup";
	}

	//------------------------------------------------------------------------------------------------
	protected void ReadGroupMemberPrefabs(ResourceName groupPrefab, notnull array<ResourceName> slots)
	{
		if (groupPrefab.IsEmpty())
			return;

		Resource resource = Resource.Load(groupPrefab);
		if (!resource || !resource.IsValid())
			return;

		BaseResourceObject resourceObject = resource.GetResource();
		if (!resourceObject)
			return;

		IEntitySource source = resourceObject.ToEntitySource();
		if (!source)
			return;

		source.Get("m_aUnitPrefabSlots", slots);
	}

	//------------------------------------------------------------------------------------------------
	//! Living milician vehicles near a point. Civilian traffic uses this so the two fleets do not spawn on top of each other.
	void AppendVehicleOriginsNear(vector center, float radius, notnull array<vector> origins)
	{
		if (!m_aLiveGroups)
			return;

		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!record || !record.m_VehicleId)
				continue;

			IEntity vehicle = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
			if (!vehicle)
				continue;

			if (vector.Distance(vehicle.GetOrigin(), center) <= radius)
				origins.Insert(vehicle.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected array<vector> CollectGroupOrigins(int locationId)
	{
		array<vector> origins = new array<vector>();
		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!record || record.m_iSourceLocationId != locationId)
				continue;

			IEntity group = GetGame().GetWorld().FindEntityByID(record.m_GroupId);
			if (group)
				origins.Insert(group.GetOrigin());
		}

		return origins;
	}

	//------------------------------------------------------------------------------------------------
	//! Local packages still inside the settlement go back into storage.
	//! A group that has already left the bubble stays in the world. That includes a patrol from
	//! this city that walked out toward another, and it never includes inbound groups: those
	//! keep the other city's source id, so this loop does not see them.
	protected void DeactivateLocation(notnull JOLP_LocationData data)
	{
		int returned = 0;
		int kept = 0;
		int held = 0;
		float localRadius = data.m_fActivationRadius * GetDespawnMultiplier();
		for (int i = m_aLiveGroups.Count() - 1; i >= 0; i--)
		{
			JOLP_LiveGroupRecord record = m_aLiveGroups[i];
			if (!record || record.m_iSourceLocationId != data.m_iId)
				continue;

			if (record.m_bExemptFromProximityDespawn)
				continue;

			if (PackageOutsideLocation(record, data.m_vPosition, localRadius))
			{
				kept++;
				continue;
			}

			if (DespawnBlockedByPlayer(record))
			{
				held++;
				continue;
			}

			returned += DespawnRecord(record, true, true);
			m_aLiveGroups.Remove(i);
		}

		if (held > 0)
			return;

		data.m_bActive = false;
		string outside = "";
		if (kept > 0)
			outside = string.Format(", left %1 outside", kept);

		Print(string.Format("[JOLP] Deactivated '%1': returned %2%3 (stored %4)", data.m_sName, returned, outside, data.m_iGarrisonStored), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the squad or its vehicle is beyond the bubble that just went quiet.
	//! No resolvable position means the package is already gone, so the caller stores it back.
	protected bool PackageOutsideLocation(notnull JOLP_LiveGroupRecord record, vector center, float radius)
	{
		vector position = LiveGroupPosition(record);
		bool havePosition = position[0] != 0 || position[1] != 0 || position[2] != 0;
		if (havePosition && vector.Distance(position, center) > radius)
			return true;

		if (!record.m_VehicleId)
			return false;

		IEntity vehicle = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
		if (!vehicle)
			return false;

		return vector.Distance(vehicle.GetOrigin(), center) > radius;
	}

	//------------------------------------------------------------------------------------------------
	//! returnInfantry puts living passengers back in the source pool. Crew never count as infantry.
	//! returnVehicle puts a surviving hull back in that same place's stock.
	//! A hull whose crew is dead or unconscious stays where it is and is not restocked.
	protected int DespawnRecord(notnull JOLP_LiveGroupRecord record, bool returnInfantry, bool returnVehicle = false)
	{
		bool crewSeparate = record.m_CrewGroupId && record.m_CrewGroupId != record.m_GroupId;
		int infantryAlive = 0;
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
		if (group)
		{
			if (crewSeparate || record.m_eVehicleClass == JOLP_VehicleClass.NONE)
				infantryAlive = JOLP_WorldUtils.CountLivingAgents(group);
		}

		IEntity vehicle = null;
		if (record.m_VehicleId)
			vehicle = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);

		SCR_AIGroup crewGroup = null;
		if (record.m_CrewGroupId)
			crewGroup = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_CrewGroupId));
		else if (record.m_eVehicleClass != JOLP_VehicleClass.NONE)
			crewGroup = group;

		bool hullOk = vehicle && !JOLP_VehicleCrew.IsHullDestroyed(vehicle);
		bool neutralized = hullOk && JOLP_VehicleCrew.IsCrewNeutralized(vehicle, crewGroup);
		if (neutralized)
		{
			// Seated men stay in the hull. They are not stored back, or the town would spawn a second copy.
			if (group && (crewSeparate || record.m_eVehicleClass == JOLP_VehicleClass.NONE))
			{
				infantryAlive = infantryAlive - JOLP_VehicleCrew.CountGroupOccupants(vehicle, group);
				if (infantryAlive < 0)
					infantryAlive = 0;
			}

			JOLP_VehicleCrew.ReleaseOccupants(vehicle);
		}

		JOLP_MilicianWaypointHelper.DeleteTrackedWaypoints(record);
		ReleaseGroup(record.m_GroupId);
		if (crewSeparate)
			ReleaseGroup(record.m_CrewGroupId);

		if (record.m_CompositionId)
			JOLP_WorldUtils.DeleteEntitySafe(GetGame().GetWorld().FindEntityByID(record.m_CompositionId));

		// A destroyed hull stays where it was knocked out.
		// An intact hull stays too when its crew is dead or unconscious. Only a hull someone can still fight from is removed.
		if (vehicle && hullOk && !neutralized)
			JOLP_WorldUtils.DeleteEntitySafe(vehicle);

		JOLP_LocationData source = JOLP_Global.GetLocations().GetLocation(record.m_iSourceLocationId);
		if (source)
		{
			if (returnInfantry)
				source.m_iGarrisonStored = source.m_iGarrisonStored + infantryAlive;

			if (returnVehicle && hullOk && !neutralized)
				source.AddVehicleStock(record.m_eVehicleClass, 1);

			if (neutralized)
				Print(string.Format("[JOLP] Neutralized vehicle left at '%1'", source.m_sName), LogLevel.NORMAL);
		}

		return infantryAlive;
	}

	//------------------------------------------------------------------------------------------------
	protected void ReleaseGroup(EntityID groupId)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(groupId));
		if (!group)
			return;

		group.GetOnAgentAdded().Remove(OnMilicianAgentAdded);
		JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
		if (civilians)
			civilians.SuppressMilicianGroup(group);

		JOLP_WorldUtils.DeleteEntitySafe(group);
	}

	//------------------------------------------------------------------------------------------------
	protected int CountLocalLiveAgents(int locationId)
	{
		int total = 0;
		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!record || record.m_iSourceLocationId != locationId)
				continue;

			// Crew are not infantry. Counting them would fill the defense budget and leave stored men unspawned.
			if (record.m_eRole == JOLP_MilicianGroupRole.VEHICLE)
				continue;

			if (record.m_bExemptFromProximityDespawn)
				continue;

			SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
			total += JOLP_WorldUtils.CountLivingAgents(group);
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Crew is dead or unconscious and the hull is not. Keep it at the settlement the player is in.
	//! Returns false when nobody is there. The caller leaves that hull in the world and does not restock it.
	protected bool ParkAbandonedVehicle(notnull JOLP_LiveGroupRecord record)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return false;

		JOLP_LocationData owner = locations.GetLocation(record.m_iSourceLocationId);
		JOLP_LocationData target = locations.GetLocation(record.m_iTargetLocationId);
		JOLP_LocationData home = null;
		if (owner && owner.m_bActive)
			home = owner;
		else if (target && target.m_bActive)
			home = target;

		if (!home)
			return false;

		bool already = record.m_eRole == JOLP_MilicianGroupRole.VEHICLE
			&& !record.m_bExemptFromProximityDespawn
			&& record.m_iSourceLocationId == home.m_iId;

		record.m_iSourceLocationId = home.m_iId;
		record.m_iTargetLocationId = home.m_iId;
		record.m_eRole = JOLP_MilicianGroupRole.VEHICLE;
		record.m_bExemptFromProximityDespawn = false;
		record.m_bCarriesSquad = false;
		record.m_bQRF = false;
		record.m_bFreeReinforcement = false;

		if (!already)
			Print(string.Format("[JOLP] Vehicle at '%1' lost its crew and stays where it is", home.m_sName), LogLevel.NORMAL);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateLivePackages()
	{
		RefreshPlayerContacts();

		float now = GetGame().GetWorld().GetWorldTime();
		JOLP_MilicianGarrisonConfig config = GetConfig();
		int patrolMax = config.m_iPatrolMaxDurationMs;
		if (patrolMax <= 0)
			patrolMax = 600000;

		int arriveDelay = config.m_iReinforceArriveDespawnMs;
		if (arriveDelay <= 0)
			arriveDelay = 120000;

		for (int i = m_aLiveGroups.Count() - 1; i >= 0; i--)
		{
			JOLP_LiveGroupRecord record = m_aLiveGroups[i];
			if (!record)
			{
				m_aLiveGroups.Remove(i);
				continue;
			}

			SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
			if ((!group || JOLP_WorldUtils.CountLivingAgents(group) <= 0) && record.m_CrewGroupId && record.m_CrewGroupId != record.m_GroupId)
			{
				SCR_AIGroup crew = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_CrewGroupId));
				if (crew && JOLP_WorldUtils.CountLivingAgents(crew) > 0)
				{
					ReleaseGroup(record.m_GroupId);
					record.m_GroupId = record.m_CrewGroupId;
					record.m_bCarriesSquad = false;
					group = crew;
				}
			}

			bool operatorsDown = !group || JOLP_WorldUtils.CountLivingAgents(group) <= 0;
			// Unconscious men still sit in the group. The hull is neutralized only when the crew is down too.
			if (!operatorsDown && record.m_VehicleId && !JOLP_VehicleCrew.HasFightingMember(group))
			{
				SCR_AIGroup crewStillUp = null;
				if (record.m_CrewGroupId && record.m_CrewGroupId != record.m_GroupId)
					crewStillUp = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_CrewGroupId));

				if (!JOLP_VehicleCrew.HasFightingMember(crewStillUp))
					operatorsDown = true;
			}

			if (operatorsDown)
			{
				IEntity wreck = null;
				if (record.m_VehicleId)
					wreck = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);

				bool hullOk = wreck && !JOLP_VehicleCrew.IsHullDestroyed(wreck);
				// Crew are dead or unconscious. Leave an intact hull here while the player is still around.
				// Putting it back in stock now makes this place spawn a replacement.
				if (hullOk && ParkAbandonedVehicle(record))
					continue;

				// Crew down: leave the hull, do not restock. A hull someone can still fight from goes back into stock.
				DespawnRecord(record, false, hullOk);
				m_aLiveGroups.Remove(i);
				continue;
			}

			if (record.m_bCommittedFight && record.m_eRole == JOLP_MilicianGroupRole.VEHICLE)
			{
				TrySeatReinforcement(record);
				IEntity hull = null;
				if (record.m_VehicleId)
					hull = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);

				if (!hull || JOLP_VehicleCrew.IsHullDestroyed(hull))
				{
					DespawnRecord(record, false, false);
					m_aLiveGroups.Remove(i);
					continue;
				}

				UpdateCommittedFight(record, group);
				continue;
			}

			if (record.m_bCommittedFight)
			{
				UpdateCommittedFight(record, group);
				continue;
			}

			if (record.m_eRole == JOLP_MilicianGroupRole.PATROL)
			{
				if (now - record.m_fSpawnedAtWorldTime >= patrolMax && !DespawnBlockedByPlayer(record))
				{
					DespawnRecord(record, true, true);
					m_aLiveGroups.Remove(i);
					Print("[JOLP] Patrol completed / timed out — stored back", LogLevel.NORMAL);
				}
				continue;
			}

			if (record.m_eRole == JOLP_MilicianGroupRole.VEHICLE)
			{
				TrySeatReinforcement(record);
				JOLP_LocationData home = JOLP_Global.GetLocations().GetLocation(record.m_iSourceLocationId);
				IEntity hull = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
				if (!hull || JOLP_VehicleCrew.IsHullDestroyed(hull))
				{
					DespawnRecord(record, false, false);
					m_aLiveGroups.Remove(i);
					continue;
				}

				if (home)
					UpdateVehicleOrders(record, home, now);

				continue;
			}

			if (record.m_eRole == JOLP_MilicianGroupRole.REINFORCEMENT)
			{
				JOLP_LocationData target = JOLP_Global.GetLocations().GetLocation(record.m_iTargetLocationId);
				if (!target)
				{
					DespawnRecord(record, !record.m_bFreeReinforcement, true);
					m_aLiveGroups.Remove(i);
					continue;
				}

				if (!record.m_bArrived)
				{
					IEntity hull = null;
					if (record.m_VehicleId)
						hull = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);

					bool hullAlive = hull && !JOLP_VehicleCrew.IsHullDestroyed(hull);
					if (record.m_VehicleId && !hullAlive)
					{
						// A destroyed hull is not returned to stock. The squad walks the rest of the way.
						if (record.m_CrewGroupId && record.m_CrewGroupId != record.m_GroupId)
							ReleaseGroup(record.m_CrewGroupId);

						record.m_CrewGroupId = record.m_GroupId;
						record.m_eVehicleClass = JOLP_VehicleClass.NONE;
						record.m_bCarriesSquad = false;
						array<AIWaypoint> held = {};
						group.GetWaypoints(held);
						if (held.IsEmpty())
						{
							JOLP_LocationData source = JOLP_Global.GetLocations().GetLocation(record.m_iSourceLocationId);
							if (source)
								AssignBehaviour(source, group, record);
						}
					}

					if (hullAlive)
					{
						TrySeatReinforcement(record);
						// Unload where the fight is, before the town-center arrival radius.
						if (VehicleContactDrop(record, group, hull, target, now))
						{
							record.m_bArrived = true;
							record.m_fArrivedAtWorldTime = now;
							Print(string.Format("[JOLP] Vehicle dropping troops under contact near '%1'", target.m_sName), LogLevel.NORMAL);
							FinishVehicleArrival(record, target, group, true);
							continue;
						}

						if (record.m_bQRF)
						{
							SCR_AIGroup driver = DriverGroup(record, group);
							UpdateVehicleQRFDrive(driver, record, target, now);
						}
					}

					if (!record.m_bQRF && PlayersFightingAt(target))
						RedirectReinforcementToPlayer(record, group, target);

					float arriveRadius = Math.Max(40, target.m_fActivationRadius * 0.5);
					bool arrived = false;
					if (hullAlive)
						arrived = vector.Distance(hull.GetOrigin(), target.m_vPosition) <= arriveRadius;
					else
						arrived = vector.Distance(group.GetOrigin(), target.m_vPosition) <= arriveRadius;

					if (arrived)
					{
						record.m_bArrived = true;
						record.m_fArrivedAtWorldTime = now;
						Print(string.Format("[JOLP] Reinforcement arrived at '%1'", target.m_sName), LogLevel.NORMAL);
					}
				}

				// A town the player is already in does not bank the squad. They go straight for the nearest player.
				if (record.m_bArrived && !record.m_bQRF && PlayersFightingAt(target))
				{
					if (record.m_VehicleId && record.m_eVehicleClass != JOLP_VehicleClass.NONE)
					{
						FinishVehicleArrival(record, target, group, true);
						continue;
					}

					if (CommitFootReinforcement(record, target, group))
						continue;
				}

				if (record.m_bArrived && record.m_VehicleId && record.m_eVehicleClass != JOLP_VehicleClass.NONE)
				{
					FinishVehicleArrival(record, target, group);
					continue;
				}

				if (record.m_bQRF)
					UpdateQRFHunt(group, record, target, now);

				if (record.m_bArrived && record.m_bQRF)
				{
					float holdRadius = target.m_fActivationRadius * GetDespawnMultiplier();
					vector unusedContact;
					bool contactFresh = FreshPlayerContact(target, now, unusedContact);
					bool stay = contactFresh || JOLP_WorldUtils.PlayerInRange(target.m_vPosition, holdRadius);

					if (stay)
					{
						if (!contactFresh && !record.m_bQRFHunting && !record.m_bQRFHolding)
							BeginQRFHold(group, record, target);
					}
					else if (!DespawnBlockedByPlayer(record))
					{
						AbsorbReinforcement(record, target);
						m_aLiveGroups.Remove(i);
					}
					continue;
				}

				if (record.m_bArrived && record.m_bFreeReinforcement)
				{
					if (!DespawnBlockedByPlayer(record))
					{
						AbsorbReinforcement(record, target);
						m_aLiveGroups.Remove(i);
					}
					continue;
				}

				if (record.m_bArrived && now - record.m_fArrivedAtWorldTime >= arriveDelay && !DespawnBlockedByPlayer(record))
				{
					AbsorbReinforcement(record, target);
					m_aLiveGroups.Remove(i);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected float GetDespawnMultiplier()
	{
		JOLP_MilicianGarrisonConfig config = GetConfig();
		if (config && config.m_fDespawnMultiplier > 1.0)
			return config.m_fDespawnMultiplier;

		return 1.15;
	}

	//------------------------------------------------------------------------------------------------
	protected void BeginQRFHold(notnull SCR_AIGroup group, notnull JOLP_LiveGroupRecord record, notnull JOLP_LocationData target)
	{
		JOLP_MilicianWaypointHelper.ClearGroupWaypoints(group, record);
		JOLP_MilicianWaypointHelper.AssignDefend(group, record, GetConfig().m_rDefendWaypointPrefab, group.GetOrigin());
		record.m_bQRFHolding = true;
		Print(string.Format("[JOLP] QRF holding at '%1'", target.m_sName), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! A sighting stays usable for three minutes, including after the spotter is dead.
	protected bool FreshPlayerContact(notnull JOLP_LocationData data, float now, out vector contact)
	{
		contact = data.m_vPlayerContact;
		if (data.m_fPlayerContactWorldTime < 0)
			return false;

		return now - data.m_fPlayerContactWorldTime <= 180000;
	}

	//------------------------------------------------------------------------------------------------
	protected void NotePlayerContact(notnull JOLP_LocationData data, vector position)
	{
		data.m_vPlayerContact = SnapSurface(position);
		data.m_fPlayerContactWorldTime = GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Living milician report players they can see. The shot that called the QRF is recorded separately.
	protected void RefreshPlayerContacts()
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations || !m_aLiveGroups)
			return;

		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!record)
				continue;

			SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
			vector seen;
			bool spotted = group && GroupSeesPlayer(group, seen);
			if (!spotted && record.m_CrewGroupId && record.m_CrewGroupId != record.m_GroupId)
			{
				SCR_AIGroup crew = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_CrewGroupId));
				spotted = crew && GroupSeesPlayer(crew, seen);
			}

			if (!spotted)
				continue;

			int locationId = record.m_iSourceLocationId;
			if (record.m_bQRF && record.m_iTargetLocationId >= 0)
				locationId = record.m_iTargetLocationId;

			JOLP_LocationData data = locations.GetLocation(locationId);
			if (!data || (!data.IsSettlement() && !data.IsHideout()))
				continue;

			NotePlayerContact(data, seen);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool GroupSeesPlayer(notnull SCR_AIGroup group, out vector position)
	{
		position = vector.Zero;
		float bestAge = 16;
		bool found = false;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity body = agent.GetControlledEntity();
			if (!body)
				continue;

			PerceptionComponent perception = PerceptionComponent.Cast(body.FindComponent(PerceptionComponent));
			if (perception)
			{
				ConsiderSeenPlayers(perception, ETargetCategory.ENEMY, bestAge, position, found);
				ConsiderSeenPlayers(perception, ETargetCategory.UNKNOWN, bestAge, position, found);
			}

			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(body.FindComponent(SCR_AICombatComponent));
			if (combat)
				ConsiderSeenPlayer(combat.GetCurrentTarget(), bestAge, position, found);
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	protected void ConsiderSeenPlayers(notnull PerceptionComponent perception, ETargetCategory category, inout float bestAge, inout vector position, inout bool found)
	{
		array<BaseTarget> targets = {};
		perception.GetTargetsList(targets, category);
		foreach (BaseTarget target : targets)
			ConsiderSeenPlayer(target, bestAge, position, found);
	}

	//------------------------------------------------------------------------------------------------
	//! Fifteen seconds is as long as a sighting still counts as "seen", matching vanilla perception use.
	protected void ConsiderSeenPlayer(BaseTarget target, inout float bestAge, inout vector position, inout bool found)
	{
		if (!target)
			return;

		float age = target.GetTimeSinceSeen();
		if (age < 0 || age >= bestAge)
			return;

		IEntity seen = target.GetTargetEntity();
		if (!seen || !JOLP_CivilianOutfitter.IsPlayerInstigator(seen))
			return;

		vector seenAt = target.GetLastSeenPosition();
		if (seenAt[0] == 0 && seenAt[1] == 0 && seenAt[2] == 0)
			seenAt = seen.GetOrigin();

		bestAge = age;
		position = seenAt;
		found = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateQRFHunt(notnull SCR_AIGroup group, notnull JOLP_LiveGroupRecord record, notnull JOLP_LocationData target, float now)
	{
		vector contact;
		if (!FreshPlayerContact(target, now, contact))
			return;

		vector from = QRFPosition(record, group);
		float distance = JOLP_WorldUtils.DistanceFlat(from, contact);
		bool mounted = QRFSquadMounted(record);

		if (!record.m_bQRFHunting && mounted && distance > 90 && !QRFReachedRoad(group, record, from))
		{
			RedirectQRFMove(group, record, target, contact, now);
			return;
		}

		OrderQRFSearch(group, record, target, contact, now);
	}

	//------------------------------------------------------------------------------------------------
	protected vector QRFPosition(notnull JOLP_LiveGroupRecord record, notnull SCR_AIGroup group)
	{
		if (record.m_VehicleId)
		{
			IEntity vehicle = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
			if (vehicle)
				return vehicle.GetOrigin();
		}

		return group.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	protected bool QRFSquadMounted(notnull JOLP_LiveGroupRecord record)
	{
		if (!record.m_VehicleId)
			return false;

		IEntity vehicle = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
		if (!vehicle)
			return false;

		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartments)
			return false;

		if (CompartmentOccupied(compartments, ECompartmentType.PILOT))
			return true;

		if (CompartmentOccupied(compartments, ECompartmentType.TURRET))
			return true;

		return CompartmentOccupied(compartments, ECompartmentType.CARGO);
	}

	//------------------------------------------------------------------------------------------------
	protected bool CompartmentOccupied(notnull SCR_BaseCompartmentManagerComponent compartments, ECompartmentType compartmentType)
	{
		array<IEntity> occupants = {};
		compartments.GetOccupantsOfType(occupants, compartmentType);
		return !occupants.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! The truck has reached the road it was sent to, even if the player is still off that road.
	protected bool QRFReachedRoad(notnull SCR_AIGroup group, notnull JOLP_LiveGroupRecord record, vector from)
	{
		if (record.m_fQRFHuntOrderedAt < 0)
			return false;

		array<AIWaypoint> waypoints = {};
		group.GetWaypoints(waypoints);
		if (waypoints.IsEmpty())
			return true;

		AIWaypoint current = waypoints[0];
		if (!current)
			return false;

		return JOLP_WorldUtils.DistanceFlat(from, current.GetOrigin()) <= 40;
	}

	//------------------------------------------------------------------------------------------------
	protected void RedirectQRFMove(notnull SCR_AIGroup group, notnull JOLP_LiveGroupRecord record, notnull JOLP_LocationData target, vector contact, float now)
	{
		if (record.m_fQRFHuntOrderedAt >= 0 && JOLP_WorldUtils.DistanceFlat(record.m_vQRFHuntPosition, contact) < 80)
			return;

		JOLP_MilicianWaypointHelper.ClearGroupWaypoints(group, record);
		JOLP_MilicianWaypointHelper.AssignMoveTo(group, record, GetConfig().m_rMoveWaypointPrefab, RoadNearContact(contact));
		record.m_vQRFHuntPosition = contact;
		record.m_fQRFHuntOrderedAt = now;
		Print(string.Format("[JOLP] QRF driving toward the spotted player near '%1'", target.m_sName), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void OrderQRFSearch(notnull SCR_AIGroup group, notnull JOLP_LiveGroupRecord record, notnull JOLP_LocationData target, vector contact, float now)
	{
		if (record.m_bQRFHunting && JOLP_WorldUtils.DistanceFlat(record.m_vQRFHuntPosition, contact) < 40)
			return;

		JOLP_MilicianGarrisonConfig config = GetConfig();
		if (config.m_rSearchAndDestroyWaypointPrefab.IsEmpty())
			return;

		vector search = SnapSurface(contact);
		bool mounted = QRFSquadMounted(record);
		JOLP_MilicianWaypointHelper.ClearGroupWaypoints(group, record);

		if (mounted && !config.m_rGetOutWaypointPrefab.IsEmpty())
			JOLP_MilicianWaypointHelper.AssignMoveTo(group, record, config.m_rGetOutWaypointPrefab, SnapSurface(QRFPosition(record, group)));

		if (!JOLP_MilicianWaypointHelper.AssignSearchAndDestroy(group, record, config.m_rSearchAndDestroyWaypointPrefab, search, 50, 300000))
			return;
		record.m_bQRFHunting = true;
		record.m_vQRFHuntPosition = contact;
		record.m_fQRFHuntOrderedAt = now;
		Print(string.Format("[JOLP] QRF searching for the spotted player near '%1'", target.m_sName), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Passenger trucks unload when the fight is on them. The town-center radius is only for a quiet arrival.
	protected bool VehicleContactDrop(notnull JOLP_LiveGroupRecord record, notnull SCR_AIGroup group, notnull IEntity hull, notnull JOLP_LocationData target, float now)
	{
		if (!record.m_bCarriesSquad || !record.m_CrewGroupId || record.m_CrewGroupId == record.m_GroupId)
			return false;

		vector from = hull.GetOrigin();
		vector seen;
		bool sees = GroupSeesPlayer(group, seen);
		if (!sees)
		{
			SCR_AIGroup crew = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_CrewGroupId));
			sees = crew && GroupSeesPlayer(crew, seen);
		}

		if (sees && JOLP_WorldUtils.DistanceFlat(from, seen) <= 300)
			return true;

		vector playerPos;
		if (JOLP_WorldUtils.NearestLivingPlayerPosition(from, playerPos) && JOLP_WorldUtils.DistanceFlat(from, playerPos) <= 200)
			return true;

		vector contact;
		if (FreshPlayerContact(target, now, contact) && JOLP_WorldUtils.DistanceFlat(from, contact) <= 200)
			return true;

		// Diverted onto a road by the fight, then stopped there short of the center.
		if (!record.m_bQRF && PlayersFightingAt(target) && record.m_fQRFHuntOrderedAt >= 0)
		{
			SCR_AIGroup driver = DriverGroup(record, group);
			if (driver && QRFReachedRoad(driver, record, from))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The town is already in a fight, so the convoy aims at the player instead of the garrison drop-off.
	protected void RedirectReinforcementToPlayer(notnull JOLP_LiveGroupRecord record, notnull SCR_AIGroup group, notnull JOLP_LocationData target)
	{
		vector playerPos;
		if (!JOLP_WorldUtils.NearestLivingPlayerPosition(target.m_vPosition, playerPos))
			return;

		if (record.m_fQRFHuntOrderedAt >= 0 && JOLP_WorldUtils.DistanceFlat(record.m_vQRFHuntPosition, playerPos) < 80)
			return;

		JOLP_MilicianGarrisonConfig config = GetConfig();
		if (config.m_rMoveWaypointPrefab.IsEmpty())
			return;

		SCR_AIGroup driver = group;
		if (record.m_CrewGroupId)
		{
			SCR_AIGroup crew = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_CrewGroupId));
			if (crew)
				driver = crew;
		}

		bool mounted = false;
		if (record.m_VehicleId && record.m_eVehicleClass != JOLP_VehicleClass.NONE)
		{
			IEntity hull = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
			mounted = hull && !JOLP_VehicleCrew.IsHullDestroyed(hull);
		}

		vector dest = SnapSurface(playerPos);
		if (mounted)
			dest = RoadNearContact(playerPos);

		bool firstOrder = record.m_fQRFHuntOrderedAt < 0;
		JOLP_MilicianWaypointHelper.ClearGroupWaypoints(driver, record);
		JOLP_MilicianWaypointHelper.AssignMoveTo(driver, record, config.m_rMoveWaypointPrefab, dest);
		record.m_vQRFHuntPosition = playerPos;
		record.m_fQRFHuntOrderedAt = GetGame().GetWorld().GetWorldTime();

		if (firstOrder)
			Print(string.Format("[JOLP] Reinforcement diverting to the player near '%1'", target.m_sName), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! True while a living player is still inside the settlement. Arrivals join that fight.
	protected bool PlayersFightingAt(notnull JOLP_LocationData data)
	{
		if (!data.IsSettlement())
			return false;

		return JOLP_WorldUtils.PlayerInRange(data.m_vPosition, PlayerPresenceRadius(data));
	}

	//------------------------------------------------------------------------------------------------
	//! Keep the squad in the world and send it at the nearest player. False when there is nobody to send them after.
	protected bool CommitFootReinforcement(notnull JOLP_LiveGroupRecord record, notnull JOLP_LocationData target, notnull SCR_AIGroup group)
	{
		vector playerPos;
		if (!JOLP_WorldUtils.NearestLivingPlayerPosition(group.GetOrigin(), playerPos))
			return false;

		record.m_iSourceLocationId = target.m_iId;
		record.m_iTargetLocationId = target.m_iId;
		record.m_eRole = JOLP_MilicianGroupRole.SENTRY;
		record.m_bExemptFromProximityDespawn = false;
		record.m_bFreeReinforcement = false;
		record.m_bQRF = false;
		record.m_bCommittedFight = true;
		OrderGroupToPlayer(group, record, playerPos, false);
		Print(string.Format("[JOLP] Reinforcement at '%1' is attacking the nearest player", target.m_sName), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The player moved. Refresh the search so the squad does not sweep an empty spot.
	protected void UpdateCommittedFight(notnull JOLP_LiveGroupRecord record, notnull SCR_AIGroup group)
	{
		JOLP_LocationData home = JOLP_Global.GetLocations().GetLocation(record.m_iSourceLocationId);
		if (!home || !PlayersFightingAt(home))
			return;

		vector from = group.GetOrigin();
		if (record.m_eRole == JOLP_MilicianGroupRole.VEHICLE)
			from = QRFPosition(record, group);

		vector playerPos;
		if (!JOLP_WorldUtils.NearestLivingPlayerPosition(from, playerPos))
			return;

		if (record.m_bQRFHunting && JOLP_WorldUtils.DistanceFlat(record.m_vQRFHuntPosition, playerPos) < 40)
			return;

		bool dismount = record.m_eRole != JOLP_MilicianGroupRole.VEHICLE && QRFSquadMounted(record);
		OrderGroupToPlayer(group, record, playerPos, dismount);
	}

	//------------------------------------------------------------------------------------------------
	//! Search-and-destroy on the player. Dismount first when the squad is still in a truck.
	protected void OrderGroupToPlayer(notnull SCR_AIGroup group, notnull JOLP_LiveGroupRecord record, vector playerPos, bool dismount)
	{
		JOLP_MilicianGarrisonConfig config = GetConfig();
		if (config.m_rSearchAndDestroyWaypointPrefab.IsEmpty())
			return;

		vector search = SnapSurface(playerPos);
		JOLP_MilicianWaypointHelper.ClearGroupWaypoints(group, record);

		if (dismount && !config.m_rGetOutWaypointPrefab.IsEmpty())
			JOLP_MilicianWaypointHelper.AssignMoveTo(group, record, config.m_rGetOutWaypointPrefab, SnapSurface(group.GetOrigin()));

		float radius = 50;
		if (record.m_eRole == JOLP_MilicianGroupRole.VEHICLE)
			radius = 80;

		if (!JOLP_MilicianWaypointHelper.AssignSearchAndDestroy(group, record, config.m_rSearchAndDestroyWaypointPrefab, search, radius, 300000))
			return;

		record.m_bCommittedFight = true;
		record.m_bQRFHunting = true;
		record.m_vQRFHuntPosition = playerPos;
		record.m_fQRFHuntOrderedAt = GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	protected void AbsorbReinforcement(notnull JOLP_LiveGroupRecord record, notnull JOLP_LocationData target)
	{
		int alive = DespawnRecord(record, false);
		target.m_iGarrisonStored = target.m_iGarrisonStored + alive;

		string kind = "Reinforcement";
		if (record.m_bQRF)
			kind = "QRF";

		Print(string.Format("[JOLP] %1 absorbed into '%2' (+%3, stored %4)",
			kind, target.m_sName, alive, target.m_iGarrisonStored), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! First player hit on a settlement milician. Extra hits during the delay or cooldown do nothing.
	//! The instigator is still recorded, so a later shot moves the search.
	void NotifySettlementAttacked(int locationId, IEntity instigator = null)
	{
		if (!Replication.IsServer() || locationId < 0)
			return;

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		JOLP_LocationData data = locations.GetLocation(locationId);
		if (!data || !data.IsSettlement() || data.IsSuppressedByFOB())
			return;

		if (instigator)
			NotePlayerContact(data, instigator.GetOrigin());

		float now = GetGame().GetWorld().GetWorldTime();
		if (data.m_fQRFRequestWorldTime >= 0)
			return;

		if (now < data.m_fQRFCooldownUntilWorldTime)
			return;

		if (HasInboundReinforcement(locationId))
			return;

		data.m_fQRFRequestWorldTime = now;
		Print(string.Format("[JOLP] '%1' called for QRF", data.m_sName), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickQRFRequests()
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		float now = GetGame().GetWorld().GetWorldTime();
		JOLP_MilicianGarrisonConfig config = GetConfig();
		int delay = config.m_iQRFReactionDelayMs;
		if (delay < 0)
			delay = 30000;

		foreach (JOLP_LocationData data : locations.GetAllLocations())
		{
			if (!data || data.m_fQRFRequestWorldTime < 0)
				continue;

			if (now - data.m_fQRFRequestWorldTime < delay)
				continue;

			data.m_fQRFRequestWorldTime = -1;

			if (!data.IsSettlement() || data.IsSuppressedByFOB() || HasInboundReinforcement(data.m_iId))
			{
				data.m_fQRFCooldownUntilWorldTime = now + GetQRFCooldownMs(config);
				continue;
			}

			JOLP_LocationData donor = FindQRFDonor(data);
			if (!donor)
			{
				data.m_fQRFCooldownUntilWorldTime = now + GetQRFRetryCooldownMs(config);
				Print(string.Format("[JOLP] '%1' QRF found no donor", data.m_sName), LogLevel.NORMAL);
				continue;
			}

			int charged = SpawnRolePackage(donor, JOLP_MilicianGroupRole.REINFORCEMENT, data.m_iId, true);
			if (charged <= 0)
			{
				data.m_fQRFCooldownUntilWorldTime = now + GetQRFRetryCooldownMs(config);
				Print(string.Format("[JOLP] '%1' QRF failed to spawn from '%2'", data.m_sName, donor.m_sName), LogLevel.WARNING);
				continue;
			}

			data.m_fQRFCooldownUntilWorldTime = now + GetQRFCooldownMs(config);
			Print(string.Format("[JOLP] QRF from '%1' dispatched to '%2'", donor.m_sName, data.m_sName), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected int GetQRFCooldownMs(notnull JOLP_MilicianGarrisonConfig config)
	{
		if (config.m_iQRFCooldownMs <= 0)
			return 480000;

		return config.m_iQRFCooldownMs;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetQRFRetryCooldownMs(notnull JOLP_MilicianGarrisonConfig config)
	{
		if (config.m_iQRFRetryCooldownMs <= 0)
			return 60000;

		return config.m_iQRFRetryCooldownMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Closest town or OP that can spare a reinforcement squad.
	protected JOLP_LocationData FindQRFDonor(notnull JOLP_LocationData target)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return null;

		JOLP_MilicianGarrisonConfig config = GetConfig();
		float range = config.m_fQRFSearchRange;
		if (range <= 0)
			range = 2500;

		int cost = GetRoleCost(JOLP_MilicianGroupRole.REINFORCEMENT);
		int reserve = config.m_iQRFDefenseReserve;
		if (reserve < 0)
			reserve = 4;

		JOLP_LocationData best;
		float bestDist = float.MAX;

		foreach (JOLP_LocationData data : locations.GetAllLocations())
		{
			if (!data || data.m_iId == target.m_iId)
				continue;

			if (!data.IsMilicianEligible() || !data.m_bAllowReinforcement)
				continue;

			if (IsLocationInCombat(data.m_iId) || HasLiveReinforcement(data.m_iId))
				continue;

			if (data.m_iGarrisonStored - cost < reserve)
				continue;

			float dist = vector.Distance(target.m_vPosition, data.m_vPosition);
			if (dist > range || dist >= bestDist)
				continue;

			bestDist = dist;
			best = data;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Settlement a reinforcement or QRF has not yet reached, when that group is inside range.
	int FindNearestInboundSettlement(vector from, float range)
	{
		if (!m_aLiveGroups || range <= 0)
			return -1;

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return -1;

		int bestId = -1;
		float bestDist = 0;
		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!IsUnarrivedReinforcement(record))
				continue;

			JOLP_LocationData target = locations.GetLocation(record.m_iTargetLocationId);
			if (!target || !target.IsSettlement() || target.m_sName.IsEmpty())
				continue;

			vector position = LiveGroupPosition(record);
			if (position[0] == 0 && position[1] == 0 && position[2] == 0)
				continue;

			float dist = JOLP_WorldUtils.DistanceFlat(from, position);
			if (dist > range)
				continue;

			if (bestId >= 0 && dist >= bestDist)
				continue;

			bestDist = dist;
			bestId = target.m_iId;
		}

		return bestId;
	}

	//------------------------------------------------------------------------------------------------
	//! True when a reinforcement or QRF is still on the way to this place.
	bool HasUnarrivedInbound(int locationId)
	{
		if (!m_aLiveGroups || locationId < 0)
			return false;

		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!IsUnarrivedReinforcement(record))
				continue;

			if (record.m_iTargetLocationId == locationId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsUnarrivedReinforcement(JOLP_LiveGroupRecord record)
	{
		if (!record || record.m_bArrived)
			return false;

		if (record.m_eRole != JOLP_MilicianGroupRole.REINFORCEMENT || record.m_iTargetLocationId < 0)
			return false;

		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
		if (!group || JOLP_WorldUtils.CountLivingAgents(group) <= 0)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected vector LiveGroupPosition(notnull JOLP_LiveGroupRecord record)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
		if (group)
		{
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;

				IEntity body = agent.GetControlledEntity();
				if (body)
					return body.GetOrigin();
			}

			return group.GetOrigin();
		}

		IEntity vehicle = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
		if (vehicle)
			return vehicle.GetOrigin();

		return "0 0 0";
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasInboundReinforcement(int locationId)
	{
		if (!m_aLiveGroups)
			return false;

		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!record)
				continue;

			if (record.m_eRole == JOLP_MilicianGroupRole.REINFORCEMENT && record.m_iTargetLocationId == locationId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest operational hideouts each keep one free reinforcement group on the road.
	protected void TickHideoutReinforcements()
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		JOLP_MilicianGarrisonConfig config = GetConfig();
		int slots = 2;
		if (config && config.m_iHideoutActiveReinforcers > 0)
			slots = config.m_iHideoutActiveReinforcers;

		array<ref JOLP_LocationData> enabled = new array<ref JOLP_LocationData>();
		array<float> enabledDist = new array<float>();
		foreach (JOLP_LocationData data : locations.GetHideouts())
		{
			if (!data || data.m_bHideoutDestroyed)
				continue;

			float dist = JOLP_WorldUtils.NearestLivingPlayerDistance(data.m_vPosition);
			if (dist < 0)
				continue;

			InsertNearestHideout(enabled, enabledDist, data, dist, slots);
		}

		foreach (JOLP_LocationData data : enabled)
		{
			if (!data || HasLiveReinforcement(data.m_iId))
				continue;

			float searchRange = data.m_fHideoutReinforceSearchRange;
			if (searchRange <= 0)
				searchRange = config.m_fHideoutReinforceSearchRange;
			if (searchRange <= 0)
				searchRange = 2500;

			JOLP_LocationData target = locations.FindWeakestSettlement(data.m_vPosition, searchRange, data.m_iId);
			if (!target)
			{
				Print(string.Format("[JOLP] Hideout '%1' found no settlement to reinforce", data.m_sName), LogLevel.VERBOSE);
				continue;
			}

			JOLP_VehicleClass rolled = RollHideoutVehicle(data);
			int spawned = SpawnRolePackage(data, JOLP_MilicianGroupRole.REINFORCEMENT, target.m_iId, false, false, rolled, true);
			if (spawned > 0)
				Print(string.Format("[JOLP] Hideout '%1' dispatched reinforcement to '%2'", data.m_sName, target.m_sName), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void InsertNearestHideout(array<ref JOLP_LocationData> enabled, array<float> dists, JOLP_LocationData data, float dist, int slots)
	{
		int insertAt = enabled.Count();
		for (int i = 0; i < enabled.Count(); i++)
		{
			if (dist < dists[i])
			{
				insertAt = i;
				break;
			}
		}

		if (insertAt >= slots)
			return;

		enabled.InsertAt(data, insertAt);
		dists.InsertAt(dist, insertAt);

		while (enabled.Count() > slots)
		{
			int last = enabled.Count() - 1;
			enabled.Remove(last);
			dists.Remove(last);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! HQ destroyed — despawn local defenders and stop reinforce/regen for this hideout.
	void OnHideoutDestroyed(int locationId)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		JOLP_LocationData data = locations.GetLocation(locationId);
		if (!data)
			return;

		if (data.m_bActive)
			DeactivateLocation(data);

		DespawnHideoutLeftovers(data);

		Print(string.Format("[JOLP] Hideout '%1' cleared after HQ destruction", data.m_sName), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Groups still tied to a destroyed hideout. A squad the player is standing next to stays until he moves off.
	protected void DespawnHideoutLeftovers(notnull JOLP_LocationData data)
	{
		for (int i = m_aLiveGroups.Count() - 1; i >= 0; i--)
		{
			JOLP_LiveGroupRecord record = m_aLiveGroups[i];
			if (!record || record.m_iSourceLocationId != data.m_iId)
				continue;

			if (DespawnBlockedByPlayer(record))
				continue;

			DespawnRecord(record, false);
			m_aLiveGroups.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True when a milician agent at this location is shooting or has already been hit.
	bool IsLocationInCombat(int locationId)
	{
		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!record || record.m_iSourceLocationId != locationId)
				continue;

			if (record.m_bExemptFromProximityDespawn)
				continue;

			SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
			if (!group)
				continue;

			array<AIAgent> agents = {};
			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;

				IEntity body = agent.GetControlledEntity();
				if (!body)
					continue;

				SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(body.FindComponent(SCR_AICombatComponent));
				if (combat && combat.GetCurrentTarget())
					return true;

				SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(body.FindComponent(SCR_CharacterDamageManagerComponent));
				if (damage && !damage.IsDestroyed() && damage.GetHealthScaled() < 1)
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Stored pool is empty and no living package still belongs to this location, including reinforcements on the road.
	bool IsLocationGarrisonGone(int locationId)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return false;

		JOLP_LocationData data = locations.GetLocation(locationId);
		if (!data || data.m_iGarrisonStored > 0 || data.HasVehicleStock())
			return false;

		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!record || record.m_iSourceLocationId != locationId)
				continue;

			SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
			if (group && JOLP_WorldUtils.CountLivingAgents(group) > 0)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Stay on the perch. Idle speed so a contact does not send him walking to the ground navmesh.
	protected void HoldSniper(notnull SCR_AIGroup group)
	{
		SCR_AIGroupMovementComponent movement = SCR_AIGroupMovementComponent.Cast(group.FindComponent(SCR_AIGroupMovementComponent));
		if (movement)
			movement.SetGroupCharactersWantedMovementType(EMovementType.IDLE);

		SCR_AIGroupSettingsComponent settings = SCR_AIGroupSettingsComponent.Cast(group.FindComponent(SCR_AIGroupSettingsComponent));
		if (!settings)
			return;

		SCR_AIGroupCharactersMovementSpeedSetting setting = SCR_AIGroupCharactersMovementSpeedSetting.Create(SCR_EAISettingOrigin.EDITOR, EMovementType.IDLE);
		if (setting)
			settings.AddSetting(setting, true, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void HoldSniperBody(notnull IEntity body)
	{
		SCR_AIInfoComponent info = SCR_AIInfoComponent.Cast(body.FindComponent(SCR_AIInfoComponent));
		if (info)
			info.SetMovementType(EMovementType.IDLE);

		AICharacterMovementComponent movement = AICharacterMovementComponent.Cast(body.FindComponent(AICharacterMovementComponent));
		if (movement)
			movement.SetMovementTypeWanted(EMovementType.IDLE);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMilicianAgentAdded(AIAgent agent)
	{
		if (!agent)
			return;

		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!group)
			return;

		JOLP_LiveGroupRecord record = FindLiveRecord(group.GetID());
		if (!record)
			return;

		IEntity body = agent.GetControlledEntity();
		if (!body)
			return;

		JOLP_PersistenceManagerComponent.UntrackTransient(body);
		ApplyMilicianFaction(group, body);
		JOLP_AISkillConfig.Apply(body, GetSkillConfig().m_eMilicianSkill);
		if (record.m_eRole == JOLP_MilicianGroupRole.SNIPER)
			HoldSniperBody(body);
		TrySeatReinforcement(record);
		TrySeatNestGunner(record, body);

		JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
		if (civilians)
		{
			bool suppressQRF = record.m_eRole == JOLP_MilicianGroupRole.REINFORCEMENT;
			civilians.TrackMilician(body, record.m_iSourceLocationId, suppressQRF);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RegisterExistingMilicianAgents(notnull SCR_AIGroup group)
	{
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
			OnMilicianAgentAdded(agent);
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_LiveGroupRecord FindLiveRecord(EntityID groupId)
	{
		foreach (JOLP_LiveGroupRecord record : m_aLiveGroups)
		{
			if (!record)
				continue;

			if (record.m_GroupId == groupId || record.m_CrewGroupId == groupId)
				return record;
		}

		return null;
	}
}
