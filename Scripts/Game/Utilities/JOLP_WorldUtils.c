//------------------------------------------------------------------------------------------------
//! Stateless world helpers: spawn, road snap, player proximity.
class JOLP_WorldUtils : Managed
{
	//------------------------------------------------------------------------------------------------
	static IEntity SpawnEntityPrefab(ResourceName prefab, vector origin, vector orientation = "0 0 0", bool global = true)
	{
		if (prefab.IsEmpty())
			return null;

		Resource resource = Resource.Load(prefab);
		if (!resource)
		{
			Print(string.Format("[JOLP] SpawnEntityPrefab: failed to load '%1'", prefab), LogLevel.WARNING);
			return null;
		}

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(orientation, spawnParams.Transform);
		spawnParams.Transform[3] = origin;

		IEntity spawned;
		if (global)
			spawned = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
		else
			spawned = GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), spawnParams);

		// Manager-owned props and AI are rebuilt from campaign data. A persistence record for
		// them is an orphan nothing will spawn back.
		JOLP_PersistenceManagerComponent.UntrackTransient(spawned);
		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	//! The root and every child. A vanilla Persistence component on a flag or a sandbag would
	//! otherwise come back beside the copy the location manager spawns from the save.
	static void UntrackHierarchy(IEntity entity)
	{
		if (!entity)
			return;

		JOLP_PersistenceManagerComponent.UntrackTransient(entity);

		IEntity child = entity.GetChildren();
		while (child)
		{
			UntrackHierarchy(child);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Static weapon in a composition. The sandbag root is not the gun.
	static IEntity FindTurretEntity(IEntity root)
	{
		if (!root)
			return null;

		if (TurretControllerComponent.Cast(root.FindComponent(TurretControllerComponent)))
			return root;

		IEntity child = root.GetChildren();
		while (child)
		{
			IEntity found = FindTurretEntity(child);
			if (found)
				return found;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Heading for Math3D.AnglesToMatrix, which reads (yaw, pitch, roll).
	//! IEntity.GetAngles() is (pitch, yaw, roll). A heading in the pitch slot stands the vehicle on its nose.
	static vector UprightYaw(float yaw)
	{
		return Vector(yaw, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	static vector OffsetOnGround(vector origin, float scatterRadius)
	{
		float angle = Math.RandomFloat(0, Math.PI2);
		float dist = Math.RandomFloat(0, scatterRadius);
		vector pos = origin + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);

		BaseWorld world = GetGame().GetWorld();
		if (world)
			pos[1] = world.GetSurfaceY(pos[0], pos[2]);

		return pos;
	}

	//------------------------------------------------------------------------------------------------
	static bool PlayerInRange(vector pos, float range)
	{
		array<int> players = new array<int>();
		PlayerManager mgr = GetGame().GetPlayerManager();
		if (!mgr)
			return false;

		mgr.GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity player = mgr.GetPlayerControlledEntity(playerId);
			if (!player)
				continue;

			DamageManagerComponent dmg = DamageManagerComponent.Cast(player.FindComponent(DamageManagerComponent));
			if (dmg && dmg.IsDestroyed())
				continue;

			if (vector.Distance(player.GetOrigin(), pos) <= range)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True when every living player is at least `range` metres away. No living players counts as clear.
	static bool ClearOfLivingPlayers(vector pos, float range)
	{
		if (range <= 0)
			return true;

		float dist = NearestLivingPlayerDistance(pos);
		if (dist < 0)
			return true;

		return dist >= range;
	}

	//------------------------------------------------------------------------------------------------
	//! Distance to the nearest living player. Negative when none are alive.
	static float NearestLivingPlayerDistance(vector pos)
	{
		array<int> players = new array<int>();
		PlayerManager mgr = GetGame().GetPlayerManager();
		if (!mgr)
			return -1;

		mgr.GetPlayers(players);
		float best = -1;
		foreach (int playerId : players)
		{
			IEntity player = mgr.GetPlayerControlledEntity(playerId);
			if (!player)
				continue;

			DamageManagerComponent dmg = DamageManagerComponent.Cast(player.FindComponent(DamageManagerComponent));
			if (dmg && dmg.IsDestroyed())
				continue;

			float dist = vector.Distance(player.GetOrigin(), pos);
			if (best < 0 || dist < best)
				best = dist;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	static bool AllPlayersBeyondRange(vector pos, float range)
	{
		array<int> players = new array<int>();
		PlayerManager mgr = GetGame().GetPlayerManager();
		if (!mgr)
			return true;

		mgr.GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity player = mgr.GetPlayerControlledEntity(playerId);
			if (!player)
				continue;

			DamageManagerComponent dmg = DamageManagerComponent.Cast(player.FindComponent(DamageManagerComponent));
			if (dmg && dmg.IsDestroyed())
				continue;

			if (vector.Distance(player.GetOrigin(), pos) <= range)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Living players inside range. Used to keep a spawn on the far side of a town the player is already in.
	static int CollectLivingPlayerPositions(vector pos, float range, notnull array<vector> outPositions)
	{
		if (range <= 0)
			return 0;

		array<int> players = new array<int>();
		PlayerManager mgr = GetGame().GetPlayerManager();
		if (!mgr)
			return 0;

		mgr.GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity player = mgr.GetPlayerControlledEntity(playerId);
			if (!player)
				continue;

			DamageManagerComponent dmg = DamageManagerComponent.Cast(player.FindComponent(DamageManagerComponent));
			if (dmg && dmg.IsDestroyed())
				continue;

			if (vector.Distance(player.GetOrigin(), pos) > range)
				continue;

			outPositions.Insert(player.GetOrigin());
		}

		return outPositions.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Closest living player, anywhere. False when nobody is alive to fight.
	static bool NearestLivingPlayerPosition(vector pos, out vector position)
	{
		position = pos;

		array<int> players = new array<int>();
		PlayerManager mgr = GetGame().GetPlayerManager();
		if (!mgr)
			return false;

		mgr.GetPlayers(players);
		float best = -1;
		bool found = false;
		foreach (int playerId : players)
		{
			IEntity player = mgr.GetPlayerControlledEntity(playerId);
			if (!player)
				continue;

			DamageManagerComponent dmg = DamageManagerComponent.Cast(player.FindComponent(DamageManagerComponent));
			if (dmg && dmg.IsDestroyed())
				continue;

			float dist = vector.Distance(player.GetOrigin(), pos);
			if (best >= 0 && dist >= best)
				continue;

			best = dist;
			position = player.GetOrigin();
			found = true;
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Flat distance to the nearest listed point. Empty lists count as far away.
	static float NearestFlatDistance(vector pos, array<vector> others)
	{
		if (!others || others.IsEmpty())
			return 100000;

		float best = 100000;
		foreach (vector other : others)
		{
			float distance = DistanceFlat(pos, other);
			if (distance < best)
				best = distance;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Clearance from pos to the nearest occupied point. Empty lists count as wide open.
	static float Clearance(vector pos, array<vector> occupied)
	{
		if (!occupied || occupied.IsEmpty())
			return 100000;

		float best = 100000;
		foreach (vector other : occupied)
		{
			float distance = vector.Distance(pos, other);
			if (distance < best)
				best = distance;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! A road pose inside the area, preferring one that is not already occupied.
	//! Falls back to the nearest road point when every sampled stretch is taken.
	static bool FindSpreadRoadSpawn(vector center, float maxDistance, array<vector> occupied, float minSeparation, out vector position, out vector angles)
	{
		if (FindClearRoadSpawn(center, maxDistance, occupied, minSeparation, position, angles))
			return true;

		return FindNearestRoadSpawn(center, maxDistance, position, angles);
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest free point on a road inside the area.
	//! Samples the carriageway, so several vehicles queue along the road instead of sharing one vertex.
	//! Returns false when no road exists, or every sample is closer than minSeparation to an occupied point.
	static bool FindClearRoadSpawn(vector center, float maxDistance, array<vector> occupied, float minSeparation, out vector position, out vector angles)
	{
		position = center;
		angles = "0 0 0";

		array<vector> positions = {};
		array<vector> angleList = {};
		CollectRoadSamples(center, maxDistance, positions, angleList);

		int best = -1;
		float bestDistance = 999999;
		for (int p = 0; p < positions.Count(); p++)
		{
			if (Clearance(positions[p], occupied) < minSeparation)
				continue;

			float fromCenter = DistanceFlat(positions[p], center);
			if (fromCenter < bestDistance)
			{
				bestDistance = fromCenter;
				best = p;
			}
		}

		if (best < 0)
			return false;

		position = positions[best];
		angles = angleList[best];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A carriageway pose a wheeled vehicle can sit on without starting inside a building or under the road.
	//! Returns false when every sampled stretch is blocked, occupied, or missing. Callers must not spawn in that case.
	static bool FindVehicleRoadSpawn(vector center, float maxDistance, array<vector> occupied, float minSeparation, out vector position, out vector angles)
	{
		position = center;
		angles = "0 0 0";

		array<vector> positions = {};
		array<vector> angleList = {};
		CollectRoadSamples(center, maxDistance, positions, angleList);

		int best = -1;
		float bestDistance = 999999;
		vector bestPos;
		vector bestAngles;
		for (int p = 0; p < positions.Count(); p++)
		{
			if (Clearance(positions[p], occupied) < minSeparation)
				continue;

			float fromCenter = DistanceFlat(positions[p], center);
			if (fromCenter >= bestDistance)
				continue;

			vector fitted;
			vector fittedAngles;
			if (!TryFitVehicleSample(positions[p], angleList[p], fitted, fittedAngles))
				continue;

			bestDistance = fromCenter;
			best = p;
			bestPos = fitted;
			bestAngles = fittedAngles;
		}

		if (best < 0)
			return false;

		position = bestPos;
		angles = bestAngles;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A road pose as far from `avoid` as the carriageway allows. vehicleFit rejects samples a hull cannot sit on.
	//! Returns false when there is nobody to avoid, or no sample exists.
	static bool FindRoadSpawnFarthestFrom(vector center, float maxDistance, array<vector> occupied, float minSeparation, array<vector> avoid, bool vehicleFit, out vector position, out vector angles)
	{
		position = center;
		angles = "0 0 0";
		if (!avoid || avoid.IsEmpty())
			return false;

		array<vector> positions = {};
		array<vector> angleList = {};
		CollectRoadSamples(center, maxDistance, positions, angleList);

		int best = -1;
		float bestScore = -1;
		vector bestPos;
		vector bestAngles;
		for (int p = 0; p < positions.Count(); p++)
		{
			vector fitted = positions[p];
			vector fittedAngles = angleList[p];
			if (vehicleFit && !TryFitVehicleSample(positions[p], angleList[p], fitted, fittedAngles))
				continue;

			float fromThreat = NearestFlatDistance(fitted, avoid);
			float score = fromThreat;
			if (Clearance(fitted, occupied) < minSeparation)
				score = fromThreat * 0.25;

			if (score <= bestScore)
				continue;

			bestScore = score;
			best = p;
			bestPos = fitted;
			bestAngles = fittedAngles;
		}

		if (best < 0)
			return false;

		position = bestPos;
		angles = bestAngles;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle-fit pose in the outer band of the settlement, on the side facing `toward`.
	//! Reinforcements start here so a roadblock at the camp is behind them.
	//! False when that band has no clear stretch. Callers fall back to the nearest road.
	static bool FindRoadSpawnOnEdge(vector center, float radius, vector toward, array<vector> occupied, float minSeparation, out vector position, out vector angles)
	{
		position = center;
		angles = "0 0 0";

		if (radius < 40)
			radius = 40;

		array<vector> positions = {};
		array<vector> angleList = {};
		CollectRoadSamples(center, radius, positions, angleList);

		vector toDest = toward - center;
		toDest[1] = 0;
		bool aimed = toDest.Length() >= 1;
		if (aimed)
			toDest.Normalize();

		float inner = radius * 0.55;
		int best = -1;
		float bestScore = -1;
		vector bestPos;
		vector bestAngles;
		for (int p = 0; p < positions.Count(); p++)
		{
			if (Clearance(positions[p], occupied) < minSeparation)
				continue;

			vector fitted;
			vector fittedAngles;
			if (!TryFitVehicleSample(positions[p], angleList[p], fitted, fittedAngles))
				continue;

			float fromCenter = DistanceFlat(fitted, center);
			if (fromCenter < inner)
				continue;

			float score = fromCenter;
			if (aimed)
			{
				vector dir = fitted - center;
				dir[1] = 0;
				if (dir.Length() < 1)
					continue;

				dir.Normalize();
				score = (vector.Dot(dir, toDest) + 1) * 1000 + fromCenter;
			}

			if (score <= bestScore)
				continue;

			bestScore = score;
			best = p;
			bestPos = fitted;
			bestAngles = fittedAngles;
		}

		if (best < 0)
			return false;

		position = bestPos;
		angles = bestAngles;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A carriageway point near `center`, outside `minDistance` so it is not under the camp.
	//! Used for a roadblock. Does not require a vehicle-sized gap.
	static bool FindNearRoadSpawn(vector center, float minDistance, float maxDistance, out vector position, out vector angles)
	{
		position = center;
		angles = "0 0 0";

		if (maxDistance < minDistance)
			maxDistance = minDistance;

		array<vector> positions = {};
		array<vector> angleList = {};
		CollectRoadSamples(center, maxDistance, positions, angleList);

		int best = -1;
		float bestDistance = 999999;
		for (int p = 0; p < positions.Count(); p++)
		{
			float fromCenter = DistanceFlat(positions[p], center);
			if (fromCenter < minDistance || fromCenter > maxDistance)
				continue;

			if (fromCenter >= bestDistance)
				continue;

			bestDistance = fromCenter;
			best = p;
		}

		if (best < 0)
			return false;

		position = positions[best];
		angles = angleList[best];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Point the road heading toward the trip. The carriageway sample has no preferred direction.
	static void FaceYawToward(out vector angles, vector position, vector destination)
	{
		float yawRad = angles[0] * Math.DEG2RAD;
		vector forward = Vector(Math.Sin(yawRad), 0, Math.Cos(yawRad));
		vector toDest = destination - position;
		toDest[1] = 0;
		if (toDest.Length() < 1)
			return;

		if (vector.Dot(forward, toDest) < 0)
			angles[0] = angles[0] + 180;
	}

	//------------------------------------------------------------------------------------------------
	//! Somewhere the crew can stand on the same road, outside the hull.
	//! Spawning them in the vehicle's volume is what launches it onto the nearest roof.
	static vector CrewStandPosition(vector vehiclePosition, vector angles, float distance)
	{
		if (distance < 4)
			distance = 8;

		float yawRad = angles[0] * Math.DEG2RAD;
		vector forward = Vector(Math.Sin(yawRad), 0, Math.Cos(yawRad));

		vector behind = StandOnSurface(vehiclePosition - (forward * distance));
		if (OverheadClear(behind, 2.2))
			return behind;

		vector ahead = StandOnSurface(vehiclePosition + (forward * distance));
		if (OverheadClear(ahead, 2.2))
			return ahead;

		return behind;
	}

	//------------------------------------------------------------------------------------------------
	//! Drop any corrective impulse the solver applied because the body grazed the road mesh.
	static void SettleSpawnedVehicle(IEntity vehicle)
	{
		if (!vehicle)
			return;

		ClearVelocity(vehicle);

		IEntity child = vehicle.GetChildren();
		while (child)
		{
			ClearVelocity(child);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest road pose within maxDistance. Returns false when none found.
	static bool FindNearestRoadSpawn(vector center, float maxDistance, out vector position, out vector angles)
	{
		position = center;
		angles = "0 0 0";

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return false;

		RoadNetworkManager roadManager = aiWorld.GetRoadNetworkManager();
		if (!roadManager)
			return false;

		BaseRoad foundRoad;
		float distance;
		if (roadManager.GetClosestRoad(center, foundRoad, distance, false) < 0)
			return false;

		if (!foundRoad || distance > maxDistance)
			return false;

		array<vector> points = {};
		foundRoad.GetPoints(points);
		if (points.Count() < 2)
			return false;

		int closestIndex = 0;
		float closestDistance = 999999;
		for (int i = 0; i < points.Count(); i++)
		{
			float pointDistance = vector.Distance(points[i], center);
			if (pointDistance < closestDistance)
			{
				closestDistance = pointDistance;
				closestIndex = i;
			}
		}

		vector segStart = points[closestIndex];
		vector segEnd;
		if (closestIndex + 1 < points.Count())
		{
			segEnd = points[closestIndex + 1];
		}
		else
		{
			segStart = points[closestIndex - 1];
			segEnd = points[closestIndex];
		}

		vector direction = segEnd - segStart;
		direction[1] = 0;
		if (direction.Length() < 0.01)
			return false;

		direction.Normalize();

		vector flat = segEnd - segStart;
		flat[1] = 0;
		float segmentLength = flat.Length();

		vector fromStart = center - segStart;
		fromStart[1] = 0;
		float along = vector.Dot(fromStart, direction);
		if (along < 0)
			along = 0;
		if (along > segmentLength)
			along = segmentLength;

		position = segStart + (direction * along);
		float blend = 0;
		if (segmentLength > 0.01)
			blend = along / segmentLength;
		position[1] = segStart[1] + ((segEnd[1] - segStart[1]) * blend);
		KeepCarriagewayHeight(position);

		float yaw = Math.Atan2(direction[0], direction[2]) * Math.RAD2DEG;
		angles = UprightYaw(yaw);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static float DistanceFlat(vector a, vector b)
	{
		vector flatA = a;
		vector flatB = b;
		flatA[1] = 0;
		flatB[1] = 0;
		return vector.Distance(flatA, flatB);
	}

	//------------------------------------------------------------------------------------------------
	//! Spread carriageway points inside the radius. Used as a vehicle patrol loop.
	static int CollectSpreadRoadPositions(vector center, float radius, int wanted, notnull array<vector> outPositions)
	{
		if (wanted < 2)
			wanted = 4;

		array<vector> positions = {};
		array<vector> angles = {};
		CollectRoadSamples(center, radius, positions, angles);

		float minSeparation = Math.Clamp(radius * 0.18, 30, 120);
		foreach (vector pos : positions)
		{
			if (Clearance(pos, outPositions) < minSeparation)
				continue;

			outPositions.Insert(pos);
			if (outPositions.Count() >= wanted)
				break;
		}

		return outPositions.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Road samples around `center`. Height stays on the carriageway spline, not the landscape under it.
	protected static void CollectRoadSamples(vector center, float maxDistance, array<vector> positions, array<vector> angleList)
	{
		if (maxDistance < 30)
			maxDistance = 30;

		float step = 8;
		SampleClosestRoad(center, maxDistance, center, maxDistance, step, positions, angleList);

		int probes = 8;
		for (int i = 0; i < probes; i++)
		{
			float angle = (i / (float)probes) * Math.PI2;
			int parity = i - (i / 2) * 2;
			float dist = maxDistance * (0.45 + parity * 0.4);
			vector probe = center + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);
			float probeReach = Math.Max(60, maxDistance * 0.35);
			SampleClosestRoad(probe, probeReach, center, maxDistance, step, positions, angleList);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Points every `step` metres along the road nearest to `probe`, kept inside `maxDistance` of `center`.
	protected static void SampleClosestRoad(vector probe, float probeReach, vector center, float maxDistance, float step, array<vector> positions, array<vector> angles)
	{
		if (step < 1)
			step = 8;

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		RoadNetworkManager roadManager = aiWorld.GetRoadNetworkManager();
		if (!roadManager)
			return;

		BaseRoad foundRoad;
		float distance;
		if (roadManager.GetClosestRoad(probe, foundRoad, distance, false) < 0)
			return;

		if (!foundRoad || distance > probeReach)
			return;

		array<vector> points = {};
		foundRoad.GetPoints(points);
		if (points.Count() < 2)
			return;

		for (int i = 0; i < points.Count() - 1; i++)
		{
			vector segStart = points[i];
			vector segEnd = points[i + 1];
			vector direction = segEnd - segStart;
			direction[1] = 0;
			float length = direction.Length();
			if (length < 0.01)
				continue;

			if (DistanceFlat(segStart, center) > maxDistance + length && DistanceFlat(segEnd, center) > maxDistance)
				continue;

			direction.Normalize();
			float travelled = 0;
			while (travelled <= length)
			{
				vector pos = segStart + (direction * travelled);
				float blend = travelled / length;
				pos[1] = segStart[1] + ((segEnd[1] - segStart[1]) * blend);

				if (DistanceFlat(pos, center) <= maxDistance)
					AddRoadSample(pos, direction, positions, angles);

				if (travelled >= length)
					break;

				travelled += step;
				if (travelled > length)
					travelled = length;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddRoadSample(vector pos, vector direction, array<vector> positions, array<vector> angles)
	{
		foreach (vector other : positions)
		{
			if (DistanceFlat(other, pos) < 4)
				return;
		}

		KeepCarriagewayHeight(pos);

		float yaw = Math.Atan2(direction[0], direction[2]) * Math.RAD2DEG;
		positions.Insert(pos);
		angles.Insert(UprightYaw(yaw));
	}

	//------------------------------------------------------------------------------------------------
	//! Sit a roadside point on the mesh near the road spline. The window stays on bridges and does not
	//! fall through to the landscape under them. False when the trace misses or hits a wall.
	static bool SnapNearRoad(vector pos, out vector snapped)
	{
		snapped = pos;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		float roadY = pos[1];
		float hitY;
		vector normal;
		bool hit = TraceDown(world, pos, roadY + 1.25, roadY - 2.2, EPhysicsLayerDefs.Projectile, hitY, normal);
		if (!hit)
			hit = TraceDown(world, pos, roadY + 1.25, roadY - 2.2, EPhysicsLayerPresets.VehicleCast, hitY, normal);

		if (!hit)
			return false;

		if (normal[1] < 0.4)
			return false;

		snapped[1] = hitY;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Road graphs store the carriageway, including bridges. GetSurfaceY is the landscape, which sits
	//! under those bridges and inside building footprints. Only replace the spline when it has no height at all.
	protected static void KeepCarriagewayHeight(inout vector pos)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float terrainY = world.GetSurfaceY(pos[0], pos[2]);
		if (pos[1] < terrainY - 2)
			pos[1] = terrainY;
	}

	//------------------------------------------------------------------------------------------------
	//! Center of the lane, then a step to either side. Village streets often have one edge inside a porch.
	protected static bool TryFitVehicleSample(vector sample, vector angles, out vector position, out vector fittedAngles)
	{
		position = sample;
		fittedAngles = angles;

		float yawRad = angles[0] * Math.DEG2RAD;
		vector right = Vector(Math.Cos(yawRad), 0, -Math.Sin(yawRad));

		if (FitVehicle(sample, angles, position))
			return true;

		vector rightLane = sample + (right * 2.2);
		rightLane[1] = sample[1];
		if (FitVehicle(rightLane, angles, position))
			return true;

		vector leftLane = sample - (right * 2.2);
		leftLane[1] = sample[1];
		if (FitVehicle(leftLane, angles, position))
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Accept the point only when the wheels meet a floor near the spline and the body is not inside a solid.
	//! A vehicle that starts interpenetrating is shoved out through the nearest roof, or down through the terrain.
	protected static bool FitVehicle(vector sample, vector angles, out vector position)
	{
		position = sample;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		float roadY = sample[1];
		float hitY;
		vector normal;
		bool hit = TraceDown(world, sample, roadY + 6, roadY - 2.5, EPhysicsLayerPresets.VehicleCast, hitY, normal);
		if (!hit)
			hit = TraceDown(world, sample, roadY + 6, roadY - 2.5, EPhysicsLayerDefs.Projectile, hitY, normal);

		if (!hit)
		{
			float terrainY = world.GetSurfaceY(sample[0], sample[2]);
			if (roadY < terrainY - 0.5)
				return false;

			position[1] = roadY + 0.35;
		}
		else
		{
			// First contact well above the spline is a roof. Well below it is the ground under a bridge.
			if (hitY > roadY + 1.6)
				return false;

			if (hitY < roadY - 1.6)
				return false;

			if (normal[1] < 0.45)
				return false;

			position[1] = hitY + 0.35;
		}

		float oceanY = world.GetOceanHeight(position[0], position[2]);
		if (position[1] < oceanY + 0.4)
			return false;

		if (!VehicleBodyClear(world, position, angles))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True when a car-sized box above `position` does not touch a wall or a ceiling.
	protected static bool VehicleBodyClear(BaseWorld world, vector position, vector angles)
	{
		if (!OverheadClear(position, 2.6))
			return false;

		float yawRad = angles[0] * Math.DEG2RAD;
		vector forward = Vector(Math.Sin(yawRad), 0, Math.Cos(yawRad));
		vector right = Vector(Math.Cos(yawRad), 0, -Math.Sin(yawRad));
		vector back = Vector(-forward[0], 0, -forward[2]);
		vector left = Vector(-right[0], 0, -right[2]);
		float body = 0.9;

		if (!SideClear(world, position, forward, 2.6, body))
			return false;
		if (!SideClear(world, position, back, 2.6, body))
			return false;
		if (!SideClear(world, position, right, 1.35, body))
			return false;
		if (!SideClear(world, position, left, 1.35, body))
			return false;
		if (!SideClear(world, position, forward + right, 2.2, body))
			return false;
		if (!SideClear(world, position, forward + left, 2.2, body))
			return false;
		if (!SideClear(world, position, back + right, 2.2, body))
			return false;
		if (!SideClear(world, position, back + left, 2.2, body))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SideClear(BaseWorld world, vector position, vector direction, float distance, float bodyHeight)
	{
		if (direction.Length() < 0.01)
			return true;

		direction.Normalize();

		TraceParam trace = new TraceParam();
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		trace.Start = position + Vector(0, bodyHeight, 0);
		trace.End = trace.Start + (direction * distance);

		float coef = world.TraceMove(trace, null);
		if (coef >= 0.85)
			return true;

		// A slope ahead is the road rising, not a wall.
		if (trace.TraceNorm[1] > 0.55)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool OverheadClear(vector position, float height)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		TraceParam trace = new TraceParam();
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		trace.Start = position + Vector(0, 0.4, 0);
		trace.End = position + Vector(0, 0.4 + height, 0);
		float coef = world.TraceMove(trace, null);
		return coef >= 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest patch of ground level enough for a mortar crew. The pad is a few metres across,
	//! so the tube, the loader, and the shells share one plane. False when the area is all slope.
	static bool FindLevelPad(vector center, float searchRadius, out vector position)
	{
		position = center;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		if (searchRadius < 20)
			searchRadius = 80;

		if (LevelPad(world, center, position))
			return true;

		float angle0 = Math.RandomFloat(0, Math.PI2);
		float ring = 12;
		while (ring <= searchRadius)
		{
			int steps = Math.Round(Math.PI2 * ring / 14);
			if (steps < 8)
				steps = 8;

			for (int i = 0; i < steps; i++)
			{
				float angle = angle0 + (i / (float)steps) * Math.PI2;
				vector sample = center + Vector(Math.Cos(angle) * ring, 0, Math.Sin(angle) * ring);
				if (LevelPad(world, sample, position))
					return true;
			}

			ring = ring + 14;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A level terrace in the ring. The map marker itself is often inside a house, so camps and guns
	//! start outside that point. Samples closer than minSeparation to `avoid` are skipped.
	static bool FindOpenGroundInRing(vector center, float minRadius, float maxRadius, array<vector> avoid, float minSeparation, out vector position)
	{
		position = center;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		if (minRadius < 4)
			minRadius = 4;

		if (maxRadius < minRadius)
			maxRadius = minRadius;

		float angle0 = Math.RandomFloat(0, Math.PI2);
		float ring = minRadius;
		while (ring <= maxRadius + 0.1)
		{
			int steps = Math.Round((Math.PI2 * ring) / 12);
			if (steps < 8)
				steps = 8;

			for (int i = 0; i < steps; i++)
			{
				float angle = angle0 + (i / (float)steps) * Math.PI2;
				vector sample = center + Vector(Math.Cos(angle) * ring, 0, Math.Sin(angle) * ring);
				if (Clearance(sample, avoid) < minSeparation)
					continue;

				vector grounded;
				if (!LevelPad(world, sample, grounded))
					continue;

				position = grounded;
				return true;
			}

			ring = ring + 10;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Flat ground with no wall inside the disk. A few metres of pad is not enough for a camp:
	//! the flag and the bunker sit several metres from the ammo stack.
	//! minRoadDistance keeps the camp in a field or a yard instead of on the carriageway. 0 skips that test.
	static bool FindClearing(vector center, float minRadius, float maxRadius, float diskRadius, array<vector> avoid, float minSeparation, float minRoadDistance, out vector position)
	{
		position = center;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		if (diskRadius < 2)
			diskRadius = 2;

		if (maxRadius < minRadius)
			maxRadius = minRadius;

		float angle0 = Math.RandomFloat(0, Math.PI2);
		if (minRadius <= 0 && AcceptClearing(world, center, diskRadius, avoid, minSeparation, minRoadDistance, position))
			return true;

		float ring = minRadius;
		if (ring < 12)
			ring = 12;

		while (ring <= maxRadius + 0.1)
		{
			int steps = Math.Round((Math.PI2 * ring) / 18);
			if (steps < 10)
				steps = 10;

			for (int i = 0; i < steps; i++)
			{
				float angle = angle0 + (i / (float)steps) * Math.PI2;
				vector sample = center + Vector(Math.Cos(angle) * ring, 0, Math.Sin(angle) * ring);
				if (AcceptClearing(world, sample, diskRadius, avoid, minSeparation, minRoadDistance, position))
					return true;
			}

			ring = ring + 14;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Sandbag gun with room behind it. The weapon faces away from the camp, so the milician
	//! approaches from the camp side and is not standing in the next house.
	static bool FindGunClearing(vector camp, float minRadius, float maxRadius, array<vector> avoid, float minSeparation, out vector position, out vector angles)
	{
		position = camp;
		angles = "0 0 0";

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		if (minRadius < 8)
			minRadius = 8;

		if (maxRadius < minRadius)
			maxRadius = minRadius;

		float angle0 = Math.RandomFloat(0, Math.PI2);
		float ring = minRadius;
		while (ring <= maxRadius + 0.1)
		{
			int steps = Math.Round((Math.PI2 * ring) / 16);
			if (steps < 8)
				steps = 8;

			for (int i = 0; i < steps; i++)
			{
				float angle = angle0 + (i / (float)steps) * Math.PI2;
				vector sample = camp + Vector(Math.Cos(angle) * ring, 0, Math.Sin(angle) * ring);
				if (Clearance(sample, avoid) < minSeparation)
					continue;

				vector away = sample - camp;
				away[1] = 0;
				if (away.Length() < 1)
					continue;

				float yaw = Math.Atan2(away[0], away[2]) * Math.RAD2DEG;
				vector grounded;
				if (!OpenDisk(world, sample, 3.6, grounded))
					continue;

				if (!GunApproachClear(world, grounded, yaw))
					continue;

				position = grounded;
				angles = UprightYaw(yaw);
				return true;
			}

			ring = ring + 12;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool AcceptClearing(BaseWorld world, vector sample, float diskRadius, array<vector> avoid, float minSeparation, float minRoadDistance, out vector position)
	{
		position = sample;

		if (Clearance(sample, avoid) < minSeparation)
			return false;

		if (minRoadDistance > 0 && DistanceToRoad(sample) < minRoadDistance)
			return false;

		return OpenDisk(world, sample, diskRadius, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Every sample in the disk is ground, not a roof, and a chest-height line from the centre reaches it.
	protected static bool OpenDisk(BaseWorld world, vector sample, float radius, out vector grounded)
	{
		grounded = sample;

		float centerY;
		vector normal;
		if (!GroundAt(world, sample, centerY, normal))
			return false;

		if (normal[1] < 0.85)
			return false;

		vector center = sample;
		center[1] = centerY;
		if (!OverheadClear(center, 3.5))
			return false;

		for (int ring = 1; ring <= 2; ring++)
		{
			float reach = radius * (ring / 2.0);
			for (int i = 0; i < 8; i++)
			{
				float angle = (i / 8.0) * Math.PI2;
				vector edge = center + Vector(Math.Cos(angle) * reach, 0, Math.Sin(angle) * reach);
				float edgeY;
				vector edgeNormal;
				if (!GroundAt(world, edge, edgeY, edgeNormal))
					return false;

				if (edgeNormal[1] < 0.85)
					return false;

				if (Math.AbsFloat(edgeY - centerY) > 2)
					return false;

				vector edgeGround = edge;
				edgeGround[1] = edgeY;
				if (!OverheadClear(edgeGround, 2.4))
					return false;

				if (!PassageClear(world, center, edgeGround))
					return false;

				// A wall just past the last ring still clips the bunker and the flag.
				if (ring == 2)
				{
					vector beyond = center + Vector(Math.Cos(angle) * (reach + 1.4), 0, Math.Sin(angle) * (reach + 1.4));
					beyond[1] = edgeY;
					if (!PassageClear(world, edgeGround, beyond))
						return false;
				}
			}
		}

		grounded = center;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The stand behind the gun, and a step to either side, so a soldier can climb into the turret.
	protected static bool GunApproachClear(BaseWorld world, vector gun, float yaw)
	{
		float yawRad = yaw * Math.DEG2RAD;
		vector forward = Vector(Math.Sin(yawRad), 0, Math.Cos(yawRad));
		vector right = Vector(Math.Cos(yawRad), 0, -Math.Sin(yawRad));
		vector back = forward * -1;

		for (int step = 1; step <= 3; step++)
		{
			float distance = (step * 1.6) + 0.8;
			vector spot = gun + (back * distance);
			float hitY;
			vector normal;
			if (!GroundAt(world, spot, hitY, normal))
				return false;

			if (normal[1] < 0.82)
				return false;

			if (Math.AbsFloat(hitY - gun[1]) > 1.4)
				return false;

			vector grounded = spot;
			grounded[1] = hitY;
			if (!OverheadClear(grounded, 2.2))
				return false;

			if (!PassageClear(world, gun, grounded))
				return false;

			if (step == 2)
			{
				if (!PassageClear(world, grounded, grounded + (right * 1.5)))
					return false;

				if (!PassageClear(world, grounded, grounded - (right * 1.5)))
					return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Chest height. A tree is not a house; a wall or a solid is.
	protected static bool PassageClear(BaseWorld world, vector from, vector to)
	{
		TraceParam trace = new TraceParam();
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		trace.Start = from;
		trace.Start[1] = from[1] + 1.25;
		trace.End = to;
		trace.End[1] = to[1] + 1.25;

		float coef = world.TraceMove(trace, null);
		if (coef >= 0.88)
			return true;

		if (trace.TraceNorm[1] > 0.45)
			return true;

		if (IsVegetation(trace.TraceEnt))
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsVegetation(IEntity entity)
	{
		if (!entity)
			return false;

		ResourceName prefabName = SCR_ResourceNameUtils.GetPrefabName(entity);
		if (prefabName.IsEmpty())
			return false;

		string path = prefabName;
		if (path.Contains("Vegetation/"))
			return true;

		if (path.Contains("/Trees/"))
			return true;

		if (path.Contains("/Bushes/"))
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static float DistanceToRoad(vector center)
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return 100000;

		RoadNetworkManager roadManager = aiWorld.GetRoadNetworkManager();
		if (!roadManager)
			return 100000;

		BaseRoad foundRoad;
		float distance;
		if (roadManager.GetClosestRoad(center, foundRoad, distance, false) < 0)
			return 100000;

		return distance;
	}

	//------------------------------------------------------------------------------------------------
	//! True when this point and a ring around it are a walkable terrace, on the ground, and clear overhead.
	protected static bool LevelPad(BaseWorld world, vector sample, out vector grounded)
	{
		grounded = sample;

		float hitY;
		vector normal;
		if (!GroundAt(world, sample, hitY, normal))
			return false;

		// About 26 degrees. A hilltop compound is a terrace, not a floor.
		if (normal[1] < 0.90)
			return false;

		if (!OverheadClear(Vector(sample[0], hitY, sample[2]), 2.2))
			return false;

		// Tube, loader, and shell box. A crate or a sandbag may spoil one or two samples.
		float pad = 3.2;
		int misses = 0;
		for (int i = 0; i < 8; i++)
		{
			float angle = (i / 8.0) * Math.PI2;
			vector edge = sample + Vector(Math.Cos(angle) * pad, 0, Math.Sin(angle) * pad);
			float edgeY;
			vector edgeNormal;
			bool miss = !GroundAt(world, edge, edgeY, edgeNormal);
			if (!miss && (edgeNormal[1] < 0.90 || Math.AbsFloat(edgeY - hitY) > 1.25))
				miss = true;

			if (miss)
				misses++;
		}

		if (misses > 2)
			return false;

		grounded = sample;
		grounded[1] = hitY;
		return true;
	}

	protected static ref array<IEntity> s_aTowerPriority;
	protected static ref array<IEntity> s_aTowerOther;
	protected static vector s_vTowerQueryCenter;
	protected static float s_fTowerQueryRadius;

	//------------------------------------------------------------------------------------------------
	//! A flat roof, or a narrow tower ledge such as a minaret balcony.
	//! The highest stand that clears the listed groups and players is used.
	static bool FindElevatedStand(vector center, float radius, array<vector> avoid, float minSeparation, array<vector> players, float playerClearance, out vector position)
	{
		position = center;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		if (radius < 20)
			radius = 20;

		array<vector> pads = {};
		for (int i = 0; i < 40; i++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float reach = Math.Sqrt(Math.RandomFloat(0, 1)) * radius;
			vector sample = center + Vector(Math.Cos(angle) * reach, 0, Math.Sin(angle) * reach);
			vector pad;
			if (!ElevatedPad(world, sample, pad))
				continue;

			if (!ElevatedStandClear(pad, avoid, minSeparation, players, playerClearance))
				continue;

			pads.Insert(pad);
		}

		AppendTowerPads(world, center, radius, avoid, minSeparation, players, pads);

		if (pads.IsEmpty())
			return false;

		vector best = pads[0];
		for (int p = 1; p < pads.Count(); p++)
		{
			if (pads[p][1] > best[1])
				best = pads[p];
		}

		position = best;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ElevatedStandClear(vector pad, array<vector> avoid, float minSeparation, array<vector> players, float playerClearance)
	{
		if (minSeparation > 0 && Clearance(pad, avoid) < minSeparation)
			return false;

		if (playerClearance > 0 && NearestFlatDistance(pad, players) < playerClearance)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Towers are too narrow for the random roof samples. Their own entity is traced instead.
	//! A minaret balcony may sit under the dome, so a ring below the tip is accepted.
	protected static void AppendTowerPads(BaseWorld world, vector center, float radius, array<vector> avoid, float minSeparation, array<vector> players, notnull array<vector> pads)
	{
		if (!s_aTowerPriority)
			s_aTowerPriority = new array<IEntity>();
		else
			s_aTowerPriority.Clear();

		if (!s_aTowerOther)
			s_aTowerOther = new array<IEntity>();
		else
			s_aTowerOther.Clear();

		s_vTowerQueryCenter = center;
		s_fTowerQueryRadius = radius;
		world.QueryEntitiesBySphere(center, radius, CollectTowerEntity, null, EQueryEntitiesFlags.STATIC);

		// Close enough that he does not appear in the street beside a player. Far closer than a roof may be.
		float towerClearance = 40;

		foreach (IEntity priority : s_aTowerPriority)
		{
			if (!priority)
				continue;

			vector origin = priority.GetOrigin();
			TryTowerColumns(world, origin, 0, avoid, minSeparation, players, towerClearance, pads);
			TryTowerColumns(world, origin, 2.5, avoid, minSeparation, players, towerClearance, pads);
			TryTowerColumns(world, origin, 5, avoid, minSeparation, players, towerClearance, pads);
		}

		int traced = 0;
		foreach (IEntity other : s_aTowerOther)
		{
			if (!other)
				continue;

			if (traced >= 120)
				break;

			traced++;
			vector origin = other.GetOrigin();
			vector pad;
			if (!TowerPerch(world, origin[0], origin[2], origin, pad))
				continue;

			if (!ElevatedStandClear(pad, avoid, minSeparation, players, towerClearance))
				continue;

			pads.Insert(pad);
		}

		s_aTowerPriority.Clear();
		s_aTowerOther.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectTowerEntity(IEntity entity)
	{
		if (!entity)
			return true;

		if (DistanceFlat(entity.GetOrigin(), s_vTowerQueryCenter) > s_fTowerQueryRadius)
			return true;

		if (IsVegetation(entity))
			return true;

		ResourceName prefabName = SCR_ResourceNameUtils.GetPrefabName(entity);
		if (prefabName.IsEmpty())
			return true;

		string path = prefabName;
		path.ToLower();
		if (path.Contains("minaret") || path.Contains("minar") || path.Contains("mosque") || path.Contains("masjid") || path.Contains("tower") || path.Contains("spire"))
		{
			s_aTowerPriority.Insert(entity);
			return true;
		}

		if (path.Contains("/props/") || path.Contains("vegetation") || path.Contains("/rock") || path.Contains("stone") || path.Contains("clutter"))
			return true;

		if (path.Contains("structure") || path.Contains("building") || path.Contains("house"))
			s_aTowerOther.Insert(entity);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void TryTowerColumns(BaseWorld world, vector origin, float ring, array<vector> avoid, float minSeparation, array<vector> players, float playerClearance, notnull array<vector> pads)
	{
		int steps = 1;
		if (ring > 0.1)
			steps = 4;

		for (int i = 0; i < steps; i++)
		{
			float angle = (i / (float)steps) * Math.PI2;
			float x = origin[0] + (Math.Cos(angle) * ring);
			float z = origin[2] + (Math.Sin(angle) * ring);
			vector pad;
			if (!TowerPerch(world, x, z, origin, pad))
				continue;

			if (!ElevatedStandClear(pad, avoid, minSeparation, players, playerClearance))
				continue;

			pads.Insert(pad);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The crown of a tower, or a ledge just under a dome. False when the column is only a house.
	//! hub is the tower entity, so a balcony stand is pulled in off the outer lip.
	protected static bool TowerPerch(BaseWorld world, float x, float z, vector hub, out vector pad)
	{
		pad = Vector(x, 0, z);

		float terrainY = world.GetSurfaceY(x, z);
		float peakY;
		vector peakNormal;
		IEntity peakEntity;
		if (!TraceDownHit(world, Vector(x, 0, z), terrainY + 60, terrainY + 4, peakY, peakNormal, peakEntity))
			return false;

		if (peakY < terrainY + 8)
			return false;

		if (IsVegetation(peakEntity))
			return false;

		vector crown = Vector(x, peakY + 0.05, z);
		if (peakNormal[1] >= 0.8 && TowerFooting(world, x, peakY, z) && !StandIsOnEdge(world, crown, hub))
		{
			pad = crown;
			return true;
		}

		float bestY = -1;
		vector best;
		bool found = false;
		for (int ring = 0; ring < 4; ring++)
		{
			float reach = 0.6 + (ring * 0.7);
			for (int i = 0; i < 8; i++)
			{
				float angle = (i / 8.0) * Math.PI2;
				float sx = x + (Math.Cos(angle) * reach);
				float sz = z + (Math.Sin(angle) * reach);
				float hitY;
				vector normal;
				IEntity hitEntity;
				if (!TraceDownHit(world, Vector(sx, 0, sz), peakY + 2, peakY - 8, hitY, normal, hitEntity))
					continue;

				if (hitY > peakY - 0.5)
					continue;

				if (hitY < terrainY + 6)
					continue;

				// The balcony bowl is curved, so a lip-flat test rejects the walkable floor.
				if (normal[1] < 0.5)
					continue;

				if (IsVegetation(hitEntity))
					continue;

				if (!TowerFooting(world, sx, hitY, sz))
					continue;

				vector candidate = Vector(sx, hitY + 0.05, sz);
				if (!found)
				{
					bestY = hitY;
					best = candidate;
					found = true;
					continue;
				}

				// A lower storey, or a much higher dome. Stay on this balcony.
				if (hitY > bestY + 1.2 || hitY < bestY - 1.2)
					continue;

				// The raised outer lip used to win by being higher. Keep the side nearer the shaft.
				if (DistanceFlat(candidate, hub) < DistanceFlat(best, hub))
				{
					bestY = hitY;
					best = candidate;
				}
			}
		}

		if (!found)
			return false;

		if (!StandIsOnEdge(world, best, hub))
		{
			pad = best;
			return true;
		}

		vector pulled;
		if (!PullOffTheLip(world, best, hub, pulled))
			return false;

		pad = pulled;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the drop is right beside the stand. A roof with floor all around is not an edge.
	protected static bool StandIsOnEdge(BaseWorld world, vector stand, vector hub)
	{
		int solid = 0;
		for (int i = 0; i < 6; i++)
		{
			float angle = (i / 6.0) * Math.PI2;
			float sx = stand[0] + (Math.Cos(angle) * 0.75);
			float sz = stand[2] + (Math.Sin(angle) * 0.75);
			if (!SameFloor(world, stand, sx, sz, 0.55, 0.65, 0.4))
				continue;

			solid++;
		}

		// A real roof has floor all around. A balcony lip does not, even when the rim is wide.
		if (solid >= 5)
			return false;

		vector shaft;
		if (!ShaftAim(world, stand, hub, shaft))
			return true;

		float dx = stand[0] - shaft[0];
		float dz = stand[2] - shaft[2];
		float dist = Math.Sqrt((dx * dx) + (dz * dz));
		if (dist < 0.3)
			return false;

		float scale = 0.75 / dist;
		float ox = stand[0] + (dx * scale);
		float oz = stand[2] + (dz * scale);
		// A rise is the lip behind him. Only a drop beside his feet is the edge.
		return !SameFloor(world, stand, ox, oz, 0.9, 0.5, 1.2);
	}

	//------------------------------------------------------------------------------------------------
	//! Leave the outer lip and stop on the balcony floor, short of the shaft.
	protected static bool PullOffTheLip(BaseWorld world, vector stand, vector hub, out vector pad)
	{
		pad = stand;

		vector shaft;
		if (!ShaftAim(world, stand, hub, shaft))
			return false;

		float dx = shaft[0] - stand[0];
		float dz = shaft[2] - stand[2];
		float dist = Math.Sqrt((dx * dx) + (dz * dz));
		if (dist < 1.0)
			return false;

		float originY = stand[1];
		vector kept = stand;
		bool placed = false;
		int misses = 0;
		float moved = 0;
		float step = 0.25;
		while (moved + step <= 1.7 && moved + step < dist - 0.45)
		{
			moved = moved + step;
			float scale = moved / dist;
			float sx = stand[0] + (dx * scale);
			float sz = stand[2] + (dz * scale);
			float hitY;
			vector normal;
			IEntity hitEntity;
			if (!TraceDownHit(world, Vector(sx, 0, sz), originY + 1.5, originY - 2.0, hitY, normal, hitEntity))
			{
				misses++;
				if (misses >= 3)
					break;

				continue;
			}

			if (IsVegetation(hitEntity) || normal[1] < 0.5 || hitY > originY + 0.35 || hitY < originY - 1.6)
			{
				misses++;
				if (misses >= 3)
					break;

				continue;
			}

			if (!TowerFooting(world, sx, hitY, sz))
			{
				if (placed)
					break;

				misses++;
				if (misses >= 3)
					break;

				continue;
			}

			misses = 0;
			if (moved < 0.75)
				continue;

			vector candidate = Vector(sx, hitY + 0.05, sz);
			if (StandIsOnEdge(world, candidate, hub))
				continue;

			kept = candidate;
			placed = true;
		}

		if (!placed)
			return false;

		pad = kept;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Chest-height wall toward the tower. The entity pivot is used when the ray hits it.
	protected static bool ShaftAim(BaseWorld world, vector stand, vector hub, out vector shaft)
	{
		shaft = hub;

		vector chest = stand;
		chest[1] = stand[1] + 0.9;
		float towardHub;
		if (WallDistanceToward(world, chest, hub, 4.0, towardHub))
			return true;

		float best = 5;
		bool found = false;
		for (int i = 0; i < 8; i++)
		{
			float angle = (i / 8.0) * Math.PI2;
			vector aim = chest + Vector(Math.Cos(angle) * 4, 0, Math.Sin(angle) * 4);
			float hitDist;
			if (!WallDistance(world, chest, aim, hitDist))
				continue;

			if (hitDist < 0.35 || hitDist >= best)
				continue;

			best = hitDist;
			shaft = stand + Vector(Math.Cos(angle) * (hitDist + 0.3), 0, Math.Sin(angle) * (hitDist + 0.3));
			shaft[1] = stand[1];
			found = true;
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SameFloor(BaseWorld world, vector stand, float x, float z, float drop, float minNormalY, float rise)
	{
		float hitY;
		vector normal;
		IEntity hitEntity;
		if (!TraceDownHit(world, Vector(x, 0, z), stand[1] + 1.5, stand[1] - 1.6, hitY, normal, hitEntity))
			return false;

		if (IsVegetation(hitEntity))
			return false;

		if (normal[1] < minNormalY)
			return false;

		if (hitY > stand[1] + rise || hitY < stand[1] - drop)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool WallDistanceToward(BaseWorld world, vector from, vector hub, float reach, out float hitDist)
	{
		hitDist = 0;

		vector flatHub = hub;
		flatHub[1] = from[1];
		float dist = DistanceFlat(from, flatHub);
		if (dist < 0.3)
			return false;

		float scale = reach / dist;
		if (scale > 1)
			scale = 1;

		vector end = from;
		end[0] = from[0] + ((flatHub[0] - from[0]) * scale);
		end[2] = from[2] + ((flatHub[2] - from[2]) * scale);
		end[1] = from[1];
		return WallDistance(world, from, end, hitDist);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool WallDistance(BaseWorld world, vector start, vector end, out float hitDist)
	{
		hitDist = 0;

		TraceParam trace = new TraceParam();
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		trace.Start = start;
		trace.End = end;

		float coef = world.TraceMove(trace, null);
		if (coef <= 0.02 || coef >= 1)
			return false;

		hitDist = vector.Distance(start, end) * coef;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Headroom for a man on a balcony. A dome may sit close above the ledge.
	protected static bool TowerFooting(BaseWorld world, float x, float hitY, float z)
	{
		vector stand = Vector(x, hitY, z);
		return OverheadClear(stand, 1.05);
	}

	//------------------------------------------------------------------------------------------------
	//! First solid at least 3.5 m above the landscape, flat enough to stand, with headroom and a small level disk.
	protected static bool ElevatedPad(BaseWorld world, vector sample, out vector pad)
	{
		pad = sample;

		float terrainY = world.GetSurfaceY(sample[0], sample[2]);
		float hitY;
		vector normal;
		IEntity hitEntity;
		if (!TraceDownHit(world, sample, terrainY + 40, terrainY + 3, hitY, normal, hitEntity))
			return false;

		if (hitY < terrainY + 3.5)
			return false;

		if (hitY > terrainY + 40)
			return false;

		if (normal[1] < 0.85)
			return false;

		if (IsVegetation(hitEntity))
			return false;

		vector stand = sample;
		stand[1] = hitY;
		if (!OverheadClear(stand, 2))
			return false;

		for (int i = 0; i < 4; i++)
		{
			float angle = (i / 4.0) * Math.PI2;
			vector edge = sample + Vector(Math.Cos(angle) * 0.75, 0, Math.Sin(angle) * 0.75);
			float edgeTerrain = world.GetSurfaceY(edge[0], edge[2]);
			float edgeY;
			vector edgeNormal;
			IEntity edgeEntity;
			if (!TraceDownHit(world, edge, edgeTerrain + 40, edgeTerrain + 3, edgeY, edgeNormal, edgeEntity))
				return false;

			if (edgeNormal[1] < 0.85)
				return false;

			if (Math.AbsFloat(edgeY - hitY) > 0.35)
				return false;

			if (IsVegetation(edgeEntity))
				return false;
		}

		// Just above the slab so the body does not start inside it.
		stand[1] = hitY + 0.05;
		pad = stand;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Terrain under the sample, not a roof or a boulder top.
	protected static bool GroundAt(BaseWorld world, vector sample, out float hitY, out vector normal)
	{
		hitY = sample[1];
		normal = vector.Up;

		float terrainY = world.GetSurfaceY(sample[0], sample[2]);
		if (!TraceDown(world, sample, terrainY + 30, terrainY - 40, EPhysicsLayerDefs.Projectile, hitY, normal))
			return false;

		if (hitY > terrainY + 1.5)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static vector StandOnSurface(vector position)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return position;

		float hitY;
		vector normal;
		bool hit = TraceDown(world, position, position[1] + 3, position[1] - 4, EPhysicsLayerDefs.Projectile, hitY, normal);
		if (hit && normal[1] >= 0.45)
			position[1] = hitY;
		else
			position[1] = world.GetSurfaceY(position[0], position[2]);

		return position;
	}

	//------------------------------------------------------------------------------------------------
	//! Downward trace. False when it misses or starts already inside a solid.
	protected static bool TraceDown(BaseWorld world, vector position, float top, float bottom, int layerMask, out float hitY, out vector normal)
	{
		hitY = position[1];
		normal = vector.Up;

		TraceParam trace = new TraceParam();
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = layerMask;
		trace.Start = position;
		trace.Start[1] = top;
		trace.End = position;
		trace.End[1] = bottom;

		float coef = world.TraceMove(trace, null);
		if (coef <= 0.01 || coef >= 1)
			return false;

		hitY = trace.Start[1] + ((trace.End[1] - trace.Start[1]) * coef);
		normal = trace.TraceNorm;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Downward trace that also returns the entity, so a canopy can be told from a roof.
	protected static bool TraceDownHit(BaseWorld world, vector position, float top, float bottom, out float hitY, out vector normal, out IEntity hitEntity)
	{
		hitY = position[1];
		normal = vector.Up;
		hitEntity = null;

		TraceParam trace = new TraceParam();
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		trace.Start = position;
		trace.Start[1] = top;
		trace.End = position;
		trace.End[1] = bottom;

		float coef = world.TraceMove(trace, null);
		if (coef <= 0.01 || coef >= 1)
			return false;

		hitY = trace.Start[1] + ((trace.End[1] - trace.Start[1]) * coef);
		normal = trace.TraceNorm;
		hitEntity = trace.TraceEnt;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ClearVelocity(IEntity entity)
	{
		if (!entity)
			return;

		Physics physics = entity.GetPhysics();
		if (!physics)
			return;

		physics.SetVelocity(vector.Zero);
		physics.SetAngularVelocity(vector.Zero);
	}

	//------------------------------------------------------------------------------------------------
	static int CountLivingAgents(SCR_AIGroup group)
	{
		if (!group)
			return 0;

		return group.GetAgentsCount();
	}

	//------------------------------------------------------------------------------------------------
	static void DeleteEntitySafe(IEntity entity)
	{
		if (!entity)
			return;

		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! True for settlement-like map name markers (includes Kunar LOCAL hamlets).
	static bool IsSettlementMapDescriptor(IEntity entity)
	{
		if (!entity)
			return false;

		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (!mapdesc)
			mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(SCR_MapDescriptorComponent));
		if (!mapdesc)
			return false;

		int type = mapdesc.GetBaseType();
		if (type == EMapDescriptorType.MDT_NAME_CITY)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_TOWN)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_VILLAGE)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_SETTLEMENT)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_LOCAL)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_GENERIC)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Prefer map display name, then entity name, then fallback. Resolves #loc keys when possible.
	static string ResolveMapDescriptorName(MapDescriptorComponent mapdesc, IEntity entity, string fallback)
	{
		string name;
		if (mapdesc && mapdesc.Item())
			name = mapdesc.Item().GetDisplayName();

		if (name.IsEmpty() && entity)
			name = entity.GetName();

		if (!name.IsEmpty() && name.StartsWith("#"))
		{
			string translated = WidgetManager.Translate(name);
			if (!translated.IsEmpty() && translated != name)
			{
				name = translated;
			}
			else
			{
				array<string> parts = {};
				name.Split("_", parts, true);
				if (parts.Count() > 0)
					name = parts[parts.Count() - 1];
			}
		}

		if (name.IsEmpty())
			name = fallback;

		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Safe World Editor / debug entity name from a settlement display string.
	static string SanitizeEntityName(string name)
	{
		if (name.IsEmpty())
			return "Town";

		name.Replace(" ", "_");
		name.Replace("#", "");
		name.Replace("-", "_");
		name.Replace(".", "_");
		name.Replace("/", "_");
		return name;
	}
}
