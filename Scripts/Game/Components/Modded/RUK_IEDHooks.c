modded class SCR_BaseTriggerComponent
{
	//------------------------------------------------------------------------------------------------
	//! RUK plays the bang from this call, then spawns an impact-fuzed shell that often never
	//! touches anything. The kill blast is spawned from here, on the server, once per charge.
	override void RPC_DoTrigger()
	{
		super.RPC_DoTrigger();

		if (!Replication.IsServer())
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		IEntity rukEntity;
		IEntity cursor = owner;
		int guard = 0;
		while (cursor && guard < 6)
		{
			if (cursor.FindComponent(RUK_IEDManagerComponent))
			{
				rukEntity = cursor;
				break;
			}

			cursor = cursor.GetParent();
			guard++;
		}

		if (!rukEntity)
			return;

		JOLP_IEDManagerComponent ieds = JOLP_IEDManagerComponent.GetInstance();
		if (ieds)
			ieds.NotifyChargeDetonated(rukEntity);
	}
}

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
