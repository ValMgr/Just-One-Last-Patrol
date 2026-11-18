class JOLP_PersistentCompositionComponentClass : JOLP_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Marks a player-built composition so persistence can self-spawn it.
//! Do not put this on the placed Thunderbolt HQ. That entity is already in the layer, and
//! this component's config has SelfSpawn, so the placed building would come back twice.
//! Tracking waits until the composition is finished, so an outline is not saved as a FOB.
class JOLP_PersistentCompositionComponent : JOLP_Component
{
	[Attribute("", desc: "FOB name applied when this composition is a headquarters")]
	protected string m_sFOBName;

	protected bool m_bTracked;

	//------------------------------------------------------------------------------------------------
	string GetFOBName()
	{
		return m_sFOBName;
	}

	//------------------------------------------------------------------------------------------------
	void SetFOBName(string name)
	{
		m_sFOBName = name;
	}

	//------------------------------------------------------------------------------------------------
	//! Assigns saved identity and supply stock. Safe to run again on a live session.
	void ApplyPersisted(string fobName, float supplies)
	{
		if (!fobName.IsEmpty())
			m_sFOBName = fobName;

		JOLP_FOBControllerComponent fob = JOLP_FOBControllerComponent.Cast(GetOwner().FindComponent(JOLP_FOBControllerComponent));
		if (fob && !m_sFOBName.IsEmpty())
			fob.SetFOBName(m_sFOBName);

		JOLP_SupplyStock.Write(GetOwner(), supplies);

		JOLP_SupplyIncomeComponent income = JOLP_SupplyIncomeComponent.Cast(GetOwner().FindComponent(JOLP_SupplyIncomeComponent));
		if (income)
			income.MarkInitialSeedApplied();
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

		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(owner.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (!composition || composition.IsCompositionSpawned())
		{
			TrackFinished();
			return;
		}

		composition.GetOnCompositionSpawned().Insert(OnCompositionSpawned);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(owner.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (composition)
			composition.GetOnCompositionSpawned().Remove(OnCompositionSpawned);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCompositionSpawned(bool spawned)
	{
		if (!spawned)
			return;

		TrackFinished();
	}

	//------------------------------------------------------------------------------------------------
	protected void TrackFinished()
	{
		if (m_bTracked)
			return;

		m_bTracked = true;

		JOLP_FOBControllerComponent fob = JOLP_FOBControllerComponent.Cast(GetOwner().FindComponent(JOLP_FOBControllerComponent));
		if (fob && !m_sFOBName.IsEmpty())
			fob.SetFOBName(m_sFOBName);

		JOLP_PersistenceTracking.Track(GetOwner());
	}
}
