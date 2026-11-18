class JOLP_HideoutHQComponentClass : JOLP_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Marker on the physical hideout HQ prop. Destruction ends that hideout.
class JOLP_HideoutHQComponent : JOLP_Component
{
	protected int m_iLocationId = -1;
	protected bool m_bHooked;
	protected bool m_bDestroyedHandled;
	protected int m_iHookAttempts;

	//------------------------------------------------------------------------------------------------
	void Configure(int locationId)
	{
		m_iLocationId = locationId;
	}

	//------------------------------------------------------------------------------------------------
	int GetLocationId()
	{
		return m_iLocationId;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		if (JOLP_Component.IsPreviewInstance(owner))
			return;

		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(HookDamage, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void HookDamage()
	{
		if (m_bHooked)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(owner.FindComponent(SCR_DamageManagerComponent));
		if (!damage)
		{
			m_iHookAttempts++;
			if (m_iHookAttempts < 20)
				GetGame().GetCallqueue().CallLater(HookDamage, 500, false);
			else
				Print(string.Format("[JOLP] Hideout HQ at location %1 has no damage manager — destroy may not register", m_iLocationId), LogLevel.WARNING);
			return;
		}

		damage.GetOnDamageStateChanged().Insert(OnDamageStateChanged);
		m_bHooked = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDamageStateChanged(EDamageState state)
	{
		if (state != EDamageState.DESTROYED)
			return;

		NotifyDestroyed();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (!m_bDestroyedHandled && Replication.IsServer())
			NotifyDestroyed();

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void NotifyDestroyed()
	{
		if (m_bDestroyedHandled)
			return;

		m_bDestroyedHandled = true;

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (locations)
			locations.OnHideoutHQDestroyed(m_iLocationId);
	}
}
