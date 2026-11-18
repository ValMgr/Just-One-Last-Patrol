class JOLP_LocationControllerComponentClass : JOLP_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! World-authored marker for a town, OP, or hideout. Registers with the location manager.
class JOLP_LocationControllerComponent : JOLP_Component
{
	[Attribute("", desc: "Display name (e.g. Asadabad, OP_Test_North, Hideout_NorthRidge)")]
	protected string m_sName;

	[Attribute("0", UIWidgets.ComboBox, "Location type", "", ParamEnumArray.FromEnum(JOLP_LocationType))]
	protected JOLP_LocationType m_eType;

	[Attribute("1", UIWidgets.ComboBox, "Settlement size hint (towns). Civilian counts come from this unless overridden", "", ParamEnumArray.FromEnum(JOLP_SettlementSize))]
	protected JOLP_SettlementSize m_eSettlementSize;

	[Attribute("-1", desc: "Foot villager budget override (-1 = use the size default from the civilian config)")]
	protected int m_iCivilianFootOverride;

	[Attribute("-1", desc: "Concurrent civilian car slots override (-1 = use the size default)")]
	protected int m_iCivilianVehicleOverride;

	[Attribute("400", UIWidgets.Slider, "Activation radius in metres", "50 2000 10")]
	protected float m_fActivationRadius;

	[Attribute("20", desc: "Initial milician garrison stored at this location")]
	protected int m_iInitialGarrison;

	[Attribute("0.65", UIWidgets.Slider, "Fraction of garrison to spawn when players are near", "0.1 1 0.05")]
	protected float m_fActiveGarrisonRatio;

	[Attribute("-1", desc: "Hard max live agents override (-1 = use ratio × stored)")]
	protected int m_iMaxActiveOverride;

	[Attribute("1", desc: "Allow sentry packages")]
	protected bool m_bAllowSentry;

	[Attribute("1", desc: "Allow patrol packages (towns/OPs)")]
	protected bool m_bAllowPatrol;

	[Attribute("1", desc: "Allow checkpoint packages (towns/OPs)")]
	protected bool m_bAllowCheckpoint;

	[Attribute("1", desc: "Allow player-triggered reinforcement from this settlement")]
	protected bool m_bAllowReinforcement;

	[Attribute("-1", desc: "Hideout reinforce interval ms (-1 = manager default 30 min)")]
	protected int m_iHideoutReinforceIntervalMs;

	[Attribute("-1", desc: "Hideout reinforce group size override (-1 = manager reinforce agent count)")]
	protected int m_iHideoutReinforceGroupSize;

	[Attribute("2500", desc: "Hideout search range for weakest town to reinforce")]
	protected float m_fHideoutReinforceSearchRange;

	[Attribute("4", desc: "Hideout keeps at least this many stored men when sending reinforcements")]
	protected int m_iHideoutDefenseReserve;

	protected int m_iLocationId = -1;
	protected bool m_bRegistered;

#ifdef WORKBENCH
	protected ref Shape m_ActivationRangeShape;
#endif

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		if (JOLP_Component.IsPreviewInstance(owner))
			return;

		GetGame().GetCallqueue().CallLater(TryRegister, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void TryRegister()
	{
		if (m_bRegistered)
			return;

		JOLP_LocationManagerComponent manager = JOLP_Global.GetLocations();
		if (!manager)
		{
			GetGame().GetCallqueue().CallLater(TryRegister, 100, false);
			return;
		}

		m_iLocationId = manager.RegisterLocation(this);
		m_bRegistered = m_iLocationId >= 0;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_bRegistered)
		{
			JOLP_LocationManagerComponent manager = JOLP_Global.GetLocations();
			if (manager)
				manager.UnregisterLocation(this);
		}

		super.OnDelete(owner);
	}

	string GetLocationName() { return m_sName; }
	void SetLocationName(string name) { m_sName = name; }

	JOLP_LocationType GetLocationType() { return m_eType; }
	void SetLocationType(JOLP_LocationType locationType) { m_eType = locationType; }

	JOLP_SettlementSize GetSettlementSize() { return m_eSettlementSize; }
	void SetSettlementSize(JOLP_SettlementSize size) { m_eSettlementSize = size; }

	int GetCivilianFootOverride() { return m_iCivilianFootOverride; }
	void SetCivilianFootOverride(int count) { m_iCivilianFootOverride = count; }

	int GetCivilianVehicleOverride() { return m_iCivilianVehicleOverride; }
	void SetCivilianVehicleOverride(int count) { m_iCivilianVehicleOverride = count; }

	float GetActivationRadius() { return m_fActivationRadius; }
	void SetActivationRadius(float radius) { m_fActivationRadius = radius; }

	int GetInitialGarrison() { return m_iInitialGarrison; }
	void SetInitialGarrison(int garrison) { m_iInitialGarrison = garrison; }

	float GetActiveGarrisonRatio() { return m_fActiveGarrisonRatio; }
	int GetMaxActiveOverride() { return m_iMaxActiveOverride; }

	bool GetAllowSentry() { return m_bAllowSentry; }
	bool GetAllowPatrol() { return m_bAllowPatrol; }
	bool GetAllowCheckpoint() { return m_bAllowCheckpoint; }
	bool GetAllowReinforcement() { return m_bAllowReinforcement; }

	void SetAllowSentry(bool allow) { m_bAllowSentry = allow; }
	void SetAllowPatrol(bool allow) { m_bAllowPatrol = allow; }
	void SetAllowCheckpoint(bool allow) { m_bAllowCheckpoint = allow; }
	void SetAllowReinforcement(bool allow) { m_bAllowReinforcement = allow; }

	int GetHideoutReinforceIntervalMs() { return m_iHideoutReinforceIntervalMs; }
	int GetHideoutReinforceGroupSize() { return m_iHideoutReinforceGroupSize; }
	float GetHideoutReinforceSearchRange() { return m_fHideoutReinforceSearchRange; }
	int GetHideoutDefenseReserve() { return m_iHideoutDefenseReserve; }

	int GetLocationId() { return m_iLocationId; }
	void SetLocationId(int locationId) { m_iLocationId = locationId; }

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
		int color = Color.FromRGBA(200, 80, 40, 30).PackToInt();
		if (m_eType == JOLP_LocationType.HIDEOUT)
			color = Color.FromRGBA(160, 40, 160, 35).PackToInt();
		else if (m_eType == JOLP_LocationType.OP)
			color = Color.FromRGBA(40, 120, 200, 30).PackToInt();

		m_ActivationRangeShape = Shape.CreateSphere(
			color,
			ShapeFlags.TRANSP | ShapeFlags.NOOUTLINE,
			owner.GetOrigin(),
			m_fActivationRadius);

		super._WB_AfterWorldUpdate(owner, timeSlice);
	}
#endif
}
