#ifdef WORKBENCH
//------------------------------------------------------------------------------------------------
//! Shared generator. Prefer Plugins → JOLP Generate Town Locations (Ctrl+Shift+G).
//! If this still fails in World Editor, towns are also auto-seeded at mission start by
//! JOLP_LocationManagerComponent when no authored town controllers exist.
class JOLP_GenerateTownLocationsRunner
{
	static const ResourceName DEFAULT_LOCATION_PREFAB = "{664B13AF00000001}Prefabs/Controllers/JOLP_LocationController.et";
	static const float DEFAULT_DUPLICATE_RADIUS = 100;

	//------------------------------------------------------------------------------------------------
	static void Run(ResourceName locationPrefab, float duplicateRadius)
	{
		Print("[JOLP] GenerateTownLocations: starting…", LogLevel.NORMAL);

		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		WorldEditorAPI api = null;
		if (worldEditor)
			api = worldEditor.GetApi();
		if (!api)
			api = SCR_WorldEditorToolHelper.GetWorldEditorAPI();

		if (!api)
		{
			Print("[JOLP] GenerateTownLocations: no WorldEditorAPI (is World Editor the active module?)", LogLevel.ERROR);
			return;
		}

		Print(string.Format("[JOLP]   IsPrefabEditMode=%1 IsWorldLoaded=%2 IsEditMode=%3",
			api.IsPrefabEditMode(),
			SCR_WorldEditorToolHelper.IsWorldLoaded(),
			api.IsEditMode()), LogLevel.NORMAL);

		if (api.IsPrefabEditMode())
		{
			Print("[JOLP] GenerateTownLocations: close Prefab Edit mode, File→Open JustOneLastPatrol_kunar.ent", LogLevel.ERROR);
			return;
		}

		BaseWorld world = api.GetWorld();
		string worldPath = SCR_WorldEditorToolHelper.GetWorldPath();
		Print(string.Format("[JOLP]   GetWorld()=%1 path='%2' name='%3'",
			world != null,
			worldPath,
			SCR_WorldEditorToolHelper.GetWorldName()), LogLevel.NORMAL);

		if (!world)
		{
			Print("[JOLP] GenerateTownLocations: api.GetWorld() is null. Use Play mode auto-seed instead (host the mission).", LogLevel.ERROR);
			return;
		}

		if (locationPrefab.IsEmpty())
			locationPrefab = DEFAULT_LOCATION_PREFAB;
		if (duplicateRadius <= 0)
			duplicateRadius = DEFAULT_DUPLICATE_RADIUS;

		ref array<ref JOLP_TownSeedCandidate> candidates = new array<ref JOLP_TownSeedCandidate>();
		CollectCandidatesFromWorldQuery(world, candidates);
		CollectCandidatesFromEditorEntities(api, candidates);

		Print(string.Format("[JOLP]   unique settlement candidates=%1", candidates.Count()), LogLevel.NORMAL);
		if (candidates.IsEmpty())
		{
			Print("[JOLP] GenerateTownLocations: no CITY/TOWN/VILLAGE/SETTLEMENT descriptors found. Host the mission — LocationManager will auto-seed at runtime.", LogLevel.WARNING);
			return;
		}

		ref array<vector> createdPositions = new array<vector>();
		int created = 0;
		int skippedDup = 0;

		api.BeginEntityAction("JOLP Generate Town Locations");

		foreach (JOLP_TownSeedCandidate candidate : candidates)
		{
			if (!candidate)
				continue;

			if (IsNearAny(candidate.m_vPosition, duplicateRadius, createdPositions) || HasControllerNearby(world, candidate.m_vPosition, duplicateRadius))
			{
				skippedDup++;
				continue;
			}

			string entityName = JOLP_WorldUtils.SanitizeEntityName(candidate.m_sName);
			IEntitySource entSrc = api.CreateEntity(locationPrefab, entityName, api.GetCurrentEntityLayerId(), null, candidate.m_vPosition, vector.Zero);
			if (!entSrc)
			{
				Print(string.Format("[JOLP] Failed to create controller at %1", candidate.m_vPosition.ToString()), LogLevel.WARNING);
				continue;
			}

			array<ref ContainerIdPathEntry> path = { new ContainerIdPathEntry("JOLP_LocationControllerComponent") };
			api.SetVariableValue(entSrc, path, "m_sName", candidate.m_sName);
			api.SetVariableValue(entSrc, path, "m_eType", "0");
			api.SetVariableValue(entSrc, path, "m_eSettlementSize", candidate.m_iSize.ToString());
			api.SetVariableValue(entSrc, path, "m_fActivationRadius", candidate.m_fRadius.ToString());
			api.SetVariableValue(entSrc, path, "m_iInitialGarrison", candidate.m_iGarrison.ToString());
			api.SetVariableValue(entSrc, path, "m_fActiveGarrisonRatio", "0.65");
			api.SetVariableValue(entSrc, path, "m_bAllowSentry", "1");
			api.SetVariableValue(entSrc, path, "m_bAllowPatrol", "1");
			api.SetVariableValue(entSrc, path, "m_bAllowCheckpoint", "1");
			api.SetVariableValue(entSrc, path, "m_bAllowReinforcement", "1");

			createdPositions.Insert(candidate.m_vPosition);
			created++;
		}

		api.EndEntityAction();
		Print(string.Format("[JOLP] GenerateTownLocations: created=%1 skippedDup=%2. Save the active layer (AI_SpawnArea).", created, skippedDup), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectCandidatesFromWorldQuery(BaseWorld world, notnull array<ref JOLP_TownSeedCandidate> outCandidates)
	{
		array<IEntity> hits = SCR_WorldEditorToolHelper.QueryEntitiesBySphere(world, "0 0 0", 99999999, EQueryEntitiesFlags.STATIC);
		if (!hits || hits.IsEmpty())
		{
			vector dims = SCR_WorldEditorToolHelper.GetTerrainDimensions();
			if (dims[0] > 0 && dims[2] > 0)
				hits = SCR_WorldEditorToolHelper.QueryEntitiesByAABB(world, Vector(0, -10000, 0), Vector(dims[0], 10000, dims[2]), EQueryEntitiesFlags.STATIC);
		}

		if (!hits)
			return;

		Print(string.Format("[JOLP]   world query entities=%1", hits.Count()), LogLevel.NORMAL);
		foreach (IEntity ent : hits)
			TryAddCandidateFromEntity(ent, outCandidates);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectCandidatesFromEditorEntities(WorldEditorAPI api, notnull array<ref JOLP_TownSeedCandidate> outCandidates)
	{
		int editorCount = api.GetEditorEntityCount();
		Print(string.Format("[JOLP]   editor entities=%1 subScenes=%2", editorCount, api.GetNumSubScenes()), LogLevel.NORMAL);

		for (int i = 0; i < editorCount; i++)
		{
			IEntitySource src = api.GetEditorEntity(i);
			if (!src)
				continue;

			IEntity ent = api.SourceToEntity(src);
			TryAddCandidateFromEntity(ent, outCandidates);
		}

		int scenes = api.GetNumSubScenes();
		for (int s = 0; s < scenes; s++)
		{
			int n = api.GetEntityCount(s);
			for (int i = 0; i < n; i++)
			{
				IEntitySource src = api.GetEntity(s, i);
				if (!src)
					continue;

				IEntity ent = api.SourceToEntity(src);
				TryAddCandidateFromEntity(ent, outCandidates);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void TryAddCandidateFromEntity(IEntity ent, notnull array<ref JOLP_TownSeedCandidate> outCandidates)
	{
		if (!ent)
			return;

		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(ent.FindComponent(MapDescriptorComponent));
		if (!mapdesc)
			mapdesc = MapDescriptorComponent.Cast(ent.FindComponent(SCR_MapDescriptorComponent));
		if (!mapdesc)
			return;

		int type = mapdesc.GetBaseType();
		int size = -1;
		float radius = 400;
		int garrison = 20;

		if (type == EMapDescriptorType.MDT_NAME_VILLAGE
			|| type == EMapDescriptorType.MDT_NAME_SETTLEMENT
			|| type == EMapDescriptorType.MDT_NAME_LOCAL
			|| type == EMapDescriptorType.MDT_NAME_GENERIC)
		{
			size = 0; // VILLAGE
			radius = 250;
			garrison = 20;
		}
		else if (type == EMapDescriptorType.MDT_NAME_TOWN)
		{
			size = 1;
			radius = 400;
			garrison = 20;
		}
		else if (type == EMapDescriptorType.MDT_NAME_CITY)
		{
			size = 2;
			radius = 600;
			garrison = 40;
		}
		else
		{
			return;
		}

		vector origin = ent.GetOrigin();
		string displayName = JOLP_WorldUtils.ResolveMapDescriptorName(
			mapdesc, ent, string.Format("Town_%1", outCandidates.Count()));

		// De-dupe by position
		foreach (JOLP_TownSeedCandidate existing : outCandidates)
		{
			if (existing && vector.Distance(existing.m_vPosition, origin) < 50)
				return;
		}

		JOLP_TownSeedCandidate candidate = new JOLP_TownSeedCandidate();
		candidate.m_sName = displayName;
		candidate.m_vPosition = origin;
		candidate.m_iSize = size;
		candidate.m_fRadius = radius;
		candidate.m_iGarrison = garrison;
		outCandidates.Insert(candidate);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsNearAny(vector origin, float radius, array<vector> positions)
	{
		foreach (vector pos : positions)
		{
			if (vector.Distance(pos, origin) <= radius)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasControllerNearby(BaseWorld world, vector origin, float radius)
	{
		array<IEntity> nearby = SCR_WorldEditorToolHelper.QueryEntitiesBySphere(world, origin, radius, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);
		if (!nearby)
			return false;

		foreach (IEntity entity : nearby)
		{
			if (entity && entity.FindComponent(JOLP_LocationControllerComponent))
				return true;
		}
		return false;
	}
}

//------------------------------------------------------------------------------------------------
class JOLP_TownSeedCandidate : Managed
{
	string m_sName;
	vector m_vPosition;
	int m_iSize;
	float m_fRadius;
	int m_iGarrison;
}

//------------------------------------------------------------------------------------------------
[WorkbenchToolAttribute(
	name: "JOLP Generate Town Locations",
	description: "Generate JOLP_LocationController from map descriptors.\nPrefer Plugins → JOLP Generate Town Locations (Ctrl+Shift+G).\nIf World Editor fails, host the mission — runtime auto-seed will create towns.",
	wbModules: { "WorldEditor" },
	awesomeFontCode: 0xf279)]
class JOLP_GenerateTownLocationsTool : WorldEditorTool
{
	[Attribute("{664B13AF00000001}Prefabs/Controllers/JOLP_LocationController.et", UIWidgets.ResourceNamePicker, "Location controller prefab", "et")]
	protected ResourceName m_rLocationPrefab;

	[Attribute("100", desc: "Skip descriptors that already have a JOLP location within this distance (m)")]
	protected float m_fDuplicateRadius;

	[ButtonAttribute("Generate Town Locations")]
	void GenerateTownLocations()
	{
		JOLP_GenerateTownLocationsRunner.Run(m_rLocationPrefab, m_fDuplicateRadius);
	}
}

//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(
	name: "JOLP Generate Town Locations",
	description: "Generate JOLP location controllers from map CITY/TOWN/VILLAGE/SETTLEMENT descriptors.",
	wbModules: { "WorldEditor" },
	shortcut: "Ctrl+Shift+G",
	awesomeFontCode: 0xf279)]
class JOLP_GenerateTownLocationsPlugin : WorkbenchPlugin
{
	override void Run()
	{
		JOLP_GenerateTownLocationsRunner.Run(JOLP_GenerateTownLocationsRunner.DEFAULT_LOCATION_PREFAB, JOLP_GenerateTownLocationsRunner.DEFAULT_DUPLICATE_RADIUS);
	}
}
#endif
