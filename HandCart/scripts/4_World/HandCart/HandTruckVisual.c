//! Client: the hand truck as drawn while someone holds it. The real truck stays the item
//! in their hands (inventory, saving and the actions all work on it) but is hidden; this
//! puts a local copy, with copies of the load, in front of the player, attached to them.
//! Every frame the copy is posed by HandTruckPose around the player's hands:
//! - Turning: the wheels stay where they are and the truck pivots on them, so when the
//!   player turns, the truck swings the other way; it then swings back to face where they
//!   look at no more than TurnRateDegrees a second, and never points more than
//!   MaxTurnDegrees away (beyond that it's dragged round).
//! - Terrain (FollowTerrain): it tips and rolls so both wheels sit on the ground.
//! - Sounds: gravel crunching while it rolls loaded, clanks while it rolls empty.
class HandTruckVisual
{
	static const string TYPE = "HandCart_HandTruckVisual";
	static const float REHIDE_INTERVAL = 250;   //!< ms; vanilla unhides held items after ladders and swimming
	static const float TERRAIN_RATE = 10;       //!< how fast the truck settles onto the ground (1/s)
	static const float MAX_LIFT = 0.5;          //!< metres the wheels may rise or drop to the ground
	static const float MAX_ROLL = 20;           //!< degrees
	static const float TURN_SEND_INTERVAL = 0.2;   //!< seconds between turn updates to the server
	static const float MOVING_SPEED = 0.3;      //!< m/s above which the truck counts as rolling
	static const float RUN_SPEED = 3.5;         //!< m/s at which the sounds are loudest

	protected HandCart_HandTruck_Base m_Truck;
	protected PlayerBase m_Player;
	protected EntityAI m_Copy;

	protected bool m_HasWheels;
	protected vector m_Wheels;        //!< world: the axle's spot on the ground (only x, z used)
	protected float m_Turn;           //!< degrees the truck points right of where the player faces
	protected float m_Lift;           //!< metres the wheels are raised to meet the ground
	protected float m_Roll;           //!< degrees
	protected float m_SentTurn;
	protected float m_SendTimer;

	protected vector m_LastPos;
	protected float m_Speed;          //!< m/s, smoothed
	protected bool m_Moving;
	protected float m_ClankTimer;
	protected float m_CreakTimer;
	protected EffectSound m_Rolling;

	void HandTruckVisual(HandCart_HandTruck_Base truck, PlayerBase player)
	{
		m_Truck = truck;
		m_Player = player;

		m_Copy = EntityAI.Cast(g_Game.CreateObjectEx(TYPE, player.GetPosition(), ECE_LOCAL | ECE_NOLIFETIME));
		if (!m_Copy)
			return;

		// Same colour as the real truck
		TStringArray textures = new TStringArray;
		g_Game.ConfigGetTextArray("CfgVehicles " + truck.GetType() + " hiddenSelectionsTextures", textures);
		if (textures.Count() > 0)
			m_Copy.SetObjectTexture(0, textures[0]);
		SyncHealth();

		// Local transform, then attach: the transform becomes relative to the player
		vector transform[4];
		HandTruckPose.Solve(truck, truck.HasRider(), 0, transform);
		m_Copy.SetTransform(transform);
		player.AddChild(m_Copy, -1);

		m_LastPos = player.GetPosition();
		m_ClankTimer = Math.RandomFloat(2, 4);

		SyncLoad();
		Hide();
		Print("[HandCart] drawing the held truck for " + player);
		g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(Hide, REHIDE_INTERVAL, true);
		g_Game.GetUpdateQueue(CALL_CATEGORY_GAMEPLAY).Insert(Update);
	}

	void ~HandTruckVisual()
	{
		if (g_Game)
		{
			g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(Hide);
			g_Game.GetUpdateQueue(CALL_CATEGORY_GAMEPLAY).Remove(Update);
		}
		StopRolling();
		if (m_Player && m_Player.GetItemAccessor())
			m_Player.GetItemAccessor().HideItemInHands(false);
		if (m_Truck)
			SetLoadInvisible(m_Truck, false);
		if (m_Copy)
		{
			if (m_Player)
				m_Player.RemoveChild(m_Copy);
			g_Game.ObjectDelete(m_Copy);
		}
	}

