//! No sprinting, jumping, climbing or vaulting with a hand truck in hands, loaded or empty
//! (an empty one may still run). Pushing a loaded one: walking
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

		if (!GetLoadedHandTruck())
		{
			if (m_HandCartSpeedLimited)
			{
				hic.SetErectSpeedLimit(false);
				m_HandCartSpeedLimited = false;
			}
			return;
		}

		// Set every frame: other systems (vanilla's RestartAllMovement) can clear it
		hic.SetErectSpeedLimit(true, DayZPlayerConstants.MOVEMENTIDX_WALK);
		m_HandCartSpeedLimited = true;

		m_HandCartSurfaceTimer -= dt;
		if (m_HandCartSurfaceTimer <= 0)
		{
			m_HandCartSurfaceTimer = HANDCART_SURFACE_INTERVAL;
			m_HandCartSoftGround = HandCartIsOnSoftGround();
		}

		if (!m_HandCartSoftGround)
			return;

		// Scale the walk down; speed runs 0 idle, 1 walk, 2 run, 3 sprint
		float cap = HandCartSettings.Get().SoftGroundSpeed;
		float speed;
		vector direction;
		hic.GetMovement(speed, direction);
		if (speed > cap)
			hic.OverrideMovementSpeed(HumanInputControllerOverrideType.ONE_FRAME, cap);
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
