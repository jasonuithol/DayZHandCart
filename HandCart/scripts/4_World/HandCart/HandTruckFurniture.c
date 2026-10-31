//! A furniture piece taken off the map: which model, and where it stood.
class HandTruckTakenPiece
{
	string Model;
	vector Position;

	void HandTruckTakenPiece(string model = "", vector position = "0 0 0")
	{
		Model = model;
		Position = position;
	}
}

class HandTruckTakenList
{
	ref array<ref HandTruckTakenPiece> Pieces = new array<ref HandTruckTakenPiece>;
}

//! Furniture standing loose on the map (Nasdara places tables, sofas, shelves and so on as
//! map objects of their own; Chernarus's is built into the buildings) can be taken onto a
//! hand truck: the map object is hidden and a HandCart_Furniture_* item with the same model
//! takes its place. Map objects exist separately on the server and every client, so each
//! hides it itself: the server keeps the list of taken pieces (in the mission's storage,
//! so a wipe puts them all back), hides them at start-up, and sends the list to each
//! client as it connects and every new piece to everyone.
class HandTruckFurniture
{
	static const int RPC_HIDE = 48201732;
	static const string BASE = "HandCart_Furniture_Base";
	static const float HIDE_OFFSET = 10000;   //!< metres a hidden piece is moved away, as CF's HideMapObject does

	protected static ref map<string, string> s_ClassByModel;   //!< lower-case model path -> item class
	protected static ref HandTruckTakenList s_Taken;           //!< server

	//! The furniture item class for a map object, or "" if it isn't a loose furniture piece
	static string ClassFor(Object obj)
	{
		if (!IsMapPiece(obj))
			return "";

		if (!s_ClassByModel)
			InitClasses();
		string cls;
		s_ClassByModel.Find(NormalModel(obj.GetShapeName()), cls);
		return cls;
	}

	//! A plain map object (placed from a model with no config of its own), still in place
	static bool IsMapPiece(Object obj)
	{
		return obj && obj.GetType() == string.Empty && obj.Type() == Object && obj.GetPosition()[1] > -HIDE_OFFSET / 2;
	}

	protected static string NormalModel(string model)
	{
		model.ToLower();
		model.Replace("/", "\\");
		if (model.IndexOf("\\") == 0)
			model = model.Substring(1, model.Length() - 1);
		return model;
	}

	protected static void InitClasses()
	{
		s_ClassByModel = new map<string, string>;
		string name;
		int count = g_Game.ConfigGetChildrenCount("CfgVehicles");
		for (int i = 0; i < count; i++)
		{
			g_Game.ConfigGetChildName("CfgVehicles", i, name);
			if (name == BASE || !g_Game.IsKindOf(name, BASE) || g_Game.ConfigGetInt("CfgVehicles " + name + " scope") != 2)
				continue;
			s_ClassByModel.Set(NormalModel(g_Game.ConfigGetTextOut("CfgVehicles " + name + " model")), name);
		}
	}

	//! The map piece with this model standing at position, if it's still there
	static Object FindPiece(string model, vector position)
	{
		array<Object> objects = new array<Object>;
		g_Game.GetObjectsAtPosition(position, 0.5, objects, null);
		for (int i = 0; i < objects.Count(); i++)
		{
			if (IsMapPiece(objects[i]) && NormalModel(objects[i].GetShapeName()) == model)
				return objects[i];
		}
		return null;
	}

	//! Hide a map object on this machine only: no longer drawn, solid or simulated, and
	//! moved well out of the way
	static void Hide(Object obj)
	{
		vector position = obj.GetPosition();
		obj.ClearFlags(obj.GetFlags(), true);
		obj.ClearEventMask(obj.GetEventMask());

		vector transform[4];
		obj.GetTransform(transform);
		transform[3] = transform[3] - Vector(HIDE_OFFSET, HIDE_OFFSET, HIDE_OFFSET);
		obj.SetTransform(transform);
		obj.Update();

		if (obj.CanAffectPathgraph())
		{
			vector minMax[2];
			float radius = obj.ClippingInfo(minMax);
			vector extent = Vector(radius, radius, radius);
			g_Game.UpdatePathgraphRegion(position - extent, position + extent);
		}
	}

	//! Hide the piece with this model at position, if it's there
	static void HidePiece(string model, vector position)
	{
		Object obj = FindPiece(model, position);
		if (obj)
			Hide(obj);
	}

	// --- server -----------------------------------------------------------------------------

	protected static string TakenPath()
	{
		return "$mission:storage_" + g_Game.ServerConfigGetInt("instanceId") + "/handcart_taken_furniture.json";
	}

