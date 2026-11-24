class JOLP_AdminNetworkComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Client asks, server applies. Lives on the player controller.
//! The replicated flag is the same eligibility rule as Game Master, so the self-interaction
//! wheel can hide Admin from everyone else. Every request is checked again on the server.
class JOLP_AdminNetworkComponent : ScriptComponent
{
	protected static const int REFRESH_RETRY_MS = 250;
	protected static const int REFRESH_RETRY_MAX = 40;

	[RplProp()]
	protected bool m_bToolsAvailable;

	protected int m_iRefreshAttempts;

	//------------------------------------------------------------------------------------------------
	static JOLP_AdminNetworkComponent Local()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return null;

		return JOLP_AdminNetworkComponent.Cast(controller.FindComponent(JOLP_AdminNetworkComponent));
	}

	//------------------------------------------------------------------------------------------------
	static JOLP_AdminNetworkComponent FromPlayer(int playerId)
	{
		if (playerId <= 0)
			return null;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return null;

		PlayerController controller = players.GetPlayerController(playerId);
		if (!controller)
			return null;

		return JOLP_AdminNetworkComponent.Cast(controller.FindComponent(JOLP_AdminNetworkComponent));
	}

	//------------------------------------------------------------------------------------------------
	bool HasTools()
	{
		return m_bToolsAvailable;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(RefreshTools, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(RefreshTools);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-read Game Master eligibility. Safe to call more than once.
	void RefreshTools()
	{
		if (!Replication.IsServer())
			return;

		PlayerController controller = PlayerController.Cast(GetOwner());
		if (!controller)
			return;

		int playerId = controller.GetPlayerId();
		if (playerId <= 0)
		{
			m_iRefreshAttempts++;
			if (m_iRefreshAttempts > REFRESH_RETRY_MAX)
				return;

			GetGame().GetCallqueue().CallLater(RefreshTools, REFRESH_RETRY_MS, false);
			return;
		}

		bool eligible = false;
		JOLP_GameMasterComponent gameMaster = JOLP_GameMasterComponent.GetInstance();
		if (gameMaster)
			eligible = gameMaster.IsEligible(playerId);

		if (m_bToolsAvailable == eligible)
			return;

		m_bToolsAvailable = eligible;
		if (Replication.IsRunning())
			Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	void RequestSave()
	{
		JOLP_AdminDialog.Deliver("Saving...");
		Rpc(RpcAsk_Save);
	}

	//------------------------------------------------------------------------------------------------
	void RequestAdvanceHours(int hours)
	{
		JOLP_AdminDialog.Deliver("Changing the time...");
		Rpc(RpcAsk_AdvanceHours, hours);
	}

	//------------------------------------------------------------------------------------------------
	void Reply(string message)
	{
		Rpc(RpcDo_AdminResult, message);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Save()
	{
		if (!IsCallerEligible())
		{
			Reply("Admin tools are not available.");
			return;
		}

		JOLP_PersistenceManagerComponent persistence = JOLP_Global.GetPersistence();
		if (!persistence)
		{
			Reply("Cannot save.");
			return;
		}

		PlayerController controller = PlayerController.Cast(GetOwner());
		int playerId = 0;
		if (controller)
			playerId = controller.GetPlayerId();

		string reason;
		if (!persistence.RequestManualSave(playerId, reason))
		{
			if (reason.IsEmpty())
				reason = "Cannot save.";

			Reply(reason);
		}
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AdvanceHours(int hours)
	{
		if (!IsCallerEligible())
		{
			Reply("Admin tools are not available.");
			return;
		}

		Reply(AdvanceHours(hours));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_AdminResult(string message)
	{
		JOLP_AdminDialog.Deliver(message);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsCallerEligible()
	{
		if (!Replication.IsServer())
			return false;

		PlayerController controller = PlayerController.Cast(GetOwner());
		if (!controller)
			return false;

		JOLP_GameMasterComponent gameMaster = JOLP_GameMasterComponent.GetInstance();
		if (!gameMaster)
			return false;

		return gameMaster.IsEligible(controller.GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the sun forward. Supply income is resampled around the write so the jump is not paid.
	protected string AdvanceHours(int hours)
	{
		if (!IsAllowedStep(hours))
			return "That time step is not allowed.";

		BaseWorld baseWorld = GetGame().GetWorld();
		if (!baseWorld)
			return "Could not change the time.";

		ChimeraWorld world = ChimeraWorld.CastFrom(baseWorld);
		if (!world)
			return "Could not change the time.";

		TimeAndWeatherManagerEntity time = world.GetTimeAndWeatherManager();
		if (!time)
			return "Could not change the time.";

		int year;
		int month;
		int day;
		time.GetDate(year, month, day);

		int hour;
		int minute;
		int second;
		time.GetHoursMinutesSeconds(hour, minute, second);

		if (month < 1 || month > 12 || day < 1)
			return "Could not change the time.";

		int total = hour + hours;
		int extraDays = total / 24;
		int newHour = total - (extraDays * 24);

		if (extraDays > 0)
		{
			day = day + extraDays;
			int span = DaysInMonth(year, month);
			if (day > span)
			{
				day = day - span;
				month = month + 1;
				if (month > 12)
				{
					month = 1;
					year = year + 1;
				}
			}
		}

		JOLP_SupplyIncomeComponent.SnapAllClocks();

		bool dateOk = true;
		if (extraDays > 0)
			dateOk = time.SetDate(year, month, day, true);

		bool timeOk = false;
		if (dateOk)
			timeOk = time.SetHoursMinutesSeconds(newHour, minute, second, true);

		JOLP_SupplyIncomeComponent.SnapAllClocks();

		if (!dateOk || !timeOk)
			return "Could not change the time.";

		JOLP_IEDManagerComponent ieds = JOLP_IEDManagerComponent.GetInstance();
		if (ieds)
			ieds.AcknowledgeAdminTimeSkip();

		return string.Format("Time is now %1:%2.", Pad2(newHour), Pad2(minute));
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAllowedStep(int hours)
	{
		if (hours == 1)
			return true;

		if (hours == 3)
			return true;

		if (hours == 6)
			return true;

		if (hours == 12)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static int DaysInMonth(int year, int month)
	{
		if (month == 2)
		{
			if (IsLeapYear(year))
				return 29;

			return 28;
		}

		if (month == 4 || month == 6 || month == 9 || month == 11)
			return 30;

		return 31;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsLeapYear(int year)
	{
		if ((year % 400) == 0)
			return true;

		if ((year % 100) == 0)
			return false;

		if ((year % 4) == 0)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Pad2(int value)
	{
		if (value < 0)
			value = 0;

		if (value < 10)
			return "0" + value.ToString();

		return value.ToString();
	}
}
