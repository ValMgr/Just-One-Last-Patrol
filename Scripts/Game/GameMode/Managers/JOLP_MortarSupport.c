//------------------------------------------------------------------------------------------------
//! One live 2B14 battery. The crew is not part of the OP garrison.
class JOLP_MortarMission : Managed
{
	int m_iFightLocationId = -1;
	int m_iOPLocationId = -1;
	EntityID m_GroupId;
	EntityID m_MortarId;
	EntityID m_AmmoBoxId;
	//! Given straight to the group. A waypoint would sit on the impact point and walk the crew toward it.
	ref SCR_AIStaticArtilleryActivity m_Order;
	vector m_vMortarPosition;
	vector m_vLastContact;
	bool m_bHasContact;
	bool m_bRegistered;
	bool m_bOrderActive;
	//! Ranging shots are 0, 1 and 2. Later values are fire-for-effect salvos.
	int m_iOrdersCompleted;
	//! Attempts in a row that got no shell away. Three of these end the mission.
	int m_iFastFails;
	vector m_vAim;
	//! Counted from the tube itself. The vanilla activity does not reliably report its own completion.
	int m_iShotsWanted;
	int m_iShotsFired;
	int m_iShotsAtAttempt;
	float m_fSpawnedAtWorldTime;
	float m_fOrderIssuedAtWorldTime = -1;
	float m_fLastProgressAtWorldTime = -1;
	//! Negative until a finished shell is waiting out its gap.
	float m_fResumeAtWorldTime = -1;
}

//------------------------------------------------------------------------------------------------
//! Spawns an OP mortar when a settlement first enters combat at very low or low reputation.
//! Three ranging shells walk onto the last known player position, then five salvos, then the tube is removed.
class JOLP_MortarSupport : Managed
{
	protected ref array<ref JOLP_MortarMission> m_aMissions = new array<ref JOLP_MortarMission>();
	protected ref array<int> m_aLatchedFights = new array<int>();

	//------------------------------------------------------------------------------------------------
	void Tick(notnull JOLP_MilicianManagerComponent milician, notnull JOLP_MilicianGarrisonConfig config, EAISkill skill)
	{
		float now = GetGame().GetWorld().GetWorldTime();
		AdvanceMissions(config, now);
		ConsiderFights(milician, config, skill, now);
	}

