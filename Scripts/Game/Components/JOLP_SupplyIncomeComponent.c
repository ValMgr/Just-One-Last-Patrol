class JOLP_SupplyIncomeComponentClass : JOLP_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! In-game period one supply grant is paid for.
enum JOLP_SupplyIncomeTimeUnit
{
	SECOND = 0,
	MINUTE = 1,
	HOUR = 2
}

//------------------------------------------------------------------------------------------------
//! In-game calendar reading. Day index and second-of-day stay in ints so a one-second
//! step is still visible; packing the whole date into one float loses minutes.
class JOLP_GameClock
{
	//------------------------------------------------------------------------------------------------
	static bool Read(out int dayIndex, out int secondOfDay)
	{
		dayIndex = 0;
		secondOfDay = 0;

		BaseWorld baseWorld = GetGame().GetWorld();
		if (!baseWorld)
			return false;

		ChimeraWorld world = ChimeraWorld.CastFrom(baseWorld);
		if (!world)
			return false;

		TimeAndWeatherManagerEntity time = world.GetTimeAndWeatherManager();
		if (!time)
			return false;

		int year;
		int month;
		int day;
		time.GetDate(year, month, day);

		int hour;
		int minute;
		int second;
		time.GetHoursMinutesSeconds(hour, minute, second);

		dayIndex = (year * 365) + (month * 31) + day;
		secondOfDay = (hour * 3600) + (minute * 60) + second;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Server-only supply drip. Pays m_iSuppliesPerPeriod into the linked SUPPLIES generator
//! each time one in-game period (second, minute, or hour) completes. The unfinished
//! fraction is persisted as in-game hours so a restart does not pay it twice and does
//! not throw it away.
class JOLP_SupplyIncomeComponent : JOLP_Component
{
	[Attribute("500", UIWidgets.EditBox, "Supplies granted each time one in-game period completes")]
	protected int m_iSuppliesPerPeriod;

	[Attribute("2", UIWidgets.ComboBox, "In-game time unit that period is measured in", "", ParamEnumArray.FromEnum(JOLP_SupplyIncomeTimeUnit))]
	protected JOLP_SupplyIncomeTimeUnit m_eTimeUnit;

	[Attribute("5000", UIWidgets.EditBox, "Real-time tick interval (ms) used to poll the in-game clock. Several periods can pay on one tick.")]
	protected int m_iPollIntervalMs;

	[Attribute("1", UIWidgets.CheckBox, "When true, prefer the HQ/master building provider resource pool over this entity")]
	protected bool m_bPreferProviderPool;

	[Attribute("0", UIWidgets.EditBox, "Optional one-shot seed applied once on server init (0 = none)")]
	protected int m_iInitialSupplies;

	[Attribute("0", UIWidgets.CheckBox, "When true, the seed sets Storage_Supplies to m_iInitialSupplies even if the pile is already higher")]
	protected bool m_bExactInitialSupplies;

	protected float m_fUnpaidSeconds;
	protected int m_iLastDayIndex;
	protected int m_iLastSecondOfDay;
	protected bool m_bClockSampled;
	protected bool m_bSeedApplied;
	protected bool m_bRunning;
	protected int m_iSeedAttempts;
	protected int m_iServerWaitAttempts;

	//------------------------------------------------------------------------------------------------
	float GetUnpaidGameHours()
	{
		return m_fUnpaidSeconds / 3600.0;
	}

	//------------------------------------------------------------------------------------------------
	//! A restored stock must stay where the save left it. The one-shot seed tops anything below
	//! m_iInitialSupplies, which would refund supplies the players already spent.
	void MarkInitialSeedApplied()
	{
		m_bSeedApplied = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Restores the partial hour and snaps the clock.
	//! Does not mark the one-shot seed done — ApplyInitialSeed still runs and no-ops when
	//! HQ Storage_Supplies already holds m_iInitialSupplies (continue / save path).
	void ApplyUnpaidGameHours(float unpaid)
	{
		if (unpaid < 0)
			unpaid = 0;

		m_fUnpaidSeconds = unpaid * 3600.0;
		SampleClock();
	}

	//------------------------------------------------------------------------------------------------
	int GetSuppliesPerPeriod()
	{
		return m_iSuppliesPerPeriod;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		if (JOLP_Component.IsPreviewInstance(owner))
			return;

		// World-placed HQs init before GameMode / replication is ready — defer server start.
		GetGame().GetCallqueue().CallLater(TryStartServer, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void TryStartServer()
	{
		if (!Replication.IsServer() || !GetGame().GetGameMode())
		{
			m_iServerWaitAttempts++;
			if (m_iServerWaitAttempts > 120)
			{
				Print("[JOLP] SupplyIncome aborted: server/game mode never became ready", LogLevel.WARNING);
				return;
			}

			GetGame().GetCallqueue().CallLater(TryStartServer, 250, false);
			return;
		}

		Print(string.Format("[JOLP] SupplyIncome server-ready on %1 (seed=%2)", GetOwner(), m_iInitialSupplies), LogLevel.NORMAL);

		if (!m_bSeedApplied)
			GetGame().GetCallqueue().CallLater(ApplyInitialSeed, 1000, false);

		if (!m_bRunning)
		{
			int poll = m_iPollIntervalMs;
			if (poll <= 0)
				poll = 5000;

			GetGame().GetCallqueue().CallLater(StartIncome, poll, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		StopIncome();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartIncome()
	{
		if (m_bRunning)
			return;

		m_bRunning = true;
		if (!m_bSeedApplied)
			ApplyInitialSeed();
		if (!m_bClockSampled)
			SampleClock();

		int poll = m_iPollIntervalMs;
		if (poll <= 0)
			poll = 5000;

		GetGame().GetCallqueue().CallLater(PollIncome, poll, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopIncome()
	{
		if (!m_bRunning)
			return;

		m_bRunning = false;
		GetGame().GetCallqueue().Remove(PollIncome);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyInitialSeed()
	{
		if (m_bSeedApplied || m_iInitialSupplies <= 0)
			return;

		IEntity root = GetOwner();
		if (!root)
			return;

		float current = JOLP_SupplyStock.Read(root);
		if (current < 0)
		{
			m_iSeedAttempts++;
			if (m_iSeedAttempts >= 30)
			{
				Print("[JOLP] SupplyIncome seed aborted: HQ Storage_Supplies not found", LogLevel.WARNING);
				m_bSeedApplied = true;
				return;
			}

			GetGame().GetCallqueue().CallLater(ApplyInitialSeed, 1000, false);
			return;
		}

		if (m_bExactInitialSupplies)
		{
			if (!JOLP_SupplyStock.SetStorageValue(root, m_iInitialSupplies))
			{
				m_iSeedAttempts++;
				if (m_iSeedAttempts >= 30)
				{
					Print(string.Format("[JOLP] SupplyIncome exact seed failed at %1 / %2 on %3",
						current, m_iInitialSupplies, root), LogLevel.WARNING);
					m_bSeedApplied = true;
					return;
				}

				GetGame().GetCallqueue().CallLater(ApplyInitialSeed, 1000, false);
				return;
			}

			m_bSeedApplied = true;
			Print(string.Format("[JOLP] SupplyIncome set HQ storage to %1 (was %2) on %3",
				m_iInitialSupplies, current, root), LogLevel.NORMAL);
			return;
		}

		if (current + 0.01 >= m_iInitialSupplies)
		{
			m_bSeedApplied = true;
			// Re-apply through SetResourceValue so encapsulator refreshes crate meshes
			// even when the world layer already stamped the virtual container value.
			JOLP_SupplyStock.RefreshStorageVisuals(root);
			Print(string.Format("[JOLP] SupplyIncome already has %1 supplies in HQ storage on %2", current, root), LogLevel.NORMAL);
			return;
		}

		if (!JOLP_SupplyStock.Write(root, m_iInitialSupplies))
		{
			m_iSeedAttempts++;
			if (m_iSeedAttempts >= 30)
			{
				Print(string.Format("[JOLP] SupplyIncome seed failed at %1 / %2 in HQ storage on %3",
					JOLP_SupplyStock.Read(root), m_iInitialSupplies, root), LogLevel.WARNING);
				m_bSeedApplied = true;
				return;
			}

			GetGame().GetCallqueue().CallLater(ApplyInitialSeed, 1000, false);
			return;
		}

		m_bSeedApplied = true;
		JOLP_SupplyStock.RefreshStorageVisuals(root);
		Print(string.Format("[JOLP] SupplyIncome seeded %1 supplies into HQ Storage_Supplies (was %2) on %3",
			m_iInitialSupplies, current, root), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void PollIncome()
	{
		if (!Replication.IsServer())
			return;

		float elapsed = ConsumeElapsedSeconds();
		if (elapsed <= 0 || m_iSuppliesPerPeriod <= 0)
			return;

		int periodSeconds = GetPeriodSeconds();
		if (periodSeconds <= 0)
			return;

		m_fUnpaidSeconds = m_fUnpaidSeconds + elapsed;
		int wholePeriods = m_fUnpaidSeconds / periodSeconds;
		if (wholePeriods <= 0)
			return;

		m_fUnpaidSeconds = m_fUnpaidSeconds - (wholePeriods * periodSeconds);
		float amount = m_iSuppliesPerPeriod * wholePeriods;
		if (GenerateSupplies(amount))
			Print(string.Format("[JOLP] SupplyIncome +%1 supplies (%2 in-game %3(s))", amount, wholePeriods, GetPeriodLabel()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void SampleClock()
	{
		int dayIndex;
		int secondOfDay;
		if (!JOLP_GameClock.Read(dayIndex, secondOfDay))
			return;

		m_iLastDayIndex = dayIndex;
		m_iLastSecondOfDay = secondOfDay;
		m_bClockSampled = true;
	}

	//------------------------------------------------------------------------------------------------
	//! In-game seconds since the last sample. Zero when the clock is missing or moved backwards.
	protected float ConsumeElapsedSeconds()
	{
		int dayIndex;
		int secondOfDay;
		if (!JOLP_GameClock.Read(dayIndex, secondOfDay))
			return 0;

		if (!m_bClockSampled)
		{
			m_iLastDayIndex = dayIndex;
			m_iLastSecondOfDay = secondOfDay;
			m_bClockSampled = true;
			return 0;
		}

		float elapsed = ((dayIndex - m_iLastDayIndex) * 86400.0) + (secondOfDay - m_iLastSecondOfDay);
		m_iLastDayIndex = dayIndex;
		m_iLastSecondOfDay = secondOfDay;
		if (elapsed < 0)
			return 0;

		return elapsed;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetPeriodSeconds()
	{
		if (m_eTimeUnit == JOLP_SupplyIncomeTimeUnit.SECOND)
			return 1;

		if (m_eTimeUnit == JOLP_SupplyIncomeTimeUnit.MINUTE)
			return 60;

		return 3600;
	}

	//------------------------------------------------------------------------------------------------
	protected string GetPeriodLabel()
	{
		if (m_eTimeUnit == JOLP_SupplyIncomeTimeUnit.SECOND)
			return "second";

		if (m_eTimeUnit == JOLP_SupplyIncomeTimeUnit.MINUTE)
			return "minute";

		return "hour";
	}

	//------------------------------------------------------------------------------------------------
	//! Manual grant into the FOB stock. The hold finishes on the client; the server applies it.
	void GrantSupplies(float amount)
	{
		if (amount <= 0)
			return;

		if (Replication.IsServer())
		{
			GenerateSupplies(amount);
			return;
		}

		Rpc(RpcAsk_GrantSupplies, amount);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_GrantSupplies(float amount)
	{
		GenerateSupplies(amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts supplies into the provider consumer the build menu reads. A generator
	//! RequestGeneration can succeed against a queue that is not that stock.
	protected bool GenerateSupplies(float amount)
	{
		if (amount <= 0)
			return false;

		float before = ReadStock();
		float remaining = amount;

		if (m_bPreferProviderPool)
			remaining = StoreInProviderConsumer(remaining);

		if (remaining > 0.01)
		{
			if (JOLP_SupplyStock.AddDelta(GetOwner(), remaining))
				remaining = 0;
		}

		SCR_ResourceComponent resource = FindProviderResource();
		if (resource)
			resource.Replicate();

		float after = ReadStock();
		float stored = amount - remaining;
		if (stored <= 0.01)
		{
			Print(string.Format("[JOLP] SupplyIncome stored nothing (asked %1, supplies %2)", amount, before), LogLevel.WARNING);
			return false;
		}

		Print(string.Format("[JOLP] SupplyIncome stored %1, supplies %2 -> %3", stored, before, after), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The HQ pile. Read() also counts the crate stacks, so a +10 grant looked like +20.
	protected float ReadStock()
	{
		return JOLP_SupplyStock.ReadDisplayed(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the amount that did not fit in the provider consumer's containers.
	protected float StoreInProviderConsumer(float amount)
	{
		if (amount <= 0)
			return 0;

		SCR_ResourceComponent resource = FindProviderResource();
		if (!resource)
			return amount;

		SCR_ResourceConsumer consumer = resource.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (!consumer)
			return amount;

		SCR_ResourceGrid grid = GetGame().GetResourceGrid();
		if (grid)
			grid.UpdateInteractor(consumer);

		SCR_ResourceContainerQueueBase queue = consumer.GetContainerQueue();
		if (!queue)
			return amount;

		float remaining = amount;
		int count = queue.GetContainerCount();
		for (int i = 0; i < count && remaining > 0.01; i++)
		{
			SCR_ResourceContainer container = queue.GetContainerAt(i);
			if (!container)
				continue;

			remaining = remaining - StoreInContainer(container, remaining, false);
		}

		if (remaining > 0.01 && count > 0)
		{
			SCR_ResourceContainer container = queue.GetContainerAt(0);
			if (container)
				remaining = remaining - StoreInContainer(container, remaining, true);
		}

		return remaining;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns how much was actually added. Grows the container when `grow` is set and it is full.
	//! A virtual Storage_Supplies container forwards the value into its encapsulator's children.
	protected float StoreInContainer(SCR_ResourceContainer container, float amount, bool grow)
	{
		if (!container || amount <= 0)
			return 0;

		SCR_ResourceEncapsulator encapsulator = container.GetResourceEncapsulator();
		if (encapsulator)
			return StoreInEncapsulator(encapsulator, amount, grow);

		float before = container.GetResourceValue();
		float space = container.GetMaxResourceValue() - before;
		if (space + 0.01 < amount && grow)
		{
			container.SetMaxResourceValue(before + amount, true);
			space = container.GetMaxResourceValue() - before;
		}

		if (space <= 0.01)
			return 0;

		float add = amount;
		if (add > space)
			add = space;

		container.IncreaseResourceValue(add);
		float applied = container.GetResourceValue() - before;
		if (applied < 0)
			return 0;

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	protected float StoreInEncapsulator(SCR_ResourceEncapsulator encapsulator, float amount, bool grow)
	{
		SCR_ResourceContainerQueueBase queue = encapsulator.GetContainerQueue();
		if (!queue)
			return 0;

		float remaining = amount;
		int count = queue.GetContainerCount();
		for (int i = 0; i < count && remaining > 0.01; i++)
		{
			SCR_ResourceContainer child = queue.GetContainerAt(i);
			if (!child || child.GetResourceEncapsulator())
				continue;

			remaining = remaining - StoreInContainer(child, remaining, false);
		}

		if (remaining > 0.01 && grow && count > 0)
		{
			SCR_ResourceContainer child = queue.GetContainerAt(0);
			if (child && !child.GetResourceEncapsulator())
				remaining = remaining - StoreInContainer(child, remaining, true);
		}

		return amount - remaining;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_ResourceComponent FindProviderResource()
	{
		SCR_CampaignBuildingProviderComponent provider;

		SCR_CampaignBuildingCompositionComponent composition = SCR_CampaignBuildingCompositionComponent.Cast(
			GetOwner().FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (composition)
		{
			IEntity providerEnt = composition.GetProviderEntity();
			if (providerEnt)
				provider = SCR_CampaignBuildingProviderComponent.Cast(
					providerEnt.FindComponent(SCR_CampaignBuildingProviderComponent));
		}

		if (!provider)
			provider = FindNearestMasterProvider(GetOwner().GetOrigin(), 500);

		if (!provider)
			return null;

		SCR_ResourceComponent resource = provider.GetResourceComponent();
		if (resource)
			return resource;

		return FindResourceRecursive(provider.GetOwner(), 0);
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_CampaignBuildingProviderComponent FindNearestMasterProvider(vector origin, float maxDistance)
	{
		SCR_CampaignBuildingProviderComponent best;
		float bestDistSq = maxDistance * maxDistance;

		SCR_MilitaryBaseSystem baseSystem = SCR_MilitaryBaseSystem.GetInstance();
		if (baseSystem)
		{
			array<SCR_MilitaryBaseComponent> bases = {};
			baseSystem.GetBases(bases);
			foreach (SCR_MilitaryBaseComponent base : bases)
			{
				if (!base)
					continue;

				IEntity baseOwner = base.GetOwner();
				if (!baseOwner)
					continue;

				float distSq = vector.DistanceSqXZ(origin, baseOwner.GetOrigin());
				if (distSq > bestDistSq)
					continue;

				SCR_CampaignBuildingProviderComponent provider = FindProviderOnEntityTree(baseOwner, 0);
				if (!provider || !provider.IsMasterProvider())
					continue;

				best = provider;
				bestDistSq = distSq;
			}
		}

		if (best)
			return best;

		return FindProviderOnEntityTree(GetOwner(), 0);
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_CampaignBuildingProviderComponent FindProviderOnEntityTree(IEntity entity, int depth)
	{
		if (!entity || depth > 6)
			return null;

		SCR_CampaignBuildingProviderComponent provider = SCR_CampaignBuildingProviderComponent.Cast(
			entity.FindComponent(SCR_CampaignBuildingProviderComponent));
		if (provider)
			return provider;

		IEntity child = entity.GetChildren();
		while (child)
		{
			SCR_CampaignBuildingProviderComponent nested = FindProviderOnEntityTree(child, depth + 1);
			if (nested)
				return nested;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_ResourceComponent FindResourceRecursive(IEntity entity, int depth)
	{
		if (!entity || depth > 6)
			return null;

		SCR_ResourceComponent resource = SCR_ResourceComponent.Cast(entity.FindComponent(SCR_ResourceComponent));
		if (resource && SCR_ResourceSystemHelper.GetFirstValidGenerator(resource, EResourceType.SUPPLIES))
			return resource;

		IEntity child = entity.GetChildren();
		while (child)
		{
			SCR_ResourceComponent nested = FindResourceRecursive(child, depth + 1);
			if (nested)
				return nested;

			child = child.GetSibling();
		}

		return null;
	}
}
