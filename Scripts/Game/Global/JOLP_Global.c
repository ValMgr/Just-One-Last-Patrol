//------------------------------------------------------------------------------------------------
//! Static accessors for Just One Last Patrol managers and shared helpers.
class JOLP_Global : Managed
{
	//------------------------------------------------------------------------------------------------
	static JOLP_LocationManagerComponent GetLocations()
	{
		return JOLP_LocationManagerComponent.GetInstance();
	}

	//------------------------------------------------------------------------------------------------
	static JOLP_MilicianManagerComponent GetMilician()
	{
		return JOLP_MilicianManagerComponent.GetInstance();
	}

	//------------------------------------------------------------------------------------------------
	static JOLP_CivilianManagerComponent GetCivilians()
	{
		return JOLP_CivilianManagerComponent.GetInstance();
	}

	//------------------------------------------------------------------------------------------------
	static JOLP_IEDManagerComponent GetIEDs()
	{
		return JOLP_IEDManagerComponent.GetInstance();
	}

	//------------------------------------------------------------------------------------------------
	static JOLP_GameMasterComponent GetGameMaster()
	{
		return JOLP_GameMasterComponent.GetInstance();
	}

	//------------------------------------------------------------------------------------------------
	static JOLP_LoadoutManagerComponent GetLoadouts()
	{
		return JOLP_LoadoutManagerComponent.GetInstance();
	}

	//------------------------------------------------------------------------------------------------
	static JOLP_PersistenceManagerComponent GetPersistence()
	{
		JOLP_JustOneLastPatrolGameMode mode = GetGameMode();
		if (!mode)
			return JOLP_PersistenceManagerComponent.GetInstance();

		JOLP_PersistenceManagerComponent persistence = JOLP_PersistenceManagerComponent.Cast(mode.FindComponent(JOLP_PersistenceManagerComponent));
		if (persistence)
			return persistence;

		return JOLP_PersistenceManagerComponent.GetInstance();
	}

	//------------------------------------------------------------------------------------------------
	static JOLP_JustOneLastPatrolGameMode GetGameMode()
	{
		return JOLP_JustOneLastPatrolGameMode.Cast(GetGame().GetGameMode());
	}

	//------------------------------------------------------------------------------------------------
	//! True outside dedicated servers (offline / listen / Workbench).
	static bool IsDebug()
	{
		return JOLP_Debug.IsEnabled();
	}

	//------------------------------------------------------------------------------------------------
	static IEntity SpawnEntityPrefab(ResourceName prefab, vector origin, vector orientation = "0 0 0", bool global = true)
	{
		return JOLP_WorldUtils.SpawnEntityPrefab(prefab, origin, orientation, global);
	}

	//------------------------------------------------------------------------------------------------
	static bool PlayerInRange(vector pos, float range)
	{
		return JOLP_WorldUtils.PlayerInRange(pos, range);
	}

	//------------------------------------------------------------------------------------------------
	static bool AllPlayersBeyondRange(vector pos, float range)
	{
		return JOLP_WorldUtils.AllPlayersBeyondRange(pos, range);
	}
}
