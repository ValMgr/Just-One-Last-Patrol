//------------------------------------------------------------------------------------------------
//! One entity waiting for lazy persistence registration so it can be released again.
class JOLP_TransientUntrackEntry : Managed
{
	EntityID m_EntityId;
	int m_iAttempts;
}

//------------------------------------------------------------------------------------------------
//! Facade over SaveGameManager. Campaign data lives in the serializers, not here.
//! Saving is server-only. Clients never write a save point.
[ComponentEditorProps(category: "JOLP", description: "Save and autosave for Just One Last Patrol. Place on the game mode.")]
class JOLP_PersistenceManagerComponentClass : ScriptComponentClass
{
}

class JOLP_PersistenceManagerComponent : ScriptComponent
{
	[Attribute(defvalue: "1", desc: "Autosave while the campaign is running.", category: "Auto-Save")]
	protected bool m_bEnableAutosave;

	[Attribute(defvalue: "600", desc: "Seconds between autosaves.", category: "Auto-Save")]
	protected float m_fAutosaveInterval;

	protected SCR_PersistenceSystem m_PersistenceSystem;
	protected bool m_bAutosaveScheduled;
	protected bool m_bShutdownSaveRequested;
	protected bool m_bHasSaveGame;
	protected bool m_bSaveInProgress;
	protected static JOLP_PersistenceManagerComponent s_Instance;
	protected ref array<ref JOLP_TransientUntrackEntry> m_aTransientUntrack;
	protected bool m_bTransientUntrackScheduled;
	protected static int s_iTransientReleased;

	protected static const int TRANSIENT_UNTRACK_INTERVAL_MS = 1000;
	protected static const int TRANSIENT_UNTRACK_MAX_ATTEMPTS = 60;

