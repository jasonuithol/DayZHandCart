//! Click (LMB) while holding a loaded hand truck: enters unload mode with a ghost of the
//! item on top of the load. Client-only, like vanilla's ActionTogglePlaceObject; nothing
//! happens on the server until the Place action.
class ActionHandTruckUnload : ActionSingleUseBase
{
	void ActionHandTruckUnload()
	{
		m_Text = "Unload";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINonRuined();
		m_ConditionTarget = new CCTNone();
	}

	override bool HasTarget()
	{
		return false;
	}

	override bool IsLocal()
	{
		return true;
	}

	override bool IsInstant()
	{
		return true;
	}

	override bool RemoveForceTargetAfterUse()
	{
		return false;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!HandTruckRules.IsHandTruck(item) || HandTruckPlacement.IsActive())
			return false;

		return HandTruckLoad.GetTopItem(item) != null;
	}

	override void Start(ActionData action_data)
	{
		super.Start(action_data);

		if (!g_Game.IsDedicatedServer())
			HandTruckPlacement.Get().Begin(action_data.m_Player, action_data.m_MainItem);
	}
}
