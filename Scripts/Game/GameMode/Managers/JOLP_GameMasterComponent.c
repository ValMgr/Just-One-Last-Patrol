class JOLP_GameMasterComponentClass : JOLP_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Grants the vanilla Game Master editor (Zeus).
//! A replicated server gives it only to admins. An offline session, and the local player in Workbench, always receive it so solo play can debug.
class JOLP_GameMasterComponent : JOLP_Component
{
	static JOLP_GameMasterComponent s_Instance;

	protected static const int UNLIMITED_BUDGET = 100000;
	protected static const int SUBSCRIBE_RETRY_MS = 500;
	protected static const int SUBSCRIBE_RETRY_MAX = 20;

	protected bool m_bSubscribed;
	protected int m_iSubscribeAttempts;
	protected ref set<int> m_aUnlimitedBudgetPlayers;

	//------------------------------------------------------------------------------------------------
	static JOLP_GameMasterComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode gameMode = GetGame().GetGameMode();
			if (gameMode)
				s_Instance = JOLP_GameMasterComponent.Cast(gameMode.FindComponent(JOLP_GameMasterComponent));
		}

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void Init(IEntity owner)
	{
		s_Instance = this;

		if (!Replication.IsServer())
			return;

		Subscribe();
	}

	//------------------------------------------------------------------------------------------------
	//! Re-evaluate one player after their editor manager exists or their admin role changes.
	void ApplyGameMaster(int playerId)
	{
		if (!Replication.IsServer() || playerId <= 0)
			return;

		SCR_EditorManagerCore core = GetEditorCore();
		if (!core)
			return;

		SCR_EditorManagerEntity editorManager = core.GetEditorManager(playerId);
		if (!editorManager)
			return;

		if (IsEligible(playerId))
			Grant(playerId, editorManager);
		else
			Revoke(playerId, editorManager);
	}

	//------------------------------------------------------------------------------------------------
	protected void Subscribe()
	{
		if (m_bSubscribed)
			return;

		SCR_EditorManagerCore core = GetEditorCore();
		if (!core)
		{
			m_iSubscribeAttempts++;
			if (m_iSubscribeAttempts > SUBSCRIBE_RETRY_MAX)
			{
				Print("[JOLP] Game Master: editor core never became available", LogLevel.ERROR);
				return;
			}

			GetGame().GetCallqueue().CallLater(Subscribe, SUBSCRIBE_RETRY_MS, false);
			return;
		}

		core.Event_OnEditorManagerCreatedServer.Remove(OnEditorManagerCreatedServer);
		core.Event_OnEditorManagerCreatedServer.Insert(OnEditorManagerCreatedServer);
		m_bSubscribed = true;

		array<SCR_EditorManagerEntity> managers = {};
		core.GetEditorEntities(managers);
		foreach (SCR_EditorManagerEntity editorManager : managers)
		{
			if (!editorManager)
				continue;

			ApplyGameMaster(editorManager.GetPlayerID());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The editor core is still writing base and role modes when this fires. Apply on the next callqueue turn.
	protected void OnEditorManagerCreatedServer(SCR_EditorManagerEntity editorManager)
	{
		if (!editorManager)
			return;

		GetGame().GetCallqueue().CallLater(ApplyGameMaster, 0, false, editorManager.GetPlayerID());
	}

	//------------------------------------------------------------------------------------------------
	protected void Grant(int playerId, SCR_EditorManagerEntity editorManager)
	{
		bool created = !editorManager.HasMode(EEditorMode.EDIT);
		if (created)
			editorManager.AddEditorModes(EEditorModeAccess.BASE, EEditorMode.EDIT, false);

		if (!editorManager.HasMode(EEditorMode.EDIT))
		{
			Print(string.Format("[JOLP] Game Master: failed to create EDIT mode for player %1", playerId), LogLevel.ERROR);
			return;
		}

		if (created && !editorManager.IsOpened())
			editorManager.SetCurrentMode(EEditorMode.EDIT);

		RaiseBudgets(playerId, editorManager);

		if (created)
			Print(string.Format("[JOLP] Game Master granted to player %1 (%2)", playerId, GetGame().GetPlayerManager().GetPlayerName(playerId)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void Revoke(int playerId, SCR_EditorManagerEntity editorManager)
	{
		if (!editorManager.HasMode(EEditorMode.EDIT))
			return;

		editorManager.RemoveEditorModes(EEditorModeAccess.BASE, EEditorMode.EDIT);
		if (editorManager.HasMode(EEditorMode.EDIT))
			return;

		if (m_aUnlimitedBudgetPlayers)
			m_aUnlimitedBudgetPlayers.Remove(playerId);

		Print(string.Format("[JOLP] Game Master removed from player %1 (%2)", playerId, GetGame().GetPlayerManager().GetPlayerName(playerId)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Offline play and Workbench are debug sessions. A live server keeps the editor for admins.
	protected bool IsEligible(int playerId)
	{
		if (!Replication.IsRunning())
			return true;

#ifdef WORKBENCH
		if (playerId == SCR_PlayerController.GetLocalPlayerId())
			return true;
#endif

		return SCR_Global.IsAdmin(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void RaiseBudgets(int playerId, SCR_EditorManagerEntity editorManager)
	{
		if (m_aUnlimitedBudgetPlayers && m_aUnlimitedBudgetPlayers.Contains(playerId))
			return;

		SCR_EditorModeEntity mode = editorManager.FindModeEntity(EEditorMode.EDIT);
		if (!mode)
			return;

		SCR_BudgetEditorComponent budget = SCR_BudgetEditorComponent.Cast(mode.FindComponent(SCR_BudgetEditorComponent));
		if (!budget)
			budget = SCR_BudgetEditorComponent.Cast(editorManager.FindComponent(SCR_BudgetEditorComponent));
		if (!budget)
			return;

		budget.SetMaxBudgetValue(EEditableEntityBudget.PROPS, UNLIMITED_BUDGET);
		budget.SetMaxBudgetValue(EEditableEntityBudget.AI, UNLIMITED_BUDGET);
		budget.SetMaxBudgetValue(EEditableEntityBudget.VEHICLES, UNLIMITED_BUDGET);
		budget.SetMaxBudgetValue(EEditableEntityBudget.WAYPOINTS, UNLIMITED_BUDGET);
		budget.SetMaxBudgetValue(EEditableEntityBudget.SYSTEMS, UNLIMITED_BUDGET);
		budget.SetMaxBudgetValue(EEditableEntityBudget.CAMPAIGN, UNLIMITED_BUDGET);

		if (!m_aUnlimitedBudgetPlayers)
			m_aUnlimitedBudgetPlayers = new set<int>();

		m_aUnlimitedBudgetPlayers.Insert(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_EditorManagerCore GetEditorCore()
	{
		return SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
	}
}
