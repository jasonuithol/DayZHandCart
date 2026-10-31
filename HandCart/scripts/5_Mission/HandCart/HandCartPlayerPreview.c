//! The inventory screen's character preview is a separate dummy that vanilla hands a copy
//! of the real item in hands every update. For a hand truck that draws the truck hanging
//! off the hand (the in-world one is hidden and replaced by HandTruckVisual, which the
//! dummy doesn't get), so the preview shows empty hands instead. The rest is vanilla's
//! UpdateInterval, which runs as normal for any other item.
modded class PlayerPreview
{
	override void UpdateInterval()
	{
		if (!HandTruckRules.IsHandTruck(g_Game.GetPlayer().GetEntityInHands()))
		{
			super.UpdateInterval();
			return;
		}

		m_CharacterPanelWidget.UpdateItemInHands(null);

		PlayerBase realPlayer = PlayerBase.Cast(g_Game.GetPlayer());
		DayZPlayer dummyPlayer = m_CharacterPanelWidget.GetDummyPlayer();
		if (realPlayer && dummyPlayer)
		{
			HumanCommandAdditives hca = dummyPlayer.GetCommandModifier_Additives();
			if (hca && realPlayer.m_InjuryHandler)
				hca.SetInjured(realPlayer.m_InjuryHandler.GetInjuryAnimValue(), realPlayer.m_InjuryHandler.IsInjuryAnimEnabled());
		}

		if (m_IsHolding)
		{
			int mouse_x;
			int mouse_y;
			GetMousePos(mouse_x, mouse_y);
			if (GetMouseState(MouseState.LEFT) & 0x80000000)
			{
				UpdateRotation(mouse_x, mouse_y, true);
			}
			else
			{
				UpdateRotation(mouse_x, mouse_y, false);
				m_IsHolding = false;
			}
		}
	}
}
