// Hand truck test bed for the HandCart mod (~/Projects/DayZHandCart).
// Players spawn holding a blue hand truck on open, flat, empty ground near Balota (found
// at start: FindOpenGround). Around them: the red and yellow trucks and one of every load -
// safes (Expansion), sea chests (one with things in it), wooden crates, a dual MS3 wheel,
// MS3 and M1025 wheels and car wheels - a loaded MS3 and two handcuffed survivors, so
// loading, steering and unloading can all be tried in one place.
// -missiontest: loads and unloads a truck headless, logs [HandCartTest] lines, then quits.
#include "lib/RoadFinder.c"

void main()
{
	Hive ce = CreateHive();
	if (ce)
		ce.InitOffline();

	GetGame().GetWorld().SetDate(2026, 9, 20, 10, 0);
}

class CustomMission: MissionServer
{
	static const vector ANCHOR = "4810 0 2440";

	protected vector m_Road;
	protected vector m_Along;     //!< unit vector along the road
	protected vector m_Across;    //!< unit vector to the verge
	protected float m_Width;
	protected bool m_Ready;

	override void OnInit()
	{
		super.OnInit();
		FindSite();
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(Setup, 3000, false);

		string value;
		if (GetGame().CommandlineGetParam("missiontest", value))
			GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(TestStart, 8000, false);
	}

	void FindSite()
	{
		float heading;
		if (!RoadFinder.Locate(ANCHOR, 600, m_Road, heading, m_Width))
		{
			Print("[HandCart] no road near " + ANCHOR);
			m_Road = RoadFinder.OnSurface(ANCHOR[0], ANCHOR[2]);
			m_Width = 6;
		}
		m_Along = RoadFinder.HeadingToDir(heading);
		m_Across = Vector(m_Along[2], 0, -m_Along[0]);
		Print("[HandCart] site " + m_Road + " heading " + heading + " width " + m_Width);
	}

	//! Spot on the verge: `off` metres off the road edge, `along` metres down the road
	protected EntityAI m_MS3;      //!< on the docks, with full containers in its slots and cargo
	protected vector m_Dock;       //!< where it's parked
	protected vector m_DockDir;    //!< which way it faces
	protected EntityAI m_Generator;
	protected ref array<EntityAI> m_Tents;

	//! Spot in the load layout, to the right of the players' spawn: `off` metres to the
	//! right, `along` metres ahead
	vector Lay(float off, float along)
	{
		vector spawn = m_Dock - m_DockDir * 7;
		vector side = Vector(m_DockDir[2], 0, -m_DockDir[0]);
		vector p = spawn + side * (5 + off) + m_DockDir * (along - 4);
		p[1] = GetGame().SurfaceY(p[0], p[2]);
		return p;
	}

	vector Verge(float off, float along)
	{
		vector p = m_Road + m_Across * (m_Width / 2 + off) + m_Along * along;
		p[1] = GetGame().SurfaceY(p[0], p[2]);
		return p;
	}

	EntityAI Drop(string type, vector pos)
	{
		EntityAI item = EntityAI.Cast(GetGame().CreateObjectEx(type, pos, ECE_PLACE_ON_SURFACE));
		if (!item)
		{
			Print("[HandCart] could not create " + type);
			return null;
		}
		vector dir = m_Across * -1;   // facing the road
		item.SetDirection(dir);
		return item;
	}

	//! Open ground near ANCHOR for the whole test site: within 0.6 m of level over a 56 m
	//! square and nothing standing (trees, bushes, buildings, fences, lights) within 30 m.
	//! The flattest, nearest such spot wins. m_Dock is where the MS3 parks, 7 m ahead of the
	//! players' spawn; the site's middle is 2 m behind it.
	bool FindOpenGround()
	{
		array<vector> spots = new array<vector>;
		array<float> scores = new array<float>;
		for (float x = -600; x <= 600; x += 20)
		{
			for (float z = -600; z <= 600; z += 20)
			{
				vector c = Vector(ANCHOR[0] + x, 0, ANCHOR[2] + z);
				float range = HeightRange(c, 28, 7);
				if (range > 0.6)
					continue;
				spots.Insert(c);
				scores.Insert(range + vector.Distance(c, ANCHOR) / 1000);
			}
		}

		for (int tries = 0; tries < 300 && spots.Count() > 0; tries++)
		{
			int best = 0;
			for (int i = 1; i < scores.Count(); i++)
			{
				if (scores[i] < scores[best])
					best = i;
			}
			vector spot = spots[best];
			spots.Remove(best);
			scores.Remove(best);
			if (!IsOpen(spot, 30))
				continue;

			m_DockDir = m_Along;
			m_Dock = spot + m_DockDir * 2;
			m_Dock[1] = GetGame().SurfaceRoadY(m_Dock[0], m_Dock[2]);
			Print("[HandCart] open ground " + spot + " (" + tries + " tried), the MS3 at " + m_Dock);
			return true;
		}
		Print("[HandCart] no open ground near " + ANCHOR);
		return false;
	}

	//! Highest minus lowest ground over a square of half-width `half`, sampled n x n; 99 if
	//! any of it is water
	float HeightRange(vector c, float half, int n)
	{
		float low = 100000;
		float high = -100000;
		float step = half * 2 / (n - 1);
		for (int i = 0; i < n; i++)
		{
			for (int j = 0; j < n; j++)
			{
				float px = c[0] - half + i * step;
				float pz = c[2] - half + j * step;
				if (GetGame().SurfaceIsSea(px, pz) || GetGame().SurfaceIsPond(px, pz))
					return 99;
				float y = GetGame().SurfaceRoadY(px, pz);
				low = Math.Min(low, y);
				high = Math.Max(high, y);
			}
		}
		return high - low;
	}

