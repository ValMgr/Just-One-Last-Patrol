//------------------------------------------------------------------------------------------------
//! Vehicles spawned by a prefab spawner under a FOB, a service point, or a persisted
//! composition are player logistics. They stay on the inherited vehicle config.
modded class SCR_BasePrefabSpawner
{
	//------------------------------------------------------------------------------------------------
	protected override void OnSpawn(IEntity newEnt)
	{
		super.OnSpawn(newEnt);
		// SCR_BasePrefabSpawner is a GenericEntity, not a component — the spawner is `this`.
		JOLP_PersistenceTracking.TrackDepotVehicle(newEnt, this);
	}
}
