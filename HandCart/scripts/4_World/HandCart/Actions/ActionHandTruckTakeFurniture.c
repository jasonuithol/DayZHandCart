//! F at a loose furniture piece on the map while holding an empty hand truck: the piece
//! comes off the map and onto the truck (HandTruckFurniture). A player action, as map
//! objects have none of their own.
class ActionHandTruckTakeFurniture : ActionSingleUseBase
{
	void ActionHandTruckTakeFurniture()
	{
		m_Text = "Load";
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

		// Furniture goes on one piece at a time, on its own
		if (HandCart_HandTruck_Base.Cast(item).IsLoaded())
			return false;

		string cls = HandTruckFurniture.ClassFor(target.GetObject());
		if (cls == "")
			return false;

		m_Text = "Load " + g_Game.ConfigGetTextOut("CfgVehicles " + cls + " displayName");
		return true;
	}

	override void OnStartServer(ActionData action_data)
	{
		super.OnStartServer(action_data);

		HandCart_HandTruck_Base truck = HandCart_HandTruck_Base.Cast(action_data.m_MainItem);
		Object piece = action_data.m_Target.GetObject();
		if (!truck || truck.IsLoaded() || !HandTruckFurniture.TakeServer(truck, piece))
			NotificationSystem.SendNotificationToPlayerExtended(action_data.m_Player, 4, "Hand truck", "It won't go on");
	}
}
