//! Client: the hand truck as drawn while someone holds it. The real truck stays the item
//! in their hands (inventory, saving and the actions all work on it) but is hidden; this
//! puts a local copy, with copies of the load, in front of the player, leaning back onto
//! its wheels, attached to the player so it moves with them.
class HandTruckVisual
{
	static const string TYPE = "HandCart_HandTruckVisual";
	static const float REHIDE_INTERVAL = 250;   //!< ms; vanilla unhides held items after ladders and swimming

	protected HandCart_HandTruck_Base m_Truck;
	protected PlayerBase m_Player;
	protected EntityAI m_Copy;

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
		GetHeldTransform(m_Copy, transform);
		m_Copy.SetTransform(transform);
		player.AddChild(m_Copy, -1);

		SyncLoad();
		Hide();
		Print("[HandCart] drawing the held truck for " + player);
		g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(Hide, REHIDE_INTERVAL, true);
	}

	void ~HandTruckVisual()
	{
		g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(Hide);
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

	//! The truck's transform in the player's model space (x right, y up, z forward): nose
	//! plate forward, leaning back by HoldTiltDegrees, wheels HoldLift above the player's
	//! origin and the grip HoldGripForward ahead. In the truck model the nose points down -z.
	static void GetHeldTransform(EntityAI truck, out vector transform[4])
	{
		HandCartSettings settings = HandCartSettings.Get();
		float tilt = settings.HoldTiltDegrees * Math.DEG2RAD;
		float c = Math.Cos(tilt);
		float s = Math.Sin(tilt);
		transform[1] = Vector(0, c, -s);    // truck up: leaning back towards the player
		transform[2] = Vector(0, -s, -c);   // truck z: nose (-z) forward and tipped up
		transform[0] = "-1 0 0";            // truck x: completes the frame (up x z)

		// Axle midway between the wheel axis points, grip at the "handle" memory point
		vector axle = (truck.GetMemoryPointPos("wheel_l_axis_1") + truck.GetMemoryPointPos("wheel_r_axis_2")) * 0.5;
		vector grip = truck.GetMemoryPointPos("handle");
		vector gripFromAxle = Rotate(grip - axle, transform);
		vector axleAt = Vector(0, axle[1] + settings.HoldLift, settings.HoldGripForward - gripFromAxle[2]);
		transform[3] = axleAt - Rotate(axle, transform);
	}

	//! A truck-space direction in the player's space
	static vector Rotate(vector v, vector transform[4])
	{
		return transform[0] * v[0] + transform[1] * v[1] + transform[2] * v[2];
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
