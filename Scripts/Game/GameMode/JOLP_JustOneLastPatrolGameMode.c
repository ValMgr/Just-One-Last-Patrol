class JOLP_JustOneLastPatrolGameModeClass : SCR_BaseGameModeClass
{
}

//------------------------------------------------------------------------------------------------
//! Hosts Just One Last Patrol managers and drives Init → DoStartGame → PostGameStart.
class JOLP_JustOneLastPatrolGameMode : SCR_BaseGameMode
{
	protected JOLP_LocationManagerComponent m_LocationManager;
	protected JOLP_MilicianManagerComponent m_MilicianManager;
	protected JOLP_CivilianManagerComponent m_CivilianManager;
	protected JOLP_IEDManagerComponent m_IEDManager;
	protected JOLP_GameMasterComponent m_GameMaster;
	protected JOLP_PersistenceManagerComponent m_Persistence;

	protected bool m_bGameStarted;
	protected bool m_bStartScheduled;
	protected bool m_bRestoredFromSave;
	protected int m_iPersistenceWaitAttempts;

	//------------------------------------------------------------------------------------------------
	bool HasGameStarted()
	{
		return m_bGameStarted;
	}

	//------------------------------------------------------------------------------------------------
	//! True when this session continued a save. PostGameStart must not roll a new campaign.
	bool WasRestoredFromSave()
	{
		return m_bRestoredFromSave;
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the game mode serializer when the save says the campaign was already running.
	//! Safe to call again on a live session: DoStartGame is not entered a second time.
	void RestoreStartedCampaign()
	{
		if (!IsMaster())
			return;

		m_bRestoredFromSave = true;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		Print("[JOLP] Initializing Just One Last Patrol", LogLevel.NORMAL);

		m_Persistence = JOLP_PersistenceManagerComponent.Cast(FindComponent(JOLP_PersistenceManagerComponent));

		m_LocationManager = JOLP_LocationManagerComponent.Cast(FindComponent(JOLP_LocationManagerComponent));
		if (m_LocationManager)
		{
			JOLP_LocationManagerComponent.s_Instance = m_LocationManager;
			m_LocationManager.Init(this);
		}
		else
		{
			Print("[JOLP] Missing JOLP_LocationManagerComponent on game mode prefab", LogLevel.ERROR);
		}

		// Milician manager prefers the Manager.layer world entity; fall back to game-mode component.
		m_MilicianManager = JOLP_MilicianManagerComponent.GetInstance();
		if (m_MilicianManager)
		{
			m_MilicianManager.Init(m_MilicianManager.GetOwner());
		}
		else
		{
			Print("[JOLP] Missing JOLP_MilicianManagerComponent (place Prefabs/Managers/JOLP_MilicianManager.et in Manager.layer)", LogLevel.ERROR);
		}

		m_CivilianManager = JOLP_CivilianManagerComponent.GetInstance();
		if (m_CivilianManager)
		{
			m_CivilianManager.Init(m_CivilianManager.GetOwner());
		}
		else
		{
			Print("[JOLP] Missing JOLP_CivilianManagerComponent (place Prefabs/Managers/JOLP_CivilianManager.et in Manager.layer)", LogLevel.ERROR);
		}

		m_IEDManager = JOLP_IEDManagerComponent.GetInstance();
		if (m_IEDManager)
		{
			m_IEDManager.Init(m_IEDManager.GetOwner());
		}
		else
		{
			Print("[JOLP] Missing JOLP_IEDManagerComponent (place Prefabs/Managers/JOLP_IEDManager.et in Manager.layer)", LogLevel.ERROR);
		}

		m_GameMaster = JOLP_GameMasterComponent.Cast(FindComponent(JOLP_GameMasterComponent));
		if (m_GameMaster)
		{
			JOLP_GameMasterComponent.s_Instance = m_GameMaster;
			m_GameMaster.Init(this);
		}
		else
		{
			Print("[JOLP] Missing JOLP_GameMasterComponent on game mode prefab", LogLevel.ERROR);
		}

		ScheduleStartGame();
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnPlayerRoleChange(int playerId, EPlayerRole roleFlags)
	{
		super.OnPlayerRoleChange(playerId, roleFlags);

		if (m_GameMaster)
			m_GameMaster.ApplyGameMaster(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! The shutdown save has to land before vanilla turns saving off. A dedicated server must
	//! not ClearStorage and Purge the playthrough when the game mode ends.
	override void OnGameEnd()
	{
		if (IsMaster())
		{
			if (!m_Persistence)
				m_Persistence = JOLP_PersistenceManagerComponent.Cast(FindComponent(JOLP_PersistenceManagerComponent));

			if (m_Persistence)
				m_Persistence.OnGameEnd();
		}

		super.OnGameEnd();
	}

	//------------------------------------------------------------------------------------------------
	protected override void HandleOnGameModeEndSaveData()
	{
		if (!IsMaster())
			return;

		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (manager)
			manager.SetSavingAllowed(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ScheduleStartGame()
	{
		if (m_bStartScheduled || m_bGameStarted)
			return;

		m_bStartScheduled = true;
		GetGame().GetCallqueue().CallLater(DoStartGame, 500, false);
	}

	//------------------------------------------------------------------------------------------------
	void DoStartGame()
	{
		if (m_bGameStarted)
			return;

		if (!Replication.IsServer())
		{
			m_bGameStarted = true;
			return;
		}

		if (WaitingForPersistence())
		{
			m_iPersistenceWaitAttempts++;
			if (m_iPersistenceWaitAttempts < 50)
			{
				GetGame().GetCallqueue().CallLater(DoStartGame, 100, false);
				return;
			}

			Print("[JOLP] Persistence did not become active. Starting with the state already loaded", LogLevel.WARNING);
		}

		m_bGameStarted = true;
		Print("[JOLP] DoStartGame", LogLevel.NORMAL);

		if (m_LocationManager)
			m_LocationManager.PostGameStart();

		m_MilicianManager = JOLP_MilicianManagerComponent.GetInstance();
		if (m_MilicianManager)
			m_MilicianManager.PostGameStart();

		m_CivilianManager = JOLP_CivilianManagerComponent.GetInstance();
		if (m_CivilianManager)
			m_CivilianManager.PostGameStart();

		m_IEDManager = JOLP_IEDManagerComponent.GetInstance();
		if (m_IEDManager)
			m_IEDManager.PostGameStart();

		if (!m_Persistence)
			m_Persistence = JOLP_PersistenceManagerComponent.Cast(FindComponent(JOLP_PersistenceManagerComponent));

		if (m_Persistence)
			m_Persistence.StartAutosaves();

		// World-placed Thunderbolt HQ inits before replication; seed after game start as backup.
		GetGame().GetCallqueue().CallLater(SeedThunderboltSupplies, 2000, false);
	}

	protected static IEntity s_StarterHQHit;

	//------------------------------------------------------------------------------------------------
	protected void SeedThunderboltSupplies()
	{
		if (!Replication.IsServer())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		s_StarterHQHit = null;
		// FOB.layer places Thunderbolt HQ at this origin.
		world.QueryEntitiesBySphere(
			"292.115 40 3192.747",
			40,
			CaptureStarterHQ,
			null,
			EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);

		IEntity hq = s_StarterHQHit;
		s_StarterHQHit = null;
		if (!hq)
		{
			Print("[JOLP] Starter supply seed skipped: FOB_Thunderbolt not found", LogLevel.WARNING);
			return;
		}

		float current = JOLP_SupplyStock.Read(hq);
		if (current + 0.01 >= 1000)
		{
			Print(string.Format("[JOLP] Starter HQ already has %1 supplies", current), LogLevel.NORMAL);
			return;
		}

		if (JOLP_SupplyStock.Write(hq, 1000))
			Print(string.Format("[JOLP] Starter HQ seeded 1000 supplies into Storage_Supplies (was %1)", current), LogLevel.NORMAL);
		else
			Print(string.Format("[JOLP] Starter HQ supply seed failed (read %1)", JOLP_SupplyStock.Read(hq)), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CaptureStarterHQ(IEntity entity)
	{
		if (!entity)
			return true;

		string name = entity.GetName();
		if (name != "FOB_Thunderbolt")
			return true;

		s_StarterHQHit = entity;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A continue must not roll hideouts until the serializers have applied the save.
	protected bool WaitingForPersistence()
	{
		if (!m_Persistence)
			m_Persistence = JOLP_PersistenceManagerComponent.Cast(FindComponent(JOLP_PersistenceManagerComponent));

		if (!m_Persistence || !m_Persistence.IsPlayingLoadedSave())
			return false;

		SCR_PersistenceSystem system = m_Persistence.GetPersistenceSystem();
		if (!system)
			system = SCR_PersistenceSystem.GetScriptedInstance();

		if (!system)
			return true;

		EPersistenceSystemState state = system.GetState();
		if (state == EPersistenceSystemState.FAILURE || state == EPersistenceSystemState.SHUTDOWN)
			return false;

		return state != EPersistenceSystemState.ACTIVE;
	}
}
