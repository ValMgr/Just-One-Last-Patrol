//------------------------------------------------------------------------------------------------
//! Registers runtime entities with Reforger's vanilla persistence system.
//!
//! Tracking does not choose a serializer. Configs/Systems/Persistence/JustOneLastPatrol.conf does.
//! The system is server-only, so every call is a no-op on a client.
//! StartTracking's bool is not proof of registration. Registration is lazy. IsTracked is the check.
//------------------------------------------------------------------------------------------------
class JOLP_PersistenceTracking
{
	//------------------------------------------------------------------------------------------------
	static bool Track(IEntity entity, bool lazy = true)
	{
		if (!entity)
			return false;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetByEntityWorld(entity);
		if (!persistence)
			return false;

		return persistence.StartTracking(entity, lazy);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsTracked(IEntity entity)
	{
		if (!entity)
			return false;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetByEntityWorld(entity);
		if (!persistence)
			return false;

		return persistence.IsTracked(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! keepData false discards the stored record. Transient AI uses that so orphans do not accumulate.
	static bool Untrack(IEntity entity, bool keepData)
	{
		if (!entity)
			return false;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetByEntityWorld(entity);
		if (!persistence)
			return false;

		bool removeData = true;
		if (keepData)
			removeData = false;

		return persistence.StopTracking(entity, removeData);
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle that just left a player service (depot / FOB composition) must stay on the
	//! inherited vehicle config. Do not give cars their own JOLP rule.
	static void TrackDepotVehicle(IEntity vehicle, IEntity spawnerOwner)
	{
		if (!vehicle || !Vehicle.Cast(vehicle))
			return;

		if (!IsPlayerService(spawnerOwner))
			return;

		Track(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsPlayerService(IEntity entity)
	{
		IEntity cursor = entity;
		while (cursor)
		{
			if (JOLP_PersistentCompositionComponent.Cast(cursor.FindComponent(JOLP_PersistentCompositionComponent)))
				return true;

			if (JOLP_FOBControllerComponent.Cast(cursor.FindComponent(JOLP_FOBControllerComponent)))
				return true;

			if (SCR_ServicePointComponent.Cast(cursor.FindComponent(SCR_ServicePointComponent)))
				return true;

			cursor = cursor.GetParent();
		}

		return false;
	}
}
