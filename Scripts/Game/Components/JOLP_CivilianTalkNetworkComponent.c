class JOLP_CivilianTalkNetworkComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Client asks, server answers. Lives on the player controller.
class JOLP_CivilianTalkNetworkComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (SCR_Global.IsEditMode())
			return;

		SetEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		PlayerController local = GetGame().GetPlayerController();
		if (!local || local != PlayerController.Cast(owner))
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		if (GetGame().GetMenuManager() && GetGame().GetMenuManager().GetTopMenu())
			return;

		input.ActivateContext("JOLP_GameplayContext", 250);
	}

	//------------------------------------------------------------------------------------------------
	void RequestCallCivilians()
	{
		Rpc(RpcAsk_CallCivilians);
	}

	//------------------------------------------------------------------------------------------------
	static JOLP_CivilianTalkNetworkComponent Local()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return null;

		return JOLP_CivilianTalkNetworkComponent.Cast(controller.FindComponent(JOLP_CivilianTalkNetworkComponent));
	}

	//------------------------------------------------------------------------------------------------
	void Ask(int choice, RplId speakerId)
	{
		Rpc(RpcAsk_CivilianTalk, choice, speakerId);
	}

	//------------------------------------------------------------------------------------------------
	void Hold(RplId speakerId)
	{
		Rpc(RpcAsk_CivilianTalkHold, speakerId, true);
	}

	//------------------------------------------------------------------------------------------------
	void Release(RplId speakerId)
	{
		Rpc(RpcAsk_CivilianTalkHold, speakerId, false);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_CivilianTalk(int choice, RplId speakerId)
	{
		PlayerController controller = PlayerController.Cast(GetOwner());
		if (!controller)
			return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(controller.GetPlayerId());
		IEntity speaker = null;
		RplComponent speakerReplication = RplComponent.Cast(Replication.FindItem(speakerId));
		if (speakerReplication)
			speaker = speakerReplication.GetEntity();

		string reply = JOLP_CivilianTalk.Answer(choice, speaker, player);
		Rpc(RpcDo_CivilianTalkReply, reply);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_CallCivilians()
	{
		PlayerController controller = PlayerController.Cast(GetOwner());
		if (!controller)
			return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(controller.GetPlayerId());
		JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
		if (!player || !civilians)
			return;

		civilians.CallToPlayer(player);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_CivilianTalkHold(RplId speakerId, bool hold)
	{
		PlayerController controller = PlayerController.Cast(GetOwner());
		if (!controller)
			return;

		JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
		if (!civilians)
			return;

		IEntity speaker = null;
		RplComponent speakerReplication = RplComponent.Cast(Replication.FindItem(speakerId));
		if (speakerReplication)
			speaker = speakerReplication.GetEntity();

		if (hold)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(controller.GetPlayerId());
			civilians.HoldForTalk(speaker, player);
			return;
		}

		civilians.ReleaseTalk(speaker, controller.GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_CivilianTalkReply(string text)
	{
		JOLP_CivilianTalkDialog.Deliver(text);
	}
}
