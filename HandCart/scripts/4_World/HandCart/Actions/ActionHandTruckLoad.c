//! Press F while holding the hand truck and looking at something it can carry: loads it
//! onto the truck as an attachment, contents and all, at once (the same as dragging it
//! onto the truck; a timed, animated load kept aborting). F only offers the target's
//! actions, so this is added to every item (HandTruckTakeActions.c) and checks the truck
//! in hands.
class ActionHandTruckLoad : ActionSingleUseBase
{
	void ActionHandTruckLoad()
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

		return HandTruckRules.CanLoad(item, EntityAI.Cast(target.GetObject()));
	}

	override void OnStartServer(ActionData action_data)
	{
		super.OnStartServer(action_data);

		EntityAI truck = action_data.m_MainItem;
		EntityAI target = EntityAI.Cast(action_data.m_Target.GetObject());
		if (!target || !HandTruckRules.CanLoad(truck, target))
			return;

		if (!HandTruckRules.LoadServer(truck, target))
			NotificationSystem.SendNotificationToPlayerExtended(action_data.m_Player, 4, "Hand truck", "It won't go on");
	}
}
