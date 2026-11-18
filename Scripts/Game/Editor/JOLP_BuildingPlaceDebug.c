//------------------------------------------------------------------------------------------------
//! Diagnose FreeRoam place rejects (CreateEntityOwner empty entityIds) and soften
//! AreLabelsMatching for JOLP's faction-authored catalog.
modded class SCR_CampaignBuildingPlacingEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void CreateEntityServer(SCR_EditorPreviewParams params, RplId prefabID, int playerID, int entityIndex, bool isQueue, array<RplId> recipientIds, bool canBePlayer, RplId holderId)
	{
		JOLP_InstallPlacingList();

		// Keep the catalog row (before GetRandomVariant) so the supply price still matches VehicleSpawn.conf.
		SCR_PlacingEditorComponentClass prefabData = SCR_PlacingEditorComponentClass.Cast(GetEditorComponentData());
		if (prefabData)
			JOLP_VehicleSpawnCatalog.SetPendingPrefab(prefabData.GetPrefab(prefabID));

		super.CreateEntityServer(params, prefabID, playerID, entityIndex, isQueue, recipientIds, canBePlayer, holderId);
		JOLP_VehicleSpawnCatalog.SetPendingPrefab(ResourceName.Empty);
	}

	//------------------------------------------------------------------------------------------------
	override protected bool CanPlaceEntityServer(IEntityComponentSource editableEntitySource, out EEditableEntityBudget blockingBudget, bool updatePreview, bool showNotification, int prefabID = -1, int playerID = -1, SCR_EditorPreviewParams params = null)
	{
		JOLP_InstallPlacingList();

		ResourceName prefab = JOLP_VehicleSpawnCatalog.GetPendingPrefab();
		SCR_PlacingEditorComponentClass prefabData = SCR_PlacingEditorComponentClass.Cast(GetEditorComponentData());
		if (prefabData && prefabID >= 0)
		{
			ResourceName listed = prefabData.GetPrefab(prefabID);
			if (!listed.IsEmpty())
				prefab = listed;
		}

		if (!prefab.IsEmpty())
			JOLP_VehicleSpawnCatalog.SetPendingPrefab(prefab);

		bool ok = super.CanPlaceEntityServer(editableEntitySource, blockingBudget, updatePreview, showNotification, prefabID, playerID, params);
		if (!ok && prefabID >= 0)
			JOLP_VehicleSpawnCatalog.SetPendingPrefab(ResourceName.Empty);

		if (!ok)
		{
			Print(string.Format("[JOLP] Building place rejected by CanPlaceEntityServer (budget=%1 prefabID=%2 player=%3 prefab=%4)",
				typename.EnumToString(EEditableEntityBudget, blockingBudget), prefabID, playerID, prefab), LogLevel.WARNING);
		}

		return ok;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool CanSelectEntityPrefab(ResourceName prefab, out EEditableEntityBudget blockingBudget, bool updatePreview = true, bool showBudgetMaxNotification = true)
	{
		if (!prefab.IsEmpty())
			JOLP_VehicleSpawnCatalog.SetPendingPrefab(prefab);

		return super.CanSelectEntityPrefab(prefab, blockingBudget, updatePreview, showBudgetMaxNotification);
	}

	//------------------------------------------------------------------------------------------------
	override bool IsThereEnoughBudgetToSpawn(IEntityComponentSource entitySource)
	{
		int vehicleCost;
		if (JOLP_VehicleSpawnCatalog.TryGetSupplyCost(JOLP_VehicleSpawnCatalog.GetPendingPrefab(), vehicleCost) && vehicleCost > 0 && m_Provider)
		{
			IEntity stockRoot = JOLP_SupplyStock.ResolveRoot(m_Provider);
			float vehicleStock = -1;
			if (stockRoot)
				vehicleStock = JOLP_SupplyStock.Read(stockRoot);

			if (vehicleStock >= 0 && vehicleStock + 0.01 < vehicleCost)
			{
				Print(string.Format("[JOLP] Vehicle request refused: price=%1 stock=%2 prefab=%3", vehicleCost, vehicleStock, JOLP_VehicleSpawnCatalog.GetPendingPrefab()), LogLevel.WARNING);
				return false;
			}
		}

		bool ok = super.IsThereEnoughBudgetToSpawn(entitySource);
		if (ok)
			return true;

		float stock = -1;
		if (m_Provider)
		{
			IEntity root = m_Provider;
			while (root && root.GetParent())
				root = root.GetParent();

			stock = JOLP_SupplyStock.Read(root);

			SCR_ResourceComponent resource = SCR_ResourceComponent.FindResourceComponent(m_Provider);
			float aggregated = -1;
			if (resource)
			{
				SCR_ResourceConsumer consumer = resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
				if (consumer)
					aggregated = consumer.GetAggregatedResourceValue();
			}

			Print(string.Format("[JOLP] Building budget reject: providerAggregated=%1 hqStorageStock=%2", aggregated, stock), LogLevel.WARNING);
		}
		else
		{
			Print("[JOLP] Building budget reject: no provider", LogLevel.WARNING);
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla requires entityLabels to contain providerFaction.GetFactionLabel().
	//! JOLP catalog is already US-authored; if affiliation/blacklist flakes, fall back to trait overlap.
	override protected bool AreLabelsMatching(notnull array<EEditableEntityLabel> entityLabels)
	{
		int configuredCost;
		if (JOLP_VehicleSpawnCatalog.TryGetSupplyCost(JOLP_VehicleSpawnCatalog.GetPendingPrefab(), configuredCost))
			return true;

		if (super.AreLabelsMatching(entityLabels))
			return true;

		if (!m_Provider)
		{
			Print("[JOLP] Building AreLabelsMatching fail: no provider", LogLevel.WARNING);
			return false;
		}

		SCR_CampaignBuildingProviderComponent providerComp = SCR_CampaignBuildingProviderComponent.Cast(
			m_Provider.FindComponent(SCR_CampaignBuildingProviderComponent));
		if (!providerComp)
		{
			Print("[JOLP] Building AreLabelsMatching fail: no provider component", LogLevel.WARNING);
			return false;
		}

		array<EEditableEntityLabel> providerLabels = providerComp.GetAvailableTraits();
		bool traitMatch;
		string entityStr;
		string providerStr;
		foreach (EEditableEntityLabel label : entityLabels)
		{
			if (entityStr != "")
				entityStr += ",";
			entityStr += label.ToString();
		}

		foreach (EEditableEntityLabel providerLabel : providerLabels)
		{
			if (providerStr != "")
				providerStr += ",";
			providerStr += providerLabel.ToString();

			if (entityLabels.Contains(providerLabel))
				traitMatch = true;
		}

		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(m_Provider.FindComponent(FactionAffiliationComponent));
		if (!fac)
		{
			IEntity parent = m_Provider.GetParent();
			while (parent && !fac)
			{
				fac = FactionAffiliationComponent.Cast(parent.FindComponent(FactionAffiliationComponent));
				parent = parent.GetParent();
			}
		}

		FactionKey facKey = "";
		if (fac)
		{
			Faction affiliated = fac.GetAffiliatedFaction();
			if (!affiliated)
				affiliated = fac.GetDefaultAffiliatedFaction();
			if (affiliated)
				facKey = affiliated.GetFactionKey();
		}

		// RHS_USAF (and similar) often use a non-FACTION_US editable label while vanilla
		// placeables still carry FACTION_US (11). Trait overlap is enough for JOLP.
		bool factionOk = entityLabels.Contains(EEditableEntityLabel.FACTION_US)
			&& (facKey == "US" || facKey == "RHS_USAF" || facKey.Contains("US"));

		Print(string.Format("[JOLP] Building AreLabelsMatching vanilla fail; fallback traitMatch=%1 factionOk=%2 faction=%3 entity=[%4] provider=[%5]",
			traitMatch, factionOk, facKey, entityStr, providerStr), LogLevel.WARNING);

		return traitMatch || factionOk;
	}

	//------------------------------------------------------------------------------------------------
	override protected void EOnEditorActivate()
	{
		super.EOnEditorActivate();
		JOLP_InstallPlacingList(true);
	}

	//------------------------------------------------------------------------------------------------
	override protected void EOnEditorActivateServer()
	{
		super.EOnEditorActivateServer();
		JOLP_InstallPlacingList(true);
	}

	//------------------------------------------------------------------------------------------------
	override protected void EOnEditorDeactivate()
	{
		JOLP_ClearPlacingList();
		super.EOnEditorDeactivate();
	}

	//------------------------------------------------------------------------------------------------
	override protected void EOnEditorDeactivateServer()
	{
		JOLP_ClearPlacingList();
		super.EOnEditorDeactivateServer();
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_InstallPlacingList(bool logResult = false)
	{
		SCR_CampaignBuildingEditorComponent building = m_CampaignBuildingComponent;
		if (!building)
			building = SCR_CampaignBuildingEditorComponent.Cast(GetOwner().FindComponent(SCR_CampaignBuildingEditorComponent));

		if (!building)
			return;

		array<ResourceName> list = {};
		bool vehicleMenu;
		building.JOLP_CollectMenuPrefabs(list, vehicleMenu, logResult);

		SCR_PlacingEditorComponentClass data = SCR_PlacingEditorComponentClass.Cast(GetEditorComponentData());
		if (!data)
			return;

		if (list.IsEmpty())
			data.JOLP_SetMenuPrefabs(null);
		else
			data.JOLP_SetMenuPrefabs(list);
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_ClearPlacingList()
	{
		SCR_PlacingEditorComponentClass data = SCR_PlacingEditorComponentClass.Cast(GetEditorComponentData());
		if (data)
			data.JOLP_SetMenuPrefabs(null);
	}
}

//------------------------------------------------------------------------------------------------
//! When MilitaryBase consumer still returns 0, fall back to HQ Storage_Supplies stock.
modded class SCR_CampaignBuildingProviderComponent
{
	//------------------------------------------------------------------------------------------------
	override int GetBudgetValue(EEditableEntityBudget type, out SCR_CampaignBuildingProviderComponent componentToUse)
	{
		int value = super.GetBudgetValue(type, componentToUse);
		if (type != EEditableEntityBudget.CAMPAIGN)
			return value;

		if (value > 0)
			return value;

		float stock = ReadOwnerStorageStock();
		if (stock > 0)
		{
			Print(string.Format("[JOLP] Building budget fallback: consumer=0, using Storage_Supplies=%1", stock), LogLevel.NORMAL);
			return stock;
		}

		return value;
	}

	//------------------------------------------------------------------------------------------------
	float ReadOwnerStorageStock()
	{
		IEntity root = JOLP_SupplyStock.ResolveRoot(GetOwner());
		if (!root)
			return -1;

		return JOLP_SupplyStock.Read(root);
	}
}

//------------------------------------------------------------------------------------------------
//! Budget UI / CanPlace also reads consumer.GetAggregatedResourceValue() directly.
modded class SCR_CampaignBuildingBudgetEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override bool GetMaxBudget(EEditableEntityBudget type, out SCR_EntityBudgetValue budget)
	{
		bool valid = super.GetMaxBudget(type, budget);
		if (!valid || type != EEditableEntityBudget.CAMPAIGN)
			return valid;

		if (budget && budget.GetBudgetValue() > 0)
			return true;

		if (!m_CampaignBuildingComponent)
			return valid;

		SCR_CampaignBuildingProviderComponent provider = m_CampaignBuildingComponent.GetProviderComponent();
		if (!provider)
			return valid;

		float stock = provider.ReadOwnerStorageStock();
		if (stock > 0)
			budget.SetBudgetValue(stock);

		return valid;
	}
}

//------------------------------------------------------------------------------------------------
//! A depot vehicle is charged the supply cost from VehicleSpawn.conf.
modded class SCR_BudgetEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override void GetBudgetCostsDontDiscardCampaignBudget(IEntityComponentSource editableEntitySource, out notnull array<ref SCR_EntityBudgetValue> budgetCosts)
	{
		super.GetBudgetCostsDontDiscardCampaignBudget(editableEntitySource, budgetCosts);
		JOLP_VehicleSpawnCatalog.ApplySourceSupplyCost(editableEntitySource, budgetCosts);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetEntitySourcePreviewBudgetCosts(IEntityComponentSource editableEntitySource, out notnull array<ref SCR_EntityBudgetValue> budgetCosts)
	{
		bool ok = super.GetEntitySourcePreviewBudgetCosts(editableEntitySource, budgetCosts);
		if (JOLP_VehicleSpawnCatalog.ApplySourceSupplyCost(editableEntitySource, budgetCosts))
			return true;

		return ok;
	}
}