	PlayerBase GetPlayer()
	{
		return m_Player;
	}

	//! Degrees the drawn truck points right of where its pusher faces
	float GetTurn()
	{
		return m_Turn;
	}

	protected void Hide()
	{
		if (m_Player && m_Player.GetItemAccessor() && !m_Player.GetItemAccessor().IsItemInHandsHidden())
			m_Player.GetItemAccessor().HideItemInHands(true);
	}

	//! Every frame
	void Update(float dt)
	{
		if (!m_Copy || !m_Player || !m_Truck || dt <= 0)
			return;

		dt = Math.Min(dt, 0.1);
		UpdateTurn(dt);

		vector tm[4];
		HandTruckPose.Solve(m_Truck, m_Truck.HasRider(), m_Lift, tm);
		HandTruckPose.Turn(m_Truck, m_Turn, m_Roll, tm);
		Place(tm);

		if (HandCartSettings.Get().FollowTerrain)
			UpdateTerrain(dt, tm);
		else
		{
			m_Lift = 0;
			m_Roll = 0;
		}

		UpdateSounds(dt);
	}

	//! Move the copy to tm in the player's space. A child can't be moved in place: off, move,
	//! back on (as vanilla does with lights)
	protected void Place(vector tm[4])
	{
		m_Player.RemoveChild(m_Copy);
		m_Copy.SetTransform(tm);
		m_Player.AddChild(m_Copy, -1);
		m_Player.Update();
	}

	protected void UpdateTurn(float dt)
	{
		HandCartSettings settings = HandCartSettings.Get();
		vector p[4];
		m_Player.GetTransform(p);
		vector forward = Flat(p[2]);
		vector right = Vector(forward[2], 0, -forward[0]);

		float axleHeight;
		vector hands;
		HandTruckPose.GetHands(m_Truck, axleHeight, hands);
		vector handsWS = p[3] + p[0] * hands[0] + p[2] * hands[2];

		vector level[4];
		HandTruckPose.Solve(m_Truck, m_Truck.HasRider(), m_Lift, level);
		float reach = HandTruckPose.Reach(m_Truck, level);

		vector apart = m_Wheels - handsWS;
		apart[1] = 0;
		vector toWheels = Flat(apart);
		if (!m_HasWheels || apart.Length() < 0.01 || apart.Length() > reach + 1.5)
		{
			// Start (or after a teleport): straight ahead
			m_HasWheels = true;
			m_Turn = 0;
		}
		else
		{
			// Where the wheels were pulls the truck round: the angle to them from the hands
			toWheels.Normalize();
			float turn = Math.Atan2(vector.Dot(toWheels, right), vector.Dot(toWheels, forward)) * Math.RAD2DEG;
			float before = m_Turn;
			float step = settings.TurnRateDegrees * dt;
			turn = Math.Clamp(turn, -settings.MaxTurnDegrees, settings.MaxTurnDegrees);
			if (turn > step)
				turn -= step;
			else if (turn < -step)
				turn += step;
			else
				turn = 0;
			m_Turn = turn;

			// Swung hard against the limit: it creaks
			if (Math.AbsFloat(m_Turn) >= settings.MaxTurnDegrees - 1 && Math.AbsFloat(m_Turn - before) > 60 * dt)
				Creak();
		}

		// Someone else's truck carrying a person: the turn the server has, so the person on
		// it and the truck agree
		bool own = m_Player == g_Game.GetPlayer();
		if (m_Truck.HasRider() && !own)
			m_Turn = Math.Lerp(m_Turn, m_Truck.GetTurn(), Math.Min(1, dt * 8));

		float yaw = m_Turn * Math.DEG2RAD;
		vector dir = forward * Math.Cos(yaw) + right * Math.Sin(yaw);
		m_Wheels = handsWS + dir * reach;

		if (own && m_Truck.HasRider())
		{
			m_SendTimer -= dt;
			if (m_SendTimer <= 0 && Math.AbsFloat(m_Turn - m_SentTurn) > 0.5)
			{
				m_SendTimer = TURN_SEND_INTERVAL;
				m_SentTurn = m_Turn;
				m_Truck.SendTurn(m_Turn);
			}
		}
	}

