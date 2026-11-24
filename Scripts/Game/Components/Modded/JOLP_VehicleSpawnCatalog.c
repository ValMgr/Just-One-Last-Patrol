//------------------------------------------------------------------------------------------------
//! Depot vehicle list and supply prices from Configs/Vehicles/VehicleSpawn.conf.
//! The US depot and any playable faction use this list instead of the vanilla vehicle catalog.
class JOLP_VehicleSpawnCatalog
{
	protected static const ResourceName CONFIG = "{664B13B0A1000320}Configs/Vehicles/VehicleSpawn.conf";

	protected static const ResourceName COMPOSITIONS = "{664B13AF00000120}Configs/Building/JOLP_Compositions_FreeRoamBuilding.conf";

	protected static ref SCR_EntityCatalog s_Catalog;
	protected static ref array<ref JOLP_VehicleSpawnEntry> s_aEntries;
	protected static ref map<ResourceName, int> s_mSupplyCost;
	protected static ResourceName s_sPendingPrefab;
	protected static bool s_bPlaceCharged;
	protected static int s_iSuppressRefund;
	protected static IEntity s_LastRefundRoot;
	protected static bool s_bLoaded;
	protected static bool s_bLoading;

	//------------------------------------------------------------------------------------------------
	static bool IsActive()
	{
		return s_Catalog != null;
	}

	//------------------------------------------------------------------------------------------------
	static SCR_EntityCatalog Get()
	{
		if (!s_bLoaded)
			Load();

		return s_Catalog;
	}

	//------------------------------------------------------------------------------------------------
	static bool AppliesTo(notnull SCR_Faction faction)
	{
		if (!IsActive() && !s_bLoaded)
			Get();

		if (!s_Catalog)
			return false;

		if (faction.IsPlayable())
			return true;

		return faction.GetFactionKey() == "US";
	}

	//------------------------------------------------------------------------------------------------
	//! Player faction and depot faction can both use this list without being the same faction.
	static bool SharesList(Faction left, Faction right)
	{
		SCR_Faction factionLeft = SCR_Faction.Cast(left);
		SCR_Faction factionRight = SCR_Faction.Cast(right);
		if (!factionLeft || !factionRight)
			return false;

		return AppliesTo(factionLeft) && AppliesTo(factionRight);
	}

