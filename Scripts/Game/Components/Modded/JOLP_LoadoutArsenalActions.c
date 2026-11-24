//! The ammo storage spawns its boxes through editor links, the same way a depot
//! spawns its service. Those boxes are the entities the player looks at.
//! Swap them for the same box with Save loadout and Loadouts on that action list.
modded class SCR_EditorLinkComponent
{
	protected static const ResourceName JOLP_LINKED_WEAPONS = "{B455DA2086A1C821}PrefabsEditable/Auto/Props/Military/Compositions/US/E_ArsenalBox_US_Weapons.et";
	protected static const ResourceName JOLP_LINKED_EQUIPMENT = "{0FB92802AFA233FA}PrefabsEditable/Auto/Props/Military/Compositions/US/E_ArsenalBox_US_Equipment.et";
	protected static const ResourceName JOLP_LOADOUT_WEAPONS = "{664B13B0A10008EA}PrefabsEditable/Auto/Props/Military/Compositions/US/E_JOLP_ArsenalBox_Weapons.et";
	protected static const ResourceName JOLP_LOADOUT_EQUIPMENT = "{664B13B0A10008EB}PrefabsEditable/Auto/Props/Military/Compositions/US/E_JOLP_ArsenalBox_Equipment.et";

	//------------------------------------------------------------------------------------------------
	override void SpawnComposition()
	{
		super.SpawnComposition();
		JOLP_ReplaceArsenalBoxes();
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_ReplaceArsenalBoxes()
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
			if (prefab == JOLP_LINKED_WEAPONS)
				replacement = JOLP_LOADOUT_WEAPONS;
			else if (prefab == JOLP_LINKED_EQUIPMENT)
				replacement = JOLP_LOADOUT_EQUIPMENT;
			else
				continue;

			Resource resource = Resource.Load(replacement);
			if (!resource || !resource.IsValid())
			{
				Print("[JOLP] Loadout arsenal box failed to load", LogLevel.ERROR);
				continue;
			}

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
		}
	}
}
