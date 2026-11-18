//------------------------------------------------------------------------------------------------
//! Dig outline: ensure OutlineManager before vanilla spawn, and resolve layouts even if
//! GetOutlineManager still fails (modded configRoot classes often break LoadContainer).
modded class SCR_CampaignBuildingCompositionComponent
{
	protected static const ResourceName JOLP_LAYOUT_SLOT_FLAT_SMALL = "{7FD4E6A668B4537A}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotFLatSmall.et";
	protected static const ResourceName JOLP_LAYOUT_SLOT_FLAT_MEDIUM = "{579FF731013436B5}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotFlatMedium.et";
	protected static const ResourceName JOLP_LAYOUT_AMMO = "{8E350990D5F5ABB5}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_AmmoStorage_S_01.et";

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SCR_EditorLinkComponent linkComponent = SCR_EditorLinkComponent.Cast(owner.FindComponent(SCR_EditorLinkComponent));
		if (!linkComponent)
		{
			Print(string.Format("[JOLP] Dig skip: no EditorLink on %1", owner), LogLevel.WARNING);
			super.OnPostInit(owner);
			return;
		}

		if (!linkComponent.IsSpawningIgnored())
		{
			Print(string.Format("[JOLP] Dig skip: IgnoreSpawning=false (composition spawned fully) on %1", owner), LogLevel.WARNING);
			super.OnPostInit(owner);
			return;
		}

		BaseGameMode gameMode = GetGame().GetGameMode();
		SCR_CampaignBuildingManagerComponent buildingManager;
		if (gameMode)
			buildingManager = SCR_CampaignBuildingManagerComponent.Cast(gameMode.FindComponent(SCR_CampaignBuildingManagerComponent));
		if (buildingManager)
			buildingManager.GetOutlineManager();

		super.OnPostInit(owner);

		if (IsProxy())
			return;

		if (GetCompositionLayout())
		{
			Print(string.Format("[JOLP] Dig outline spawned on %1", owner), LogLevel.NORMAL);
			return;
		}

		// Vanilla SpawnCompositionLayout bailed (manager missing / empty outline). Force dig spawn.
		SCR_EditableEntityComponent editableEntity = SCR_EditableEntityComponent.Cast(owner.FindComponent(SCR_EditableEntityComponent));
		ResourceName prefab;
		if (editableEntity)
			prefab = editableEntity.GetPrefab();

		int prefabId = -1;
		bool hasOutlineMgr;
		ResourceName outline;
		if (buildingManager)
		{
			hasOutlineMgr = buildingManager.GetOutlineManager() != null;
			if (editableEntity)
				prefabId = buildingManager.GetCompositionId(prefab);
		}

		if (editableEntity)
			outline = GetOutlineToSpawn(editableEntity);
		if (prefabId == -1 && buildingManager && prefab)
		{
			array<ResourceName> placeables = buildingManager.GetPlaceablePrefabs();
			if (placeables)
			{
				string path = prefab.GetPath();
				for (int i = 0; i < placeables.Count(); i++)
				{
					if (placeables[i] == prefab || (!path.IsEmpty() && placeables[i].GetPath() == path))
					{
						prefabId = i;
						break;
					}
				}
			}
		}

		if (editableEntity && outline)
		{
			Print(string.Format("[JOLP] Dig outline FORCE spawn outline='%1' on %2", outline, owner), LogLevel.WARNING);
			SpawnCompositionLayout(prefabId, editableEntity);
			if (GetCompositionLayout())
			{
				Print(string.Format("[JOLP] Dig outline spawned (forced) on %1", owner), LogLevel.NORMAL);
				return;
			}
		}

		Print(string.Format("[JOLP] Dig outline MISSING on %1 prefab=%2 catalogId=%3 outlineMgr=%4 outline='%5'",
			owner, prefab, prefabId, hasOutlineMgr, outline), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	override ResourceName GetOutlineToSpawn(notnull SCR_EditableEntityComponent entity)
	{
		ResourceName outline = super.GetOutlineToSpawn(entity);
		if (outline)
			return outline;

		return JOLP_ResolveOutlineFallback(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName JOLP_ResolveOutlineFallback(notnull SCR_EditableEntityComponent entity)
	{
		ResourceName prefab = entity.GetPrefab();
		if (prefab == "{1BED8F81D2FA2137}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_AmmoStorage_S_US_01.et"
			|| prefab == "{664B13B0A1000220}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_AmmoStorage_USMC.et")
			return JOLP_LAYOUT_AMMO;

		if (prefab == "{664B13B0A1000240}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_JOLP_VehicleMaintenance_Large_USMC.et")
			return JOLP_LAYOUT_SLOT_FLAT_MEDIUM;

		SCR_EditableEntityUIInfo uiInfo = SCR_EditableEntityUIInfo.Cast(entity.GetInfo());
		if (uiInfo)
		{
			if (uiInfo.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_MEDIUM))
				return JOLP_LAYOUT_SLOT_FLAT_MEDIUM;
			if (uiInfo.HasEntityLabel(EEditableEntityLabel.SLOT_FLAT_LARGE))
				return "{D06D9D47008E794F}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotRoadLarge.et";
			if (uiInfo.HasEntityLabel(EEditableEntityLabel.SLOT_ROAD_SMALL))
				return "{8DBA7E7BF662A393}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotRoadSmall.et";
			if (uiInfo.HasEntityLabel(EEditableEntityLabel.SLOT_ROAD_MEDIUM))
				return "{3A5E87ADA26C6940}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotRoadMedium.et";
			if (uiInfo.HasEntityLabel(EEditableEntityLabel.SLOT_ROAD_LARGE))
				return "{D06D9D47008E794F}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotRoadLarge.et";
		}

		// SupplyCache + most SlotFlatSmall placeables (and unknown slot size).
		return JOLP_LAYOUT_SLOT_FLAT_SMALL;
	}
}