	//------------------------------------------------------------------------------------------------
	static bool TryGetSupplyCost(ResourceName prefab, out int cost)
	{
		if (prefab.IsEmpty() || !s_mSupplyCost)
			return false;

		if (s_mSupplyCost.Find(prefab, cost))
			return true;

		string path = prefab.GetPath();
		if (path.IsEmpty())
			return false;

		foreach (ResourceName key, int value : s_mSupplyCost)
		{
			if (key.GetPath() != path)
				continue;

			cost = value;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static void SetPendingPrefab(ResourceName prefab)
	{
		// Clearing the pending row happens after the spawn, while child parts still
		// raise a second budget event. Only a new purchase may charge again.
		if (!prefab.IsEmpty() && prefab != s_sPendingPrefab)
			s_bPlaceCharged = false;

		s_sPendingPrefab = prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Deploy deletes the command truck. That delete must not pay its purchase price back.
	static void PushSuppressRefund()
	{
		s_iSuppressRefund++;
	}

	//------------------------------------------------------------------------------------------------
	static void PopSuppressRefund()
	{
		if (s_iSuppressRefund > 0)
			s_iSuppressRefund--;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRefundSuppressed()
	{
		return s_iSuppressRefund > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Parent and child parts both ask for the full config refund. Honour the first one.
	static bool TryConsumeRefund(IEntity entity)
	{
		IEntity root = entity;
		while (root && root.GetParent() && Vehicle.Cast(root.GetParent()))
			root = root.GetParent();

		if (!root)
			root = entity;

		if (root && root == s_LastRefundRoot)
			return false;

		s_LastRefundRoot = root;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One config charge per request. Child parts of the vehicle also raise a budget event.
	static bool TryConsumePlaceCharge()
	{
		if (s_bPlaceCharged)
			return false;

		s_bPlaceCharged = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ReleasePlaceCharge()
	{
		s_bPlaceCharged = false;
	}

	//------------------------------------------------------------------------------------------------
	//! A requested vehicle can spawn as a variant. Remember that prefab so a later refund uses the same price.
	static void RememberSpawnedPrefab(ResourceName spawned, int cost)
	{
		if (!s_mSupplyCost || spawned.IsEmpty() || cost < 0)
			return;

		int existing;
		if (s_mSupplyCost.Find(spawned, existing))
			return;

		s_mSupplyCost.Set(spawned, cost);
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName GetPendingPrefab()
	{
		return s_sPendingPrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Light depot passes VEHICLE_SMALL, heavy depot VEHICLE_MEDIUM, helipad VEHICLE_LARGE.
	static void FillPrefabs(SCR_EEntitySpawnerSlotType pad, notnull array<ResourceName> outPrefabs)
	{
		if (!s_aEntries)
			Get();

		if (!s_aEntries)
			return;

		foreach (JOLP_VehicleSpawnEntry entry : s_aEntries)
		{
			if (!entry || entry.m_rPrefab.IsEmpty())
				continue;

			SCR_EEntitySpawnerSlotType slots = entry.m_eSlotTypes;
			if (slots == SCR_EEntitySpawnerSlotType.INVALID)
				slots = SCR_EEntitySpawnerSlotType.VEHICLE_SMALL;

			if (!SCR_Enum.HasFlag(slots, pad))
				continue;

			if (outPrefabs.Find(entry.m_rPrefab) != -1)
				continue;

			outPrefabs.Insert(entry.m_rPrefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void FillCompositionPrefabs(notnull array<ResourceName> outPrefabs)
	{
		Resource container = BaseContainerTools.LoadContainer(COMPOSITIONS);
		if (!container || !container.IsValid())
		{
			Print("[JOLP] Building composition list missing: " + COMPOSITIONS, LogLevel.ERROR);
			return;
		}

		SCR_PlaceableEntitiesRegistry registry = SCR_PlaceableEntitiesRegistry.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!registry)
		{
			Print("[JOLP] Building composition list could not be read", LogLevel.ERROR);
			return;
		}

		array<ResourceName> prefabs = registry.GetPrefabs(false);
		if (!prefabs)
			return;

		foreach (ResourceName prefab : prefabs)
		{
			if (prefab.IsEmpty())
				continue;

			outPrefabs.Insert(prefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ReplaceCampaignCost(notnull array<ref SCR_EntityBudgetValue> budgetCosts, int cost)
	{
		for (int i = budgetCosts.Count() - 1; i >= 0; i--)
		{
			SCR_EntityBudgetValue budget = budgetCosts[i];
			if (budget && budget.GetBudgetType() == EEditableEntityBudget.CAMPAIGN)
				budgetCosts.Remove(i);
		}

		budgetCosts.Insert(new SCR_EntityBudgetValue(EEditableEntityBudget.CAMPAIGN, cost));
	}

	//------------------------------------------------------------------------------------------------
	static bool ApplySourceSupplyCost(IEntityComponentSource source, notnull array<ref SCR_EntityBudgetValue> budgetCosts)
	{
		int cost;
		if (!TryGetSupplyCost(PrefabFromSource(source), cost) && !TryGetSupplyCost(GetPendingPrefab(), cost))
			return false;

		ReplaceCampaignCost(budgetCosts, cost);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static ResourceName PrefabFromSource(IEntityComponentSource source)
	{
		if (!source)
			return ResourceName.Empty;

		BaseContainer entitySource = source.GetParent();
		if (!entitySource)
			return ResourceName.Empty;

		return entitySource.GetResourceName();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Load()
	{
		if (s_bLoading)
			return;

		if (!GetGame() || !GetGame().InPlayMode())
			return;

		s_bLoading = true;
		s_bLoaded = true;

		Resource container = BaseContainerTools.LoadContainer(CONFIG);
		if (!container || !container.IsValid())
		{
			s_bLoading = false;
			Print("[JOLP] Vehicle spawn config missing: " + CONFIG, LogLevel.ERROR);
			return;
		}

		JOLP_VehicleSpawnConfig config = JOLP_VehicleSpawnConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config || !config.m_aVehicles || config.m_aVehicles.IsEmpty())
		{
			s_bLoading = false;
			Print("[JOLP] Vehicle spawn config has no vehicles", LogLevel.WARNING);
			return;
		}

		s_mSupplyCost = new map<ResourceName, int>();
		SCR_EntityCatalog catalog = new SCR_EntityCatalog();
		s_aEntries = {};
		array<ref JOLP_VehicleSpawnEntry> entries = s_aEntries;
		foreach (JOLP_VehicleSpawnEntry entry : config.m_aVehicles)
		{
			if (!entry || entry.m_rPrefab.IsEmpty())
				continue;

			s_mSupplyCost.Set(entry.m_rPrefab, entry.m_iSupplyCost);
			entries.Insert(entry);
		}

		if (entries.IsEmpty())
		{
			s_bLoading = false;
			Print("[JOLP] Vehicle spawn config has no prefabs", LogLevel.WARNING);
			return;
		}

		s_Catalog = catalog;
		s_bLoading = false;
		catalog.JOLP_SetVehicles(entries);
		Print(string.Format("[JOLP] Vehicle spawn list: %1 vehicles", entries.Count()), LogLevel.NORMAL);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_EntityCatalog
{
	//------------------------------------------------------------------------------------------------
	void JOLP_SetVehicles(notnull array<ref JOLP_VehicleSpawnEntry> entries)
	{
		m_eEntityCatalogType = EEntityCatalogType.VEHICLE;
		m_aEntityEntryList = {};

		foreach (JOLP_VehicleSpawnEntry source : entries)
		{
			SCR_EntityCatalogSpawnerData spawnerData = new SCR_EntityCatalogSpawnerData();
			spawnerData.JOLP_SetSlotTypes(source.m_eSlotTypes);
			spawnerData.JOLP_SetDisplayName(source.m_sName);

			SCR_EntityCatalogEntry entry = new SCR_EntityCatalogEntry();
			entry.JOLP_Setup(source.m_rPrefab, spawnerData);
			m_aEntityEntryList.Insert(entry);
		}

		InitCatalog();
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_EntityCatalogEntry
{
	//------------------------------------------------------------------------------------------------
	void JOLP_Setup(ResourceName prefab, notnull SCR_EntityCatalogSpawnerData spawnerData)
	{
		m_bEnabled = true;
		m_sEntityPrefab = prefab;
		m_aEntityDataList = {};
		m_aEntityDataList.Insert(spawnerData);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_EntityCatalogSpawnerData
{
	//------------------------------------------------------------------------------------------------
	void JOLP_SetSlotTypes(SCR_EEntitySpawnerSlotType slotTypes)
	{
		m_bEnabled = true;
		m_iEntityCount = 1;
		if (!m_aVariantData)
			m_aVariantData = {};

		if (slotTypes == SCR_EEntitySpawnerSlotType.INVALID)
			slotTypes = SCR_EEntitySpawnerSlotType.VEHICLE_SMALL;

		m_eSlotTypes = slotTypes;
	}

	//------------------------------------------------------------------------------------------------
	void JOLP_SetDisplayName(string name)
	{
		if (name.IsEmpty())
			return;

		m_UiInfo = SCR_UIInfo.CreateInfo(name);
	}

	//------------------------------------------------------------------------------------------------
	override int GetSupplyCost()
	{
		int cost;
		if (JOLP_VehicleSpawnCatalog.TryGetSupplyCost(m_sDefaultPrefab, cost))
			return cost;

		return super.GetSupplyCost();
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_Faction
{
	//------------------------------------------------------------------------------------------------
	override SCR_EntityCatalog GetFactionEntityCatalogOfType(EEntityCatalogType catalogType, bool printNotFound = true)
	{
		if (catalogType == EEntityCatalogType.VEHICLE && JOLP_VehicleSpawnCatalog.AppliesTo(this))
		{
			SCR_EntityCatalog catalog = JOLP_VehicleSpawnCatalog.Get();
			if (catalog)
				return catalog;
		}

		return super.GetFactionEntityCatalogOfType(catalogType, printNotFound);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_CatalogSpawnerUserAction
{
	protected int m_iJOLPAssetIndex = -1;

	//------------------------------------------------------------------------------------------------
	void JOLP_SetAssetIndex(int index)
	{
		m_iJOLPAssetIndex = index;
	}

	//------------------------------------------------------------------------------------------------
	//! The action prefab is spawned, then parented. Init runs before the parent exists, so the depot is bound afterwards.
	void JOLP_BindSpawner(notnull SCR_CatalogEntitySpawnerComponent spawner)
	{
		m_EntitySpawner = spawner;
		m_ResourceComponent = spawner.GetSpawnerResourceComponent();
		if (!m_ResourceComponent)
			return;

		m_ResourceGenerator = m_ResourceComponent.GetGenerator(EResourceGeneratorID.DEFAULT, m_eResourceType);
		m_ResourceConsumer = m_ResourceComponent.GetConsumer(EResourceGeneratorID.DEFAULT, m_eResourceType);
	}

	//------------------------------------------------------------------------------------------------
	protected override int GetActionIndex()
	{
		if (m_iJOLPAssetIndex >= 0)
			return m_iJOLPAssetIndex;

		return super.GetActionIndex();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_EntitySpawner || !m_EntityData || !m_EntitySpawnerData)
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		Faction userFaction = character.GetFaction();
		Faction spawnerFaction = m_EntitySpawner.GetFaction();
		if (userFaction != spawnerFaction && !JOLP_VehicleSpawnCatalog.SharesList(userFaction, spawnerFaction))
			return false;

		m_iRequestStatus = m_EntitySpawner.GetRequestState(m_EntityData, user);
		return m_iRequestStatus != SCR_EEntityRequestStatus.NOT_AVAILABLE;
	}
}

//------------------------------------------------------------------------------------------------
//! Vanilla depots ship one "Request vehicle" action, not one action per vehicle.
//! A child with a fixed set of catalog actions is parented to the service so the config list can show.
modded class SCR_CatalogEntitySpawnerComponent
{
	protected static const ResourceName VEHICLE_ACTIONS = "{664B13B0A1000300}Prefabs/Systems/JOLP_VehicleSpawnActions.et";
	protected static const string VEHICLE_ACTIONS_NAME = "JOLP_VehicleSpawnActions";

	//------------------------------------------------------------------------------------------------
	override Faction GetRequesterFaction(notnull IEntity user)
	{
		Faction requester = super.GetRequesterFaction(user);
		if (!requester)
			return null;

		Faction spawnerFaction = GetFaction();
		if (requester == spawnerFaction)
			return requester;

		if (JOLP_VehicleSpawnCatalog.SharesList(requester, spawnerFaction))
			return spawnerFaction;

		return requester;
	}

	//------------------------------------------------------------------------------------------------
	protected override void SetCurrentFactionCatalog()
	{
		m_aAssetList.Clear();

		SCR_Faction faction = SCR_Faction.Cast(GetFaction());
		if (!faction || !m_aCatalogTypes || m_aCatalogTypes.IsEmpty())
			return;

		bool vehicleSpawner = false;
		foreach (EEntityCatalogType catalogType : m_aCatalogTypes)
		{
			if (catalogType == EEntityCatalogType.VEHICLE)
				vehicleSpawner = true;

			SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(catalogType);
			if (!catalog)
				continue;

			AddAssetsFromCatalog(catalog);
		}

		if (vehicleSpawner && JOLP_VehicleSpawnCatalog.IsActive())
			JOLP_BindVehicleActions();
		else
			AssignUserActions();
	}

	//------------------------------------------------------------------------------------------------
	//! Config vehicles rarely carry the vanilla VEHICLE_CAR / VEHICLE_HELICOPTER label, so the label filter would hide the whole list.
	override protected void AddAssetsFromCatalog(notnull SCR_EntityCatalog entityCatalog, bool overwriteOld = false)
	{
		SCR_EntityCatalog configured = JOLP_VehicleSpawnCatalog.Get();
		if (!configured || entityCatalog != configured)
		{
			super.AddAssetsFromCatalog(entityCatalog, overwriteOld);
			return;
		}

		if (overwriteOld)
			m_aAssetList.Clear();

		array<SCR_EntityCatalogEntry> entries = {};
		array<typename> dataClasses = {};
		dataClasses.Insert(SCR_EntityCatalogSpawnerData);
		entityCatalog.GetFullFilteredEntityList(entries, null, null, dataClasses);

		SCR_EEntitySpawnerSlotType pad = JOLP_PadSlot();
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			if (pad != SCR_EEntitySpawnerSlotType.INVALID)
			{
				SCR_EntityCatalogSpawnerData data = SCR_EntityCatalogSpawnerData.Cast(entry.GetEntityDataOfType(SCR_EntityCatalogSpawnerData));
				if (!data || !data.CanSpawnInSlot(pad))
					continue;
			}

			m_aAssetList.Insert(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_EEntitySpawnerSlotType JOLP_PadSlot()
	{
		SCR_EServicePointType serviceType = GetType();
		if (serviceType == SCR_EServicePointType.LIGHT_VEHICLE_DEPOT)
			return SCR_EEntitySpawnerSlotType.VEHICLE_SMALL;

		if (serviceType == SCR_EServicePointType.HEAVY_VEHICLE_DEPOT)
			return SCR_EEntitySpawnerSlotType.VEHICLE_MEDIUM;

		if (serviceType == SCR_EServicePointType.HELIPAD)
			return SCR_EEntitySpawnerSlotType.VEHICLE_LARGE;

		return SCR_EEntitySpawnerSlotType.INVALID;
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_BindVehicleActions()
	{
		IEntity carrier = JOLP_FindActionCarrier();
		if (!carrier)
			carrier = JOLP_SpawnActionCarrier();

		if (!carrier)
			return;

		ActionsManagerComponent manager = ActionsManagerComponent.Cast(carrier.FindComponent(ActionsManagerComponent));
		if (!manager)
		{
			Print("[JOLP] Vehicle spawn actions prefab has no ActionsManagerComponent", LogLevel.ERROR);
			return;
		}

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);

		int slots = 0;
		int assetIndex = 0;
		foreach (BaseUserAction action : actions)
		{
			SCR_CatalogSpawnerUserAction spawnerAction = SCR_CatalogSpawnerUserAction.Cast(action);
			if (!spawnerAction)
				continue;

			slots++;
			spawnerAction.JOLP_BindSpawner(this);
			if (m_aAssetList.IsIndexValid(assetIndex))
			{
				spawnerAction.SetSpawnerData(m_aAssetList[assetIndex]);
				spawnerAction.JOLP_SetAssetIndex(assetIndex);
			}
			else
			{
				spawnerAction.SetSpawnerData(null);
			}

			assetIndex++;
		}

		Print(string.Format("[JOLP] Depot %1 offers %2 vehicles (%3 menu slots)",
			typename.EnumToString(SCR_EServicePointType, GetType()), m_aAssetList.Count(), slots), LogLevel.NORMAL);

		if (m_aAssetList.IsEmpty())
		{
			Print("[JOLP] Depot vehicle list is empty. Check Configs/Vehicles/VehicleSpawn.conf and the pad size.", LogLevel.WARNING);
			return;
		}

		if (m_aAssetList.Count() > slots)
		{
			Print(string.Format("[JOLP] Vehicle spawn list has %1 vehicles and the depot menu has %2 slots. Extra vehicles are hidden.",
				m_aAssetList.Count(), slots), LogLevel.WARNING);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity JOLP_FindActionCarrier()
	{
		IEntity owner = GetOwner();
		if (JOLP_CountCatalogActions(owner) > 0)
			return owner;

		if (!owner)
			return null;

		IEntity child = owner.GetChildren();
		while (child)
		{
			if (child.GetName() == VEHICLE_ACTIONS_NAME)
				return child;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected int JOLP_CountCatalogActions(IEntity entity)
	{
		if (!entity)
			return 0;

		ActionsManagerComponent manager = ActionsManagerComponent.Cast(entity.FindComponent(ActionsManagerComponent));
		if (!manager)
			return 0;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		int count = 0;
		foreach (BaseUserAction action : actions)
		{
			if (SCR_CatalogSpawnerUserAction.Cast(action))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity JOLP_SpawnActionCarrier()
	{
		IEntity owner = GetOwner();
		if (!owner || !owner.GetWorld())
			return null;

		Resource resource = Resource.Load(VEHICLE_ACTIONS);
		if (!resource || !resource.IsValid())
		{
			Print("[JOLP] Vehicle spawn actions prefab failed to load", LogLevel.ERROR);
			return null;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.LOCAL;
		params.Parent = owner;
		params.Transform[0] = "1 0 0";
		params.Transform[1] = "0 1 0";
		params.Transform[2] = "0 0 1";
		params.Transform[3] = "0 0 0";

		IEntity carrier = GetGame().SpawnEntityPrefab(resource, owner.GetWorld(), params);
		if (!carrier)
			return null;

		// RplComponent reads the parent during init. Spawn params Parent is unreliable
		// for GenericEntity, so force the link if the engine dropped it.
		if (carrier.GetParent() != owner)
			owner.AddChild(carrier, -1);
		carrier.SetName(VEHICLE_ACTIONS_NAME);
		JOLP_PersistenceManagerComponent.UntrackTransient(carrier);
		return carrier;
	}
}

//------------------------------------------------------------------------------------------------
//! Built depots link the vanilla service, which has no per-vehicle actions.
//! Replace the spawned barrels after the links exist. Do not mod SCR_EditorLinkEntry:
//! that class is stored inside prefabs, and a modded container class makes the engine
//! report "Unknown class 'SCR_EditorLinkEntry'", which drops the ghost and the finished build.
modded class SCR_EditorLinkComponent
{
	protected static const ResourceName JOLP_SERVICE_SMALL = "{D4573A9778DB3CB0}PrefabsEditable/Auto/Props/Military/Compositions/US/E_VehicleService_US_Small.et";
	protected static const ResourceName JOLP_SERVICE_MEDIUM = "{BBB43CA8D41101AD}PrefabsEditable/Auto/Props/Military/Compositions/US/E_VehicleService_US_Medium.et";
	protected static const ResourceName JOLP_SERVICE_LIGHT = "{664B13B0A1000440}PrefabsEditable/Auto/Props/Military/Compositions/US/E_JOLP_VehicleService_Light.et";
	protected static const ResourceName JOLP_SERVICE_HEAVY = "{664B13B0A1000441}PrefabsEditable/Auto/Props/Military/Compositions/US/E_JOLP_VehicleService_Heavy.et";

	//------------------------------------------------------------------------------------------------
	override void SpawnComposition()
	{
		super.SpawnComposition();
		JOLP_ReplaceVehicleServices();
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_ReplaceVehicleServices()
	{
		if (!m_aLinkedChildren)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		for (int i = m_aLinkedChildren.Count() - 1; i >= 0; i--)
		{
			IEntity child = m_aLinkedChildren[i];
			if (!child)
				continue;

			EntityPrefabData prefabData = child.GetPrefabData();
			if (!prefabData)
				continue;

			ResourceName replacement;
			ResourceName prefab = prefabData.GetPrefabName();
			if (prefab == JOLP_SERVICE_SMALL)
				replacement = JOLP_SERVICE_LIGHT;
			else if (prefab == JOLP_SERVICE_MEDIUM)
				replacement = JOLP_SERVICE_HEAVY;
			else
				continue;

			Resource resource = Resource.Load(replacement);
			if (!resource || !resource.IsValid())
				continue;

			vector parentTransform[4];
			vector childTransform[4];
			owner.GetTransform(parentTransform);
			child.GetTransform(childTransform);

			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.LOCAL;
			params.Parent = owner;
			Math3D.MatrixInvMultiply4(parentTransform, childTransform, params.Transform);

			IEntity swapped = GetGame().SpawnEntityPrefab(resource, owner.GetWorld(), params);
			if (!swapped)
				continue;

			m_aLinkedChildren.Set(i, swapped);
			SCR_EntityHelper.DeleteEntityAndChildren(child);

			// The parent link already fired OnLinkedEntitiesSpawned, which is what
			// makes a depot load its vehicle list. The replacement was spawned after
			// that event, so register it directly.
			SCR_ServicePointComponent service = SCR_ServicePointComponent.Cast(swapped.FindComponent(SCR_ServicePointComponent));
			if (service)
				service.RegisterService();
		}
	}
}
