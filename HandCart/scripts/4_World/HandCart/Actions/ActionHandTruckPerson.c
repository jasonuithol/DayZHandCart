//! Holding an empty hand truck and looking at a tied-up player (tape, rope, handcuffs): F
//! loads them onto it. F only offers the target's actions, so this is one of every player's
//! actions as a target (PlayerBase.SetActionsRemoteTarget, HandTruckRider.c).
class ActionHandTruckLoadPerson : ActionSingleUseBase
{
	void ActionHandTruckLoadPerson()
	{
		m_Text = "Load onto hand truck";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINonRuined();
		m_ConditionTarget = new CCTMan(UAMaxDistances.DEFAULT, false);
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
		HandCart_HandTruck_Base truck = HandCart_HandTruck_Base.Cast(item);
		if (!truck)
			return false;
		if (!g_Game.IsDedicatedServer() && HandTruckPlacement.IsActive())
			return false;
		string reason;
		return HandTruckRiders.CanCarry(player, truck, PlayerBase.Cast(target.GetObject()), reason);
	}

	override void OnStartServer(ActionData action_data)
	{
		super.OnStartServer(action_data);

		HandCart_HandTruck_Base truck = HandCart_HandTruck_Base.Cast(action_data.m_MainItem);
		PlayerBase person = PlayerBase.Cast(action_data.m_Target.GetObject());
		if (!HandTruckRiders.LoadServer(action_data.m_Player, truck, person))
			NotificationSystem.SendNotificationToPlayerExtended(action_data.m_Player, 4, "Hand truck", "They won't go on");
	}
}

//! Click (LMB) while a person rides the truck: sets them down where they stand on its nose
//! plate, if there's room and the pusher can see the spot.
class ActionHandTruckUnloadPerson : ActionSingleUseBase
{
	void ActionHandTruckUnloadPerson()
	{
		m_Text = "Unload person";
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

	override bool IsInstant()
	{
		return true;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		HandCart_HandTruck_Base truck = HandCart_HandTruck_Base.Cast(item);
		return truck && truck.HasRider();
	}

	override void OnStartServer(ActionData action_data)
	{
		super.OnStartServer(action_data);

		HandCart_HandTruck_Base truck = HandCart_HandTruck_Base.Cast(action_data.m_MainItem);
		string reason;
		if (truck && !HandTruckRiders.UnloadServer(action_data.m_Player, truck, reason) && reason != "")
			NotificationSystem.SendNotificationToPlayerExtended(action_data.m_Player, 4, "Hand truck", reason);
	}
}