	//! No object with a model within radius
	bool IsOpen(vector c, float radius)
	{
		array<Object> objects = new array<Object>;
		GetGame().GetObjectsAtPosition(c, radius, objects, null);
		for (int i = 0; i < objects.Count(); i++)
		{
			if (objects[i].GetShapeName() != "")
				return false;
		}
		return true;
	}

	//! A flat, clear stretch of the Chernogorsk port quay, beside the crane rails (if there's
	//! no open ground)
	void FindDock()
	{
		vector a = "7474 0 2677";
		vector b = "7670 0 3033";
		vector along = vector.Direction(a, b).Normalized();
		vector across = Vector(along[2], 0, -along[0]);
		for (float t = 0.1; t <= 0.9; t += 0.05)
		{
			for (float off = -16; off <= 16; off += 2)
			{
				vector c = a + (b - a) * t + across * off;
				float y = GetGame().SurfaceRoadY(c[0], c[2]);
				if (y < 3 || !IsFlat(c, y, along, across) || !IsClear(c))
					continue;
				c[1] = y;
				m_Dock = c;
				m_DockDir = along;
				Print("[HandCart] dock spot " + m_Dock);
				return;
			}
		}
		m_Dock = m_Road + m_Along * 14;
		m_DockDir = m_Along;
		Print("[HandCart] no clear spot on the docks, the MS3 is down the road at " + m_Dock);
	}

	//! Ground within 0.15 m of y under a 3 x 16 m strip (the MS3 plus room behind it)
	bool IsFlat(vector c, float y, vector along, vector across)
	{
		for (float l = -9; l <= 7; l += 2)
		{
			for (float w = -1.5; w <= 1.5; w += 1.5)
			{
				vector p = c + along * l + across * w;
				if (Math.AbsFloat(GetGame().SurfaceRoadY(p[0], p[2]) - y) > 0.15)
					return false;
			}
		}
		return true;
	}

	//! Nothing but the quay itself within 9 m
	bool IsClear(vector c)
	{
		array<Object> objects = new array<Object>;
		GetGame().GetObjectsAtPosition(c, 9, objects, null);
		for (int i = 0; i < objects.Count(); i++)
		{
			string model = objects[i].GetShapeName();
			model.ToLower();
			if (model != "" && !model.Contains("pier_concrete") && !model.Contains("pier_crane_rails"))
				return false;
		}
		return true;
	}

	//! The MS3, ready to drive, carrying full containers: barrels in two of its barrel
	//! slots, crates in two crate slots, a sea chest in its cargo. A full barrel and a full
	//! sea chest wait beside it.
	void SetupDocks()
	{
		m_MS3 = EntityAI.Cast(GetGame().CreateObjectEx("Truck_01_Covered", m_Dock, ECE_PLACE_ON_SURFACE));
		if (!m_MS3)
		{
			Print("[HandCart] could not create the MS3");
			return;
		}
		m_MS3.SetDirection(m_DockDir);
		// Vanilla gives vehicles a 3 s lifetime (its own vehicle events keep theirs alive),
		// so the economy's cleanup removed every one spawned here
		m_MS3.SetLifetimeMax(3888000);
		m_MS3.SetLifetime(3888000);
		CarScript.Cast(m_MS3).OnDebugSpawn();   // wheels, doors, battery, fluids

		GameInventory inv = m_MS3.GetInventory();
		for (int i = 1; i <= 2; i++)
		{
			Fill(inv.CreateAttachmentEx("Barrel_Blue", InventorySlots.GetSlotIdFromString("Truck_01_Barrel" + i)));
			Fill(inv.CreateAttachmentEx("WoodenCrate", InventorySlots.GetSlotIdFromString("Truck_01_WoodenCrate" + i)));
		}
		// Filled first: nothing can be created inside a container already in cargo
		EntityAI chest = Drop("SeaChest", m_Dock);
		Fill(chest);
		InventoryLocation src = new InventoryLocation();
		InventoryLocation dst = new InventoryLocation();
		chest.GetInventory().GetCurrentInventoryLocation(src);
		if (inv.FindFreeLocationFor(chest, FindInventoryLocationType.CARGO, dst))
			GameInventory.LocationSyncMoveEntity(src, dst);

		vector side = Vector(m_DockDir[2], 0, -m_DockDir[0]);
		Fill(Drop("Barrel_Green", m_Dock + side * 3 - m_DockDir * 5));
		Fill(Drop("SeaChest", m_Dock + side * 3 - m_DockDir * 7));
		Drop("Barrel_Red", m_Dock - side * 3 - m_DockDir * 5);   // empty

		// Life Machines' stove (with things in it) and sewing machine
		Fill(Drop("HM_Stove", m_Dock - side * 3 - m_DockDir * 8));
		Drop("HM_Sewing_Machine", m_Dock - side * 3 - m_DockDir * 10);
	}

	//! Put a few things in a container (one that only takes things while open, like a
	//! closed barrel or stove, is opened for it and closed again)
	void Fill(EntityAI container)
	{
		if (!container)
			return;
		ItemBase openable = ItemBase.Cast(container);
		if (openable && openable.IsOpen())
			openable = null;
		if (openable)
			openable.Open();
		container.GetInventory().CreateInInventory("Rice");
		container.GetInventory().CreateInInventory("Canteen");
		container.GetInventory().CreateInInventory("BakedBeansCan");
		if (openable)
			openable.Close();
	}

