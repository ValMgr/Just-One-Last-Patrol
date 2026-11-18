class JOLP_LocationManagerComponentClass : JOLP_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Registry of world locations (towns / OPs / hideouts) and their garrison pool state.
class JOLP_LocationManagerComponent : JOLP_Component
{
	static JOLP_LocationManagerComponent s_Instance;

	protected ref array<ref JOLP_LocationData> m_aLocations;
	protected ref array<JOLP_LocationControllerComponent> m_aControllers;
	protected ref array<JOLP_FOBControllerComponent> m_aFOBs;

	//------------------------------------------------------------------------------------------------
	static JOLP_LocationManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode gameMode = GetGame().GetGameMode();
			if (gameMode)
				s_Instance = JOLP_LocationManagerComponent.Cast(gameMode.FindComponent(JOLP_LocationManagerComponent));
		}

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	protected ref array<IEntity> m_aMapDescriptorHits;

	//------------------------------------------------------------------------------------------------
	void Init(IEntity owner)
	{
		s_Instance = this;
		m_aLocations = new array<ref JOLP_LocationData>();
		m_aControllers = new array<JOLP_LocationControllerComponent>();
		m_aFOBs = new array<JOLP_FOBControllerComponent>();

		DiscoverWorldLocations();
		SeedFromMapDescriptorsIfNeeded();
		DiscoverWorldFOBs();
		RefreshFOBSuppression();
		Print(string.Format("[JOLP] LocationManager Init: %1 location(s), %2 FOB(s)", m_aLocations.Count(), m_aFOBs.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	void PostGameStart()
	{
		// Resolve once so the first hideout promote sees the correct flag.
		JOLP_Debug.IsEnabled();

		RefreshFOBSuppression();

		JOLP_JustOneLastPatrolGameMode mode = JOLP_JustOneLastPatrolGameMode.Cast(GetGame().GetGameMode());
		if (mode && mode.WasRestoredFromSave())
			RespawnSavedHideouts();
		else
			SelectAndPromoteHideouts();

		if (JOLP_Debug.IsEnabled())
			GetGame().GetCallqueue().CallLater(PlaceDebugHideoutMarkers, 1000, false);

		Print(string.Format("[JOLP] LocationManager PostGameStart: %1 location(s) ready (%2 hideout(s), %3 FOB(s))",
			m_aLocations.Count(), GetAliveHideoutCount(), GetFOBCount()), LogLevel.NORMAL);

		foreach (JOLP_LocationData data : m_aLocations)
		{
			string flags = "";
			if (data.m_bSuppressedByFOB)
				flags = flags + " SUPPRESSED_BY_FOB";
			if (data.IsHideout())
				flags = flags + " HIDEOUT";
			if (data.m_bHideoutDestroyed)
				flags = flags + " DESTROYED";

			string vehicles = string.Format("%1/%2/%3", data.m_iTransportStored, data.m_iLightStored, data.m_iHeavyStored);
			Print(string.Format("[JOLP]   [%1] %2 type=%3 size=%4 garrison=%5 vehicles=%6 ratio=%7 radius=%8%9",
				data.m_iId,
				data.m_sName,
				typename.EnumToString(JOLP_LocationType, data.m_eType),
				typename.EnumToString(JOLP_SettlementSize, data.m_eSettlementSize),
				data.m_iGarrisonStored,
				vehicles,
				data.m_fActiveGarrisonRatio,
				data.m_fActivationRadius,
				flags), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pick N village-sized TOWN locations (mission param) and promote them to HIDEOUT with an HQ prop.
	//! Full separation is spent first on one hideout per map quarter. Slots still missing after that use half the radius.
	protected void SelectAndPromoteHideouts()
	{
		if (!Replication.IsServer())
			return;

		if (!m_aLocations)
			return;

		int wanted = JOLP_MissionHeader.GetHideoutCount();
		if (wanted <= 0)
		{
			Print("[JOLP] Hideout selection skipped (mission m_iHideoutCount=0)", LogLevel.NORMAL);
			return;
		}

		array<ref JOLP_LocationData> candidates = new array<ref JOLP_LocationData>();
		foreach (JOLP_LocationData data : m_aLocations)
		{
			if (!data)
				continue;

			if (data.m_eType != JOLP_LocationType.TOWN)
				continue;

			if (data.m_eSettlementSize != JOLP_SettlementSize.VILLAGE)
				continue;

			if (data.m_bSuppressedByFOB)
				continue;

			candidates.Insert(data);
		}

		if (candidates.IsEmpty())
		{
			Print("[JOLP] Hideout selection: no village-sized TOWN candidates", LogLevel.WARNING);
			return;
		}

		// Fisher–Yates shuffle.
		for (int i = candidates.Count() - 1; i > 0; i--)
		{
			int j = Math.RandomInt(0, i + 1);
			JOLP_LocationData tmp = candidates[i];
			candidates[i] = candidates[j];
			candidates[j] = tmp;
		}

		JOLP_MilicianGarrisonConfig config;
		JOLP_MilicianManagerComponent milician = JOLP_Global.GetMilician();
		if (milician)
			config = milician.GetConfig();

		int initialGarrison = 40;
		int maxGarrison = 60;
		float minSeparation = 2000;
		ResourceName hqPrefab = "{664B13AF00000300}Prefabs/Props/Military/JOLP_HideoutSite.et";
		if (config)
		{
			if (config.m_iHideoutInitialGarrison > 0)
				initialGarrison = config.m_iHideoutInitialGarrison;
			if (config.m_iHideoutMaxGarrison > 0)
				maxGarrison = config.m_iHideoutMaxGarrison;
			if (config.m_fHideoutMinSeparation > 0)
				minSeparation = config.m_fHideoutMinSeparation;
			if (!config.m_rHideoutHQPrefab.IsEmpty())
				hqPrefab = config.m_rHideoutHQPrefab;
		}

		if (maxGarrison < initialGarrison)
			maxGarrison = initialGarrison;

		array<ref JOLP_LocationData> chosen = new array<ref JOLP_LocationData>();
		float midX;
		float midZ;
		GetHideoutMapMidpoint(midX, midZ);

		// One hideout per quarter, still at the full safe radius.
		TryPromoteHideouts(candidates, chosen, wanted, minSeparation, midX, midZ, true, initialGarrison, maxGarrison, hqPrefab);

		// Spare slots still use the full radius when a village is far enough from the ones already placed.
		TryPromoteHideouts(candidates, chosen, wanted, minSeparation, midX, midZ, false, initialGarrison, maxGarrison, hqPrefab);

		// Shortfall (for example 3 of 5) uses half the radius, empty quarters first.
		float relaxedSeparation = minSeparation * 0.5;
		int beforeRelaxed = chosen.Count();
		TryPromoteHideouts(candidates, chosen, wanted, relaxedSeparation, midX, midZ, true, initialGarrison, maxGarrison, hqPrefab);
		TryPromoteHideouts(candidates, chosen, wanted, relaxedSeparation, midX, midZ, false, initialGarrison, maxGarrison, hqPrefab);

		int quarters = CountFilledHideoutQuarters(chosen, midX, midZ);
		Print(string.Format("[JOLP] Hideout selection: promoted %1 / wanted %2 (candidates %3, quarters %4/4, relaxed %5 at %6 m)",
			chosen.Count(), wanted, candidates.Count(), quarters, chosen.Count() - beforeRelaxed, relaxedSeparation), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Accept shuffled candidates until wanted is reached. emptyQuartersOnly places at most one hideout in each map quarter.
	protected void TryPromoteHideouts(notnull array<ref JOLP_LocationData> candidates, notnull array<ref JOLP_LocationData> chosen, int wanted, float separation, float midX, float midZ, bool emptyQuartersOnly, int initialGarrison, int maxGarrison, ResourceName hqPrefab)
	{
		foreach (JOLP_LocationData data : candidates)
		{
			if (chosen.Count() >= wanted)
				return;

			if (!data)
				continue;

			if (IsHideoutAlreadyChosen(data, chosen))
				continue;

			int quarter = GetHideoutMapQuarter(data.m_vPosition, midX, midZ);
			if (emptyQuartersOnly && IsHideoutQuarterFilled(quarter, chosen, midX, midZ))
				continue;

			if (IsWithinHideoutSeparation(data, chosen, separation))
				continue;

			PromoteLocationToHideout(data, initialGarrison, maxGarrison, hqPrefab);
			chosen.Insert(data);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsHideoutAlreadyChosen(notnull JOLP_LocationData candidate, notnull array<ref JOLP_LocationData> chosen)
	{
		foreach (JOLP_LocationData other : chosen)
		{
			if (other && other.m_iId == candidate.m_iId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! World XZ centre. Quarters are the four rectangles split by this point.
	protected void GetHideoutMapMidpoint(out float midX, out float midZ)
	{
		midX = 0;
		midZ = 0;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		vector worldMin;
		vector worldMax;
		world.GetBoundBox(worldMin, worldMax);
		if (worldMax[0] <= worldMin[0] || worldMax[2] <= worldMin[2])
			return;

		midX = (worldMin[0] + worldMax[0]) * 0.5;
		midZ = (worldMin[2] + worldMax[2]) * 0.5;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetHideoutMapQuarter(vector pos, float midX, float midZ)
	{
		int east = 0;
		if (pos[0] >= midX)
			east = 1;

		int north = 0;
		if (pos[2] >= midZ)
			north = 1;

		return east + (north * 2);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsHideoutQuarterFilled(int quarter, notnull array<ref JOLP_LocationData> chosen, float midX, float midZ)
	{
		foreach (JOLP_LocationData other : chosen)
		{
			if (!other)
				continue;

			if (GetHideoutMapQuarter(other.m_vPosition, midX, midZ) == quarter)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountFilledHideoutQuarters(notnull array<ref JOLP_LocationData> chosen, float midX, float midZ)
	{
		int filled = 0;
		for (int quarter = 0; quarter < 4; quarter++)
		{
			if (IsHideoutQuarterFilled(quarter, chosen, midX, midZ))
				filled++;
		}

		return filled;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsWithinHideoutSeparation(notnull JOLP_LocationData candidate, notnull array<ref JOLP_LocationData> chosen, float minSeparation)
	{
		foreach (JOLP_LocationData other : chosen)
		{
			if (!other)
				continue;

			if (vector.Distance(candidate.m_vPosition, other.m_vPosition) < minSeparation)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void PromoteLocationToHideout(notnull JOLP_LocationData data, int initialGarrison, int maxGarrison, ResourceName hqPrefab)
	{
		data.m_eType = JOLP_LocationType.HIDEOUT;
		data.m_bAllowSentry = true;
		data.m_bAllowPatrol = false;
		data.m_bAllowCheckpoint = false;
		data.m_bAllowReinforcement = false;
		data.m_fActiveGarrisonRatio = 0.5;
		data.m_iGarrisonStored = initialGarrison;
		data.m_iHideoutMaxGarrison = maxGarrison;
		data.m_bHideoutDestroyed = false;
		data.m_bActive = false;

		if (data.m_sName.IsEmpty())
			data.m_sName = string.Format("Hideout_%1", data.m_iId);
		else if (!data.m_sName.Contains("Hideout"))
			data.m_sName = "Hideout_" + data.m_sName;

		EnsureHideoutLayout(data, hqPrefab);
		SpawnHideoutLayout(data);
		SeedVehicleStock(data);

		Print(string.Format("[JOLP] Promoted '%1' to hideout (garrison %2, heavies %3)",
			data.m_sName, data.m_iGarrisonStored, data.m_iHeavyStored), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! A player already knocked a piece out. Rebuilding would put that piece back.
	protected bool HideoutLayoutDamaged(notnull JOLP_LocationData data)
	{
		if (!data.m_aHideoutPieces)
			return false;

		foreach (JOLP_HideoutPiece piece : data.m_aHideoutPieces)
		{
			if (piece && piece.m_bDestroyed)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Positions are chosen once. A continue spawns this list again, so the camp does not shuffle.
	protected void EnsureHideoutLayout(notnull JOLP_LocationData data, ResourceName sitePrefab = "")
	{
		if (data.m_bHideoutDestroyed)
			return;

		// Layout 2 is a clearing that holds the whole camp, plus a path into each gun.
		// The first version only checked a few metres under the root, so the flag and bunker sat in houses.
		if (data.m_iHideoutLayoutVersion < 2 && data.m_aHideoutPieces && !data.m_aHideoutPieces.IsEmpty())
		{
			if (HideoutLayoutDamaged(data))
				data.m_iHideoutLayoutVersion = 2;
			else
				data.m_aHideoutPieces.Clear();
		}

		if (data.m_aHideoutPieces && !data.m_aHideoutPieces.IsEmpty())
			return;

		if (sitePrefab.IsEmpty())
		{
			JOLP_MilicianManagerComponent milician = JOLP_Global.GetMilician();
			if (milician && milician.GetConfig() && !milician.GetConfig().m_rHideoutHQPrefab.IsEmpty())
				sitePrefab = milician.GetConfig().m_rHideoutHQPrefab;
			else
				sitePrefab = "{664B13AF00000300}Prefabs/Props/Military/JOLP_HideoutSite.et";
		}

		data.m_aHideoutPieces = new array<ref JOLP_HideoutPiece>();
		data.m_iHideoutLayoutVersion = 2;

		// The marker is often a house. Search the settlement for a field or a yard, off the road.
		float reach = data.m_fActivationRadius * 0.7;
		if (reach < 80)
			reach = 80;
		if (reach > 170)
			reach = 170;

		vector sitePos;
		bool placed = JOLP_WorldUtils.FindClearing(data.m_vPosition, 0, reach, 8, null, 0, 10, sitePos);
		if (!placed)
			placed = JOLP_WorldUtils.FindClearing(data.m_vPosition, 0, reach, 8, null, 0, 0, sitePos);
		if (!placed)
			placed = JOLP_WorldUtils.FindClearing(data.m_vPosition, 0, reach, 7, null, 0, 0, sitePos);

		if (!placed)
		{
			Print(string.Format("[JOLP] Hideout '%1': no clearing for the camp", data.m_sName), LogLevel.WARNING);
			return;
		}

		vector siteAngles = JOLP_WorldUtils.UprightYaw(Math.RandomFloat(0, 360));
		AddHideoutPiece(data, JOLP_HideoutPieceKind.SITE, sitePrefab, sitePos, siteAngles);

		array<vector> occupied = {};
		occupied.Insert(sitePos);
		AddHideoutNests(data, sitePos, occupied);
		AddHideoutRoadblock(data, sitePos);

		int nests = 0;
		bool roadblock = false;
		foreach (JOLP_HideoutPiece piece : data.m_aHideoutPieces)
		{
			if (!piece)
				continue;

			if (piece.m_eKind == JOLP_HideoutPieceKind.NEST)
				nests++;
			else if (piece.m_eKind == JOLP_HideoutPieceKind.ROADBLOCK)
				roadblock = true;
		}

		Print(string.Format("[JOLP] Hideout '%1' layout: camp, %2 nest(s), roadblock %3", data.m_sName, nests, roadblock), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddHideoutNests(notnull JOLP_LocationData data, vector sitePos, notnull array<vector> occupied)
	{
		array<ResourceName> prefabs = {};
		JOLP_MilicianManagerComponent milician = JOLP_Global.GetMilician();
		if (milician && milician.GetConfig() && milician.GetConfig().m_aHideoutNestPrefabs)
		{
			foreach (ResourceName prefab : milician.GetConfig().m_aHideoutNestPrefabs)
			{
				if (!prefab.IsEmpty())
					prefabs.Insert(prefab);
			}
		}

		if (prefabs.IsEmpty())
		{
			prefabs.Insert("{664B13AF00000310}Prefabs/Props/Military/JOLP_HideoutNest_NSV.et");
			prefabs.Insert("{664B13AF00000313}Prefabs/Props/Military/JOLP_HideoutNest_PKM.et");
		}

		foreach (ResourceName prefab : prefabs)
		{
			vector nestPos;
			vector nestAngles;
			// The gun faces out. The space behind it is where the milician walks in.
			if (!JOLP_WorldUtils.FindGunClearing(sitePos, 18, 85, occupied, 20, nestPos, nestAngles))
			{
				Print(string.Format("[JOLP] Hideout '%1': no open ground for a static weapon", data.m_sName), LogLevel.WARNING);
				continue;
			}

			AddHideoutPiece(data, JOLP_HideoutPieceKind.NEST, prefab, nestPos, nestAngles);
			occupied.Insert(nestPos);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Near the camp, not on the settlement rim, so a departing reinforcement is already past it.
	protected void AddHideoutRoadblock(notnull JOLP_LocationData data, vector sitePos)
	{
		ResourceName prefab = "{664B13AF00000316}Prefabs/Props/Military/JOLP_HideoutRoadblock.et";
		JOLP_MilicianManagerComponent milician = JOLP_Global.GetMilician();
		if (milician && milician.GetConfig() && !milician.GetConfig().m_rHideoutRoadblockPrefab.IsEmpty())
			prefab = milician.GetConfig().m_rHideoutRoadblockPrefab;

		vector roadPos;
		vector roadAngles;
		if (!JOLP_WorldUtils.FindNearRoadSpawn(sitePos, 12, 90, roadPos, roadAngles))
		{
			Print(string.Format("[JOLP] Hideout '%1': no road near the camp for a roadblock", data.m_sName), LogLevel.NORMAL);
			return;
		}

		roadAngles[0] = roadAngles[0] + 90;
		AddHideoutPiece(data, JOLP_HideoutPieceKind.ROADBLOCK, prefab, roadPos, roadAngles);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddHideoutPiece(notnull JOLP_LocationData data, JOLP_HideoutPieceKind kind, ResourceName prefab, vector position, vector angles)
	{
		JOLP_HideoutPiece piece = new JOLP_HideoutPiece();
		piece.m_eKind = kind;
		piece.m_sPrefab = prefab;
		piece.m_vPosition = position;
		piece.m_vAngles = angles;
		data.m_aHideoutPieces.Insert(piece);
	}

	//------------------------------------------------------------------------------------------------
	protected void UntrackSpawnedPiece(IEntity entity)
	{
		JOLP_WorldUtils.UntrackHierarchy(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns every piece that is still intact and not already in the world.
	protected int SpawnHideoutLayout(notnull JOLP_LocationData data)
	{
		if (data.m_bHideoutDestroyed || !data.m_aHideoutPieces)
			return 0;

		BaseWorld world = GetGame().GetWorld();
		int spawned = 0;
		for (int i = 0; i < data.m_aHideoutPieces.Count(); i++)
		{
			JOLP_HideoutPiece piece = data.m_aHideoutPieces[i];
			if (!piece || piece.m_bDestroyed || piece.m_sPrefab.IsEmpty())
				continue;

			if (world && piece.m_EntityId != EntityID.INVALID && world.FindEntityByID(piece.m_EntityId))
				continue;

			IEntity entity = JOLP_WorldUtils.SpawnEntityPrefab(piece.m_sPrefab, piece.m_vPosition, piece.m_vAngles, true);
			if (!entity)
			{
				Print(string.Format("[JOLP] Hideout '%1': failed to spawn %2", data.m_sName, piece.m_sPrefab), LogLevel.ERROR);
				continue;
			}

			// Children such as the flag and the tripod carry their own Persistence component.
			// The spawn helper only drops the root, so a save would bring those children back twice.
			// The flag cloth is slotted in a moment later, so the walk runs again.
			JOLP_WorldUtils.UntrackHierarchy(entity);
			GetGame().GetCallqueue().CallLater(UntrackSpawnedPiece, 1000, false, entity);
			piece.m_EntityId = entity.GetID();
			spawned++;

			if (piece.m_eKind == JOLP_HideoutPieceKind.SITE)
			{
				data.m_HQEntityId = piece.m_EntityId;
				JOLP_HideoutHQComponent hq = JOLP_HideoutHQComponent.Cast(entity.FindComponent(JOLP_HideoutHQComponent));
				if (hq)
					hq.Configure(data.m_iId);
				else
					Print(string.Format("[JOLP] Hideout '%1': camp prefab missing JOLP_HideoutHQComponent", data.m_sName), LogLevel.WARNING);
			}
			else
			{
				JOLP_HideoutPieceComponent marker = JOLP_HideoutPieceComponent.Cast(entity.FindComponent(JOLP_HideoutPieceComponent));
				if (marker)
					marker.Configure(data.m_iId, i);
				else
					Print(string.Format("[JOLP] Hideout '%1': piece prefab missing JOLP_HideoutPieceComponent", data.m_sName), LogLevel.WARNING);
			}
		}

		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	protected void RespawnSavedHideouts()
	{
		if (!m_aLocations)
			return;

		int restored = 0;
		foreach (JOLP_LocationData data : m_aLocations)
		{
			if (!data || !data.IsHideout() || data.m_bHideoutDestroyed)
				continue;

			// An older save has the hideout, but not the camp layout. Build it once, then keep it.
			EnsureHideoutLayout(data, "");
			restored = restored + SpawnHideoutLayout(data);
		}

		Print(string.Format("[JOLP] Restored %1 hideout piece(s)", restored), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Debug map pins for every live hideout. Retries until the marker manager exists.
	protected void PlaceDebugHideoutMarkers()
	{
		if (!JOLP_Debug.IsEnabled())
			return;

		if (!Replication.IsServer())
			return;

		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markerMgr)
		{
			GetGame().GetCallqueue().CallLater(PlaceDebugHideoutMarkers, 1000, false);
			return;
		}

		int placed = 0;
		array<ref JOLP_LocationData> hideouts = GetHideouts();
		foreach (JOLP_LocationData data : hideouts)
		{
			if (!data || data.m_bHideoutDestroyed)
				continue;

			if (data.m_DebugMapMarker)
				continue;

			if (CreateDebugHideoutMarker(data))
				placed++;
		}

		Print(string.Format("[JOLP] Debug: placed %1 hideout map marker(s)", placed), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected bool CreateDebugHideoutMarker(notnull JOLP_LocationData data)
	{
		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markerMgr)
			return false;

		SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
		marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		marker.SetWorldPos(data.m_vPosition[0], data.m_vPosition[2]);
		marker.SetCustomText(data.m_sName);
		// marker.SetColor(Color(1.0, 0.0, 0.0, 1.0)); // red
		marker.SetIconEntry(2); // Cross icon
		marker.SetCanBeRemovedByOwner(false);

		// Server marker so listen-host clients see it; local-only still works offline.
		bool isLocal = RplSession.Mode() == RplMode.None;
		markerMgr.InsertStaticMarker(marker, isLocal, !isLocal);

		data.m_DebugMapMarker = marker;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveDebugHideoutMarker(notnull JOLP_LocationData data)
	{
		if (!data.m_DebugMapMarker)
			return;

		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (markerMgr)
			markerMgr.RemoveStaticMarker(data.m_DebugMapMarker);

		data.m_DebugMapMarker = null;
	}

	//------------------------------------------------------------------------------------------------
	void OnHideoutHQDestroyed(int locationId)
	{
		JOLP_LocationData data = GetLocation(locationId);
		if (!data || !data.IsHideout())
			return;

		if (data.m_bHideoutDestroyed)
			return;

		data.m_bHideoutDestroyed = true;
		data.m_iGarrisonStored = 0;
		data.m_HQEntityId = EntityID.INVALID;
		MarkHideoutSiteDestroyed(data);
		RemoveDebugHideoutMarker(data);

		Print(string.Format("[JOLP] Hideout HQ destroyed at '%1' (%2 alive remain)",
			data.m_sName, GetAliveHideoutCount()), LogLevel.NORMAL);

		JOLP_MilicianManagerComponent milician = JOLP_Global.GetMilician();
		if (milician)
			milician.OnHideoutDestroyed(locationId);
	}

	//------------------------------------------------------------------------------------------------
	//! A nest or roadblock is gone. The camp stays until the ammo stack itself is destroyed.
	void OnHideoutPieceDestroyed(int locationId, int pieceIndex)
	{
		JOLP_LocationData data = GetLocation(locationId);
		if (!data)
			return;

		JOLP_HideoutPiece piece = data.GetHideoutPiece(pieceIndex);
		if (!piece || piece.m_bDestroyed)
			return;

		piece.m_bDestroyed = true;
		piece.m_EntityId = EntityID.INVALID;
		piece.m_bGunnerAssigned = false;

		Print(string.Format("[JOLP] Hideout '%1' lost a %2", data.m_sName, typename.EnumToString(JOLP_HideoutPieceKind, piece.m_eKind)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void MarkHideoutSiteDestroyed(notnull JOLP_LocationData data)
	{
		if (!data.m_aHideoutPieces)
			return;

		foreach (JOLP_HideoutPiece piece : data.m_aHideoutPieces)
		{
			if (!piece || piece.m_eKind != JOLP_HideoutPieceKind.SITE)
				continue;

			piece.m_bDestroyed = true;
			piece.m_EntityId = EntityID.INVALID;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DiscoverWorldLocations()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(
			"0 0 0",
			99999999,
			FilterLocationEntity,
			null,
			EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);
	}

	//------------------------------------------------------------------------------------------------
	//! If no authored TOWN controllers exist, build location records from map descriptors.
	//! Reliable path when the World Editor generator cannot see Kunar parent-world markers.
	protected void SeedFromMapDescriptorsIfNeeded()
	{
		int authoredTowns = 0;
		foreach (JOLP_LocationData existing : m_aLocations)
		{
			if (existing && existing.m_eType == JOLP_LocationType.TOWN)
				authoredTowns++;
		}

		if (authoredTowns > 0)
		{
			Print(string.Format("[JOLP] Skipping map-descriptor seed (%1 authored town controller(s))", authoredTowns), LogLevel.NORMAL);
			return;
		}

		m_aMapDescriptorHits = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere(
			"0 0 0",
			99999999,
			CollectMapDescriptorEntity,
			FilterMapDescriptorEntity,
			EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);

		int seeded = 0;
		int nCity = 0;
		int nTown = 0;
		int nVillage = 0;
		int nSettlement = 0;
		int nLocal = 0;
		int nOther = 0;
		foreach (IEntity entity : m_aMapDescriptorHits)
		{
			MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
			if (mapdesc)
			{
				int hitType = mapdesc.GetBaseType();
				if (hitType == EMapDescriptorType.MDT_NAME_CITY)
					nCity++;
				else if (hitType == EMapDescriptorType.MDT_NAME_TOWN)
					nTown++;
				else if (hitType == EMapDescriptorType.MDT_NAME_VILLAGE)
					nVillage++;
				else if (hitType == EMapDescriptorType.MDT_NAME_SETTLEMENT)
					nSettlement++;
				else if (hitType == EMapDescriptorType.MDT_NAME_LOCAL)
					nLocal++;
				else
					nOther++;
			}

			if (RegisterFromMapDescriptor(entity))
				seeded++;
		}

		Print(string.Format("[JOLP] Map-descriptor seed: scanned=%1 seeded=%2 (city=%3 town=%4 village=%5 settlement=%6 local=%7 other=%8)",
			m_aMapDescriptorHits.Count(), seeded, nCity, nTown, nVillage, nSettlement, nLocal, nOther), LogLevel.NORMAL);
		m_aMapDescriptorHits = null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectMapDescriptorEntity(IEntity entity)
	{
		if (entity)
			m_aMapDescriptorHits.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool FilterMapDescriptorEntity(IEntity entity)
	{
		return JOLP_WorldUtils.IsSettlementMapDescriptor(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected bool RegisterFromMapDescriptor(IEntity entity)
	{
		if (!entity)
			return false;

		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (!mapdesc)
			mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(SCR_MapDescriptorComponent));
		if (!mapdesc)
			return false;

		vector origin = entity.GetOrigin();
		foreach (JOLP_LocationData existing : m_aLocations)
		{
			if (existing && vector.Distance(existing.m_vPosition, origin) < 100)
				return false;
		}

		int type = mapdesc.GetBaseType();
		JOLP_SettlementSize size = JOLP_SettlementSize.TOWN;
		float radius = 400;
		int garrison = 20;

		// Kunar tags many hamlets as LOCAL / SETTLEMENT, not VILLAGE.
		if (type == EMapDescriptorType.MDT_NAME_VILLAGE
			|| type == EMapDescriptorType.MDT_NAME_SETTLEMENT
			|| type == EMapDescriptorType.MDT_NAME_LOCAL
			|| type == EMapDescriptorType.MDT_NAME_GENERIC)
		{
			size = JOLP_SettlementSize.VILLAGE;
			radius = 250;
			garrison = 20;
		}
		else if (type == EMapDescriptorType.MDT_NAME_CITY)
		{
			size = JOLP_SettlementSize.CITY;
			radius = 600;
			garrison = 40;
		}

		string displayName = JOLP_WorldUtils.ResolveMapDescriptorName(
			mapdesc, entity, string.Format("Town_%1", m_aLocations.Count()));

		JOLP_LocationData data = new JOLP_LocationData();
		data.m_iId = m_aLocations.Count();
		data.m_sName = displayName;
		data.m_eType = JOLP_LocationType.TOWN;
		data.m_eSettlementSize = size;
		data.m_vPosition = origin;
		data.m_fActivationRadius = radius;
		data.m_iGarrisonStored = garrison;
		data.m_fActiveGarrisonRatio = 0.65;
		data.m_iMaxActiveOverride = -1;
		data.m_bAllowSentry = true;
		data.m_bAllowPatrol = true;
		data.m_bAllowCheckpoint = true;
		data.m_bAllowReinforcement = true;
		data.m_bActive = false;
		data.m_iCivilianBudget = -1;
		data.m_iCivilianStored = 0;
		data.m_iVehicleSlots = 0;
		data.m_iCivilianFootOverride = -1;
		data.m_iCivilianVehicleOverride = -1;
		data.m_fReputation = 0;
		data.m_bIEDInitialized = false;
		data.m_iIEDInitialCount = 0;
		AssignPersistentKey(data);
		SeedVehicleStock(data);

		m_aLocations.Insert(data);
		m_aControllers.Insert(null);

		Print(string.Format("[JOLP] Seeded %1 '%2' (desc=%3) at %4",
			typename.EnumToString(JOLP_SettlementSize, size),
			data.m_sName,
			typename.EnumToString(EMapDescriptorType, type),
			data.m_vPosition.ToString()), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool FilterLocationEntity(IEntity entity)
	{
		if (!entity)
			return true;

		JOLP_LocationControllerComponent controller = JOLP_LocationControllerComponent.Cast(
			entity.FindComponent(JOLP_LocationControllerComponent));
		if (controller)
			RegisterLocation(controller);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void DiscoverWorldFOBs()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(
			"0 0 0",
			99999999,
			FilterFOBEntity,
			null,
			EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);
	}

	//------------------------------------------------------------------------------------------------
	protected bool FilterFOBEntity(IEntity entity)
	{
		if (!entity)
			return true;

		JOLP_FOBControllerComponent fob = JOLP_FOBControllerComponent.Cast(
			entity.FindComponent(JOLP_FOBControllerComponent));
		if (!fob)
			return true;

		// Starter / marker FOBs are live as soon as they exist. Do not wait for OnPostInit —
		// composition roots can skip that path, and discovery runs during game-mode Init.
		if (!fob.IsRegistrationDeferred())
		{
			fob.ArmAndRegister();
			return true;
		}

		if (fob.IsArmed())
			RegisterFOB(fob);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	int RegisterLocation(JOLP_LocationControllerComponent controller)
	{
		if (!controller)
			return -1;

		if (!m_aLocations)
			m_aLocations = new array<ref JOLP_LocationData>();
		if (!m_aControllers)
			m_aControllers = new array<JOLP_LocationControllerComponent>();

		int existing = m_aControllers.Find(controller);
		if (existing >= 0)
			return existing;

		JOLP_LocationData data = new JOLP_LocationData();
		data.m_iId = m_aLocations.Count();
		data.m_sName = controller.GetLocationName();
		data.m_eType = controller.GetLocationType();
		data.m_eSettlementSize = controller.GetSettlementSize();
		data.m_vPosition = controller.GetPosition();
		data.m_fActivationRadius = controller.GetActivationRadius();
		data.m_iGarrisonStored = Math.Max(0, controller.GetInitialGarrison());
		data.m_fActiveGarrisonRatio = controller.GetActiveGarrisonRatio();
		data.m_iMaxActiveOverride = controller.GetMaxActiveOverride();
		data.m_bAllowSentry = controller.GetAllowSentry();
		data.m_bAllowPatrol = controller.GetAllowPatrol();
		data.m_bAllowCheckpoint = controller.GetAllowCheckpoint();
		data.m_bAllowReinforcement = controller.GetAllowReinforcement();
		data.m_iHideoutReinforceIntervalMs = controller.GetHideoutReinforceIntervalMs();
		data.m_iHideoutReinforceGroupSize = controller.GetHideoutReinforceGroupSize();
		data.m_fHideoutReinforceSearchRange = controller.GetHideoutReinforceSearchRange();
		data.m_iHideoutDefenseReserve = controller.GetHideoutDefenseReserve();
		data.m_bActive = false;
		data.m_fHideoutNextReinforceWorldTime = 0;
		data.m_iCivilianBudget = -1;
		data.m_iCivilianStored = 0;
		data.m_iVehicleSlots = 0;
		data.m_iCivilianFootOverride = controller.GetCivilianFootOverride();
		data.m_iCivilianVehicleOverride = controller.GetCivilianVehicleOverride();
		data.m_fReputation = 0;
		data.m_bIEDInitialized = false;
		data.m_iIEDInitialCount = 0;

		if (data.m_sName.IsEmpty())
			data.m_sName = string.Format("Location_%1", data.m_iId);

		AssignPersistentKey(data);
		SeedVehicleStock(data);

		m_aLocations.Insert(data);
		m_aControllers.Insert(controller);
		controller.SetLocationId(data.m_iId);

		RefreshFOBSuppression();

		Print(string.Format("[JOLP] Registered location %1 '%2' (%3) at %4",
			data.m_iId, data.m_sName, typename.EnumToString(JOLP_LocationType, data.m_eType), data.m_vPosition.ToString()), LogLevel.NORMAL);

		return data.m_iId;
	}

	//------------------------------------------------------------------------------------------------
	void UnregisterLocation(JOLP_LocationControllerComponent controller)
	{
		if (!controller || !m_aControllers)
			return;

		int idx = m_aControllers.Find(controller);
		if (idx < 0)
			return;

		m_aControllers.Remove(idx);
		if (m_aLocations && idx < m_aLocations.Count())
			m_aLocations.Remove(idx);

		for (int i = 0; i < m_aLocations.Count(); i++)
		{
			m_aLocations[i].m_iId = i;
			if (m_aControllers[i])
				m_aControllers[i].SetLocationId(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	protected void AssignPersistentKey(notnull JOLP_LocationData data)
	{
		if (!data.m_sPersistentKey.IsEmpty())
			return;

		data.m_sPersistentKey = data.m_sName;
	}

	//------------------------------------------------------------------------------------------------
	JOLP_LocationData FindByPersistentKey(string key)
	{
		if (key.IsEmpty() || !m_aLocations)
			return null;

		foreach (JOLP_LocationData data : m_aLocations)
		{
			if (data && data.m_sPersistentKey == key)
				return data;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Living men drawn from this place's pool. QRF and free hideout waves were never charged.
	int GetFieldedGarrison(int locationId)
	{
		JOLP_MilicianManagerComponent milician = JOLP_Global.GetMilician();
		if (!milician)
			return 0;

		return milician.GetFieldedGarrison(locationId);
	}

	//------------------------------------------------------------------------------------------------
	int GetFieldedFootCivilians(int locationId)
	{
		JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
		if (!civilians)
			return 0;

		return civilians.GetFieldedFootCount(locationId);
	}

	//------------------------------------------------------------------------------------------------
	int GetFieldedVehicles(int locationId, JOLP_VehicleClass kind)
	{
		JOLP_MilicianManagerComponent milician = JOLP_Global.GetMilician();
		if (!milician)
			return 0;

		return milician.GetFieldedVehicles(locationId, kind);
	}

	//------------------------------------------------------------------------------------------------
	protected void SeedVehicleStock(notnull JOLP_LocationData data)
	{
		JOLP_MilicianGarrisonConfig config;
		JOLP_MilicianManagerComponent milician = JOLP_Global.GetMilician();
		if (milician)
			config = milician.GetConfig();

		if (config)
			config.ApplyDefaultVehicleStock(data);
		else
			ApplyBuiltinVehicleStock(data);
	}

	//------------------------------------------------------------------------------------------------
	//! Used when the milician config is not loaded yet. Matches the config defaults.
	protected void ApplyBuiltinVehicleStock(notnull JOLP_LocationData data)
	{
		data.SetVehicleStock(JOLP_VehicleClass.TRANSPORT, 0);
		data.SetVehicleStock(JOLP_VehicleClass.LIGHT, 0);
		data.SetVehicleStock(JOLP_VehicleClass.HEAVY, 0);

		if (data.IsHideout())
		{
			data.SetVehicleStock(JOLP_VehicleClass.HEAVY, 2);
			return;
		}

		if (data.IsOP())
			return;

		if (data.m_eSettlementSize == JOLP_SettlementSize.CITY)
			data.SetVehicleStock(JOLP_VehicleClass.TRANSPORT, 1);
		else if (data.m_eSettlementSize == JOLP_SettlementSize.TOWN)
		{
			data.SetVehicleStock(JOLP_VehicleClass.TRANSPORT, 2);
			data.SetVehicleStock(JOLP_VehicleClass.LIGHT, 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a save back onto locations that already exist. Does not create towns.
	void ApplyPersistedLocations(array<ref JOLP_PersistedLocation> records)
	{
		if (!records)
			return;

		foreach (JOLP_PersistedLocation record : records)
		{
			if (!record || record.m_sPersistentKey.IsEmpty())
				continue;

			JOLP_LocationData data = FindByPersistentKey(record.m_sPersistentKey);
			if (!data)
			{
				Print(string.Format("[JOLP] Saved location '%1' has no controller and was skipped", record.m_sPersistentKey), LogLevel.WARNING);
				continue;
			}

			data.m_bRestoredFromSave = true;
			data.m_eType = record.m_eType;
			data.m_eSettlementSize = record.m_eSettlementSize;
			data.m_fActiveGarrisonRatio = record.m_fActiveGarrisonRatio;
			data.m_iMaxActiveOverride = record.m_iMaxActiveOverride;
			data.m_bAllowSentry = record.m_bAllowSentry;
			data.m_bAllowPatrol = record.m_bAllowPatrol;
			data.m_bAllowCheckpoint = record.m_bAllowCheckpoint;
			data.m_bAllowReinforcement = record.m_bAllowReinforcement;
			data.m_bHideoutDestroyed = record.m_bHideoutDestroyed;
			data.m_iHideoutMaxGarrison = record.m_iHideoutMaxGarrison;
			data.m_fReputation = record.m_fReputation;
			data.m_iCivilianBudget = record.m_iCivilianBudget;
			data.m_iVehicleSlots = record.m_iVehicleSlots;
			data.m_bIEDInitialized = record.m_bIEDInitialized;
			data.m_iIEDInitialCount = record.m_iIEDInitialCount;

			if (data.IsHideout() && !data.m_sName.Contains("Hideout"))
				data.m_sName = "Hideout_" + data.m_sName;

			int garrison = record.m_iGarrisonTotal - GetFieldedGarrison(data.m_iId);
			if (garrison < 0)
				garrison = 0;
			data.m_iGarrisonStored = garrison;

			int civilians = record.m_iCivilianStoredTotal - GetFieldedFootCivilians(data.m_iId);
			if (civilians < 0)
				civilians = 0;
			data.m_iCivilianStored = civilians;

			// Version 1 has no vehicle pools. Grant the defaults once the restored type is known.
			SeedVehicleStock(data);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Version 2. Vehicle totals include hulls that were on the road when the save was written.
	void ApplyPersistedLocationsV2(array<ref JOLP_PersistedLocationV2> records)
	{
		if (!records)
			return;

		foreach (JOLP_PersistedLocationV2 record : records)
		{
			if (!record || record.m_sPersistentKey.IsEmpty())
				continue;

			JOLP_LocationData data = FindByPersistentKey(record.m_sPersistentKey);
			if (!data)
			{
				Print(string.Format("[JOLP] Saved location '%1' has no controller and was skipped", record.m_sPersistentKey), LogLevel.WARNING);
				continue;
			}

			data.m_bRestoredFromSave = true;
			data.m_eType = record.m_eType;
			data.m_eSettlementSize = record.m_eSettlementSize;
			data.m_fActiveGarrisonRatio = record.m_fActiveGarrisonRatio;
			data.m_iMaxActiveOverride = record.m_iMaxActiveOverride;
			data.m_bAllowSentry = record.m_bAllowSentry;
			data.m_bAllowPatrol = record.m_bAllowPatrol;
			data.m_bAllowCheckpoint = record.m_bAllowCheckpoint;
			data.m_bAllowReinforcement = record.m_bAllowReinforcement;
			data.m_bHideoutDestroyed = record.m_bHideoutDestroyed;
			data.m_iHideoutMaxGarrison = record.m_iHideoutMaxGarrison;
			data.m_fReputation = record.m_fReputation;
			data.m_iCivilianBudget = record.m_iCivilianBudget;
			data.m_iVehicleSlots = record.m_iVehicleSlots;
			data.m_bIEDInitialized = record.m_bIEDInitialized;
			data.m_iIEDInitialCount = record.m_iIEDInitialCount;

			if (data.IsHideout() && !data.m_sName.Contains("Hideout"))
				data.m_sName = "Hideout_" + data.m_sName;

			int garrison = record.m_iGarrisonTotal - GetFieldedGarrison(data.m_iId);
			if (garrison < 0)
				garrison = 0;
			data.m_iGarrisonStored = garrison;

			int civilians = record.m_iCivilianStoredTotal - GetFieldedFootCivilians(data.m_iId);
			if (civilians < 0)
				civilians = 0;
			data.m_iCivilianStored = civilians;

			int transport = record.m_iTransportTotal - GetFieldedVehicles(data.m_iId, JOLP_VehicleClass.TRANSPORT);
			int light = record.m_iLightTotal - GetFieldedVehicles(data.m_iId, JOLP_VehicleClass.LIGHT);
			int heavy = record.m_iHeavyTotal - GetFieldedVehicles(data.m_iId, JOLP_VehicleClass.HEAVY);
			data.SetVehicleStock(JOLP_VehicleClass.TRANSPORT, transport);
			data.SetVehicleStock(JOLP_VehicleClass.LIGHT, light);
			data.SetVehicleStock(JOLP_VehicleClass.HEAVY, heavy);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Version 3 adds the hideout camp layout. Vehicle pools still come from the version 2 path.
	void ApplyPersistedLocationsV3(array<ref JOLP_PersistedLocationV3> records, array<int> layoutVersions)
	{
		if (!records)
			return;

		array<ref JOLP_PersistedLocationV2> legacy = new array<ref JOLP_PersistedLocationV2>();
		foreach (JOLP_PersistedLocationV3 record : records)
		{
			if (!record)
				continue;

			JOLP_PersistedLocationV2 copy = new JOLP_PersistedLocationV2();
			copy.m_sPersistentKey = record.m_sPersistentKey;
			copy.m_vPosition = record.m_vPosition;
			copy.m_eType = record.m_eType;
			copy.m_eSettlementSize = record.m_eSettlementSize;
			copy.m_iGarrisonTotal = record.m_iGarrisonTotal;
			copy.m_fActiveGarrisonRatio = record.m_fActiveGarrisonRatio;
			copy.m_iMaxActiveOverride = record.m_iMaxActiveOverride;
			copy.m_bAllowSentry = record.m_bAllowSentry;
			copy.m_bAllowPatrol = record.m_bAllowPatrol;
			copy.m_bAllowCheckpoint = record.m_bAllowCheckpoint;
			copy.m_bAllowReinforcement = record.m_bAllowReinforcement;
			copy.m_bHideoutDestroyed = record.m_bHideoutDestroyed;
			copy.m_iHideoutMaxGarrison = record.m_iHideoutMaxGarrison;
			copy.m_fReputation = record.m_fReputation;
			copy.m_iCivilianBudget = record.m_iCivilianBudget;
			copy.m_iCivilianStoredTotal = record.m_iCivilianStoredTotal;
			copy.m_iVehicleSlots = record.m_iVehicleSlots;
			copy.m_bIEDInitialized = record.m_bIEDInitialized;
			copy.m_iIEDInitialCount = record.m_iIEDInitialCount;
			copy.m_iTransportTotal = record.m_iTransportTotal;
			copy.m_iLightTotal = record.m_iLightTotal;
			copy.m_iHeavyTotal = record.m_iHeavyTotal;
			legacy.Insert(copy);
		}

		ApplyPersistedLocationsV2(legacy);

		for (int i = 0; i < records.Count(); i++)
		{
			JOLP_PersistedLocationV3 record = records[i];
			if (!record || record.m_sPersistentKey.IsEmpty())
				continue;

			JOLP_LocationData data = FindByPersistentKey(record.m_sPersistentKey);
			if (!data)
				continue;

			ApplyHideoutPieces(data, record.m_aHideoutPieces);
			if (layoutVersions && i < layoutVersions.Count())
				data.m_iHideoutLayoutVersion = layoutVersions[i];
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A live reapply keeps the entities already in the world. A fresh load has none yet.
	protected void ApplyHideoutPieces(notnull JOLP_LocationData data, array<ref JOLP_PersistedHideoutPiece> saved)
	{
		if (!saved)
			return;

		if (data.m_aHideoutPieces && data.m_aHideoutPieces.Count() == saved.Count() && saved.Count() > 0)
		{
			bool samePlace = true;
			for (int i = 0; i < saved.Count(); i++)
			{
				JOLP_PersistedHideoutPiece savedPiece = saved[i];
				JOLP_HideoutPiece live = data.m_aHideoutPieces[i];
				if (!savedPiece || !live || vector.Distance(live.m_vPosition, savedPiece.m_vPosition) > 1)
					samePlace = false;
			}

			if (samePlace)
			{
				for (int i = 0; i < saved.Count(); i++)
				{
					if (saved[i])
						data.m_aHideoutPieces[i].m_bDestroyed = saved[i].m_bDestroyed;
				}

				return;
			}
		}

		data.m_aHideoutPieces = new array<ref JOLP_HideoutPiece>();
		foreach (JOLP_PersistedHideoutPiece savedPiece : saved)
		{
			if (!savedPiece)
				continue;

			JOLP_HideoutPiece piece = new JOLP_HideoutPiece();
			piece.m_eKind = savedPiece.m_eKind;
			piece.m_sPrefab = savedPiece.m_sPrefab;
			piece.m_vPosition = savedPiece.m_vPosition;
			piece.m_vAngles = savedPiece.m_vAngles;
			piece.m_bDestroyed = savedPiece.m_bDestroyed;
			data.m_aHideoutPieces.Insert(piece);
		}
	}

	//------------------------------------------------------------------------------------------------
	JOLP_LocationData GetLocation(int locationId)
	{
		if (!m_aLocations || locationId < 0 || locationId >= m_aLocations.Count())
			return null;

		return m_aLocations[locationId];
	}

	//------------------------------------------------------------------------------------------------
	JOLP_LocationControllerComponent GetController(int locationId)
	{
		if (!m_aControllers || locationId < 0 || locationId >= m_aControllers.Count())
			return null;

		return m_aControllers[locationId];
	}

	//------------------------------------------------------------------------------------------------
	array<ref JOLP_LocationData> GetAllLocations()
	{
		return m_aLocations;
	}

	//------------------------------------------------------------------------------------------------
	int GetLocationCount()
	{
		if (!m_aLocations)
			return 0;

		return m_aLocations.Count();
	}

	//------------------------------------------------------------------------------------------------
	array<ref JOLP_LocationData> GetLocationsByType(JOLP_LocationType locationType)
	{
		array<ref JOLP_LocationData> result = new array<ref JOLP_LocationData>();
		if (!m_aLocations)
			return result;

		foreach (JOLP_LocationData data : m_aLocations)
		{
			if (data.m_eType == locationType)
				result.Insert(data);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	array<ref JOLP_LocationData> GetHideouts()
	{
		return GetLocationsByType(JOLP_LocationType.HIDEOUT);
	}

	//------------------------------------------------------------------------------------------------
	//! Alive = hideout whose HQ has not been destroyed (win-condition hook).
	int GetAliveHideoutCount()
	{
		int count = 0;
		array<ref JOLP_LocationData> hideouts = GetHideouts();
		foreach (JOLP_LocationData data : hideouts)
		{
			if (data && !data.m_bHideoutDestroyed)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	JOLP_LocationData GetNearestLocation(vector position)
	{
		if (!m_aLocations || m_aLocations.Count() == 0)
			return null;

		JOLP_LocationData nearest;
		float bestDist = float.MAX;
		foreach (JOLP_LocationData data : m_aLocations)
		{
			float dist = vector.Distance(position, data.m_vPosition);
			if (dist < bestDist)
			{
				bestDist = dist;
				nearest = data;
			}
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	//! Weakest eligible TOWN/OP within range (never hideout / FOB-suppressed). Ties broken by distance.
	JOLP_LocationData FindWeakestSettlement(vector fromPos, float maxRange, int excludeLocationId = -1)
	{
		if (!m_aLocations)
			return null;

		JOLP_LocationData best;
		int bestGarrison = 999999;
		float bestDist = 999999;

		foreach (JOLP_LocationData data : m_aLocations)
		{
			if (!data || !data.IsMilicianEligible())
				continue;

			if (data.m_iId == excludeLocationId)
				continue;

			float dist = vector.Distance(fromPos, data.m_vPosition);
			if (dist > maxRange)
				continue;

			if (data.m_iGarrisonStored < bestGarrison || (data.m_iGarrisonStored == bestGarrison && dist < bestDist))
			{
				bestGarrison = data.m_iGarrisonStored;
				bestDist = dist;
				best = data;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	void RegisterFOB(JOLP_FOBControllerComponent fob)
	{
		if (!fob)
			return;

		if (!m_aFOBs)
			m_aFOBs = new array<JOLP_FOBControllerComponent>();

		if (m_aFOBs.Find(fob) >= 0)
		{
			RefreshFOBSuppression();
			return;
		}

		m_aFOBs.Insert(fob);
		RefreshFOBSuppression();

		Print(string.Format("[JOLP] Registered FOB '%1' radius=%2 at %3",
			fob.GetFOBName(), fob.GetSuppressionRadius(), fob.GetPosition().ToString()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	void UnregisterFOB(JOLP_FOBControllerComponent fob)
	{
		if (!fob || !m_aFOBs)
			return;

		int idx = m_aFOBs.Find(fob);
		if (idx < 0)
			return;

		m_aFOBs.Remove(idx);
		RefreshFOBSuppression();
		fob.ClearFactionMapMarker();

		Print(string.Format("[JOLP] Unregistered FOB '%1'", fob.GetFOBName()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	int GetFOBCount()
	{
		if (!m_aFOBs)
			return 0;

		return m_aFOBs.Count();
	}

	//------------------------------------------------------------------------------------------------
	array<JOLP_FOBControllerComponent> GetFOBs()
	{
		return m_aFOBs;
	}

	//------------------------------------------------------------------------------------------------
	//! Mark settlements whose centre lies inside any FOB suppression radius.
	void RefreshFOBSuppression()
	{
		if (!m_aLocations)
			return;

		foreach (JOLP_LocationData data : m_aLocations)
		{
			if (!data)
				continue;

			bool wasSuppressed = data.m_bSuppressedByFOB;
			data.m_bSuppressedByFOB = false;

			if (!data.IsSettlement())
				continue;

			if (!m_aFOBs)
				continue;

			foreach (JOLP_FOBControllerComponent fob : m_aFOBs)
			{
				if (!fob)
					continue;

				float radius = fob.GetSuppressionRadius();
				if (radius <= 0)
					continue;

				if (vector.Distance(data.m_vPosition, fob.GetPosition()) <= radius)
				{
					data.m_bSuppressedByFOB = true;
					break;
				}
			}

			if (data.m_bSuppressedByFOB && !wasSuppressed)
			{
				Print(string.Format("[JOLP] Location '%1' suppressed by FOB", data.m_sName), LogLevel.NORMAL);
				JOLP_IEDManagerComponent ieds = JOLP_Global.GetIEDs();
				if (ieds)
					ieds.ClearChargesAtLocation(data.m_iId);
			}
			else if (!data.m_bSuppressedByFOB && wasSuppressed)
			{
				Print(string.Format("[JOLP] Location '%1' no longer FOB-suppressed", data.m_sName), LogLevel.NORMAL);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	bool IsPositionInsideFOB(vector position)
	{
		if (!m_aFOBs)
			return false;

		foreach (JOLP_FOBControllerComponent fob : m_aFOBs)
		{
			if (!fob)
				continue;

			float radius = fob.GetSuppressionRadius();
			if (radius > 0 && vector.Distance(position, fob.GetPosition()) <= radius)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the whole disc sits inside one player FOB.
	bool IsAreaInsideFOB(vector center, float radius)
	{
		if (!m_aFOBs || radius < 0)
			return false;

		foreach (JOLP_FOBControllerComponent fob : m_aFOBs)
		{
			if (!fob)
				continue;

			float fobRadius = fob.GetSuppressionRadius();
			if (fobRadius <= 0)
				continue;

			if (vector.Distance(center, fob.GetPosition()) + radius <= fobRadius)
				return true;
		}

		return false;
	}
}
