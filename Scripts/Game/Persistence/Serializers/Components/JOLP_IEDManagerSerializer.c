//------------------------------------------------------------------------------------------------
//! One planted charge. The entity is respawned from this record. EntityID is not saved.
//! Field order is the binary format.
class JOLP_PersistedIED : Managed
{
	string m_sLocationKey;
	vector m_vPosition;
	vector m_vAngles;
	string m_sPrefab;
	bool m_bDefused;
}

//------------------------------------------------------------------------------------------------
//! Charges are not tracked as entities. Tracking them and also respawning from this list
//! would plant every charge twice. Camouflage trash is not saved.
//------------------------------------------------------------------------------------------------
class JOLP_IEDManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return JOLP_IEDManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		JOLP_IEDManagerComponent ieds = JOLP_IEDManagerComponent.Cast(component);
		if (!ieds)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		int lastDailyKey = ieds.GetLastDailyKey();
		context.Write(lastDailyKey);

		array<ref JOLP_PersistedIED> records = new array<ref JOLP_PersistedIED>();
		array<ref JOLP_IEDRecord> live = ieds.GetCharges();
		if (live)
		{
			foreach (JOLP_IEDRecord charge : live)
			{
				if (!charge || charge.m_bDetonated || charge.m_sPrefab.IsEmpty())
					continue;

				JOLP_PersistedIED record = new JOLP_PersistedIED();
				record.m_sLocationKey = charge.m_sLocationKey;
				record.m_vPosition = charge.m_vPosition;
				record.m_vAngles = charge.m_vAngles;
				record.m_sPrefab = charge.m_sPrefab;
				record.m_bDefused = charge.m_bDefused;
				records.Insert(record);
			}
		}

		context.Write(records);
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		JOLP_IEDManagerComponent ieds = JOLP_IEDManagerComponent.Cast(component);
		if (!ieds)
			return false;

		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		int lastDailyKey;
		context.Read(lastDailyKey);

		array<ref JOLP_PersistedIED> records = new array<ref JOLP_PersistedIED>();
		context.Read(records);
		ieds.ApplyPersistedCharges(records, lastDailyKey);
		return true;
	}
}
