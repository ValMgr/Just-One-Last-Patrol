modded class SCR_EditableCharacterComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		JOLP_CivilianTalkAttach.Queue(owner);
	}
}
