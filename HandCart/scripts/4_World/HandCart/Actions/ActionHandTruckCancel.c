//! Click (LMB) while unloading: drop the ghost and keep the item on the truck.
//! Paired with the hold-LMB Place action the same way vanilla pairs toggle/deploy.
class ActionHandTruckCancel : ActionSingleUseBase
{
	void ActionHandTruckCancel()
	{
		m_Text = "Cancel unload";
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

	//! While the spot is no good the prompt says why, since Place isn't offered
	override string GetText()
	{
		if (HandTruckPlacement.IsActive() && !HandTruckPlacement.Get().IsValid())
		{
			string reason = HandTruckPlacement.Get().GetReason();
			if (reason != "")
				return m_Text + " (" + reason + ")";
		}
		return m_Text;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		return HandTruckRules.IsHandTruck(item) && HandTruckPlacement.IsActive();
	}

	override void Start(ActionData action_data)
	{
		super.Start(action_data);

		if (!g_Game.IsDedicatedServer())
			HandTruckPlacement.Get().Stop();
	}
}