	//! Server start: load the list of taken pieces and hide them again
	static void LoadServer()
	{
		s_Taken = new HandTruckTakenList();
		string error;
		if (FileExist(TakenPath()) && !JsonFileLoader<HandTruckTakenList>.LoadFile(TakenPath(), s_Taken, error))
		{
			ErrorEx("[HandCart] " + error);
			s_Taken = new HandTruckTakenList();
		}

		for (int i = 0; i < s_Taken.Pieces.Count(); i++)
			HidePiece(s_Taken.Pieces[i].Model, s_Taken.Pieces[i].Position);
		if (s_Taken.Pieces.Count() > 0)
			Print("[HandCart] " + s_Taken.Pieces.Count() + " furniture pieces taken from the map");
	}

	//! Server: take a map piece onto truck. Its item is created and loaded first, so if it
	//! won't go on, the map is left as it was.
	static bool TakeServer(HandCart_HandTruck_Base truck, Object obj)
	{
		string cls = ClassFor(obj);
		if (cls == "")
			return false;

		EntityAI item = EntityAI.Cast(g_Game.CreateObjectEx(cls, obj.GetPosition(), ECE_NONE));
		if (!item)
			return false;
		if (!HandTruckRules.LoadServer(truck, item))
		{
			g_Game.ObjectDelete(item);
			return false;
		}

		HandTruckTakenPiece piece = new HandTruckTakenPiece(NormalModel(obj.GetShapeName()), obj.GetPosition());
		s_Taken.Pieces.Insert(piece);
		string error;
		MakeDirectory("$mission:storage_" + g_Game.ServerConfigGetInt("instanceId"));
		if (!JsonFileLoader<HandTruckTakenList>.SaveFile(TakenPath(), s_Taken, error))
			ErrorEx("[HandCart] " + error);

		Hide(obj);

		array<Man> players = new array<Man>;
		g_Game.GetPlayers(players);
		array<ref HandTruckTakenPiece> pieces = new array<ref HandTruckTakenPiece>;
		pieces.Insert(piece);
		for (int i = 0; i < players.Count(); i++)
			SendHide(players[i], pieces);
		return true;
	}

	//! Server: a client has connected, tell it everything taken so far
	static void SyncClient(Man player)
	{
		if (s_Taken && s_Taken.Pieces.Count() > 0)
			SendHide(player, s_Taken.Pieces);
	}

	protected static void SendHide(Man player, array<ref HandTruckTakenPiece> pieces)
	{
		if (!player || !player.GetIdentity())
			return;

		ScriptRPC rpc = new ScriptRPC();
		rpc.Write(pieces.Count());
		for (int i = 0; i < pieces.Count(); i++)
		{
			rpc.Write(pieces[i].Model);
			rpc.Write(pieces[i].Position);
		}
		rpc.Send(player, RPC_HIDE, true, player.GetIdentity());
	}

	// --- client -----------------------------------------------------------------------------

	static void ReadHide(ParamsReadContext ctx)
	{
		int count;
		if (!ctx.Read(count))
			return;
		for (int i = 0; i < count; i++)
		{
			string model;
			vector position;
			if (!ctx.Read(model) || !ctx.Read(position))
				return;
			HidePiece(model, position);
		}
	}
}

//! A furniture item: heavy, never in hands, and only into a vehicle's cargo. The server's
//! drag check asks whether an item could go into hands whatever the destination
//! (TryAcquireInventoryJunctureFromServer), so for someone holding a hand truck it passes
//! and the player's CanReceiveItemIntoHands keeps furniture out of hands.
class HandCart_Furniture_Base : Inventory_Base
{
	static const float LIFETIME = 3888000;   //!< 45 days, like a sea chest; it isn't in any types.xml

	override void EEInit()
	{
		super.EEInit();

		if (g_Game.IsServer())
		{
			SetLifetimeMax(LIFETIME);
			SetLifetime(LIFETIME);
		}
	}

	override void AfterStoreLoad()
	{
		super.AfterStoreLoad();

		SetLifetime(LIFETIME);
	}

	override bool CanPutIntoHands(EntityAI parent)
	{
		return HandTruckRules.HoldsHandTruck(parent);
	}

	override bool CanPutInCargo(EntityAI parent)
	{
		return Transport.Cast(parent) != null;
	}
}

modded class PlayerBase
{
	override bool CanReceiveItemIntoHands(EntityAI item_to_hands)
	{
		if (item_to_hands && item_to_hands.IsInherited(HandCart_Furniture_Base))
			return false;
		return super.CanReceiveItemIntoHands(item_to_hands);
	}

	override void SetActions(out TInputActionMap InputActionMap)
	{
		super.SetActions(InputActionMap);

		AddAction(ActionHandTruckTakeFurniture, InputActionMap);
	}

	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		super.OnRPC(sender, rpc_type, ctx);

		if (rpc_type == HandTruckFurniture.RPC_HIDE && !g_Game.IsDedicatedServer())
			HandTruckFurniture.ReadHide(ctx);
	}
}
