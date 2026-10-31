//! Holding a hand truck and looking at something it can carry: F loads it. Vanilla's Take
//! would otherwise claim the prompt.
modded class ItemBase
{
	override void SetActions()
	{
		super.SetActions();

		AddAction(ActionHandTruckLoad);
	}
}

modded class ActionTakeItem
{
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (HandTruckRules.IsHandTruck(item) && HandTruckRules.CanLoad(item, EntityAI.Cast(target.GetObject())))
			return false;
		return super.ActionCondition(player, target, item);
	}
}

modded class ActionTakeItemToHands
{
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (HandTruckRules.IsHandTruck(item) && HandTruckRules.CanLoad(item, EntityAI.Cast(target.GetObject())))
			return false;
		return super.ActionCondition(player, target, item);
	}
}

modded class ActionSwapItemToHands
{
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (HandTruckRules.IsHandTruck(item) && HandTruckRules.CanLoad(item, EntityAI.Cast(target.GetObject())))
			return false;
		return super.ActionCondition(player, target, item);
	}
}