	void Setup()
	{
		if (!FindOpenGround())
			FindDock();

		// One row per kind of load, to the right of the spawn, running forward
		Drop("HandCart_HandTruck_Red", Lay(1.5, -6));
		Drop("HandCart_HandTruck_Yellow", Lay(1.5, -4.5));

		Drop("ExpansionSafeLarge", Lay(3, -2));
		Drop("ExpansionSafeMedium", Lay(5, -2));
		Drop("ExpansionSafeMini", Lay(6.5, -2));

		EntityAI chest = Drop("SeaChest", Lay(3, 1));
		Drop("SeaChest", Lay(5, 1));
		if (chest)
		{
			chest.GetInventory().CreateInInventory("AKM");
			chest.GetInventory().CreateInInventory("Mag_AKM_30Rnd");
			chest.GetInventory().CreateInInventory("Canteen");
		}

		for (int c = 0; c < 4; c++)
			Drop("WoodenCrate", Lay(3 + c * 1.2, 3.5));

		Drop("Truck_01_WheelDouble", Lay(3, 6));
		Drop("Truck_01_Wheel", Lay(5, 6));
		Drop("Truck_01_Wheel", Lay(6.5, 6));
		Drop("Offroad_02_Wheel", Lay(8, 6));

		Drop("PowerGenerator", Lay(3, 11));
		for (int t = 0; t < 4; t++)
			Drop("PartyTent", Lay(5 + t * 1.5, 11));

		Drop("HatchbackWheel", Lay(3, 8.5));
		Drop("CivSedanWheel", Lay(4.2, 8.5));
		Drop("Sedan_02_Wheel", Lay(5.4, 8.5));
		Drop("Hatchback_02_Wheel", Lay(6.6, 8.5));

		SetupDocks();
		SetupCaptives();

		m_Ready = true;
		Print("[HandCart] test site ready");
	}

	override void StartingEquipSetup(PlayerBase player, bool clothesChosen)
	{
		// At the back of the MS3, facing it
		vector pos = m_Dock - m_DockDir * 7;
		pos[1] = GetGame().SurfaceRoadY(pos[0], pos[2]);
		player.SetPosition(pos);
		player.SetDirection(m_DockDir);
		player.GetHumanInventory().CreateInHands("HandCart_HandTruck_Blue");
	}

	// --- -missiontest ----------------------------------------------------------------------

	protected PlayerBase m_Dummy;
	protected HandCart_HandTruck_Base m_Truck;
	protected ref array<EntityAI> m_Crates;
	protected EntityAI m_Chest;
	protected EntityAI m_Safe;
	protected int m_Failures;

	void Check(bool ok, string what)
	{
		if (ok)
		{
			Print("[HandCartTest] ok   " + what);
		}
		else
		{
			Print("[HandCartTest] FAIL " + what);
			m_Failures++;
		}
	}

	void TestStart()
	{
		string value;
		if (GetGame().CommandlineGetParam("boxes", value))
			LogBoxes();
		if (GetGame().CommandlineGetParam("docks", value))
			LogDocks();
		TestPose();
		TestRider();
		vector at = Verge(-m_Width / 2 - 1, -12);   // on the road, away from the layout
		m_Dummy = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorM_Mirek", at, 0, "NONE"));
		Check(m_Dummy != null, "test survivor created");
		if (!m_Dummy)
		{
			Finish();
			return;
		}

		m_Truck = HandCart_HandTruck_Base.Cast(m_Dummy.GetHumanInventory().CreateInHands("HandCart_HandTruck_Blue"));
		Check(m_Truck != null, "hand truck created in hands");
		if (!m_Truck)
		{
			Finish();
			return;
		}

		m_Crates = new array<EntityAI>();
		for (int i = 0; i < 5; i++)
			m_Crates.Insert(Drop("WoodenCrate", Verge(-m_Width / 2 - 1 + 1.5, -14 - i)));
		m_Chest = Drop("SeaChest", Verge(-m_Width / 2 - 1 + 1.5, -20));
		m_Safe = Drop("ExpansionSafeMini", Verge(-m_Width / 2 - 1 + 1.5, -22));
		m_Generator = Drop("PowerGenerator", Verge(-m_Width / 2 - 1 + 1.5, -24));
		m_Tents = new array<EntityAI>();
		for (int t = 0; t < 4; t++)
			m_Tents.Insert(Drop("PartyTent", Verge(-m_Width / 2 - 1 + 1.5, -26 - 1.5 * t)));
		if (m_Chest)
			m_Chest.GetInventory().CreateInInventory("Canteen");

		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(TestHeld, 1000, false);
	}

	// --- people on hand trucks --------------------------------------------------------------

	//! Two handcuffed survivors to the left of the players' spawn, to load onto a hand truck
	void SetupCaptives()
	{
		vector side = Vector(m_DockDir[2], 0, -m_DockDir[0]);
		for (int i = 0; i < 2; i++)
		{
			vector pos = m_Dock - m_DockDir * 7 - side * (6 + 1.5 * i);
			pos[1] = GetGame().SurfaceRoadY(pos[0], pos[2]);
			PlayerBase captive = Captive(pos);
			if (captive)
				captive.SetDirection(side);   // facing the spawn
		}
	}

