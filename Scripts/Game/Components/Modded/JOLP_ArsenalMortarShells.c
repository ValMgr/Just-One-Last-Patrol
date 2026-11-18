//! Lets the milician mortar box hand out shells whatever its faction catalog lists.
modded class SCR_ArsenalInventoryStorageManagerComponent
{
	protected ref set<ResourceName> m_JOLP_ForcedPrefabs;

	//------------------------------------------------------------------------------------------------
	void JOLP_ForcePrefab(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return;

		if (!m_JOLP_ForcedPrefabs)
			m_JOLP_ForcedPrefabs = new set<ResourceName>();

		m_JOLP_ForcedPrefabs.Insert(prefab);
	}

	//------------------------------------------------------------------------------------------------
	override bool IsPrefabInArsenalStorage(ResourceName item)
	{
		if (m_JOLP_ForcedPrefabs && m_JOLP_ForcedPrefabs.Contains(item))
			return true;

		return super.IsPrefabInArsenalStorage(item);
	}
}

//! The AI loader pays the box's supply cost per shell. This mode never stocks the box.
modded class SCR_ArsenalComponent
{
	protected bool m_bJOLP_Free;

	//------------------------------------------------------------------------------------------------
	void JOLP_SetFree(bool free)
	{
		m_bJOLP_Free = free;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsArsenalUsingSupplies()
	{
		if (m_bJOLP_Free)
			return false;

		return super.IsArsenalUsingSupplies();
	}
}

//! Dragging an item asks for the factionless supply-container catalog. This game mode does not have one, and the inventory menu does not check for that.
modded class SCR_EntityCatalogManagerComponent
{
	override SCR_EntityCatalog GetEntityCatalogOfType(EEntityCatalogType catalogType, bool printNotFound = true)
	{
		SCR_EntityCatalog catalog = super.GetEntityCatalogOfType(catalogType, false);
		if (catalog)
			return catalog;

		if (catalogType != EEntityCatalogType.SUPPLY_CONTAINER_ITEM)
		{
			if (printNotFound)
				Print(string.Format("'SCR_EntityCatalogManagerComponent' trying to get entity list of type '%1' but there is no catalog with that type.", typename.EnumToString(EEntityCatalogType, catalogType)), LogLevel.WARNING);

			return null;
		}

		FactionManager manager = GetGame().GetFactionManager();
		if (manager)
		{
			array<Faction> factions = {};
			manager.GetFactionsList(factions);
			foreach (Faction faction : factions)
			{
				SCR_Faction scrFaction = SCR_Faction.Cast(faction);
				if (!scrFaction)
					continue;

				catalog = scrFaction.GetFactionEntityCatalogOfType(catalogType, false);
				if (catalog)
					return catalog;
			}
		}

		if (printNotFound)
			Print(string.Format("'SCR_EntityCatalogManagerComponent' trying to get entity list of type '%1' but there is no catalog with that type.", typename.EnumToString(EEntityCatalogType, catalogType)), LogLevel.WARNING);

		return null;
	}
}
