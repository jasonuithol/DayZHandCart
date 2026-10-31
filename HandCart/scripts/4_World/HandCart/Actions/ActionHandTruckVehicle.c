//! F at a vehicle while holding the hand truck: take a load out of it (its slots first,
//! like an MS3's barrels and crates, then its cargo) or put the top item into it.
//! Instant, like Load. The inventory screen does the same for a chosen item, by dragging
//! it onto the truck in hands.
class ActionHandTruckLoadFromVehicle : ActionSingleUseBase
{
	void ActionHandTruckLoadFromVehicle()
	{
		m_Text = "Load from vehicle";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINonRuined();
		m_ConditionTarget = new CCTObject(UAMaxDistances.DEFAULT);
	}

	override typename GetInputType()
	{
		return InteractActionInput;
	}

	override bool IsInstant()
	{
		return true;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!HandTruckRules.IsHandTruck(item))
			return false;
		if (!g_Game.IsDedicatedServer() && HandTruckPlacement.IsActive())
			return false;

		Transport vehicle = Transport.Cast(target.GetObject());
		if (!vehicle)
			return false;

		EntityAI load = HandTruckRules.FindVehicleLoad(item, vehicle);
		if (!load)
			return false;

		m_Text = "Load " + load.GetDisplayName();
		return true;
	}

	override void OnStartServer(ActionData action_data)
	{
		super.OnStartServer(action_data);

		Transport vehicle = Transport.Cast(action_data.m_Target.GetObject());
		if (!vehicle)
			return;

		EntityAI load = HandTruckRules.FindVehicleLoad(action_data.m_MainItem, vehicle);
		if (!load || !HandTruckRules.LoadServer(action_data.m_MainItem, load))
			NotificationSystem.SendNotificationToPlayerExtended(action_data.m_Player, 4, "Hand truck", "It won't go on");
	}
}

class ActionHandTruckUnloadIntoVehicle : ActionSingleUseBase
{
	void ActionHandTruckUnloadIntoVehicle()
	{
		m_Text = "Put in vehicle";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINonRuined();
		m_ConditionTarget = new CCTObject(UAMaxDistances.DEFAULT);
	}

	override typename GetInputType()
	{
		return InteractActionInput;
	}

	override bool IsInstant()
	{
		return true;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!HandTruckRules.IsHandTruck(item))
			return false;
		if (!g_Game.IsDedicatedServer() && HandTruckPlacement.IsActive())
			return false;

		Transport vehicle = Transport.Cast(target.GetObject());
		InventoryLocation dst;
		if (!vehicle || !HandTruckRules.FindVehicleSpot(item, vehicle, dst))
			return false;

		m_Text = "Put " + dst.GetItem().GetDisplayName() + " in vehicle";
		return true;
	}

	override void OnStartServer(ActionData action_data)
	{
		super.OnStartServer(action_data);

		HandCart_HandTruck_Base truck = HandCart_HandTruck_Base.Cast(action_data.m_MainItem);
		Transport vehicle = Transport.Cast(action_data.m_Target.GetObject());
		if (!truck || !vehicle || !HandTruckRules.UnloadIntoVehicleServer(truck, vehicle))
			NotificationSystem.SendNotificationToPlayerExtended(action_data.m_Player, 4, "Hand truck", "There's no room for it");
	}
}

modded class CarScript
{
	override void SetActions()
	{
		super.SetActions();

		AddAction(ActionHandTruckLoadFromVehicle);
		AddAction(ActionHandTruckUnloadIntoVehicle);
	}
}
