//------------------------------------------------------------------------------------------------
//! Reads/writes SUPPLIES used by the HQ building provider.
//!
//! Conflict Storage_Supplies piles are often EditorLink-spawned near the HQ, so collection:
//! 1) walks the HQ hierarchy for any SUPPLIES container
//! 2) probes a short radius around the HQ / Storage_Supplies yard
//------------------------------------------------------------------------------------------------
class JOLP_SupplyStock
{
	protected static const float NEARBY_STORAGE_RADIUS = 20.0;
	protected static const float DEPOT_SUPPLY_SEARCH_RADIUS = 400.0;

	protected static ref array<SCR_ResourceContainer> s_aQueryHits;
	protected static vector s_vSupplyQueryOrigin;
	protected static IEntity s_SupplyQueryHit;
	protected static float s_fSupplyQueryBestDistSq;

	//------------------------------------------------------------------------------------------------
	//! HQ pile a depot can spend. The depot itself often has no Storage_Supplies;
	//! the stock the player watches is on the base the depot was built from.
	static IEntity ResolveRoot(IEntity hint)
	{
		IEntity best;
		float bestStock = -1;

		ConsiderStockRoot(hint, best, bestStock);
		if (!hint)
			return best;

		SCR_CampaignBuildingProviderComponent provider = FindProvider(hint, 0);
		if (provider)
			ConsiderStockRoot(provider.GetMasterProviderEntity(), best, bestStock);

		ConsiderStockRoot(FindNearestMasterOwner(hint.GetOrigin(), 500), best, bestStock);
		ConsiderStockRoot(FindStorageEntityNear(hint.GetOrigin(), DEPOT_SUPPLY_SEARCH_RADIUS), best, bestStock);
		return best;
	}

