//------------------------------------------------------------------------------------------------
//! Authorable location kinds for milician garrisons.
enum JOLP_LocationType
{
	TOWN = 0,
	OP = 1,
	HIDEOUT = 2
}

//------------------------------------------------------------------------------------------------
//! Settlement size hint used by the town generator / defaults.
enum JOLP_SettlementSize
{
	VILLAGE = 0,
	TOWN = 1,
	CITY = 2
}

//------------------------------------------------------------------------------------------------
//! Milician vehicle pool. NONE means the live package has no vehicle.
enum JOLP_VehicleClass
{
	NONE = 0,
	TRANSPORT = 1,
	LIGHT = 2,
	HEAVY = 3
}

//------------------------------------------------------------------------------------------------
//! Spawned milician package roles.
enum JOLP_MilicianGroupRole
{
	SENTRY = 0,
	PATROL = 1,
	CHECKPOINT = 2,
	REINFORCEMENT = 3,
	//! Crewed vehicle left in a settlement after drop-off, or fielded as local defense.
	VEHICLE = 4
}

//------------------------------------------------------------------------------------------------
//! One live spawned package owned by the milician manager.
class JOLP_LiveGroupRecord : Managed
{
	EntityID m_GroupId;
	EntityID m_CompositionId;
	EntityID m_VehicleId;
	ref array<EntityID> m_aWaypointIds = new array<EntityID>();
	JOLP_MilicianGroupRole m_eRole;
	int m_iSourceLocationId = -1;
	int m_iTargetLocationId = -1;
	bool m_bExemptFromProximityDespawn;
	bool m_bArrived;
	//! Attack response. Hunts a spotted player, then absorbs into the target.
	bool m_bQRF;
	//! Defend order has replaced the move order.
	bool m_bQRFHolding;
	//! Search-and-destroy has replaced the road move.
	bool m_bQRFHunting;
	//! Contact the last QRF order was aimed at. A small shift must not reset the search.
	vector m_vQRFHuntPosition;
	//! World time of that order. Negative means the squad is still on the plain town move.
	float m_fQRFHuntOrderedAt = -1;
	//! Hideout wave. Not drawn from the source garrison, and absorbed as soon as it arrives.
	bool m_bFreeReinforcement;
	//! Arrived while a player was already fighting here. Hunts that player instead of entering the garrison.
	bool m_bCommittedFight;
	float m_fArrivedAtWorldTime = -1;
	float m_fSpawnedAtWorldTime = -1;
	int m_iChargedAgents;
	//! Operators. Distinct from the passenger squad until that squad is dropped off.
	EntityID m_CrewGroupId;
	JOLP_VehicleClass m_eVehicleClass;
	//! Truck or BTR. The passenger squad is m_GroupId until drop-off.
	bool m_bCarriesSquad;
	//! Post-arrival orders have been given to the crew.
	bool m_bVehicleOrdersSet;
	//! Hideout static this sentry pair was sent to man. -1 when the pair is not a gun crew.
	int m_iNestPieceIndex = -1;
	bool m_bNestGunnerSeated;
	int m_iNestSeatAttempts;
}

//------------------------------------------------------------------------------------------------
//! What a saved hideout piece is. The site is the camp the player must destroy.
enum JOLP_HideoutPieceKind
{
	SITE = 0,
	NEST = 1,
	ROADBLOCK = 2
}

//------------------------------------------------------------------------------------------------
//! One spawned hideout prop. The save stores the transform. The entity id is this session only.
class JOLP_HideoutPiece : Managed
{
	JOLP_HideoutPieceKind m_eKind;
	ResourceName m_sPrefab;
	vector m_vPosition;
	vector m_vAngles;
	bool m_bDestroyed;
	EntityID m_EntityId;
	//! A sentry pair has already been sent to man this nest.
	bool m_bGunnerAssigned;
}

