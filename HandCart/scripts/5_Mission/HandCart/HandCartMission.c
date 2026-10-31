modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();
		HandCartSettings.LoadServer();
		HandTruckFurniture.LoadServer();
	}

	override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
	{
		super.InvokeOnConnect(player, identity);

		ScriptRPC rpc = new ScriptRPC();
		HandCartSettings.Get().Write(rpc);
		rpc.Send(player, HandCartSettings.RPC_SYNC, true, identity);
		HandTruckFurniture.SyncClient(player);
	}
}

modded class MissionGameplay
{
	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		if (HandTruckPlacement.IsActive())
			HandTruckPlacement.Get().Update(PlayerBase.Cast(g_Game.GetPlayer()), timeslice);
	}

	override void OnMissionFinish()
	{
		if (HandTruckPlacement.IsActive())
			HandTruckPlacement.Get().Stop();

		super.OnMissionFinish();
	}
}
