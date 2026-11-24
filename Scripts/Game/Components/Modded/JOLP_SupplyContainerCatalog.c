//! Dragging an item asks for the factionless supply-container catalog and then uses it
//! without a null check. This game mode's manager does not have that catalog in its map.
//! Load the vanilla list so the drag can continue.
modded class SCR_EntityCatalogManagerComponent
{
	protected static const ResourceName JOLP_SUPPLY_CONTAINERS = "{B80293331FB7BFCF}Configs/EntityCatalog/FactionLess/SupplyContainerItems_EntityCatalog_Factionless.conf";

	protected bool m_bJOLP_SupplyCatalogTried;

	//------------------------------------------------------------------------------------------------
	override SCR_EntityCatalog GetEntityCatalogOfType(EEntityCatalogType catalogType, bool printNotFound = true)
	{
		SCR_EntityCatalog catalog = super.GetEntityCatalogOfType(catalogType, false);
		if (catalog)
			return catalog;

		if (catalogType == EEntityCatalogType.SUPPLY_CONTAINER_ITEM && !m_bJOLP_SupplyCatalogTried)
		{
			m_bJOLP_SupplyCatalogTried = true;
			JOLP_InstallSupplyContainerCatalog();
			catalog = super.GetEntityCatalogOfType(catalogType, false);
			if (catalog)
				return catalog;
		}

		if (printNotFound)
			Print(string.Format("'SCR_EntityCatalogManagerComponent' trying to get entity list of type '%1' but there is no catalog with that type.", typename.EnumToString(EEntityCatalogType, catalogType)), LogLevel.WARNING);

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void JOLP_InstallSupplyContainerCatalog()
	{
		if (!m_mEntityCatalogs)
			return;

		Resource container = BaseContainerTools.LoadContainer(JOLP_SUPPLY_CONTAINERS);
		if (!container || !container.IsValid())
		{
			Print("[JOLP] Supply container catalog failed to load", LogLevel.ERROR);
			return;
		}

		SCR_EntityCatalog catalog = SCR_EntityCatalog.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!catalog)
		{
			Print("[JOLP] Supply container catalog could not be read", LogLevel.ERROR);
			return;
		}

		catalog.InitCatalog();
		m_mEntityCatalogs.Insert(EEntityCatalogType.SUPPLY_CONTAINER_ITEM, catalog);
	}
}
