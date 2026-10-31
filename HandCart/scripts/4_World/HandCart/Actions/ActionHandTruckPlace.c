class HandTruckPlaceActionData : ActionData
{
	int m_TargetLow;
	int m_TargetHigh;
	vector m_Position;
	vector m_Orientation;
}

class HandTruckPlaceReciveData : ActionReciveData
{
	int m_TargetLow;
	int m_TargetHigh;
	vector m_Position;
	vector m_Orientation;
}

class ActionHandTruckPlaceCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousTime(HandCartSettings.Get().UnloadTime);
	}
}

//! Hold LMB while unloading with a valid ghost: the client sends the ghost's transform
//! and the item's network ID; the server re-validates and sets the real item down.
class ActionHandTruckPlace : ActionContinuousBase
{
	void ActionHandTruckPlace()
	{
		m_CallbackClass = ActionHandTruckPlaceCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_DEPLOY_HEAVY;
		m_FullBody = true;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT;
		m_SpecialtyWeight = UASoftSkillsWeight.PRECISE_LOW;
		m_Text = "Place";
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

	override ActionData CreateActionData()
	{
		return new HandTruckPlaceActionData();
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!HandTruckRules.IsHandTruck(item))
			return false;

		// The server has no ghost; it validates the received spot when the action completes
		if (g_Game.IsDedicatedServer())
			return true;

		return HandTruckPlacement.IsActive() && HandTruckPlacement.Get().IsValid();
	}

	override bool ActionConditionContinue(ActionData action_data)
	{
		if (g_Game.IsDedicatedServer())
			return true;

		return HandTruckPlacement.IsActive();
	}

	override bool SetupAction(PlayerBase player, ActionTarget target, ItemBase item, out ActionData action_data, Param extra_data = NULL)
	{
		if (!super.SetupAction(player, target, item, action_data, extra_data))
			return false;

		if (!g_Game.IsDedicatedServer())
		{
			if (!HandTruckPlacement.IsActive())
				return false;

			HandTruckPlacement placement = HandTruckPlacement.Get();
			HandTruckPlaceActionData data = HandTruckPlaceActionData.Cast(action_data);
			placement.GetTarget().GetNetworkID(data.m_TargetLow, data.m_TargetHigh);
			data.m_Position = placement.GetPosition();
			data.m_Orientation = placement.GetOrientation();
			placement.SetFrozen(true);
		}
		return true;
	}

	override void WriteToContext(ParamsWriteContext ctx, ActionData action_data)
	{
		super.WriteToContext(ctx, action_data);

		HandTruckPlaceActionData data = HandTruckPlaceActionData.Cast(action_data);
		ctx.Write(data.m_TargetLow);
		ctx.Write(data.m_TargetHigh);
		ctx.Write(data.m_Position);
		ctx.Write(data.m_Orientation);
	}

	override bool ReadFromContext(ParamsReadContext ctx, out ActionReciveData action_recive_data)
	{
		if (!action_recive_data)
			action_recive_data = new HandTruckPlaceReciveData();

		if (!super.ReadFromContext(ctx, action_recive_data))
			return false;

		HandTruckPlaceReciveData data = HandTruckPlaceReciveData.Cast(action_recive_data);
		if (!ctx.Read(data.m_TargetLow) || !ctx.Read(data.m_TargetHigh))
			return false;
		return ctx.Read(data.m_Position) && ctx.Read(data.m_Orientation);
	}

	override void HandleReciveData(ActionReciveData action_recive_data, ActionData action_data)
	{
		super.HandleReciveData(action_recive_data, action_data);

		HandTruckPlaceReciveData received = HandTruckPlaceReciveData.Cast(action_recive_data);
		HandTruckPlaceActionData data = HandTruckPlaceActionData.Cast(action_data);
		data.m_TargetLow = received.m_TargetLow;
		data.m_TargetHigh = received.m_TargetHigh;
		data.m_Position = received.m_Position;
		data.m_Orientation = received.m_Orientation;
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		HandTruckPlaceActionData data = HandTruckPlaceActionData.Cast(action_data);
		PlayerBase player = action_data.m_Player;
		EntityAI target = EntityAI.Cast(g_Game.GetObjectByNetworkId(data.m_TargetLow, data.m_TargetHigh));

		HandCart_HandTruck_Base truck = HandCart_HandTruck_Base.Cast(action_data.m_MainItem);

		// The item must be on the truck this player is holding
		if (!truck || !target || target.GetHierarchyParent() != truck)
		{
			Print("[HandCart] " + player + " placed an item that isn't on their truck: " + target);
			return;
		}

		string reason;
		if (!HandTruckRules.CheckSpot(player, target, data.m_Position, data.m_Orientation, null, reason))
		{
			NotificationSystem.SendNotificationToPlayerExtended(player, 4, "Hand truck", reason);
			return;
		}

		if (!HandTruckRules.UnloadServer(truck, target, data.m_Position, data.m_Orientation))
			NotificationSystem.SendNotificationToPlayerExtended(player, 4, "Hand truck", "It won't come off");
	}

	override void OnFinishProgressClient(ActionData action_data)
	{
		HandTruckPlacement.Get().Stop();
	}

	override void OnEndClient(ActionData action_data)
	{
		super.OnEndClient(action_data);

		// Interrupted before finishing: let the ghost follow the aim again
		if (HandTruckPlacement.IsActive())
			HandTruckPlacement.Get().SetFrozen(false);
	}
}
