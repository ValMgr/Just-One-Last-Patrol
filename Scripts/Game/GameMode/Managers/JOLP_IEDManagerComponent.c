class JOLP_IEDManagerComponentClass : JOLP_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Server-side RUK charges. Villages, towns, cities, hideouts, and OPs each get a pool.
//! Lives on a Manager.layer world entity, same as the civilian manager.
class JOLP_IEDManagerComponent : JOLP_Component
{
	static JOLP_IEDManagerComponent s_Instance;

	[Attribute(desc: "IED counts, daily chances, and RUK prefabs (edit on Manager.layer entity)")]
	ref JOLP_IEDConfig m_Config;

	protected bool m_bStarted;
	protected bool m_bClockPrimed;
	protected bool m_bRestoredFromSave;
	protected int m_iLastDailyKey;
	protected ref array<ref JOLP_IEDRecord> m_aCharges;
	//! Charges that already spawned their killing blast. A charge can own more than one trigger.
	protected ref array<EntityID> m_aBlastedIds;

	//------------------------------------------------------------------------------------------------
	static JOLP_IEDManagerComponent GetInstance()
	{
		if (s_Instance)
			return s_Instance;

		BaseGameMode gameMode = GetGame().GetGameMode();
		if (gameMode)
		{
			s_Instance = JOLP_IEDManagerComponent.Cast(gameMode.FindComponent(JOLP_IEDManagerComponent));
			if (s_Instance)
				return s_Instance;
		}

		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, FindIEDManagerEntity, null, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool FindIEDManagerEntity(IEntity entity)
	{
		if (!entity || s_Instance)
			return true;

		JOLP_IEDManagerComponent component = JOLP_IEDManagerComponent.Cast(entity.FindComponent(JOLP_IEDManagerComponent));
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
		Print("[JOLP] IEDManager Init", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	void PostGameStart()
	{
		if (!Replication.IsServer())
			return;

		if (m_bStarted)
			return;

		EnsureCollections();
		if (m_bRestoredFromSave)
			SpawnMissingCharges();
		else
			InitializePlaces();

		int poll = 5000;
		if (m_Config && m_Config.m_iClockPollMs > 0)
			poll = m_Config.m_iClockPollMs;

		GetGame().GetCallqueue().CallLater(PollDailyClock, poll, true);
		m_bStarted = true;

		Print(string.Format("[JOLP] IEDManager PostGameStart: %1 charge(s), daily check %2 ms",
			m_aCharges.Count(),
			poll), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	JOLP_IEDConfig GetConfig()
	{
		if (!m_Config)
			m_Config = new JOLP_IEDConfig();

		return m_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Safe-wire completion. Cutting the live wire does not come through here.
	void NotifyDefused(IEntity charge, IEntity user)
	{
		if (!Replication.IsServer() || !charge)
			return;

		JOLP_IEDRecord record = FindRecordFromEntity(charge);
		if (!record || record.m_bDefused || record.m_bDetonated)
			return;

		record.m_bDefused = true;

		JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
		if (civilians)
			civilians.NotifyIEDDefused(record.m_iLocationId, record.m_vPosition, user);
	}

	//------------------------------------------------------------------------------------------------
	//! The RUK trigger just fired. Spawn the killing blast once. A defused charge never reaches this
	//! with m_bDefused still false, and a second trigger on the same charge is ignored.
	void NotifyChargeDetonated(IEntity charge)
	{
		if (!Replication.IsServer() || !charge)
			return;

		if (!m_aBlastedIds)
			m_aBlastedIds = new array<EntityID>();

		EntityID id = charge.GetID();
		if (m_aBlastedIds.Contains(id))
			return;

		JOLP_IEDRecord record = FindRecordFromEntity(charge);
		if (record && (record.m_bDefused || record.m_bDetonated))
			return;

		m_aBlastedIds.Insert(id);
		if (record)
			MarkDetonated(record);

		vector pos = charge.GetOrigin();
		pos[1] = pos[1] + 0.5;
		SpawnLethalBlast(pos);
	}

	//------------------------------------------------------------------------------------------------
	//! True when this death belongs to one of our charges. A blast can kill more than one civilian.
	bool TryResolveIEDKill(IEntity victim, IEntity instigator, out int locationId, out vector iedPos)
	{
		locationId = -1;
		iedPos = vector.Zero;

		if (!victim)
			return false;

		JOLP_IEDRecord record = FindRecordFromEntity(instigator);
		if (record && record.m_bDefused)
			record = null;

		if (!record)
			record = FindRecentDetonation(victim.GetOrigin());

		if (!record)
			return false;

		MarkDetonated(record);
		locationId = record.m_iLocationId;
		iedPos = record.m_vPosition;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void EnsureCollections()
	{
		if (!m_Config)
			m_Config = new JOLP_IEDConfig();
		if (!m_aCharges)
			m_aCharges = new array<ref JOLP_IEDRecord>();
	}

	//------------------------------------------------------------------------------------------------
	protected void InitializePlaces()
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		foreach (JOLP_LocationData data : locations.GetAllLocations())
			InitializePlace(data);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes live charges belonging to a FOB-suppressed place (and any charge physically inside a FOB).
	void ClearChargesAtLocation(int locationId)
	{
		if (!Replication.IsServer() || locationId < 0)
			return;

		EnsureCollections();

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		int removed = 0;

		for (int i = m_aCharges.Count() - 1; i >= 0; i--)
		{
			JOLP_IEDRecord record = m_aCharges[i];
			if (!record || record.m_bDetonated)
				continue;

			bool clear = record.m_iLocationId == locationId;
			if (!clear && locations)
				clear = locations.IsPositionInsideFOB(record.m_vPosition);

			if (!clear)
				continue;

			IEntity charge = ChargeEntity(record);
			if (charge)
			{
				DisarmCharge(charge);
				SCR_EntityHelper.DeleteEntityAndChildren(charge);
			}

			m_aCharges.Remove(i);
			removed++;
		}

		if (removed > 0)
			Print(string.Format("[JOLP] Cleared %1 IED(s) after FOB suppression at location %2", removed, locationId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void InitializePlace(JOLP_LocationData data)
	{
		if (!data || data.m_bIEDInitialized || !data.CanHostIED())
			return;

		if (data.IsSuppressedByFOB() || PlaceInsideFOB(data))
		{
			data.m_iIEDInitialCount = 0;
			data.m_bIEDInitialized = true;
			return;
		}

		int count = GetConfig().RollInitialCount();
		data.m_iIEDInitialCount = count;
		data.m_bIEDInitialized = true;

		int spawned = 0;
		for (int i = 0; i < count; i++)
		{
			if (SpawnCharge(data))
				spawned++;
		}

		Print(string.Format("[JOLP] IEDs at '%1': rolled %2, planted %3",
			data.m_sName,
			count,
			spawned), LogLevel.NORMAL);

		SpawnDecoyPiles(data);
	}

	//------------------------------------------------------------------------------------------------
	protected void PollDailyClock()
	{
		if (!Replication.IsServer())
			return;

		int dateKey;
		int hour;
		if (!ReadClock(dateKey, hour))
			return;

		int dailyHour = 8;
		if (m_Config && m_Config.m_iDailyHour >= 0)
			dailyHour = m_Config.m_iDailyHour;

		if (!m_bClockPrimed)
		{
			m_bClockPrimed = true;
			// A continue keeps the saved day key. Re-priming it would either plant a second
			// charge or skip the one that had not run yet.
			if (m_bRestoredFromSave)
				return;

			if (hour >= dailyHour)
				m_iLastDailyKey = dateKey;
			else
				m_iLastDailyKey = 0;

			return;
		}

		if (hour < dailyHour)
			return;

		if (m_iLastDailyKey == dateKey)
			return;

		m_iLastDailyKey = dateKey;
		RunDaily();
	}

	//------------------------------------------------------------------------------------------------
	//! An admin clock skip is not a new day. If the landing time is already at or past the
	//! daily hour, that calendar day is marked done so the jump does not plant charges.
	//! A landing time before the daily hour still runs the check when that hour arrives.
	void AcknowledgeAdminTimeSkip()
	{
		if (!Replication.IsServer())
			return;

		int dateKey;
		int hour;
		if (!ReadClock(dateKey, hour))
			return;

		int dailyHour = 8;
		if (m_Config && m_Config.m_iDailyHour >= 0)
			dailyHour = m_Config.m_iDailyHour;

		if (hour < dailyHour)
			return;

		m_iLastDailyKey = dateKey;
		m_bClockPrimed = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDaily()
	{
		JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
		if (civilians)
			civilians.ApplyDailyGarrisonDecay();

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		int added = 0;
		foreach (JOLP_LocationData data : locations.GetAllLocations())
		{
			if (!data || !data.CanHostIED() || data.IsSuppressedByFOB() || PlaceInsideFOB(data))
				continue;

			if (!data.m_bIEDInitialized)
				InitializePlace(data);

			if (data.IsOP())
			{
				if (data.m_iGarrisonStored <= 0)
					continue;

				if (CountLive(data.m_iId) >= data.m_iIEDInitialCount)
					continue;

				if (SpawnCharge(data))
					added++;

				continue;
			}

			if (!data.HasReputation() || !civilians)
				continue;

			float chance = GetConfig().ChanceForStage(civilians.GetStage(data));
			if (chance <= 0)
				continue;

			if (Math.RandomFloat01() >= chance)
				continue;

			if (SpawnCharge(data))
				added++;
		}

		Print(string.Format("[JOLP] 08:00 IED check planted %1 charge(s)", added), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected bool ReadClock(out int dateKey, out int hour)
	{
		dateKey = 0;
		hour = 0;

		BaseWorld baseWorld = GetGame().GetWorld();
		if (!baseWorld)
			return false;

		ChimeraWorld world = ChimeraWorld.CastFrom(baseWorld);
		if (!world)
			return false;

		TimeAndWeatherManagerEntity time = world.GetTimeAndWeatherManager();
		if (!time)
			return false;

		int year;
		int month;
		int day;
		time.GetDate(year, month, day);

		int minute;
		int second;
		time.GetHoursMinutesSeconds(hour, minute, second);
		dateKey = (year * 10000) + (month * 100) + day;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool SpawnCharge(notnull JOLP_LocationData data)
	{
		ResourceName prefab = GetConfig().RandomPrefab();
		if (prefab.IsEmpty())
		{
			Print("[JOLP] IED spawn skipped: no prefabs in the IED config", LogLevel.WARNING);
			return false;
		}

		vector position;
		vector angles;
		if (!PickSpawnPosition(data, position, angles))
		{
			Print(string.Format("[JOLP] IED spawn skipped at '%1': no free spot", data.m_sName), LogLevel.WARNING);
			return false;
		}

		IEntity charge = JOLP_Global.SpawnEntityPrefab(prefab, position, angles, true);
		if (!charge)
			return false;

		RUK_IEDManagerComponent ruk = RUK_IEDManagerComponent.Cast(charge.FindComponent(RUK_IEDManagerComponent));
		if (ruk)
			ruk.JOLP_RequireWirecutters();
		else
			Print("[JOLP] Spawned IED has no RUK_IEDManagerComponent", LogLevel.WARNING);

		JOLP_IEDRecord record = new JOLP_IEDRecord();
		record.m_EntityId = charge.GetID();
		record.m_iLocationId = data.m_iId;
		record.m_sLocationKey = data.m_sPersistentKey;
		if (record.m_sLocationKey.IsEmpty())
			record.m_sLocationKey = data.m_sName;
		record.m_vPosition = charge.GetOrigin();
		record.m_vAngles = angles;
		record.m_sPrefab = prefab;
		m_aCharges.Insert(record);
		SpawnCamouflage(charge.GetOrigin());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Empty rubbish piles, away from every charge, so a heap is not a bomb marker.
	protected void SpawnDecoyPiles(notnull JOLP_LocationData data)
	{
		if (data.m_bDecoyTrashSpawned || !data.HasReputation())
			return;

		data.m_bDecoyTrashSpawned = true;

		if (data.IsSuppressedByFOB() || PlaceInsideFOB(data))
			return;

		JOLP_IEDConfig config = GetConfig();
		if (!config.m_aTrashPrefabs || config.m_aTrashPrefabs.IsEmpty())
			return;

		int want = config.RollDecoyCount(data.m_eSettlementSize);
		if (want <= 0)
			return;

		float radius = data.m_fActivationRadius;
		if (radius < 20)
			radius = 20;

		array<vector> placed = {};
		int spawned = 0;
		int attempts = want * 6;
		for (int i = 0; i < attempts && spawned < want; i++)
		{
			vector pos;
			vector angles;
			if (!PickRoadSpot(data, radius, pos, angles))
				continue;

			if (!AcceptDecoySpot(data, pos, radius, placed))
				continue;

			SpawnCamouflage(pos);
			placed.Insert(pos);
			spawned++;
		}

		Print(string.Format("[JOLP] Decoy trash at '%1': placed %2 pile(s)",
			data.m_sName,
			spawned), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected bool AcceptDecoySpot(notnull JOLP_LocationData data, vector pos, float radius, array<vector> placed)
	{
		if (vector.Distance(pos, data.m_vPosition) > radius)
			return false;

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (locations && locations.IsPositionInsideFOB(pos))
			return false;

		float clearance = 20;
		if (m_Config && m_Config.m_fDecoyClearance > 0)
			clearance = m_Config.m_fDecoyClearance;

		if (NearCharge(pos, clearance))
			return false;

		float spacing = 16;
		if (m_Config && m_Config.m_fDecoySpacing > 0)
			spacing = m_Config.m_fDecoySpacing;

		foreach (vector other : placed)
		{
			if (vector.Distance(other, pos) < spacing)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool NearCharge(vector pos, float clearance)
	{
		foreach (JOLP_IEDRecord record : m_aCharges)
		{
			if (!record)
				continue;

			if (vector.Distance(record.m_vPosition, pos) < clearance)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A few bits of rubbish beside the charge, far enough that the wire action stays reachable.
	protected void SpawnCamouflage(vector chargePos)
	{
		JOLP_IEDConfig config = GetConfig();
		int count = config.RollTrashCount();
		for (int i = 0; i < count; i++)
		{
			ResourceName prefab = config.RandomTrash();
			if (prefab.IsEmpty())
				return;

			float angle = Math.RandomFloat(0, Math.PI2);
			float dist = Math.RandomFloat(1.2, 3.2);
			vector pos = chargePos + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);
			pos[1] = chargePos[1];

			vector grounded;
			if (!JOLP_WorldUtils.SnapNearRoad(pos, grounded))
				continue;

			vector angles = JOLP_WorldUtils.UprightYaw(Math.RandomFloat(0, 360));
			JOLP_Global.SpawnEntityPrefab(prefab, grounded, angles, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool PlaceInsideFOB(notnull JOLP_LocationData data)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return false;

		float radius = data.m_fActivationRadius;
		if (radius < 0)
			radius = 0;

		return locations.IsAreaInsideFOB(data.m_vPosition, radius);
	}

	//------------------------------------------------------------------------------------------------
	protected bool PickSpawnPosition(notnull JOLP_LocationData data, out vector position, out vector angles)
	{
		position = data.m_vPosition;
		angles = "0 0 0";

		float radius = data.m_fActivationRadius;
		if (radius < 20)
			radius = 20;

		float spacing = 30;
		if (m_Config && m_Config.m_fMinSpacing > 0)
			spacing = m_Config.m_fMinSpacing;

		for (int attempt = 0; attempt < 16; attempt++)
		{
			vector candidate;
			vector candidateAngles;
			if (!PickRoadSpot(data, radius, candidate, candidateAngles))
				continue;

			if (!AcceptSpot(data, candidate, radius, spacing))
				continue;

			position = candidate;
			angles = candidateAngles;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool PickRoadSpot(notnull JOLP_LocationData data, float radius, out vector position, out vector angles)
	{
		position = data.m_vPosition;
		angles = "0 0 0";

		vector probe = RandomOffset(data.m_vPosition, radius * 0.85);
		vector roadPos;
		vector roadAngles;
		if (!JOLP_WorldUtils.FindNearestRoadSpawn(probe, radius, roadPos, roadAngles))
			return false;

		if (vector.Distance(roadPos, data.m_vPosition) > radius)
			return false;

		vector grounded;
		if (!JOLP_WorldUtils.SnapNearRoad(OnRoad(roadPos, roadAngles), grounded))
			return false;

		position = grounded;
		angles = JOLP_WorldUtils.UprightYaw(roadAngles[0] + Math.RandomFloat(-35, 35));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool AcceptSpot(notnull JOLP_LocationData data, vector pos, float radius, float spacing)
	{
		if (vector.Distance(pos, data.m_vPosition) > radius)
			return false;

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (locations && locations.IsPositionInsideFOB(pos))
			return false;

		if (TooClose(data.m_iId, pos, spacing))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected vector RandomOffset(vector center, float radius)
	{
		float angle = Math.RandomFloat(0, Math.PI2);
		float dist = Math.RandomFloat(0, radius);
		return center + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);
	}

	//------------------------------------------------------------------------------------------------
	//! Shoulder of the road, not the driving line. Height is still the spline until SnapNearRoad.
	protected vector OnRoad(vector roadPos, vector roadAngles)
	{
		float yawRad = roadAngles[0] * Math.DEG2RAD;
		float side = Math.RandomFloat(2.0, 3.2);
		if (Math.RandomFloat01() < 0.5)
			side = -side;

		float along = Math.RandomFloat(-2.5, 2.5);
		vector right = Vector(Math.Cos(yawRad), 0, -Math.Sin(yawRad));
		vector forward = Vector(Math.Sin(yawRad), 0, Math.Cos(yawRad));
		vector spot = roadPos + (right * side) + (forward * along);
		spot[1] = roadPos[1];
		return spot;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TooClose(int locationId, vector pos, float spacing)
	{
		foreach (JOLP_IEDRecord record : m_aCharges)
		{
			if (!record || record.m_iLocationId != locationId)
				continue;

			if (record.m_bDefused || record.m_bDetonated)
				continue;

			if (!ChargeEntity(record))
				continue;

			if (vector.Distance(record.m_vPosition, pos) < spacing)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountLive(int locationId)
	{
		int count = 0;
		foreach (JOLP_IEDRecord record : m_aCharges)
		{
			if (!record || record.m_iLocationId != locationId)
				continue;

			if (record.m_bDefused || record.m_bDetonated)
				continue;

			IEntity charge = ChargeEntity(record);
			if (!charge)
				continue;

			DamageManagerComponent damage = DamageManagerComponent.Cast(charge.FindComponent(DamageManagerComponent));
			if (damage && damage.IsDestroyed())
			{
				MarkDetonated(record);
				continue;
			}

			count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_IEDRecord FindRecentDetonation(vector pos)
	{
		float radius = 40;
		if (m_Config && m_Config.m_fKillRadius > 0)
			radius = m_Config.m_fKillRadius;

		float now = GetGame().GetWorld().GetWorldTime();
		JOLP_IEDRecord best;
		float bestDist = 0;
		foreach (JOLP_IEDRecord record : m_aCharges)
		{
			if (!record || record.m_bDefused)
				continue;

			IEntity charge = ChargeEntity(record);
			vector chargePos = record.m_vPosition;
			if (charge)
				chargePos = charge.GetOrigin();

			float dist = vector.Distance(chargePos, pos);
			if (dist > radius)
				continue;

			if (!record.m_bDetonated && charge)
			{
				DamageManagerComponent damage = DamageManagerComponent.Cast(charge.FindComponent(DamageManagerComponent));
				if (damage && damage.IsDestroyed())
					MarkDetonated(record);
			}

			if (!record.m_bDetonated)
				continue;

			if (now - record.m_fDetonatedAt > 3000)
				continue;

			if (!best || dist < bestDist)
			{
				best = record;
				bestDist = dist;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected void MarkDetonated(JOLP_IEDRecord record)
	{
		if (!record)
			return;

		if (record.m_fDetonatedAt < 0)
			record.m_fDetonatedAt = GetGame().GetWorld().GetWorldTime();

		record.m_bDetonated = true;
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_IEDRecord FindRecordFromEntity(IEntity entity)
	{
		IEntity cursor = entity;
		int guard = 0;
		while (cursor && guard < 6)
		{
			JOLP_IEDRecord record = FindRecord(cursor.GetID());
			if (record)
				return record;

			cursor = cursor.GetParent();
			guard++;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_IEDRecord FindRecord(EntityID id)
	{
		foreach (JOLP_IEDRecord record : m_aCharges)
		{
			if (record && record.m_EntityId == id)
				return record;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	int GetLastDailyKey()
	{
		return m_iLastDailyKey;
	}

	//------------------------------------------------------------------------------------------------
	array<ref JOLP_IEDRecord> GetCharges()
	{
		EnsureCollections();
		return m_aCharges;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the live charge list with the save. Entities are spawned by SpawnMissingCharges
	//! so a reapply does not plant a second copy of a charge that is already in the world.
	void ApplyPersistedCharges(array<ref JOLP_PersistedIED> records, int lastDailyKey)
	{
		EnsureCollections();
		m_bRestoredFromSave = true;
		m_iLastDailyKey = lastDailyKey;

		array<ref JOLP_IEDRecord> previous = m_aCharges;
		array<ref JOLP_IEDRecord> rebuilt = new array<ref JOLP_IEDRecord>();
		if (records)
		{
			foreach (JOLP_PersistedIED saved : records)
			{
				if (!saved || saved.m_sPrefab.IsEmpty())
					continue;

				JOLP_IEDRecord record = new JOLP_IEDRecord();
				record.m_sLocationKey = saved.m_sLocationKey;
				record.m_vPosition = saved.m_vPosition;
				record.m_vAngles = saved.m_vAngles;
				record.m_sPrefab = saved.m_sPrefab;
				record.m_bDefused = saved.m_bDefused;

				JOLP_IEDRecord existing = FindChargeNear(previous, saved.m_vPosition);
				if (existing)
				{
					record.m_EntityId = existing.m_EntityId;
					record.m_iLocationId = existing.m_iLocationId;
				}

				rebuilt.Insert(record);
			}
		}

		m_aCharges = rebuilt;
		if (m_bStarted)
			SpawnMissingCharges();
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_IEDRecord FindChargeNear(array<ref JOLP_IEDRecord> charges, vector pos)
	{
		if (!charges)
			return null;

		foreach (JOLP_IEDRecord record : charges)
		{
			if (!record || record.m_bDetonated)
				continue;

			if (vector.Distance(record.m_vPosition, pos) < 1.5)
				return record;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnMissingCharges()
	{
		EnsureCollections();
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		int spawned = 0;

		foreach (JOLP_IEDRecord record : m_aCharges)
		{
			if (!record || record.m_bDetonated)
				continue;

			if (locations && record.m_iLocationId < 0)
			{
				JOLP_LocationData data = locations.FindByPersistentKey(record.m_sLocationKey);
				if (data)
					record.m_iLocationId = data.m_iId;
			}

			IEntity existing = ChargeEntity(record);
			if (existing)
			{
				if (record.m_bDefused)
					DisarmCharge(existing);
				continue;
			}

			if (record.m_sPrefab.IsEmpty())
				continue;

			IEntity charge = JOLP_Global.SpawnEntityPrefab(record.m_sPrefab, record.m_vPosition, record.m_vAngles, true);
			if (!charge)
				continue;

			record.m_EntityId = charge.GetID();
			record.m_vPosition = charge.GetOrigin();

			RUK_IEDManagerComponent ruk = RUK_IEDManagerComponent.Cast(charge.FindComponent(RUK_IEDManagerComponent));
			if (ruk)
				ruk.JOLP_RequireWirecutters();

			if (record.m_bDefused)
				DisarmCharge(charge);
			else
				SpawnCamouflage(charge.GetOrigin());

			spawned++;
		}

		Print(string.Format("[JOLP] Restored %1 IED charge(s)", spawned), LogLevel.NORMAL);
		SpawnRestoredDecoys(locations);
	}

	//------------------------------------------------------------------------------------------------
	//! Rubbish is not saved. A continue puts the empty piles back once the charges exist.
	protected void SpawnRestoredDecoys(JOLP_LocationManagerComponent locations)
	{
		if (!locations)
			return;

		foreach (JOLP_LocationData data : locations.GetAllLocations())
		{
			if (!data || !data.m_bIEDInitialized)
				continue;

			SpawnDecoyPiles(data);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A defused charge is spawned again so the spot stays occupied, then its triggers are killed
	//! so driving over it does not count as a fresh bomb.
	protected void DisarmCharge(IEntity charge)
	{
		if (!charge)
			return;

		DisarmTriggers(charge);
	}

	//------------------------------------------------------------------------------------------------
	//! Instant blast, lifted off the ground so the warhead is not buried in the road.
	protected void SpawnLethalBlast(vector position)
	{
		ResourceName explosion = GetConfig().m_rBlastPrefab;
		if (explosion.IsEmpty())
		{
			Print("[JOLP] IED detonated with no blast prefab set", LogLevel.WARNING);
			return;
		}

		if (!JOLP_WorldUtils.SpawnEntityPrefab(explosion, position, "0 0 0", true))
		{
			Print("[JOLP] IED blast prefab failed to spawn", LogLevel.WARNING);
			return;
		}

		Print(string.Format("[JOLP] IED blast at %1", position), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void DisarmTriggers(IEntity entity)
	{
		if (!entity)
			return;

		SCR_BaseTriggerComponent trigger = SCR_BaseTriggerComponent.Cast(entity.FindComponent(SCR_BaseTriggerComponent));
		if (trigger)
			trigger.DisarmTrigger();

		IEntity child = entity.GetChildren();
		while (child)
		{
			DisarmTriggers(child);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity ChargeEntity(notnull JOLP_IEDRecord record)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		return world.FindEntityByID(record.m_EntityId);
	}
}
