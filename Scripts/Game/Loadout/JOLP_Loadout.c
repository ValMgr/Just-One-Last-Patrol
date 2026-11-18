//------------------------------------------------------------------------------------------------
//! One prefab in a saved kit. Attachments and container contents hang off it.
//! Field order of the persisted twin is the save format. Do not reorder those writes.
class JOLP_LoadoutItem : Managed
{
	string m_sPrefab;
	int m_iSlotIndex;
	int m_iStoragePriority;
	EStoragePurpose m_eStoragePurpose;
	bool m_bEquipped;
	int m_iWeaponSlot;
	ref array<string> m_aAttachments;
	ref array<ref JOLP_LoadoutItem> m_aChildren;

	//------------------------------------------------------------------------------------------------
	void JOLP_LoadoutItem()
	{
		m_iSlotIndex = -1;
		m_iStoragePriority = -1;
		m_eStoragePurpose = EStoragePurpose.PURPOSE_ANY;
		m_iWeaponSlot = -1;
		m_aAttachments = new array<string>();
		m_aChildren = new array<ref JOLP_LoadoutItem>();
	}
}

//------------------------------------------------------------------------------------------------
//! One named kit belonging to one player identity.
class JOLP_PlayerLoadout : Managed
{
	string m_sName;
	string m_sIdentity;
	ref array<ref JOLP_LoadoutItem> m_aItems;
	ref array<string> m_aQuickSlots;

	//------------------------------------------------------------------------------------------------
	void JOLP_PlayerLoadout()
	{
		m_aItems = new array<ref JOLP_LoadoutItem>();
		m_aQuickSlots = new array<string>();
	}

	//------------------------------------------------------------------------------------------------
	void ClearItems()
	{
		m_aItems.Clear();
		m_aQuickSlots.Clear();
	}
}

//------------------------------------------------------------------------------------------------
//! Save-file copy of one item. Gameplay fields can grow without changing this layout.
class JOLP_PersistedLoadoutItem : Managed
{
	string m_sPrefab;
	int m_iSlotIndex;
	int m_iStoragePriority;
	int m_iStoragePurpose;
	bool m_bEquipped;
	int m_iWeaponSlot;
	ref array<string> m_aAttachments;
	ref array<ref JOLP_PersistedLoadoutItem> m_aChildren;

	//------------------------------------------------------------------------------------------------
	void ReadFrom(notnull JOLP_LoadoutItem item)
	{
		m_sPrefab = item.m_sPrefab;
		m_iSlotIndex = item.m_iSlotIndex;
		m_iStoragePriority = item.m_iStoragePriority;
		m_iStoragePurpose = item.m_eStoragePurpose;
		m_bEquipped = item.m_bEquipped;
		m_iWeaponSlot = item.m_iWeaponSlot;
		m_aAttachments = new array<string>();
		m_aChildren = new array<ref JOLP_PersistedLoadoutItem>();

		if (item.m_aAttachments)
		{
			foreach (string attachment : item.m_aAttachments)
				m_aAttachments.Insert(attachment);
		}

		if (item.m_aChildren)
		{
			foreach (JOLP_LoadoutItem child : item.m_aChildren)
			{
				if (!child)
					continue;

				JOLP_PersistedLoadoutItem record = new JOLP_PersistedLoadoutItem();
				record.ReadFrom(child);
				m_aChildren.Insert(record);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	JOLP_LoadoutItem Build()
	{
		JOLP_LoadoutItem item = new JOLP_LoadoutItem();
		item.m_sPrefab = m_sPrefab;
		item.m_iSlotIndex = m_iSlotIndex;
		item.m_iStoragePriority = m_iStoragePriority;
		item.m_eStoragePurpose = m_iStoragePurpose;
		item.m_bEquipped = m_bEquipped;
		item.m_iWeaponSlot = m_iWeaponSlot;

		if (m_aAttachments)
		{
			foreach (string attachment : m_aAttachments)
				item.m_aAttachments.Insert(attachment);
		}

		if (m_aChildren)
		{
			foreach (JOLP_PersistedLoadoutItem child : m_aChildren)
			{
				if (!child)
					continue;

				item.m_aChildren.Insert(child.Build());
			}
		}

		return item;
	}

	//------------------------------------------------------------------------------------------------
	bool SerializationSave(SaveContext context)
	{
		context.Write(m_sPrefab);
		context.Write(m_iSlotIndex);
		context.Write(m_iStoragePriority);
		context.Write(m_iStoragePurpose);
		context.Write(m_bEquipped);
		context.Write(m_iWeaponSlot);
		context.Write(m_aAttachments);
		context.Write(m_aChildren);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool SerializationLoad(LoadContext context)
	{
		m_aAttachments = new array<string>();
		m_aChildren = new array<ref JOLP_PersistedLoadoutItem>();
		context.Read(m_sPrefab);
		context.Read(m_iSlotIndex);
		context.Read(m_iStoragePriority);
		context.Read(m_iStoragePurpose);
		context.Read(m_bEquipped);
		context.Read(m_iWeaponSlot);
		context.Read(m_aAttachments);
		context.Read(m_aChildren);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Save-file copy of one named kit, including its item tree.
class JOLP_PersistedLoadout : Managed
{
	string m_sIdentity;
	string m_sName;
	ref array<string> m_aQuickSlots;
	ref array<ref JOLP_PersistedLoadoutItem> m_aItems;

	//------------------------------------------------------------------------------------------------
	void ReadFrom(notnull JOLP_PlayerLoadout loadout)
	{
		m_sIdentity = loadout.m_sIdentity;
		m_sName = loadout.m_sName;
		m_aQuickSlots = new array<string>();
		m_aItems = new array<ref JOLP_PersistedLoadoutItem>();

		if (loadout.m_aQuickSlots)
		{
			foreach (string slot : loadout.m_aQuickSlots)
				m_aQuickSlots.Insert(slot);
		}

		if (loadout.m_aItems)
		{
			foreach (JOLP_LoadoutItem item : loadout.m_aItems)
			{
				if (!item)
					continue;

				JOLP_PersistedLoadoutItem record = new JOLP_PersistedLoadoutItem();
				record.ReadFrom(item);
				m_aItems.Insert(record);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	JOLP_PlayerLoadout Build()
	{
		JOLP_PlayerLoadout loadout = new JOLP_PlayerLoadout();
		loadout.m_sIdentity = m_sIdentity;
		loadout.m_sName = m_sName;

		if (m_aQuickSlots)
		{
			foreach (string slot : m_aQuickSlots)
				loadout.m_aQuickSlots.Insert(slot);
		}

		if (m_aItems)
		{
			foreach (JOLP_PersistedLoadoutItem item : m_aItems)
			{
				if (!item)
					continue;

				loadout.m_aItems.Insert(item.Build());
			}
		}

		return loadout;
	}

	//------------------------------------------------------------------------------------------------
	bool SerializationSave(SaveContext context)
	{
		context.Write(m_sIdentity);
		context.Write(m_sName);
		context.Write(m_aQuickSlots);
		context.Write(m_aItems);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool SerializationLoad(LoadContext context)
	{
		m_aQuickSlots = new array<string>();
		m_aItems = new array<ref JOLP_PersistedLoadoutItem>();
		context.Read(m_sIdentity);
		context.Read(m_sName);
		context.Read(m_aQuickSlots);
		context.Read(m_aItems);
		return true;
	}
}