//------------------------------------------------------------------------------------------------
//! Runtime record for one map location. Owned by JOLP_LocationManagerComponent.
class JOLP_LocationData : Managed
{
	int m_iId = -1;
	string m_sName;
	//! Authored name, captured once. Hideout promotion rewrites m_sName and must not rewrite this.
	string m_sPersistentKey;
	//! Set when a save record was applied. Reputation seeding skips these places.
	bool m_bRestoredFromSave;
	JOLP_LocationType m_eType;
	JOLP_SettlementSize m_eSettlementSize;
	vector m_vPosition;
	float m_fActivationRadius = 400;
	int m_iGarrisonStored;
	//! Unarmed trucks. Spawned vehicles are removed; a surviving hull is put back.
	int m_iTransportStored;
	//! Armed cars and BRDMs.
	int m_iLightStored;
	//! BTRs.
	int m_iHeavyStored;
	float m_fActiveGarrisonRatio = 0.65;
	int m_iMaxActiveOverride = -1;
	bool m_bAllowSentry = true;
	bool m_bAllowPatrol = true;
	bool m_bAllowCheckpoint = true;
	bool m_bAllowReinforcement = true;
	int m_iHideoutReinforceIntervalMs = -1;
	int m_iHideoutReinforceGroupSize = -1;
	float m_fHideoutReinforceSearchRange = 2500;
	int m_iHideoutDefenseReserve = 4;
	bool m_bActive;
	//! Civilian proximity is separate. Sharing m_bActive let one manager despawn the other's people.
	bool m_bCivilianActive;
	float m_fHideoutNextReinforceWorldTime;

	//! World time of the first player hit that started a QRF call. Negative means none is pending.
	float m_fQRFRequestWorldTime = -1;
	//! Further hits are ignored until this world time.
	float m_fQRFCooldownUntilWorldTime;

	//! World time before this OP may spawn another mortar battery. 0 means it is ready.
	float m_fMortarCooldownUntilWorldTime;

	//! Last place milician at this settlement saw a player. Zero until the first contact.
	vector m_vPlayerContact;
	//! World time of that sighting. Negative means none.
	float m_fPlayerContactWorldTime = -1;

	//! True while a player FOB covers this settlement — milician skip spawn/reinforce here.
	bool m_bSuppressedByFOB;

	//! Auto-selected hideout camp. Destroying the ammo stack ends the hideout.
	EntityID m_HQEntityId;
	bool m_bHideoutDestroyed;
	int m_iHideoutMaxGarrison;
	//! Camp, static weapons, and roadblock. Empty until the hideout is promoted or restored.
	ref array<ref JOLP_HideoutPiece> m_aHideoutPieces;
	//! 2 means the camp was put in a clearing and each gun has a path in. Older layouts are rebuilt once.
	int m_iHideoutLayoutVersion;

	//! Debug-only map marker (non-dedicated sessions).
	ref SCR_MapMarkerBase m_DebugMapMarker;

	//! -1 until the civilian manager applies the mission percent. 0 means this place has no villagers.
	int m_iCivilianBudget = -1;
	int m_iCivilianStored;
	int m_iVehicleSlots;
	//! -1 = use the size table. Copied from the location controller when one exists.
	int m_iCivilianFootOverride = -1;
	int m_iCivilianVehicleOverride = -1;

	//! This visit's draw in [0, 1], applied to the reputation ceiling. Negative until the town wakes.
	float m_fTerroristRoll = -1;
	float m_fPartisanRoll = -1;

	//! Local score. Towns, villages, cities, and hideouts use it. OPs stay at 0.
	float m_fReputation;
	//! Offset from a hideout used the first time a civilian describes it. Kept so repeat asks cannot be averaged.
	vector m_vTalkOffset;
	bool m_bTalkOffsetReady;
	//! The first IED batch for this place has been rolled.
	bool m_bIEDInitialized;
	//! Runtime only. Decoy rubbish is not saved, and a continue must be allowed to place it again.
	bool m_bDecoyTrashSpawned;
	//! Size of that first roll. An OP refills toward it while a garrison remains.
	int m_iIEDInitialCount;

	//------------------------------------------------------------------------------------------------
	int GetMaxActiveBudget()
	{
		if (m_iMaxActiveOverride >= 0)
			return m_iMaxActiveOverride;

		float ratio = m_fActiveGarrisonRatio;
		if (ratio <= 0)
			ratio = 0.65;

		return Math.Ceil(m_iGarrisonStored * ratio);
	}

