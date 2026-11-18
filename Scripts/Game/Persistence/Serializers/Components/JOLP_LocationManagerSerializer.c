//------------------------------------------------------------------------------------------------
//! One location's campaign state. Matched by m_sPersistentKey, never by session id.
//! Field order is the binary format. Append new fields. Do not reorder these.
class JOLP_PersistedLocation : Managed
{
	string m_sPersistentKey;
	vector m_vPosition;
	JOLP_LocationType m_eType;
	JOLP_SettlementSize m_eSettlementSize;
	int m_iGarrisonTotal;
	float m_fActiveGarrisonRatio;
	int m_iMaxActiveOverride;
	bool m_bAllowSentry;
	bool m_bAllowPatrol;
	bool m_bAllowCheckpoint;
	bool m_bAllowReinforcement;
	bool m_bHideoutDestroyed;
	int m_iHideoutMaxGarrison;
	float m_fReputation;
	int m_iCivilianBudget;
	int m_iCivilianStoredTotal;
	int m_iVehicleSlots;
	bool m_bIEDInitialized;
	int m_iIEDInitialCount;
}

//------------------------------------------------------------------------------------------------
//! Version 2 appends the three vehicle pools. Version 1 saves stay on JOLP_PersistedLocation.
class JOLP_PersistedLocationV2 : Managed
{
	string m_sPersistentKey;
	vector m_vPosition;
	JOLP_LocationType m_eType;
	JOLP_SettlementSize m_eSettlementSize;
	int m_iGarrisonTotal;
	float m_fActiveGarrisonRatio;
	int m_iMaxActiveOverride;
	bool m_bAllowSentry;
	bool m_bAllowPatrol;
	bool m_bAllowCheckpoint;
	bool m_bAllowReinforcement;
	bool m_bHideoutDestroyed;
	int m_iHideoutMaxGarrison;
	float m_fReputation;
	int m_iCivilianBudget;
	int m_iCivilianStoredTotal;
	int m_iVehicleSlots;
	bool m_bIEDInitialized;
	int m_iIEDInitialCount;
	int m_iTransportTotal;
	int m_iLightTotal;
	int m_iHeavyTotal;
}

//------------------------------------------------------------------------------------------------
//! One hideout prop. Entity ids are not saved. The location manager spawns this again.
class JOLP_PersistedHideoutPiece : Managed
{
	JOLP_HideoutPieceKind m_eKind;
	string m_sPrefab;
	vector m_vPosition;
	vector m_vAngles;
	bool m_bDestroyed;
}

//------------------------------------------------------------------------------------------------
//! Version 3 appends the hideout camp. Version 2 saves stay on JOLP_PersistedLocationV2.
class JOLP_PersistedLocationV3 : Managed
{
	string m_sPersistentKey;
	vector m_vPosition;
	JOLP_LocationType m_eType;
	JOLP_SettlementSize m_eSettlementSize;
	int m_iGarrisonTotal;
	float m_fActiveGarrisonRatio;
	int m_iMaxActiveOverride;
	bool m_bAllowSentry;
	bool m_bAllowPatrol;
	bool m_bAllowCheckpoint;
	bool m_bAllowReinforcement;
	bool m_bHideoutDestroyed;
	int m_iHideoutMaxGarrison;
	float m_fReputation;
	int m_iCivilianBudget;
	int m_iCivilianStoredTotal;
	int m_iVehicleSlots;
	bool m_bIEDInitialized;
	int m_iIEDInitialCount;
	int m_iTransportTotal;
	int m_iLightTotal;
	int m_iHeavyTotal;
	ref array<ref JOLP_PersistedHideoutPiece> m_aHideoutPieces;
}

