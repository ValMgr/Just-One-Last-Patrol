//------------------------------------------------------------------------------------------------
//! Reads a character's kit, prices it from the arsenal catalog, and puts a saved kit back on.
//! Unpriced prefabs cost 0. Quick slots are not priced: they point at items already in the tree.
class JOLP_LoadoutGear
{
	//------------------------------------------------------------------------------------------------
	static int CountPieces(JOLP_PlayerLoadout loadout)
	{
		if (!loadout || !loadout.m_aItems)
			return 0;

		int total;
		foreach (JOLP_LoadoutItem item : loadout.m_aItems)
			total = total + CountItem(item);

		return total;
	}

	//------------------------------------------------------------------------------------------------
	protected static int CountItem(JOLP_LoadoutItem item)
	{
		if (!item)
			return 0;

		int total = 1;
		if (item.m_aAttachments)
			total = total + item.m_aAttachments.Count();

		if (item.m_aChildren)
		{
			foreach (JOLP_LoadoutItem child : item.m_aChildren)
				total = total + CountItem(child);
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	static void Extract(IEntity character, notnull JOLP_PlayerLoadout loadout)
	{
		loadout.ClearItems();
		if (!character)
			return;

		set<IEntity> equipped = new set<IEntity>();
		ExtractWeapons(character, loadout, equipped);
		ExtractRootStorage(character, loadout, equipped);
		ExtractCharacterEquipment(character, loadout, equipped);
		ExtractQuickSlots(character, loadout);
	}

	//------------------------------------------------------------------------------------------------
	static int Price(JOLP_PlayerLoadout loadout, SCR_EArsenalSupplyCostType costType)
	{
		if (!loadout || !loadout.m_aItems)
			return 0;

		int total;
		foreach (JOLP_LoadoutItem item : loadout.m_aItems)
			total = total + PriceItem(item, costType);

		return total;
	}

	//------------------------------------------------------------------------------------------------
	static void Describe(JOLP_PlayerLoadout loadout, SCR_EArsenalSupplyCostType costType, int wornCost, int supplies, out string items, out string cost)
	{
		int savedCost = Price(loadout, costType);
		int net = savedCost - wornCost;

		items = "";
		if (!loadout || !loadout.m_aItems || loadout.m_aItems.IsEmpty())
		{
			items = "Nothing equipped.";
		}
		else
		{
			ref map<string, int> counts = new map<string, int>();
			ref map<string, int> unitCosts = new map<string, int>();
			foreach (JOLP_LoadoutItem item : loadout.m_aItems)
				Accumulate(item, costType, counts, unitCosts);

			for (int i = 0; i < counts.Count(); i++)
			{
				string prefab = counts.GetKey(i);
				int count = counts.GetElement(i);
				int unit = 0;
				unitCosts.Find(prefab, unit);
				int line = unit * count;
				if (!items.IsEmpty())
					items = items + "\n";

				items = items + string.Format("%1 x%2    %3", DisplayName(prefab), count, line);
			}
		}

		string settlement;
		if (net > 0)
			settlement = string.Format("You pay %1", net);
		else if (net < 0)
			settlement = string.Format("You are refunded %1", -net);
		else
			settlement = "No supply change";

		string afford = "";
		if (net > 0 && supplies < net)
			afford = "\nNot enough supplies.";

		cost = string.Format("Wearing %1\nLoadout %2\n%3\nSupplies %4%5", wornCost, savedCost, settlement, supplies, afford);
	}

	//------------------------------------------------------------------------------------------------
	static bool Apply(IEntity character, notnull JOLP_PlayerLoadout loadout, out string error)
	{
		error = string.Empty;
		if (!character)
		{
			error = "You have no character.";
			return false;
		}

		InventoryStorageManagerComponent storage = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!storage)
		{
			error = "You have no inventory.";
			return false;
		}

		Clear(character, storage);
		PrepareHeadGear(loadout, character);

		if (loadout.m_aItems)
		{
			array<ref JOLP_LoadoutItem> ordered = new array<ref JOLP_LoadoutItem>();
			foreach (JOLP_LoadoutItem item : loadout.m_aItems)
			{
				if (item && !item.m_bEquipped && IsHelmetPrefab(item.m_sPrefab))
					ordered.Insert(item);
			}

			foreach (JOLP_LoadoutItem item : loadout.m_aItems)
			{
				if (item && !item.m_bEquipped && IsHeadCoverItem(item, character) && ordered.Find(item) < 0)
					ordered.Insert(item);
			}

			foreach (JOLP_LoadoutItem item : loadout.m_aItems)
			{
				if (item && ordered.Find(item) < 0)
					ordered.Insert(item);
			}

			foreach (JOLP_LoadoutItem item : ordered)
			{
				if (!item)
					continue;

				if (!item.m_bEquipped && AlreadyCarried(loadout, item))
					continue;

				bool placed;
				if (item.m_bEquipped)
					placed = ApplyWeapon(character, storage, item);
				else
					placed = ApplyStored(character, storage, item);

				if (!placed)
					Print(string.Format("[JOLP] Loadout skipped '%1'", item.m_sPrefab), LogLevel.WARNING);
			}
		}

		ApplyQuickSlots(character, storage, loadout);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static string DisplayName(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return "Item";

		SCR_EntityCatalogManagerComponent catalogs = SCR_EntityCatalogManagerComponent.GetInstance();
		if (catalogs)
		{
			SCR_EntityCatalogEntry entry = catalogs.GetEntryWithPrefabFromAnyCatalog(EEntityCatalogType.ITEM, prefab);
			if (entry)
			{
				SCR_UIInfo info = entry.GetEntityUiInfo();
				if (info)
				{
					string name = info.GetName();
					if (!name.IsEmpty())
						return name;
				}
			}
		}

		string path = prefab;
		int slash = path.LastIndexOf("/");
		if (slash >= 0 && slash + 1 < path.Length())
			path = path.Substring(slash + 1, path.Length() - slash - 1);

		int dot = path.LastIndexOf(".");
		if (dot > 0)
			path = path.Substring(0, dot);

		if (path.IsEmpty())
			return "Item";

		return path;
	}

	//------------------------------------------------------------------------------------------------
	protected static int CostOf(ResourceName prefab, SCR_EArsenalSupplyCostType costType)
	{
		if (prefab.IsEmpty())
			return 0;

		SCR_EntityCatalogManagerComponent catalogs = SCR_EntityCatalogManagerComponent.GetInstance();
		if (!catalogs)
			return 0;

		SCR_EntityCatalogEntry entry = catalogs.GetEntryWithPrefabFromAnyCatalog(EEntityCatalogType.ITEM, prefab);
		if (!entry)
			return 0;

		SCR_ArsenalItem arsenalItem = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
		if (arsenalItem)
			return arsenalItem.GetSupplyCost(costType, false);

		SCR_NonArsenalItemCostCatalogData other = SCR_NonArsenalItemCostCatalogData.Cast(entry.GetEntityDataOfType(SCR_NonArsenalItemCostCatalogData));
		if (other)
			return other.GetSupplyCost(costType, false);

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static int PriceItem(JOLP_LoadoutItem item, SCR_EArsenalSupplyCostType costType)
	{
		if (!item)
			return 0;

		int total = CostOf(item.m_sPrefab, costType);
		if (item.m_aAttachments)
		{
			foreach (string attachment : item.m_aAttachments)
				total = total + CostOf(attachment, costType);
		}

		if (item.m_aChildren)
		{
			foreach (JOLP_LoadoutItem child : item.m_aChildren)
				total = total + PriceItem(child, costType);
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Accumulate(JOLP_LoadoutItem item, SCR_EArsenalSupplyCostType costType, notnull map<string, int> counts, notnull map<string, int> unitCosts)
	{
		if (!item || item.m_sPrefab.IsEmpty())
			return;

		AddCount(item.m_sPrefab, costType, counts, unitCosts);
		if (item.m_aAttachments)
		{
			foreach (string attachment : item.m_aAttachments)
				AddCount(attachment, costType, counts, unitCosts);
		}

		if (item.m_aChildren)
		{
			foreach (JOLP_LoadoutItem child : item.m_aChildren)
				Accumulate(child, costType, counts, unitCosts);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddCount(ResourceName prefab, SCR_EArsenalSupplyCostType costType, notnull map<string, int> counts, notnull map<string, int> unitCosts)
	{
		if (prefab.IsEmpty())
			return;

		string key = prefab;
		int count;
		if (!counts.Find(key, count))
			count = 0;

		counts.Set(key, count + 1);
		int knownCost;
		if (!unitCosts.Find(key, knownCost))
			unitCosts.Set(key, CostOf(prefab, costType));
	}

	//------------------------------------------------------------------------------------------------
	protected static void ExtractWeapons(IEntity character, notnull JOLP_PlayerLoadout loadout, notnull set<IEntity> equipped)
	{
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!weapons)
			return;

		array<WeaponSlotComponent> slots = {};
		weapons.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
		{
			if (!slot)
				continue;

			IEntity weapon = slot.GetWeaponEntity();
			if (!weapon)
				continue;

			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(weapon);
			if (prefab.IsEmpty())
				continue;

			JOLP_LoadoutItem item = new JOLP_LoadoutItem();
			item.m_sPrefab = prefab;
			item.m_bEquipped = true;
			item.m_iWeaponSlot = slot.GetWeaponSlotIndex();
			ExtractAttachments(weapon, item);
			ExtractContents(weapon, item, equipped);
			loadout.m_aItems.Insert(item);
			equipped.Insert(weapon);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void ExtractRootStorage(IEntity character, notnull JOLP_PlayerLoadout loadout, notnull set<IEntity> equipped)
	{
		SCR_CharacterInventoryStorageComponent storage = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (!storage)
			return;

		int priority = storage.GetPriority();
		EStoragePurpose purpose = storage.GetPurpose();
		int slots = storage.GetSlotsCount();
		for (int i = 0; i < slots; i++)
		{
			IEntity entity = storage.Get(i);
			if (!entity || equipped.Contains(entity))
				continue;

			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
			if (prefab.IsEmpty())
				continue;

			JOLP_LoadoutItem item = new JOLP_LoadoutItem();
			item.m_sPrefab = prefab;
			item.m_iSlotIndex = i;
			item.m_iStoragePriority = priority;
			item.m_eStoragePurpose = purpose;
			ExtractAttachments(entity, item);
			ExtractContents(entity, item, equipped);
			loadout.m_aItems.Insert(item);
			equipped.Insert(entity);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Watch, binoculars and the other character equipment slots. These are not clothing slots.
	protected static void ExtractCharacterEquipment(IEntity character, notnull JOLP_PlayerLoadout loadout, notnull set<IEntity> equipped)
	{
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!manager)
			return;

		set<BaseInventoryStorageComponent> seen = new set<BaseInventoryStorageComponent>();
		array<BaseInventoryStorageComponent> storages = {};
		manager.GetStorages(storages, EStoragePurpose.PURPOSE_ANY);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!storage || storage.GetOwner() != character)
				continue;

			if (IsBodyStorage(storage))
				continue;

			EquipedLoadoutStorageComponent loadoutStorage = EquipedLoadoutStorageComponent.Cast(storage);
			if (loadoutStorage)
			{
				array<BaseInventoryStorageComponent> nested = {};
				loadoutStorage.GetOwnedStorages(nested, 1, true);
				foreach (BaseInventoryStorageComponent child : nested)
				{
					if (!child || IsBodyStorage(child))
						continue;

					ExtractLoose(child, loadout, equipped, seen);
				}

				continue;
			}

			ExtractLoose(storage, loadout, equipped, seen);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void ExtractLoose(BaseInventoryStorageComponent storage, notnull JOLP_PlayerLoadout loadout, notnull set<IEntity> equipped, notnull set<BaseInventoryStorageComponent> seen)
	{
		if (!storage || seen.Contains(storage) || IsBodyStorage(storage))
			return;

		seen.Insert(storage);
		int priority = storage.GetPriority();
		EStoragePurpose purpose = storage.GetPurpose();

		int slots = storage.GetSlotsCount();
		for (int i = 0; i < slots; i++)
			AddRootItem(storage.Get(i), i, priority, purpose, loadout, equipped);

		array<IEntity> items = {};
		storage.GetAll(items);
		foreach (IEntity entity : items)
			AddRootItem(entity, -1, priority, purpose, loadout, equipped);

		array<BaseInventoryStorageComponent> nested = {};
		storage.GetOwnedStorages(nested, 1, true);
		foreach (BaseInventoryStorageComponent child : nested)
			ExtractLoose(child, loadout, equipped, seen);
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddRootItem(IEntity entity, int slotIndex, int priority, EStoragePurpose purpose, notnull JOLP_PlayerLoadout loadout, notnull set<IEntity> equipped)
	{
		if (!entity || equipped.Contains(entity))
			return;

		ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
		if (prefab.IsEmpty())
			return;

		JOLP_LoadoutItem item = new JOLP_LoadoutItem();
		item.m_sPrefab = prefab;
		item.m_iSlotIndex = slotIndex;
		item.m_iStoragePriority = priority;
		item.m_eStoragePurpose = purpose;
		ExtractAttachments(entity, item);
		ExtractContents(entity, item, equipped);
		loadout.m_aItems.Insert(item);
		equipped.Insert(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Cargo pockets, equipment slots on a vest or helmet, and anything nested inside those.
	protected static void ExtractContents(IEntity container, notnull JOLP_LoadoutItem parent, notnull set<IEntity> equipped)
	{
		if (!container)
			return;

		set<BaseInventoryStorageComponent> seenStorages = new set<BaseInventoryStorageComponent>();

		InventoryStorageManagerComponent local = InventoryStorageManagerComponent.Cast(container.FindComponent(InventoryStorageManagerComponent));
		if (local)
		{
			WalkStorages(local, null, parent, equipped, seenStorages);
			ExtractStoredItems(container, parent, equipped);
			ExtractChildEntities(container, parent, equipped);
			return;
		}

		WalkStorages(ManagerOf(container), container, parent, equipped, seenStorages);

		BaseInventoryStorageComponent direct = BaseInventoryStorageComponent.Cast(container.FindComponent(BaseInventoryStorageComponent));
		if (direct)
			ExtractStorageTree(direct, parent, equipped, seenStorages);

		ExtractStoredItems(container, parent, equipped);
		ExtractChildEntities(container, parent, equipped);
	}

	//------------------------------------------------------------------------------------------------
	protected static void WalkStorages(InventoryStorageManagerComponent manager, IEntity owner, notnull JOLP_LoadoutItem parent, notnull set<IEntity> equipped, notnull set<BaseInventoryStorageComponent> seenStorages)
	{
		if (!manager)
			return;

		array<BaseInventoryStorageComponent> storages = {};
		manager.GetStorages(storages, EStoragePurpose.PURPOSE_ANY);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!storage)
				continue;

			if (owner && !StorageServes(storage, owner))
				continue;

			ExtractStorageTree(storage, parent, equipped, seenStorages);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Items sitting in a pouch or on a helmet are owned by that cloth, even when the storage list misses them.
	protected static void ExtractStoredItems(IEntity container, notnull JOLP_LoadoutItem parent, notnull set<IEntity> equipped)
	{
		InventoryStorageManagerComponent manager = ManagerOf(container);
		if (!manager)
			return;

		array<IEntity> items = {};
		manager.GetItems(items);
		foreach (IEntity entity : items)
		{
			if (!entity || entity == container || equipped.Contains(entity))
				continue;

			if (!ItemBelongsTo(entity, container))
				continue;

			AddChildItem(entity, -1, -1, EStoragePurpose.PURPOSE_ANY, parent, equipped);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pouches on a belt are part of the preset. Save what is inside them, not a second copy of the pouch.
	protected static void ExtractChildEntities(IEntity container, notnull JOLP_LoadoutItem parent, notnull set<IEntity> equipped)
	{
		ExtractHolders(container, parent, equipped, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ExtractHolders(IEntity container, notnull JOLP_LoadoutItem parent, notnull set<IEntity> equipped, int depth)
	{
		if (!container || depth > 5)
			return;

		IEntity child = container.GetChildren();
		while (child)
		{
			IEntity next = child.GetSibling();
			TakeHolder(child, parent, equipped, depth);
			child = next;
		}

		SlotManagerComponent slots = SlotManagerComponent.Cast(container.FindComponent(SlotManagerComponent));
		if (!slots)
			return;

		array<EntitySlotInfo> infos = {};
		slots.GetSlotInfos(infos);
		foreach (EntitySlotInfo info : infos)
		{
			if (!info)
				continue;

			TakeHolder(info.GetAttachedEntity(), parent, equipped, depth);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void TakeHolder(IEntity holder, notnull JOLP_LoadoutItem parent, notnull set<IEntity> equipped, int depth)
	{
		if (!holder || equipped.Contains(holder))
			return;

		if (HoldsItems(holder))
		{
			equipped.Insert(holder);
			ExtractContents(holder, parent, equipped);
			return;
		}

		if (holder.FindComponent(InventoryItemComponent))
			AddChildItem(holder, -1, -1, EStoragePurpose.PURPOSE_ANY, parent, equipped);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HoldsItems(IEntity entity)
	{
		if (!entity)
			return false;

		BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(entity.FindComponent(BaseInventoryStorageComponent));
		if (!storage || IsBodyStorage(storage))
			return false;

		array<IEntity> items = {};
		storage.GetAll(items, true);
		foreach (IEntity item : items)
		{
			if (item && item != entity)
				return true;
		}

		int slots = storage.GetSlotsCount();
		for (int i = 0; i < slots; i++)
		{
			IEntity item = storage.Get(i);
			if (item && item != entity)
				return true;
		}

		array<BaseInventoryStorageComponent> nested = {};
		storage.GetOwnedStorages(nested, 1, true);
		foreach (BaseInventoryStorageComponent child : nested)
		{
			if (!child || IsBodyStorage(child))
				continue;

			array<IEntity> nestedItems = {};
			child.GetAll(nestedItems, true);
			foreach (IEntity item : nestedItems)
			{
				if (item && item != entity)
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ItemBelongsTo(IEntity item, IEntity container)
	{
		return ItemReaches(item, container);
	}

	//------------------------------------------------------------------------------------------------
	//! A pouch storage is often owned by a child of the belt, or by a slot attachment, not by the belt itself.
	protected static bool StorageServes(BaseInventoryStorageComponent storage, IEntity container)
	{
		if (!storage || !container)
			return false;

		return ItemReaches(storage.GetOwner(), container);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ItemReaches(IEntity item, IEntity container)
	{
		if (!item || !container || item == container)
			return item == container;

		IEntity cursor = item;
		int guard = 0;
		while (cursor && guard < 8)
		{
			guard++;
			if (cursor == container || BelongsTo(cursor, container) || AttachedWithin(cursor, container, 0))
				return true;

			InventoryItemComponent inventoryItem = InventoryItemComponent.Cast(cursor.FindComponent(InventoryItemComponent));
			if (!inventoryItem)
				return false;

			InventoryStorageSlot slot = inventoryItem.GetParentSlot();
			if (!slot)
				return false;

			BaseInventoryStorageComponent storage = slot.GetStorage();
			if (!storage)
				return false;

			IEntity next = storage.GetOwner();
			if (!next || next == cursor)
				return false;

			cursor = next;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool AttachedWithin(IEntity entity, IEntity container, int depth)
	{
		if (!entity || !container || depth > 4)
			return false;

		SlotManagerComponent slots = SlotManagerComponent.Cast(container.FindComponent(SlotManagerComponent));
		if (slots)
		{
			array<EntitySlotInfo> infos = {};
			slots.GetSlotInfos(infos);
			foreach (EntitySlotInfo info : infos)
			{
				if (!info)
					continue;

				IEntity attached = info.GetAttachedEntity();
				if (!attached || attached == container)
					continue;

				if (attached == entity || BelongsTo(entity, attached))
					return true;

				if (AttachedWithin(entity, attached, depth + 1))
					return true;
			}
		}

		IEntity child = container.GetChildren();
		while (child)
		{
			if (child == entity || BelongsTo(entity, child))
				return true;

			if (AttachedWithin(entity, child, depth + 1))
				return true;

			child = child.GetSibling();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ExtractStorageTree(BaseInventoryStorageComponent storage, notnull JOLP_LoadoutItem parent, notnull set<IEntity> equipped, notnull set<BaseInventoryStorageComponent> seenStorages)
	{
		if (!storage || seenStorages.Contains(storage) || IsBodyStorage(storage))
			return;

		seenStorages.Insert(storage);
		int priority = storage.GetPriority();
		EStoragePurpose purpose = storage.GetPurpose();

		int slots = storage.GetSlotsCount();
		for (int i = 0; i < slots; i++)
			AddChildItem(storage.Get(i), i, priority, purpose, parent, equipped);

		array<IEntity> items = {};
		storage.GetAll(items);
		int extra = 0;
		foreach (IEntity entity : items)
		{
			if (!entity || equipped.Contains(entity))
				continue;

			AddChildItem(entity, extra, priority, purpose, parent, equipped);
			extra++;
		}

		array<BaseInventoryStorageComponent> nested = {};
		storage.GetOwnedStorages(nested, 1, true);
		foreach (BaseInventoryStorageComponent child : nested)
			ExtractStorageTree(child, parent, equipped, seenStorages);
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddChildItem(IEntity entity, int slotIndex, int priority, EStoragePurpose purpose, notnull JOLP_LoadoutItem parent, notnull set<IEntity> equipped)
	{
		if (!entity || equipped.Contains(entity))
			return;

		ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
		if (prefab.IsEmpty())
			return;

		equipped.Insert(entity);
		JOLP_LoadoutItem child = new JOLP_LoadoutItem();
		child.m_sPrefab = prefab;
		child.m_iSlotIndex = slotIndex;
		child.m_iStoragePriority = priority;
		child.m_eStoragePurpose = purpose;
		ExtractAttachments(entity, child);
		ExtractContents(entity, child, equipped);
		parent.m_aChildren.Insert(child);
	}

	//------------------------------------------------------------------------------------------------
	//! Clothing, weapons, and the medical slots that are not part of a kit.
	protected static bool IsBodyStorage(BaseInventoryStorageComponent storage)
	{
		if (!storage)
			return true;

		if (WeaponAttachmentsStorageComponent.Cast(storage))
			return true;

		if (SCR_CharacterInventoryStorageComponent.Cast(storage))
			return true;

		if (EquipedWeaponStorageComponent.Cast(storage))
			return true;

		if (SCR_IdentityItemStorageComponent.Cast(storage))
			return true;

		if (SCR_SalineStorageComponent.Cast(storage))
			return true;

		if (SCR_TourniquetStorageComponent.Cast(storage))
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static InventoryStorageManagerComponent ManagerOf(IEntity entity)
	{
		IEntity cursor = entity;
		while (cursor)
		{
			InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(cursor.FindComponent(InventoryStorageManagerComponent));
			if (manager)
				return manager;

			cursor = cursor.GetParent();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ExtractAttachments(IEntity weapon, notnull JOLP_LoadoutItem item)
	{
		WeaponAttachmentsStorageComponent storage = WeaponAttachmentsStorageComponent.Cast(weapon.FindComponent(WeaponAttachmentsStorageComponent));
		if (!storage)
			return;

		array<BaseInventoryStorageComponent> slots = {};
		storage.GetOwnedStorages(slots, 1, true);
		foreach (BaseInventoryStorageComponent slot : slots)
		{
			if (!slot)
				continue;

			array<IEntity> attachments = {};
			slot.GetAll(attachments);
			foreach (IEntity attachment : attachments)
			{
				if (!attachment)
					continue;

				ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(attachment);
				if (!prefab.IsEmpty())
					item.m_aAttachments.Insert(prefab);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void ExtractQuickSlots(IEntity character, notnull JOLP_PlayerLoadout loadout)
	{
		SCR_CharacterInventoryStorageComponent storage = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (!storage)
			return;

		for (int i = 0; i < 10; i++)
		{
			string prefab = "";
			SCR_QuickslotBaseContainer container = storage.GetContainerFromQuickslot(i);
			SCR_QuickslotEntityContainer entityContainer = SCR_QuickslotEntityContainer.Cast(container);
			if (entityContainer)
			{
				IEntity entity = entityContainer.GetEntity();
				if (entity)
					prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
			}

			loadout.m_aQuickSlots.Insert(prefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Clear(IEntity character, notnull InventoryStorageManagerComponent storage)
	{
		array<IEntity> roots = {};
		SCR_CharacterInventoryStorageComponent clothes = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (clothes)
		{
			int slots = clothes.GetSlotsCount();
			for (int i = 0; i < slots; i++)
			{
				IEntity item = clothes.Get(i);
				if (item)
					roots.Insert(item);
			}
		}

		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (weapons)
		{
			array<WeaponSlotComponent> slots = {};
			weapons.GetWeaponsSlots(slots);
			foreach (WeaponSlotComponent slot : slots)
			{
				if (!slot)
					continue;

				IEntity weapon = slot.GetWeaponEntity();
				if (weapon && roots.Find(weapon) == -1)
					roots.Insert(weapon);
			}
		}

		CollectCharacterEquipment(character, roots);

		foreach (IEntity item : roots)
		{
			if (!item)
				continue;

			if (!storage.TryDeleteItem(item))
				SCR_EntityHelper.DeleteEntityAndChildren(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ApplyWeapon(IEntity character, notnull InventoryStorageManagerComponent storage, notnull JOLP_LoadoutItem item)
	{
		IEntity weapon = Spawn(item.m_sPrefab, character);
		if (!weapon)
			return false;

		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		EquipedWeaponStorageComponent weaponStorage = EquipedWeaponStorageComponent.Cast(character.FindComponent(EquipedWeaponStorageComponent));
		WeaponComponent weaponComponent = WeaponComponent.Cast(weapon.FindComponent(WeaponComponent));
		if (!weapons || !weaponStorage || !weaponComponent)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(weapon);
			return false;
		}

		array<WeaponSlotComponent> slots = {};
		weapons.GetWeaponsSlots(slots);
		bool slotted = TryWeaponSlot(storage, weaponStorage, weapon, weaponComponent, slots, item.m_iWeaponSlot, true);
		if (!slotted)
			slotted = TryWeaponSlot(storage, weaponStorage, weapon, weaponComponent, slots, -1, false);

		if (!slotted)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(weapon);
			return false;
		}

		ApplyAttachments(character, storage, weapon, item);
		ApplyContents(character, storage, weapon, item);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool TryWeaponSlot(InventoryStorageManagerComponent storage, EquipedWeaponStorageComponent weaponStorage, IEntity weapon, WeaponComponent weaponComponent, array<WeaponSlotComponent> slots, int wantedSlot, bool requireEmpty)
	{
		foreach (WeaponSlotComponent slot : slots)
		{
			if (!slot)
				continue;

			if (wantedSlot >= 0 && slot.GetWeaponSlotIndex() != wantedSlot)
				continue;

			if (slot.GetWeaponSlotType().Compare(weaponComponent.GetWeaponSlotType()) != 0)
				continue;

			if (requireEmpty && slot.GetWeaponEntity())
				continue;

			if (storage.TryInsertItemInStorage(weapon, weaponStorage, slot.GetWeaponSlotIndex()))
				return true;

			if (!requireEmpty && storage.TryReplaceItem(weapon, weaponStorage, slot.GetWeaponSlotIndex()))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ApplyStored(IEntity character, notnull InventoryStorageManagerComponent storage, notnull JOLP_LoadoutItem item)
	{
		IEntity spawned = Spawn(item.m_sPrefab, character);
		if (!spawned)
			return false;

		BaseInventoryStorageComponent target = FindStorage(storage, item.m_iStoragePriority, item.m_eStoragePurpose);
		bool inserted = InsertItem(storage, spawned, target, item.m_iSlotIndex, true);

		if (!inserted)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(spawned);
			return false;
		}

		ApplyAttachments(character, storage, spawned, item);
		ApplyContents(character, storage, spawned, item);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ApplyContents(IEntity character, notnull InventoryStorageManagerComponent storage, IEntity container, notnull JOLP_LoadoutItem item)
	{
		if (!item.m_aChildren || item.m_aChildren.IsEmpty())
			return true;

		UniversalInventoryStorageComponent universal = UniversalInventoryStorageComponent.Cast(container.FindComponent(UniversalInventoryStorageComponent));
		foreach (JOLP_LoadoutItem child : item.m_aChildren)
		{
			if (!child)
				continue;

			if (!ApplyChild(character, storage, container, universal, child))
				Print(string.Format("[JOLP] Loadout skipped '%1'", child.m_sPrefab), LogLevel.WARNING);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ApplyChild(IEntity character, notnull InventoryStorageManagerComponent storage, IEntity container, UniversalInventoryStorageComponent universal, notnull JOLP_LoadoutItem child)
	{
		IEntity spawned = Spawn(child.m_sPrefab, container);
		if (!spawned)
			return false;

		bool inserted = false;
		BaseInventoryStorageComponent target = FindStorageOn(container, child.m_iStoragePriority, child.m_eStoragePurpose);
		if (target)
			inserted = InsertItem(storage, spawned, target, child.m_iSlotIndex, false);

		if (!inserted && universal && universal != target)
			inserted = InsertItem(storage, spawned, universal, child.m_iSlotIndex, false);

		if (!inserted)
			inserted = InsertIntoOwnedSlots(storage, container, spawned);

		if (!inserted)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(spawned);
			if (!child.m_aChildren || child.m_aChildren.IsEmpty())
				return false;

			IEntity existing = FindHolder(container, child.m_sPrefab);
			if (!existing)
				existing = container;

			ApplyContents(character, storage, existing, child);
			return true;
		}

		ApplyAttachments(character, storage, spawned, child);
		ApplyContents(character, storage, spawned, child);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ApplyAttachments(IEntity character, notnull InventoryStorageManagerComponent ownerStorage, IEntity weapon, notnull JOLP_LoadoutItem item)
	{
		if (!item.m_aAttachments || item.m_aAttachments.IsEmpty())
			return true;

		WeaponAttachmentsStorageComponent attachmentStorage = WeaponAttachmentsStorageComponent.Cast(weapon.FindComponent(WeaponAttachmentsStorageComponent));
		if (!attachmentStorage)
			return true;

		foreach (string attachmentPrefab : item.m_aAttachments)
		{
			if (HasAttachment(weapon, attachmentPrefab))
				continue;

			IEntity attachment = Spawn(attachmentPrefab, weapon);
			if (!attachment)
			{
				Print(string.Format("[JOLP] Loadout skipped attachment '%1'", attachmentPrefab), LogLevel.WARNING);
				continue;
			}

			bool inserted = false;
			InventoryStorageManagerComponent weaponStorage = InventoryStorageManagerComponent.Cast(weapon.FindComponent(InventoryStorageManagerComponent));
			if (weaponStorage)
				inserted = weaponStorage.TryInsertItem(attachment);

			if (!inserted)
			{
				array<BaseInventoryStorageComponent> slots = {};
				attachmentStorage.GetOwnedStorages(slots, 1, true);
				foreach (BaseInventoryStorageComponent slot : slots)
				{
					if (!slot)
						continue;

					if (ownerStorage.TryInsertItemInStorage(attachment, slot))
					{
						inserted = true;
						break;
					}
				}
			}

			if (!inserted)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(attachment);
				Print(string.Format("[JOLP] Loadout skipped attachment '%1'", attachmentPrefab), LogLevel.WARNING);
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasAttachment(IEntity weapon, ResourceName prefab)
	{
		WeaponAttachmentsStorageComponent storage = WeaponAttachmentsStorageComponent.Cast(weapon.FindComponent(WeaponAttachmentsStorageComponent));
		if (!storage)
			return false;

		array<BaseInventoryStorageComponent> slots = {};
		storage.GetOwnedStorages(slots, 1, true);
		foreach (BaseInventoryStorageComponent slot : slots)
		{
			if (!slot)
				continue;

			array<IEntity> attachments = {};
			slot.GetAll(attachments);
			foreach (IEntity attachment : attachments)
			{
				if (attachment && SCR_ResourceNameUtils.GetPrefabName(attachment) == prefab)
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ApplyQuickSlots(IEntity character, notnull InventoryStorageManagerComponent storage, notnull JOLP_PlayerLoadout loadout)
	{
		if (!loadout.m_aQuickSlots || loadout.m_aQuickSlots.IsEmpty())
			return;

		SCR_CharacterInventoryStorageComponent inventory = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (!inventory)
			return;

		array<IEntity> items = {};
		storage.GetItems(items);

		for (int i = 0; i < loadout.m_aQuickSlots.Count() && i < 10; i++)
		{
			string prefab = loadout.m_aQuickSlots[i];
			if (prefab.IsEmpty())
				continue;

			IEntity found;
			foreach (IEntity item : items)
			{
				if (item && SCR_ResourceNameUtils.GetPrefabName(item) == prefab)
				{
					found = item;
					break;
				}
			}

			if (found)
				inventory.StoreItemToQuickSlot(found, i, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static BaseInventoryStorageComponent FindStorage(InventoryStorageManagerComponent storage, int priority, EStoragePurpose purpose)
	{
		if (!storage)
			return null;

		array<BaseInventoryStorageComponent> storages = {};
		storage.GetStorages(storages);
		foreach (BaseInventoryStorageComponent candidate : storages)
		{
			if (candidate && candidate.GetPriority() == priority && candidate.GetPurpose() == purpose && !IsMedicalStorage(candidate))
				return candidate;
		}

		foreach (BaseInventoryStorageComponent candidate : storages)
		{
			BaseInventoryStorageComponent match = MatchStorage(candidate, priority, purpose, false);
			if (match)
				return match;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Worn clothes only fit a loadout area, and that area wants a replace even when it is empty.
	protected static bool InsertItem(InventoryStorageManagerComponent storage, IEntity item, BaseInventoryStorageComponent target, int slotIndex, bool wearCloth)
	{
		if (!storage || !item)
			return false;

		if (wearCloth && InsertCloth(storage, item))
			return true;

		if (target)
		{
			if (slotIndex >= 0 && storage.TryInsertItemInStorage(item, target, slotIndex))
				return true;

			if (ReplaceInSuitableSlot(storage, target, item))
				return true;

			if (storage.TryInsertItemInStorage(item, target))
				return true;
		}

		if (ReplaceInPurpose(storage, item, EStoragePurpose.PURPOSE_LOADOUT_PROXY))
			return true;

		BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(item.FindComponent(BaseLoadoutClothComponent));
		if (cloth)
			return false;

		return storage.TryInsertItem(item);
	}

	//------------------------------------------------------------------------------------------------
	//! The helmet is the head item that holds the accessories. A headset or balaclava must not take that slot.
	protected static void PrepareHeadGear(JOLP_PlayerLoadout loadout, IEntity near)
	{
		if (!loadout || !loadout.m_aItems)
			return;

		array<JOLP_LoadoutItem> covers = {};
		foreach (JOLP_LoadoutItem root : loadout.m_aItems)
			CollectHeadCovers(root, near, covers);

		JOLP_LoadoutItem helmet;
		int bestScore = -1000;
		foreach (JOLP_LoadoutItem cover : covers)
		{
			int score = HeadCoverScore(cover);
			if (score > bestScore)
			{
				bestScore = score;
				helmet = cover;
			}
		}

		if (!helmet)
			return;

		for (int i = 0; i < loadout.m_aItems.Count(); i++)
		{
			JOLP_LoadoutItem root = loadout.m_aItems[i];
			if (!root || root == helmet)
				continue;

			if (!Detach(root, helmet))
				continue;

			if (!ContainsPrefab(helmet, root.m_sPrefab))
				helmet.m_aChildren.Insert(root);

			int existing = loadout.m_aItems.Find(helmet);
			if (existing >= 0)
				loadout.m_aItems.Remove(i);
			else
				loadout.m_aItems.Set(i, helmet);

			break;
		}

		for (int i = loadout.m_aItems.Count() - 1; i >= 0; i--)
		{
			JOLP_LoadoutItem root = loadout.m_aItems[i];
			if (!root || root == helmet || root.m_bEquipped)
				continue;

			if (!IsHeadCoverItem(root, near) && !IsHeadAccessoryPrefab(root.m_sPrefab))
				continue;

			if (!ContainsPrefab(helmet, root.m_sPrefab))
				helmet.m_aChildren.Insert(root);

			loadout.m_aItems.Remove(i);
		}

		DedupeChildren(helmet);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DedupeChildren(JOLP_LoadoutItem item)
	{
		if (!item || !item.m_aChildren)
			return;

		for (int i = item.m_aChildren.Count() - 1; i >= 0; i--)
		{
			JOLP_LoadoutItem child = item.m_aChildren[i];
			if (!child)
				continue;

			for (int earlier = 0; earlier < i; earlier++)
			{
				JOLP_LoadoutItem kept = item.m_aChildren[earlier];
				if (kept && kept.m_sPrefab == child.m_sPrefab)
				{
					item.m_aChildren.Remove(i);
					break;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectHeadCovers(JOLP_LoadoutItem item, IEntity near, notnull array<JOLP_LoadoutItem> found)
	{
		if (!item)
			return;

		if (IsHeadCoverItem(item, near))
			found.Insert(item);

		if (!item.m_aChildren)
			return;

		foreach (JOLP_LoadoutItem child : item.m_aChildren)
			CollectHeadCovers(child, near, found);
	}

	//------------------------------------------------------------------------------------------------
	protected static int HeadCoverScore(JOLP_LoadoutItem item)
	{
		int score = 0;
		if (item.m_aChildren)
			score = item.m_aChildren.Count() * 10;

		string name = item.m_sPrefab;
		name.ToLower();
		if (name.Contains("helmet"))
			score = score + 8;

		if (name.Contains("headset") || name.Contains("balaclava"))
			score = score - 8;

		return score;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsHelmetPrefab(ResourceName prefab)
	{
		string name = prefab;
		name.ToLower();
		if (name.Contains("headset") || name.Contains("balaclava"))
			return false;

		return name.Contains("helmet");
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsHeadAccessoryPrefab(ResourceName prefab)
	{
		string name = prefab;
		name.ToLower();
		return name.Contains("headset") || name.Contains("balaclava");
	}

	//------------------------------------------------------------------------------------------------
	protected static bool AlreadyCarried(JOLP_PlayerLoadout loadout, notnull JOLP_LoadoutItem item)
	{
		if (!loadout.m_aItems || item.m_sPrefab.IsEmpty())
			return false;

		foreach (JOLP_LoadoutItem root : loadout.m_aItems)
		{
			if (!root || root == item)
				continue;

			if (ContainsPrefab(root, item.m_sPrefab))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ContainsPrefab(JOLP_LoadoutItem item, ResourceName prefab)
	{
		if (!item || !item.m_aChildren || prefab.IsEmpty())
			return false;

		foreach (JOLP_LoadoutItem child : item.m_aChildren)
		{
			if (!child)
				continue;

			if (child.m_sPrefab == prefab)
				return true;

			if (ContainsPrefab(child, prefab))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Detach(JOLP_LoadoutItem parent, JOLP_LoadoutItem target)
	{
		if (!parent || !parent.m_aChildren || !target)
			return false;

		for (int i = 0; i < parent.m_aChildren.Count(); i++)
		{
			if (parent.m_aChildren[i] == target)
			{
				parent.m_aChildren.Remove(i);
				return true;
			}
		}

		foreach (JOLP_LoadoutItem child : parent.m_aChildren)
		{
			if (Detach(child, target))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsHeadCoverItem(JOLP_LoadoutItem item, IEntity near)
	{
		if (!item)
			return false;

		IEntity spawned = Spawn(item.m_sPrefab, near);
		if (!spawned)
			return false;

		bool head = false;
		BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(spawned.FindComponent(BaseLoadoutClothComponent));
		if (cloth)
		{
			LoadoutAreaType area = cloth.GetAreaType();
			if (area && area.Type().IsInherited(LoadoutHeadCoverArea))
				head = true;
		}

		SCR_EntityHelper.DeleteEntityAndChildren(spawned);
		return head;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool BelongsTo(IEntity owner, IEntity container)
	{
		IEntity cursor = owner;
		while (cursor)
		{
			if (cursor == container)
				return true;

			cursor = cursor.GetParent();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A helmet or a jacket only fits the loadout area declared on the cloth. That slot is empty after the old kit is removed, so it is an insert.
	protected static bool InsertCloth(InventoryStorageManagerComponent storage, IEntity item)
	{
		IEntity owner = storage.GetOwner();
		if (!owner)
			return false;

		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(owner.FindComponent(EquipedLoadoutStorageComponent));
		BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(item.FindComponent(BaseLoadoutClothComponent));
		if (!loadout || !cloth)
			return false;

		LoadoutAreaType area = cloth.GetAreaType();
		if (!area)
			return false;

		LoadoutSlotInfo slot = loadout.GetSlotFromArea(area.Type());
		if (!slot)
			return false;

		BaseInventoryStorageComponent slotStorage = slot.GetStorage();
		if (!slotStorage)
			slotStorage = loadout;

		int slotId = slot.GetID();
		if (slotStorage.Get(slotId))
			return false;

		return storage.TryInsertItemInStorage(item, slotStorage, slotId);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ReplaceInPurpose(InventoryStorageManagerComponent storage, IEntity item, EStoragePurpose purpose)
	{
		array<BaseInventoryStorageComponent> storages = {};
		storage.GetStorages(storages, purpose);
		foreach (BaseInventoryStorageComponent candidate : storages)
		{
			if (ReplaceInSuitableSlot(storage, candidate, item))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ReplaceInSuitableSlot(InventoryStorageManagerComponent storage, BaseInventoryStorageComponent target, IEntity item)
	{
		if (!storage || !target || !item || IsMedicalStorage(target))
			return false;

		InventoryStorageSlot slot = target.FindSuitableSlotForItem(item);
		if (!slot)
			return false;

		int slotId = slot.GetID();
		if (slot.GetAttachedEntity())
			return false;

		return storage.TryInsertItemInStorage(item, target, slotId);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool InsertIntoOwnedSlots(InventoryStorageManagerComponent storage, IEntity container, IEntity item)
	{
		if (!storage || !container || !item)
			return false;

		array<BaseInventoryStorageComponent> storages = {};
		set<BaseInventoryStorageComponent> seen = new set<BaseInventoryStorageComponent>();
		CollectServingStorages(container, storages, seen, 0);
		foreach (BaseInventoryStorageComponent candidate : storages)
		{
			if (ReplaceInSuitableSlot(storage, candidate, item))
				return true;

			if (storage.TryInsertItemInStorage(item, candidate))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectServingStorages(IEntity container, notnull array<BaseInventoryStorageComponent> storages, notnull set<BaseInventoryStorageComponent> seen, int depth)
	{
		if (!container || depth > 5)
			return;

		if (depth == 0)
		{
			InventoryStorageManagerComponent manager = ManagerOf(container);
			if (manager)
			{
				array<BaseInventoryStorageComponent> listed = {};
				manager.GetStorages(listed, EStoragePurpose.PURPOSE_ANY);
				foreach (BaseInventoryStorageComponent storage : listed)
				{
					if (!storage || seen.Contains(storage) || IsBodyStorage(storage))
						continue;

					if (!StorageServes(storage, container))
						continue;

					seen.Insert(storage);
					storages.Insert(storage);
				}
			}
		}

		BaseInventoryStorageComponent direct = BaseInventoryStorageComponent.Cast(container.FindComponent(BaseInventoryStorageComponent));
		if (direct && !seen.Contains(direct) && !IsBodyStorage(direct))
		{
			seen.Insert(direct);
			storages.Insert(direct);
			array<BaseInventoryStorageComponent> nested = {};
			direct.GetOwnedStorages(nested, 1, true);
			foreach (BaseInventoryStorageComponent child : nested)
			{
				if (!child || seen.Contains(child) || IsBodyStorage(child))
					continue;

				seen.Insert(child);
				storages.Insert(child);
			}
		}

		SlotManagerComponent slots = SlotManagerComponent.Cast(container.FindComponent(SlotManagerComponent));
		if (slots)
		{
			array<EntitySlotInfo> infos = {};
			slots.GetSlotInfos(infos);
			foreach (EntitySlotInfo info : infos)
			{
				if (!info)
					continue;

				CollectServingStorages(info.GetAttachedEntity(), storages, seen, depth + 1);
			}
		}

		IEntity child = container.GetChildren();
		while (child)
		{
			CollectServingStorages(child, storages, seen, depth + 1);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindHolder(IEntity container, ResourceName prefab)
	{
		if (!container || prefab.IsEmpty())
			return null;

		IEntity child = container.GetChildren();
		while (child)
		{
			if (SCR_ResourceNameUtils.GetPrefabName(child) == prefab)
				return child;

			IEntity nested = FindHolder(child, prefab);
			if (nested)
				return nested;

			child = child.GetSibling();
		}

		SlotManagerComponent slots = SlotManagerComponent.Cast(container.FindComponent(SlotManagerComponent));
		if (!slots)
			return null;

		array<EntitySlotInfo> infos = {};
		slots.GetSlotInfos(infos);
		foreach (EntitySlotInfo info : infos)
		{
			if (!info)
				continue;

			IEntity attached = info.GetAttachedEntity();
			if (!attached)
				continue;

			if (SCR_ResourceNameUtils.GetPrefabName(attached) == prefab)
				return attached;

			IEntity nested = FindHolder(attached, prefab);
			if (nested)
				return nested;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static BaseInventoryStorageComponent MatchStorage(BaseInventoryStorageComponent storage, int priority, EStoragePurpose purpose, bool includeSelf)
	{
		if (!storage || IsMedicalStorage(storage) || WeaponAttachmentsStorageComponent.Cast(storage))
			return null;

		if (includeSelf && storage.GetPriority() == priority && storage.GetPurpose() == purpose)
			return storage;

		array<BaseInventoryStorageComponent> nested = {};
		storage.GetOwnedStorages(nested, 1, true);
		foreach (BaseInventoryStorageComponent child : nested)
		{
			BaseInventoryStorageComponent match = MatchStorage(child, priority, purpose, true);
			if (match)
				return match;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsMedicalStorage(BaseInventoryStorageComponent storage)
	{
		if (SCR_IdentityItemStorageComponent.Cast(storage))
			return true;

		if (SCR_SalineStorageComponent.Cast(storage))
			return true;

		if (SCR_TourniquetStorageComponent.Cast(storage))
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static BaseInventoryStorageComponent FindStorageOn(IEntity container, int priority, EStoragePurpose purpose)
	{
		InventoryStorageManagerComponent manager = ManagerOf(container);
		if (manager && container)
		{
			array<BaseInventoryStorageComponent> storages = {};
			manager.GetStorages(storages, EStoragePurpose.PURPOSE_ANY);
			foreach (BaseInventoryStorageComponent candidate : storages)
			{
				if (!candidate || !StorageServes(candidate, container))
					continue;

				BaseInventoryStorageComponent match = MatchStorage(candidate, priority, purpose, true);
				if (match)
					return match;
			}
		}

		BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(container.FindComponent(BaseInventoryStorageComponent));
		return MatchStorage(storage, priority, purpose, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectCharacterEquipment(IEntity character, notnull array<IEntity> roots)
	{
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!manager)
			return;

		set<BaseInventoryStorageComponent> seen = new set<BaseInventoryStorageComponent>();
		array<BaseInventoryStorageComponent> storages = {};
		manager.GetStorages(storages, EStoragePurpose.PURPOSE_ANY);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!storage || storage.GetOwner() != character || IsBodyStorage(storage))
				continue;

			EquipedLoadoutStorageComponent loadoutStorage = EquipedLoadoutStorageComponent.Cast(storage);
			if (loadoutStorage)
			{
				array<BaseInventoryStorageComponent> nested = {};
				loadoutStorage.GetOwnedStorages(nested, 1, true);
				foreach (BaseInventoryStorageComponent child : nested)
				{
					if (!child || IsBodyStorage(child))
						continue;

					CollectLoose(child, roots, seen);
				}

				continue;
			}

			CollectLoose(storage, roots, seen);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectLoose(BaseInventoryStorageComponent storage, notnull array<IEntity> roots, notnull set<BaseInventoryStorageComponent> seen)
	{
		if (!storage || seen.Contains(storage) || IsBodyStorage(storage))
			return;

		seen.Insert(storage);
		int slots = storage.GetSlotsCount();
		for (int i = 0; i < slots; i++)
		{
			IEntity entity = storage.Get(i);
			if (entity && roots.Find(entity) == -1)
				roots.Insert(entity);
		}

		array<IEntity> items = {};
		storage.GetAll(items);
		foreach (IEntity entity : items)
		{
			if (entity && roots.Find(entity) == -1)
				roots.Insert(entity);
		}

		array<BaseInventoryStorageComponent> nested = {};
		storage.GetOwnedStorages(nested, 1, true);
		foreach (BaseInventoryStorageComponent child : nested)
			CollectLoose(child, roots, seen);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Spawn(ResourceName prefab, IEntity near)
	{
		if (prefab.IsEmpty() || !near)
			return null;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = near.GetOrigin();
		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
	}
}
