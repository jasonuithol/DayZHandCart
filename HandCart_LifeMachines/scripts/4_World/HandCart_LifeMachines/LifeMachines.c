//! The stove and sewing machine ride on the hand truck with their contents, but not while
//! they're working (cooking, sewing). They may go into a vehicle's cargo too (Life Machines
//! keeps them out of all cargo), so a truck can be unloaded into one.
//! Life Machines never lets them into hands, and the server only grants a player's drag
//! if the item could go into their hands (TryAcquireInventoryJunctureFromServer asks
//! CanPutIntoHands whatever the destination): so for someone holding a hand truck they
//! pass, and the player's own CanReceiveItemIntoHands keeps them out of hands as before.
modded class HM_Stove
{
	override bool CanPutInCargo(EntityAI parent)
	{
		return super.CanPutInCargo(parent) || Transport.Cast(parent) != null;
	}

	override bool CanPutAsAttachment(EntityAI parent)
	{
		if (!super.CanPutAsAttachment(parent))
			return false;

		if (HandTruckRules.IsHandTruck(parent))
			return !IsMachineWorking() && !IsLocked();
		return true;
	}

	override bool CanPutIntoHands(EntityAI parent)
	{
		return super.CanPutIntoHands(parent) || HandTruckRules.HoldsHandTruck(parent);
	}
}

modded class HM_Sewing_Machine
{
	override bool CanPutInCargo(EntityAI parent)
	{
		return super.CanPutInCargo(parent) || Transport.Cast(parent) != null;
	}

	override bool CanPutAsAttachment(EntityAI parent)
	{
		if (!super.CanPutAsAttachment(parent))
			return false;

		if (HandTruckRules.IsHandTruck(parent))
			return !IsMachineWorking() && !IsLocked();
		return true;
	}

	override bool CanPutIntoHands(EntityAI parent)
	{
		return super.CanPutIntoHands(parent) || HandTruckRules.HoldsHandTruck(parent);
	}
}

modded class PlayerBase
{
	override bool CanReceiveItemIntoHands(EntityAI item_to_hands)
	{
		if (item_to_hands && (item_to_hands.IsInherited(HM_Stove) || item_to_hands.IsInherited(HM_Sewing_Machine)))
			return false;
		return super.CanReceiveItemIntoHands(item_to_hands);
	}
}
