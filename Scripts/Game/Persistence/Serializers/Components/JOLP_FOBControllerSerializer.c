//------------------------------------------------------------------------------------------------
//! Placed FOB marker. SelfSpawn stays off so Thunderbolt is not created a second time.
//! Player-built HQs carry JOLP_PersistentCompositionComponent instead, which outranks this rule.
//------------------------------------------------------------------------------------------------
class JOLP_FOBControllerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return JOLP_FOBControllerComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		JOLP_FOBControllerComponent fob = JOLP_FOBControllerComponent.Cast(component);
		if (!fob)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		string name = fob.GetFOBName();
		context.Write(name);

		float supplies = JOLP_SupplyStock.Read(owner);
		context.Write(supplies);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		JOLP_FOBControllerComponent fob = JOLP_FOBControllerComponent.Cast(component);
		if (!fob)
			return false;

		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		string name;
		context.Read(name);
		if (!name.IsEmpty())
			fob.SetFOBName(name);

		float supplies;
		context.Read(supplies);
		JOLP_SupplyStock.Write(owner, supplies);
		return true;
	}
}
