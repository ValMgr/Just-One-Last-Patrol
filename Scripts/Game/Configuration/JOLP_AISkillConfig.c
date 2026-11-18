//------------------------------------------------------------------------------------------------
//! Combat aim for spawned milician and partisans. Neutrals keep the character prefab.
[BaseContainerProps(configRoot: true)]
class JOLP_AISkillConfig : Managed
{
	[Attribute("50", UIWidgets.ComboBox, "Combat aim of every spawned milician", "", ParamEnumArray.FromEnum(EAISkill))]
	EAISkill m_eMilicianSkill;

	[Attribute("10", UIWidgets.ComboBox, "Combat aim of partisans. Neutrals and bombers keep the character prefab", "", ParamEnumArray.FromEnum(EAISkill))]
	EAISkill m_ePartisanSkill;

	//------------------------------------------------------------------------------------------------
	static void Apply(IEntity character, EAISkill skill)
	{
		if (!character)
			return;

		SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(character.FindComponent(SCR_AICombatComponent));
		if (!combat)
			return;

		combat.SetAISkill(skill);
	}
}