	PlayerBase Captive(vector pos)
	{
		PlayerBase captive = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorF_Linda", pos, 0, "NONE"));
		if (!captive)
			return null;
		captive.GetInventory().CreateInInventory("TShirt_Red");
		captive.GetInventory().CreateInInventory("Jeans_Blue");
		captive.GetHumanInventory().CreateInHands("HandcuffsLocked");
		captive.SetRestrained(true);
		return captive;
	}

	//! The truck's pose maths (HandTruckPose), on a spare truck lying on the ground
	void TestPose()
	{
		EntityAI truck = Drop("HandCart_HandTruck_Red", Verge(-m_Width / 2 - 1 + 3, 40));
		Print("[HandCartTest] memory points: axle " + HandTruckPose.GetAxle(truck) + " handle " + HandTruckPose.GetHandle(truck) + " hub " + truck.GetMemoryPointPos("wheel_l_axis_1"));

		float axleHeight;
		vector hands;
		HandTruckPose.GetHands(truck, axleHeight, hands);
		vector tm[4];
		HandTruckPose.Solve(truck, false, 0, tm);
		vector grip = HandTruckPose.Apply(HandTruckPose.GetHandle(truck), tm);
		vector axle = HandTruckPose.Apply(HandTruckPose.GetAxle(truck), tm);
		float tilt = Math.Acos(tm[1][1]) * Math.RAD2DEG;
		Check(vector.Distance(grip, hands) < 0.001 && Math.AbsFloat(axle[1] - axleHeight) < 0.001 && Math.AbsFloat(tilt - 50) < 0.1, "level pose: handle at the hands, wheels on the ground, leaning 50 (" + tilt + ")");
		float pivot = HandTruckPose.GetPivot(HandCart_HandTruck_Base.Cast(truck));
		Check(pivot > 0.7 && pivot < 1.6, "the axle is " + pivot + " m in front of the pusher");

		HandTruckPose.Solve(truck, false, 0.2, tm);
		grip = HandTruckPose.Apply(HandTruckPose.GetHandle(truck), tm);
		axle = HandTruckPose.Apply(HandTruckPose.GetAxle(truck), tm);
		float lifted = Math.Acos(tm[1][1]) * Math.RAD2DEG;
		Check(vector.Distance(grip, hands) < 0.001 && Math.AbsFloat(axle[1] - axleHeight - 0.2) < 0.001 && lifted > tilt, "wheels 0.2 up: still in the hands, leaning further back (" + lifted + ")");

		HandTruckPose.Solve(truck, true, 0, tm);
		axle = HandTruckPose.Apply(HandTruckPose.GetAxle(truck), tm);
		float upright = Math.Acos(tm[1][1]) * Math.RAD2DEG;
		// the hands are on the rail line (through the handle, along the truck's up)
		vector top = HandTruckPose.Apply(HandTruckPose.GetHandle(truck), tm);
		vector off = hands - top;
		float along = vector.Dot(off, tm[1]);
		float across = (off - tm[1] * along).Length();
		Check(Math.AbsFloat(upright - 15) < 0.1 && Math.AbsFloat(axle[1] - axleHeight) < 0.001 && across < 0.001 && along < 0, "carrying a person: leaning 15, hands " + (-along) + " down the rails");

		HandTruckPose.Solve(truck, false, 0, tm);
		float reach = HandTruckPose.Reach(truck, tm);
		HandTruckPose.Turn(truck, 30, 0, tm);
		grip = HandTruckPose.Apply(HandTruckPose.GetHandle(truck), tm);
		axle = HandTruckPose.Apply(HandTruckPose.GetAxle(truck), tm);
		float heading = Math.Atan2(axle[0] - hands[0], axle[2] - hands[2]) * Math.RAD2DEG;
		Check(vector.Distance(grip, hands) < 0.001 && Math.AbsFloat(heading - 30) < 0.1 && Math.AbsFloat(HandTruckPose.Reach(truck, tm) - reach) < 0.001, "turned 30: pivots about the hands, wheels to the right (" + heading + ")");

		HandTruckPose.Solve(truck, false, 0, tm);
		HandTruckPose.Turn(truck, 0, 10, tm);
		vector hubL = HandTruckPose.Apply(truck.GetMemoryPointPos("wheel_l_axis_1"), tm);
		vector hubR = HandTruckPose.Apply(truck.GetMemoryPointPos("wheel_r_axis_2"), tm);
		vector right = hubL;
		vector left = hubR;
		if (hubR[0] > hubL[0])
		{
			right = hubR;
			left = hubL;
		}
		Check(right[1] > left[1] + 0.05, "rolled 10: the right-hand wheel is higher (" + right[1] + " vs " + left[1] + ")");
		GetGame().ObjectDelete(truck);
	}

	protected PlayerBase m_Pusher;
	protected HandCart_HandTruck_Base m_PushTruck;
	protected PlayerBase m_Rider;
	protected bool m_RiderDone;
	protected vector m_RiderOffset;

