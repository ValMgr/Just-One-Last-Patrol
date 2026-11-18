//------------------------------------------------------------------------------------------------
//! Persists whether the campaign had already started.
//!
//! Bound by overriding the shared game-mode entity config in
//! Configs/Systems/Persistence/JustOneLastPatrol.conf, reusing vanilla's serializer GUID.
//! The vanilla payload stays under "base" so disabling a resource type is not dropped.
//!
//! A continued session must run DoStartGame and must not roll hideouts, reputation, or IEDs again.
//! Deserialize also runs when save data is reapplied onto a live session. RestoreStartedCampaign
//! returns immediately once the campaign is already running.
//------------------------------------------------------------------------------------------------
class JOLP_JustOneLastPatrolGameModeSerializer : SCR_GameModeSerializer
{
	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return JOLP_JustOneLastPatrolGameMode;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity entity, notnull SaveContext context)
	{
		JOLP_JustOneLastPatrolGameMode mode = JOLP_JustOneLastPatrolGameMode.Cast(entity);
		if (!mode)
			return ESerializeResult.ERROR;

		context.StartObject("base");
		ESerializeResult baseResult = super.Serialize(entity, context);
		context.EndObject();
		if (baseResult == ESerializeResult.ERROR)
			return baseResult;

		context.WriteValue("version", 1);

		bool gameStarted = mode.HasGameStarted();
		context.Write(gameStarted);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity entity, notnull LoadContext context)
	{
		JOLP_JustOneLastPatrolGameMode mode = JOLP_JustOneLastPatrolGameMode.Cast(entity);
		if (!mode)
			return false;

		if (context.DoesObjectExist("base"))
		{
			if (!context.StartObject("base") || !super.Deserialize(entity, context) || !context.EndObject())
				return false;
		}

		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		bool gameStarted;
		if (context.Read(gameStarted) && gameStarted)
		{
			Print("[JOLP] Restoring a started campaign", LogLevel.NORMAL);
			mode.RestoreStartedCampaign();
		}

		return true;
	}
}
