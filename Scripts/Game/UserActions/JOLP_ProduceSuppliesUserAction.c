//------------------------------------------------------------------------------------------------
//! Hold interaction on Supply Production. Each finished cycle adds supplies to the stock
//! the building's passive income already fills.
class JOLP_ProduceSuppliesUserAction : SCR_ScriptedUserAction
{
	[Attribute("10", UIWidgets.EditBox, "Supplies granted each time the hold completes")]
	protected int m_iSuppliesPerCompletion;

	protected IEntity m_Owner;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);
		m_Owner = pOwnerEntity;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!super.CanBeShownScript(user))
			return false;

		if (!ChimeraCharacter.Cast(user))
			return false;

		return GetIncome() != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return GetIncome() != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Per-frame hold. PerformAction runs only when one cycle of the bar finishes.
	override void PerformContinuousAction(IEntity pOwnerEntity, IEntity pUserEntity, float timeSlice)
	{
		if (!LoopActionUpdate(timeSlice))
			return;

		PerformAction(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! The bar runs on the player. GrantSupplies asks the server to change the stock.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (pOwnerEntity && !m_Owner)
			m_Owner = pOwnerEntity;

		if (m_iSuppliesPerCompletion <= 0)
			return;

		JOLP_SupplyIncomeComponent income = GetIncome();
		if (!income)
			return;

		income.GrantSupplies(m_iSuppliesPerCompletion);
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_SupplyIncomeComponent GetIncome()
	{
		IEntity owner = m_Owner;
		if (!owner)
			owner = GetOwner();

		if (!owner)
			return null;

		return JOLP_SupplyIncomeComponent.Cast(owner.FindComponent(JOLP_SupplyIncomeComponent));
	}
}