	//! Loading a handcuffed survivor onto a hand truck and everything that lets them off
	void TestRider()
	{
		vector at = Verge(-m_Width / 2 - 1, 50);
		m_Pusher = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorM_Mirek", at, 0, "NONE"));
		m_PushTruck = HandCart_HandTruck_Base.Cast(m_Pusher.GetHumanInventory().CreateInHands("HandCart_HandTruck_Yellow"));
		m_Pusher.SetDirection(m_Along);
		m_Rider = Captive(at + m_Along * 1.2);
		PlayerBase free = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorM_Mirek", at - m_Along * 1.2, 0, "NONE"));

		string reason;
		Check(m_Rider && m_Rider.IsRestrained(), "handcuffed survivor");
		Check(!HandTruckRiders.CanCarry(m_Pusher, m_PushTruck, free, reason), "a free survivor can't be loaded");
		Check(HandTruckRiders.CanCarry(m_Pusher, m_PushTruck, m_Rider, reason), "a handcuffed one can");
		Check(HandTruckRiders.LoadServer(m_Pusher, m_PushTruck, m_Rider) && m_PushTruck.HasRider() && m_PushTruck.GetRider() == m_Rider && m_Rider.HandCartIsRiding() && m_Rider.HandCartGetTruck() == m_PushTruck, "loaded onto the truck");
		Check(m_PushTruck.IsLoaded() && m_Pusher.GetLoadedHandTruck() == m_PushTruck, "the truck counts as loaded (walking pace)");
		EntityAI crate = Drop("WoodenCrate", at + m_Across * 1.5);
		Check(!HandTruckRules.CanLoad(m_PushTruck, crate), "nothing else goes on with them");
		Check(!m_PushTruck.CanPutInCargo(null), "the truck can't go into cargo with them on");
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(TestRiding, 1500, false);
	}

	void TestRiding()
	{
		vector tm[4];
		m_Rider.HandCartGetRideTransform(tm);
		float gap = vector.Distance(m_Rider.GetPosition(), tm[3]);
		vector rel = m_Rider.GetPosition() - m_Pusher.GetPosition();
		m_RiderOffset = rel;
		Print("[HandCartTest] rider at " + m_Rider.GetPosition() + " pusher " + m_Pusher.GetPosition() + " ahead " + vector.Dot(rel, m_Pusher.GetDirection()) + " cmd " + m_Rider.GetCurrentCommandID() + " solid " + m_Rider.PhysicsIsSolid());
		Check(gap < 0.05 && m_Rider.GetCurrentCommandID() == DayZPlayerConstants.COMMANDID_SCRIPT, "riding: on the nose plate (" + gap + " m off)");
		float ahead = vector.Dot(rel, m_Pusher.GetDirection());
		Check(ahead > 0.3 && ahead < 1.5, "in front of the pusher (" + ahead + " m)");
		Check(!m_Rider.PhysicsIsSolid(), "rider doesn't collide");

		// The pusher moves on: the rider comes along
		m_Pusher.SetPosition(RoadFinder.OnSurface(m_Pusher.GetPosition()[0] + m_Along[0] * 3, m_Pusher.GetPosition()[2] + m_Along[2] * 3));
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(TestRidingMoved, 1000, false);
	}

	void TestRidingMoved()
	{
		vector rel = m_Rider.GetPosition() - m_Pusher.GetPosition();
		Check(vector.Distance(rel, m_RiderOffset) < 0.2, "follows the pusher (" + vector.Distance(rel, m_RiderOffset) + " m off)");

		string reason;
		bool unloaded = HandTruckRiders.UnloadServer(m_Pusher, m_PushTruck, reason);
		Check(unloaded && !m_PushTruck.HasRider() && !m_Rider.HandCartIsRiding(), "unloaded (" + reason + ")");
		Check(m_Rider.PhysicsIsSolid(), "solid again");

		// Freed: let off
		Check(HandTruckRiders.LoadServer(m_Pusher, m_PushTruck, m_Rider), "loaded again");
		m_Rider.SetRestrained(false);
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(TestRiderFreed, 1200, false);
	}

	void TestRiderFreed()
	{
		Check(!m_PushTruck.HasRider() && !m_Rider.HandCartIsRiding(), "freed: let off");
		Check(m_Rider.GetCurrentCommandID() != DayZPlayerConstants.COMMANDID_SCRIPT, "walking again (command " + m_Rider.GetCurrentCommandID() + ")");

		// Truck dropped: let off
		m_Rider.SetRestrained(true);
		Check(HandTruckRiders.LoadServer(m_Pusher, m_PushTruck, m_Rider), "loaded a third time");
		// What a drop ends in: the truck moved from hands to the ground
		InventoryLocation src = new InventoryLocation();
		m_PushTruck.GetInventory().GetCurrentInventoryLocation(src);
		InventoryLocation dst = new InventoryLocation();
		vector ground[4];
		Math3D.MatrixIdentity4(ground);
		ground[3] = m_Pusher.GetPosition() + m_Along;
		dst.SetGround(m_PushTruck, ground);
		Check(GameInventory.LocationSyncMoveEntity(src, dst), "truck dropped");
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(TestRiderDropped, 1000, false);
	}

	void TestRiderDropped()
	{
		Check(!m_PushTruck.HasRider() && !m_Rider.HandCartIsRiding() && !m_PushTruck.GetHierarchyParent(), "truck dropped: let off");

		// Killed: let off
		m_Pusher.GetHumanInventory().CreateInHands("HandCart_HandTruck_Blue");
		HandCart_HandTruck_Base truck = HandCart_HandTruck_Base.Cast(m_Pusher.GetItemInHands());
		Check(truck && HandTruckRiders.LoadServer(m_Pusher, truck, m_Rider), "loaded onto another truck");
		m_PushTruck = truck;
		m_Rider.SetHealth("", "", 0);
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(TestRiderKilled, 1000, false);
	}

	void TestRiderKilled()
	{
		Check(!m_PushTruck.HasRider() && !m_Rider.HandCartIsRiding(), "killed: let off");
		m_RiderDone = true;
	}

