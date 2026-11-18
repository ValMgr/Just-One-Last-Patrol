//------------------------------------------------------------------------------------------------
//! Tell civilians within 10 m to walk over. Same call as Ctrl+T.
class JOLP_CallCiviliansRadialAction : JOLP_SelfRadialAction
{
	protected static const float RANGE = 10;
	protected static const ResourceName ICON_SET = "{A4A692135635424E}UI/Imagesets/Commanding/Commanding_Icons.imageset";

	protected static vector s_vOrigin;
	protected static bool s_bFound;

	//------------------------------------------------------------------------------------------------
	override string GetId()
	{
		return "call_civilians";
	}

	//------------------------------------------------------------------------------------------------
	override string GetLabel()
	{
		return "Come closer";
	}

	//------------------------------------------------------------------------------------------------
	override string GetDescription()
	{
		return "Call civilians within 10 m";
	}

	//------------------------------------------------------------------------------------------------
	override void GetIcon(out ResourceName imageset, out string image)
	{
		imageset = ICON_SET;
		image = "follow";
	}

	//------------------------------------------------------------------------------------------------
	override bool CanShow(IEntity user)
	{
		return user != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanPerform(IEntity user)
	{
		return HasCivilianInRange(user);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(IEntity user)
	{
		JOLP_CivilianTalkNetworkComponent network = JOLP_CivilianTalkNetworkComponent.Local();
		if (!network)
			return;

		network.RequestCallCivilians();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasCivilianInRange(IEntity user)
	{
		if (!user)
			return false;

		BaseWorld world = user.GetWorld();
		if (!world)
			return false;

		s_vOrigin = user.GetOrigin();
		s_bFound = false;
		world.QueryEntitiesBySphere(s_vOrigin, RANGE, FindCivilian, null, EQueryEntitiesFlags.DYNAMIC);
		return s_bFound;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool FindCivilian(IEntity entity)
	{
		if (s_bFound || !entity)
			return true;

		if (JOLP_WorldUtils.DistanceFlat(entity.GetOrigin(), s_vOrigin) > RANGE)
			return true;

		SCR_ChimeraCharacter speaker = SCR_ChimeraCharacter.Cast(entity);
		if (!speaker || speaker.IsInVehicle())
			return true;

		PlayerManager players = GetGame().GetPlayerManager();
		if (players && players.GetPlayerIdFromControlledEntity(entity) > 0)
			return true;

		CharacterControllerComponent controller = speaker.GetCharacterController();
		if (!controller || controller.GetLifeState() == ECharacterLifeState.DEAD)
			return true;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return true;

		Faction faction = affiliation.GetAffiliatedFaction();
		if (!faction || faction.GetFactionKey() != "CIV")
			return true;

		s_bFound = true;
		return true;
	}
}
