//------------------------------------------------------------------------------------------------
//! GameMode_JOLP cannot serialize m_OutlineManager (Unknown keyword / Unknown class when the
//! outline manager type is modded). Build the manager in script instead of loading the .conf.
//!
//! Also: vanilla OnEntityCoreBudgetUpdated returns immediately unless SCR_GameModeCampaign exists,
//! so FreeRoam place never calls RequestConsumtion on JOLP (SCR_BaseGameMode). Mirror that path.
modded class SCR_CampaignBuildingManagerComponent
{
	protected IEntity m_JOLPLastCompositionOwner;
	protected int m_iJOLPLastCompositionChange;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		EnsureOutlineManager();
		super.OnPostInit(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		EnsureOutlineManager();
		super.EOnInit(owner);
		// Vanilla EOnInit loads the registry through a direct call. Load again so the JOLP catalog wins.
		GetPrefabListFromConfig();
	}

	//------------------------------------------------------------------------------------------------
	//! The build menu and the dig outline both index this list. Keep it on the JOLP catalog
	//! even if a placed game-mode instance still has the vanilla FreeRoam registry.
	override void GetPrefabListFromConfig()
	{
		m_sPrefabsToBuildResource = "{664B13AF00000120}Configs/Building/JOLP_Compositions_FreeRoamBuilding.conf";
		super.GetPrefabListFromConfig();

		int count = 0;
		if (m_aPlaceablePrefabs)
			count = m_aPlaceablePrefabs.Count();

		Print(string.Format("[JOLP] Building catalog loaded: %1 entries", count), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	override SCR_CampaignBuildingCompositionOutlineManager GetOutlineManager()
	{
		EnsureOutlineManager();
		return super.GetOutlineManager();
	}

	//------------------------------------------------------------------------------------------------
	protected void EnsureOutlineManager()
	{
		if (m_OutlineManager)
			return;

		SCR_CampaignBuildingCompositionOutlineManager manager = new SCR_CampaignBuildingCompositionOutlineManager();
		manager.JOLP_InitDefaultLayouts();
		m_OutlineManager = manager;
		Print("[JOLP] OutlineManager created in script (dig layouts ready)", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Same as vanilla spend/refund, but without the SCR_GameModeCampaign gate.
	override protected void OnEntityCoreBudgetUpdated(EEditableEntityBudget entityBudget, int originalBudgetValue, int budgetChange, int updatedBudgetValue, SCR_EditableEntityComponent entity)
	{
		if (IsProxy())
			return;

		if (entityBudget != m_BudgetType)
			return;

		if (!entity || !entity.GetOwner())
			return;

		int configCost;
		if (JOLP_ConfiguredVehicleCost(entity, configCost))
		{
			// Spawn often uses a random variant, so entity.GetPrefab() is not the catalog row.
			// Charge or refund the config price here and skip the prefab's own campaign budget.
			if (budgetChange < 0)
			{
				if (!JOLP_VehicleSpawnCatalog.IsRefundSuppressed())
					JOLP_RefundVehicle(entity.GetOwnerScripted(), configCost);

				return;
			}

			ResourceName pricedPrefab = JOLP_VehicleSpawnCatalog.GetPendingPrefab();
			if (pricedPrefab.IsEmpty())
				pricedPrefab = entity.GetPrefab();

			JOLP_ChargeVehicle(entity.GetOwnerScripted(), configCost, pricedPrefab);
			return;
		}

		// Continue with compositions placed in WB only when refund is about to happen.
		if (entity.GetOwner().IsLoaded() && budgetChange > 0)
			return;

		if (SCR_PersistenceSystem.IsLoadInProgress())
			return;

		int propBudgetValue;
		array<ref SCR_EntityBudgetValue> budgets = {};
		entity.GetEntityAndChildrenBudgetCost(budgets);

		foreach (SCR_EntityBudgetValue budget : budgets)
		{
			if (budget.GetBudgetType() != EEditableEntityBudget.PROPS)
				continue;

			propBudgetValue = budget.GetBudgetValue();
			break;
		}

		IEntity entityOwner = entity.GetOwnerScripted();
		SCR_ResourceComponent resourceComponent;
		bool wasContainerSpawned;

		if (!GetResourceComponent(entityOwner, resourceComponent))
		{
			SCR_CampaignBuildingCompositionComponent compositionComponent = SCR_CampaignBuildingCompositionComponent.Cast(entityOwner.FindComponent(SCR_CampaignBuildingCompositionComponent));
			if (compositionComponent && budgetChange < 0)
			{
				SpawnCustomResourceHolder(entityOwner, resourceComponent);
				wasContainerSpawned = true;
			}
		}

		if (!resourceComponent)
			return;

		if (!resourceComponent.IsResourceTypeEnabled())
			return;

		// The composition and one of its parts both raise this event for the full price.
		if (entityOwner == m_JOLPLastCompositionOwner && budgetChange == m_iJOLPLastCompositionChange)
			return;

		m_JOLPLastCompositionOwner = entityOwner;
		m_iJOLPLastCompositionChange = budgetChange;

		IEntity providerEntity = resourceComponent.GetOwner();
		if (!providerEntity)
			return;

		SCR_CampaignBuildingProviderComponent providerComponent = SCR_CampaignBuildingProviderComponent.Cast(providerEntity.FindComponent(SCR_CampaignBuildingProviderComponent));

		if (budgetChange < 0)
		{
			budgetChange = Math.Round(budgetChange * m_iCompositionRefundPercentage * 0.01);

			if (providerComponent)
				providerComponent.AddPropValue(-propBudgetValue);

			if (wasContainerSpawned)
			{
				SCR_ResourceContainer container = resourceComponent.GetContainer(EResourceType.SUPPLIES);
				if (container)
					container.SetResourceValue(-budgetChange);
			}
			else
			{
				SCR_ResourceGenerator generator = resourceComponent.GetGenerator(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
				if (generator)
					generator.RequestGeneration(-budgetChange);
				else
					JOLP_ApplySupplyDelta(providerEntity, -budgetChange);
			}
		}
		else
		{
			if (providerComponent)
				providerComponent.AddPropValue(propBudgetValue);

			float before = JOLP_ReadProviderStock(providerEntity);

			SCR_ResourceConsumer consumer = resourceComponent.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
			if (consumer)
				consumer.RequestConsumtion(budgetChange);

			float after = JOLP_ReadProviderStock(providerEntity);
			float spent = before - after;
			if (spent + 0.01 < budgetChange)
			{
				float remaining = budgetChange - Math.Max(spent, 0);
				if (JOLP_ApplySupplyDelta(providerEntity, -remaining))
					Print(string.Format("[JOLP] Building supply fallback consume=%1 (consumer spent=%2 needed=%3)", remaining, spent, budgetChange), LogLevel.NORMAL);
				else
					Print(string.Format("[JOLP] Building supply consume FAILED needed=%1 consumerSpent=%2 stock=%3", budgetChange, spent, after), LogLevel.WARNING);
			}
			else
			{
				Print(string.Format("[JOLP] Building supply consumed=%1 stock %2 -> %3", budgetChange, before, after), LogLevel.NORMAL);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected float JOLP_ReadProviderStock(IEntity providerEntity)
	{
		IEntity root = providerEntity;
		while (root && root.GetParent())
			root = root.GetParent();

		return JOLP_SupplyStock.Read(root);
	}

	//------------------------------------------------------------------------------------------------
	//! Positive amount adds, negative amount removes from HQ Storage_Supplies.
	protected bool JOLP_ApplySupplyDelta(IEntity providerEntity, float delta)
	{
		IEntity root = providerEntity;
		while (root && root.GetParent())
			root = root.GetParent();

		if (delta > 0)
			return JOLP_SupplyStock.Add(root, delta);

		return JOLP_SupplyStock.Consume(root, -delta);
	}

	//------------------------------------------------------------------------------------------------
	override void EntitySpawnedByProvider(int prefabID, SCR_EditableEntityComponent editableEntity, int playerId)
	{
		super.EntitySpawnedByProvider(prefabID, editableEntity, playerId);
		JOLP_ChargeVehicleRequest(editableEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected bool JOLP_ConfiguredVehicleCost(SCR_EditableEntityComponent entity, out int cost)
	{
		cost = 0;
		if (entity && JOLP_VehicleSpawnCatalog.TryGetSupplyCost(entity.GetPrefab(), cost))
			return true;

		return JOLP_VehicleSpawnCatalog.TryGetSupplyCost(JOLP_VehicleSpawnCatalog.GetPendingPrefab(), cost);
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_ChargeVehicleRequest(SCR_EditableEntityComponent entity)
	{
		if (!entity)
			return;

		int configCost;
		if (!JOLP_ConfiguredVehicleCost(entity, configCost))
			return;

		JOLP_ChargeVehicle(entity.GetOwnerScripted(), configCost, JOLP_VehicleSpawnCatalog.GetPendingPrefab());
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_ChargeVehicle(IEntity spawned, int cost, ResourceName prefab)
	{
		if (!spawned || cost <= 0)
			return;

		if (!JOLP_VehicleSpawnCatalog.TryConsumePlaceCharge())
			return;

		IEntity hint = GetTemporaryProvider();
		if (!hint)
			hint = spawned;

		IEntity root = JOLP_SupplyStock.ResolveRoot(hint);
		if (!root)
		{
			JOLP_VehicleSpawnCatalog.ReleasePlaceCharge();
			Print(string.Format("[JOLP] Vehicle supply charge failed: no HQ stock for %1 price=%2", prefab, cost), LogLevel.WARNING);
			return;
		}

		float before = JOLP_SupplyStock.Read(root);
		if (!JOLP_SupplyStock.Consume(root, cost))
		{
			JOLP_VehicleSpawnCatalog.ReleasePlaceCharge();
			Print(string.Format("[JOLP] Vehicle supply charge FAILED price=%1 stock=%2 prefab=%3", cost, before, prefab), LogLevel.WARNING);
			return;
		}

		JOLP_VehicleSpawnCatalog.RememberSpawnedPrefab(SCR_ResourceNameUtils.GetPrefabName(spawned), cost);
		Print(string.Format("[JOLP] Vehicle supply charged=%1 stock %2 -> %3 prefab=%4", cost, before, JOLP_SupplyStock.Read(root), prefab), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_RefundVehicle(IEntity spawned, int cost)
	{
		if (cost <= 0)
			return;

		if (JOLP_VehicleSpawnCatalog.IsRefundSuppressed())
			return;

		if (!JOLP_VehicleSpawnCatalog.TryConsumeRefund(spawned))
			return;

		int refund = Math.Round(cost * m_iCompositionRefundPercentage * 0.01);
		if (refund <= 0)
			return;

		IEntity hint = spawned;
		if (!hint)
			hint = GetTemporaryProvider();

		IEntity root = JOLP_SupplyStock.ResolveRoot(hint);
		if (!root || !JOLP_SupplyStock.Add(root, refund))
		{
			Print(string.Format("[JOLP] Vehicle supply refund FAILED price=%1", refund), LogLevel.WARNING);
			return;
		}

		Print(string.Format("[JOLP] Vehicle supply refunded=%1 stock=%2", refund, JOLP_SupplyStock.Read(root)), LogLevel.NORMAL);
	}
}