	//! Pier and crane objects along the south coast, for finding somewhere flat by the sea
	void LogDocks()
	{
		map<string, vector> seen = new map<string, vector>;
		for (float x = 5800; x <= 8600; x += 250)
		{
			for (float z = 2000; z <= 3200; z += 250)
			{
				array<Object> objects = new array<Object>;
				GetGame().GetObjectsAtPosition(Vector(x, 0, z), 180, objects, null);
				for (int i = 0; i < objects.Count(); i++)
				{
					string model = objects[i].GetShapeName();
					model.ToLower();
					if (model.Contains("pier") || model.Contains("crane") || model.Contains("dock") || model.Contains("quay") || model.Contains("molo") || model.Contains("nabrez"))
					{
						vector p = objects[i].GetPosition();
						string key = model + " " + Math.Round(p[0] / 50) + "," + Math.Round(p[2] / 50);
						if (!seen.Contains(key))
							seen.Insert(key, p);
					}
				}
			}
		}
		for (int k = 0; k < seen.Count(); k++)
		{
			vector q = seen.GetElement(k);
			Print("[HandCartDock] " + seen.GetKey(k) + " at " + q + " surface " + GetGame().SurfaceRoadY(q[0], q[2]));
		}
	}

	//! Model-space boxes of every loadable item, for placing the truck's load proxies
	void LogBoxes()
	{
		TStringArray types = {"ExpansionSafeLarge", "ExpansionSafeMedium", "ExpansionSafeMini", "SeaChest", "WoodenCrate",
			"Truck_01_WheelDouble", "Truck_01_Wheel", "Offroad_02_Wheel", "HatchbackWheel", "CivSedanWheel", "Sedan_02_Wheel", "Hatchback_02_Wheel", "PowerGenerator", "PartyTent", "Barrel_Blue", "BarrelHoles_Blue", "HM_Stove", "HM_Sewing_Machine"};
		for (int i = 0; i < types.Count(); i++)
		{
			EntityAI e = Drop(types[i], Verge(20, i * 3));
			vector box[2];
			bool coll = e.GetCollisionBox(box);
			vector bb[2];
			e.ClippingInfo(bb);
			Print("[HandCartBox] " + types[i] + " coll " + coll + " " + box[0] + " " + box[1] + " clip " + bb[0] + " " + bb[1]);
		}
	}

	void TestHeld()
	{
		SHumanCommandClimbResult climb = new SHumanCommandClimbResult();
		Check(!m_Dummy.CanSprint() && !m_Dummy.CanJump() && !m_Dummy.CanClimb(0, climb), "empty truck: no sprint, no jump, no climbing");

		Check(HandTruckRules.CanLoad(m_Truck, m_Safe), "empty truck: an unlocked closed safe can be loaded");

		for (int i = 0; i < 4; i++)
			Check(Load(m_Crates[i]), "crate " + (i + 1) + " loaded");
		Check(!HandTruckRules.CanLoad(m_Truck, m_Crates[4]), "fifth crate refused (truck full)");
		Check(!HandTruckRules.CanLoad(m_Truck, m_Chest), "sea chest refused while carrying crates");
		Check(!HandTruckRules.CanLoad(m_Truck, m_Safe), "safe refused while carrying crates");
		Check(m_Truck.IsLoaded(), "truck reports loaded");
		Check(!m_Dummy.CanSprint() && !m_Dummy.CanJump(), "loaded truck: no sprint, no jump");
		Check(HandTruckLoad.GetTopItem(m_Truck) == m_Crates[3], "top item is the last crate loaded");

		EntityAI top = HandTruckLoad.GetTopItem(m_Truck);
		Check(m_Truck.CanReleaseAttachment(top), "load can be dragged off in the inventory");

		vector spot = Verge(-m_Width / 2 - 1 + 3, -12);
		Check(HandTruckRules.UnloadServer(m_Truck, top, spot, "0 0 0"), "top crate unloaded");
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(TestUnloaded, 1000, false, top, spot);
	}

