//------------------------------------------------------------------------------------------------
//! Adds a Just One Last Patrol page to the controls menu.
modded class SCR_KeybindSetting
{
	protected static const string JOLP_CATEGORY_NAME = "JustOneLastPatrol";

	//------------------------------------------------------------------------------------------------
	override void OnTabCreate(Widget menuRoot, ResourceName buttonsLayout, int index)
	{
		super.OnTabCreate(menuRoot, buttonsLayout, index);

		if (!m_KeybindConfig || !m_KeybindConfig.m_KeyBindingCategories)
			return;

		foreach (SCR_KeyBindingCategory existing : m_KeybindConfig.m_KeyBindingCategories)
		{
			if (existing && existing.m_sName == JOLP_CATEGORY_NAME)
				return;
		}

		SCR_KeyBindingCategory category = new SCR_KeyBindingCategory();
		category.m_sName = JOLP_CATEGORY_NAME;
		category.m_sDisplayName = "Just One Last Patrol";
		category.m_KeyBindingEntries = {};

		SCR_KeyBindingEntry header = new SCR_KeyBindingEntry();
		header.m_sActionName = "separator";
		header.m_sDisplayName = "Interaction";
		header.m_aPlatforms = {};
		category.m_KeyBindingEntries.Insert(header);

		SCR_KeyBindingEntry radial = new SCR_KeyBindingEntry();
		radial.m_sActionName = "JOLP_SelfRadial";
		radial.m_sDisplayName = "Self interaction";
		radial.m_sPreset = "JOLP_SelfRadial";
		radial.m_aPlatforms = {};
		category.m_KeyBindingEntries.Insert(radial);

		m_KeybindConfig.m_KeyBindingCategories.Insert(category);
		InsertCategoriesToComboBox();
	}
}
