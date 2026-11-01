//! Client: the hand truck as drawn while someone holds it. The real truck stays the item
//! in their hands (inventory, saving and the actions all work on it) but is hidden; this
//! puts a local copy, with copies of the load, in front of the player, attached to them.
//! Every frame the copy is posed by HandTruckPose around the player's hands:
//! - Terrain (FollowTerrain): it tips and rolls so both wheels sit on the ground.
//! - Sounds: gravel crunching while it rolls loaded, clanks while it rolls empty, a
//!   creak while it's turned hard.
//! The steering itself (slow turns that pivot on the wheels) is the player's movement:
//! HandTruckMovement.c.
class HandTruckVisual
{
	static const string TYPE = "HandCart_HandTruckVisual";
	static const float REHIDE_INTERVAL = 250;   //!< ms; vanilla unhides held items after ladders and swimming
	static const float TERRAIN_RATE = 10;       //!< how fast the truck settles onto the ground (1/s)
	static const float MAX_LIFT = 0.5;          //!< metres the wheels may rise or drop to the ground
	static const float MAX_ROLL = 20;           //!< degrees
	static const float MOVING_SPEED = 0.3;      //!< m/s above which the truck counts as rolling
	static const float RUN_SPEED = 3.5;         //!< m/s at which the sounds are loudest

	protected HandCart_HandTruck_Base m_Truck;
	protected PlayerBase m_Player;
	protected EntityAI m_Copy;

	protected float m_Lift;           //!< metres the wheels are raised to meet the ground
	protected float m_Roll;           //!< degrees

	protected vector m_LastPos;
	protected vector m_LastDir;
	protected float m_TurnRate;       //!< degrees a second the player turns, smoothed
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

		vector tm[4];
		HandTruckPose.Solve(m_Truck, m_Truck.HasRider(), m_Lift, tm);
		HandTruckPose.Turn(m_Truck, 0, m_Roll, tm);
		Place(tm);

		if (HandCartSettings.Get().FollowTerrain)
			UpdateTerrain(dt, tm);
		else
		{
			m_Lift = 0;
			m_Roll = 0;
		}

		UpdateSounds(dt);

		m_DebugTimer -= dt;
		if (m_DebugTimer <= 0)
		{
			m_DebugTimer = 1;
			vector copyTm[4];
			m_Copy.GetLocalTransform(copyTm);
			Print("[HandCart] pose turning " + m_TurnRate + " lift " + m_Lift + " roll " + m_Roll + " ground " + m_DebugGround[0] + " " + m_DebugGround[1] + " player " + m_Player.GetPosition() + " dir " + m_Player.GetDirection() + " copy " + m_Copy.GetPosition() + " local " + copyTm[3] + " up " + copyTm[1]);
		}
	}

	protected float m_DebugTimer;
	protected float m_DebugGround[2];

	//! Move the copy to tm in the player's space. A child can't be moved in place: off, move,
	//! back on (as vanilla does with lights)
	protected void Place(vector tm[4])
	{
		m_Player.RemoveChild(m_Copy);
		m_Copy.SetTransform(tm);
		m_Player.AddChild(m_Copy, -1);
		m_Player.Update();
	}

	//! Raise or drop the wheels, and roll the truck, to the ground under them. Measured
	//! from the ground under the player's feet (where the level pose has the wheels), at
	//! the wheels' spots worked out from the player's transform: the copy's own world
	//! position isn't up to date straight after it's re-attached.
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

		vector p[4];
		m_Player.GetTransform(p);
		vector feet = p[3];
		float base;
		if (!HandTruckRules.GroundHeightBetween(feet + Vector(0, 0.5, 0), feet - Vector(0, 1, 0), excluded, base))
			base = feet[1];

		// The two wheel hubs, outermost points of the axle
		vector hubs[2];
		hubs[0] = m_Truck.GetMemoryPointPos("wheel_l_axis_1");
		hubs[1] = m_Truck.GetMemoryPointPos("wheel_r_axis_2");

		float rise[2];
		float side[2];
		int found = 0;
		for (int i = 0; i < 2; i++)
		{
			vector hub = HandTruckPose.Apply(hubs[i], tm);
			side[i] = hub[0];
			vector at = p[3] + p[0] * hub[0] + p[2] * hub[2];
			float ground;
			if (HandTruckRules.GroundHeightBetween(Vector(at[0], base + 1.0, at[2]), Vector(at[0], base - MAX_LIFT - 0.5, at[2]), excluded, ground))
			{
				rise[i] = ground - base;
				found++;
			}
			else
			{
				rise[i] = 0;   // over an edge: as if level
			}
			m_DebugGround[i] = rise[i];
		}

		float lift = 0;
		float roll = 0;
		if (found > 0)
		{
			lift = Math.Clamp((rise[0] + rise[1]) * 0.5, -MAX_LIFT, MAX_LIFT);
			// Rolling by r raises a point at x by x * sin r
			float span = side[0] - side[1];
			if (Math.AbsFloat(span) > 0.1)
				roll = Math.Clamp(Math.Asin(Math.Clamp((rise[0] - rise[1]) / span, -1, 1)) * Math.RAD2DEG, -MAX_ROLL, MAX_ROLL);
		}

		float k = Math.Min(1, dt * TERRAIN_RATE);
		m_Lift += (lift - m_Lift) * k;
		m_Roll += (roll - m_Roll) * k;
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

		vector dir = m_Player.GetDirection();
		dir[1] = 0;
		dir.Normalize();
		float cross = m_LastDir[0] * dir[2] - m_LastDir[2] * dir[0];
		m_LastDir = dir;
		float turning = Math.AbsFloat(Math.Asin(Math.Clamp(cross, -1, 1))) * Math.RAD2DEG / dt;
		if (turning > 720)
			turning = 0;   // the first frame, or snapped round
		m_TurnRate = Math.Lerp(m_TurnRate, turning, Math.Min(1, dt * 6));

		float volume = HandCartSettings.Get().SoundVolume;
		bool moving = m_Speed > MOVING_SPEED;
		bool started = moving && !m_Moving;
		bool stopped = !moving && m_Moving;
		m_Moving = moving;
		m_CreakTimer -= dt;

		// Turned as hard as it goes: it creaks
		float limit = HandCartSettings.Get().TurnRateDegrees;
		if (limit > 0 && m_TurnRate > limit * 0.8)
			Creak();

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