	void TestUnloaded(EntityAI crate, vector spot)
	{
		Check(crate.GetHierarchyParent() == null, "unloaded crate is on the ground");
		Check(vector.Distance(crate.GetPosition(), spot) < 1, "unloaded crate is where it was put (" + vector.Distance(crate.GetPosition(), spot) + " m off)");
		Check(m_Truck.GetInventory().AttachmentCount() == 3, "three crates left on the truck");

		// Empty it, then the sea chest with its contents
		while (HandTruckLoad.GetTopItem(m_Truck))
		{
			if (!HandTruckRules.UnloadServer(m_Truck, HandTruckLoad.GetTopItem(m_Truck), Verge(-m_Width / 2 + 3, -10 - m_Truck.GetInventory().AttachmentCount()), "0 0 0"))
				break;
		}
		Check(!m_Truck.IsLoaded(), "truck emptied");

		// One generator; up to three packed canopy tents
		Check(Load(m_Generator), "power generator loaded");
		Check(!HandTruckRules.CanLoad(m_Truck, m_Tents[0]), "canopy tent refused while carrying the generator");
		Check(HandTruckRules.UnloadServer(m_Truck, m_Generator, Verge(4, -16), "0 0 0"), "power generator unloaded");
		for (int t = 0; t < 3; t++)
			Check(Load(m_Tents[t]), "packed canopy tent " + (t + 1) + " loaded");
		Check(!HandTruckRules.CanLoad(m_Truck, m_Tents[3]), "fourth canopy tent refused (truck full)");
		while (HandTruckLoad.GetTopItem(m_Truck))
			HandTruckRules.UnloadServer(m_Truck, HandTruckLoad.GetTopItem(m_Truck), Verge(4, -18 - 1.5 * m_Truck.GetInventory().AttachmentCount()), "0 0 0");
		Check(!m_Truck.IsLoaded(), "truck emptied again");
		Check(Load(m_Chest), "sea chest (with a canteen in it) loaded");
		Check(m_Chest.GetInventory().GetCargo().GetItemCount() == 1, "sea chest kept its contents");

		// Unload spot rules on the flat road: close is fine, past MaxUnloadDistance is not
		string reason;
		vector near = m_Dummy.GetPosition() + m_Along * 1.5;
		near[1] = GetGame().SurfaceRoadY(near[0], near[2]);
		bool ok = HandTruckRules.CheckSpot(m_Dummy, m_Chest, near, m_Dummy.GetOrientation(), null, reason);
		Check(ok, "sea chest can be set down 1.5 m away on the road (" + reason + ")");
		reason = "";
		vector far = m_Dummy.GetPosition() + m_Along * 3;
		far[1] = GetGame().SurfaceRoadY(far[0], far[2]);
		ok = HandTruckRules.CheckSpot(m_Dummy, m_Chest, far, m_Dummy.GetOrientation(), null, reason);
		Check(!ok && reason == "Too far away", "3 m away is too far (" + reason + ")");

		TestVehicle();

		// Dropped from hands, the truck must come to rest standing on the ground
		// (what the drop hand event does; queued events never run for this AI dummy)
		InventoryLocation src = new InventoryLocation();
		m_Truck.GetInventory().GetCurrentInventoryLocation(src);
		InventoryLocation dst = new InventoryLocation();
		GameInventory.SetGroundPosByOwner(m_Dummy, m_Truck, dst);
		GameInventory.LocationSyncMoveEntity(src, dst);
		Print("[HandCartTest] dropped truck placed at " + m_Truck.GetPosition());
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(TestDropped, 4000, false);
	}

	//! Barrels, and full containers in and out of the MS3's slots and cargo
	void TestVehicle()
	{
		while (HandTruckLoad.GetTopItem(m_Truck))
			HandTruckRules.UnloadServer(m_Truck, HandTruckLoad.GetTopItem(m_Truck), Verge(4, -30 - m_Truck.GetInventory().AttachmentCount()), "0 0 0");

		Barrel_ColorBase barrel = Barrel_ColorBase.Cast(Drop("Barrel_Yellow", Verge(-m_Width / 2 - 1 + 1.5, -34)));
		Fill(barrel);
		barrel.Open();
		Check(Load(barrel) && barrel.GetNumberOfItems() == 3, "full, open barrel loaded");
		Check(!barrel.IsOpen(), "its lid is shut on the truck");
		HandTruckRules.UnloadServer(m_Truck, barrel, Verge(4, -34), "0 0 0");

		TestLifeMachines();

		Check(m_MS3 != null, "MS3 parked");
		if (!m_MS3)
			return;
		Transport ms3 = Transport.Cast(m_MS3);
		EntityAI load = HandTruckRules.FindVehicleLoad(m_Truck, ms3);
		Check(load && load.IsInherited(Barrel_ColorBase), "MS3 offers a barrel from its barrel slots (" + load + ")");
		Check(HandTruckRules.LoadServer(m_Truck, load) && load.GetHierarchyParent() == m_Truck, "full barrel taken out of the MS3");
		Check(!Barrel_ColorBase.Cast(load).IsOpen(), "its lid is shut on the truck");
		Check(HandTruckRules.UnloadIntoVehicleServer(m_Truck, ms3) && load.GetHierarchyParent() == m_MS3 && load.GetInventory().IsAttachment(), "full barrel put back in a barrel slot");

		// A player's drag is only granted if the item could go into their hands
		Check(load.CanPutIntoHands(m_Dummy), "full barrel passes the drag check while holding a hand truck");
		Check(!m_Dummy.CanReceiveItemIntoHands(load), "but still can't be taken into hands full");

		// The same both ways by dragging in the inventory (the game's own move checks)
		InventoryLocation src = new InventoryLocation();
		load.GetInventory().GetCurrentInventoryLocation(src);
		InventoryLocation dst = new InventoryLocation();
		dst.SetAttachment(m_Truck, load, InventorySlots.GetSlotIdFromString("HandCart_Barrel"));
		Check(load.IsTakeable() && GameInventory.LocationCanMoveEntity(src, dst), "full barrel can be dragged from the MS3 onto the truck");
		GameInventory.LocationSyncMoveEntity(src, dst);
		Check(load.GetHierarchyParent() == m_Truck, "dragged onto the truck");
		InventoryLocation back = new InventoryLocation();
		back.Copy(src);
		load.GetInventory().GetCurrentInventoryLocation(src);
		Check(GameInventory.LocationCanMoveEntity(src, back), "full barrel can be dragged from the truck into the MS3's barrel slot");
		GameInventory.LocationSyncMoveEntity(src, back);

		EntityAI crate = m_MS3.GetInventory().FindAttachment(InventorySlots.GetSlotIdFromString("Truck_01_WoodenCrate1"));
		Check(Load(crate), "full crate taken from the MS3's crate slot");
		Check(HandTruckRules.UnloadIntoVehicleServer(m_Truck, ms3) && crate.GetHierarchyParent() == m_MS3 && crate.GetInventory().IsAttachment(), "full crate put back in a crate slot");

		EntityAI chest;
		CargoBase cargo = m_MS3.GetInventory().GetCargo();
		for (int i = 0; i < cargo.GetItemCount(); i++)
		{
			if (cargo.GetItem(i).IsInherited(SeaChest))
				chest = cargo.GetItem(i);
		}
		Check(Load(chest) && chest.GetInventory().GetCargo().GetItemCount() == 3, "full sea chest taken from the MS3's cargo");
		Check(HandTruckRules.UnloadIntoVehicleServer(m_Truck, ms3) && chest.GetHierarchyParent() == m_MS3 && !chest.GetInventory().IsAttachment(), "full sea chest put back in the cargo");
		Check(chest.GetInventory().GetCargo().GetItemCount() == 3, "sea chest kept its contents");

		CarWheel wheel = CarWheel.Cast(m_MS3.GetInventory().FindAttachment(InventorySlots.GetSlotIdFromString("Truck_01_Wheel_1_1")));
		Check(wheel && !HandTruckRules.CanLoad(m_Truck, wheel), "the MS3's own wheels are not a load");
	}

