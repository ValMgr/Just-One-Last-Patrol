class JOLP_LoadoutManagerComponentClass : JOLP_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Named kits, one list per player identity. The server owns the map. The save writes it.
class JOLP_LoadoutManagerComponent : JOLP_Component
{
	static const int MAX_LOADOUTS = 16;
	static const int MAX_NAME_LENGTH = 24;
	static const float MAX_DISTANCE_M = 8;

	static JOLP_LoadoutManagerComponent s_Instance;

	protected ref map<string, ref array<ref JOLP_PlayerLoadout>> m_mLoadouts;

	//------------------------------------------------------------------------------------------------
	static JOLP_LoadoutManagerComponent GetInstance()
	{
		if (s_Instance)
			return s_Instance;

		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return null;

		s_Instance = JOLP_LoadoutManagerComponent.Cast(gameMode.FindComponent(JOLP_LoadoutManagerComponent));
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		if (JOLP_Component.IsPreviewInstance(owner))
			return;

		s_Instance = this;
		EnsureMap();
	}

	//------------------------------------------------------------------------------------------------
	static string IdentityOf(int playerId)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		string identity;
		if (player)
			identity = SCR_PlayerIdentityUtils.GetPlayerIdentityId(player);

		if (IsUsableIdentity(identity))
			return identity;

		if (Replication.IsServer() && !Replication.IsClient())
			return string.Empty;

