//! No sprinting, jumping, climbing or vaulting with a hand truck in hands, loaded or empty
//! (an empty one may still run), and it steers like one (HandCartSteer). Pushing a loaded one: walking
//! pace on hard ground (roads, floors, concrete), slower on soft ground (anything diggable,
//! as the game's surface config defines it). Applied in CommandHandler, which runs on both
//! the owning client and the server, so the two simulate the same movement.
//! Unloading (choosing a spot) stops the player in place.
//! See the item behaviour in Registration.c.
modded class PlayerBase
{
	protected static const float HANDCART_SURFACE_INTERVAL = 0.25;   //!< seconds between ground checks

	protected bool m_HandCartSpeedLimited;
	protected bool m_HandCartSoftGround;
	protected float m_HandCartSurfaceTimer;

	HandCart_HandTruck_Base GetLoadedHandTruck()
	{
		HandCart_HandTruck_Base truck = HandCart_HandTruck_Base.Cast(GetItemInHands());
		if (truck && truck.IsLoaded())
			return truck;
		return null;
	}

	override bool CanSprint()
	{
		// Never with a hand truck, loaded or empty
		if (HandCart_HandTruck_Base.Cast(GetItemInHands()))
			return false;
		return super.CanSprint();
	}

	override bool CanJump()
	{
		if (HandCart_HandTruck_Base.Cast(GetItemInHands()))
			return false;
		return super.CanJump();
	}

	override bool CanClimb(int climbType, SHumanCommandClimbResult climbRes)
	{
		if (HandCart_HandTruck_Base.Cast(GetItemInHands()))
			return false;
		return super.CanClimb(climbType, climbRes);
	}

	override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
	{
		super.CommandHandler(pDt, pCurrentCommandID, pCurrentCommandFinished);

		HandCartLimitMovement(pDt);
	}

	protected void HandCartLimitMovement(float dt)
	{
		HumanInputController hic = GetInputController();
		if (!hic)
			return;

		// Stand still while choosing where to unload, so walking doesn't carry the truck
		// into the spot. The ghost is client-side, so only the owner knows; the server
		// follows the input it sends.
		if (IsControlledPlayer() && HandTruckPlacement.IsActive())
		{
			hic.OverrideMovementSpeed(HumanInputControllerOverrideType.ONE_FRAME, 0);
			return;
		}

		HandCart_HandTruck_Base truck = HandCart_HandTruck_Base.Cast(GetItemInHands());
		bool loaded = truck && truck.IsLoaded();
		if (!loaded)
		{
			if (m_HandCartSpeedLimited)
			{
				hic.SetErectSpeedLimit(false);
				m_HandCartSpeedLimited = false;
			}
		}
		else
		{
			// Set every frame: other systems (vanilla's RestartAllMovement) can clear it
			hic.SetErectSpeedLimit(true, DayZPlayerConstants.MOVEMENTIDX_WALK);
			m_HandCartSpeedLimited = true;

			m_HandCartSurfaceTimer -= dt;
			if (m_HandCartSurfaceTimer <= 0)
			{
				m_HandCartSurfaceTimer = HANDCART_SURFACE_INTERVAL;
				m_HandCartSoftGround = HandCartIsOnSoftGround();
			}
		}

		// Fastest allowed; speed runs 0 idle, 1 walk, 2 run, 3 sprint
		float cap = 2;
		if (loaded)
		{
			cap = 1;
			if (m_HandCartSoftGround)
				cap = HandCartSettings.Get().SoftGroundSpeed;
		}

		if (truck && HandCartSteers())
		{
			HandCartSteer(dt, hic, truck, cap);
			return;
		}
		m_HandCartHasDir = false;

		if (!loaded)
			return;
		float speed;
		vector direction;
		hic.GetMovement(speed, direction);
		if (speed > cap)
			hic.OverrideMovementSpeed(HumanInputControllerOverrideType.ONE_FRAME, cap);
	}

	// --- Steering like a hand truck (SteerLikeACart) -----------------------------------------
	// The truck rolls on its wheels, so the player holding it can only push it forward or
	// pull it back, turns no faster than TurnRateDegrees (the mouse is held back, A and D
	// turn), and as they turn they step sideways round the axle: the wheels stay put and
	// the handles swing. Movement and heading are both simulated on the owning client and
	// the server from the same input, so the two agree.

	static const float HANDCART_WALK_SPEED = 1.5;   //!< m/s for movement speed 1 (walk)

	protected float m_HandCartHeading;    //!< radians, the heading the truck lets the player have
	protected bool m_HandCartHeadingSet;
	protected vector m_HandCartLastDir;
	protected bool m_HandCartHasDir;
	protected float m_HandCartTurnRate;   //!< radians a second the body turned in the last frame, + right
	protected float m_HandCartSteerLog;

	bool HandCartSteers()
	{
		if (!HandCartSettings.Get().SteerLikeACart || !IsAlive() || IsUnconscious() || IsRestrained())
			return false;
		return HandCart_HandTruck_Base.Cast(GetItemInHands()) != null;
	}

	float HandCartGetTurnRate()
	{
		return m_HandCartTurnRate;
	}

	static float HandCartWrap(float angle)
	{
		while (angle > Math.PI)
			angle -= Math.PI2;
		while (angle < -Math.PI)
			angle += Math.PI2;
		return angle;
	}

	//! The heading (where the player aims, and so turns to) may change at most
	//! TurnRateDegrees a second; A and D turn it instead of strafing
	override bool HeadingModel(float pDt, SDayZPlayerHeadingModel pModel)
	{
		bool changed = super.HeadingModel(pDt, pModel);

		if (!HandCartSteers() || pModel.m_iCurrentCommandID != DayZPlayerConstants.COMMANDID_MOVE)
		{
			m_HandCartHeadingSet = false;
			return changed;
		}
		if (!m_HandCartHeadingSet)
		{
			m_HandCartHeadingSet = true;
			m_HandCartHeading = pModel.m_fHeadingAngle;
		}

		float step = HandCartSettings.Get().TurnRateDegrees * Math.DEG2RAD * pDt;
		float diff = HandCartWrap(pModel.m_fHeadingAngle - m_HandCartHeading);

		// A/D: turn (heading grows to the left)
		HumanInputController hic = GetInputController();
		if (hic)
		{
			float speed;
			vector direction;
			hic.GetMovement(speed, direction);
			if (speed > 0 && direction[0] > 0.3)
				diff = -step;
			else if (speed > 0 && direction[0] < -0.3)
				diff = step;
		}

		m_HandCartHeading = HandCartWrap(m_HandCartHeading + Math.Clamp(diff, -step, step));
		pModel.m_fHeadingAngle = m_HandCartHeading;
		return true;
	}

	//! Forward and back only, and a sidestep that keeps the wheels where they are while
	//! the body turns
	protected void HandCartSteer(float dt, HumanInputController hic, HandCart_HandTruck_Base truck, float cap)
	{
		vector dir = GetDirection();
		dir[1] = 0;
		dir.Normalize();
		if (!m_HandCartHasDir || dt <= 0)
		{
			m_HandCartHasDir = true;
			m_HandCartLastDir = dir;
			m_HandCartTurnRate = 0;
		}
		// Turning right (clockwise from above) makes this cross product negative
		float cross = m_HandCartLastDir[0] * dir[2] - m_HandCartLastDir[2] * dir[0];
		float turned = -Math.Asin(Math.Clamp(cross, -1, 1));
		m_HandCartLastDir = dir;
		if (dt > 0)
			m_HandCartTurnRate = turned / dt;

		// Turning right swings the truck's axle right by pivot * angle: step left as much
		float pivot = HandTruckPose.GetPivot(truck);
		float side = -pivot * m_HandCartTurnRate / HANDCART_WALK_SPEED;

		float speed;
		vector direction;
		hic.GetMovement(speed, direction);
		float forward = 0;
		if (speed > 0 && Math.AbsFloat(direction[2]) > 0.3)
			forward = Math.Min(speed, cap) * Math.Sign(direction[2]);

		side = Math.Clamp(side, -1, 1);
		float total = Math.Sqrt(forward * forward + side * side);
		if (total < 0.05)
		{
			hic.OverrideMovementSpeed(HumanInputControllerOverrideType.ONE_FRAME, 0);
			return;
		}
		total = Math.Min(total, Math.Max(cap, 1));
		float angle = Math.Atan2(side, forward) * Math.RAD2DEG;
		hic.OverrideMovementSpeed(HumanInputControllerOverrideType.ONE_FRAME, total);
		hic.OverrideMovementAngle(HumanInputControllerOverrideType.ONE_FRAME, angle);

		m_HandCartSteerLog -= dt;
		if (m_HandCartSteerLog <= 0 && !g_Game.IsDedicatedServer())
		{
			m_HandCartSteerLog = 1;
			HumanCommandMove hcm = GetCommand_Move();
			float actual = 0;
			if (hcm)
				actual = hcm.GetCurrentMovementAngle();
			Print("[HandCart] steer turning " + (m_HandCartTurnRate * Math.RAD2DEG) + " pivot " + pivot + " forward " + forward + " side " + side + " angle " + angle + " actual " + actual + " dir " + dir + " pos " + GetPosition());
		}
	}

	protected bool HandCartIsOnSoftGround()
	{
		string surface;
		int liquid;
		g_Game.SurfaceUnderObject(this, surface, liquid);

		HandCartSettings settings = HandCartSettings.Get();
		if (settings.HardSurfaces.Find(surface) >= 0)
			return false;
		if (settings.SoftSurfaces.Find(surface) >= 0)
			return true;
		return g_Game.IsSurfaceDigable(surface);
	}
}