	//! Life Machines' stove and sewing machine (the HandCart_LifeMachines add-on)
	void TestLifeMachines()
	{
		HM_Stove stove = HM_Stove.Cast(Drop("HM_Stove", Verge(-m_Width / 2 - 1 + 1.5, -36)));
		Check(stove != null, "Life Machines stove created");
		if (!stove)
			return;
		Fill(stove);
		Check(stove.GetInventory().GetCargo().GetItemCount() == 3, "stove has things in it");
		Check(stove.CanPutIntoHands(m_Dummy) && !m_Dummy.CanReceiveItemIntoHands(stove), "stove passes the drag check, but still can't go into hands");
		Check(Load(stove), "stove loaded");
		Check(stove.GetInventory().GetCargo().GetItemCount() == 3, "stove kept its contents");
		Check(HandTruckRules.UnloadServer(m_Truck, stove, Verge(4, -36), "0 0 0") && !stove.GetHierarchyParent(), "stove set down");

		HM_Sewing_Machine sewing = HM_Sewing_Machine.Cast(Drop("HM_Sewing_Machine", Verge(-m_Width / 2 - 1 + 1.5, -38)));
		Check(sewing != null, "Life Machines sewing machine created");
		if (!sewing)
			return;
		sewing.SetMachineWorking(true);
		Check(!HandTruckRules.CanLoad(m_Truck, sewing), "working sewing machine refused");
		sewing.SetMachineWorking(false);
		Check(Load(sewing), "sewing machine loaded once it's done");
		Check(!HandTruckRules.CanLoad(m_Truck, stove), "stove refused while carrying the sewing machine");
		Check(!sewing.CanPutInCargo(m_Truck) && sewing.CanPutInCargo(m_MS3), "sewing machine fits a vehicle's cargo, no other");
		Check(HandTruckRules.UnloadIntoVehicleServer(m_Truck, Transport.Cast(m_MS3)) && sewing.GetHierarchyParent() == m_MS3, "sewing machine put in the MS3");
		Check(Load(sewing) && sewing.GetHierarchyParent() == m_Truck, "and taken back out");
		Check(HandTruckRules.UnloadServer(m_Truck, sewing, Verge(4, -38), "0 0 0") && !sewing.GetHierarchyParent(), "sewing machine set down");
	}

	void TestDropped()
	{
		vector pos = m_Truck.GetPosition();
		float ground = GetGame().SurfaceRoadY(pos[0], pos[2]);
		vector minMax[2];
		m_Truck.GetCollisionBox(minMax);
		float sunk = ground - (pos[1] + minMax[0][1]);
		Print("[HandCartTest] dropped truck at " + pos + " ori " + m_Truck.GetOrientation() + " ground " + ground + " box " + minMax[0] + " " + minMax[1] + " parent " + m_Truck.GetHierarchyParent());
		Check(!m_Truck.GetHierarchyParent() && Math.AbsFloat(sunk) < 0.1, "dropped truck rests on the ground (bottom " + sunk + " m below)");

		// Into an empty MS3's bed (10x60), loaded or not: it fits crossways, as in every
		// vehicle (all cargo is 10 wide and at least 30 deep)
		EntityAI ms3 = EntityAI.Cast(GetGame().CreateObjectEx("Truck_01_Covered", Verge(12, -20), ECE_PLACE_ON_SURFACE));
		Check(ms3 && ms3.GetInventory().CanAddEntityInCargo(m_Truck, false), "hand truck fits crossways in an MS3's bed");
		Check(ms3 && ms3.GetInventory().TakeEntityToCargo(InventoryMode.SERVER, m_Truck) && m_Truck.GetHierarchyParent() == ms3, "hand truck put in the MS3");

		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(Finish, 1000, false);
	}

	bool Load(EntityAI item)
	{
		if (!item || !HandTruckRules.CanLoad(m_Truck, item))
			return false;
		return HandTruckRules.LoadServer(m_Truck, item);   // what the Load action does
	}

	void Finish()
	{
		if (!m_RiderDone)
		{
			GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(Finish, 500, false);
			return;
		}
		Print("[HandCartTest] done, " + m_Failures + " failures");
		GetGame().RequestExit(0);
	}
};

Mission CreateCustomMission(string path)
{
	return new CustomMission();
}
