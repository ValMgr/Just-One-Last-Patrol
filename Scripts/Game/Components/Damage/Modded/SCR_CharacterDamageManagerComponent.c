modded class SCR_CharacterDamageManagerComponent : SCR_ExtendedDamageManagerComponent
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
		GetOnDamage().Insert(Jolp_OnDamage);
		GetOnDamageStateChanged().Insert(Jolp_OnState);
	}

	//------------------------------------------------------------------------------------------------
	protected void Jolp_OnDamage(BaseDamageContext damageContext)
	{
		JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
		if (!civilians || !damageContext)
			return;

		IEntity instigator;
		if (damageContext.instigator)
			instigator = damageContext.instigator.GetInstigatorEntity();

		civilians.OnCharacterDamaged(GetOwner(), instigator);
	}

	//------------------------------------------------------------------------------------------------
	protected void Jolp_OnState(EDamageState state)
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

		civilians.OnCharacterDestroyed(GetOwner(), instigator);
	}
}
