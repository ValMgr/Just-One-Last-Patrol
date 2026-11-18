//------------------------------------------------------------------------------------------------
//! Every named kit, keyed by player identity inside the record.
//! Version first. A missing version means this component had nothing to restore.
class JOLP_LoadoutManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return JOLP_LoadoutManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		JOLP_LoadoutManagerComponent manager = JOLP_LoadoutManagerComponent.Cast(component);
		if (!manager)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		array<ref JOLP_PersistedLoadout> records = new array<ref JOLP_PersistedLoadout>();
		manager.CollectPersisted(records);
		context.Write(records);
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		JOLP_LoadoutManagerComponent manager = JOLP_LoadoutManagerComponent.Cast(component);
		if (!manager)
			return false;

		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		array<ref JOLP_PersistedLoadout> records = new array<ref JOLP_PersistedLoadout>();
		context.Read(records);
		manager.ReplacePersisted(records);
		return true;
	}
}
