//------------------------------------------------------------------------------------------------
//! Opens the list of this player's saved kits.
class JOLP_OpenLoadoutsUserAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return CanUse(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return CanUse(user);
	}

	//------------------------------------------------------------------------------------------------
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
		if (!CanUse(pUserEntity))
			return;

		RplId arsenalId = JOLP_LoadoutNetworkComponent.FindArsenalId(pOwnerEntity);
		if (arsenalId == RplId.Invalid())
			return;

		JOLP_LoadoutsDialog.Open(arsenalId);
	}

	//------------------------------------------------------------------------------------------------
	protected bool CanUse(IEntity user)
	{
		if (!GetOwner() || !JOLP_LoadoutManagerComponent.ArsenalOf(GetOwner()))
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character || character.IsInVehicle())
			return false;

		return true;
	}
}
