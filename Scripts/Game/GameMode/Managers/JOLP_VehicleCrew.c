//------------------------------------------------------------------------------------------------
//! Dedicated operators for a milician vehicle. They are not part of the infantry pool.
class JOLP_VehicleCrew
{
	//------------------------------------------------------------------------------------------------
	static SCR_AIGroup Spawn(notnull JOLP_MilicianGarrisonConfig config, JOLP_VehicleClass kind, vector position, vector angles)
	{
		if (config.m_rCrewGroupPrefab.IsEmpty())
			return null;

		ResourceName characterPrefab = config.CrewCharacterPrefab(kind);
		if (characterPrefab.IsEmpty())
			return null;

		int count = config.CrewCount(kind);
		IEntity groupEnt = JOLP_WorldUtils.SpawnEntityPrefab(config.m_rCrewGroupPrefab, position, angles, true);
		SCR_AIGroup group = SCR_AIGroup.Cast(groupEnt);
		if (!group)
			return null;

		group.SetDeleteWhenEmpty(false);
		group.SetMaxMembers(count);
		group.SetNumberOfMembersToSpawn(0);

		float yawRad = angles[0] * Math.DEG2RAD;
		vector right = Vector(Math.Cos(yawRad), 0, -Math.Sin(yawRad));

		for (int i = 0; i < count; i++)
		{
			vector offset = position + (right * (i * 0.8));
			IEntity body = JOLP_WorldUtils.SpawnEntityPrefab(characterPrefab, offset, angles, true);
			if (!body)
				continue;

			if (!group.AddAIEntityToGroup(body))
				JOLP_WorldUtils.DeleteEntitySafe(body);
		}

		return group;
	}

	//------------------------------------------------------------------------------------------------
	//! Driver first, then guns. Anyone still outside takes a cargo seat so they leave with the hull.
	static void Seat(SCR_AIGroup group, IEntity vehicle)
	{
		if (!group || !vehicle)
			return;

		array<IEntity> loose = {};
		CollectLoose(group, vehicle, loose, true);
		if (loose.IsEmpty())
			return;

		FillSeats(vehicle, loose, ECompartmentType.PILOT);
		FillSeats(vehicle, loose, ECompartmentType.TURRET);
		FillSeats(vehicle, loose, ECompartmentType.CARGO);
	}

	//------------------------------------------------------------------------------------------------
	//! Passengers only. Pilot and turret seats stay with the crew.
	static void SeatCargo(SCR_AIGroup group, IEntity vehicle)
	{
		if (!group || !vehicle)
			return;

		array<IEntity> loose = {};
		CollectLoose(group, vehicle, loose, false);
		FillSeats(vehicle, loose, ECompartmentType.CARGO);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsHullDestroyed(IEntity vehicle)
	{
		if (!vehicle)
			return true;

		DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
		return damage && damage.IsDestroyed();
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectLoose(notnull SCR_AIGroup group, notnull IEntity vehicle, notnull array<IEntity> loose, bool includeCargo)
	{
		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));

		array<IEntity> pilots = {};
		array<IEntity> gunners = {};
		array<IEntity> cargo = {};
		if (compartments)
		{
			compartments.GetOccupantsOfType(pilots, ECompartmentType.PILOT);
			compartments.GetOccupantsOfType(gunners, ECompartmentType.TURRET);
			compartments.GetOccupantsOfType(cargo, ECompartmentType.CARGO);
		}

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity body = agent.GetControlledEntity();
			if (!body)
				continue;

			if (pilots.Find(body) >= 0 || gunners.Find(body) >= 0)
				continue;

			bool inCargo = cargo.Find(body) >= 0;
			if (inCargo && !includeCargo)
				continue;

			SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(body.FindComponent(SCR_CompartmentAccessComponent));
			if (access && access.IsInCompartment() && !inCargo)
				continue;

			loose.Insert(body);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void FillSeats(notnull IEntity vehicle, notnull array<IEntity> loose, ECompartmentType compartmentType)
	{
		for (int i = loose.Count() - 1; i >= 0; i--)
		{
			IEntity body = loose[i];
			if (!body)
			{
				loose.Remove(i);
				continue;
			}

			SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(body.FindComponent(SCR_CompartmentAccessComponent));
			if (!access)
				continue;

			if (!access.MoveInVehicle(vehicle, compartmentType))
				continue;

			loose.Remove(i);
		}
	}
}
