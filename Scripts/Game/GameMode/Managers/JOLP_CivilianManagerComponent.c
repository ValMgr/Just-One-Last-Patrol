class JOLP_CivilianManagerComponentClass : JOLP_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Server-side villagers, inter-town cars, and per-place reputation.
//! Lives on a Manager.layer world entity, same as the milician manager.
class JOLP_CivilianManagerComponent : JOLP_Component
{
	static JOLP_CivilianManagerComponent s_Instance;

	[Attribute(desc: "Civilian counts, attitude caps, and reputation (edit on Manager.layer entity)")]
	ref JOLP_CivilianConfig m_Config;

	[Attribute(desc: "Combat aim for partisans. Same file as the milician manager: Configs/AI/AISkill.conf")]
	ref JOLP_AISkillConfig m_SkillConfig;

	protected bool m_bTickRunning;
	protected int m_iPopulationPercent = 100;
	protected int m_iBlastMute;
	protected ref array<ref JOLP_CivilianLiveRecord> m_aLive;
	protected ref array<ref JOLP_TrackedAgent> m_aTracked;
	protected ref array<EntityID> m_aSuicideSpeakers;
	protected ref array<float> m_aSuicideSpeakerAt;
	//! Settlements already reported as having no road, so the tick does not repeat the warning.
	protected ref array<int> m_aNoRoadWarned;
	//! Move orders given to villagers we do not track, deleted when the call expires.
	protected ref array<EntityID> m_aGuestWaypointIds;
	protected ref array<float> m_aGuestWaypointUntil;
	protected static vector s_vGatherOrigin;
	protected static ref array<IEntity> s_aGather;

