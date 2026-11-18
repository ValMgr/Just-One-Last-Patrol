modded class RUK_IEDManagerComponent
{
	//------------------------------------------------------------------------------------------------
	//! Planted charges demand wirecutters in hand. The stock prefabs leave this off.
	void JOLP_RequireWirecutters()
	{
		m_RequireWirecutters = true;
	}
}

modded class RUK_CutInertWireUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		JOLP_IEDManagerComponent ieds = JOLP_IEDManagerComponent.GetInstance();
		if (ieds)
			ieds.NotifyDefused(pOwnerEntity, pUserEntity);

		super.PerformAction(pOwnerEntity, pUserEntity);
	}
}
