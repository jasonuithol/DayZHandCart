//! The hand truck item. Colour variants (HandCart_HandTruck_Blue/_Red/_Yellow) are
//! config-only subclasses and pick up this script class automatically.
//! The load is carried as attachments (slots and proxies in config.cpp), so it is drawn
//! on the truck, saved with it, and keeps its own contents.
class HandCart_HandTruck_Base : Inventory_Base
{
	bool IsLoaded()
	{
		return GetInventory().AttachmentCount() > 0;
	}

	protected ref HandTruckVisual m_Visual;   //!< client: the copy drawn while the truck is held

	override void EEInit()
	{
		super.EEInit();

		// A truck that spawns already in hands (character loaded from storage, or a
		// remote player's truck streaming in) gets no location change
		UpdateVisual();
	}

	override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
	{
		super.EEItemLocationChanged(oldLoc, newLoc);

		UpdateVisual();
	}

	override void EEItemAttached(EntityAI item, string slot_name)
	{
		super.EEItemAttached(item, slot_name);

		if (m_Visual)
			m_Visual.SyncLoad();
	}

	override void EEItemDetached(EntityAI item, string slot_name)
	{
		super.EEItemDetached(item, slot_name);

		// Hidden while it rode on a held truck (HandTruckVisual)
		if (!g_Game.IsDedicatedServer())
		{
			HandTruckVisual.SetLoadInvisible(item, false);
			item.SetInvisible(false);
		}

		if (m_Visual)
			m_Visual.SyncLoad();
	}

	override void EEHealthLevelChanged(int oldLevel, int newLevel, string zone)
	{
		super.EEHealthLevelChanged(oldLevel, newLevel, zone);

		if (m_Visual)
			m_Visual.SyncHealth();
	}

	override void EEDelete(EntityAI parent)
	{
		m_Visual = null;
		super.EEDelete(parent);
	}

	//! Client: while in someone's hands the truck itself is hidden and HandTruckVisual draws
	//! a copy stood in front of them (a held item hangs off the hand bone at an angle that
	//! no model trick turned into a believable push)
	void UpdateVisual()
	{
		if (g_Game.IsDedicatedServer())
			return;

		InventoryLocation loc = new InventoryLocation();
		bool held = GetInventory().GetCurrentInventoryLocation(loc) && loc.GetType() == InventoryLocationType.HANDS;
		PlayerBase player = PlayerBase.Cast(loc.GetParent());
		if (held && player)
		{
			if (!m_Visual || m_Visual.GetPlayer() != player)
				m_Visual = new HandTruckVisual(this, player);
		}
		else
		{
			m_Visual = null;
		}
	}

	override void SetActions()
	{
		super.SetActions();

		AddAction(ActionHandTruckUnload);
		AddAction(ActionHandTruckCancel);
		AddAction(ActionHandTruckPlace);
	}

	//! One kind of load at a time (see HandTruckLoad)
	override bool CanReceiveAttachment(EntityAI attachment, int slotId)
	{
		if (!super.CanReceiveAttachment(attachment, slotId))
			return false;

		return HandTruckLoad.Accepts(this, attachment, slotId);
	}

	//! Only show slots that hold something (drag a load out of one to unload it there)
	override bool CanDisplayAttachmentSlot(int slot_id)
	{
		if (!super.CanDisplayAttachmentSlot(slot_id))
			return false;

		return GetInventory().FindAttachment(slot_id) != null;
	}
}
