class JOLP_LoadoutNetworkComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Client asks, server saves, prices, and equips. Loadouts never travel to other players.
class JOLP_LoadoutNetworkComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	static JOLP_LoadoutNetworkComponent Local()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return null;

		return JOLP_LoadoutNetworkComponent.Cast(controller.FindComponent(JOLP_LoadoutNetworkComponent));
	}

	//------------------------------------------------------------------------------------------------
	static RplId FindArsenalId(IEntity owner)
	{
		IEntity cursor = owner;
		while (cursor)
		{
			RplComponent replication = RplComponent.Cast(cursor.FindComponent(RplComponent));
			if (replication)
				return replication.Id();

			cursor = cursor.GetParent();
		}

		return RplId.Invalid();
	}

	//------------------------------------------------------------------------------------------------
	void AskSave(RplId arsenalId, string name)
	{
		Rpc(RpcAsk_SaveLoadout, arsenalId, name);
	}

	//------------------------------------------------------------------------------------------------
	void AskList(RplId arsenalId)
	{
		Rpc(RpcAsk_ListLoadouts, arsenalId);
	}

	//------------------------------------------------------------------------------------------------
	void AskDetail(RplId arsenalId, string name)
	{
		Rpc(RpcAsk_LoadoutDetail, arsenalId, name);
	}

	//------------------------------------------------------------------------------------------------
	void AskDelete(RplId arsenalId, string name)
	{
		Rpc(RpcAsk_DeleteLoadout, arsenalId, name);
	}

	//------------------------------------------------------------------------------------------------
	void AskApply(RplId arsenalId, string name)
	{
		Rpc(RpcAsk_ApplyLoadout, arsenalId, name);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SaveLoadout(RplId arsenalId, string name)
	{
		IEntity player;
		IEntity arsenal;
		string rejection = Gate(arsenalId, player, arsenal);
		if (!rejection.IsEmpty())
		{
			Rpc(RpcDo_SaveResult, false, rejection);
			return;
		}

		JOLP_LoadoutManagerComponent manager = JOLP_LoadoutManagerComponent.GetInstance();
		if (!manager)
		{
			Rpc(RpcDo_SaveResult, false, "Loadouts are not available.");
			return;
		}

		PlayerController controller = PlayerController.Cast(GetOwner());
		string error;
		bool saved = manager.SaveLoadout(controller.GetPlayerId(), name, player, error);
		if (!saved && error.IsEmpty())
			error = "The loadout could not be saved.";

		Rpc(RpcDo_SaveResult, saved, error);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SaveResult(bool saved, string message)
	{
		JOLP_SaveLoadoutDialog.Deliver(saved, message);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ListLoadouts(RplId arsenalId)
	{
		IEntity player;
		IEntity arsenal;
		string rejection = Gate(arsenalId, player, arsenal);
		if (!rejection.IsEmpty())
		{
			Rpc(RpcDo_ListLoadouts, false, rejection, string.Empty);
			return;
		}

		JOLP_LoadoutManagerComponent manager = JOLP_LoadoutManagerComponent.GetInstance();
		if (!manager)
		{
			Rpc(RpcDo_ListLoadouts, false, "Loadouts are not available.", string.Empty);
			return;
		}

		PlayerController controller = PlayerController.Cast(GetOwner());
		array<string> names = {};
		manager.GetNames(controller.GetPlayerId(), names);

		string packed = "";
		foreach (string name : names)
		{
			if (!packed.IsEmpty())
				packed = packed + "\n";

			packed = packed + name;
		}

		Rpc(RpcDo_ListLoadouts, true, string.Empty, packed);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_ListLoadouts(bool ok, string message, string names)
	{
		JOLP_LoadoutsDialog.DeliverList(ok, message, names);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LoadoutDetail(RplId arsenalId, string name)
	{
		IEntity player;
		IEntity arsenal;
		string rejection = Gate(arsenalId, player, arsenal);
		if (!rejection.IsEmpty())
		{
			Rpc(RpcDo_LoadoutDetail, false, rejection, string.Empty);
			return;
		}

		JOLP_LoadoutManagerComponent manager = JOLP_LoadoutManagerComponent.GetInstance();
		if (!manager)
		{
			Rpc(RpcDo_LoadoutDetail, false, "Loadouts are not available.", string.Empty);
			return;
		}

		PlayerController controller = PlayerController.Cast(GetOwner());
		string text;
		string error;
		bool described = manager.Describe(controller.GetPlayerId(), name, player, arsenal, text, error);
		if (!described && error.IsEmpty())
			error = "That loadout could not be priced.";

		Rpc(RpcDo_LoadoutDetail, described, error, text);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LoadoutDetail(bool ok, string message, string detail)
	{
		JOLP_LoadoutsDialog.DeliverDetail(ok, message, detail);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DeleteLoadout(RplId arsenalId, string name)
	{
		IEntity player;
		IEntity arsenal;
		string rejection = Gate(arsenalId, player, arsenal);
		if (!rejection.IsEmpty())
		{
			Rpc(RpcDo_DeleteResult, false, rejection);
			return;
		}

		JOLP_LoadoutManagerComponent manager = JOLP_LoadoutManagerComponent.GetInstance();
		if (!manager)
		{
			Rpc(RpcDo_DeleteResult, false, "Loadouts are not available.");
			return;
		}

		PlayerController controller = PlayerController.Cast(GetOwner());
		string error;
		bool deleted = manager.DeleteLoadout(controller.GetPlayerId(), name, error);
		Rpc(RpcDo_DeleteResult, deleted, error);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_DeleteResult(bool deleted, string message)
	{
		JOLP_LoadoutsDialog.DeliverDelete(deleted, message);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ApplyLoadout(RplId arsenalId, string name)
	{
		IEntity player;
		IEntity arsenal;
		string rejection = Gate(arsenalId, player, arsenal);
		if (!rejection.IsEmpty())
		{
			Rpc(RpcDo_ApplyResult, false, rejection);
			return;
		}

		JOLP_LoadoutManagerComponent manager = JOLP_LoadoutManagerComponent.GetInstance();
		if (!manager)
		{
			Rpc(RpcDo_ApplyResult, false, "Loadouts are not available.");
			return;
		}

		PlayerController controller = PlayerController.Cast(GetOwner());
		string error;
		bool applied = manager.ApplyLoadout(controller.GetPlayerId(), name, player, arsenal, error);
		Rpc(RpcDo_ApplyResult, applied, error);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_ApplyResult(bool applied, string message)
	{
		JOLP_LoadoutsDialog.DeliverApply(applied, message);
	}

	//------------------------------------------------------------------------------------------------
	protected string Gate(RplId arsenalId, out IEntity player, out IEntity arsenal)
	{
		player = null;
		arsenal = null;

		PlayerController controller = PlayerController.Cast(GetOwner());
		if (!controller)
			return "That arsenal cannot be used.";

		player = GetGame().GetPlayerManager().GetPlayerControlledEntity(controller.GetPlayerId());
		RplComponent replication = RplComponent.Cast(Replication.FindItem(arsenalId));
		if (replication)
			arsenal = replication.GetEntity();

		if (!player || !arsenal)
			return "That arsenal cannot be used.";

		if (!JOLP_LoadoutManagerComponent.ArsenalOf(arsenal))
			return "That is not an arsenal.";

		ChimeraCharacter character = ChimeraCharacter.Cast(player);
		if (!character || character.IsInVehicle())
			return "Step outside first.";

		if (vector.Distance(player.GetOrigin(), arsenal.GetOrigin()) > JOLP_LoadoutManagerComponent.MAX_DISTANCE_M)
			return "You are too far from the arsenal.";

		return string.Empty;
	}
}
