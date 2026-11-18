class JOLP_HideoutPieceComponentClass : JOLP_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Marker on a hideout nest or roadblock. Destruction of the watched part keeps that piece from returning.
class JOLP_HideoutPieceComponent : JOLP_Component
{
	[Attribute(defvalue: "0", desc: "When set, only the static weapon's destruction retires the piece. A sandbag hit does not.")]
	protected bool m_bWatchTurret;

	protected int m_iLocationId = -1;
	protected int m_iPieceIndex = -1;
	protected bool m_bHooked;
	protected bool m_bDestroyedHandled;
	protected int m_iHookAttempts;

	//------------------------------------------------------------------------------------------------
	void Configure(int locationId, int pieceIndex)
	{
		m_iLocationId = locationId;
		m_iPieceIndex = pieceIndex;
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

		IEntity watched = owner;
		if (m_bWatchTurret)
		{
			IEntity turret = JOLP_WorldUtils.FindTurretEntity(owner);
			if (!turret)
			{
				RetryHook();
				return;
			}

			watched = turret;
		}

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(watched.FindComponent(SCR_DamageManagerComponent));
		if (!damage)
		{
			RetryHook();
			return;
		}

		damage.GetOnDamageStateChanged().Insert(OnDamageStateChanged);
		m_bHooked = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void RetryHook()
	{
		m_iHookAttempts++;
		if (m_iHookAttempts < 20)
			GetGame().GetCallqueue().CallLater(HookDamage, 500, false);
		else
			Print(string.Format("[JOLP] Hideout piece %1 at location %2 has no damage manager — destroy may not register", m_iPieceIndex, m_iLocationId), LogLevel.WARNING);
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
		if (!m_bDestroyedHandled && Replication.IsServer() && m_iLocationId >= 0)
			NotifyDestroyed();

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void NotifyDestroyed()
	{
		if (m_bDestroyedHandled)
			return;

		if (m_iLocationId < 0 || m_iPieceIndex < 0)
			return;

		m_bDestroyedHandled = true;

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (locations)
			locations.OnHideoutPieceDestroyed(m_iLocationId, m_iPieceIndex);
	}
}
