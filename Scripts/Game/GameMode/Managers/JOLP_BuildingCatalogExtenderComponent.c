class JOLP_BuildingCatalogExtenderComponentClass : SCR_BaseGameModeComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Deprecated. GameMode_JOLP now points m_sPrefabsToBuildResource at the JOLP-only catalog.
//! GetPlaceablePrefabs() returns a copy — Insert here never reached the manager registry.
//! Kept as a no-op so a stale world/prefab instance cannot re-extend a vanilla catalog.
class JOLP_BuildingCatalogExtenderComponent : SCR_BaseGameModeComponent
{
	[Attribute(desc: "Ignored — catalog is owned by SCR_CampaignBuildingManagerComponent")]
	protected ref array<ResourceName> m_aExtraPrefabs;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		// Intentionally empty.
	}
}