	//------------------------------------------------------------------------------------------------
	protected void AdvanceMissions(notnull JOLP_MilicianGarrisonConfig config, float now)
	{
		for (int i = m_aMissions.Count() - 1; i >= 0; i--)
		{
			JOLP_MortarMission mission = m_aMissions[i];
			if (!mission)
			{
				m_aMissions.Remove(i);
				continue;
			}

			AdvanceMission(mission, config, now);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AdvanceMission(notnull JOLP_MortarMission mission, notnull JOLP_MilicianGarrisonConfig config, float now)
	{
		// A battery at the OP is far from the fight. Without this the crew freezes and never takes the tube.
		KeepCrewSimulated(mission);

		if (!MortarIntact(mission))
		{
			Finish(mission, config, now, "the tube is gone");
			return;
		}

		int alive = LivingCrew(mission);
		if (alive <= 0)
		{
			if (now - mission.m_fSpawnedAtWorldTime > 15000)
				Finish(mission, config, now, "the crew is gone");
			return;
		}

		if (!mission.m_bRegistered)
		{
			if (RegisterMortar(mission))
				mission.m_bRegistered = true;
			else if (now - mission.m_fSpawnedAtWorldTime > 15000)
				Finish(mission, config, now, "the crew never took the tube");

			if (!mission.m_bRegistered)
				return;
		}

		if (!mission.m_bOrderActive)
		{
			if (mission.m_fResumeAtWorldTime >= 0 && now < mission.m_fResumeAtWorldTime)
				return;

			if (!IssueOrder(mission, config, now))
				Finish(mission, config, now, "no shot in range");
			return;
		}

		int timeout = Positive(config.m_iMortarOrderTimeoutMs, 480000);
		if (mission.m_fOrderIssuedAtWorldTime >= 0 && now - mission.m_fOrderIssuedAtWorldTime > timeout)
		{
			Finish(mission, config, now, "the fire order stalled");
			return;
		}

		if (mission.m_iShotsFired >= mission.m_iShotsWanted)
		{
			mission.m_iFastFails = 0;
			OnOrderFinished(mission, config, now);
			return;
		}

		if (!mission.m_Order)
		{
			if (now < mission.m_fResumeAtWorldTime)
				return;

			if (!SendOrder(mission, now))
				Finish(mission, config, now, "the crew lost its orders");
			return;
		}

		// Aiming, fetching and loading one shell takes about 30 s. A crew quiet for twice that has stopped.
		if (OrderPending(mission) && now - mission.m_fLastProgressAtWorldTime < 60000)
			return;

		bool progressed = mission.m_iShotsFired > mission.m_iShotsAtAttempt;
		DropOrder(mission);
		if (progressed)
			mission.m_iFastFails = 0;
		else
			mission.m_iFastFails = mission.m_iFastFails + 1;

		if (mission.m_iFastFails >= 3)
		{
			Finish(mission, config, now, "the crew could not fire");
			return;
		}

		mission.m_fResumeAtWorldTime = now + 5000;
	}

	//------------------------------------------------------------------------------------------------
	protected void ConsiderFights(notnull JOLP_MilicianManagerComponent milician, notnull JOLP_MilicianGarrisonConfig config, EAISkill skill, float now)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return;

		for (int i = m_aLatchedFights.Count() - 1; i >= 0; i--)
		{
			int locationId = m_aLatchedFights[i];
			if (!milician.IsLocationInCombat(locationId))
				m_aLatchedFights.Remove(i);
		}

		JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
		array<ref JOLP_LocationData> all = locations.GetAllLocations();
		if (!all)
			return;

		foreach (JOLP_LocationData data : all)
		{
			if (!data || !milician.IsLocationInCombat(data.m_iId))
				continue;

			if (m_aLatchedFights.Find(data.m_iId) >= 0)
				continue;

			m_aLatchedFights.Insert(data.m_iId);
			if (!data.IsSettlement() || !data.HasReputation() || !civilians)
				continue;

			JOLP_ReputationStage stage = civilians.GetStage(data);
			if (stage != JOLP_ReputationStage.VERY_LOW && stage != JOLP_ReputationStage.LOW)
				continue;

			TryStart(data, config, skill, now);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TryStart(notnull JOLP_LocationData fight, notnull JOLP_MilicianGarrisonConfig config, EAISkill skill, float now)
	{
		vector contact;
		if (!ResolveContact(fight, contact))
		{
			Print(string.Format("[JOLP] '%1' mortar skipped: no known player position", fight.m_sName), LogLevel.NORMAL);
			return;
		}

		JOLP_LocationData op = NearestOP(fight);
		if (!op)
		{
			Print(string.Format("[JOLP] '%1' mortar skipped: no milician OP on the map", fight.m_sName), LogLevel.NORMAL);
			return;
		}

		if (now < op.m_fMortarCooldownUntilWorldTime)
		{
			Print(string.Format("[JOLP] '%1' mortar skipped: '%2' is still cooling down", fight.m_sName, op.m_sName), LogLevel.NORMAL);
			return;
		}

		if (OPHasBattery(op.m_iId))
		{
			Print(string.Format("[JOLP] '%1' mortar skipped: '%2' already has a battery", fight.m_sName, op.m_sName), LogLevel.NORMAL);
			return;
		}

		float range = JOLP_WorldUtils.DistanceFlat(op.m_vPosition, contact);
		float minRange = PositiveF(config.m_fMortarMinRange, 150) + 50;
		float maxRange = PositiveF(config.m_fMortarMaxRange, 3000) - 50;
		if (range < minRange || range > maxRange)
		{
			Print(string.Format("[JOLP] '%1' mortar skipped: '%2' is out of 2B14 range", fight.m_sName, op.m_sName), LogLevel.NORMAL);
			return;
		}

		if (config.m_rMortarPrefab.IsEmpty() || config.m_rMortarAmmoBoxPrefab.IsEmpty() || config.m_rCrewGroupPrefab.IsEmpty() || config.m_rDriverPrefab.IsEmpty())
		{
			Print(string.Format("[JOLP] '%1' mortar skipped: a mortar prefab is missing", fight.m_sName), LogLevel.WARNING);
			return;
		}

		vector mortarPos;
		if (!PickMortarPosition(op.m_vPosition, mortarPos))
		{
			Print(string.Format("[JOLP] '%1' mortar skipped: '%2' has no level ground", fight.m_sName, op.m_sName), LogLevel.NORMAL);
			return;
		}

		vector toFight = contact - mortarPos;
		toFight[1] = 0;
		float yaw = 0;
		if (toFight.Length() >= 1)
			yaw = Math.Atan2(toFight[0], toFight[2]) * Math.RAD2DEG;
		vector angles = JOLP_WorldUtils.UprightYaw(yaw);

		IEntity mortar = JOLP_WorldUtils.SpawnEntityPrefab(config.m_rMortarPrefab, mortarPos, angles, true);
		if (!mortar)
		{
			Print(string.Format("[JOLP] '%1' mortar failed to spawn at '%2'", fight.m_sName, op.m_sName), LogLevel.WARNING);
			return;
		}

		JOLP_CivilianOutfitter.SetCharacterFaction(mortar, config.m_sFactionKey);
		HookMortar(mortar);
		mortarPos = mortar.GetOrigin();

		float yawRad = angles[0] * Math.DEG2RAD;
		vector forward = Vector(Math.Sin(yawRad), 0, Math.Cos(yawRad));
		vector right = Vector(Math.Cos(yawRad), 0, -Math.Sin(yawRad));
		// Kept inside the level pad. A longer offset walks the loader back onto the slope.
		vector ammoPos = mortarPos + (right * 2.4);
		ammoPos[1] = mortarPos[1];
		IEntity ammo = JOLP_WorldUtils.SpawnEntityPrefab(config.m_rMortarAmmoBoxPrefab, ammoPos, angles, true);
		// Leave the box on its authored faction. Retagging it removes the 82mm shells from the catalog.
		if (!ammo)
		{
			JOLP_WorldUtils.DeleteEntitySafe(mortar);
			Print(string.Format("[JOLP] '%1' mortar failed to spawn shells at '%2'", fight.m_sName, op.m_sName), LogLevel.WARNING);
			return;
		}

		if (!PrepareShellBox(ammo, mortar))
			Print(string.Format("[JOLP] '%1' mortar box at '%2' cannot hand out shells", fight.m_sName, op.m_sName), LogLevel.WARNING);

		vector crewPos = mortarPos - (forward * 2.4);
		crewPos[1] = mortarPos[1];
		IEntity groupEnt = JOLP_WorldUtils.SpawnEntityPrefab(config.m_rCrewGroupPrefab, crewPos, angles, true);
		SCR_AIGroup group = SCR_AIGroup.Cast(groupEnt);
		if (!group)
		{
			JOLP_WorldUtils.DeleteEntitySafe(groupEnt);
			JOLP_WorldUtils.DeleteEntitySafe(ammo);
			JOLP_WorldUtils.DeleteEntitySafe(mortar);
			Print(string.Format("[JOLP] '%1' mortar crew failed to spawn at '%2'", fight.m_sName, op.m_sName), LogLevel.WARNING);
			return;
		}

		int crewCount = config.m_iMortarCrew;
		if (crewCount < 1)
			crewCount = 2;

		group.SetDeleteWhenEmpty(false);
		group.SetMaxMembers(crewCount);
		group.SetNumberOfMembersToSpawn(0);

		ResourceName crewPrefab = config.m_rMortarCrewPrefab;
		if (crewPrefab.IsEmpty())
			crewPrefab = config.m_rDriverPrefab;

		int seated = 0;
		for (int i = 0; i < crewCount; i++)
		{
			vector offset = crewPos + (right * (i * 0.8));
			offset[1] = mortarPos[1];
			IEntity body = JOLP_WorldUtils.SpawnEntityPrefab(crewPrefab, offset, angles, true);
			if (!body)
				continue;

			if (!group.AddAIEntityToGroup(body))
			{
				JOLP_WorldUtils.DeleteEntitySafe(body);
				continue;
			}

			JOLP_CivilianOutfitter.SetCharacterFaction(body, config.m_sFactionKey);
			JOLP_AISkillConfig.Apply(body, skill);
			JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
			if (civilians)
				civilians.TrackMilician(body, op.m_iId, true);
			seated++;
		}

		JOLP_CivilianOutfitter.SetGroupFaction(group, config.m_sFactionKey);
		if (seated <= 0)
		{
			JOLP_WorldUtils.DeleteEntitySafe(group);
			JOLP_WorldUtils.DeleteEntitySafe(ammo);
			JOLP_WorldUtils.DeleteEntitySafe(mortar);
			Print(string.Format("[JOLP] '%1' mortar crew failed to spawn at '%2'", fight.m_sName, op.m_sName), LogLevel.WARNING);
			return;
		}

		JOLP_MortarMission mission = new JOLP_MortarMission();
		mission.m_iFightLocationId = fight.m_iId;
		mission.m_iOPLocationId = op.m_iId;
		mission.m_GroupId = group.GetID();
		mission.m_MortarId = mortar.GetID();
		mission.m_AmmoBoxId = ammo.GetID();
		mission.m_vMortarPosition = mortarPos;
		mission.m_vLastContact = contact;
		mission.m_bHasContact = true;
		mission.m_fSpawnedAtWorldTime = now;
		// The group brain is not listening on the spawn frame. The first shell waits until it is.
		mission.m_fResumeAtWorldTime = now + 4000;
		m_aMissions.Insert(mission);
		Print(string.Format("[JOLP] Mortar at '%1' is firing for '%2'", op.m_sName, fight.m_sName), LogLevel.NORMAL);
		AdvanceMission(mission, config, now);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IssueOrder(notnull JOLP_MortarMission mission, notnull JOLP_MilicianGarrisonConfig config, float now)
	{
		vector contact;
		if (!CurrentContact(mission, contact))
			return false;

		int index = mission.m_iOrdersCompleted;
		int shots = 1;
		float offset = 0;
		if (index <= 0)
			offset = PositiveF(config.m_fMortarRangingFirst, 150);
		else if (index == 1)
			offset = PositiveF(config.m_fMortarRangingSecond, 100);
		else if (index == 2)
			offset = PositiveF(config.m_fMortarRangingThird, 50);
		else
			shots = Positive(config.m_iMortarRoundsPerSalvo, 5);

		vector aim;
		if (!BuildAim(mission.m_vMortarPosition, contact, offset, config.m_fMortarAimJitter, config, aim))
			return false;

		mission.m_vAim = aim;
		mission.m_iShotsWanted = shots;
		mission.m_iShotsFired = 0;
		mission.m_bOrderActive = true;
		mission.m_fOrderIssuedAtWorldTime = now;
		return SendOrder(mission, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Hands the shells still owed on the current aim point to the crew.
	protected bool SendOrder(notnull JOLP_MortarMission mission, float now)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(mission.m_GroupId));
		if (!group)
			return false;

		SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
		if (!utility)
			return false;

		int shots = mission.m_iShotsWanted - mission.m_iShotsFired;
		if (shots < 1)
			shots = 1;

		DropOrder(mission);
		// Keeps the crew on the pad. An idle crew otherwise walks off to investigate the fight.
		utility.SetMaxAutonomousDistance(15);
		SCR_AIStaticArtilleryActivity order = new SCR_AIStaticArtilleryActivity(utility, null, mission.m_vAim, SCR_EAIArtilleryAmmoType.HIGH_EXPLOSIVE, shots);
		utility.AddAction(order);

		mission.m_Order = order;
		mission.m_iShotsAtAttempt = mission.m_iShotsFired;
		mission.m_fLastProgressAtWorldTime = now;
		mission.m_fResumeAtWorldTime = -1;

		string kind = "a ranging shell";
		if (mission.m_iShotsWanted > 1)
			kind = string.Format("%1 of a salvo of %2", shots, mission.m_iShotsWanted);
		Print(string.Format("[JOLP] Mortar for location %1 ordered %2", mission.m_iOPLocationId, kind), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void HookMortar(notnull IEntity mortar)
	{
		SCR_MortarMuzzleComponent muzzle = SCR_MortarMuzzleComponent.Cast(mortar.FindComponent(SCR_MortarMuzzleComponent));
		if (!muzzle)
			return;

		SCR_MuzzleEffectComponent effect = SCR_MuzzleEffectComponent.Cast(muzzle.FindComponent(SCR_MuzzleEffectComponent));
		if (effect)
			effect.GetOnWeaponFired().Insert(OnMortarFired);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMortarFired(IEntity effectEntity, BaseMuzzleComponent muzzle, IEntity projectileEntity)
	{
		if (!muzzle || !muzzle.GetOwner())
			return;

		EntityID mortarId = muzzle.GetOwner().GetID();
		foreach (JOLP_MortarMission mission : m_aMissions)
		{
			if (!mission || mission.m_MortarId != mortarId || !mission.m_bOrderActive)
				continue;

			mission.m_iShotsFired = mission.m_iShotsFired + 1;
			mission.m_fLastProgressAtWorldTime = GetGame().GetWorld().GetWorldTime();
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOrderFinished(notnull JOLP_MortarMission mission, notnull JOLP_MilicianGarrisonConfig config, float now)
	{
		DropOrder(mission);
		mission.m_bOrderActive = false;
		mission.m_iOrdersCompleted = mission.m_iOrdersCompleted + 1;

		int total = 3 + Positive(config.m_iMortarSalvoCount, 5);
		if (mission.m_iOrdersCompleted >= total)
		{
			Finish(mission, config, now, "the fire mission is over");
			return;
		}

		int gap = GapAfter(mission.m_iOrdersCompleted, config);
		mission.m_fResumeAtWorldTime = now + gap;
		Print(string.Format("[JOLP] Mortar for location %1 fired %2, next order in %3 s", mission.m_iOPLocationId, mission.m_iShotsFired, gap / 1000), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish(notnull JOLP_MortarMission mission, notnull JOLP_MilicianGarrisonConfig config, float now, string reason)
	{
		DropOrder(mission);

		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(mission.m_GroupId));
		if (group)
		{
			JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
			if (civilians)
				civilians.SuppressMilicianGroup(group);

			array<AIAgent> agents = {};
			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;

				agent.AllowMaxLOD();
				JOLP_WorldUtils.DeleteEntitySafe(agent.GetControlledEntity());
			}

			JOLP_WorldUtils.DeleteEntitySafe(group);
		}

		JOLP_WorldUtils.DeleteEntitySafe(GetGame().GetWorld().FindEntityByID(mission.m_MortarId));
		JOLP_WorldUtils.DeleteEntitySafe(GetGame().GetWorld().FindEntityByID(mission.m_AmmoBoxId));

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		JOLP_LocationData op = null;
		if (locations)
			op = locations.GetLocation(mission.m_iOPLocationId);
		if (op)
			op.m_fMortarCooldownUntilWorldTime = now + Positive(config.m_iMortarCooldownMs, 900000);

		for (int i = m_aMissions.Count() - 1; i >= 0; i--)
		{
			if (m_aMissions[i] == mission)
				m_aMissions.Remove(i);
		}

		string opName = "an OP";
		if (op)
			opName = op.m_sName;
		Print(string.Format("[JOLP] Mortar at '%1' stood down: %2", opName, reason), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! The loader only takes a shell the box lists, and pays supplies for it. Neither is reliable outside Conflict.
	protected bool PrepareShellBox(notnull IEntity ammo, notnull IEntity mortar)
	{
		SCR_AIStaticArtilleryVehicleUsageComponent usage = SCR_AIStaticArtilleryVehicleUsageComponent.Cast(
			mortar.FindComponent(SCR_AIStaticArtilleryVehicleUsageComponent));
		SCR_ArsenalInventoryStorageManagerComponent storage = SCR_ArsenalInventoryStorageManagerComponent.Cast(
			ammo.FindComponent(SCR_ArsenalInventoryStorageManagerComponent));
		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.FindArsenalComponent(ammo);
		if (!usage || !storage || !arsenal)
			return false;

		ResourceName shell = usage.GetAmmoResourceName(SCR_EAIArtilleryAmmoType.HIGH_EXPLOSIVE);
		if (shell.IsEmpty())
			return false;

		storage.JOLP_ForcePrefab(shell);
		arsenal.JOLP_SetFree(true);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool RegisterMortar(notnull JOLP_MortarMission mission)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(mission.m_GroupId));
		IEntity mortar = GetGame().GetWorld().FindEntityByID(mission.m_MortarId);
		if (!group || !mortar)
			return false;

		SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
		SCR_AIStaticArtilleryVehicleUsageComponent usage = SCR_AIStaticArtilleryVehicleUsageComponent.Cast(
			mortar.FindComponent(SCR_AIStaticArtilleryVehicleUsageComponent));
		if (!utility || !utility.m_VehicleMgr || !usage)
			return false;

		utility.m_VehicleMgr.TryAddVehicle(usage);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OrderPending(notnull JOLP_MortarMission mission)
	{
		if (!mission.m_Order)
			return false;

		EAIActionState state = mission.m_Order.GetActionState();
		return state != EAIActionState.COMPLETED && state != EAIActionState.FAILED;
	}

	//------------------------------------------------------------------------------------------------
	protected void DropOrder(notnull JOLP_MortarMission mission)
	{
		if (!mission.m_Order)
			return;

		if (OrderPending(mission))
			mission.m_Order.Complete();

		mission.m_Order = null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CurrentContact(notnull JOLP_MortarMission mission, out vector contact)
	{
		contact = mission.m_vLastContact;
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		JOLP_LocationData fight = null;
		if (locations)
			fight = locations.GetLocation(mission.m_iFightLocationId);

		vector fresh;
		if (fight && ResolveContact(fight, fresh))
		{
			mission.m_vLastContact = fresh;
			mission.m_bHasContact = true;
			contact = fresh;
			return true;
		}

		return mission.m_bHasContact;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ResolveContact(notnull JOLP_LocationData fight, out vector contact)
	{
		contact = fight.m_vPosition;
		if (fight.m_fPlayerContactWorldTime >= 0)
		{
			contact = Snap(fight.m_vPlayerContact);
			return true;
		}

		float radius = fight.m_fActivationRadius;
		if (radius < 40)
			radius = 40;

		array<vector> players = {};
		if (JOLP_WorldUtils.CollectLivingPlayerPositions(fight.m_vPosition, radius, players) <= 0)
			return false;

		vector best = players[0];
		float bestDist = JOLP_WorldUtils.DistanceFlat(fight.m_vPosition, best);
		for (int i = 1; i < players.Count(); i++)
		{
			float dist = JOLP_WorldUtils.DistanceFlat(fight.m_vPosition, players[i]);
			if (dist >= bestDist)
				continue;

			bestDist = dist;
			best = players[i];
		}

		contact = Snap(best);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A ranging shot keeps the written offset and a random bearing, as long as the tube can reach it.
	protected bool BuildAim(vector mortarPos, vector contact, float offset, float jitter, notnull JOLP_MilicianGarrisonConfig config, out vector aim)
	{
		aim = Snap(contact);
		if (offset <= 0)
			return InRange(mortarPos, aim, config);

		float distance = offset;
		if (jitter > 0)
			distance = offset + Math.RandomFloat(-jitter, jitter);
		if (distance < 10)
			distance = 10;

		for (int i = 0; i < 8; i++)
		{
			float bearing = Math.RandomFloat(0, Math.PI2);
			vector candidate = contact + Vector(Math.Cos(bearing) * distance, 0, Math.Sin(bearing) * distance);
			candidate = Snap(candidate);
			if (!InRange(mortarPos, candidate, config))
				continue;

			aim = candidate;
			return true;
		}

		return InRange(mortarPos, aim, config);
	}

	//------------------------------------------------------------------------------------------------
	protected bool InRange(vector from, vector aim, notnull JOLP_MilicianGarrisonConfig config)
	{
		float distance = JOLP_WorldUtils.DistanceFlat(from, aim);
		return distance >= PositiveF(config.m_fMortarMinRange, 150) && distance <= PositiveF(config.m_fMortarMaxRange, 3000);
	}

	//------------------------------------------------------------------------------------------------
	//! Closest OP still held by milician. Player FOBs are skipped so fire can come from the next post.
	protected JOLP_LocationData NearestOP(notnull JOLP_LocationData fight)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return null;

		array<ref JOLP_LocationData> all = locations.GetAllLocations();
		if (!all)
			return null;

		JOLP_LocationData best = null;
		float bestDist = -1;
		foreach (JOLP_LocationData data : all)
		{
			if (!data || !data.IsOP() || data.IsSuppressedByFOB())
				continue;

			float dist = JOLP_WorldUtils.DistanceFlat(fight.m_vPosition, data.m_vPosition);
			if (bestDist >= 0 && dist >= bestDist)
				continue;

			bestDist = dist;
			best = data;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OPHasBattery(int opLocationId)
	{
		foreach (JOLP_MortarMission mission : m_aMissions)
		{
			if (mission && mission.m_iOPLocationId == opLocationId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Flat ground near the post. Mountain OPs sit on slopes a crew cannot serve a mortar from.
	protected bool PickMortarPosition(vector center, out vector position)
	{
		return JOLP_WorldUtils.FindLevelPad(center, 100, position);
	}

	//------------------------------------------------------------------------------------------------
	protected void KeepCrewSimulated(notnull JOLP_MortarMission mission)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(mission.m_GroupId));
		if (!group)
			return;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (agent)
				agent.PreventMaxLOD();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool MortarIntact(notnull JOLP_MortarMission mission)
	{
		IEntity mortar = GetGame().GetWorld().FindEntityByID(mission.m_MortarId);
		return mortar && !JOLP_VehicleCrew.IsHullDestroyed(mortar);
	}

	//------------------------------------------------------------------------------------------------
	protected int LivingCrew(notnull JOLP_MortarMission mission)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(mission.m_GroupId));
		if (!group)
			return 0;

		return JOLP_WorldUtils.CountLivingAgents(group);
	}

	//------------------------------------------------------------------------------------------------
	protected int GapAfter(int ordersCompleted, notnull JOLP_MilicianGarrisonConfig config)
	{
		if (ordersCompleted <= 1)
			return Positive(config.m_iMortarGapAfterFirstMs, 60000);
		if (ordersCompleted == 2)
			return Positive(config.m_iMortarGapAfterSecondMs, 30000);
		if (ordersCompleted == 3)
			return Positive(config.m_iMortarGapAfterThirdMs, 30000);

		return Positive(config.m_iMortarSalvoIntervalMs, 180000);
	}

	//------------------------------------------------------------------------------------------------
	protected int Positive(int value, int fallback)
	{
		if (value <= 0)
			return fallback;

		return value;
	}

	//------------------------------------------------------------------------------------------------
	protected float PositiveF(float value, float fallback)
	{
		if (value <= 0)
			return fallback;

		return value;
	}

	//------------------------------------------------------------------------------------------------
	protected vector Snap(vector position)
	{
		BaseWorld world = GetGame().GetWorld();
		if (world)
			position[1] = world.GetSurfaceY(position[0], position[2]);

		return position;
	}
}
