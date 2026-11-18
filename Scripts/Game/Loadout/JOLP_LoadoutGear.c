//------------------------------------------------------------------------------------------------
//! Reads a character's kit, prices it from the arsenal catalog, and puts a saved kit back on.
//! Unpriced prefabs cost 0. Quick slots are not priced: they point at items already in the tree.
class JOLP_LoadoutGear
{
	//------------------------------------------------------------------------------------------------
	static void Extract(IEntity character, notnull JOLP_PlayerLoadout loadout)
	{
		loadout.ClearItems();
		if (!character)
			return;

		set<IEntity> equipped = new set<IEntity>();
		ExtractWeapons(character, loadout, equipped);
		ExtractRootStorage(character, loadout, equipped);
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
	static string Describe(JOLP_PlayerLoadout loadout, SCR_EArsenalSupplyCostType costType, int wornCost, int supplies)
	{
		int savedCost = Price(loadout, costType);
		int net = savedCost - wornCost;

		string body = "";
		if (!loadout || !loadout.m_aItems || loadout.m_aItems.IsEmpty())
		{
			body = "Nothing equipped.";
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
				if (!body.IsEmpty())
					body = body + "\n";

				body = body + string.Format("%1 x%2    %3", DisplayName(prefab), count, line);
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

		return string.Format("%1\n\nWearing %2\nLoadout %3\n%4\nSupplies %5%6", body, wornCost, savedCost, settlement, supplies, afford);
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

		if (loadout.m_aItems)
		{
			foreach (JOLP_LoadoutItem item : loadout.m_aItems)
			{
				if (!item)
					continue;

				bool placed;
				if (item.m_bEquipped)
					placed = ApplyWeapon(character, storage, item);
				else
					placed = ApplyStored(character, storage, item);

				if (!placed)
				{
					Print(string.Format("[JOLP] Loadout could not place '%1'", item.m_sPrefab), LogLevel.WARNING);
					error = "The loadout could not be equipped.";
					return false;
				}
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
			ExtractContents(weapon, item);
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
			ExtractContents(entity, item);
			loadout.m_aItems.Insert(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void ExtractContents(IEntity container, notnull JOLP_LoadoutItem parent)
	{
		if (!container)
			return;

		UniversalInventoryStorageComponent universal = UniversalInventoryStorageComponent.Cast(container.FindComponent(UniversalInventoryStorageComponent));
		if (universal)
		{
			ExtractStorage(universal, parent);
			return;
		}

		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(container.FindComponent(InventoryStorageManagerComponent));
		if (!manager)
			return;

		array<BaseInventoryStorageComponent> storages = {};
		manager.GetStorages(storages, EStoragePurpose.PURPOSE_ANY);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!storage || WeaponAttachmentsStorageComponent.Cast(storage))
				continue;

			ExtractStorage(storage, parent);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void ExtractStorage(BaseInventoryStorageComponent storage, notnull JOLP_LoadoutItem parent)
	{
		if (!storage)
			return;

		int priority = storage.GetPriority();
		EStoragePurpose purpose = storage.GetPurpose();
		int slots = storage.GetSlotsCount();
		for (int i = 0; i < slots; i++)
		{
			IEntity entity = storage.Get(i);
			if (!entity)
				continue;

			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
			if (prefab.IsEmpty())
				continue;

			JOLP_LoadoutItem child = new JOLP_LoadoutItem();
			child.m_sPrefab = prefab;
			child.m_iSlotIndex = i;
			child.m_iStoragePriority = priority;
			child.m_eStoragePurpose = purpose;
			ExtractAttachments(entity, child);
			ExtractContents(entity, child);
			parent.m_aChildren.Insert(child);
		}
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

		if (!ApplyAttachments(character, storage, weapon, item))
			return false;

		return ApplyContents(character, storage, weapon, item);
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
		bool inserted = false;
		if (target)
			inserted = storage.TryInsertItemInStorage(spawned, target, item.m_iSlotIndex);

		if (!inserted && target)
			inserted = storage.TryInsertItemInStorage(spawned, target);

		if (!inserted)
			inserted = storage.TryInsertItem(spawned);

		if (!inserted)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(spawned);
			return false;
		}

		if (!ApplyAttachments(character, storage, spawned, item))
			return false;

		return ApplyContents(character, storage, spawned, item);
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
				return false;
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
		if (universal)
		{
			inserted = storage.TryInsertItemInStorage(spawned, universal, child.m_iSlotIndex);
			if (!inserted)
				inserted = storage.TryInsertItemInStorage(spawned, universal);
		}
		else
		{
			BaseInventoryStorageComponent target = FindStorageOn(container, child.m_iStoragePriority, child.m_eStoragePurpose);
			if (target)
			{
				inserted = storage.TryInsertItemInStorage(spawned, target, child.m_iSlotIndex);
				if (!inserted)
					inserted = storage.TryInsertItemInStorage(spawned, target);
			}
		}

		if (!inserted)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(spawned);
			return false;
		}

		if (!ApplyAttachments(character, storage, spawned, child))
			return false;

		return ApplyContents(character, storage, spawned, child);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ApplyAttachments(IEntity character, notnull InventoryStorageManagerComponent ownerStorage, IEntity weapon, notnull JOLP_LoadoutItem item)
	{
		if (!item.m_aAttachments || item.m_aAttachments.IsEmpty())
			return true;

		WeaponAttachmentsStorageComponent attachmentStorage = WeaponAttachmentsStorageComponent.Cast(weapon.FindComponent(WeaponAttachmentsStorageComponent));
		if (!attachmentStorage)
			return false;

		foreach (string attachmentPrefab : item.m_aAttachments)
		{
			if (HasAttachment(weapon, attachmentPrefab))
				continue;

			IEntity attachment = Spawn(attachmentPrefab, weapon);
			if (!attachment)
				return false;

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
				return false;
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
			if (candidate && candidate.GetPriority() == priority && candidate.GetPurpose() == purpose)
				return candidate;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static BaseInventoryStorageComponent FindStorageOn(IEntity container, int priority, EStoragePurpose purpose)
	{
		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(container.FindComponent(InventoryStorageManagerComponent));
		BaseInventoryStorageComponent found = FindStorage(manager, priority, purpose);
		if (found)
			return found;

		BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(container.FindComponent(BaseInventoryStorageComponent));
		if (storage && !WeaponAttachmentsStorageComponent.Cast(storage))
			return storage;

		return null;
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
