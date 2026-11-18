//------------------------------------------------------------------------------------------------
//! Outside the empty command truck, asks for a FOB name. The server does the rest.
class JOLP_DeployFOBUserAction : ScriptedUserAction
{
	protected IEntity m_Owner;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);
		m_Owner = pOwnerEntity;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return IsDeployable(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return IsDeployable(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (pOwnerEntity && !m_Owner)
			m_Owner = pOwnerEntity;

		if (!IsDeployable(pUserEntity))
			return;

		IEntity vehicle = GetVehicle();
		if (!vehicle)
			return;

		RplComponent replication = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!replication)
			return;

		JOLP_FOBNameDialog.Open(replication.Id());
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity GetVehicle()
	{
		if (m_Owner)
			return m_Owner;

		return GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsDeployable(IEntity user)
	{
		IEntity vehicle = GetVehicle();
		if (!vehicle || !user)
			return false;

		if (!Vehicle.Cast(vehicle))
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character || character.IsInVehicle())
			return false;

		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartments)
			return false;

		return compartments.GetOccupantCount() == 0;
	}
}
