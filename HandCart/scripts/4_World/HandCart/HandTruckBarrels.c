//! Barrels ride on the hand truck full or empty, with the lid on so nothing spills; a fire
//! barrel has to be out. Vanilla opens a barrel's lid whenever it's attached to anything
//! (it expects the MS3's barrel slots, which keep barrels open), so on the truck, and on
//! the copy drawn while it's held, it's shut again straight after.
//! Vanilla also won't let a barrel out of any slot while it holds anything, and keeps it
//! open in a vehicle's slot, which makes it untakeable: so it could never be dragged
//! between an MS3 and a hand truck. A barrel in a vehicle or on a hand truck may now be
//! dragged out full (into hands it still has to be empty: CanPutIntoHands).
modded class Barrel_ColorBase
{
	override bool CanPutIntoHands(EntityAI parent)
	{
		return super.CanPutIntoHands(parent) || HandTruckRules.HoldsHandTruck(parent);
	}

	//! Vanilla's own answer: may this barrel really be held?
	bool HandCartCanHold(EntityAI parent)
	{
		return super.CanPutIntoHands(parent);
	}

	override bool IsTakeable()
	{
		return super.IsTakeable() || Transport.Cast(GetHierarchyParent()) != null;
	}

	override bool CanDetachAttachment(EntityAI parent)
	{
		return super.CanDetachAttachment(parent) || HandTruckRules.IsCarrier(parent);
	}

	override void OnWasAttached(EntityAI parent, int slot_id)
	{
		super.OnWasAttached(parent, slot_id);

		if (HandTruckRules.IsHandTruckOrCopy(parent))
			Close();
	}
}

modded class BarrelHoles_ColorBase
{
	override bool CanPutIntoHands(EntityAI parent)
	{
		return super.CanPutIntoHands(parent) || HandTruckRules.HoldsHandTruck(parent);
	}

	//! Vanilla's own answer: may this barrel really be held?
	bool HandCartCanHold(EntityAI parent)
	{
		return super.CanPutIntoHands(parent);
	}

	override bool CanDetachAttachment(EntityAI parent)
	{
		return super.CanDetachAttachment(parent) || HandTruckRules.IsCarrier(parent);
	}

	override bool CanPutAsAttachment(EntityAI parent)
	{
		if (!super.CanPutAsAttachment(parent))
			return false;

		if (HandTruckRules.IsHandTruck(parent))
			return !IsBurning();
		return true;
	}

	override void OnWasAttached(EntityAI parent, int slot_id)
	{
		super.OnWasAttached(parent, slot_id);

		if (HandTruckRules.IsHandTruckOrCopy(parent))
			Close();
	}
}

modded class PlayerBase
{
	override bool CanReceiveItemIntoHands(EntityAI item_to_hands)
	{
		if (!super.CanReceiveItemIntoHands(item_to_hands))
			return false;

		Barrel_ColorBase barrel = Barrel_ColorBase.Cast(item_to_hands);
		if (barrel)
			return barrel.HandCartCanHold(this);
		BarrelHoles_ColorBase fireBarrel = BarrelHoles_ColorBase.Cast(item_to_hands);
		if (fireBarrel)
			return fireBarrel.HandCartCanHold(this);
		return true;
	}
}
