class JOLP_FOBControllerComponentClass : JOLP_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Player FOB marker. Settlements whose centre falls inside m_fSuppressionRadius are suppressed:
//! milician will not spawn there or pick them as reinforcement targets.
//!
//! Buildable HQs set m_bDeferRegistrationUntilArmed so unfinished outlines do not suppress towns.
//! Call ArmAndRegister() once SCR_CampaignBuildingCompositionComponent reports completion.
class JOLP_FOBControllerComponent : JOLP_Component
{
	[Attribute("FOB", desc: "Display name (e.g. FOB_Asadabad)")]
	protected string m_sName;

	[Attribute("400", UIWidgets.Slider, "Disable milician locations whose centre is within this radius (m)", "50 2000 10")]
	protected float m_fSuppressionRadius;

	[Attribute("0", UIWidgets.CheckBox, "When true, wait for ArmAndRegister() before registering with the location manager (player-built HQs)")]
	protected bool m_bDeferRegistrationUntilArmed;

	protected bool m_bRegistered;
	protected bool m_bArmed;
	protected bool m_bNameAssigned;
	protected ref SCR_MapMarkerBase m_MapMarker;

#ifdef WORKBENCH
	protected ref Shape m_SuppressionRangeShape;
#endif

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		if (JOLP_Component.IsPreviewInstance(owner))
			return;

		if (m_bDeferRegistrationUntilArmed)
		{
			m_bArmed = false;
			GetGame().GetCallqueue().CallLater(TryHookComposition, 0, false);
			return;
		}

		m_bArmed = true;
		GetGame().GetCallqueue().CallLater(TryRegister, 0, false);

		// Only HQs that already carry a composition component need the rebuild hook.
		if (SCR_CampaignBuildingCompositionComponent.Cast(owner.FindComponent(SCR_CampaignBuildingCompositionComponent)))
			GetGame().GetCallqueue().CallLater(TryHookComposition, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void TryHookComposition()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(
			owner.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (!composition)
		{
			// Buildable HQs can receive the composition component a frame later.
			if (m_bDeferRegistrationUntilArmed)
				GetGame().GetCallqueue().CallLater(TryHookComposition, 250, false);
			return;
		}

		if (composition.IsCompositionSpawned())
		{
			ArmAndRegister();
			return;
		}

		composition.GetOnCompositionSpawned().Insert(OnCompositionSpawned);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCompositionSpawned(bool spawned)
	{
		if (!spawned)
			return;

		ArmAndRegister();
	}

	//------------------------------------------------------------------------------------------------
	//! Runtime deferral for persistence / buildable HQ flows (also used by JOLP_PersistentCompositionComponent).
	void DeferRegistrationUntilArmed()
	{
		m_bDeferRegistrationUntilArmed = true;
		m_bArmed = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Marks the FOB active and registers suppression with the location manager.
	void ArmAndRegister()
	{
		if (m_bArmed && m_bRegistered)
			return;

		m_bArmed = true;

		if (!m_bNameAssigned && (!m_sName || m_sName.IsEmpty() || m_sName == "FOB"))
			m_sName = GenerateFOBName();

		TryRegister();
	}

	//------------------------------------------------------------------------------------------------
	//! Alias used by JOLP_PersistentCompositionComponent.
	void ArmRegistration()
	{
		ArmAndRegister();
	}

	//------------------------------------------------------------------------------------------------
	protected string GenerateFOBName()
	{
		vector pos = GetPosition();
		return string.Format("FOB_%1_%2", Math.Round(pos[0]), Math.Round(pos[2]));
	}

	//------------------------------------------------------------------------------------------------
	protected void TryRegister()
	{
		if (m_bRegistered || !m_bArmed)
			return;

		JOLP_LocationManagerComponent manager = JOLP_Global.GetLocations();
		if (!manager)
		{
			GetGame().GetCallqueue().CallLater(TryRegister, 100, false);
			return;
		}

		manager.RegisterFOB(this);
		m_bRegistered = true;
		PlaceFactionMapMarker();
	}

	//------------------------------------------------------------------------------------------------
	//! Faction-visible static map pin so the whole side can find the FOB.
	protected void PlaceFactionMapMarker()
	{
		if (m_MapMarker)
			return;

		if (!Replication.IsServer())
			return;

		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markerMgr)
		{
			GetGame().GetCallqueue().CallLater(PlaceFactionMapMarker, 1000, false);
			return;
		}

		vector pos = GetPosition();
		SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
		marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		marker.SetWorldPos(pos[0], pos[2]);
		marker.SetCustomText(GetFOBName());
		// marker.SetColor(Color(0.0, 0.0, 1.0, 1.0)); // blue
		marker.SetIconEntry(10); // Flag icon
		marker.SetCanBeRemovedByOwner(false);

		bool isLocal = RplSession.Mode() == RplMode.None;
		markerMgr.InsertStaticMarker(marker, isLocal, !isLocal);
		m_MapMarker = marker;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveFactionMapMarker()
	{
		if (!m_MapMarker)
			return;

		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (markerMgr)
			markerMgr.RemoveStaticMarker(m_MapMarker);

		m_MapMarker = null;
	}

	//------------------------------------------------------------------------------------------------
	void ClearFactionMapMarker()
	{
		RemoveFactionMapMarker();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(
			owner.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (composition)
			composition.GetOnCompositionSpawned().Remove(OnCompositionSpawned);

		RemoveFactionMapMarker();

		if (m_bRegistered)
		{
			JOLP_LocationManagerComponent manager = JOLP_Global.GetLocations();
			if (manager)
				manager.UnregisterFOB(this);
			m_bRegistered = false;
		}

		super.OnDelete(owner);
	}

	bool IsArmed()
	{
		return m_bArmed;
	}

	bool IsRegistrationDeferred()
	{
		return m_bDeferRegistrationUntilArmed;
	}

	string GetFOBName()
	{
		return m_sName;
	}

	void SetFOBName(string name)
	{
		m_sName = name;
		m_bNameAssigned = true;

		if (!m_MapMarker)
			return;

		RemoveFactionMapMarker();
		PlaceFactionMapMarker();
	}

	float GetSuppressionRadius()
	{
		return m_fSuppressionRadius;
	}

	void SetSuppressionRadius(float radius)
	{
		m_fSuppressionRadius = radius;
	}

	//------------------------------------------------------------------------------------------------
	vector GetPosition()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return vector.Zero;

		return owner.GetOrigin();
	}

#ifdef WORKBENCH
	override int _WB_GetAfterWorldUpdateSpecs(IEntity owner, IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_SELECTED;
	}

	protected override void _WB_AfterWorldUpdate(IEntity owner, float timeSlice)
	{
		m_SuppressionRangeShape = Shape.CreateSphere(
			Color.FromRGBA(40, 160, 80, 28).PackToInt(),
			ShapeFlags.TRANSP | ShapeFlags.NOOUTLINE,
			owner.GetOrigin(),
			m_fSuppressionRadius);

		super._WB_AfterWorldUpdate(owner, timeSlice);
	}
#endif
}