	//------------------------------------------------------------------------------------------------
	static float Read(IEntity root)
	{
		array<SCR_ResourceContainer> containers = Collect(root);
		if (!containers || containers.IsEmpty())
			return -1;

		float total;
		foreach (SCR_ResourceContainer container : containers)
		{
			if (container)
				total = total + container.GetResourceValue();
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Fill HQ storage to at least `value`. Prefers Storage_Supplies generator, then containers.
	static bool Write(IEntity root, float value)
	{
		if (!root || value < 0)
			return false;

		float currentTotal = Read(root);
		if (currentTotal + 0.01 >= value)
			return true;

		if (currentTotal < 0)
			currentTotal = 0;

		float remaining = value - currentTotal;

		// BuildingService FreeBuilding generator Store action fills Storage_Supplies
		// through the encapsulator, which drives child crate meshes.
		if (TryProviderGeneration(root, remaining))
		{
			if (Read(root) + 0.01 >= value)
				return true;

			remaining = value - Math.Max(Read(root), 0);
		}

		if (TryStorageGeneration(root, remaining))
		{
			if (Read(root) + 0.01 >= value)
				return true;

			remaining = value - Math.Max(Read(root), 0);
		}

		// Prefer the Storage_Supplies virtual container — SetResourceValue routes through
		// the encapsulator so SupplyStack children (crate meshes) match the fill level.
		SCR_ResourceContainer storageVirtual = FindStorageVirtualContainer(root);
		if (storageVirtual)
		{
			float maxValue = storageVirtual.GetMaxResourceValue();
			if (maxValue + 0.01 < value)
				storageVirtual.SetMaxResourceValue(value, true);

			float current = storageVirtual.GetResourceValue();
			if (current + 0.01 < value)
				storageVirtual.SetResourceValue(value, true);

			if (Read(root) + 0.01 >= value)
				return true;

			remaining = value - Math.Max(Read(root), 0);
		}

		array<SCR_ResourceContainer> containers = Collect(root);
		if (!containers || containers.IsEmpty())
			return Read(root) + 0.01 >= value;

		float maxTotal;
		foreach (SCR_ResourceContainer container : containers)
		{
			if (container)
				maxTotal = maxTotal + Math.Max(container.GetMaxResourceValue(), 0);
		}

		// Virtual Storage_Supplies container starts at max 0 until encapsulator raises it.
		if (maxTotal + 0.01 < value)
		{
			float need = value - maxTotal;
			int count = Math.Max(containers.Count(), 1);
			foreach (SCR_ResourceContainer container : containers)
			{
				if (!container || need <= 0)
					break;

				float raise = Math.Max(need / count, 1);
				container.SetMaxResourceValue(container.GetMaxResourceValue() + raise, true);
				need = need - raise;
			}
		}

		foreach (SCR_ResourceContainer container : containers)
		{
			if (!container || remaining <= 0.01)
				break;

			float current = container.GetResourceValue();
			float maxValue = container.GetMaxResourceValue();
			if (maxValue <= current + 0.01)
				continue;

			float add = Math.Min(remaining, maxValue - current);
			float target = current + add;
			// Prefer safe setter so encapsulators can update SupplyStack crate meshes.
			if (!container.SetResourceValue(target, true))
				container.SetResourceValueUnsafe(target, true);

			float applied = container.GetResourceValue() - current;
			if (applied > 0)
				remaining = remaining - applied;
		}

		return Read(root) + 0.01 >= value;
	}

	//------------------------------------------------------------------------------------------------
	//! The pile the HQ shows. Read() also adds the child crates, which mirror this value,
	//! so the sum is about twice the number on screen.
	static float ReadDisplayed(IEntity hint)
	{
		if (!hint)
			return -1;

		IEntity root = ResolveRoot(hint);
		if (!root)
			root = hint;

		SCR_ResourceContainer storage = FindStorageVirtualContainer(root);
		if (!storage)
			return Read(root);

		return storage.GetResourceValue();
	}

	//------------------------------------------------------------------------------------------------
	//! Set the Storage_Supplies pile the build menu displays. Read() also sums the child
	//! crates, so writing that total makes the menu show about half of what was asked.
	static bool SetStorageValue(IEntity root, float value)
	{
		if (!root || value < 0)
			return false;

		SCR_ResourceContainer storageVirtual = FindStorageVirtualContainer(root);
		if (!storageVirtual)
			return false;

		float maxValue = storageVirtual.GetMaxResourceValue();
		if (maxValue + 0.01 < value)
			storageVirtual.SetMaxResourceValue(value, true);

		storageVirtual.SetResourceValue(value, true);
		RefreshStorageVisuals(root);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool Add(IEntity root, float amount)
	{
		if (amount <= 0)
			return false;

		float current = Read(root);
		if (current < 0)
			current = 0;

		return Write(root, current + amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Add `amount` onto HQ Storage_Supplies itself. Read() sums every child container, so using
	//! that total as a write target can miss the pile the base actually spends.
	static bool AddDelta(IEntity hint, float amount)
	{
		if (!hint || amount <= 0)
			return false;

		IEntity root = ResolveRoot(hint);
		if (!root)
			root = hint;

		SCR_ResourceContainer container = FindStorageVirtualContainer(root);
		if (!container)
			return false;

		float before = container.GetResourceValue();
		SCR_ResourceEncapsulator encapsulator = container.GetResourceEncapsulator();
		if (encapsulator)
			GrowEncapsulator(encapsulator, amount);
		else
		{
			float space = container.GetMaxResourceValue() - before;
			if (space + 0.01 < amount)
				container.SetMaxResourceValue(before + amount, true);

			container.IncreaseResourceValue(amount);
		}

		RefreshStorageVisuals(root);
		return container.GetResourceValue() > before + 0.01;
	}

	//------------------------------------------------------------------------------------------------
	protected static void GrowEncapsulator(SCR_ResourceEncapsulator encapsulator, float amount)
	{
		SCR_ResourceContainerQueueBase queue = encapsulator.GetContainerQueue();
		if (!queue || amount <= 0)
			return;

		int count = queue.GetContainerCount();
		if (count <= 0)
			return;

		SCR_ResourceContainer child = queue.GetContainerAt(0);
		if (!child)
			return;

		float before = child.GetResourceValue();
		float space = child.GetMaxResourceValue() - before;
		if (space + 0.01 < amount)
			child.SetMaxResourceValue(before + amount, true);

		child.IncreaseResourceValue(amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Remove supplies from HQ Storage_Supplies (FreeRoam place cost when consumer queue misses stock).
	static bool Consume(IEntity root, float amount)
	{
		if (!root || amount <= 0)
			return false;

		float remaining = amount;

		SCR_ResourceContainer storageVirtual = FindStorageVirtualContainer(root);
		if (storageVirtual)
		{
			float current = storageVirtual.GetResourceValue();
			float used = Math.Min(remaining, current);
			if (used > 0)
			{
				SCR_ResourceEncapsulator encapsulator = storageVirtual.GetResourceEncapsulator();
				if (encapsulator)
					encapsulator.RequestConsumtion(used);
				else
					storageVirtual.DecreaseResourceValue(used);

				remaining -= used;
			}
		}

		if (remaining <= 0.01)
		{
			RefreshStorageVisuals(root);
			return true;
		}

		array<SCR_ResourceContainer> containers = Collect(root);
		foreach (SCR_ResourceContainer container : containers)
		{
			if (!container || remaining <= 0)
				continue;

			if (storageVirtual && container == storageVirtual)
				continue;

			float current = container.GetResourceValue();
			float used = Math.Min(remaining, current);
			if (used <= 0)
				continue;

			SCR_ResourceEncapsulator encapsulator = container.GetResourceEncapsulator();
			if (encapsulator)
				encapsulator.RequestConsumtion(used);
			else
				container.DecreaseResourceValue(used);

			remaining -= used;
		}

		RefreshStorageVisuals(root);
		return remaining <= 0.01;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-apply the Storage_Supplies virtual value through SetResourceValue so the
	//! encapsulator redistributes into SupplyStack children (visible crate piles).
	static void RefreshStorageVisuals(IEntity root)
	{
		SCR_ResourceContainer storageVirtual = FindStorageVirtualContainer(root);
		if (!storageVirtual)
			return;

		float value = storageVirtual.GetResourceValue();
		if (value < 0)
			return;

		// Nudge then restore so encapsulator RequestGeneration/Consumtion runs even when
		// the world layer already stamped the same absolute value.
		if (value > 0)
		{
			storageVirtual.SetResourceValue(0, true);
			storageVirtual.SetResourceValue(value, true);
		}
		else
		{
			storageVirtual.SetResourceValue(0, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_ResourceContainer FindStorageVirtualContainer(IEntity root)
	{
		IEntity storage = FindNamedChild(root, "Storage_Supplies", 0);
		if (!storage)
			return null;

		SCR_ResourceComponent resource = SCR_ResourceComponent.Cast(storage.FindComponent(SCR_ResourceComponent));
		if (!resource)
			resource = SCR_ResourceComponent.FindResourceComponent(storage);

		if (!resource)
			return null;

		return resource.GetContainer(EResourceType.SUPPLIES);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool TryStorageGeneration(IEntity root, float amount)
	{
		if (!root || amount <= 0)
			return false;

		IEntity storage = FindNamedChild(root, "Storage_Supplies", 0);
		if (!storage)
			return false;

		SCR_ResourceComponent resource = SCR_ResourceComponent.Cast(storage.FindComponent(SCR_ResourceComponent));
		if (!resource)
			resource = SCR_ResourceComponent.FindResourceComponent(storage);

		if (!resource)
			return false;

		SCR_ResourceGenerator generator = SCR_ResourceSystemHelper.GetFirstValidGenerator(resource, EResourceType.SUPPLIES);
		if (!generator)
			return false;

		generator.RequestGeneration(amount);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool TryProviderGeneration(IEntity root, float amount)
	{
		if (!root || amount <= 0)
			return false;

		SCR_CampaignBuildingProviderComponent provider = FindProvider(root, 0);
		if (!provider)
			return false;

		SCR_ResourceComponent resource = provider.GetResourceComponent();
		if (!resource)
			resource = SCR_ResourceComponent.FindResourceComponent(provider.GetOwner());

		if (!resource)
			return false;

		SCR_ResourceGenerator generator = SCR_ResourceSystemHelper.GetFirstValidGenerator(resource, EResourceType.SUPPLIES);
		if (!generator)
			return false;

		generator.RequestGeneration(amount);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ConsiderStockRoot(IEntity entity, out IEntity best, out float bestStock)
	{
		if (!entity)
			return;

		while (entity.GetParent())
			entity = entity.GetParent();

		float stock = Read(entity);
		if (stock < 0)
			return;

		if (!best || stock > bestStock)
		{
			best = entity;
			bestStock = stock;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindNearestMasterOwner(vector origin, float maxDistance)
	{
		SCR_MilitaryBaseSystem baseSystem = SCR_MilitaryBaseSystem.GetInstance();
		if (!baseSystem)
			return null;

		SCR_CampaignBuildingProviderComponent best;
		float bestDistSq = maxDistance * maxDistance;
		array<SCR_MilitaryBaseComponent> bases = {};
		baseSystem.GetBases(bases);

		foreach (SCR_MilitaryBaseComponent base : bases)
		{
			if (!base)
				continue;

			IEntity baseOwner = base.GetOwner();
			if (!baseOwner)
				continue;

			float distSq = vector.DistanceSqXZ(origin, baseOwner.GetOrigin());
			if (distSq > bestDistSq)
				continue;

			SCR_CampaignBuildingProviderComponent provider = FindProvider(baseOwner, 0);
			if (!provider || !provider.IsMasterProvider())
				continue;

			best = provider;
			bestDistSq = distSq;
		}

		if (!best)
			return null;

		return best.GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindStorageEntityNear(vector origin, float radius)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		s_vSupplyQueryOrigin = origin;
		s_SupplyQueryHit = null;
		s_fSupplyQueryBestDistSq = radius * radius;

		world.QueryEntitiesBySphere(
			origin,
			radius,
			CaptureSupplyStorage,
			null,
			EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);

		IEntity hit = s_SupplyQueryHit;
		s_SupplyQueryHit = null;
		return hit;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CaptureSupplyStorage(IEntity entity)
	{
		if (!entity)
			return true;

		IEntity storage = FindNamedChild(entity, "Storage_Supplies", 0);
		if (!storage)
			return true;

		float distSq = vector.DistanceSq(storage.GetOrigin(), s_vSupplyQueryOrigin);
		if (distSq >= s_fSupplyQueryBestDistSq)
			return true;

		s_fSupplyQueryBestDistSq = distSq;
		s_SupplyQueryHit = storage;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_CampaignBuildingProviderComponent FindProvider(IEntity entity, int depth)
	{
		if (!entity || depth > 8)
			return null;

		SCR_CampaignBuildingProviderComponent provider = SCR_CampaignBuildingProviderComponent.Cast(
			entity.FindComponent(SCR_CampaignBuildingProviderComponent));
		if (provider)
			return provider;

		IEntity child = entity.GetChildren();
		while (child)
		{
			SCR_CampaignBuildingProviderComponent nested = FindProvider(child, depth + 1);
			if (nested)
				return nested;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<SCR_ResourceContainer> Collect(IEntity root)
	{
		array<SCR_ResourceContainer> containers = new array<SCR_ResourceContainer>();
		if (!root)
			return containers;

		// Prefer the HQ Storage_Supplies yard (virtual container + child stacks).
		IEntity storage = FindNamedChild(root, "Storage_Supplies", 0);
		if (storage)
			CollectFromTree(storage, 0, containers);

		CollectFromTree(root, 0, containers);
		CollectNearby(root, containers);
		return containers;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectFromTree(IEntity entity, int depth, notnull array<SCR_ResourceContainer> outContainers)
	{
		if (!entity || depth > 10)
			return;

		AppendContainers(entity, outContainers);

		IEntity child = entity.GetChildren();
		while (child)
		{
			CollectFromTree(child, depth + 1, outContainers);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectNearby(IEntity root, notnull array<SCR_ResourceContainer> outContainers)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		s_aQueryHits = new array<SCR_ResourceContainer>();
		vector center = ResolveStorageCenter(root);

		world.QueryEntitiesBySphere(
			center,
			NEARBY_STORAGE_RADIUS,
			AppendNearbyEntity,
			null,
			EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);

		foreach (SCR_ResourceContainer container : s_aQueryHits)
		{
			if (container && outContainers.Find(container) < 0)
				outContainers.Insert(container);
		}

		s_aQueryHits = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static vector ResolveStorageCenter(IEntity root)
	{
		IEntity storage = FindNamedChild(root, "Storage_Supplies", 0);
		if (storage)
			return storage.GetOrigin();

		return root.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindNamedChild(IEntity entity, string token, int depth)
	{
		if (!entity || depth > 8 || token.IsEmpty())
			return null;

		string name = entity.GetName();
		if (name && name.Contains(token))
			return entity;

		ResourceName prefabName = SCR_ResourceNameUtils.GetPrefabName(entity);
		if (!prefabName.IsEmpty())
		{
			string path = prefabName;
			if (path.Contains(token))
				return entity;
		}

		IEntity child = entity.GetChildren();
		while (child)
		{
			IEntity nested = FindNamedChild(child, token, depth + 1);
			if (nested)
				return nested;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool AppendNearbyEntity(IEntity entity)
	{
		if (!entity || !s_aQueryHits)
			return true;

		AppendContainers(entity, s_aQueryHits);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AppendContainers(IEntity entity, notnull array<SCR_ResourceContainer> outContainers)
	{
		SCR_ResourceComponent resource = SCR_ResourceComponent.FindResourceComponent(entity);
		if (!resource)
			return;

		SCR_ResourceContainer container = resource.GetContainer(EResourceType.SUPPLIES);
		if (!container)
			return;

		if (outContainers.Find(container) >= 0)
			return;

		outContainers.Insert(container);
	}
}
