//! A tied-up player riding a hand truck. Bodies can't be tilted (the engine keeps players
//! upright, whatever a script asks), so a truck carrying a person stands more upright
//! (PersonTiltDegrees) and they stand on its nose plate. The truck and the rider each hold
//! the other's network id (synced), and wherever the rider is simulated (the server and
//! their own client) a scripted movement command puts their body on the nose plate every
//! frame: HandTruckPose.GetRiderTransform, from the pusher's position and the truck's turn.

//! Server: the rules and the moves
class HandTruckRiders
{
	static const float MAX_LOAD_DISTANCE = 2.5;    //!< metres from the pusher to the person
	static const float MAX_RIDE_DISTANCE = 4.0;    //!< beyond this from the pusher they're let off

	//! Can pusher load person onto truck? Both sides check this; reason says why not.
	static bool CanCarry(PlayerBase pusher, HandCart_HandTruck_Base truck, PlayerBase person, out string reason)
	{
		if (!HandCartSettings.Get().AllowPeople || !pusher || !truck || !person || person == pusher)
			return false;
		if (truck.GetPusher() != pusher || truck.IsRuined())
			return false;
		if (!person.IsAlive() || !person.IsRestrained() || person.IsUnconscious())
			return false;
		if (person.IsInVehicle() || person.HandCartIsRiding() || person.IsPlayerInStance(DayZPlayerConstants.STANCEMASK_PRONE))
			return false;
		if (truck.IsLoaded())
		{
			reason = "The hand truck is loaded";
			return false;
		}
		if (vector.Distance(pusher.GetPosition(), person.GetPosition()) > MAX_LOAD_DISTANCE)
		{
			reason = "Too far away";
			return false;
		}
		return true;
	}

	static bool LoadServer(PlayerBase pusher, HandCart_HandTruck_Base truck, PlayerBase person)
	{
		string reason;
		if (!CanCarry(pusher, truck, person, reason))
			return false;

		truck.SetRiderServer(person);
		person.HandCartSetTruckServer(truck);
		Print("[HandCart] " + person + " loaded onto " + truck + " by " + pusher);
		return true;
	}

	//! Let the rider off where they stand, no questions asked
	static void ReleaseServer(HandCart_HandTruck_Base truck)
	{
		PlayerBase rider = truck.GetRider();
		truck.SetRiderServer(null);
		if (rider)
		{
			rider.HandCartSetTruckServer(null);
			Print("[HandCart] " + rider + " let off " + truck);
		}
	}

	//! The pusher sets the rider down: only somewhere they can stand and the pusher can see
	static bool UnloadServer(PlayerBase pusher, HandCart_HandTruck_Base truck, out string reason)
	{
		PlayerBase rider = truck.GetRider();
		if (!rider || truck.GetPusher() != pusher)
			return false;
		if (!CanStandThere(pusher, truck, rider, reason))
			return false;
		ReleaseServer(truck);
		return true;
	}

	static bool CanStandThere(PlayerBase pusher, HandCart_HandTruck_Base truck, PlayerBase rider, out string reason)
	{
		array<Object> excluded = new array<Object>;
		excluded.Insert(pusher);
		excluded.Insert(rider);
		excluded.Insert(truck);

		vector pos = rider.GetPosition();
		if (g_Game.GetWaterDepth(pos) > 0.5)
		{
			reason = "Not in the water";
			return false;
		}

		// A body-sized box from just off the floor to head height
		if (g_Game.IsBoxCollidingGeometry(pos + Vector(0, 0.95, 0), rider.GetOrientation(), Vector(0.4, 1.5, 0.4), ObjIntersectFire, ObjIntersectGeom, excluded))
		{
			reason = "Something is in the way";
			return false;
		}

		if (HandCartSettings.Get().RequireLineOfSight)
		{
			vector eye = pusher.GetBonePositionWS(pusher.GetBoneIndexByName("Head"));
			RaycastRVParams los = new RaycastRVParams(eye, pos + Vector(0, 1.2, 0));
			los.type = ObjIntersectView;
			array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
			if (DayZPhysics.RaycastRVProxy(los, hits, excluded) && hits.Count() > 0)
			{
				reason = "You can't see that spot";
				return false;
			}
		}
		return true;
	}

	//! Server, every half second while someone rides: let them off once riding stops
	//! making sense (freed, dead, knocked out, gone, or the pusher is)
	static void CheckServer(HandCart_HandTruck_Base truck)
	{
		if (!truck.HasRider())
		{
			truck.SetRiderServer(null);   // stops the checks
			return;
		}
		if (!StillRiding(truck))
			ReleaseServer(truck);
	}

	static bool StillRiding(HandCart_HandTruck_Base truck)
	{
		PlayerBase rider = truck.GetRider();
		PlayerBase pusher = truck.GetPusher();
		if (!HandCartSettings.Get().AllowPeople || !rider || !pusher)
			return false;
		if (rider.HandCartGetTruck() != truck)
			return false;
		if (!rider.IsAlive() || !rider.IsRestrained() || rider.IsUnconscious())
			return false;
		if (!pusher.IsAlive() || pusher.IsUnconscious())
			return false;
		return vector.Distance(rider.GetPosition(), pusher.GetPosition()) <= MAX_RIDE_DISTANCE;
	}
}

//! The rider's movement command: holds their body where HandCartGetRideTransform says.
//! Non-managed: the engine owns and deletes it once started (Human.StartCommand_ScriptInst).
class HandTruckRideCommand : HumanCommandScript
{
	protected PlayerBase m_Player;

	void HandTruckRideCommand(Human pHuman)
	{
		m_Player = PlayerBase.Cast(pHuman);
	}

