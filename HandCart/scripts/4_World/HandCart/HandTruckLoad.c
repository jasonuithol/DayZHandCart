//! One kind of load the truck can carry, as a stack of attachment slots filled bottom-up,
//! or (furniture) a slot per item of which only m_Max are used at once.
class HandTruckLoadKind
{
	string m_Name;
	ref array<int> m_Slots = new array<int>;
	int m_Max;   //!< how many at once; the slot count unless set lower

	//! count 1: a single slot named slotName; otherwise slotName1..slotNameN
	void HandTruckLoadKind(string name, string slotName, int count)
	{
		m_Name = name;
		m_Max = count;
		if (count == 1)
		{
			m_Slots.Insert(InventorySlots.GetSlotIdFromString(slotName));
			return;
		}
		for (int i = 1; i <= count; i++)
			m_Slots.Insert(InventorySlots.GetSlotIdFromString(slotName + i.ToString()));
	}

	//! A kind whose items each have their own slot (slotPrefix + suffix), max at once
	static HandTruckLoadKind OfSlots(string name, string slotPrefix, TStringArray suffixes, int max)
	{
		HandTruckLoadKind kind = new HandTruckLoadKind(name, slotPrefix + suffixes[0], 1);
		for (int i = 1; i < suffixes.Count(); i++)
			kind.m_Slots.Insert(InventorySlots.GetSlotIdFromString(slotPrefix + suffixes[i]));
		kind.m_Max = max;
		return kind;
	}

	bool HasSlot(int slotId)
	{
		return m_Slots.Find(slotId) >= 0;
	}
}

//! What can go on the truck, and in what combinations. An item belongs to a kind if its
//! config lists one of that kind's slots in inventorySlot[] (see config.cpp), so another
//! mod's item becomes loadable just by adding the slot names to its config.
//! The truck carries one kind at a time, up to that kind's slot count.
class HandTruckLoad
{
	protected static ref array<ref HandTruckLoadKind> s_Kinds;

	static array<ref HandTruckLoadKind> GetKinds()
	{
		if (!s_Kinds)
		{
			s_Kinds = new array<ref HandTruckLoadKind>;
			s_Kinds.Insert(new HandTruckLoadKind("Safe", "HandCart_Safe", 1));
			s_Kinds.Insert(new HandTruckLoadKind("SeaChest", "HandCart_SeaChest", 2));
			s_Kinds.Insert(new HandTruckLoadKind("Crate", "HandCart_Crate", 4));
			s_Kinds.Insert(new HandTruckLoadKind("DualWheel", "HandCart_DualWheel", 1));
			s_Kinds.Insert(new HandTruckLoadKind("TruckWheel", "HandCart_TruckWheel", 2));
			s_Kinds.Insert(new HandTruckLoadKind("CarWheel", "HandCart_CarWheel", 3));
			s_Kinds.Insert(new HandTruckLoadKind("Generator", "HandCart_Generator", 1));
			s_Kinds.Insert(new HandTruckLoadKind("CanopyTent", "HandCart_CanopyTent", 3));
			s_Kinds.Insert(new HandTruckLoadKind("Barrel", "HandCart_Barrel", 1));
			s_Kinds.Insert(new HandTruckLoadKind("Stove", "HandCart_Stove", 1));
			s_Kinds.Insert(new HandTruckLoadKind("SewingMachine", "HandCart_SewingMachine", 1));
			TStringArray furniture = {"Sofa", "Table", "OfficeDesk", "MedicalTable", "Shelf", "WorkbenchOld", "Wardrobe", "Cupboard",
				"StretcherGreen", "StretcherRed", "Fridge", "TV", "PC"};
			s_Kinds.Insert(HandTruckLoadKind.OfSlots("Furniture", "HandCart_Furniture_", furniture, 1));
		}
		return s_Kinds;
	}

	//! The kind an item can be loaded as, from the slots its config allows; null if none.
	static HandTruckLoadKind GetKindOf(EntityAI item)
	{
		// Index loops throughout: foreach over a function result or a member is unreliable
		array<string> blocked = HandCartSettings.Get().BlockedItems;
		for (int b = 0; b < blocked.Count(); b++)
		{
			if (item.IsKindOf(blocked[b]))
				return null;
		}

		// Tents only travel packed
		TentBase tent = TentBase.Cast(item);
		if (tent && tent.GetState() != TentBase.PACKED)
			return null;

		GameInventory inv = item.GetInventory();
		for (int i = 0; i < inv.GetSlotIdCount(); i++)
		{
			HandTruckLoadKind kind = GetKindOfSlot(inv.GetSlotId(i));
			if (kind)
				return kind;
		}
		return null;
	}

	static HandTruckLoadKind GetKindOfSlot(int slotId)
	{
		array<ref HandTruckLoadKind> kinds = GetKinds();
		for (int i = 0; i < kinds.Count(); i++)
		{
			if (kinds[i].HasSlot(slotId))
				return kinds[i];
		}
		return null;
	}

	//! The kind currently on the truck; null when empty.
	static HandTruckLoadKind GetLoadedKind(EntityAI truck)
	{
		GameInventory inv = truck.GetInventory();
		for (int i = 0; i < inv.AttachmentCount(); i++)
		{
			InventoryLocation loc = new InventoryLocation();
			inv.GetAttachmentFromIndex(i).GetInventory().GetCurrentInventoryLocation(loc);
			HandTruckLoadKind kind = GetKindOfSlot(loc.GetSlot());
			if (kind)
				return kind;
		}
		return null;
	}

	//! Rule check used by the truck's CanReceiveAttachment: right kind for the slot, the
	//! same kind as anything already loaded, and room for another.
	static bool Accepts(EntityAI truck, EntityAI item, int slotId)
	{
		HandTruckLoadKind kind = GetKindOf(item);
		if (!kind || !kind.HasSlot(slotId))
			return false;

		HandTruckLoadKind loaded = GetLoadedKind(truck);
		if (!loaded)
			return true;
		return loaded == kind && truck.GetInventory().AttachmentCount() < kind.m_Max;
	}

	//! Lowest free slot item can go into, or InventorySlots.INVALID if it can't be loaded
	//! right now. Slot ids are name hashes and can be negative: never test them with < 0.
	static int FindFreeSlot(EntityAI truck, EntityAI item)
	{
		HandTruckLoadKind kind = GetKindOf(item);
		if (!kind)
			return InventorySlots.INVALID;

		GameInventory inv = truck.GetInventory();
		if (GetLoadedKind(truck) == kind && inv.AttachmentCount() >= kind.m_Max)
			return InventorySlots.INVALID;
		for (int i = 0; i < kind.m_Slots.Count(); i++)
		{
			int slotId = kind.m_Slots[i];
			if (!inv.FindAttachment(slotId) && inv.CanAddAttachmentEx(item, slotId))
				return slotId;
		}
		return InventorySlots.INVALID;
	}

	//! Top of the stack: the item to unload next (last in, first out).
	static EntityAI GetTopItem(EntityAI truck)
	{
		HandTruckLoadKind kind = GetLoadedKind(truck);
		if (!kind)
			return null;

		GameInventory inv = truck.GetInventory();
		for (int i = kind.m_Slots.Count() - 1; i >= 0; i--)
		{
			EntityAI item = inv.FindAttachment(kind.m_Slots[i]);
			if (item)
				return item;
		}
		return null;
	}
}
