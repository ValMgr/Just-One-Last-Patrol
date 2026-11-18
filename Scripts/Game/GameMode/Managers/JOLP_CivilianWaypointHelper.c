//------------------------------------------------------------------------------------------------
//! Spawns and deletes civilian waypoints. The live record owns the ids.
class JOLP_CivilianWaypointHelper : Managed
{
	//------------------------------------------------------------------------------------------------
	static AIWaypoint SpawnWaypoint(ResourceName prefab, vector position)
	{
		IEntity entity = JOLP_WorldUtils.SpawnEntityPrefab(prefab, position, "0 0 0", true);
		return AIWaypoint.Cast(entity);
	}

	//------------------------------------------------------------------------------------------------
	static void TrackWaypoint(JOLP_CivilianLiveRecord record, AIWaypoint waypoint)
	{
		if (!record || !waypoint)
			return;

		if (!record.m_aWaypointIds)
			record.m_aWaypointIds = new array<EntityID>();

		record.m_aWaypointIds.Insert(waypoint.GetID());
	}

	//------------------------------------------------------------------------------------------------
	static void AssignCyclePatrol(notnull SCR_AIGroup group, notnull JOLP_CivilianLiveRecord record, ResourceName patrolPrefab, ResourceName cyclePrefab, vector center, float radius, int pointCount = 4, float startAngle = 0)
	{
		if (pointCount < 2)
			pointCount = 4;

		array<AIWaypoint> built = new array<AIWaypoint>();
		for (int i = 0; i < pointCount; i++)
		{
			float angle = startAngle + (i / (float)pointCount) * Math.PI2;
			vector pos = center + Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius);
			BaseWorld world = GetGame().GetWorld();
			if (world)
				pos[1] = world.GetSurfaceY(pos[0], pos[2]);

			AIWaypoint waypoint = SpawnWaypoint(patrolPrefab, pos);
			if (!waypoint)
				continue;

			group.AddWaypoint(waypoint);
			TrackWaypoint(record, waypoint);
			built.Insert(waypoint);
		}

		if (built.Count() < 2)
			return;

		AIWaypointCycle cycle = AIWaypointCycle.Cast(SpawnWaypoint(cyclePrefab, center));
		if (!cycle)
			return;

		cycle.SetWaypoints(built);
		cycle.SetRerunCounter(-1);
		group.AddWaypoint(cycle);
		TrackWaypoint(record, cycle);
	}

	//------------------------------------------------------------------------------------------------
	static void AssignMoveTo(notnull SCR_AIGroup group, notnull JOLP_CivilianLiveRecord record, ResourceName movePrefab, vector destination, float completionRadius = -1)
	{
		AIWaypoint waypoint = SpawnWaypoint(movePrefab, destination);
		if (!waypoint)
			return;

		if (completionRadius > 0)
			waypoint.SetCompletionRadius(completionRadius);

		group.AddWaypoint(waypoint);
		TrackWaypoint(record, waypoint);
	}

	//------------------------------------------------------------------------------------------------
	static void DeleteTrackedWaypoints(JOLP_CivilianLiveRecord record)
	{
		if (!record || !record.m_aWaypointIds)
			return;

		foreach (EntityID id : record.m_aWaypointIds)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(id);
			JOLP_WorldUtils.DeleteEntitySafe(entity);
		}

		record.m_aWaypointIds.Clear();
	}

	//------------------------------------------------------------------------------------------------
	static void ClearGroupWaypoints(SCR_AIGroup group, JOLP_CivilianLiveRecord record)
	{
		if (group)
		{
			array<AIWaypoint> waypoints = {};
			group.GetWaypoints(waypoints);
			foreach (AIWaypoint waypoint : waypoints)
			{
				if (waypoint)
					group.RemoveWaypoint(waypoint);
			}
		}

		DeleteTrackedWaypoints(record);
	}
}