	override void OnActivate()
	{
		Print("[HandCart] " + m_Player + " riding");
	}

	override void OnDeactivate()
	{
		Print("[HandCart] " + m_Player + " off");
	}

	override void PreAnimUpdate(float pDt)
	{
		vector transform[4];
		if (!m_Player || !m_Player.HandCartGetRideTransform(transform))
			return;
		SetHeading(-Math.Atan2(transform[2][0], transform[2][2]));
	}

	//! Move the body by what's left to its spot: physics then applies it like root motion
	override void PrePhysUpdate(float pDt)
	{
		vector transform[4];
		if (!m_Player || !m_Player.HandCartGetRideTransform(transform))
		{
			PrePhys_SetTranslation(vector.Zero);
			return;
		}

		vector current[4];
		m_Player.GetTransform(current);
		vector delta = transform[3] - current[3];
		PrePhys_SetTranslation(Vector(vector.Dot(delta, current[0]), vector.Dot(delta, current[1]), vector.Dot(delta, current[2])));
	}

	override bool PostPhysUpdate(float pDt)
	{
		vector transform[4];
		if (!m_Player || !m_Player.HandCartGetRideTransform(transform))
			return true;

		PostPhys_SetPosition(transform[3]);
		vector rot[3];
		rot[0] = transform[0];
		rot[1] = transform[1];
		rot[2] = transform[2];
		float q[4];
		Math3D.MatrixToQuat(rot, q);
		PostPhys_SetRotation(q);
		return true;
	}
}

modded class PlayerBase
{
	protected HandTruckRideCommand m_HandCartRideCmd;   //!< not ref: owned by the engine
	protected int m_HandCartTruckLow;                   //!< network id of the truck being ridden, 0 0 if none
	protected int m_HandCartTruckHigh;
	protected bool m_HandCartBodyOff;                   //!< collision switched off while riding

	override void Init()
	{
		super.Init();

		RegisterNetSyncVariableInt("m_HandCartTruckLow");
		RegisterNetSyncVariableInt("m_HandCartTruckHigh");
	}

	bool HandCartIsRiding()
	{
		return m_HandCartTruckLow != 0 || m_HandCartTruckHigh != 0;
	}

	HandCart_HandTruck_Base HandCartGetTruck()
	{
		if (!HandCartIsRiding())
			return null;
		return HandCart_HandTruck_Base.Cast(g_Game.GetObjectByNetworkId(m_HandCartTruckLow, m_HandCartTruckHigh));
	}

	//! Server: on (truck) or off (null) a hand truck
	void HandCartSetTruckServer(HandCart_HandTruck_Base truck)
	{
		if (truck)
		{
			truck.GetNetworkID(m_HandCartTruckLow, m_HandCartTruckHigh);
		}
		else
		{
			m_HandCartTruckLow = 0;
			m_HandCartTruckHigh = 0;
		}
		SetSynchDirty();
		HandCartApplyRiding();
	}

	override void OnVariablesSynchronized()
	{
		super.OnVariablesSynchronized();

		HandCartApplyRiding();
	}

	//! A rider's body doesn't collide, or the pusher would walk into it
	protected void HandCartApplyRiding()
	{
		bool riding = HandCartIsRiding();
		if (riding == m_HandCartBodyOff)
			return;
		m_HandCartBodyOff = riding;
		PhysicsSetSolid(!riding);
	}

	bool HandCartGetRideTransform(out vector transform[4])
	{
		HandCart_HandTruck_Base truck = HandCartGetTruck();
		if (!truck)
			return false;
		PlayerBase pusher = truck.GetPusher();
		if (!pusher)
			return false;
		HandTruckPose.GetRiderTransform(pusher, truck, this, transform);
		return true;
	}

	override bool ModCommandHandlerBefore(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
	{
		if (super.ModCommandHandlerBefore(pDt, pCurrentCommandID, pCurrentCommandFinished))
			return true;

		bool running = pCurrentCommandID == DayZPlayerConstants.COMMANDID_SCRIPT && m_HandCartRideCmd && GetCommand_Script() == m_HandCartRideCmd;

		// Ride while there's a truck and a pusher to ride with (on a client they may not
		// have streamed in yet) and while alive (vanilla handles death below)
		vector transform[4];
		bool ride = HandCartIsRiding() && IsAlive() && HandCartGetRideTransform(transform);
		if (!ride)
		{
			if (running)
				m_HandCartRideCmd.SetFlagFinished(true);
			if (!running || pCurrentCommandFinished)
				m_HandCartRideCmd = null;
			return false;
		}

		if (!running)
			m_HandCartRideCmd = HandTruckRideCommand.Cast(StartCommand_ScriptInst(HandTruckRideCommand));
		return true;
	}

	override void EEKilled(Object killer)
	{
		HandCartLetOff();
		super.EEKilled(killer);
	}

	override void EEDelete(EntityAI parent)
	{
		HandCartLetOff();
		super.EEDelete(parent);
	}

	//! Server: a rider who dies or leaves gets off; so does anyone riding the truck of a
	//! pusher who does (their truck drops, which lets them off too)
	protected void HandCartLetOff()
	{
		if (!g_Game.IsServer() || !HandCartIsRiding())
			return;
		HandCart_HandTruck_Base truck = HandCartGetTruck();
		if (truck && truck.GetRider() == this)
			HandTruckRiders.ReleaseServer(truck);
		else
			HandCartSetTruckServer(null);
	}

	override void SetActionsRemoteTarget(out TInputActionMap InputActionMap)
	{
		super.SetActionsRemoteTarget(InputActionMap);

		AddAction(ActionHandTruckLoadPerson, InputActionMap);
	}
}
