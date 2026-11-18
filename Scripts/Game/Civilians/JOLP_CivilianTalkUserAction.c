//------------------------------------------------------------------------------------------------
//! Opens the civilian dialogue on the player who used it. The server answers later.
class JOLP_CivilianTalkUserAction : ScriptedUserAction
{
	protected IEntity m_Owner;
	protected bool m_bLoggedShown;

	//------------------------------------------------------------------------------------------------
	//! GetOwner() can be NULL on streamed/proxy actions. Keep the entity Init gave us.
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);
		m_Owner = pOwnerEntity;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		bool talkable = IsTalkable(user);
		if (!m_bLoggedShown)
		{
			m_bLoggedShown = true;
			Print(string.Format("[JOLP] Speak evaluated on %1: talkable %2", GetSpeaker(), talkable), LogLevel.NORMAL);
		}

		return talkable;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return IsTalkable(user);
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
		if (pOwnerEntity && !m_Owner)
			m_Owner = pOwnerEntity;

		IEntity speaker = GetSpeaker();
		if (!speaker && pOwnerEntity)
		{
			if (ChimeraCharacter.Cast(pOwnerEntity))
				speaker = pOwnerEntity;
			else
				speaker = pOwnerEntity.GetParent();
		}

		if (!IsTalkable(pUserEntity) || !speaker)
			return;

		RplComponent replication = RplComponent.Cast(speaker.FindComponent(RplComponent));
		if (!replication)
			return;

		Print("[JOLP] Civilian talk opened", LogLevel.NORMAL);
		JOLP_CivilianTalkDialog.Open(replication.Id());
	}

	//------------------------------------------------------------------------------------------------
	//! The action sits on the villager, or on a small proxy parented to a map civilian.
	protected IEntity GetSpeaker()
	{
		IEntity owner = m_Owner;
		if (!owner)
			owner = GetOwner();

		if (!owner)
			return null;

		if (ChimeraCharacter.Cast(owner))
			return owner;

		return owner.GetParent();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsTalkable(IEntity user)
	{
		IEntity speakerEntity = GetSpeaker();
		if (!user || !speakerEntity || user == speakerEntity)
			return false;

		ChimeraCharacter userCharacter = ChimeraCharacter.Cast(user);
		SCR_ChimeraCharacter speaker = SCR_ChimeraCharacter.Cast(speakerEntity);
		if (!userCharacter || !speaker)
			return false;

		if (userCharacter.IsInVehicle() || speaker.IsInVehicle())
			return false;

		CharacterControllerComponent controller = speaker.GetCharacterController();
		if (!controller || controller.GetLifeState() == ECharacterLifeState.DEAD)
			return false;

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(speaker.GetDamageManager());
		if (damage && damage.IsDestroyed())
			return false;

		return true;
	}
}
