//------------------------------------------------------------------------------------------------
//! The fraction of an in-game hour that has not yet paid out.
//! Stored in hours whatever period the prefab uses (second, minute, hour).
//! Restoring it and snapping the clock stops a restart from paying that time twice
//! or from throwing the fraction away.
//------------------------------------------------------------------------------------------------
class JOLP_SupplyIncomeSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return JOLP_SupplyIncomeComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		JOLP_SupplyIncomeComponent income = JOLP_SupplyIncomeComponent.Cast(component);
		if (!income)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		float unpaid = income.GetUnpaidGameHours();
		context.Write(unpaid);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		JOLP_SupplyIncomeComponent income = JOLP_SupplyIncomeComponent.Cast(component);
		if (!income)
			return false;

		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		float unpaid;
		context.Read(unpaid);
		income.ApplyUnpaidGameHours(unpaid);
		return true;
	}
}
