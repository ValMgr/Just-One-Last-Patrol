//! Dig-layout resolution for FreeRoam building.
//! Do NOT load CampaignBuildingCompositionOutline.conf via BaseContainerTools while this
//! class is modded — config deserialize then reports "Unknown class" and outlineMgr stays null.
//! Instantiate in script and seed slot-size defaults (vanilla SupplyCache etc. use SLOT_* labels).
modded class SCR_CampaignBuildingCompositionOutlineManager
{
	//------------------------------------------------------------------------------------------------
	//! Fill slot-size fallbacks used when a composition has no per-prefab outline entry.
	void JOLP_InitDefaultLayouts()
	{
		m_sSlotFlatSmallLayout = "{7FD4E6A668B4537A}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotFLatSmall.et";
		m_sSlotFlatMediumLayout = "{579FF731013436B5}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotFlatMedium.et";
		m_sSlotFlatLargeLayout = "{D06D9D47008E794F}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotRoadLarge.et";
		m_sSlotRoadSmallLayout = "{8DBA7E7BF662A393}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotRoadSmall.et";
		m_sSlotRoadMediumLayout = "{3A5E87ADA26C6940}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotRoadMedium.et";
		m_sSlotRoadLargeLayout = "{D06D9D47008E794F}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotRoadLarge.et";

		if (!m_aCompositionLayouts)
			m_aCompositionLayouts = new array<ref SCR_CampaignBuildingCompositionOutline>();

		// Specific layouts that differ from the generic slot fallback.
		JOLP_AddLayout("{1BED8F81D2FA2137}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_AmmoStorage_S_US_01.et",
			"{8E350990D5F5ABB5}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_AmmoStorage_S_01.et", 50);
		JOLP_AddLayout("{94D9E9B690F98756}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SupplyCache_S_US_01.et",
			"{7FD4E6A668B4537A}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotFLatSmall.et", 50);
		JOLP_AddLayout("{E87F2E0B5C56A1C3}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Bunker_S_US_01.et",
			"{E9CCAD99622A35F5}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_Bunker_US_01.et", 50);
		JOLP_AddLayout("{31DBF7E7C9D70576}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SandbagPosition_S_US_03.et",
			"{C57C7CC829D7DAA2}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SandbagPosition_S_03.et", 50);
		JOLP_AddLayout("{02C9F8198A7B9897}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_MachineGunNest_S_US_01.et",
			"{33A2B437003B14DB}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_MachineGunNest_01.et", 50);
		JOLP_AddLayout("{A0FEA3E718F5B605}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_VehicleMaintenance_S_Conflict_US_01.et",
			"{B2EAAF9C153CAFDE}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_VehicleMaintenance_S_01.et", 50);
		JOLP_AddLayout("{7AD06D9CF2672AC5}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_VehicleMaintenance_M_Conflict_US_01.et",
			"{F28A8D6AC99AD4E8}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_VehicleMaintenance_M_01.et", 100);
		JOLP_AddLayout("{16AF2FE47A6C297B}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Headquarters_S_Conflict_US_01.et",
			"{7FD4E6A668B4537A}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotFLatSmall.et", 100);
		JOLP_AddLayout("{664B13B0A1000280}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_JOLP_Helipad_USMC.et",
			"{D06D9D47008E794F}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotRoadLarge.et", 100);
		JOLP_AddLayout("{664B13B0A1000290}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_FuelStorage_USMC.et",
			"{7FD4E6A668B4537A}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotFLatSmall.et", 50);

		// FreeRoam props (same GUIDs as vanilla outline conf).
		JOLP_AddLayout("{7419CCE202790CBB}PrefabsEditable/Auto/Compositions/Misc/FreeRoamBuilding/E_SandbagRoundPlastic_S_US_01.et",
			"{A407FE596AB356EE}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_Sandbag_01_Round.et", 20);
		JOLP_AddLayout("{C90548D80CB6F09A}PrefabsEditable/Auto/Compositions/Misc/FreeRoamBuilding/E_SandbagRoundHighPlastic_S_US_01.et",
			"{A407FE596AB356EE}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_Sandbag_01_Round.et", 20);
		JOLP_AddLayout("{2C8BC8352CE10EE2}PrefabsEditable/Auto/Compositions/Misc/FreeRoamBuilding/E_SandbagLongPlastic_S_US_01.et",
			"{1EC6725681D75742}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_Sandbag_Long.et", 30);
		JOLP_AddLayout("{1CFE263700517700}PrefabsEditable/Auto/Compositions/Misc/FreeRoamBuilding/E_SandbagLongHighPlastic_S_US_01.et",
			"{1EC6725681D75742}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_Sandbag_Long.et", 30);
		JOLP_AddLayout("{FD726FB018BC4169}PrefabsEditable/Auto/Compositions/Misc/FreeRoamBuilding/E_SandbagWallSolidPlastic_S_US_01.et",
			"{F470194B53F2A4F2}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_Sandbag_Solid.et", 30);
		JOLP_AddLayout("{CC778DF86D8A69F5}PrefabsEditable/Auto/Compositions/Misc/FreeRoamBuilding/E_SandbagWallPlastic_S_US_01.et",
			"{F470194B53F2A4F2}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_Sandbag_Solid.et", 30);
		JOLP_AddLayout("{18BEFC9A7FA72CD8}PrefabsEditable/Auto/Compositions/Misc/FreeRoamBuilding/E_BarbedTapeKnifeRest_S_US_01.et",
			"{9721D32C8E5CDE9C}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_BarbedTapeKnifeRest.et", 20);
		JOLP_AddLayout("{F3208517509E0B9B}PrefabsEditable/Auto/Compositions/Misc/FreeRoamBuilding/E_Dragonsteeth_S_US_01.et",
			"{8E0A17590AA58A72}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_CzechHedgehog.et", 20);
		JOLP_AddLayout("{C1641825486E3697}PrefabsEditable/Auto/Compositions/Misc/FreeRoamBuilding/E_CamoNetSmall_S_US_01.et",
			"{347608996C687A02}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_CamoNet_Small_US.et", 30);
		JOLP_AddLayout("{EA8CDB3A64FC1181}PrefabsEditable/Auto/Compositions/Misc/FreeRoamBuilding/E_CamoNetMedium_S_US_01.et",
			"{A90C5CF19DF184B9}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_CamoNet_Medium_US.et", 40);
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_AddLayout(ResourceName editable, ResourceName layout, int buildingValue)
	{
		SCR_CampaignBuildingCompositionOutline entry = new SCR_CampaignBuildingCompositionOutline();
		entry.JOLP_Set(editable, layout, buildingValue);
		m_aCompositionLayouts.Insert(entry);
	}

	//------------------------------------------------------------------------------------------------
	override ResourceName GetCompositionOutline(notnull SCR_EditableEntityComponent entity)
	{
		ResourceName jolpLayout = GetJOLPLayout(entity.GetPrefab());
		if (jolpLayout)
			return jolpLayout;

		return super.GetCompositionOutline(entity);
	}

	//------------------------------------------------------------------------------------------------
	override int GetCompositionBuildingValue(ResourceName originalComposition)
	{
		int jolpValue = GetJOLPBuildingValue(originalComposition);
		if (jolpValue > 0)
			return jolpValue;

		return super.GetCompositionBuildingValue(originalComposition);
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName GetJOLPLayout(ResourceName editablePrefab)
	{
		if (!editablePrefab)
			return string.Empty;

		if (editablePrefab == "{664B13B0A1000200}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_Headquarters_USMC.et")
			return "{7FD4E6A668B4537A}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotFLatSmall.et";
		if (editablePrefab == "{664B13B0A1000210}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_SupplyProduction_USMC.et")
			return "{7FD4E6A668B4537A}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotFLatSmall.et";
		if (editablePrefab == "{664B13B0A1000220}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_AmmoStorage_USMC.et")
			return "{8E350990D5F5ABB5}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_AmmoStorage_S_01.et";
		if (editablePrefab == "{664B13B0A1000230}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_VehicleMaintenance_USMC.et")
			return "{B2EAAF9C153CAFDE}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_VehicleMaintenance_S_01.et";
		if (editablePrefab == "{664B13B0A1000240}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_JOLP_VehicleMaintenance_Large_USMC.et")
			return "{F28A8D6AC99AD4E8}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_VehicleMaintenance_M_01.et";
		if (editablePrefab == "{664B13B0A1000250}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_Bunker_USMC.et")
			return "{E9CCAD99622A35F5}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_Bunker_US_01.et";
		if (editablePrefab == "{664B13B0A1000260}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_SandbagPosition_USMC.et")
			return "{4CB404D199F6C2E7}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SandbagPosition_S_01.et";
		if (editablePrefab == "{664B13B0A1000270}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_MachineGunNest_USMC.et")
			return "{33A2B437003B14DB}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_MachineGunNest_01.et";
		if (editablePrefab == "{664B13B0A1000280}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_JOLP_Helipad_USMC.et")
			return "{D06D9D47008E794F}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotRoadLarge.et";
		if (editablePrefab == "{664B13B0A1000290}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_FuelStorage_USMC.et")
			return "{7FD4E6A668B4537A}Prefabs/Compositions/Misc/FreeRoamBuilding/Layouts/FRB_SlotFLatSmall.et";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetJOLPBuildingValue(ResourceName editablePrefab)
	{
		if (!editablePrefab)
			return 0;

		if (editablePrefab == "{664B13B0A1000200}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_Headquarters_USMC.et")
			return 100;
		if (editablePrefab == "{664B13B0A1000210}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_SupplyProduction_USMC.et")
			return 50;
		if (editablePrefab == "{664B13B0A1000220}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_AmmoStorage_USMC.et")
			return 50;
		if (editablePrefab == "{664B13B0A1000230}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_VehicleMaintenance_USMC.et")
			return 50;
		if (editablePrefab == "{664B13B0A1000240}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_JOLP_VehicleMaintenance_Large_USMC.et")
			return 100;
		if (editablePrefab == "{664B13B0A1000250}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_Bunker_USMC.et")
			return 50;
		if (editablePrefab == "{664B13B0A1000260}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_SandbagPosition_USMC.et")
			return 30;
		if (editablePrefab == "{664B13B0A1000270}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_MachineGunNest_USMC.et")
			return 40;
		if (editablePrefab == "{664B13B0A1000280}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_JOLP_Helipad_USMC.et")
			return 100;
		if (editablePrefab == "{664B13B0A1000290}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_JOLP_FuelStorage_USMC.et")
			return 50;

		return 0;
	}
}

//------------------------------------------------------------------------------------------------
//! Allows script-side construction of outline entries (fields are otherwise only set from .conf).
modded class SCR_CampaignBuildingCompositionOutline
{
	//------------------------------------------------------------------------------------------------
	void JOLP_Set(ResourceName editable, ResourceName layout, int buildingValue)
	{
		m_sEditableEntity = editable;
		m_sCompositionLayout = layout;
		m_iBuildingValue = buildingValue;
	}
}