//------------------------------------------------------------------------------------------------
//! Towns, OPs, and hideouts are placed in the world. The save restores what happened to them.
//! Garrison and foot-civilian totals include people who are currently spawned. On load those
//! people are not in the world yet, so the whole total goes back into the stored pool.
//! A reapply onto a live session subtracts whoever is still spawned, so they are not counted twice.
//------------------------------------------------------------------------------------------------
class JOLP_LocationManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return JOLP_LocationManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		JOLP_LocationManagerComponent locations = JOLP_LocationManagerComponent.Cast(component);
		if (!locations)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 4);

		array<ref JOLP_PersistedLocationV3> records = new array<ref JOLP_PersistedLocationV3>();
		array<int> layoutVersions = new array<int>();
		array<ref JOLP_LocationData> live = locations.GetAllLocations();
		if (live)
		{
			foreach (JOLP_LocationData data : live)
			{
				if (!data || data.m_sPersistentKey.IsEmpty())
					continue;

				JOLP_PersistedLocationV3 record = new JOLP_PersistedLocationV3();
				record.m_sPersistentKey = data.m_sPersistentKey;
				record.m_vPosition = data.m_vPosition;
				record.m_eType = data.m_eType;
				record.m_eSettlementSize = data.m_eSettlementSize;
				record.m_iGarrisonTotal = data.m_iGarrisonStored + locations.GetFieldedGarrison(data.m_iId);
				record.m_fActiveGarrisonRatio = data.m_fActiveGarrisonRatio;
				record.m_iMaxActiveOverride = data.m_iMaxActiveOverride;
				record.m_bAllowSentry = data.m_bAllowSentry;
				record.m_bAllowPatrol = data.m_bAllowPatrol;
				record.m_bAllowCheckpoint = data.m_bAllowCheckpoint;
				record.m_bAllowReinforcement = data.m_bAllowReinforcement;
				record.m_bHideoutDestroyed = data.m_bHideoutDestroyed;
				record.m_iHideoutMaxGarrison = data.m_iHideoutMaxGarrison;
				record.m_fReputation = data.m_fReputation;
				record.m_iCivilianBudget = data.m_iCivilianBudget;
				record.m_iCivilianStoredTotal = data.m_iCivilianStored + locations.GetFieldedFootCivilians(data.m_iId);
				record.m_iVehicleSlots = data.m_iVehicleSlots;
				record.m_bIEDInitialized = data.m_bIEDInitialized;
				record.m_iIEDInitialCount = data.m_iIEDInitialCount;
				record.m_iTransportTotal = data.m_iTransportStored + locations.GetFieldedVehicles(data.m_iId, JOLP_VehicleClass.TRANSPORT);
				record.m_iLightTotal = data.m_iLightStored + locations.GetFieldedVehicles(data.m_iId, JOLP_VehicleClass.LIGHT);
				record.m_iHeavyTotal = data.m_iHeavyStored + locations.GetFieldedVehicles(data.m_iId, JOLP_VehicleClass.HEAVY);
				record.m_aHideoutPieces = new array<ref JOLP_PersistedHideoutPiece>();
				if (data.m_aHideoutPieces)
				{
					foreach (JOLP_HideoutPiece piece : data.m_aHideoutPieces)
					{
						if (!piece)
							continue;

						JOLP_PersistedHideoutPiece savedPiece = new JOLP_PersistedHideoutPiece();
						savedPiece.m_eKind = piece.m_eKind;
						savedPiece.m_sPrefab = piece.m_sPrefab;
						savedPiece.m_vPosition = piece.m_vPosition;
						savedPiece.m_vAngles = piece.m_vAngles;
						savedPiece.m_bDestroyed = piece.m_bDestroyed;
						record.m_aHideoutPieces.Insert(savedPiece);
					}
				}

				records.Insert(record);
				layoutVersions.Insert(data.m_iHideoutLayoutVersion);
			}
		}

		context.Write(records);
		context.WriteValue("layoutCount", layoutVersions.Count());
		foreach (int layoutVersion : layoutVersions)
			context.WriteValue("layoutVersion", layoutVersion);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		JOLP_LocationManagerComponent locations = JOLP_LocationManagerComponent.Cast(component);
		if (!locations)
			return false;

		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		if (version < 2)
		{
			array<ref JOLP_PersistedLocation> legacy = new array<ref JOLP_PersistedLocation>();
			context.Read(legacy);
			locations.ApplyPersistedLocations(legacy);
			return true;
		}

		if (version < 3)
		{
			array<ref JOLP_PersistedLocationV2> records = new array<ref JOLP_PersistedLocationV2>();
			context.Read(records);
			locations.ApplyPersistedLocationsV2(records);
			return true;
		}

		array<ref JOLP_PersistedLocationV3> recordsV3 = new array<ref JOLP_PersistedLocationV3>();
		context.Read(recordsV3);

		// Version 3 has the pieces, but not which placement rules produced them.
		array<int> layoutVersions = null;
		if (version >= 4)
		{
			int layoutCount;
			context.ReadValue("layoutCount", layoutCount);
			layoutVersions = new array<int>();
			for (int i = 0; i < layoutCount; i++)
			{
				int layoutVersion;
				context.ReadValue("layoutVersion", layoutVersion);
				layoutVersions.Insert(layoutVersion);
			}
		}

		locations.ApplyPersistedLocationsV3(recordsV3, layoutVersions);
		return true;
	}
}
