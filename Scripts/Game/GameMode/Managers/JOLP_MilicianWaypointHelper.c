//------------------------------------------------------------------------------------------------
//! Spawns and attaches AI waypoints for milician packages; owns deletion lists on the live record.
class JOLP_MilicianWaypointHelper : Managed
{
	//------------------------------------------------------------------------------------------------
	static AIWaypoint SpawnWaypoint(ResourceName prefab, vector position)
	{
		IEntity entity = JOLP_WorldUtils.SpawnEntityPrefab(prefab, position, "0 0 0", true);
		return AIWaypoint.Cast(entity);
	}

	//------------------------------------------------------------------------------------------------
	static void TrackWaypoint(JOLP_LiveGroupRecord record, AIWaypoint waypoint)
	{
		if (!record || !waypoint)
			return;

		if (!record.m_aWaypointIds)
			record.m_aWaypointIds = new array<EntityID>();

		record.m_aWaypointIds.Insert(waypoint.GetID());
	}

	//------------------------------------------------------------------------------------------------
	static void AssignDefend(notnull SCR_AIGroup group, notnull JOLP_LiveGroupRecord record, ResourceName defendPrefab, vector position)
	{
		AIWaypoint waypoint = SpawnWaypoint(defendPrefab, position);
		if (!waypoint)
			return;

		group.AddWaypoint(waypoint);
		TrackWaypoint(record, waypoint);
	}

	//------------------------------------------------------------------------------------------------
	static void AssignCyclePatrol(notnull SCR_AIGroup group, notnull JOLP_LiveGroupRecord record, ResourceName patrolPrefab, ResourceName cyclePrefab, vector center, float radius, int pointCount = 4, float angleOffset = 0, float radiusJitter = 0)
	{
		if (pointCount < 2)
			pointCount = 4;

		array<AIWaypoint> built = new array<AIWaypoint>();
		for (int i = 0; i < pointCount; i++)
		{
			float pointRadius = radius;
			if (radiusJitter > 0)
				pointRadius = radius * Math.RandomFloat(Math.Max(0.25, 1 - radiusJitter), 1);

			float angle = angleOffset + (i / (float)pointCount) * Math.PI2;
			vector pos = center + Vector(Math.Cos(angle) * pointRadius, 0, Math.Sin(angle) * pointRadius);
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
	//! Patrol along roads inside the settlement. A circle is used when the road network has too few points.
	static void AssignRoadPatrol(notnull SCR_AIGroup group, notnull JOLP_LiveGroupRecord record, ResourceName patrolPrefab, ResourceName cyclePrefab, vector center, float radius)
	{
		array<vector> roads = {};
		JOLP_WorldUtils.CollectSpreadRoadPositions(center, radius, 4, roads);
		if (roads.Count() < 2)
		{
			AssignCyclePatrol(group, record, patrolPrefab, cyclePrefab, center, Math.Max(40, radius * 0.45), 4);
			return;
		}

		array<AIWaypoint> built = new array<AIWaypoint>();
		foreach (vector pos : roads)
		{
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
	static void AssignMoveTo(notnull SCR_AIGroup group, notnull JOLP_LiveGroupRecord record, ResourceName movePrefab, vector destination)
	{
		AIWaypoint waypoint = SpawnWaypoint(movePrefab, destination);
		if (!waypoint)
			return;

		group.AddWaypoint(waypoint);
		TrackWaypoint(record, waypoint);
	}

	//------------------------------------------------------------------------------------------------
	//! Completion radius is the search box. Holding time is compared to world time, which is milliseconds,
	//! and the prefab's 600 expires the sweep immediately.
	static bool AssignSearchAndDestroy(notnull SCR_AIGroup group, notnull JOLP_LiveGroupRecord record, ResourceName prefab, vector position, float radius, float holdingTimeMs)
	{
		AIWaypoint waypoint = SpawnWaypoint(prefab, position);
		if (!waypoint)
			return false;

		if (radius > 0)
			waypoint.SetCompletionRadius(radius);

		SCR_SearchAndDestroyWaypoint search = SCR_SearchAndDestroyWaypoint.Cast(waypoint);
		if (search && holdingTimeMs > 0)
			search.SetHoldingTime(holdingTimeMs);

		group.AddWaypoint(waypoint);
		TrackWaypoint(record, waypoint);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ClearGroupWaypoints(SCR_AIGroup group, JOLP_LiveGroupRecord record)
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

	//------------------------------------------------------------------------------------------------
	static void DeleteTrackedWaypoints(JOLP_LiveGroupRecord record)
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
}
