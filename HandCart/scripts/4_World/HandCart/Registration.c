modded class ActionConstructor
{
	override void RegisterActions(TTypenameArray actions)
	{
		super.RegisterActions(actions);

		actions.Insert(ActionHandTruckLoad);
		actions.Insert(ActionHandTruckUnload);
		actions.Insert(ActionHandTruckCancel);
		actions.Insert(ActionHandTruckPlace);
		actions.Insert(ActionHandTruckLoadFromVehicle);
		actions.Insert(ActionHandTruckUnloadIntoVehicle);
		actions.Insert(ActionHandTruckLoadPerson);
		actions.Insert(ActionHandTruckUnloadPerson);
	}
}

modded class ModItemRegisterCallbacks
{
	override void RegisterHeavy(DayZPlayerType pType, DayzPlayerItemBehaviorCfg pBehavior)
	{
		super.RegisterHeavy(pType, pBehavior);

		// Heavy two-handed hold: the empty truck may run, but never sprint or jump; the
		// loaded limits are applied in HandTruckMovement.c
		DayzPlayerItemBehaviorCfg truckBehaviour = new DayzPlayerItemBehaviorCfg();
		truckBehaviour.SetHeavyItems();
		truckBehaviour.m_StanceMovements[DayZPlayerConstants.STANCEIDX_ERECT] = DayZPlayerConstants.MOVEMENT_IDLE | DayZPlayerConstants.MOVEMENT_WALK | DayZPlayerConstants.MOVEMENT_RUN;
		truckBehaviour.m_bJumpAllowed = false;

		// Borrowed from the power generator: both hands low in front, the nearest vanilla
		// pose to pushing a handle. The truck itself is hidden and drawn by HandTruckVisual.
		pType.AddItemInHandsProfileIK("HandCart_HandTruck_Base", "dz/anims/workspaces/player/player_main/props/player_main_heavy_generator.asi", truckBehaviour, "dz/anims/anm/player/ik/heavy/generator.anm");
	}
}

modded class PlayerBase
{
	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		super.OnRPC(sender, rpc_type, ctx);

		if (rpc_type == HandCartSettings.RPC_SYNC && !g_Game.IsDedicatedServer())
			HandCartSettings.Get().Read(ctx);
	}
}
