//! A safe goes onto a hand truck locked or not, but only with its door shut. It stays
//! locked on the truck and wherever it's unloaded, so the thief still needs the code.
//! Checked by the engine whenever the truck tests or makes the attachment
//! (HandTruckLoad.FindFreeSlot and the server-side load).
modded class ExpansionSafeBase
{
	override bool CanPutAsAttachment(EntityAI parent)
	{
		if (!super.CanPutAsAttachment(parent))
			return false;

		if (HandTruckRules.IsHandTruck(parent))
			return !ExpansionIsOpened();

		return true;
	}
}
