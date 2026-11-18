//------------------------------------------------------------------------------------------------
//! Sets a civilian's faction, puts a bomb vest on a terrorist, and arms a partisan when the fight starts.
//! Clothes stay on the character prefab. Swapping them after spawn drops the old outfit on the ground.
class JOLP_CivilianOutfitter : Managed
{
	//------------------------------------------------------------------------------------------------
	static void Prepare(IEntity character, notnull JOLP_CivilianLiveRecord record, notnull JOLP_CivilianConfig config)
	{
		if (!character)
			return;

		SetCharacterFaction(character, config.m_sCivilianFactionKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Rifle and magazines. Called when a partisan joins the fight, not at spawn.
	//! The rifle is drawn on a short delay so the weapon slot exists before the character selects it.
	static void ArmPartisan(IEntity character, notnull JOLP_CivilianConfig config)
	{
		if (!character)
			return;

		if (!EquipWeapon(character, config.m_rPartisanWeaponPrefab))
			return;

		int magazines = config.m_iPartisanMagazines;
		if (magazines < 1)
			magazines = 1;

		for (int i = 0; i < magazines; i++)
			InsertItem(character, config.m_rPartisanMagazinePrefab);

		GetGame().GetCallqueue().CallLater(ReadyWeapon, 400, false, character, false);
		GetGame().GetCallqueue().CallLater(ReadyWeapon, 1200, false, character, true);
	}

	//------------------------------------------------------------------------------------------------
	static void SetGroupFaction(SCR_AIGroup group, string factionKey)
	{
		if (!group || factionKey.IsEmpty())
			return;

		FactionManager manager = GetGame().GetFactionManager();
		if (!manager)
			return;

		Faction faction = manager.GetFactionByKey(factionKey);
		if (!faction)
			return;

		group.SetFaction(faction);

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			SetCharacterFaction(agent.GetControlledEntity(), factionKey);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void SetCharacterFaction(IEntity character, string factionKey)
	{
		if (!character || factionKey.IsEmpty())
			return;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (affiliation)
			affiliation.SetAffiliatedFactionByKey(factionKey);
	}

	//------------------------------------------------------------------------------------------------
	static bool SeatDriver(IEntity character, IEntity vehicle)
	{
		if (!character || !vehicle)
			return false;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return false;

		if (access.IsInCompartment())
			return true;

		return access.MoveInVehicle(vehicle, ECompartmentType.PILOT);
	}

	//------------------------------------------------------------------------------------------------
	//! Driver first, then a turret, then a cargo seat. False when the character or the vehicle is full.
	static bool SeatAny(IEntity character, IEntity vehicle)
	{
		if (!character || !vehicle)
			return false;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return false;

		if (access.IsInCompartment())
			return true;

		if (access.MoveInVehicle(vehicle, ECompartmentType.PILOT))
			return true;

		if (access.MoveInVehicle(vehicle, ECompartmentType.TURRET))
			return true;

		return access.MoveInVehicle(vehicle, ECompartmentType.CARGO);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsPlayerInstigator(IEntity instigator)
	{
		if (!instigator)
			return false;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return false;

		if (players.GetPlayerIdFromControlledEntity(instigator) > 0)
			return true;

		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(
			instigator.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartments)
			return false;

		if (OccupantIsPlayer(compartments, players, ECompartmentType.PILOT))
			return true;

		return OccupantIsPlayer(compartments, players, ECompartmentType.CARGO);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool OccupantIsPlayer(SCR_BaseCompartmentManagerComponent compartments, PlayerManager players, ECompartmentType compartmentType)
	{
		array<IEntity> occupants = {};
		compartments.GetOccupantsOfType(occupants, compartmentType);
		foreach (IEntity occupant : occupants)
		{
			if (occupant && players.GetPlayerIdFromControlledEntity(occupant) > 0)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts one clothing prefab on the character. True when a loadout slot accepted it.
	static bool WearPrefab(IEntity character, ResourceName prefab)
	{
		if (!character || prefab.IsEmpty())
			return false;

		InventoryStorageManagerComponent storageManager = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!storageManager)
			return false;

		IEntity item = SpawnItem(prefab, character.GetOrigin());
		if (!item)
		{
			Print(string.Format("[JOLP] Bomb vest prefab failed to spawn: '%1'", prefab), LogLevel.WARNING);
			return false;
		}

		array<BaseInventoryStorageComponent> storages = {};
		storageManager.GetStorages(storages, EStoragePurpose.PURPOSE_LOADOUT_PROXY);
		foreach (BaseInventoryStorageComponent loadoutStorage : storages)
		{
			if (!loadoutStorage)
				continue;

			InventoryStorageSlot suitableSlot = loadoutStorage.FindSuitableSlotForItem(item);
			if (!suitableSlot)
				continue;

			if (storageManager.TryReplaceItem(item, loadoutStorage, suitableSlot.GetID()))
				return true;
		}

		Print("[JOLP] Bomb vest found no clothing slot", LogLevel.WARNING);
		SCR_EntityHelper.DeleteEntityAndChildren(item);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the rifle is in a primary weapon slot. Never dump it in a hand/inventory slot —
	//! that leaves them holding it one-handed and the AI will not fire.
	protected static bool EquipWeapon(IEntity character, ResourceName prefab)
	{
		if (!character || prefab.IsEmpty())
			return false;

		IEntity weapon = SpawnItem(prefab, character.GetOrigin());
		if (!weapon)
			return false;

		InventoryStorageManagerComponent storage = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		EquipedWeaponStorageComponent weaponStorage = EquipedWeaponStorageComponent.Cast(character.FindComponent(EquipedWeaponStorageComponent));
		WeaponComponent weaponComponent = WeaponComponent.Cast(weapon.FindComponent(WeaponComponent));

		if (storage && weaponManager && weaponStorage && weaponComponent)
		{
			array<WeaponSlotComponent> slots = {};
			weaponManager.GetWeaponsSlots(slots);
			foreach (WeaponSlotComponent slot : slots)
			{
				if (!slot || slot.GetWeaponEntity())
					continue;

				if (slot.GetWeaponSlotType().Compare(weaponComponent.GetWeaponSlotType()) != 0)
					continue;

				if (storage.TryInsertItemInStorage(weapon, weaponStorage, slot.GetWeaponSlotIndex()))
					return true;
			}
		}

		Print("[JOLP] Partisan rifle found no primary weapon slot", LogLevel.WARNING);
		SCR_EntityHelper.DeleteEntityAndChildren(weapon);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Move the slung rifle into the hands and load a magazine from the pockets.
	//! The later call may spawn a loose magazine when the clothes had no room for one.
	protected static void ReadyWeapon(IEntity character, bool allowLooseMagazine)
	{
		if (!character)
			return;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!controller || !weaponManager)
			return;

		WeaponSlotComponent heldSlot = null;
		BaseWeaponComponent weaponComp = null;
		array<WeaponSlotComponent> slots = {};
		weaponManager.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
		{
			if (!slot)
				continue;

			IEntity weaponEnt = slot.GetWeaponEntity();
			if (!weaponEnt)
				continue;

			heldSlot = slot;
			weaponComp = BaseWeaponComponent.Cast(weaponEnt.FindComponent(WeaponComponent));
			if (!weaponComp)
				weaponComp = BaseWeaponComponent.Cast(weaponEnt.FindComponent(BaseWeaponComponent));
			break;
		}

		if (!heldSlot)
			return;

		if (weaponComp)
		{
			weaponManager.SelectWeapon(weaponComp);
			controller.SelectWeapon(weaponComp);
		}
		else
		{
			weaponManager.SelectWeapon(heldSlot);
			controller.SelectWeapon(heldSlot);
		}

		BaseWeaponComponent current = weaponManager.GetCurrentWeapon();
		if (!current)
			current = weaponComp;
		if (!current)
			current = heldSlot;

		BaseMuzzleComponent muzzle = current.GetCurrentMuzzle();
		if (!muzzle || !muzzle.GetMagazine())
		{
			if (!controller.ReloadWeapon() && allowLooseMagazine)
				LoadLooseMagazine(character, controller);
		}

		controller.SetWeaponRaised(true);

		SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(character.FindComponent(SCR_AICombatComponent));
		if (combat)
			combat.SetHoldFire(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Civilian clothes often have no pouch. Put one magazine straight into the rifle.
	protected static void LoadLooseMagazine(IEntity character, CharacterControllerComponent controller)
	{
		JOLP_CivilianManagerComponent manager = JOLP_CivilianManagerComponent.GetInstance();
		if (!manager)
			return;

		ResourceName magazinePrefab = manager.GetConfig().m_rPartisanMagazinePrefab;
		IEntity magazine = SpawnItem(magazinePrefab, character.GetOrigin());
		if (!magazine)
			return;

		if (controller.ReloadWeaponWith(magazine, true))
			return;

		InventoryStorageManagerComponent storage = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!storage || !storage.TryInsertItem(magazine))
			SCR_EntityHelper.DeleteEntityAndChildren(magazine);
	}

	//------------------------------------------------------------------------------------------------
	protected static void InsertItem(IEntity character, ResourceName prefab)
	{
		if (!character || prefab.IsEmpty())
			return;

		InventoryStorageManagerComponent storageManager = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!storageManager)
			return;

		IEntity item = SpawnItem(prefab, character.GetOrigin());
		if (!item)
			return;

		if (!storageManager.TryInsertItem(item))
			SCR_EntityHelper.DeleteEntityAndChildren(item);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity SpawnItem(ResourceName prefab, vector origin)
	{
		if (prefab.IsEmpty())
			return null;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = origin;
		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
	}
}