	//------------------------------------------------------------------------------------------------
	static JOLP_CivilianManagerComponent GetInstance()
	{
		if (s_Instance)
			return s_Instance;

		BaseGameMode gameMode = GetGame().GetGameMode();
		if (gameMode)
		{
			s_Instance = JOLP_CivilianManagerComponent.Cast(gameMode.FindComponent(JOLP_CivilianManagerComponent));
			if (s_Instance)
				return s_Instance;
		}

		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, FindCivilianManagerEntity, null, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool FindCivilianManagerEntity(IEntity entity)
	{
		if (!entity || s_Instance)
			return true;

		JOLP_CivilianManagerComponent component = JOLP_CivilianManagerComponent.Cast(
			entity.FindComponent(JOLP_CivilianManagerComponent));
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
		EnsureCollections();
	}

	//------------------------------------------------------------------------------------------------
	void Init(IEntity owner)
	{
		s_Instance = this;
		EnsureCollections();
		Print("[JOLP] CivilianManager Init", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	void PostGameStart()
	{
		if (!Replication.IsServer())
			return;

		if (m_bTickRunning)
			return;

		EnsureCollections();
		m_iPopulationPercent = JOLP_MissionHeader.GetCivilianPopulationPercent();

		JOLP_JustOneLastPatrolGameMode mode = JOLP_JustOneLastPatrolGameMode.Cast(GetGame().GetGameMode());
		if (!mode || !mode.WasRestoredFromSave())
			SeedLocalReputation();

		ResolveBudgets();

		int interval = 2000;
		if (m_Config && m_Config.m_iTickIntervalMs > 0)
			interval = m_Config.m_iTickIntervalMs;

		GetGame().GetCallqueue().CallLater(CivilianTick, interval, true);
		GetGame().GetCallqueue().CallLater(TickTerrorists, 500, true);
		m_bTickRunning = true;

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.GetOnPlayerKilled().Insert(OnPlayerKilled);

		float starting = GetConfig().m_fStartingReputation;
		Print(string.Format("[JOLP] CivilianManager PostGameStart: population %1 percent, local reputation start %2 (%3), low ceiling terrorist %4 partisan %5, tick %6 ms",
			m_iPopulationPercent,
			starting,
			typename.EnumToString(JOLP_ReputationStage, GetConfig().StageForReputation(starting)),
			GetConfig().m_fLowTerroristShare,
			GetConfig().m_fLowPartisanShare,
			interval), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	JOLP_CivilianConfig GetConfig()
	{
		if (!m_Config)
			m_Config = new JOLP_CivilianConfig();

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
	JOLP_ReputationStage GetStage(notnull JOLP_LocationData data)
	{
		return GetConfig().StageForReputation(data.m_fReputation);
	}

	//------------------------------------------------------------------------------------------------
	int GetPopulationPercent()
	{
		return m_iPopulationPercent;
	}

	//------------------------------------------------------------------------------------------------
	//! Living foot groups still out of this place's pool. Dead villagers are already spent.
	int GetFieldedFootCount(int locationId)
	{
		if (!m_aLive)
			return 0;

		int count = 0;
		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record || record.m_bDead)
				continue;

			if (record.m_eKind != JOLP_CivilianKind.FOOT || record.m_iSourceLocationId != locationId)
				continue;

			count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! 08:00 occupation pressure. Each stored man in a reputation place pulls the local score down.
	void ApplyDailyGarrisonDecay()
	{
		if (!Replication.IsServer())
			return;

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		float perMan = GetConfig().m_fReputationPerGarrisonMan;
		if (perMan == 0)
			return;

		foreach (JOLP_LocationData data : locations.GetAllLocations())
		{
			if (!data || !data.HasReputation() || data.m_iGarrisonStored <= 0)
				continue;

			float delta = -perMan * data.m_iGarrisonStored;
			AddReputationToPlace(data, delta, "garrison present");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A player character died. The place they died inside loses one point.
	void OnPlayerKilled(notnull SCR_InstigatorContextData data)
	{
		if (!Replication.IsServer())
			return;

		if (data.GetVictimPlayerID() <= 0)
			return;

		IEntity victim = data.GetVictimEntity();
		if (!victim)
			return;

		float delta = GetConfig().m_fReputationPlayerKilled;
		if (delta == 0)
			return;

		JOLP_LocationData place = FindReputationPlaceAt(victim.GetOrigin());
		if (!place)
			return;

		AddReputationToPlace(place, delta, "player killed");
	}

	//------------------------------------------------------------------------------------------------
	//! Safe wire cut. An OP charge credits the closest reputation place near the player.
	void NotifyIEDDefused(int locationId, vector chargePos, IEntity user)
	{
		if (!Replication.IsServer())
			return;

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		float delta = GetConfig().m_fReputationIEDDefused;
		JOLP_LocationData data = locations.GetLocation(locationId);
		if (data && data.HasReputation())
		{
			AddReputationToPlace(data, delta, "IED defused");
			return;
		}

		vector around = chargePos;
		if (user)
			around = user.GetOrigin();

		JOLP_LocationData closest = FindClosestReputationPlace(around, GetReputationRadius());
		if (closest)
			AddReputationToPlace(closest, delta, "IED defused");
	}

	//------------------------------------------------------------------------------------------------
	protected void SeedLocalReputation()
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		float start = GetConfig().m_fStartingReputation;
		foreach (JOLP_LocationData data : locations.GetAllLocations())
		{
			if (!data || !data.HasReputation() || data.m_bRestoredFromSave)
				continue;

			data.m_fReputation = start;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected float GetReputationRadius()
	{
		float radius = 500;
		if (m_Config && m_Config.m_fReputationRadius > 0)
			radius = m_Config.m_fReputationRadius;

		return radius;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddReputationToPlace(notnull JOLP_LocationData data, float delta, string reason)
	{
		if (!Replication.IsServer() || !data.HasReputation())
			return;

		JOLP_ReputationStage before = GetStage(data);
		data.m_fReputation = data.m_fReputation + delta;

		Print(string.Format("[JOLP] Reputation at '%1' %2 for %3 (%4 -> %5), now %6",
			data.m_sName,
			delta,
			reason,
			typename.EnumToString(JOLP_ReputationStage, before),
			typename.EnumToString(JOLP_ReputationStage, GetStage(data)),
			data.m_fReputation), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawner place, plus the closest reputation place inside the radius of a player instigator.
	protected void AddReputationNearby(int sourceLocationId, IEntity instigator, float delta, string reason)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		JOLP_LocationData source = locations.GetLocation(sourceLocationId);
		bool wroteSource = false;
		if (source && source.HasReputation())
		{
			AddReputationToPlace(source, delta, reason);
			wroteSource = true;
		}

		if (!instigator || !JOLP_CivilianOutfitter.IsPlayerInstigator(instigator))
			return;

		JOLP_LocationData closest = FindClosestReputationPlace(instigator.GetOrigin(), GetReputationRadius());
		if (!closest)
			return;

		if (wroteSource && source && closest.m_iId == source.m_iId)
			return;

		AddReputationToPlace(closest, delta, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! The reputation place whose area contains this point. Closest centre if several overlap.
	protected JOLP_LocationData FindReputationPlaceAt(vector pos)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return null;

		JOLP_LocationData best;
		float bestDist;
		foreach (JOLP_LocationData data : locations.GetAllLocations())
		{
			if (!data || !data.HasReputation())
				continue;

			float radius = data.m_fActivationRadius;
			if (radius < 1)
				continue;

			float dist = vector.Distance(pos, data.m_vPosition);
			if (dist > radius)
				continue;

			if (!best || dist < bestDist)
			{
				best = data;
				bestDist = dist;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! maxRadius below 0 means the nearest reputation place on the map.
	protected JOLP_LocationData FindClosestReputationPlace(vector pos, float maxRadius)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return null;

		bool unlimited = maxRadius < 0;
		JOLP_LocationData best;
		float bestDist = 0;
		foreach (JOLP_LocationData data : locations.GetAllLocations())
		{
			if (!data || !data.HasReputation())
				continue;

			float dist = vector.Distance(pos, data.m_vPosition);
			if (!unlimited && dist > maxRadius)
				continue;

			if (!best || dist < bestDist)
			{
				best = data;
				bestDist = dist;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TryApplyIEDCivilianKill(IEntity character, IEntity instigator)
	{
		JOLP_IEDManagerComponent ieds = JOLP_Global.GetIEDs();
		if (!ieds || !character)
			return false;

		int locationId;
		vector iedPos;
		if (!ieds.TryResolveIEDKill(character, instigator, locationId, iedPos))
			return false;

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return true;

		JOLP_LocationData data = locations.GetLocation(locationId);
		if (data && data.HasReputation())
		{
			AddReputationToPlace(data, GetConfig().m_fReputationDestroyed, "civilian killed by IED");
			return true;
		}

		JOLP_LocationData closest = FindClosestReputationPlace(iedPos, -1);
		if (closest)
			AddReputationToPlace(closest, GetConfig().m_fReputationDestroyed, "civilian killed by IED");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Called when a milician package is deleted on purpose. Those deaths must not raise reputation.
	void SuppressMilicianGroup(SCR_AIGroup group)
	{
		if (!group)
			return;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity body = agent.GetControlledEntity();
			if (body)
				SuppressCharacter(body.GetID());
		}
	}

	//------------------------------------------------------------------------------------------------
	void TrackMilician(IEntity character, int locationId, bool suppressQRF = false)
	{
		if (!character)
			return;

		JOLP_TrackedAgent tracked = FindTracked(character.GetID());
		if (!tracked)
		{
			tracked = new JOLP_TrackedAgent();
			tracked.m_EntityId = character.GetID();
			m_aTracked.Insert(tracked);
		}

		tracked.m_bMilician = true;
		tracked.m_iLocationId = locationId;
		tracked.m_bSuppressQRF = suppressQRF;
		tracked.m_bSuppressReputation = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void NotifyMilicianAttacked(int locationId, IEntity instigator)
	{
		JOLP_MilicianManagerComponent milician = JOLP_Global.GetMilician();
		if (milician)
			milician.NotifySettlementAttacked(locationId, instigator);
	}

	//------------------------------------------------------------------------------------------------
	void OnCharacterDamaged(IEntity character, IEntity instigator)
	{
		if (!Replication.IsServer() || !character)
			return;

		JOLP_TrackedAgent tracked = FindTracked(character.GetID());
		if (m_iBlastMute > 0)
		{
			if (tracked)
				tracked.m_bSuppressReputation = true;
			return;
		}

		if (!tracked || tracked.m_bVehicle)
			return;

		if (tracked.m_bMilician)
		{
			if (!tracked.m_bSuppressQRF && JOLP_CivilianOutfitter.IsPlayerInstigator(instigator))
				NotifyMilicianAttacked(tracked.m_iLocationId, instigator);
			return;
		}

		if (tracked.m_bSuppressReputation || tracked.m_bInjuredApplied || tracked.m_bDestroyedApplied)
			return;

		if (tracked.m_eAttitude == JOLP_CivilianAttitude.PARTISAN || tracked.m_eAttitude == JOLP_CivilianAttitude.TERRORIST)
			return;

		if (!JOLP_CivilianOutfitter.IsPlayerInstigator(instigator))
			return;

		DamageManagerComponent damage = DamageManagerComponent.Cast(character.FindComponent(DamageManagerComponent));
		if (damage && damage.IsDestroyed())
			return;

		tracked.m_bInjuredApplied = true;
		AddReputationNearby(tracked.m_iLocationId, instigator, GetConfig().m_fReputationDamaged, "civilian injured");
	}

	//------------------------------------------------------------------------------------------------
	void OnCharacterDestroyed(IEntity character, IEntity instigator)
	{
		if (!Replication.IsServer() || !character)
			return;

		JOLP_TrackedAgent tracked = FindTracked(character.GetID());
		if (m_iBlastMute > 0)
		{
			if (tracked && !tracked.m_bDestroyedApplied)
			{
				tracked.m_bSuppressReputation = true;
				tracked.m_bDestroyedApplied = true;
				if (!tracked.m_bMilician)
					MarkRecordDead(character.GetID());
			}
			return;
		}

		if (!tracked || tracked.m_bSuppressReputation || tracked.m_bDestroyedApplied)
			return;

		tracked.m_bDestroyedApplied = true;
		MarkRecordDead(character.GetID());

		if (tracked.m_bMilician)
		{
			if (!tracked.m_bSuppressQRF && JOLP_CivilianOutfitter.IsPlayerInstigator(instigator))
				NotifyMilicianAttacked(tracked.m_iLocationId, instigator);

			AddReputationNearby(tracked.m_iLocationId, instigator, GetConfig().m_fReputationPerMilicianKill, "milician killed");
			return;
		}

		if (tracked.m_eAttitude == JOLP_CivilianAttitude.PARTISAN || tracked.m_eAttitude == JOLP_CivilianAttitude.TERRORIST)
			return;

		if (JOLP_CivilianOutfitter.IsPlayerInstigator(instigator))
		{
			AddReputationNearby(tracked.m_iLocationId, instigator, GetConfig().m_fReputationDestroyed, "civilian killed");
			RadicalizeNeutrals(tracked.m_iLocationId);
			return;
		}

		TryApplyIEDCivilianKill(character, instigator);
	}

	//------------------------------------------------------------------------------------------------
	void OnVehicleDamaged(IEntity vehicle, IEntity instigator, bool destroyed)
	{
		if (!Replication.IsServer() || !vehicle || !destroyed)
			return;

		JOLP_TrackedAgent tracked = FindTracked(vehicle.GetID());
		if (!tracked || !tracked.m_bVehicle || tracked.m_bSuppressReputation || tracked.m_bDestroyedApplied)
			return;

		tracked.m_bDestroyedApplied = true;
		DropVehicleRecord(vehicle.GetID());
	}

	//------------------------------------------------------------------------------------------------
	protected void EnsureCollections()
	{
		if (!m_Config)
			m_Config = new JOLP_CivilianConfig();
		if (!m_aLive)
			m_aLive = new array<ref JOLP_CivilianLiveRecord>();
		if (!m_aTracked)
			m_aTracked = new array<ref JOLP_TrackedAgent>();
		if (!m_aSuicideSpeakers)
			m_aSuicideSpeakers = new array<EntityID>();
		if (!m_aSuicideSpeakerAt)
			m_aSuicideSpeakerAt = new array<float>();
	}

	//------------------------------------------------------------------------------------------------
	protected void ResolveBudgets()
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		foreach (JOLP_LocationData data : locations.GetAllLocations())
			ResolveLocation(data);
	}

	//------------------------------------------------------------------------------------------------
	protected void ResolveLocation(JOLP_LocationData data)
	{
		if (!data || data.m_iCivilianBudget >= 0)
			return;

		if (!data.IsTown())
		{
			data.m_iCivilianBudget = 0;
			data.m_iCivilianStored = 0;
			data.m_iVehicleSlots = 0;
			return;
		}

		JOLP_CivilianConfig config = GetConfig();
		int foot = config.FootBudgetForSize(data.m_eSettlementSize);
		int cars = config.VehicleSlotsForSize(data.m_eSettlementSize);
		if (data.m_iCivilianFootOverride >= 0)
			foot = data.m_iCivilianFootOverride;
		if (data.m_iCivilianVehicleOverride >= 0)
			cars = data.m_iCivilianVehicleOverride;

		data.m_iCivilianBudget = JOLP_CivilianConfig.ScaleCount(foot, m_iPopulationPercent);
		data.m_iCivilianStored = data.m_iCivilianBudget;
		data.m_iVehicleSlots = JOLP_CivilianConfig.ScaleCount(cars, m_iPopulationPercent);

		Print(string.Format("[JOLP] Civilians at '%1' (%2): foot %3, cars %4",
			data.m_sName,
			typename.EnumToString(JOLP_SettlementSize, data.m_eSettlementSize),
			data.m_iCivilianBudget,
			data.m_iVehicleSlots), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void CivilianTick()
	{
		if (!Replication.IsServer())
			return;

		UpdateLive();
		EnsureTalkProxies();
		UpdatePartisans();
		ReleaseSummons();

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		float despawnMultiplier = 1.15;
		if (m_Config && m_Config.m_fDespawnMultiplier > 1.0)
			despawnMultiplier = m_Config.m_fDespawnMultiplier;

		foreach (JOLP_LocationData data : locations.GetAllLocations())
		{
			if (!data || !data.IsTown())
				continue;

			ResolveLocation(data);
			if (data.m_iCivilianBudget <= 0 && data.m_iVehicleSlots <= 0)
				continue;

			if (data.IsSuppressedByFOB())
			{
				if (data.m_bCivilianActive)
					DeactivateLocation(data);

				DespawnVehiclesTouching(data.m_iId);
				continue;
			}

			if (!data.m_bCivilianActive)
			{
				if (JOLP_WorldUtils.PlayerInRange(data.m_vPosition, data.m_fActivationRadius))
					ActivateLocation(data);
			}
			else
			{
				float despawnRadius = data.m_fActivationRadius * despawnMultiplier;
				if (JOLP_WorldUtils.AllPlayersBeyondRange(data.m_vPosition, despawnRadius))
					DeactivateLocation(data);
				else
					RefillLocation(data);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ActivateLocation(notnull JOLP_LocationData data)
	{
		data.m_bCivilianActive = true;
		RollAttitudeMix(data);

		int terroristCap;
		int partisanCap;
		AttitudeCaps(data, terroristCap, partisanCap);
		Print(string.Format("[JOLP] Villagers waking '%1': budget %2 on foot, %3 cars, %4 still in the pool, this visit %5 terrorists and %6 partisans",
			data.m_sName,
			data.m_iCivilianBudget,
			data.m_iVehicleSlots,
			data.m_iCivilianStored,
			terroristCap,
			partisanCap), LogLevel.NORMAL);
		RefillLocation(data);
	}

	//------------------------------------------------------------------------------------------------
	protected void DeactivateLocation(notnull JOLP_LocationData data)
	{
		int returned = 0;
		for (int i = m_aLive.Count() - 1; i >= 0; i--)
		{
			JOLP_CivilianLiveRecord record = m_aLive[i];
			if (!record || record.m_iSourceLocationId != data.m_iId)
				continue;

			if (record.m_eKind == JOLP_CivilianKind.VEHICLE)
				continue;

			bool refund = !record.m_bDead;
			if (refund)
				returned++;

			DespawnRecord(record, refund);
			m_aLive.Remove(i);
		}

		data.m_bCivilianActive = false;
		ClearAttitudeMix(data);
		Print(string.Format("[JOLP] Villagers left '%1': returned %2 on foot (pool %3 / budget %4)",
			data.m_sName,
			returned,
			data.m_iCivilianStored,
			data.m_iCivilianBudget), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Cars leaving this town, or driving toward it, are removed while a player FOB covers it.
	protected void DespawnVehiclesTouching(int locationId)
	{
		for (int i = m_aLive.Count() - 1; i >= 0; i--)
		{
			JOLP_CivilianLiveRecord record = m_aLive[i];
			if (!record || record.m_eKind != JOLP_CivilianKind.VEHICLE)
				continue;

			if (record.m_iSourceLocationId != locationId && record.m_iTargetLocationId != locationId)
				continue;

			DespawnRecord(record, false);
			m_aLive.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RefillLocation(notnull JOLP_LocationData data)
	{
		int budget = 3;
		if (m_Config && m_Config.m_iMaxFootSpawnsPerTick > 0)
			budget = m_Config.m_iMaxFootSpawnsPerTick;

		int spawned = 0;
		while (spawned < budget && data.m_iCivilianStored > 0)
		{
			if (!SpawnFoot(data))
				break;

			spawned++;
		}

		int carsBefore = CountLiveVehicles(data.m_iId);
		if (carsBefore < data.m_iVehicleSlots)
			SpawnVehicle(data);

		if (spawned > 0 || CountLiveVehicles(data.m_iId) != carsBefore)
		{
			LogVillagePopulation(data, spawned);
			GetGame().GetCallqueue().CallLater(LogVillageBodies, 1000, false, data.m_iId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool SpawnFoot(notnull JOLP_LocationData data)
	{
		if (data.m_iCivilianStored <= 0)
			return false;

		JOLP_CivilianAttitude attitude = PickAttitude(data);
		vector spawnPos = PickFootSpawn(data);
		if (!CreateFootGroup(data, attitude, spawnPos))
			return false;

		data.m_iCivilianStored = data.m_iCivilianStored - 1;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A dry point inside the town, away from the player and from civilians already standing here.
	//! The engine's group queue used to drop every member on the navmesh in front of the player.
	protected vector PickFootSpawn(notnull JOLP_LocationData data)
	{
		float radius = data.m_fActivationRadius;
		if (radius < 40)
			radius = 40;

		array<vector> occupied = CollectFootOrigins(data.m_iId);
		float minSeparation = 16;
		float playerClearance = 50;
		if (radius >= 100)
			playerClearance = 70;

		BaseWorld world = GetGame().GetWorld();
		vector best = data.m_vPosition;
		if (world)
			best[1] = world.GetSurfaceY(best[0], best[2]);

		float bestScore = -1;
		bool haveBest = false;

		for (int i = 0; i < 16; i++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float dist = radius * Math.RandomFloat(0.28, 0.92);
			vector pos = data.m_vPosition + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);
			if (world)
				pos[1] = world.GetSurfaceY(pos[0], pos[2]);

			if (world)
			{
				float oceanY = world.GetOceanHeight(pos[0], pos[2]);
				if (pos[1] < oceanY + 0.6)
					continue;
			}

			float fromPlayers = JOLP_WorldUtils.NearestLivingPlayerDistance(pos);
			if (fromPlayers < 0)
				fromPlayers = 100000;

			float clearance = JOLP_WorldUtils.Clearance(pos, occupied);
			float score = fromPlayers;
			if (clearance < minSeparation)
				score = fromPlayers * 0.15;

			if (!haveBest || score > bestScore)
			{
				bestScore = score;
				best = pos;
				haveBest = true;
			}

			if (fromPlayers < playerClearance)
				continue;

			if (clearance < minSeparation)
				continue;

			return pos;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected array<vector> CollectFootOrigins(int locationId)
	{
		array<vector> origins = {};
		if (!m_aLive)
			return origins;

		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record || record.m_bDead)
				continue;

			if (record.m_eKind != JOLP_CivilianKind.FOOT || record.m_iSourceLocationId != locationId)
				continue;

			IEntity body = FirstCharacter(record);
			if (body)
			{
				origins.Insert(body.GetOrigin());
				continue;
			}

			IEntity group = GetGame().GetWorld().FindEntityByID(record.m_GroupId);
			if (group)
				origins.Insert(group.GetOrigin());
		}

		return origins;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CreateFootGroup(notnull JOLP_LocationData data, JOLP_CivilianAttitude attitude, vector spawnPos)
	{
		JOLP_CivilianConfig config = GetConfig();
		ResourceName prefab = config.m_rFootGroupPrefab;
		if (attitude == JOLP_CivilianAttitude.PARTISAN && !config.m_rPartisanGroupPrefab.IsEmpty())
			prefab = config.m_rPartisanGroupPrefab;

		if (prefab.IsEmpty())
		{
			Print(string.Format("[JOLP] No civilian group prefab, '%1' spawned nobody", data.m_sName), LogLevel.WARNING);
			return false;
		}

		float yaw = Math.RandomFloat(-180, 180);
		vector angles = JOLP_WorldUtils.UprightYaw(yaw);
		SCR_AIGroup group = SpawnEmptyGroup(prefab, spawnPos, angles);
		if (!group && attitude == JOLP_CivilianAttitude.PARTISAN && !config.m_rFootGroupPrefab.IsEmpty())
		{
			Print(string.Format("[JOLP] Partisan prefab failed at '%1', using the civilian group", data.m_sName), LogLevel.WARNING);
			prefab = config.m_rFootGroupPrefab;
			group = SpawnEmptyGroup(prefab, spawnPos, angles);
		}

		if (!group)
		{
			Print(string.Format("[JOLP] Failed to spawn civilian at '%1'", data.m_sName), LogLevel.WARNING);
			return false;
		}

		group.SetDeleteWhenEmpty(false);
		JOLP_CivilianOutfitter.SetGroupFaction(group, config.m_sCivilianFactionKey);

		if (attitude == JOLP_CivilianAttitude.TERRORIST)
			Print(string.Format("[JOLP] Spawned a suicide bomber at '%1'", data.m_sName), LogLevel.NORMAL);

		JOLP_CivilianLiveRecord record = new JOLP_CivilianLiveRecord();
		record.m_eKind = JOLP_CivilianKind.FOOT;
		record.m_eAttitude = attitude;
		record.m_GroupId = group.GetID();
		record.m_iSourceLocationId = data.m_iId;
		record.m_fSpawnedAtWorldTime = GetGame().GetWorld().GetWorldTime();
		m_aLive.Insert(record);

		group.GetOnAgentAdded().Insert(OnAgentAdded);
		if (!AttachMember(group, prefab, spawnPos, angles))
		{
			Print(string.Format("[JOLP] Civilian body failed at '%1'", data.m_sName), LogLevel.WARNING);
			m_aLive.RemoveItem(record);
			JOLP_WorldUtils.DeleteEntitySafe(group);
			return false;
		}

		CatchExistingAgents(group);

		float patrolRadius = Math.RandomFloat(22, 48);
		int points = Math.RandomInt(3, 5);
		float startAngle = Math.RandomFloat(0, Math.PI2);
		JOLP_CivilianWaypointHelper.AssignCyclePatrol(group, record, config.m_rPatrolWaypointPrefab, config.m_rCycleWaypointPrefab, spawnPos, patrolRadius, points, startAngle);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Empty group. Members are placed by AttachMember, not by the engine queue.
	protected SCR_AIGroup SpawnEmptyGroup(ResourceName prefab, vector spawnPos, vector angles)
	{
		SCR_AIGroup.IgnoreSpawning(true);
		IEntity groupEnt = JOLP_WorldUtils.SpawnEntityPrefab(prefab, spawnPos, angles, true);
		SCR_AIGroup.IgnoreSpawning(false);

		SCR_AIGroup group = SCR_AIGroup.Cast(groupEnt);
		if (!group && groupEnt)
			JOLP_WorldUtils.DeleteEntitySafe(groupEnt);

		return group;
	}

	//------------------------------------------------------------------------------------------------
	//! The character prefab from the group, stood on the chosen point.
	protected bool AttachMember(notnull SCR_AIGroup group, ResourceName groupPrefab, vector position, vector angles)
	{
		array<ResourceName> slots = {};
		ReadGroupMemberPrefabs(groupPrefab, slots);

		ResourceName characterPrefab;
		if (!slots.IsEmpty())
			characterPrefab = slots[0];
		else
			characterPrefab = "{664B13AF00000160}Prefabs/Characters/JOLP_Civilian.et";

		characterPrefab = RandomDressedPrefab(characterPrefab);

		IEntity body = JOLP_WorldUtils.SpawnEntityPrefab(characterPrefab, position, angles, true);
		if (!body)
			return false;

		if (!group.AddAIEntityToGroup(body))
		{
			JOLP_WorldUtils.DeleteEntitySafe(body);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One of the prefab's authored outfits, already dressed. Swapping clothes after spawn drops the old set.
	protected ResourceName RandomDressedPrefab(ResourceName prefab)
	{
		array<ResourceName> variants = {};
		CollectBodyVariants(prefab, variants);
		if (variants.IsEmpty())
			return prefab;

		return variants.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	//! Variant list on this prefab or the first ancestor that has one. That is how Character_MEC_Randomized dresses.
	protected void CollectBodyVariants(ResourceName prefab, notnull array<ResourceName> variants)
	{
		ResourceName current = prefab;
		for (int depth = 0; depth < 8 && !current.IsEmpty(); depth++)
		{
			Resource holder = Resource.Load(current);
			if (!holder || !holder.IsValid())
				return;

			BaseResourceObject resourceObject = holder.GetResource();
			if (!resourceObject)
				return;

			IEntitySource source = resourceObject.ToEntitySource();
			if (!source)
				return;

			if (ReadBodyVariants(source, variants))
				return;

			BaseContainer ancestor = source.GetAncestor();
			if (!ancestor)
				return;

			ResourceName parent = ancestor.GetResourceName();
			if (parent.IsEmpty() || parent == current)
				return;

			current = parent;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool ReadBodyVariants(notnull IEntitySource source, notnull array<ResourceName> variants)
	{
		for (int i = 0; i < source.GetComponentCount(); i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (!component)
				continue;

			if (component.GetClassName() != "SCR_EditableCharacterComponent")
				continue;

			BaseContainer variantData = component.GetObject("m_VariantData");
			if (!variantData)
				continue;

			BaseContainerList list = variantData.GetObjectArray("m_aVariants");
			if (!list)
				continue;

			for (int v = 0; v < list.Count(); v++)
			{
				BaseContainer entry = list.Get(v);
				if (!entry)
					continue;

				bool enabled = true;
				entry.Get("m_bEnabled", enabled);
				if (!enabled)
					continue;

				ResourceName variant;
				if (!entry.Get("m_sVariantPrefab", variant) || variant.IsEmpty())
					continue;

				variants.Insert(variant);
			}
		}

		return !variants.IsEmpty();
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
	protected bool SpawnVehicle(notnull JOLP_LocationData data)
	{
		JOLP_LocationData target = FindVehicleDestination(data);
		if (!target)
			return false;

		JOLP_CivilianConfig config = GetConfig();
		ResourceName vehiclePrefab = config.RandomVehiclePrefab();
		if (vehiclePrefab.IsEmpty() || config.m_rFootGroupPrefab.IsEmpty())
			return false;

		float reach = data.m_fActivationRadius;
		if (reach < 80)
			reach = 80;

		array<vector> occupied = CollectNearbyVehicleOrigins(data.m_vPosition, reach);
		float minSeparation = 16;

		vector spawnPos;
		vector angles;
		if (!JOLP_WorldUtils.FindVehicleRoadSpawn(data.m_vPosition, reach, occupied, minSeparation, spawnPos, angles))
		{
			vector unusedPos;
			vector unusedAngles;
			if (!JOLP_WorldUtils.FindNearestRoadSpawn(data.m_vPosition, reach, unusedPos, unusedAngles))
				WarnNoRoad(data);

			return false;
		}

		vector destination = RoadDestination(target);
		JOLP_WorldUtils.FaceYawToward(angles, spawnPos, destination);

		IEntity vehicle = JOLP_WorldUtils.SpawnEntityPrefab(vehiclePrefab, spawnPos, angles, true);
		if (!vehicle)
			return false;

		JOLP_WorldUtils.SettleSpawnedVehicle(vehicle);

		// Behind the car, on the carriageway. A point beside the road is often inside a house, and the same spot as the hull launches the car.
		vector groupPos = JOLP_WorldUtils.CrewStandPosition(spawnPos, angles, 8);

		SCR_AIGroup group = SpawnEmptyGroup(config.m_rFootGroupPrefab, groupPos, angles);
		if (!group)
		{
			JOLP_WorldUtils.DeleteEntitySafe(vehicle);
			return false;
		}

		group.SetDeleteWhenEmpty(false);
		JOLP_CivilianOutfitter.SetGroupFaction(group, config.m_sCivilianFactionKey);

		JOLP_CivilianLiveRecord record = new JOLP_CivilianLiveRecord();
		record.m_eKind = JOLP_CivilianKind.VEHICLE;
		record.m_eAttitude = JOLP_CivilianAttitude.NEUTRAL;
		record.m_GroupId = group.GetID();
		record.m_VehicleId = vehicle.GetID();
		record.m_iSourceLocationId = data.m_iId;
		record.m_iTargetLocationId = target.m_iId;
		record.m_fSpawnedAtWorldTime = GetGame().GetWorld().GetWorldTime();
		m_aLive.Insert(record);

		group.GetOnAgentAdded().Insert(OnAgentAdded);
		if (!AttachMember(group, config.m_rFootGroupPrefab, groupPos, angles))
		{
			Print(string.Format("[JOLP] Civilian driver failed at '%1'", data.m_sName), LogLevel.WARNING);
			m_aLive.RemoveItem(record);
			JOLP_WorldUtils.DeleteEntitySafe(group);
			JOLP_WorldUtils.DeleteEntitySafe(vehicle);
			return false;
		}

		TrackVehicle(vehicle, data.m_iId);

		CatchExistingAgents(group);
		JOLP_CivilianWaypointHelper.AssignMoveTo(group, record, config.m_rMoveWaypointPrefab, destination);

		Print(string.Format("[JOLP] Civilian car left '%1' for '%2'", data.m_sName, target.m_sName), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Cars already on the road near this settlement, civilian and milician.
	protected array<vector> CollectNearbyVehicleOrigins(vector center, float radius)
	{
		array<vector> origins = {};
		float keep = radius + 20;
		AppendVehicleOriginsNear(center, keep, origins);

		JOLP_MilicianManagerComponent milician = JOLP_Global.GetMilician();
		if (milician)
			milician.AppendVehicleOriginsNear(center, keep, origins);

		return origins;
	}

	//------------------------------------------------------------------------------------------------
	//! Living civilian vehicles near a point. Milician traffic uses this so the two fleets do not spawn on top of each other.
	void AppendVehicleOriginsNear(vector center, float radius, notnull array<vector> origins)
	{
		if (!m_aLive)
			return;

		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record || record.m_eKind != JOLP_CivilianKind.VEHICLE || !record.m_VehicleId)
				continue;

			IEntity vehicle = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
			if (!vehicle)
				continue;

			if (vector.Distance(vehicle.GetOrigin(), center) <= radius)
				origins.Insert(vehicle.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A road inside the destination town. The move order ends on that road so the car can stay on the network.
	protected vector RoadDestination(notnull JOLP_LocationData target)
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

		return target.m_vPosition;
	}

	//------------------------------------------------------------------------------------------------
	protected void WarnNoRoad(notnull JOLP_LocationData data)
	{
		if (!m_aNoRoadWarned)
			m_aNoRoadWarned = new array<int>();

		if (m_aNoRoadWarned.Find(data.m_iId) >= 0)
			return;

		m_aNoRoadWarned.Insert(data.m_iId);
		Print(string.Format("[JOLP] '%1' has no road within range — civilian cars will not spawn", data.m_sName), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAgentAdded(AIAgent agent)
	{
		if (!agent)
			return;

		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!group)
			return;

		JOLP_CivilianLiveRecord record = FindRecordByGroup(group.GetID());
		if (!record)
			return;

		record.m_bSeenAgent = true;
		IEntity character = agent.GetControlledEntity();
		JOLP_PersistenceManagerComponent.UntrackTransient(character);
		if (!character)
		{
			if (record.m_eAttitude == JOLP_CivilianAttitude.TERRORIST)
				Print("[JOLP] Terrorist agent has no body yet", LogLevel.WARNING);
			return;
		}

		if (!record.m_aCharacterIds)
			record.m_aCharacterIds = new array<EntityID>();

		if (record.m_aCharacterIds.Find(character.GetID()) >= 0)
			return;

		record.m_aCharacterIds.Insert(character.GetID());

		JOLP_CivilianOutfitter.Prepare(character, record, GetConfig());
		JOLP_CivilianTalkAttach.Queue(character);
		if (record.m_eAttitude == JOLP_CivilianAttitude.PARTISAN)
			JOLP_AISkillConfig.Apply(character, GetSkillConfig().m_ePartisanSkill);
		TrackCivilian(character, record);

		if (record.m_eKind == JOLP_CivilianKind.VEHICLE)
			TrySeat(record);
	}

	//------------------------------------------------------------------------------------------------
	protected void TryStrapBomber(notnull JOLP_CivilianLiveRecord record, IEntity character)
	{
		if (record.m_bChargesAttached)
			return;

		if (!character)
		{
			Print("[JOLP] Terrorist has no body to wear the bomb vest", LogLevel.WARNING);
			return;
		}

		ResourceName prefab = GetConfig().m_rBomberChargePrefab;
		Print(string.Format("[JOLP] Equipping bomb vest '%1'", prefab), LogLevel.NORMAL);
		if (JOLP_CivilianOutfitter.WearPrefab(character, prefab))
		{
			record.m_bChargesAttached = true;
			PlaySuicideSound(character.GetOrigin());
			Print("[JOLP] Bomb vest equipped", LogLevel.NORMAL);
		}
		else
		{
			Print("[JOLP] Bomb vest failed to equip", LogLevel.WARNING);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity BodyFromGroup(SCR_AIGroup group)
	{
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
	protected void CatchExistingAgents(notnull SCR_AIGroup group)
	{
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
			OnAgentAdded(agent);
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_CivilianAttitude PickAttitude(notnull JOLP_LocationData data)
	{
		int budget = data.m_iCivilianBudget;
		if (budget < 1)
			return JOLP_CivilianAttitude.NEUTRAL;

		int terroristCap;
		int partisanCap;
		AttitudeCaps(data, terroristCap, partisanCap);

		if (CountLiveAttitude(data.m_iId, JOLP_CivilianAttitude.TERRORIST) < terroristCap)
			return JOLP_CivilianAttitude.TERRORIST;

		if (CountLiveAttitude(data.m_iId, JOLP_CivilianAttitude.PARTISAN) < partisanCap)
			return JOLP_CivilianAttitude.PARTISAN;

		return JOLP_CivilianAttitude.NEUTRAL;
	}

	//------------------------------------------------------------------------------------------------
	//! Config shares are ceilings. This visit rolls a count from none up to that ceiling.
	protected void AttitudeCaps(notnull JOLP_LocationData data, out int terroristCap, out int partisanCap)
	{
		int budget = data.m_iCivilianBudget;
		float terroristShare;
		float partisanShare;
		JOLP_CivilianConfig config = GetConfig();
		JOLP_ReputationStage stage = GetStage(data);
		config.GetAttitudeShares(stage, terroristShare, partisanShare);

		if (stage == JOLP_ReputationStage.LOW)
		{
			if (terroristShare <= 0)
				terroristShare = 0.1;
			if (partisanShare <= 0)
				partisanShare = 0.4;
		}

		if (data.m_fTerroristRoll < 0 || data.m_fPartisanRoll < 0)
			RollAttitudeMix(data);

		terroristCap = ScaledCap(CountForShare(budget, terroristShare), data.m_fTerroristRoll);
		partisanCap = ScaledCap(CountForShare(budget, partisanShare), data.m_fPartisanRoll);
		if (terroristCap + partisanCap > budget)
			partisanCap = budget - terroristCap;
		if (partisanCap < 0)
			partisanCap = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void RollAttitudeMix(notnull JOLP_LocationData data)
	{
		data.m_fTerroristRoll = Math.RandomFloat01();
		data.m_fPartisanRoll = Math.RandomFloat01();
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearAttitudeMix(notnull JOLP_LocationData data)
	{
		data.m_fTerroristRoll = -1;
		data.m_fPartisanRoll = -1;
	}

	//------------------------------------------------------------------------------------------------
	protected int ScaledCap(int maxCount, float roll)
	{
		if (maxCount <= 0 || roll <= 0)
			return 0;

		int count = Math.Round(maxCount * roll);
		if (count < 0)
			return 0;
		if (count > maxCount)
			return maxCount;

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountForShare(int budget, float share)
	{
		if (budget <= 0 || share <= 0)
			return 0;

		float exact = budget * share;
		int count = Math.Round(exact + 0.499);
		if (count < 1)
			count = 1;
		if (count > budget)
			count = budget;

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! A player kill turns part of the remaining neutrals in that settlement into partisans.
	protected void RadicalizeNeutrals(int locationId)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		JOLP_LocationData data = locations.GetLocation(locationId);
		if (!data || !data.IsTown())
			return;

		array<JOLP_CivilianLiveRecord> neutrals = new array<JOLP_CivilianLiveRecord>();
		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record || record.m_bDead)
				continue;

			if (record.m_eKind != JOLP_CivilianKind.FOOT || record.m_iSourceLocationId != locationId)
				continue;

			if (record.m_eAttitude != JOLP_CivilianAttitude.NEUTRAL)
				continue;

			neutrals.Insert(record);
		}

		if (neutrals.IsEmpty())
			return;

		float share = 0.25;
		if (m_Config && m_Config.m_fKillRadicalizeShare > 0)
			share = m_Config.m_fKillRadicalizeShare;

		int convert = Math.Round((neutrals.Count() * share) + 0.499);
		if (convert < 1)
			convert = 1;
		if (convert > neutrals.Count())
			convert = neutrals.Count();

		for (int i = 0; i < convert; i++)
		{
			int index = Math.RandomInt(0, neutrals.Count());
			ConvertNeutralToPartisan(neutrals[index], data);
			neutrals.Remove(index);
		}

		Print(string.Format("[JOLP] '%1': a civilian was killed, %2 neutrals became partisans", data.m_sName, convert), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void ConvertNeutralToPartisan(JOLP_CivilianLiveRecord record, notnull JOLP_LocationData data)
	{
		if (!record)
			return;

		vector pos = data.m_vPosition;
		IEntity groupEnt = GetGame().GetWorld().FindEntityByID(record.m_GroupId);
		if (groupEnt)
			pos = groupEnt.GetOrigin();

		RemoveLiveRecord(record);
		DespawnRecord(record, false);

		if (!CreateFootGroup(data, JOLP_CivilianAttitude.PARTISAN, pos))
			data.m_iCivilianStored = data.m_iCivilianStored + 1;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveLiveRecord(notnull JOLP_CivilianLiveRecord record)
	{
		for (int i = m_aLive.Count() - 1; i >= 0; i--)
		{
			JOLP_CivilianLiveRecord live = m_aLive[i];
			if (live && live.m_GroupId == record.m_GroupId)
			{
				m_aLive.Remove(i);
				return;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdatePartisans()
	{
		JOLP_MilicianManagerComponent milician = JOLP_Global.GetMilician();
		if (!milician)
			return;

		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record || record.m_bDead || record.m_bHostile)
				continue;

			if (record.m_eAttitude != JOLP_CivilianAttitude.PARTISAN)
				continue;

			if (!milician.IsLocationInCombat(record.m_iSourceLocationId))
				continue;

			FlipPartisan(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void FlipPartisan(notnull JOLP_CivilianLiveRecord record)
	{
		record.m_bHostile = true;
		record.m_iTalkPlayerId = 0;
		record.m_fSummonedUntil = -1;
		record.m_bSheltering = false;

		JOLP_CivilianConfig config = GetConfig();
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
		if (group)
		{
			// Civilians are neutral to every side, so a slung rifle is never drawn. FIA fights the patrol and the milician.
			JOLP_CivilianOutfitter.SetGroupFaction(group, config.m_sHostileFactionKey);
			SCR_AIConfigComponent aiConfig = SCR_AIConfigComponent.Cast(group.FindComponent(SCR_AIConfigComponent));
			if (aiConfig)
			{
				aiConfig.m_EnablePerception = true;
				aiConfig.m_EnableAttack = true;
				aiConfig.m_EnableDangerEvents = true;
			}

			// Drop peaceful patrol/talk orders so combat behavior can take over.
			JOLP_CivilianWaypointHelper.ClearGroupWaypoints(group, record);
		}

		if (record.m_aCharacterIds)
		{
			foreach (EntityID id : record.m_aCharacterIds)
			{
				IEntity body = GetGame().GetWorld().FindEntityByID(id);
				if (!body)
					continue;

				JOLP_CivilianOutfitter.SetCharacterFaction(body, config.m_sHostileFactionKey);
				JOLP_AISkillConfig.Apply(body, GetSkillConfig().m_ePartisanSkill);

				SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(body.FindComponent(SCR_AICombatComponent));
				if (combat)
				{
					combat.SetHoldFire(false);
					combat.SetPerceptionFactor(1.0);
				}

				JOLP_CivilianOutfitter.ArmPartisan(body, config);
				SuppressCharacter(id);
			}
		}

		Print(string.Format("[JOLP] Partisan at location %1 joined the gunfight", record.m_iSourceLocationId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickTerrorists()
	{
		if (!Replication.IsServer())
			return;

		CleanupSuicideSpeakers();
		if (!m_aLive)
			return;

		float detonateRadius = 8;
		if (m_Config && m_Config.m_fTerroristTriggerRadius > 0)
			detonateRadius = m_Config.m_fTerroristTriggerRadius;

		float revealRadius = detonateRadius + 3;
		if (m_Config && m_Config.m_fTerroristRevealRadius > detonateRadius)
			revealRadius = m_Config.m_fTerroristRevealRadius;

		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record || record.m_bDead || record.m_bDetonated)
				continue;

			if (record.m_eAttitude != JOLP_CivilianAttitude.TERRORIST)
				continue;

			IEntity body = FirstCharacter(record);
			if (!body)
				continue;

			if (!record.m_bChargesAttached && JOLP_WorldUtils.PlayerInRange(body.GetOrigin(), revealRadius))
				TryStrapBomber(record, body);

			if (!JOLP_WorldUtils.PlayerInRange(body.GetOrigin(), detonateRadius))
			{
				// A hide order or an open conversation already replaced their route.
				if (!record.m_bSheltering && record.m_iTalkPlayerId == 0 && record.m_fSummonedUntil < 0)
					TryApproachPlayer(record, body);
				continue;
			}

			Detonate(record, body);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Detonate(notnull JOLP_CivilianLiveRecord record, notnull IEntity body)
	{
		record.m_bDetonated = true;
		record.m_bDead = true;
		m_iBlastMute = m_iBlastMute + 1;

		SuppressCharacter(body.GetID());
		JOLP_TrackedAgent tracked = FindTracked(body.GetID());
		if (tracked)
			tracked.m_eAttitude = JOLP_CivilianAttitude.TERRORIST;

		if (!DetonateWornVest(body))
			Print("[JOLP] Bomb vest had no explosive to detonate", LogLevel.WARNING);

		DamageManagerComponent damage = DamageManagerComponent.Cast(body.FindComponent(DamageManagerComponent));
		if (damage)
			damage.SetHealthScaled(0);

		SpawnLethalBlast(body.GetOrigin());

		GetGame().GetCallqueue().CallLater(EndBlastMute, 2000, false);
		Print("[JOLP] Terrorist detonated", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! The worn vest is the explosive. Destroying it runs that prefab's own warhead.
	protected bool DetonateWornVest(notnull IEntity body)
	{
		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(body.FindComponent(EquipedLoadoutStorageComponent));
		if (!loadout)
			return false;

		IEntity vest = loadout.GetClothFromArea(LoadoutVestArea);
		if (!vest)
			return false;

		DamageManagerComponent damage = DamageManagerComponent.Cast(vest.FindComponent(DamageManagerComponent));
		if (!damage || damage.IsDestroyed())
			return false;

		damage.SetHealthScaled(0);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Second blast at the body, on top of the vest warhead, so the detonation kills anyone in range.
	protected void SpawnLethalBlast(vector position)
	{
		ResourceName explosion = GetConfig().m_rExplosionPrefab;
		if (explosion.IsEmpty())
			return;

		if (!JOLP_WorldUtils.SpawnEntityPrefab(explosion, position, "0 0 0", true))
			Print("[JOLP] Lethal blast prefab failed to spawn", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected void EndBlastMute()
	{
		if (m_iBlastMute > 0)
			m_iBlastMute = m_iBlastMute - 1;
	}

	//------------------------------------------------------------------------------------------------
	protected void PlaySuicideSound(vector position)
	{
		ResourceName prefab = GetConfig().m_rSuicideSoundPrefab;
		if (prefab.IsEmpty())
			return;

		EnsureCollections();

		IEntity speaker = JOLP_WorldUtils.SpawnEntityPrefab(prefab, position, "0 0 0", true);
		if (!speaker)
		{
			Print("[JOLP] Suicide scream prefab failed to spawn", LogLevel.WARNING);
			return;
		}

		SimpleSoundComponent sound = SimpleSoundComponent.Cast(speaker.FindComponent(SimpleSoundComponent));
		if (!sound)
		{
			Print("[JOLP] Suicide scream has no sound component", LogLevel.WARNING);
			return;
		}

		vector mat[4];
		speaker.GetWorldTransform(mat);
		sound.SetTransformation(mat);
		sound.PlayStr("SUICIDE");
		m_aSuicideSpeakers.Insert(speaker.GetID());
		m_aSuicideSpeakerAt.Insert(GetGame().GetWorld().GetWorldTime());
	}

	//------------------------------------------------------------------------------------------------
	protected void CleanupSuicideSpeakers()
	{
		if (!m_aSuicideSpeakers || m_aSuicideSpeakers.IsEmpty())
			return;

		float now = GetGame().GetWorld().GetWorldTime();
		for (int i = m_aSuicideSpeakers.Count() - 1; i >= 0; i--)
		{
			if (now - m_aSuicideSpeakerAt[i] < 8000)
				continue;

			IEntity speaker = GetGame().GetWorld().FindEntityByID(m_aSuicideSpeakers[i]);
			if (speaker)
				JOLP_WorldUtils.DeleteEntitySafe(speaker);

			m_aSuicideSpeakers.Remove(i);
			m_aSuicideSpeakerAt.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TryApproachPlayer(notnull JOLP_CivilianLiveRecord record, notnull IEntity body)
	{
		if (record.m_bSeeking)
			return;

		IEntity player = NearestPlayer(body.GetOrigin(), 80);
		if (!player)
			return;

		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
		if (!group)
			return;

		record.m_bSeeking = true;
		JOLP_CivilianWaypointHelper.ClearGroupWaypoints(group, record);
		JOLP_CivilianWaypointHelper.AssignMoveTo(group, record, GetConfig().m_rMoveWaypointPrefab, player.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity NearestPlayer(vector position, float range)
	{
		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return null;

		array<int> players = {};
		manager.GetPlayers(players);

		IEntity nearest;
		float best = range;
		foreach (int playerId : players)
		{
			IEntity player = manager.GetPlayerControlledEntity(playerId);
			if (!player)
				continue;

			DamageManagerComponent damage = DamageManagerComponent.Cast(player.FindComponent(DamageManagerComponent));
			if (damage && damage.IsDestroyed())
				continue;

			float distance = vector.Distance(player.GetOrigin(), position);
			if (distance <= best)
			{
				best = distance;
				nearest = player;
			}
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateLive()
	{
		float now = GetGame().GetWorld().GetWorldTime();
		int grace = 20000;
		if (m_Config && m_Config.m_iAgentGraceMs > 0)
			grace = m_Config.m_iAgentGraceMs;

		for (int i = m_aLive.Count() - 1; i >= 0; i--)
		{
			JOLP_CivilianLiveRecord record = m_aLive[i];
			if (!record)
			{
				m_aLive.Remove(i);
				continue;
			}

			SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
			if (!record.m_bSeenAgent)
			{
				if (group && group.GetAgentsCount() > 0)
					record.m_bSeenAgent = true;
				else if (now - record.m_fSpawnedAtWorldTime >= grace)
				{
					JOLP_LocationData source = JOLP_Global.GetLocations().GetLocation(record.m_iSourceLocationId);
					string name = "unknown";
					if (source)
						name = source.m_sName;

					Print(string.Format("[JOLP] Villager group at '%1' never got a body and was returned to the pool", name), LogLevel.WARNING);
					RefundFoot(record);
					DespawnRecord(record, false);
					m_aLive.Remove(i);
				}
				continue;
			}

			if (record.m_eKind == JOLP_CivilianKind.VEHICLE)
			{
				if (!record.m_bSeated)
					TrySeat(record);

				if (VehicleFinished(record))
				{
					DespawnRecord(record, false);
					m_aLive.Remove(i);
				}
				continue;
			}

			if (record.m_bDead || !group || group.GetAgentsCount() <= 0)
			{
				DespawnRecord(record, false);
				m_aLive.Remove(i);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! OnAgentAdded can fire before the body exists, and UpdateLive still marks the group as seen.
	//! Those bodies were never registered and never got Speak, so every tick picks them up here.
	protected void EnsureTalkProxies()
	{
		array<AIAgent> agents = {};
		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record || record.m_bDead)
				continue;

			SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
			if (!group)
				continue;

			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;

				IEntity character = agent.GetControlledEntity();
				if (!character)
					continue;

				if (!record.m_aCharacterIds || record.m_aCharacterIds.Find(character.GetID()) < 0)
				{
					OnAgentAdded(agent);
					continue;
				}

				JOLP_CivilianTalkAttach.Try(character);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool VehicleFinished(notnull JOLP_CivilianLiveRecord record)
	{
		IEntity vehicle = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
		if (!vehicle)
			return true;

		DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
		if (damage && damage.IsDestroyed())
			return true;

		JOLP_LocationData target = JOLP_Global.GetLocations().GetLocation(record.m_iTargetLocationId);
		if (!target)
			return true;

		float arriveRadius = Math.Max(40, target.m_fActivationRadius * 0.5);
		if (vector.Distance(vehicle.GetOrigin(), target.m_vPosition) <= arriveRadius)
		{
			Print(string.Format("[JOLP] Civilian car arrived at '%1'", target.m_sName), LogLevel.NORMAL);
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void TrySeat(notnull JOLP_CivilianLiveRecord record)
	{
		IEntity vehicle = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
		IEntity driver = FirstCharacter(record);
		if (!vehicle || !driver)
			return;

		if (JOLP_CivilianOutfitter.SeatDriver(driver, vehicle))
			record.m_bSeated = true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param refundFoot True when a living footman is stored back. Cars never change a town's count.
	protected void DespawnRecord(notnull JOLP_CivilianLiveRecord record, bool refundFoot)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
		if (group)
			group.GetOnAgentAdded().Remove(OnAgentAdded);

		SuppressRecord(record);

		if (refundFoot)
			RefundFoot(record);

		if (group)
			DeleteGroupMembers(group);

		JOLP_CivilianWaypointHelper.DeleteTrackedWaypoints(record);

		if (record.m_VehicleId)
		{
			IEntity vehicle = GetGame().GetWorld().FindEntityByID(record.m_VehicleId);
			DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
			if (vehicle && (!damage || !damage.IsDestroyed()))
				JOLP_WorldUtils.DeleteEntitySafe(vehicle);
		}

		if (group)
			JOLP_WorldUtils.DeleteEntitySafe(group);
	}

	//------------------------------------------------------------------------------------------------
	protected void DropVehicleRecord(EntityID vehicleId)
	{
		for (int i = m_aLive.Count() - 1; i >= 0; i--)
		{
			JOLP_CivilianLiveRecord record = m_aLive[i];
			if (!record || record.m_VehicleId != vehicleId)
				continue;

			DespawnRecord(record, false);
			m_aLive.Remove(i);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RefundFoot(notnull JOLP_CivilianLiveRecord record)
	{
		if (record.m_eKind != JOLP_CivilianKind.FOOT)
			return;

		JOLP_LocationData source = JOLP_Global.GetLocations().GetLocation(record.m_iSourceLocationId);
		if (source)
			source.m_iCivilianStored = source.m_iCivilianStored + 1;
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteGroupMembers(notnull SCR_AIGroup group)
	{
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity controlled = agent.GetControlledEntity();
			if (controlled)
				JOLP_WorldUtils.DeleteEntitySafe(controlled);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SuppressRecord(notnull JOLP_CivilianLiveRecord record)
	{
		if (record.m_aCharacterIds)
		{
			foreach (EntityID id : record.m_aCharacterIds)
				SuppressCharacter(id);
		}

		if (record.m_VehicleId)
			SuppressCharacter(record.m_VehicleId);
	}

	//------------------------------------------------------------------------------------------------
	protected void SuppressCharacter(EntityID id)
	{
		JOLP_TrackedAgent tracked = FindTracked(id);
		if (tracked)
			tracked.m_bSuppressReputation = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void TrackCivilian(IEntity character, notnull JOLP_CivilianLiveRecord record)
	{
		JOLP_TrackedAgent tracked = FindTracked(character.GetID());
		if (!tracked)
		{
			tracked = new JOLP_TrackedAgent();
			tracked.m_EntityId = character.GetID();
			m_aTracked.Insert(tracked);
		}

		tracked.m_bMilician = false;
		tracked.m_bVehicle = false;
		tracked.m_iLocationId = record.m_iSourceLocationId;
		tracked.m_eAttitude = record.m_eAttitude;
	}

	//------------------------------------------------------------------------------------------------
	protected void TrackVehicle(IEntity vehicle, int locationId)
	{
		JOLP_TrackedAgent tracked = FindTracked(vehicle.GetID());
		if (!tracked)
		{
			tracked = new JOLP_TrackedAgent();
			tracked.m_EntityId = vehicle.GetID();
			m_aTracked.Insert(tracked);
		}

		tracked.m_bVehicle = true;
		tracked.m_iLocationId = locationId;
	}

	//------------------------------------------------------------------------------------------------
	protected void MarkRecordDead(EntityID characterId)
	{
		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record || !record.m_aCharacterIds)
				continue;

			if (record.m_aCharacterIds.Find(characterId) >= 0)
				record.m_bDead = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_LocationData FindVehicleDestination(notnull JOLP_LocationData source)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return null;

		float range = 2500;
		if (m_Config && m_Config.m_fVehicleSearchRange > 0)
			range = m_Config.m_fVehicleSearchRange;

		array<ref JOLP_LocationData> candidates = new array<ref JOLP_LocationData>();
		foreach (JOLP_LocationData data : locations.GetAllLocations())
		{
			if (!data || !data.IsTown() || data.m_iId == source.m_iId || data.IsSuppressedByFOB())
				continue;

			if (vector.Distance(source.m_vPosition, data.m_vPosition) <= range)
				candidates.Insert(data);
		}

		if (candidates.IsEmpty())
			return null;

		return candidates.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	protected int CountLiveAttitude(int locationId, JOLP_CivilianAttitude attitude)
	{
		int count = 0;
		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record || record.m_bDead)
				continue;

			if (record.m_eKind != JOLP_CivilianKind.FOOT)
				continue;

			if (record.m_iSourceLocationId == locationId && record.m_eAttitude == attitude)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected void LogVillagePopulation(notnull JOLP_LocationData data, int spawnedThisTick)
	{
		int foot = CountLiveFoot(data.m_iId);
		int cars = CountLiveVehicles(data.m_iId);
		int terrorists = CountLiveAttitude(data.m_iId, JOLP_CivilianAttitude.TERRORIST);
		int partisans = CountLiveAttitude(data.m_iId, JOLP_CivilianAttitude.PARTISAN);
		int neutrals = CountLiveAttitude(data.m_iId, JOLP_CivilianAttitude.NEUTRAL);
		int terroristCap;
		int partisanCap;
		AttitudeCaps(data, terroristCap, partisanCap);

		Print(string.Format("[JOLP] Villagers at '%1': spawned %2 this tick, on foot %3/%4, cars %5/%6",
			data.m_sName,
			spawnedThisTick,
			foot,
			data.m_iCivilianBudget,
			cars,
			data.m_iVehicleSlots)
			+ string.Format(", terrorist %1/%2, partisan %3/%4, neutral %5",
			terrorists,
			terroristCap,
			partisans,
			partisanCap,
			neutrals), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void LogVillageBodies(int locationId)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		JOLP_LocationData data = locations.GetLocation(locationId);
		if (!data)
			return;

		int foot = CountLiveFoot(locationId);
		int bodies = CountLiveFootBodies(locationId);
		Print(string.Format("[JOLP] Villagers at '%1': %2/%3 on foot have a body", data.m_sName, bodies, foot), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected int CountLiveFoot(int locationId)
	{
		int count = 0;
		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record || record.m_bDead)
				continue;

			if (record.m_eKind == JOLP_CivilianKind.FOOT && record.m_iSourceLocationId == locationId)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountLiveFootBodies(int locationId)
	{
		int count = 0;
		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record || record.m_bDead)
				continue;

			if (record.m_eKind != JOLP_CivilianKind.FOOT || record.m_iSourceLocationId != locationId)
				continue;

			SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
			if (group)
				count += group.GetAgentsCount();
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountLiveVehicles(int locationId)
	{
		int count = 0;
		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record)
				continue;

			if (record.m_eKind == JOLP_CivilianKind.VEHICLE && record.m_iSourceLocationId == locationId)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Civilians within 10 m walk to the player and stay long enough to be spoken to.
	void CallToPlayer(IEntity player)
	{
		if (!Replication.IsServer() || !player)
			return;

		EnsureCollections();
		s_vGatherOrigin = player.GetOrigin();
		s_aGather = {};

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		world.QueryEntitiesBySphere(s_vGatherOrigin, 10, GatherCivilian, null, EQueryEntitiesFlags.DYNAMIC);

		float until = world.GetWorldTime() + 20000;
		ref array<EntityID> orderedGroups = {};
		int called = 0;
		foreach (IEntity body : s_aGather)
		{
			if (SummonCivilian(body, player, called, until, orderedGroups))
				called++;
		}

		Print(string.Format("[JOLP] Called %1 civilian(s) over", called), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool GatherCivilian(IEntity entity)
	{
		if (!entity || !s_aGather)
			return true;

		if (JOLP_WorldUtils.DistanceFlat(entity.GetOrigin(), s_vGatherOrigin) > 10)
			return true;

		s_aGather.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool SummonCivilian(IEntity body, IEntity player, int slot, float until, notnull array<EntityID> orderedGroups)
	{
		SCR_ChimeraCharacter speaker = SCR_ChimeraCharacter.Cast(body);
		if (!speaker || speaker.IsInVehicle())
			return false;

		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(body) > 0)
			return false;

		CharacterControllerComponent controller = speaker.GetCharacterController();
		if (!controller || controller.GetLifeState() == ECharacterLifeState.DEAD)
			return false;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(body.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return false;

		Faction faction = affiliation.GetAffiliatedFaction();
		if (!faction || faction.GetFactionKey() != "CIV")
			return false;

		vector stand = CallStand(player, slot);
		JOLP_CivilianLiveRecord record = FindFootRecord(body);
		if (record && !record.m_bDead && record.m_eKind == JOLP_CivilianKind.FOOT)
		{
			if (orderedGroups.Find(record.m_GroupId) >= 0)
				return true;

			SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
			if (!group)
				return false;

			record.m_bSheltering = false;
			record.m_fSummonedUntil = until;
			JOLP_CivilianWaypointHelper.ClearGroupWaypoints(group, record);
			JOLP_CivilianWaypointHelper.AssignMoveTo(group, record, GetConfig().m_rMoveWaypointPrefab, stand, 1.5);
			orderedGroups.Insert(record.m_GroupId);
			return true;
		}

		AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
		if (!control)
			return false;

		AIAgent agent = control.GetAIAgent();
		if (!agent)
			return false;

		SCR_AIGroup guestGroup = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!guestGroup)
			return false;

		EntityID groupId = guestGroup.GetID();
		if (orderedGroups.Find(groupId) >= 0)
			return true;

		array<AIWaypoint> existing = {};
		guestGroup.GetWaypoints(existing);
		foreach (AIWaypoint waypoint : existing)
		{
			if (waypoint)
				guestGroup.RemoveWaypoint(waypoint);
		}

		AIWaypoint move = JOLP_CivilianWaypointHelper.SpawnWaypoint(GetConfig().m_rMoveWaypointPrefab, stand);
		if (!move)
			return false;

		move.SetCompletionRadius(1.5);
		guestGroup.AddWaypoint(move);
		if (!m_aGuestWaypointIds)
		{
			m_aGuestWaypointIds = new array<EntityID>();
			m_aGuestWaypointUntil = new array<float>();
		}

		m_aGuestWaypointIds.Insert(move.GetID());
		m_aGuestWaypointUntil.Insert(until);
		orderedGroups.Insert(groupId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected vector CallStand(IEntity player, int slot)
	{
		vector tm[4];
		player.GetTransform(tm);
		float angle = slot * 0.7;
		vector offset = (tm[2] * Math.Cos(angle)) + (tm[0] * Math.Sin(angle));
		vector stand = player.GetOrigin() + (offset * 1.5);
		BaseWorld world = GetGame().GetWorld();
		if (world)
			stand[1] = world.GetSurfaceY(stand[0], stand[2]);

		return stand;
	}

	//------------------------------------------------------------------------------------------------
	protected void ReleaseSummons()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_aLive)
		{
			foreach (JOLP_CivilianLiveRecord record : m_aLive)
			{
				if (!record || record.m_fSummonedUntil < 0 || now < record.m_fSummonedUntil)
					continue;

				record.m_fSummonedUntil = -1;
				if (record.m_bDead || record.m_iTalkPlayerId != 0 || record.m_bSheltering)
					continue;

				ResumePatrol(record);
			}
		}

		if (!m_aGuestWaypointIds)
			return;

		for (int i = m_aGuestWaypointIds.Count() - 1; i >= 0; i--)
		{
			if (now < m_aGuestWaypointUntil[i])
				continue;

			IEntity waypoint = world.FindEntityByID(m_aGuestWaypointIds[i]);
			JOLP_WorldUtils.DeleteEntitySafe(waypoint);
			m_aGuestWaypointIds.Remove(i);
			m_aGuestWaypointUntil.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Stop this villager while a player is speaking with them. They resume when that menu closes.
	void HoldForTalk(IEntity speaker, IEntity player)
	{
		if (!Replication.IsServer() || !speaker || !player)
			return;

		JOLP_CivilianLiveRecord record = FindFootRecord(speaker);
		if (!record || record.m_bDead)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(player);
		if (playerId <= 0 || record.m_iTalkPlayerId != 0)
			return;

		float reach = 8;
		if (GetConfig().m_fTalkDistance > reach)
			reach = GetConfig().m_fTalkDistance;

		if (JOLP_WorldUtils.DistanceFlat(player.GetOrigin(), speaker.GetOrigin()) > reach)
			return;

		record.m_iTalkPlayerId = playerId;

		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
		if (group)
			JOLP_CivilianWaypointHelper.ClearGroupWaypoints(group, record);
	}

	//------------------------------------------------------------------------------------------------
	//! The player closed the dialogue. Restore the route that HoldForTalk cleared.
	void ReleaseTalk(IEntity speaker, int playerId)
	{
		if (!Replication.IsServer() || !speaker || playerId <= 0)
			return;

		JOLP_CivilianLiveRecord record = FindFootRecord(speaker);
		if (!record || record.m_iTalkPlayerId != playerId)
			return;

		record.m_iTalkPlayerId = 0;
		if (record.m_bDead)
			return;

		if (record.m_bSheltering)
		{
			if (!record.m_aWaypointIds || record.m_aWaypointIds.Count() == 0)
				SendRecordIndoors(record);
			return;
		}

		ResumePatrol(record);
	}

	//------------------------------------------------------------------------------------------------
	protected void SendRecordIndoors(notnull JOLP_CivilianLiveRecord record)
	{
		JOLP_CivilianConfig config = GetConfig();
		if (config.m_rMoveWaypointPrefab.IsEmpty())
			return;

		IEntity body = FirstCharacter(record);
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
		if (!body || !group)
			return;

		float search = 80;
		if (config.m_fShelterSearchRadius > 0)
			search = config.m_fShelterSearchRadius;

		vector stand;
		if (!JOLP_CivilianShelter.FindStand(body.GetOrigin(), search, stand))
			return;

		JOLP_CivilianWaypointHelper.ClearGroupWaypoints(group, record);
		JOLP_CivilianWaypointHelper.AssignMoveTo(group, record, config.m_rMoveWaypointPrefab, stand, 1.2);
	}

	//------------------------------------------------------------------------------------------------
	protected void ResumePatrol(notnull JOLP_CivilianLiveRecord record)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
		if (!group)
			return;

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		JOLP_LocationData data = locations.GetLocation(record.m_iSourceLocationId);
		if (!data)
			return;

		JOLP_CivilianConfig config = GetConfig();
		if (config.m_rPatrolWaypointPrefab.IsEmpty() || config.m_rCycleWaypointPrefab.IsEmpty())
			return;

		vector center = data.m_vPosition;
		IEntity body = FirstCharacter(record);
		if (body)
			center = body.GetOrigin();

		float patrolRadius = Math.RandomFloat(22, 48);
		int points = Math.RandomInt(3, 5);
		float startAngle = Math.RandomFloat(0, Math.PI2);
		JOLP_CivilianWaypointHelper.AssignCyclePatrol(group, record, config.m_rPatrolWaypointPrefab, config.m_rCycleWaypointPrefab, center, patrolRadius, points, startAngle);
	}

	//------------------------------------------------------------------------------------------------
	//! Foot villager this body belongs to. Null for drivers, soldiers, and anyone we did not spawn.
	JOLP_CivilianLiveRecord FindFootRecord(IEntity character)
	{
		if (!character || !m_aLive)
			return null;

		EntityID id = character.GetID();
		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record || record.m_eKind != JOLP_CivilianKind.FOOT || !record.m_aCharacterIds)
				continue;

			if (record.m_aCharacterIds.Find(id) >= 0)
				return record;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Send nearby foot civilians into the closest building and leave them there.
	void OrderShelter(IEntity speaker, out int inRange, out int sheltered)
	{
		inRange = 0;
		sheltered = 0;
		if (!Replication.IsServer() || !speaker)
			return;

		EnsureCollections();

		JOLP_CivilianConfig config = GetConfig();
		float radius = 45;
		float search = 80;
		if (config.m_fShelterRadius > 0)
			radius = config.m_fShelterRadius;
		if (config.m_fShelterSearchRadius > 0)
			search = config.m_fShelterSearchRadius;

		if (config.m_rMoveWaypointPrefab.IsEmpty())
			return;

		vector origin = speaker.GetOrigin();
		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (!record || record.m_bDead)
				continue;

			if (record.m_eKind != JOLP_CivilianKind.FOOT)
				continue;

			IEntity body = FirstCharacter(record);
			if (!body)
				continue;

			if (JOLP_WorldUtils.DistanceFlat(origin, body.GetOrigin()) > radius)
				continue;

			inRange++;

			SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(record.m_GroupId));
			if (!group)
				continue;

			vector stand;
			if (!JOLP_CivilianShelter.FindStand(body.GetOrigin(), search, stand, sheltered))
				continue;

			JOLP_CivilianWaypointHelper.ClearGroupWaypoints(group, record);
			JOLP_CivilianWaypointHelper.AssignMoveTo(group, record, config.m_rMoveWaypointPrefab, stand, 1.2);
			record.m_bSheltering = true;
			sheltered++;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_CivilianLiveRecord FindRecordByGroup(EntityID groupId)
	{
		foreach (JOLP_CivilianLiveRecord record : m_aLive)
		{
			if (record && record.m_GroupId == groupId)
				return record;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FirstCharacter(notnull JOLP_CivilianLiveRecord record)
	{
		if (!record.m_aCharacterIds)
			return null;

		foreach (EntityID id : record.m_aCharacterIds)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(id);
			if (entity)
				return entity;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_TrackedAgent FindTracked(EntityID id)
	{
		if (!m_aTracked)
			return null;

		foreach (JOLP_TrackedAgent tracked : m_aTracked)
		{
			if (tracked && tracked.m_EntityId == id)
				return tracked;
		}

		return null;
	}
}
