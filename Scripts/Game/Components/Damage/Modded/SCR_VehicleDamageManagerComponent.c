modded class SCR_VehicleDamageManagerComponent
{
	protected bool m_bJolpHooked;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_bJolpHooked || !owner || SCR_Global.IsEditMode())
			return;

		if (!Replication.IsServer())
			return;

		m_bJolpHooked = true;
		GetOnDamage().Insert(Jolp_OnVehicleDamage);
		GetOnDamageStateChanged().Insert(Jolp_OnVehicleState);
	}

	//------------------------------------------------------------------------------------------------
	protected void Jolp_OnVehicleDamage(BaseDamageContext damageContext)
	{
		JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
		if (!civilians || !damageContext)
			return;

		IEntity instigator;
		if (damageContext.instigator)
			instigator = damageContext.instigator.GetInstigatorEntity();

		civilians.OnVehicleDamaged(GetOwner(), instigator, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Jolp_OnVehicleState(EDamageState state)
	{
		if (state != EDamageState.DESTROYED)
			return;

		JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
		if (!civilians)
			return;

		IEntity instigator;
		Instigator source = GetInstigator();
		if (source)
			instigator = source.GetInstigatorEntity();

		civilians.OnVehicleDamaged(GetOwner(), instigator, true);
	}
}
