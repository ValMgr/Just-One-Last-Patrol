//------------------------------------------------------------------------------------------------
//! Map civilians may not use our character prefab. A proxy parented to the body carries Speak
//! when the prefab itself has no action. Prefer the baked action on JOLP_Civilian.et.
class JOLP_CivilianTalkAttach
{
	protected static const ResourceName PROXY = "{664B13AF00000180}Prefabs/Characters/JOLP_CivilianTalkProxy.et";
	protected static const int MAX_TRIES = 30;
	protected static const int MAX_NOT_SERVER_PASSES = 120;
	protected static const int FLUSH_INTERVAL_MS = 1000;
	protected static ref array<EntityID> s_aPending;
	protected static ref array<int> s_aTries;
	protected static int s_iNotServerPasses;

	//------------------------------------------------------------------------------------------------
	//! Never spawns the proxy right away: this runs from OnPostInit, before the character is replicated.
	static void Queue(IEntity character)
	{
		if (!character || !GetGame() || !GetGame().GetCallqueue())
			return;

		if (SCR_Global.IsEditMode())
			return;

		// A dedicated client only displays the proxy the server replicates.
		if (Replication.IsClient() && !Replication.IsServer())
			return;

		if (!s_aPending)
		{
			s_aPending = new array<EntityID>();
			s_aTries = new array<int>();
		}

		EntityID id = character.GetID();
		if (s_aPending.Find(id) < 0)
		{
			s_aPending.Insert(id);
			s_aTries.Insert(0);
		}

		if (GetGame().GetCallqueue().GetRemainingTime(Flush) >= 0)
			return;

		GetGame().GetCallqueue().CallLater(Flush, FLUSH_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	static void Flush()
	{
		if (!s_aPending || !GetGame() || !GetGame().GetWorld())
		{
			Stop();
			return;
		}

		if (!Replication.IsServer())
		{
			s_iNotServerPasses++;
			if (s_iNotServerPasses >= MAX_NOT_SERVER_PASSES)
				Stop();

			return;
		}

		s_iNotServerPasses = 0;

		for (int i = s_aPending.Count() - 1; i >= 0; i--)
		{
			IEntity character = GetGame().GetWorld().FindEntityByID(s_aPending[i]);
			s_aTries[i] = s_aTries[i] + 1;
			if (!character || Try(character) || s_aTries[i] >= MAX_TRIES)
			{
				s_aPending.Remove(i);
				s_aTries.Remove(i);
			}
		}

		if (s_aPending.IsEmpty())
			Stop();
	}

	//------------------------------------------------------------------------------------------------
	//! True when this character is finished (proxy on, already able to speak, or not a villager).
	static bool Try(IEntity character)
	{
		if (!character)
			return true;

		if (!Replication.IsServer())
			return false;

		if (!SCR_ChimeraCharacter.Cast(character))
			return true;

		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character) > 0)
			return true;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return false;

		Faction faction = affiliation.GetAffiliatedFaction();
		if (!faction)
			return false;

		if (faction.GetFactionKey() != "CIV")
			return true;

		if (HasTalkAction(character))
			return true;

		if (HasProxy(character))
			return true;

		Resource resource = Resource.Load(PROXY);
		if (!resource)
			return false;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.LOCAL;
		params.Transform[0] = "1 0 0";
		params.Transform[1] = "0 1 0";
		params.Transform[2] = "0 0 1";
		params.Transform[3] = "0 1.65 0.12";

		IEntity proxy = GetGame().SpawnEntityPrefab(resource, character.GetWorld(), params);
		if (!proxy)
			return false;

		// Spawn params Parent is unreliable for GenericEntity. Force the hierarchy link.
		character.AddChild(proxy, -1);
		proxy.SetName("JOLP_TalkProxy");
		JOLP_PersistenceManagerComponent.UntrackTransient(proxy);

		if (proxy.GetParent() != character)
		{
			Print("[JOLP] Talk proxy failed to parent — Speak will not work on this civilian", LogLevel.WARNING);
			SCR_EntityHelper.DeleteEntityAndChildren(proxy);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stop()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Flush);

		s_iNotServerPasses = 0;
		if (s_aPending)
			s_aPending.Clear();
		if (s_aTries)
			s_aTries.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasTalkAction(IEntity character)
	{
		ActionsManagerComponent manager = ActionsManagerComponent.Cast(character.FindComponent(ActionsManagerComponent));
		if (!manager)
			return false;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (JOLP_CivilianTalkUserAction.Cast(action))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasProxy(IEntity character)
	{
		IEntity child = character.GetChildren();
		while (child)
		{
			if (child.GetName() == "JOLP_TalkProxy")
				return true;

			child = child.GetSibling();
		}

		return false;
	}
}