	//------------------------------------------------------------------------------------------------
	bool IsHideout()
	{
		return m_eType == JOLP_LocationType.HIDEOUT;
	}

	//------------------------------------------------------------------------------------------------
	bool IsSettlement()
	{
		return m_eType == JOLP_LocationType.TOWN || m_eType == JOLP_LocationType.OP;
	}

	//------------------------------------------------------------------------------------------------
	bool IsTown()
	{
		return m_eType == JOLP_LocationType.TOWN;
	}

	//------------------------------------------------------------------------------------------------
	bool IsSuppressedByFOB()
	{
		return m_bSuppressedByFOB;
	}

	//------------------------------------------------------------------------------------------------
	//! Settlements that milician may still garrison / reinforce.
	bool IsMilicianEligible()
	{
		return IsSettlement() && !m_bSuppressedByFOB;
	}

	//------------------------------------------------------------------------------------------------
	bool IsOP()
	{
		return m_eType == JOLP_LocationType.OP;
	}

	//------------------------------------------------------------------------------------------------
	//! Villages, towns, cities, and hideouts. OPs have no score.
	bool HasReputation()
	{
		return IsTown() || IsHideout();
	}

	//------------------------------------------------------------------------------------------------
	//! Places that receive an IED pool: reputation places, plus OPs.
	bool CanHostIED()
	{
		return HasReputation() || IsOP();
	}

	//------------------------------------------------------------------------------------------------
	int GetVehicleStock(JOLP_VehicleClass kind)
	{
		if (kind == JOLP_VehicleClass.TRANSPORT)
			return m_iTransportStored;
		if (kind == JOLP_VehicleClass.LIGHT)
			return m_iLightStored;
		if (kind == JOLP_VehicleClass.HEAVY)
			return m_iHeavyStored;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	void SetVehicleStock(JOLP_VehicleClass kind, int count)
	{
		if (count < 0)
			count = 0;

		if (kind == JOLP_VehicleClass.TRANSPORT)
			m_iTransportStored = count;
		else if (kind == JOLP_VehicleClass.LIGHT)
			m_iLightStored = count;
		else if (kind == JOLP_VehicleClass.HEAVY)
			m_iHeavyStored = count;
	}

	//------------------------------------------------------------------------------------------------
	void AddVehicleStock(JOLP_VehicleClass kind, int count)
	{
		if (count == 0 || kind == JOLP_VehicleClass.NONE)
			return;

		SetVehicleStock(kind, GetVehicleStock(kind) + count);
	}

	//------------------------------------------------------------------------------------------------
	bool TakeVehicleStock(JOLP_VehicleClass kind)
	{
		if (GetVehicleStock(kind) <= 0)
			return false;

		SetVehicleStock(kind, GetVehicleStock(kind) - 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool HasVehicleStock()
	{
		return m_iTransportStored > 0 || m_iLightStored > 0 || m_iHeavyStored > 0;
	}

	//------------------------------------------------------------------------------------------------
	JOLP_HideoutPiece GetHideoutPiece(int index)
	{
		if (!m_aHideoutPieces || index < 0 || index >= m_aHideoutPieces.Count())
			return null;

		return m_aHideoutPieces[index];
	}

	//------------------------------------------------------------------------------------------------
	//! First intact nest that does not already have a gunner. -1 when none is free.
	int ClaimHideoutNest()
	{
		if (!m_aHideoutPieces)
			return -1;

		BaseWorld world = GetGame().GetWorld();
		for (int i = 0; i < m_aHideoutPieces.Count(); i++)
		{
			JOLP_HideoutPiece piece = m_aHideoutPieces[i];
			if (!piece || piece.m_eKind != JOLP_HideoutPieceKind.NEST)
				continue;

			if (piece.m_bDestroyed || piece.m_bGunnerAssigned)
				continue;

			if (!world || !world.FindEntityByID(piece.m_EntityId))
				continue;

			piece.m_bGunnerAssigned = true;
			return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	void ReleaseHideoutNest(int index)
	{
		JOLP_HideoutPiece piece = GetHideoutPiece(index);
		if (!piece)
			return;

		piece.m_bGunnerAssigned = false;
	}
}
