class JOLP_FOBDeployNetworkComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Client asks, server places the headquarters and removes the command truck.
class JOLP_FOBDeployNetworkComponent : ScriptComponent
{
	static const int MAX_NAME_LENGTH = 32;
	static const float MAX_DISTANCE_M = 8;

	protected static const ResourceName VEHICLE_PREFAB = "{664B13B0A1000710}Prefabs/Vehicles/Wheeled/JOLP_M1083_Command_Tan.et";
	protected static const ResourceName FOB_PREFAB = "{664B13B0A1000700}Prefabs/Compositions/Slotted/SlotFlatSmall/JOLP_Headquarters_Deployed.et";

	//------------------------------------------------------------------------------------------------
	static JOLP_FOBDeployNetworkComponent Local()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return null;

		return JOLP_FOBDeployNetworkComponent.Cast(controller.FindComponent(JOLP_FOBDeployNetworkComponent));
	}

	//------------------------------------------------------------------------------------------------
	void AskDeploy(RplId vehicleId, string name)
	{
		Rpc(RpcAsk_DeployFOB, vehicleId, name);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DeployFOB(RplId vehicleId, string name)
	{
		PlayerController controller = PlayerController.Cast(GetOwner());
		if (!controller)
			return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(controller.GetPlayerId());
		IEntity vehicle = null;
		RplComponent vehicleReplication = RplComponent.Cast(Replication.FindItem(vehicleId));
		if (vehicleReplication)
			vehicle = vehicleReplication.GetEntity();

		string rejection = Rejection(player, vehicle, name);
		if (!rejection.IsEmpty())
		{
			Rpc(RpcDo_DeployFOBResult, false, rejection);
			return;
		}

		string trimmed = name.Trim();
		if (!Establish(vehicle, trimmed))
		{
			Rpc(RpcDo_DeployFOBResult, false, "The headquarters could not be placed.");
			return;
		}

		Rpc(RpcDo_DeployFOBResult, true, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_DeployFOBResult(bool established, string message)
	{
		JOLP_FOBNameDialog.Deliver(established, message);
	}

	//------------------------------------------------------------------------------------------------
	protected string Rejection(IEntity player, IEntity vehicle, string name)
	{
		if (!player || !vehicle)
			return "That truck cannot establish a FOB.";

		if (!IsCommandTruck(vehicle))
			return "That truck cannot establish a FOB.";

		ChimeraCharacter character = ChimeraCharacter.Cast(player);
		if (!character || character.IsInVehicle())
			return "Step outside the truck.";

		if (vector.Distance(player.GetOrigin(), vehicle.GetOrigin()) > MAX_DISTANCE_M)
			return "You are too far from the truck.";

		if (!IsEmpty(vehicle))
			return "The truck has to be empty.";

		if (!name)
			return "Enter a name.";

		string trimmed = name.Trim();
		if (trimmed.IsEmpty())
			return "Enter a name.";

		if (trimmed.Length() > MAX_NAME_LENGTH)
			return "That name is too long.";

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return "The base list is not ready.";

		if (locations.IsPositionInsideFOB(vehicle.GetOrigin()))
			return "A FOB already covers this ground.";

		if (NameTaken(locations, trimmed))
			return "That name is already in use.";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Establish(IEntity vehicle, string name)
	{
		Resource resource = Resource.Load(FOB_PREFAB);
		if (!resource || !resource.IsValid())
		{
			Print(string.Format("[JOLP] Deploy FOB failed to load '%1'", FOB_PREFAB), LogLevel.WARNING);
			return false;
		}

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		vehicle.GetTransform(spawnParams.Transform);

		IEntity headquarters = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
		if (!headquarters)
		{
			Print("[JOLP] Deploy FOB spawn returned nothing", LogLevel.WARNING);
			return false;
		}

		JOLP_FOBControllerComponent fob = JOLP_FOBControllerComponent.Cast(headquarters.FindComponent(JOLP_FOBControllerComponent));
		if (!fob)
		{
			Print("[JOLP] Deployed headquarters has no FOB controller", LogLevel.WARNING);
			SCR_EntityHelper.DeleteEntityAndChildren(headquarters);
			return false;
		}

		JOLP_PersistentCompositionComponent composition = JOLP_PersistentCompositionComponent.Cast(headquarters.FindComponent(JOLP_PersistentCompositionComponent));
		if (composition)
			composition.SetFOBName(name);

		fob.SetFOBName(name);

		if (!JOLP_PersistenceTracking.IsTracked(headquarters))
			JOLP_PersistenceTracking.Track(headquarters);

		JOLP_PersistenceManagerComponent.UntrackTransient(vehicle);
		JOLP_VehicleSpawnCatalog.PushSuppressRefund();
		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		JOLP_VehicleSpawnCatalog.PopSuppressRefund();

		Print(string.Format("[JOLP] Established FOB '%1' at %2", name, headquarters.GetOrigin().ToString()), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsCommandTruck(IEntity vehicle)
	{
		if (!Vehicle.Cast(vehicle))
			return false;

		ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(vehicle);
		return prefab == VEHICLE_PREFAB;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsEmpty(IEntity vehicle)
	{
		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartments)
			return false;

		return compartments.GetOccupantCount() == 0;
	}

	//------------------------------------------------------------------------------------------------
	protected bool NameTaken(JOLP_LocationManagerComponent locations, string name)
	{
		array<JOLP_FOBControllerComponent> fobs = locations.GetFOBs();
		if (!fobs)
			return false;

		string candidate = name;
		candidate.ToLower();

		foreach (JOLP_FOBControllerComponent fob : fobs)
		{
			if (!fob)
				continue;

			string existing = fob.GetFOBName();
			if (!existing || existing.IsEmpty())
				continue;

			existing.ToLower();
			if (existing == candidate)
				return true;
		}

		return false;
	}
}