	//------------------------------------------------------------------------------------------------
	static JOLP_PersistenceManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	SCR_PersistenceSystem GetPersistenceSystem()
	{
		return m_PersistenceSystem;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPlayingLoadedSave()
	{
		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
			return false;

		return manager.GetActiveSave() != null;
	}

	//------------------------------------------------------------------------------------------------
	void SaveGame()
	{
		RequestSavePoint(ESaveGameType.MANUAL);
	}

	//------------------------------------------------------------------------------------------------
	void StartAutosaves()
	{
		if (m_bAutosaveScheduled || !m_bEnableAutosave || m_fAutosaveInterval <= 0)
			return;

		if (!Replication.IsServer())
			return;

		m_bAutosaveScheduled = true;
		GetGame().GetCallqueue().CallLater(OnAutosaveTimer, m_fAutosaveInterval * 1000, true);
		Print(string.Format("[JOLP] Autosave every %1 seconds", m_fAutosaveInterval), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAutosaveTimer()
	{
		if (m_bSaveInProgress)
			return;

		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager || manager.IsBusy())
			return;

		AutoSave();
	}

	//------------------------------------------------------------------------------------------------
	void AutoSave()
	{
		if (!PassesSaveGates())
			return;

		SaveGameManager manager = GetGame().GetSaveGameManager();
		m_bSaveInProgress = true;
		manager.GetSaves(manager.GetCurrentMissionResource(), new SaveGameObtainCallback(OnAutosaveTargetObtained));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAutosaveTargetObtained(bool success, array<SaveGame> saves)
	{
		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
		{
			m_bSaveInProgress = false;
			Print("[JOLP] Autosave aborted: SaveGameManager is gone", LogLevel.ERROR);
			return;
		}

		SaveGame latestAuto;
		if (success && saves)
		{
			int playthrough = manager.GetCurrentPlaythroughNumber();
			for (int i = saves.Count() - 1; i >= 0; i--)
			{
				if ((saves[i].GetType() & ESaveGameType.AUTO) == 0)
					continue;

				if (saves[i].GetPlaythroughNumber() != playthrough)
					continue;

				latestAuto = saves[i];
				break;
			}
		}

		ESaveGameRequestFlags flags;
		if (RplSession.Mode() == RplMode.None)
			flags = ESaveGameRequestFlags.BLOCKING;

		if (latestAuto)
			manager.RequestSavePointOverwrite(latestAuto, flags, new SaveGameOperationCallback(OnSavePointCompleted));
		else
			manager.RequestSavePoint(ESaveGameType.AUTO, string.Empty, flags, new SaveGameOperationCallback(OnSavePointCompleted));
	}

	//------------------------------------------------------------------------------------------------
	//! Shutdown save. OnGameEnd can run twice; the second pass is after saving has been disabled.
	void OnGameEnd()
	{
		if (m_bShutdownSaveRequested)
			return;

		JOLP_JustOneLastPatrolGameMode mode = JOLP_JustOneLastPatrolGameMode.Cast(GetGame().GetGameMode());
		if (!mode || !mode.HasGameStarted())
		{
			Print("[JOLP] Campaign never started, skipping shutdown save", LogLevel.NORMAL);
			return;
		}

		m_bShutdownSaveRequested = true;
		RequestSavePoint(ESaveGameType.SHUTDOWN);
	}

	//------------------------------------------------------------------------------------------------
	protected bool PassesSaveGates()
	{
		JOLP_JustOneLastPatrolGameMode mode = JOLP_JustOneLastPatrolGameMode.Cast(GetGame().GetGameMode());
		if (!mode || !mode.HasGameStarted())
		{
			Print("[JOLP] Cannot save: campaign has not started", LogLevel.WARNING);
			return false;
		}

		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
		{
			Print("[JOLP] Cannot save: no SaveGameManager", LogLevel.ERROR);
			return false;
		}

		if (!manager.IsSavingAllowed())
		{
			Print("[JOLP] Cannot save: saving is disabled", LogLevel.WARNING);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestSavePoint(ESaveGameType saveType)
	{
		if (!PassesSaveGates())
			return;

		SaveGameManager manager = GetGame().GetSaveGameManager();

		ESaveGameRequestFlags flags;
		if (RplSession.Mode() == RplMode.None || saveType == ESaveGameType.SHUTDOWN)
			flags = ESaveGameRequestFlags.BLOCKING;

		m_bSaveInProgress = true;
		manager.RequestSavePoint(saveType, string.Empty, flags, new SaveGameOperationCallback(OnSavePointCompleted));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSavePointCompleted(bool success)
	{
		m_bSaveInProgress = false;
		if (success)
		{
			m_bHasSaveGame = true;
			Print("[JOLP] Save point created", LogLevel.NORMAL);
		}
		else
		{
			Print("[JOLP] Save point failed", LogLevel.ERROR);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Drops an entity this campaign rebuilds itself (AI, waypoints, IED props) before it becomes
	//! a permanent orphan. Registration is lazy, so a miss is queued and retried.
	static void UntrackTransient(IEntity entity)
	{
		if (!entity)
			return;

		if (!Replication.IsServer())
			return;

		if (JOLP_PersistenceTracking.IsTracked(entity))
		{
			JOLP_PersistenceTracking.Untrack(entity, false);
			s_iTransientReleased++;
			return;
		}

		if (s_Instance)
			s_Instance.QueueTransientUntrack(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected void QueueTransientUntrack(IEntity entity)
	{
		if (!m_PersistenceSystem)
			return;

		if (!m_aTransientUntrack)
			m_aTransientUntrack = new array<ref JOLP_TransientUntrackEntry>();

		JOLP_TransientUntrackEntry entry = new JOLP_TransientUntrackEntry();
		entry.m_EntityId = entity.GetID();
		m_aTransientUntrack.Insert(entry);

		if (!m_bTransientUntrackScheduled)
		{
			GetGame().GetCallqueue().CallLater(ProcessTransientUntrackQueue, TRANSIENT_UNTRACK_INTERVAL_MS, true);
			m_bTransientUntrackScheduled = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ProcessTransientUntrackQueue()
	{
		if (!m_aTransientUntrack || m_aTransientUntrack.IsEmpty())
		{
			StopTransientUntrackTimer();
			return;
		}

		BaseWorld world = GetOwner().GetWorld();
		for (int i = m_aTransientUntrack.Count() - 1; i >= 0; i--)
		{
			JOLP_TransientUntrackEntry entry = m_aTransientUntrack[i];
			IEntity entity = world.FindEntityByID(entry.m_EntityId);
			if (!entity)
			{
				m_aTransientUntrack.Remove(i);
				continue;
			}

			if (JOLP_PersistenceTracking.IsTracked(entity))
			{
				JOLP_PersistenceTracking.Untrack(entity, false);
				s_iTransientReleased++;
				m_aTransientUntrack.Remove(i);
				continue;
			}

			entry.m_iAttempts++;
			if (entry.m_iAttempts >= TRANSIENT_UNTRACK_MAX_ATTEMPTS)
				m_aTransientUntrack.Remove(i);
		}

		if (m_aTransientUntrack.IsEmpty())
			StopTransientUntrackTimer();
	}

	//------------------------------------------------------------------------------------------------
	protected void StopTransientUntrackTimer()
	{
		if (!m_bTransientUntrackScheduled)
			return;

		GetGame().GetCallqueue().Remove(ProcessTransientUntrackQueue);
		m_bTransientUntrackScheduled = false;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		m_PersistenceSystem = SCR_PersistenceSystem.GetScriptedInstance();
		if (!m_PersistenceSystem)
		{
			if (GetGame().InPlayMode())
				Print("[JOLP] No SCR_PersistenceSystem. Check Configs/Systems/ChimeraSystemsConfig.conf", LogLevel.ERROR);
		}
		else
		{
			s_Instance = this;
			m_PersistenceSystem.GetOnAfterSave().Insert(OnAfterSave);
			Print("[JOLP] Persistence manager ready", LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
			s_Instance = null;

		if (m_bAutosaveScheduled)
			GetGame().GetCallqueue().Remove(OnAutosaveTimer);

		StopTransientUntrackTimer();

		if (m_PersistenceSystem)
		{
			m_PersistenceSystem.GetOnAfterSave().Remove(OnAfterSave);
			m_PersistenceSystem = null;
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAfterSave(ESaveGameType saveType, bool success)
	{
		if (success)
		{
			m_bHasSaveGame = true;
			Print(string.Format("[JOLP] Save completed (%1)", typename.EnumToString(ESaveGameType, saveType)), LogLevel.NORMAL);
		}
		else
		{
			Print(string.Format("[JOLP] Save failed (%1)", typename.EnumToString(ESaveGameType, saveType)), LogLevel.ERROR);
		}
	}
}
