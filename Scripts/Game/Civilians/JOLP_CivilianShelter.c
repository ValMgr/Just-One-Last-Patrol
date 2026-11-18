//------------------------------------------------------------------------------------------------
//! Nearest building door, a stand point just inside it, and the door left open.
class JOLP_CivilianShelter
{
	protected static vector s_vFrom;
	protected static float s_fRadius;
	protected static float s_fBest;
	protected static DoorComponent s_Door;
	protected static IEntity s_DoorEntity;

	//------------------------------------------------------------------------------------------------
	//! False when no door is inside the radius. Opens the door and returns a point past the threshold.
	static bool FindStand(vector from, float searchRadius, out vector stand, int slot = 0)
	{
		stand = from;
		s_vFrom = from;
		s_fRadius = searchRadius;
		s_fBest = -1;
		s_Door = null;
		s_DoorEntity = null;

		BaseWorld world = GetGame().GetWorld();
		if (!world || searchRadius <= 0)
			return false;

		world.QueryEntitiesBySphere(from, searchRadius, VisitDoor, null, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);
		if (!s_Door)
			return false;

		s_Door.SetControlValue(1, RplId.Invalid());
		stand = StandPoint(s_Door, from, slot);
		s_Door = null;
		s_DoorEntity = null;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool VisitDoor(IEntity entity)
	{
		if (!entity)
			return true;

		DoorComponent door = DoorComponent.Cast(entity.FindComponent(DoorComponent));
		if (!door)
			return true;

		float dist = JOLP_WorldUtils.DistanceFlat(door.GetDoorPivotPointWS(), s_vFrom);
		if (dist > s_fRadius)
			return true;

		if (s_fBest >= 0 && dist >= s_fBest)
			return true;

		s_fBest = dist;
		s_Door = door;
		s_DoorEntity = entity;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The civilian is outside. The stand point is the far side of the door, a step past the threshold.
	protected static vector StandPoint(notnull DoorComponent door, vector from, int slot)
	{
		vector pivot = door.GetDoorPivotPointWS();
		vector normal = door.GetDoorNormal();
		normal[1] = 0;
		if (normal.Length() < 0.05)
			normal = Vector(0, 0, 1);

		normal = normal * (1 / normal.Length());

		vector sideA = pivot + (normal * 3.2);
		vector sideB = pivot - (normal * 3.2);
		vector insideDir = normal;
		if (JOLP_WorldUtils.DistanceFlat(sideB, from) > JOLP_WorldUtils.DistanceFlat(sideA, from))
			insideDir = normal * -1;

		vector side = Vector(-insideDir[2], 0, insideDir[0]);
		int lane = slot - ((slot / 3) * 3);
		float lateral = (lane - 1) * 0.75;

		vector deep = pivot + (insideDir * 3.2) + (side * lateral);
		deep[1] = pivot[1];
		vector snapped;
		if (SnapInside(deep, insideDir, pivot, snapped))
			return snapped;

		vector threshold = pivot + (insideDir * 1.4) + (side * lateral);
		threshold[1] = pivot[1];
		if (SnapInside(threshold, insideDir, pivot, snapped))
			return snapped;

		return deep;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SnapInside(vector sample, vector insideDir, vector pivot, out vector snapped)
	{
		if (!SnapNavmesh(sample, snapped))
			return false;

		vector fromPivot = snapped - pivot;
		fromPivot[1] = 0;
		return vector.Dot(fromPivot, insideDir) >= 0.6;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SnapNavmesh(vector from, out vector snapped)
	{
		snapped = from;

		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
			return false;

		NavmeshWorldComponent navmesh = aiWorld.GetNavmeshWorldComponent("Soldiers");
		if (!navmesh)
			return false;

		vector reachable;
		if (!navmesh.GetReachablePoint(from, 4, reachable))
			return false;

		if (JOLP_WorldUtils.DistanceFlat(from, reachable) > 8)
			return false;

		snapped = reachable;
		return true;
	}
}
