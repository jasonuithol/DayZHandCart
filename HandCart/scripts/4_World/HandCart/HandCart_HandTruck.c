//! The hand truck item. Colour variants (HandCart_HandTruck_Blue/_Red/_Yellow) are
//! config-only subclasses and pick up this script class automatically.
//! The load is carried as attachments (slots and proxies in config.cpp), so it is drawn
//! on the truck, saved with it, and keeps its own contents.
//! A tied-up player can ride it instead (HandTruckRider.c): the truck holds their network id.
class HandCart_HandTruck_Base : Inventory_Base
{
	static const float WATCH_INTERVAL = 500;   //!< ms between the server's checks on a rider

	protected ref HandTruckVisual m_Visual;   //!< client: the copy drawn while the truck is held
	protected int m_RiderLow;                 //!< network id of the person riding it, 0 0 if none
	protected int m_RiderHigh;
	protected bool m_HadRider;

	void HandCart_HandTruck_Base()
	{
		RegisterNetSyncVariableInt("m_RiderLow");
		RegisterNetSyncVariableInt("m_RiderHigh");
	}

	//! Carrying anything: a load, or a person
	bool IsLoaded()
	{
		return GetInventory().AttachmentCount() > 0 || HasRider();
	}

	bool HasRider()
	{
		return m_RiderLow != 0 || m_RiderHigh != 0;
	}

	PlayerBase GetRider()
	{
		if (!HasRider())
			return null;
		return PlayerBase.Cast(g_Game.GetObjectByNetworkId(m_RiderLow, m_RiderHigh));
	}

	//! The player holding it, if anyone
	PlayerBase GetPusher()
	{
		InventoryLocation loc = new InventoryLocation();
		if (!GetInventory().GetCurrentInventoryLocation(loc) || loc.GetType() != InventoryLocationType.HANDS)
			return null;
		return PlayerBase.Cast(loc.GetParent());
	}

	//! Server: set who rides it (null: nobody)
	void SetRiderServer(PlayerBase rider)
	{
		if (rider)
			rider.GetNetworkID(m_RiderLow, m_RiderHigh);
		else
		{
			m_RiderLow = 0;
			m_RiderHigh = 0;
		}
		SetSynchDirty();

		g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(WatchRider);
		if (rider)
			g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(WatchRider, WATCH_INTERVAL, true);
	}

	//! Server: let a rider off whenever riding stops being possible
	protected void WatchRider()
	{
		HandTruckRiders.CheckServer(this);
	}

	override void OnVariablesSynchronized()
	{
		super.OnVariablesSynchronized();

		bool hasRider = HasRider();
		if (hasRider != m_HadRider)
		{
			m_HadRider = hasRider;
			if (m_Visual)
				m_Visual.SyncLoad();
		}
	}

	//! Nowhere but hands or the ground while someone rides it
	override bool CanPutInCargo(EntityAI parent)
	{
		return !HasRider() && super.CanPutInCargo(parent);
	}

	override bool CanPutAsAttachment(EntityAI parent)
	{
		return !HasRider() && super.CanPutAsAttachment(parent);
	}

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

		// Out of the pusher's hands: the rider gets off where they stand
		if (g_Game.IsServer() && HasRider() && newLoc.GetType() != InventoryLocationType.HANDS)
			HandTruckRiders.ReleaseServer(this);

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
		if (g_Game.IsServer() && HasRider())
			HandTruckRiders.ReleaseServer(this);
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
		AddAction(ActionHandTruckUnloadPerson);
		AddAction(ActionHandTruckCancel);
		AddAction(ActionHandTruckPlace);
	}

	//! One kind of load at a time (see HandTruckLoad)
	override bool CanReceiveAttachment(EntityAI attachment, int slotId)
	{
		if (!super.CanReceiveAttachment(attachment, slotId) || HasRider())
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