		return "local:" + playerId.ToString();
	}

	//------------------------------------------------------------------------------------------------
	bool SaveLoadout(int playerId, string name, IEntity character, out string error)
	{
		error = string.Empty;
		if (!Replication.IsServer())
		{
			error = "The server stores loadouts.";
			return false;
		}

		string identity = IdentityOf(playerId);
		if (identity.IsEmpty())
		{
			error = "Your identity is not available yet.";
			return false;
		}

		string trimmed = SanitizeName(name, error);
		if (trimmed.IsEmpty())
			return false;

		if (!character)
		{
			error = "You have no character.";
			return false;
		}

		EnsureMap();
		array<ref JOLP_PlayerLoadout> list = ListOf(identity);
		int existing = IndexOf(list, trimmed);
		if (existing < 0 && list.Count() >= MAX_LOADOUTS)
		{
			error = "You already have 16 loadouts.";
			return false;
		}

		JOLP_PlayerLoadout loadout = new JOLP_PlayerLoadout();
		loadout.m_sName = trimmed;
		loadout.m_sIdentity = identity;
		JOLP_LoadoutGear.Extract(character, loadout);

		if (existing >= 0)
		{
			list.RemoveOrdered(existing);
			list.InsertAt(loadout, existing);
		}
		else
			list.Insert(loadout);

		Print(string.Format("[JOLP] Saved loadout '%1' for %2 (%3 pieces)", trimmed, identity, JOLP_LoadoutGear.CountPieces(loadout)), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void GetNames(int playerId, notnull array<string> names)
	{
		names.Clear();
		if (!Replication.IsServer())
			return;

		string identity = IdentityOf(playerId);
		if (identity.IsEmpty())
			return;

		EnsureMap();
		array<ref JOLP_PlayerLoadout> list = ListOf(identity);
		foreach (JOLP_PlayerLoadout loadout : list)
		{
			if (loadout && !loadout.m_sName.IsEmpty())
				names.Insert(loadout.m_sName);
		}
	}

	//------------------------------------------------------------------------------------------------
	bool Describe(int playerId, string name, IEntity character, IEntity arsenal, out string text, out string cost, out string error)
	{
		text = string.Empty;
		cost = string.Empty;
		error = string.Empty;
		if (!Replication.IsServer())
		{
			error = "The server prices loadouts.";
			return false;
		}

		JOLP_PlayerLoadout saved = Find(playerId, name);
		if (!saved)
		{
			error = "That loadout is gone.";
			return false;
		}

		SCR_EArsenalSupplyCostType costType = CostType(arsenal);
		JOLP_PlayerLoadout worn = new JOLP_PlayerLoadout();
		JOLP_LoadoutGear.Extract(character, worn);
		int wornCost = JOLP_LoadoutGear.Price(worn, costType);
		int supplies = JOLP_LoadoutSupplies.Read(arsenal);
		JOLP_LoadoutGear.Describe(saved, costType, wornCost, supplies, text, cost);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool DeleteLoadout(int playerId, string name, out string error)
	{
		error = string.Empty;
		if (!Replication.IsServer())
		{
			error = "The server stores loadouts.";
			return false;
		}

		string identity = IdentityOf(playerId);
		if (identity.IsEmpty())
		{
			error = "Your identity is not available yet.";
			return false;
		}

		EnsureMap();
		array<ref JOLP_PlayerLoadout> list = ListOf(identity);
		int index = IndexOf(list, name.Trim());
		if (index < 0)
		{
			error = "That loadout is gone.";
			return false;
		}

		list.Remove(index);
		if (list.IsEmpty())
			m_mLoadouts.Remove(identity);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool ApplyLoadout(int playerId, string name, IEntity character, IEntity arsenal, out string error)
	{
		error = string.Empty;
		if (!Replication.IsServer())
		{
			error = "The server equips loadouts.";
			return false;
		}

		JOLP_PlayerLoadout saved = Find(playerId, name);
		if (!saved)
		{
			error = "That loadout is gone.";
			return false;
		}

		if (!character)
		{
			error = "You have no character.";
			return false;
		}

		SCR_EArsenalSupplyCostType costType = CostType(arsenal);
		JOLP_PlayerLoadout worn = new JOLP_PlayerLoadout();
		JOLP_LoadoutGear.Extract(character, worn);
		int wornCost = JOLP_LoadoutGear.Price(worn, costType);
		int savedCost = JOLP_LoadoutGear.Price(saved, costType);
		int net = savedCost - wornCost;

		if (!JOLP_LoadoutSupplies.CanSettle(arsenal, net, error))
			return false;

		string applyError;
		if (!JOLP_LoadoutGear.Apply(character, saved, applyError))
		{
			error = RestoreWorn(character, worn, "The loadout could not be equipped.");
			return false;
		}

		if (!JOLP_LoadoutSupplies.Settle(arsenal, net, error))
		{
			string payment = error;
			if (payment.IsEmpty())
				payment = "The supplies could not be settled.";

			error = RestoreWorn(character, worn, payment);
			return false;
		}

		Print(string.Format("[JOLP] Loaded '%1' net %2", saved.m_sName, net), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void CollectPersisted(notnull array<ref JOLP_PersistedLoadout> records)
	{
		records.Clear();
		EnsureMap();
		for (int i = 0; i < m_mLoadouts.Count(); i++)
		{
			array<ref JOLP_PlayerLoadout> list = m_mLoadouts.GetElement(i);
			if (!list)
				continue;

			foreach (JOLP_PlayerLoadout loadout : list)
			{
				if (!loadout || loadout.m_sIdentity.IsEmpty() || loadout.m_sName.IsEmpty())
					continue;

				JOLP_PersistedLoadout record = new JOLP_PersistedLoadout();
				record.ReadFrom(loadout);
				records.Insert(record);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	void ReplacePersisted(array<ref JOLP_PersistedLoadout> records)
	{
		EnsureMap();
		m_mLoadouts.Clear();
		if (!records)
			return;

		foreach (JOLP_PersistedLoadout record : records)
		{
			if (!record || record.m_sIdentity.IsEmpty() || record.m_sName.IsEmpty())
				continue;

			JOLP_PlayerLoadout loadout = record.Build();
			array<ref JOLP_PlayerLoadout> list = ListOf(loadout.m_sIdentity);
			int existing = IndexOf(list, loadout.m_sName);
			if (existing >= 0)
			{
				list.RemoveOrdered(existing);
				list.InsertAt(loadout, existing);
			}
			else
				list.Insert(loadout);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected string RestoreWorn(IEntity character, notnull JOLP_PlayerLoadout worn, string reason)
	{
		string restoreError;
		if (JOLP_LoadoutGear.Apply(character, worn, restoreError))
			return reason + " Your previous kit was kept.";

		Print("[JOLP] Loadout restore failed: " + restoreError, LogLevel.WARNING);
		return reason + " Your previous kit could not be restored.";
	}

	//------------------------------------------------------------------------------------------------
	protected JOLP_PlayerLoadout Find(int playerId, string name)
	{
		string identity = IdentityOf(playerId);
		if (identity.IsEmpty() || !name)
			return null;

		EnsureMap();
		array<ref JOLP_PlayerLoadout> list = ListOf(identity);
		int index = IndexOf(list, name.Trim());
		if (index < 0)
			return null;

		return list[index];
	}

	//------------------------------------------------------------------------------------------------
	protected array<ref JOLP_PlayerLoadout> ListOf(string identity)
	{
		EnsureMap();
		array<ref JOLP_PlayerLoadout> list;
		if (!m_mLoadouts.Find(identity, list) || !list)
		{
			list = new array<ref JOLP_PlayerLoadout>();
			m_mLoadouts.Set(identity, list);
		}

		return list;
	}

	//------------------------------------------------------------------------------------------------
	protected int IndexOf(array<ref JOLP_PlayerLoadout> list, string name)
	{
		if (!list)
			return -1;

		for (int i = 0; i < list.Count(); i++)
		{
			JOLP_PlayerLoadout loadout = list[i];
			if (loadout && loadout.m_sName == name)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected string SanitizeName(string name, out string error)
	{
		error = string.Empty;
		if (!name)
		{
			error = "Enter a name.";
			return string.Empty;
		}

		string trimmed = name.Trim();
		if (trimmed.IsEmpty())
		{
			error = "Enter a name.";
			return string.Empty;
		}

		if (trimmed.Contains("\n") || trimmed.Contains("\r"))
		{
			error = "That name is not allowed.";
			return string.Empty;
		}

		if (trimmed.Length() > MAX_NAME_LENGTH)
		{
			error = "That name is too long.";
			return string.Empty;
		}

		return trimmed;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_EArsenalSupplyCostType CostType(IEntity arsenal)
	{
		SCR_ArsenalComponent component = ArsenalOf(arsenal);
		if (!component)
			return SCR_EArsenalSupplyCostType.DEFAULT;

		return component.GetSupplyCostType();
	}

	//------------------------------------------------------------------------------------------------
	static SCR_ArsenalComponent ArsenalOf(IEntity entity)
	{
		if (!entity)
			return null;

		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(entity.FindComponent(SCR_ArsenalComponent));
		if (arsenal)
			return arsenal;

		IEntity parent = entity.GetParent();
		if (!parent)
			return null;

		return SCR_ArsenalComponent.Cast(parent.FindComponent(SCR_ArsenalComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected void EnsureMap()
	{
		if (!m_mLoadouts)
			m_mLoadouts = new map<string, ref array<ref JOLP_PlayerLoadout>>();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsUsableIdentity(string identity)
	{
		if (!identity || identity.IsEmpty())
			return false;

		if (identity == "00000000-0000-0000-0000-000000000000")
			return false;

		return true;
	}
}
