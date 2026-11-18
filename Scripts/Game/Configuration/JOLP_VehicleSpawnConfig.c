//------------------------------------------------------------------------------------------------
//! One vehicle a depot can request. Price is supplies, taken from the depot stock.
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class JOLP_VehicleSpawnEntry
{
	[Attribute(desc: "Name in this list. The game shows the vehicle's own name.")]
	string m_sName;

	[Attribute(desc: "Vehicle prefab to spawn", uiwidget: UIWidgets.ResourcePickerThumbnail, params: "et")]
	ResourceName m_rPrefab;

	[Attribute(defvalue: "0", desc: "Supplies charged when this vehicle is requested", params: "0 inf")]
	int m_iSupplyCost;

	[Attribute(defvalue: "8", uiwidget: UIWidgets.Flags, enums: ParamEnumArray.FromEnum(SCR_EEntitySpawnerSlotType), desc: "Pads this vehicle fits. Small maintenance is VEHICLE_SMALL, trucks VEHICLE_MEDIUM, helicopters VEHICLE_LARGE.")]
	SCR_EEntitySpawnerSlotType m_eSlotTypes;
}

//------------------------------------------------------------------------------------------------
//! Vehicles a player can request at a depot, and what each one costs in supplies.
[BaseContainerProps(configRoot: true)]
class JOLP_VehicleSpawnConfig
{
	[Attribute(desc: "Vehicles offered at a depot, in this order. A light depot lists VEHICLE_SMALL, a heavy depot lists VEHICLE_MEDIUM, a helipad lists VEHICLE_LARGE.")]
	ref array<ref JOLP_VehicleSpawnEntry> m_aVehicles;
}
