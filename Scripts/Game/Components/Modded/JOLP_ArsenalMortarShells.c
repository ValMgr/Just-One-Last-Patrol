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