	//! Raise or drop the wheels, and roll the truck, until both sit on the ground
	protected void UpdateTerrain(float dt, vector tm[4])
	{
		array<Object> excluded = new array<Object>;
		excluded.Insert(m_Player);
		excluded.Insert(m_Truck);
		excluded.Insert(m_Copy);
		AddLoad(m_Truck, excluded);
		AddLoad(m_Copy, excluded);
		PlayerBase rider = m_Truck.GetRider();
		if (rider)
			excluded.Insert(rider);

		// The two wheel hubs, outermost points of the axle
		vector hubs[2];
		hubs[0] = m_Truck.GetMemoryPointPos("wheel_l_axis_1");
		hubs[1] = m_Truck.GetMemoryPointPos("wheel_r_axis_2");
		float radius = (hubs[0][1] + hubs[1][1]) * 0.5;   // hubs are a radius above the model's floor

		float error[2];
		float side[2];
		int found = 0;
		for (int i = 0; i < 2; i++)
		{
			vector hub = HandTruckPose.Apply(hubs[i], tm);
			side[i] = hub[0];
			vector world = m_Copy.ModelToWorld(hubs[i]);
			float bottom = world[1] - radius;
			float ground;
			if (HandTruckRules.GroundHeightBetween(world + Vector(0, 0.6, 0), world - Vector(0, radius + 0.6, 0), excluded, ground))
			{
				error[i] = ground - bottom;
				found++;
			}
			else
			{
				error[i] = 0;   // over an edge: hold it there
			}
		}

		float k = Math.Min(1, dt * TERRAIN_RATE);
		if (found > 0)
		{
			m_Lift = Math.Clamp(m_Lift + (error[0] + error[1]) * 0.5 * k, -MAX_LIFT, MAX_LIFT);

			// Rolling by r raises a point at x by x * sin r
			float span = side[0] - side[1];
			if (Math.AbsFloat(span) > 0.1)
			{
				float roll = Math.Asin(Math.Clamp((error[0] - error[1]) / span, -1, 1)) * Math.RAD2DEG;
				m_Roll = Math.Clamp(m_Roll + roll * k, -MAX_ROLL, MAX_ROLL);
			}
		}
		else
		{
			m_Lift = m_Lift * (1 - k);
			m_Roll = m_Roll * (1 - k);
		}
	}

	protected static void AddLoad(EntityAI entity, array<Object> list)
	{
		GameInventory inv = entity.GetInventory();
		for (int i = 0; i < inv.AttachmentCount(); i++)
		{
			EntityAI item = inv.GetAttachmentFromIndex(i);
			list.Insert(item);
			AddLoad(item, list);
		}
	}

	static vector Flat(vector v)
	{
		v[1] = 0;
		float len = v.Length();
		if (len > 0.0001)
			return v * (1 / len);
		return "0 0 1";
	}

	// --- Sounds -----------------------------------------------------------------------------

	protected void UpdateSounds(float dt)
	{
		vector pos = m_Player.GetPosition();
		vector moved = pos - m_LastPos;
		moved[1] = 0;
		m_LastPos = pos;
		float speed = moved.Length() / dt;
		if (speed > 20)
			speed = 0;   // teleported
		m_Speed = Math.Lerp(m_Speed, speed, Math.Min(1, dt * 6));

		float volume = HandCartSettings.Get().SoundVolume;
		bool moving = m_Speed > MOVING_SPEED;
		bool started = moving && !m_Moving;
		bool stopped = !moving && m_Moving;
		m_Moving = moving;
		m_CreakTimer -= dt;

		if (volume <= 0 || g_Game.IsDedicatedServer())
		{
			StopRolling();
			return;
		}

		if (m_Truck.IsLoaded())
		{
			// Loaded: gravel crunching while it rolls, louder the faster
			if (moving)
			{
				if (!m_Rolling)
					m_Rolling = SEffectManager.PlaySoundOnObject("HandCart_Gravel_SoundSet", m_Copy, 0.3, 0.4, true);
				if (m_Rolling)
					m_Rolling.SetSoundVolume(volume * Math.Clamp(m_Speed / RUN_SPEED * 1.5, 0.35, 1));
			}
			else
			{
				StopRolling();
			}
			return;
		}

		// Empty: clanks as it starts, stops and every few seconds while it rolls
		StopRolling();
		if (started || stopped)
		{
			Clank(volume * 0.8);
			m_ClankTimer = Math.RandomFloat(1.5, 3.5);
		}
		else if (moving)
		{
			m_ClankTimer -= dt * Math.Clamp(m_Speed / 1.5, 0.5, 2);
			if (m_ClankTimer <= 0)
			{
				Clank(volume * Math.RandomFloat(0.5, 1));
				m_ClankTimer = Math.RandomFloat(1.5, 4);
			}
		}
	}

