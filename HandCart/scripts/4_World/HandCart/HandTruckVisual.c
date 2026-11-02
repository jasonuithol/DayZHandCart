//! Client: the hand truck as drawn while someone holds it. The real truck stays the item
//! in their hands (inventory, saving and the actions all work on it) but is hidden; this
//! puts a local copy, with copies of the load, in front of the player, leaning back onto
//! its wheels (more upright while a person rides it), attached to the player so it moves
//! with them. It also makes the truck's
//! sounds: gravel crunching while it rolls loaded, clanks while it rolls empty, a creak
//! when it's swung round hard.
class HandTruckVisual
{
	static const string TYPE = "HandCart_HandTruckVisual";
	static const float REHIDE_INTERVAL = 250;   //!< ms; vanilla unhides held items after ladders and swimming
	static const float MOVING_SPEED = 0.3;      //!< m/s above which the truck counts as rolling
	static const float RUN_SPEED = 3.5;         //!< m/s at which the rolling sound is loudest
	static const float CREAK_TURN_RATE = 150;   //!< degrees a second of turning that make it creak

	protected HandCart_HandTruck_Base m_Truck;
	protected PlayerBase m_Player;
	protected EntityAI m_Copy;

	protected vector m_LastPos;
	protected vector m_LastDir;
	protected float m_TurnRate;       //!< degrees a second the player turns, smoothed
	protected float m_Speed;          //!< m/s, smoothed
	protected bool m_Moving;
	protected float m_ClankTimer;
	protected float m_CreakTimer;
	protected EffectSound m_Rolling;
	protected bool m_HadRider;        //!< the copy is placed for a person riding

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

		Place();

		m_LastPos = player.GetPosition();
		m_LastDir = player.GetDirection();
		m_ClankTimer = Math.RandomFloat(2, 4);

		SyncLoad();
		Hide();
		Print("[HandCart] drawing the held truck for " + player);
		g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(Hide, REHIDE_INTERVAL, true);
		g_Game.GetUpdateQueue(CALL_CATEGORY_GAMEPLAY).Insert(UpdateSounds);
	}

	void ~HandTruckVisual()
	{
		if (g_Game)
		{
			g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(Hide);
			g_Game.GetUpdateQueue(CALL_CATEGORY_GAMEPLAY).Remove(UpdateSounds);
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

	//! Put the copy in front of the player (HandTruckPose): more upright while a person rides.
	//! Local transform, then attach: the transform becomes relative to the player.
	protected void Place()
	{
		vector transform[4];
		m_HadRider = m_Truck.HasRider();
		HandTruckPose.Solve(m_Copy, m_HadRider, transform);
		if (m_Copy.GetParent() == m_Player)
			m_Player.RemoveChild(m_Copy);
		m_Copy.SetTransform(transform);
		m_Player.AddChild(m_Copy, -1);
		m_Player.Update();
	}

	protected void Hide()
	{
		if (m_Player && m_Player.GetItemAccessor() && !m_Player.GetItemAccessor().IsItemInHandsHidden())
			m_Player.GetItemAccessor().HideItemInHands(true);
	}

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
		if (m_HadRider != m_Truck.HasRider())
			Place();

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

	// --- Sounds -----------------------------------------------------------------------------

	//! Every frame
	void UpdateSounds(float dt)
	{
		if (!m_Copy || !m_Player || !m_Truck || dt <= 0)
			return;
		dt = Math.Min(dt, 0.1);

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
		if (turning > 1000)
			turning = 0;   // snapped round
		m_TurnRate = Math.Lerp(m_TurnRate, turning, Math.Min(1, dt * 6));

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

		// Swung round hard: it creaks
		if (m_TurnRate > CREAK_TURN_RATE)
			Creak(volume);

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

	protected void Creak(float volume)
	{
		if (m_CreakTimer > 0 || !m_Copy)
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
