modded class SCR_AIConfigComponent
{
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		PackGoalReactions();
	}

	//------------------------------------------------------------------------------------------------
	//! The stock init skips this when the AI settings entity is not ready, and move orders then crash.
	protected void PackGoalReactions()
	{
		if (!m_aGoalReactions || !m_aGoalReactionsPacked)
			return;

		int needed = m_aGoalReactionsPacked.Count();
		foreach (SCR_AIGoalReaction reaction : m_aGoalReactions)
		{
			if (!reaction)
				continue;

			int typeId = reaction.m_eType;
			if (typeId + 1 > needed)
				needed = typeId + 1;
		}

		if (m_aGoalReactionsPacked.Count() < needed)
			m_aGoalReactionsPacked.Resize(needed);

		foreach (SCR_AIGoalReaction reaction : m_aGoalReactions)
		{
			if (!reaction || reaction.m_eType == EMessageType_Goal.NONE)
				continue;

			if (reaction.m_eType < 0 || reaction.m_eType >= m_aGoalReactionsPacked.Count())
				continue;

			m_aGoalReactionsPacked[reaction.m_eType] = reaction;
		}
	}

	//------------------------------------------------------------------------------------------------
	override void PerformGoalReaction(SCR_AIGroupUtilityComponent utility, SCR_AIMessageBase message)
	{
		if (!CanPerformGoal(message))
			return;

		super.PerformGoalReaction(utility, message);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformGoalReaction(SCR_AIUtilityComponent utility, SCR_AIMessageBase message)
	{
		if (!CanPerformGoal(message))
			return;

		super.PerformGoalReaction(utility, message);
	}

	//------------------------------------------------------------------------------------------------
	protected bool CanPerformGoal(SCR_AIMessageBase message)
	{
		SCR_AIMessageGoal goalMessage = SCR_AIMessageGoal.Cast(message);
		if (!goalMessage || !m_aGoalReactionsPacked)
			return false;

		return goalMessage.m_MessageType >= 0 && goalMessage.m_MessageType < m_aGoalReactionsPacked.Count();
	}
}