	protected void Clank(float volume)
	{
		if (!m_Copy)
			return;
		EffectSound sound = SEffectManager.PlaySoundOnObject("HandCart_Clank_SoundSet", m_Copy);
		if (!sound)
			return;
		sound.SetSoundAutodestroy(true);
		sound.SetSoundVolume(volume);
	}

	protected void Creak()
	{
		float volume = HandCartSettings.Get().SoundVolume;
		if (m_CreakTimer > 0 || volume <= 0 || !m_Copy || g_Game.IsDedicatedServer())
			return;
		m_CreakTimer = 1.5;
		EffectSound sound = SEffectManager.PlaySoundOnObject("HandCart_Creak_SoundSet", m_Copy);
		if (!sound)
			return;
		sound.SetSoundAutodestroy(true);
		sound.SetSoundVolume(volume);
	}

	protected void StopRolling()
	{
		if (!m_Rolling)
			return;
		m_Rolling.SetSoundAutodestroy(true);
		m_Rolling.SoundStop();
		m_Rolling = null;
	}

	// --- Looks ------------------------------------------------------------------------------

	//! Draw the copy's frame worn the same as the real truck (config.cpp healthLevels). The
	//! wheels aren't a hidden selection, so on the copy they always look new.
	void SyncHealth()
	{
		if (!m_Copy)
			return;

		string suffix;
		int level = m_Truck.GetHealthLevel();
		if (level >= GameConstants.STATE_RUINED)
			suffix = "_destruct";
		else if (level >= GameConstants.STATE_DAMAGED)
			suffix = "_damage";
		m_Copy.SetObjectMaterial(0, "HandCart\\data\\handcart_frame" + suffix + ".rvmat");
	}

	//! Show or hide an entity's attachments, and theirs. Hiding the truck in hands leaves
	//! its load drawn at the hand.
	static void SetLoadInvisible(EntityAI entity, bool invisible)
	{
		GameInventory inv = entity.GetInventory();
		for (int i = 0; i < inv.AttachmentCount(); i++)
		{
			EntityAI item = inv.GetAttachmentFromIndex(i);
			SetLoadInvisible(item, invisible);
			item.SetInvisible(invisible);
		}
	}

	//! Copy whatever is loaded on the real truck onto the drawn one, and hide the real load
	void SyncLoad()
	{
		if (!m_Copy)
			return;

		SetLoadInvisible(m_Truck, true);

		GameInventory copyInv = m_Copy.GetInventory();
		for (int i = copyInv.AttachmentCount() - 1; i >= 0; i--)
			g_Game.ObjectDelete(copyInv.GetAttachmentFromIndex(i));

		GameInventory inv = m_Truck.GetInventory();
		for (int j = 0; j < inv.AttachmentCount(); j++)
		{
			EntityAI item = inv.GetAttachmentFromIndex(j);
			InventoryLocation loc = new InventoryLocation();
			item.GetInventory().GetCurrentInventoryLocation(loc);
			if (!copyInv.CreateAttachmentEx(item.GetType(), loc.GetSlot()))
				Print("[HandCart] could not draw " + item.GetType() + " on the held truck");
		}

		// Loading and unloading clank
		if (!g_Game.IsDedicatedServer() && HandCartSettings.Get().SoundVolume > 0)
			Clank(HandCartSettings.Get().SoundVolume);
	}
}

//! The drawn copy: takes any load (copies, never real containers with contents)
class HandCart_HandTruckVisual : Inventory_Base
{
	override bool CanReceiveAttachment(EntityAI attachment, int slotId)
	{
		return true;
	}

	override bool CanDisplayAttachmentSlot(int slot_id)
	{
		return false;
	}

	override bool IsTakeable()
	{
		return false;
	}
}
