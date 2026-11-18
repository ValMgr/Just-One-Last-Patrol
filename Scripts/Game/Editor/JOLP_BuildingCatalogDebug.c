//! Logs placeables / provider traits / faction when FreeRoam building mode activates.
//! Also skips ContentBrowser faction AND-filter — JOLP catalog is already faction-authored,
//! and placeables that only carry FACTION_US as auto-label were being dropped.
modded class SCR_CampaignBuildingEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void AddRemoveFactionLabel(SCR_Faction faction, bool addLabel)
	{
		// Intentionally no-op: our placeable list is US-only with authored FACTION_US where needed.
		// Vanilla AddRemoveFactionLabel AND-filters the browser and hides services/forts that
		// only store FACTION_US on m_aAutoLabels (and FreeRoam props with broken int labels).
		Print(string.Format("[JOLP] Building: skip ContentBrowser faction label filter (faction=%1 add=%2)",
			faction, addLabel), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	override protected void EOnEditorActivate()
	{
		super.EOnEditorActivate();
		LogBuildingCatalogState();
	}

	//------------------------------------------------------------------------------------------------
	protected void LogBuildingCatalogState()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return;

		SCR_CampaignBuildingManagerComponent buildingManager =
			SCR_CampaignBuildingManagerComponent.Cast(gameMode.FindComponent(SCR_CampaignBuildingManagerComponent));
		if (!buildingManager)
		{
			Print("[JOLP] Building: no CampaignBuildingManagerComponent", LogLevel.WARNING);
			return;
		}

		array<ResourceName> placeables = buildingManager.GetPlaceablePrefabs();
		int count = 0;
		if (placeables)
			count = placeables.Count();

		Print(string.Format("[JOLP] Building: placeable catalog count=%1", count), LogLevel.NORMAL);
		if (placeables)
		{
			for (int i = 0; i < count; i++)
				Print(string.Format("[JOLP] Building:  [%1] %2", i, placeables[i]), LogLevel.NORMAL);
		}

		SCR_CampaignBuildingProviderComponent provider = GetProviderComponent(true);
		if (!provider)
		{
			Print("[JOLP] Building: no provider on activate", LogLevel.WARNING);
			return;
		}

		array<EEditableEntityLabel> traits = provider.GetAvailableTraits();
		string traitStr = "";
		if (traits)
		{
			foreach (EEditableEntityLabel trait : traits)
			{
				if (traitStr != "")
					traitStr += ",";
				traitStr += trait.ToString();
			}
		}
		Print(string.Format("[JOLP] Building: provider AvailableTraits=[%1]", traitStr), LogLevel.NORMAL);

		SCR_ResourceComponent resource = provider.GetResourceComponent();
		float aggregated = -1;
		if (resource)
		{
			SCR_ResourceConsumer consumer = resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
			if (consumer)
				aggregated = consumer.GetAggregatedResourceValue();
		}

		IEntity root = provider.GetOwner();
		while (root && root.GetParent())
			root = root.GetParent();

		float stock = JOLP_SupplyStock.Read(root);
		Print(string.Format("[JOLP] Building: provider supplies aggregated=%1 Storage_Supplies=%2", aggregated, stock), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! The depot the player used, even when the provider list is still empty.
	SCR_CampaignBuildingProviderComponent JOLP_GetMenuProvider()
	{
		if (m_ForcedProviderComponent)
			return m_ForcedProviderComponent;

		if (!m_aProvidersComponents.IsEmpty())
			return m_aProvidersComponents[0];

		return null;
	}

	//------------------------------------------------------------------------------------------------
	SCR_EEntitySpawnerSlotType JOLP_GetDepotPad()
	{
		SCR_CampaignBuildingProviderComponent provider = JOLP_GetMenuProvider();
		if (!provider)
			return SCR_EEntitySpawnerSlotType.INVALID;

		array<EEditableEntityLabel> traits = provider.GetAvailableTraits();
		if (!traits)
			return SCR_EEntitySpawnerSlotType.INVALID;

		if (traits.Contains(EEditableEntityLabel.VEHICLE_HELICOPTER))
			return SCR_EEntitySpawnerSlotType.VEHICLE_LARGE;

		if (traits.Contains(EEditableEntityLabel.VEHICLE_TRUCK) || traits.Contains(EEditableEntityLabel.VEHICLE_APC))
			return SCR_EEntitySpawnerSlotType.VEHICLE_MEDIUM;

		if (traits.Contains(EEditableEntityLabel.VEHICLE_CAR))
			return SCR_EEntitySpawnerSlotType.VEHICLE_SMALL;

		return SCR_EEntitySpawnerSlotType.INVALID;
	}

	//------------------------------------------------------------------------------------------------
	void JOLP_CollectMenuPrefabs(out notnull array<ResourceName> list, out bool vehicleMenu, bool logResult = true)
	{
		vehicleMenu = false;
		list.Clear();

		SCR_EEntitySpawnerSlotType pad = JOLP_GetDepotPad();
		if (pad != SCR_EEntitySpawnerSlotType.INVALID)
		{
			vehicleMenu = true;
			JOLP_VehicleSpawnCatalog.FillPrefabs(pad, list);
			if (logResult)
				Print(string.Format("[JOLP] Depot menu: %1 vehicles (pad=%2)", list.Count(), typename.EnumToString(SCR_EEntitySpawnerSlotType, pad)), LogLevel.NORMAL);
			return;
		}

		JOLP_VehicleSpawnCatalog.FillCompositionPrefabs(list);
		if (logResult)
			Print(string.Format("[JOLP] Building menu: %1 compositions", list.Count()), LogLevel.NORMAL);
	}
}

//------------------------------------------------------------------------------------------------
//! The content browser copies every faction's compositions before the first card is built.
//! Replace that copy with the JOLP composition list so custom prefabs have a ghost, or with VehicleSpawn.conf at a depot.
modded class SCR_PlacingEditorComponentClass
{
	protected ref array<ResourceName> m_aJOLPMenuPrefabs;

	//------------------------------------------------------------------------------------------------
	void JOLP_SetMenuPrefabs(array<ResourceName> prefabs)
	{
		if (!prefabs)
		{
			m_aJOLPMenuPrefabs = null;
			return;
		}

		m_aJOLPMenuPrefabs = {};
		foreach (ResourceName prefab : prefabs)
			m_aJOLPMenuPrefabs.Insert(prefab);
	}

	//------------------------------------------------------------------------------------------------
	bool JOLP_HasMenuList()
	{
		return m_aJOLPMenuPrefabs != null;
	}

	//------------------------------------------------------------------------------------------------
	override ResourceName GetPrefab(int index)
	{
		if (!m_aJOLPMenuPrefabs)
			return super.GetPrefab(index);

		if (!m_aJOLPMenuPrefabs.IsIndexValid(index))
			return ResourceName.Empty;

		return m_aJOLPMenuPrefabs[index];
	}

	//------------------------------------------------------------------------------------------------
	override int GetPrefabID(ResourceName prefab)
	{
		if (!m_aJOLPMenuPrefabs)
			return super.GetPrefabID(prefab);

		return m_aJOLPMenuPrefabs.Find(prefab);
	}

	//------------------------------------------------------------------------------------------------
	override int CountPrefabs()
	{
		if (!m_aJOLPMenuPrefabs)
			return super.CountPrefabs();

		return m_aJOLPMenuPrefabs.Count();
	}

	//------------------------------------------------------------------------------------------------
	override int GetPrefabs(out notnull array<ResourceName> outPrefabs, bool onlyExposed = false)
	{
		if (!m_aJOLPMenuPrefabs)
			return super.GetPrefabs(outPrefabs, onlyExposed);

		outPrefabs.InsertAll(m_aJOLPMenuPrefabs);
		return outPrefabs.Count();
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ContentBrowserEditorComponent
{
	protected SCR_CampaignBuildingEditorComponent m_JOLPBuilding;
	protected ref array<ResourceName> m_aJOLPMenuPrefabs;
	protected bool m_bJOLPVehicleMenu;
	protected bool m_bJOLPReloadMenu;

	//------------------------------------------------------------------------------------------------
	override void EOnEditorActivate()
	{
		super.EOnEditorActivate();

		m_JOLPBuilding = SCR_CampaignBuildingEditorComponent.Cast(GetOwner().FindComponent(SCR_CampaignBuildingEditorComponent));
		if (!m_JOLPBuilding)
			return;

		m_JOLPBuilding.GetOnProviderChanged().Insert(JOLP_OnProviderChanged);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnEditorDeactivate()
	{
		if (m_JOLPBuilding)
			m_JOLPBuilding.GetOnProviderChanged().Remove(JOLP_OnProviderChanged);

		JOLP_ClearMenuList();
		super.EOnEditorDeactivate();
	}

	//------------------------------------------------------------------------------------------------
	override bool EOnEditorActivateAsync(int attempt)
	{
		if (attempt == 0 && m_JOLPBuilding)
			JOLP_InstallMenuList();

		if (attempt > 0 && m_bJOLPReloadMenu)
		{
			m_bJOLPReloadMenu = false;
			JOLP_InstallMenuList();
			if (m_aJOLPMenuPrefabs)
				JOLP_CopyMenuIntoAsync();
		}

		bool pending = super.EOnEditorActivateAsync(attempt);

		// The faction catalog has no E_JOLP prefabs, so a card built from it has no ghost.
		// Load the JOLP list into the same indexes the placer uses.
		if (attempt == 0 && m_aJOLPMenuPrefabs)
		{
			JOLP_CopyMenuIntoAsync();
			return false;
		}

		return pending;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetFirstAvailabeBudgetCost(int prefabID, out SCR_EntityBudgetValue budgetCost)
	{
		int cost;
		ResourceName prefab = GetResourceNamePrefabID(prefabID);
		if (JOLP_VehicleSpawnCatalog.TryGetSupplyCost(prefab, cost))
		{
			budgetCost = new SCR_EntityBudgetValue(EEditableEntityBudget.CAMPAIGN, cost);
			return true;
		}

		return super.GetFirstAvailabeBudgetCost(prefabID, budgetCost);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanPlace(int prefabID, out notnull array<ref SCR_EntityBudgetValue> budgetCosts, out SCR_UIInfo blockingBudgetInfo, bool showNotification = false, out EEditableEntityBudget blockingBudget = -1)
	{
		int cost;
		ResourceName prefab = GetResourceNamePrefabID(prefabID);
		if (!JOLP_VehicleSpawnCatalog.TryGetSupplyCost(prefab, cost))
			return super.CanPlace(prefabID, budgetCosts, blockingBudgetInfo, showNotification, blockingBudget);

		budgetCosts.Clear();
		budgetCosts.Insert(new SCR_EntityBudgetValue(EEditableEntityBudget.CAMPAIGN, cost));

		if (!m_BudgetManager)
			return true;

		if (m_BudgetManager.CanPlace(budgetCosts, blockingBudget))
			return true;

		m_BudgetManager.GetCurrentBudgetInfo(blockingBudget, blockingBudgetInfo);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool RefreshPreviewCost(int prefabID = -1)
	{
		int cost;
		if (prefabID >= 0 && m_BudgetManager)
		{
			ResourceName prefab = GetResourceNamePrefabID(prefabID);
			if (JOLP_VehicleSpawnCatalog.TryGetSupplyCost(prefab, cost))
			{
				array<ref SCR_EntityBudgetValue> budgetCosts = {};
				budgetCosts.Insert(new SCR_EntityBudgetValue(EEditableEntityBudget.CAMPAIGN, cost));
				m_BudgetManager.UpdatePreviewCost(budgetCosts);
				return true;
			}
		}

		return super.RefreshPreviewCost(prefabID);
	}

	//------------------------------------------------------------------------------------------------
	override void FilterEntries()
	{
		super.FilterEntries();

		if (!m_JOLPBuilding)
			return;

		if (!m_bJOLPVehicleMenu)
		{
			JOLP_KeepCompositionCards();
			return;
		}

		m_aFilteredPrefabIDs.Clear();
		m_aLocalizationKeys.Clear();

		int shown;
		int missing;
		int count = GetInfoCount();
		for (int i = 0; i < count; i++)
		{
			SCR_EditableEntityUIInfo info = GetInfo(i);
			if (!info)
			{
				missing++;
				continue;
			}

			m_aFilteredPrefabIDs.Insert(i);
			m_aLocalizationKeys.Insert(info.GetName());
			shown++;
		}

		string currentSearch = GetCurrentSearch();
		if (!currentSearch.IsEmpty())
		{
			array<int> searchResultPrefabID = {};
			array<int> searchResultIndices = {};
			WidgetManager.SearchLocalized(currentSearch, m_aLocalizationKeys, searchResultIndices);

			foreach (int searchResultIndex : searchResultIndices)
				searchResultPrefabID.Insert(m_aFilteredPrefabIDs.Get(searchResultIndex));

			m_aFilteredPrefabIDs.Copy(searchResultPrefabID);
		}

		m_iFilteredPrefabIDsCount = m_aFilteredPrefabIDs.Count();
		Event_OnBrowserEntriesFiltered.Invoke();
		Print(string.Format("[JOLP] Depot menu cards: %1 shown, %2 missing a build-menu UI", shown, missing), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_KeepCompositionCards()
	{
		JOLP_ResolvePlacingData();

		array<ResourceName> allowed = {};
		JOLP_VehicleSpawnCatalog.FillCompositionPrefabs(allowed);
		if (allowed.IsEmpty() || !m_PlacingManagerData)
			return;

		array<int> kept = {};
		array<ResourceName> seen = {};
		foreach (int prefabId : m_aFilteredPrefabIDs)
		{
			ResourceName prefab = m_PlacingManagerData.GetPrefab(prefabId);
			if (!JOLP_IsConfiguredComposition(prefab, allowed))
				continue;

			if (seen.Find(prefab) != -1)
				continue;

			seen.Insert(prefab);
			kept.Insert(prefabId);
		}

		m_aFilteredPrefabIDs.Clear();
		m_aLocalizationKeys.Clear();
		foreach (int prefabId : kept)
		{
			m_aFilteredPrefabIDs.Insert(prefabId);
			SCR_EditableEntityUIInfo info = GetInfo(prefabId);
			if (info)
				m_aLocalizationKeys.Insert(info.GetName());
			else
				m_aLocalizationKeys.Insert(string.Empty);
		}

		m_iFilteredPrefabIDsCount = m_aFilteredPrefabIDs.Count();
		Event_OnBrowserEntriesFiltered.Invoke();
		Print(string.Format("[JOLP] Building menu cards: %1", m_iFilteredPrefabIDsCount), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected bool JOLP_IsConfiguredComposition(ResourceName prefab, notnull array<ResourceName> allowed)
	{
		if (prefab.IsEmpty())
			return false;

		if (allowed.Find(prefab) != -1)
			return true;

		string path = prefab.GetPath();
		if (path.IsEmpty())
			return false;

		foreach (ResourceName allowedPrefab : allowed)
		{
			if (allowedPrefab.GetPath() == path)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_ResolvePlacingData()
	{
		if (m_PlacingManagerData)
			return;

		SCR_PlacingEditorComponent placing = SCR_PlacingEditorComponent.Cast(FindEditorComponent(SCR_PlacingEditorComponent, true, true));
		if (placing)
			m_PlacingManagerData = SCR_PlacingEditorComponentClass.Cast(placing.GetEditorComponentData());
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_OnProviderChanged(SCR_CampaignBuildingProviderComponent provider)
	{
		if (m_aAsyncPrefabs)
		{
			m_bJOLPReloadMenu = true;
			return;
		}

		if (!m_aInfos)
			return;

		JOLP_InstallMenuList();
		if (m_aJOLPMenuPrefabs)
			JOLP_RebuildInfosNow();
		else
			FilterEntries();
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_InstallMenuList()
	{
		m_bJOLPVehicleMenu = false;
		if (!m_JOLPBuilding)
			return;

		array<ResourceName> list = {};
		bool vehicleMenu;
		m_JOLPBuilding.JOLP_CollectMenuPrefabs(list, vehicleMenu);
		m_bJOLPVehicleMenu = vehicleMenu;
		if (list.IsEmpty())
		{
			m_aJOLPMenuPrefabs = null;
			JOLP_PushList(null);
			return;
		}

		m_aJOLPMenuPrefabs = list;
		JOLP_PushList(list);
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_PushList(array<ResourceName> list)
	{
		SCR_PlacingEditorComponentClass fromInstance = SCR_PlacingEditorComponentClass.Cast(SCR_PlacingEditorComponentClass.GetInstance(SCR_PlacingEditorComponent, true));
		if (fromInstance)
			fromInstance.JOLP_SetMenuPrefabs(list);

		if (!m_PlacingManagerData)
		{
			SCR_PlacingEditorComponent placing = SCR_PlacingEditorComponent.Cast(FindEditorComponent(SCR_PlacingEditorComponent, true, true));
			if (placing)
				m_PlacingManagerData = SCR_PlacingEditorComponentClass.Cast(placing.GetEditorComponentData());
		}

		if (m_PlacingManagerData && m_PlacingManagerData != fromInstance)
			m_PlacingManagerData.JOLP_SetMenuPrefabs(list);
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_CopyMenuIntoAsync()
	{
		m_aInfos = {};
		if (!m_aAsyncPrefabs)
			m_aAsyncPrefabs = {};
		else
			m_aAsyncPrefabs.Clear();

		if (!m_aJOLPMenuPrefabs)
			return;

		foreach (ResourceName prefab : m_aJOLPMenuPrefabs)
			m_aAsyncPrefabs.Insert(prefab);

		m_iAsyncIndex = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_RebuildInfosNow()
	{
		m_aInfos = {};
		if (m_aJOLPMenuPrefabs)
		{
			foreach (ResourceName prefab : m_aJOLPMenuPrefabs)
				m_aInfos.Insert(SCR_EditableEntityUIInfo.ExtractEditableUIInfoFromPrefab(prefab));
		}

		FilterEntries();
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_ClearMenuList()
	{
		m_aJOLPMenuPrefabs = null;
		m_bJOLPVehicleMenu = false;
		m_bJOLPReloadMenu = false;

		SCR_PlacingEditorComponentClass fromInstance = SCR_PlacingEditorComponentClass.Cast(SCR_PlacingEditorComponentClass.GetInstance(SCR_PlacingEditorComponent, true));
		if (fromInstance)
			fromInstance.JOLP_SetMenuPrefabs(null);

		if (m_PlacingManagerData)
			m_PlacingManagerData.JOLP_SetMenuPrefabs(null);
	}
}

//------------------------------------------------------------------------------------------------
//! Catalog cards hide CompositionCost unless a budget value is passed in.
//! VehicleSpawn.conf is that value. The prefab's own campaign budget is not.
modded class SCR_AssetCardFrontUIComponent
{
	//------------------------------------------------------------------------------------------------
	override void InitCard(int prefabID, SCR_UIInfo info, ResourceName prefab, SCR_UIInfo blockingBudgetInfo = null, SCR_ContentBrowserEditorComponent contentBrowser = null)
	{
		super.InitCard(prefabID, info, prefab, blockingBudgetInfo, contentBrowser);

		int cost;
		if (!JOLP_VehicleSpawnCatalog.TryGetSupplyCost(prefab, cost))
			return;

		UpdateBudgetCost(new SCR_EntityBudgetValue(EEditableEntityBudget.CAMPAIGN, cost));
	}
}
