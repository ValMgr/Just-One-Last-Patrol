//------------------------------------------------------------------------------------------------
//! Player-built composition identity and the SUPPLIES sitting in it.
//! The placed Thunderbolt HQ must not use this component: its config SelfSpawns, and a
//! world-placed entity that also self-spawns comes back twice.
//! Field order is the binary format.
//------------------------------------------------------------------------------------------------
class JOLP_PersistentCompositionSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return JOLP_PersistentCompositionComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		JOLP_PersistentCompositionComponent composition = JOLP_PersistentCompositionComponent.Cast(component);
		if (!composition)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		string fobName = composition.GetFOBName();
		context.Write(fobName);

		float supplies = JOLP_SupplyStock.Read(owner);
		context.Write(supplies);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		JOLP_PersistentCompositionComponent composition = JOLP_PersistentCompositionComponent.Cast(component);
		if (!composition)
			return false;

		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		string fobName;
		context.Read(fobName);

		float supplies;
		context.Read(supplies);

		composition.ApplyPersisted(fobName, supplies);
		return true;
	}
}
