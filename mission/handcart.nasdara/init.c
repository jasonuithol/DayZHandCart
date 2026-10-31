// Hand truck test mission on Nasdara, for furniture taken off the map: players spawn
// holding a hand truck by an old sofa (one of Nasdara's loose map pieces) next to a
// rebuildable house, with tables and shelves around the town.
//
// Started with -missiontest it runs headless checks, logs [HandCartTest] lines and shuts
// the server down: the sofa is taken onto a truck, hidden from the map and remembered.
// A second run on the same persistence (KEEP=1 tools/test.sh) checks it stays gone.

void main()
{
	Hive ce = CreateHive();
	if (ce)
		ce.InitOffline();

	GetGame().GetWorld().SetDate(2026, 9, 20, 10, 0);
}

class CustomMission: MissionServer
{
	static const vector SOFA = "9343.71 162.79 4800.12";
	static const string SOFA_MODEL = "dz\\structures\\furniture\\chairs\\sofa_leather\\brown_sofa_old.p3d";

	protected int m_Failures;

	override void OnInit()
	{
		super.OnInit();
		Print("[HandCart] test site ready");

		string value;
		if (GetGame().CommandlineGetParam("missiontest", value))
			GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(TestStart, 5000, false);
	}

	override void StartingEquipSetup(PlayerBase player, bool clothesChosen)
	{
		vector pos = SOFA + "2.5 0 0";
		pos[1] = GetGame().SurfaceRoadY(pos[0], pos[2]);
		player.SetPosition(pos);
		player.SetDirection("-1 0 0");
		player.GetHumanInventory().CreateInHands("HandCart_HandTruck_Blue");
	}

	void Check(bool ok, string what)
	{
		if (ok)
			Print("[HandCartTest] ok   " + what);
		else
		{
			Print("[HandCartTest] FAIL " + what);
			m_Failures++;
		}
	}

	void TestStart()
	{
		Object sofa = HandTruckFurniture.FindPiece(SOFA_MODEL, SOFA);
		if (!sofa)
		{
			Check(true, "the sofa is already gone (a run on kept persistence)");
			Finish();
			return;
		}
		Check(HandTruckFurniture.ClassFor(sofa) == "HandCart_Furniture_Sofa", "the map's sofa is takeable as HandCart_Furniture_Sofa");

		vector at = SOFA + "2 0 0";
		at[1] = GetGame().SurfaceRoadY(at[0], at[2]);
		PlayerBase dummy = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorM_Mirek", at, 0, "NONE"));
		HandCart_HandTruck_Base truck = HandCart_HandTruck_Base.Cast(dummy.GetHumanInventory().CreateInHands("HandCart_HandTruck_Blue"));
		Check(truck != null, "test survivor holding a hand truck");
		if (!truck)
		{
			Finish();
			return;
		}

		Check(HandTruckFurniture.TakeServer(truck, sofa), "sofa taken");
		EntityAI item = HandTruckLoad.GetTopItem(truck);
		Check(item && item.IsKindOf("HandCart_Furniture_Sofa"), "a sofa item is on the truck");
		Check(!HandTruckFurniture.FindPiece(SOFA_MODEL, SOFA), "the map's sofa is gone");
		Check(!HandTruckFurniture.TakeServer(truck, sofa), "it can't be taken twice");

		Object table = FindNear("table_dz.p3d", "10646.26 165.02 4599.21");
		Check(table != null && HandTruckFurniture.ClassFor(table) == "HandCart_Furniture_Table", "the map's tables are takeable too");

		// Set the sofa down beside the house, then the furniture rules
		vector spot = SOFA + "0 0 3";
		spot[1] = GetGame().SurfaceRoadY(spot[0], spot[2]);
		Check(HandTruckRules.UnloadServer(truck, item, spot, "0 0 0") && !item.GetHierarchyParent(), "sofa set down");
		Check(item.CanPutIntoHands(dummy) && !dummy.CanReceiveItemIntoHands(item), "sofa passes the drag check, but never goes into hands");
		Check(HandTruckRules.CanLoad(truck, item), "the sofa item loads again");
		Finish();
	}

	Object FindNear(string modelEnd, vector pos)
	{
		array<Object> objects = new array<Object>;
		GetGame().GetObjectsAtPosition(pos, 1, objects, null);
		for (int i = 0; i < objects.Count(); i++)
		{
			string model = objects[i].GetShapeName();
			model.ToLower();
			if (model.Contains(modelEnd))
				return objects[i];
		}
		return null;
	}

	void Finish()
	{
		Print("[HandCartTest] done, " + m_Failures + " failures");
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(GetGame().RequestExit, 1000, false, 0);
	}
};

Mission CreateCustomMission(string path)
{
	return new CustomMission();
}
