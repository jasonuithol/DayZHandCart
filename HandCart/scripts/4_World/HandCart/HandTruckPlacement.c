//! Client-only unload mode: a local ghost copy of the item on top of the truck's load
//! follows the player's aim until it is placed (ActionHandTruckPlace) or cancelled.
//! Updated every frame from MissionGameplay.OnUpdate.
class HandTruckPlacement
{
	static const float ROTATE_SPEED = 90.0;   //!< degrees per second while a rotate key is held
	static const float ROTATE_STEP = 15.0;    //!< degrees per mouse wheel notch, as vanilla placing
	static const string GHOST_TEXTURE = "#(argb,8,8,3)color(0.5,0.5,0.5,0.75,ca)";

	protected static ref HandTruckPlacement s_Instance;

	protected EntityAI m_Target;
	protected EntityAI m_Ghost;
	protected float m_YawOffset;      //!< ghost yaw relative to the player's heading
	protected bool m_Frozen;          //!< true while the Place action is running
	protected bool m_Valid;
	protected int m_ShownState = -1;  //!< last material applied: -1 none, 0 invalid, 1 valid
	protected string m_Reason;

	static HandTruckPlacement Get()
	{
		if (!s_Instance)
			s_Instance = new HandTruckPlacement();
		return s_Instance;
	}

	static bool IsActive()
	{
		return s_Instance && s_Instance.m_Target && s_Instance.m_Ghost;
	}

	EntityAI GetTarget()
	{
		return m_Target;
	}

	bool IsValid()
	{
		return m_Valid;
	}

	string GetReason()
	{
		return m_Reason;
	}

	vector GetPosition()
	{
		return m_Ghost.GetPosition();
	}

	vector GetOrientation()
	{
		return m_Ghost.GetOrientation();
	}

	void SetFrozen(bool frozen)
	{
		m_Frozen = frozen;
	}

	void Begin(PlayerBase player, EntityAI truck)
	{
		Stop();
		EntityAI target = HandTruckLoad.GetTopItem(truck);
		if (!target)
			return;

		m_Ghost = EntityAI.Cast(g_Game.CreateObjectEx(target.GetType(), player.GetPosition(), ECE_LOCAL));
		if (!m_Ghost)
			return;

		m_Target = target;
		m_Ghost.SetAllowDamage(false);
		ItemBase ghostItem = ItemBase.Cast(m_Ghost);
		if (ghostItem)
			ghostItem.SetIsHologram(true);

		m_YawOffset = 0;
		m_ShownState = -1;
		m_Valid = false;
	}

	void Stop()
	{
		if (m_Ghost)
			g_Game.ObjectDelete(m_Ghost);
		m_Ghost = null;
		m_Target = null;
		m_Frozen = false;
		m_Valid = false;
	}

	void Update(PlayerBase player, float timeslice)
	{
		if (!player)
		{
			Stop();
			return;
		}

		// Still holding the truck, and the item still on it?
		EntityAI truck = player.GetItemInHands();
		if (!player.IsAlive() || !HandTruckRules.IsHandTruck(truck) || !m_Target || m_Target.GetHierarchyParent() != truck)
		{
			Stop();
			return;
		}

		HandCartSettings settings = HandCartSettings.Get();

		if (m_Frozen)
			return;

		if (GetUApi().GetInputByName("UAHandCartRotateLeft").LocalHold())
			m_YawOffset -= ROTATE_SPEED * timeslice;
		if (GetUApi().GetInputByName("UAHandCartRotateRight").LocalHold())
			m_YawOffset += ROTATE_SPEED * timeslice;
		if (GetUApi().GetInputByID(UANextAction).LocalRelease())
			m_YawOffset -= ROTATE_STEP;
		if (GetUApi().GetInputByID(UAPrevAction).LocalRelease())
			m_YawOffset += ROTATE_STEP;

		array<Object> excluded = new array<Object>;
		excluded.Insert(player);
		excluded.Insert(truck);
		excluded.Insert(m_Target);
		excluded.Insert(m_Ghost);

		vector from = g_Game.GetCurrentCameraPosition();
		vector to = from + g_Game.GetCurrentCameraDirection() * (settings.MaxUnloadDistance + 5);
		RaycastRVParams aim = new RaycastRVParams(from, to);
		aim.type = ObjIntersectView;
		aim.sorted = true;
		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		bool hit = DayZPhysics.RaycastRVProxy(aim, hits, excluded) && hits.Count() > 0;

		vector pos = to;
		if (hit)
			pos = hits[0].pos;

		// Stand the item on the hit point: lift its origin by the box's bottom offset
		vector minMax[2];
		HandTruckRules.GetBox(m_Target, minMax);
		pos[1] = pos[1] - minMax[0][1];
		vector ori = Vector(player.GetOrientation()[0] + m_YawOffset, 0, 0);

		m_Ghost.SetPosition(pos);
		m_Ghost.SetOrientation(ori);

		excluded.Clear();
		excluded.Insert(m_Ghost);
		m_Reason = "";
		m_Valid = hit && HandTruckRules.CheckSpot(player, m_Target, pos, ori, excluded, m_Reason);
		ShowState(m_Valid);
	}

	//! Swap the ghost to the item's own deployable/undeployable hologram material,
	//! the same config entries vanilla Hologram uses. Items without them (wheels, the
	//! Expansion safes) keep their normal look; the Cancel prompt still names the problem.
	protected void ShowState(bool valid)
	{
		int state = 0;
		if (valid)
			state = 1;
		if (state == m_ShownState)
			return;
		m_ShownState = state;

		string cfg = "CfgVehicles " + m_Ghost.GetType();
		string material = g_Game.ConfigGetTextOut(cfg + " hologramMaterial");
		string path = g_Game.ConfigGetTextOut(cfg + " hologramMaterialPath");
		if (material == "" || path == "")
			return;

		string suffix = "_undeployable.rvmat";
		if (valid)
			suffix = "_deployable.rvmat";
		string rvmat = string.Format("%1\\%2%3", path, material, suffix);

		TStringArray selections = new TStringArray;
		g_Game.ConfigGetTextArray(cfg + " hiddenSelections", selections);
		foreach (string selection : selections)
		{
			int index = m_Ghost.GetHiddenSelectionIndex(selection);
			if (index < 0)
				continue;
			m_Ghost.SetObjectTexture(index, GHOST_TEXTURE);
			m_Ghost.SetObjectMaterial(index, rvmat);
		}
	}
}
