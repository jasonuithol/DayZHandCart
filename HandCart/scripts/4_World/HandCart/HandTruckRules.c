//! Rules shared by client and server: what can be loaded, and whether a proposed spot
//! to unload onto is acceptable. The client uses these to offer actions and colour the
//! ghost; the server repeats them before changing anything, so a modified client gains nothing.
class HandTruckRules
{
	static bool IsHandTruck(EntityAI item)
	{
		return item && item.IsInherited(HandCart_HandTruck_Base);
	}

	//! Is this a player with a hand truck in hands?
	static bool HoldsHandTruck(EntityAI entity)
	{
		Man man = Man.Cast(entity);
		return man && IsHandTruck(man.GetHumanInventory().GetEntityInHands());
	}

	//! A vehicle or a hand truck: what a full barrel may be dragged out of
	static bool IsCarrier(EntityAI entity)
	{
		return IsHandTruck(entity) || Transport.Cast(entity) != null;
	}

	//! The truck, or the copy drawn in its place while it's held (HandTruckVisual)
	static bool IsHandTruckOrCopy(EntityAI item)
	{
		return IsHandTruck(item) || (item && item.IsInherited(HandCart_HandTruckVisual));
	}

	//! Can target go onto the truck right now? It must be lying loose in the world, or sit
	//! directly in a vehicle (one of its slots, like an MS3's barrel slots, or its cargo).
	static bool CanLoad(EntityAI truck, EntityAI target)
	{
		ItemBase item = ItemBase.Cast(target);
		if (!item || item.IsHologram() || item.IsSetForDeletion())
			return false;

		EntityAI parent = item.GetHierarchyParent();
		if (parent && !CanTakeFromVehicle(Transport.Cast(parent), item))
			return false;

		return HandTruckLoad.FindFreeSlot(truck, item) != InventorySlots.INVALID;
	}

	//! item is directly in vehicle and the vehicle lets it out (vanilla's own checks, e.g.
	//! a closed car trunk)
	static bool CanTakeFromVehicle(Transport vehicle, EntityAI item)
	{
		if (!vehicle || item.GetHierarchyParent() != vehicle || !vehicle.GetInventory().AreChildrenAccessible())
			return false;

		// Mounted wheels are the vehicle's own (spares in its cargo are fair game)
		if (item.GetInventory().IsAttachment())
			return !item.IsInherited(CarWheel) && vehicle.CanReleaseAttachment(item);
		return vehicle.CanReleaseCargo(item);
	}

	//! What the truck would take out of vehicle next: something in the vehicle's slots
	//! first (an MS3's barrels and crates), then its cargo; null if nothing fits.
	static EntityAI FindVehicleLoad(EntityAI truck, Transport vehicle)
	{
		GameInventory inv = vehicle.GetInventory();
		for (int i = 0; i < inv.AttachmentCount(); i++)
		{
			EntityAI att = inv.GetAttachmentFromIndex(i);
			if (CanLoad(truck, att))
				return att;
		}

		CargoBase cargo = inv.GetCargo();
		if (!cargo)
			return null;
		for (int j = 0; j < cargo.GetItemCount(); j++)
		{
			EntityAI item = cargo.GetItem(j);
			if (CanLoad(truck, item))
				return item;
		}
		return null;
	}

	//! Where the truck's top item would go in vehicle: a slot made for it first (an MS3's
	//! barrel or crate slots), else the cargo. Vanilla's rules apply: a full barrel only
	//! goes in a barrel slot.
	static bool FindVehicleSpot(EntityAI truck, Transport vehicle, out InventoryLocation dst)
	{
		EntityAI top = HandTruckLoad.GetTopItem(truck);
		if (!top || !vehicle.GetInventory().AreChildrenAccessible())
			return false;

		dst = new InventoryLocation();
		if (vehicle.GetInventory().FindFreeLocationFor(top, FindInventoryLocationType.ATTACHMENT, dst))
			return true;
		return vehicle.GetInventory().FindFreeLocationFor(top, FindInventoryLocationType.CARGO, dst);
	}

	//! Server: put the truck's top item into vehicle (see FindVehicleSpot), synced to every client.
	static bool UnloadIntoVehicleServer(HandCart_HandTruck_Base truck, Transport vehicle)
	{
		InventoryLocation dst;
		if (!FindVehicleSpot(truck, vehicle, dst))
			return false;

		InventoryLocation src = new InventoryLocation();
		dst.GetItem().GetInventory().GetCurrentInventoryLocation(src);
		bool moved = GameInventory.LocationSyncMoveEntity(src, dst);
		if (moved)
			InventoryInputUserData.SendServerMove(null, InventoryCommandType.SYNC_MOVE, src, dst);
		return moved;
	}

	//! Model-space bounding box of a container (falls back like vanilla Hologram does).
	static void GetBox(EntityAI entity, out vector minMax[2])
	{
		if (entity.GetCollisionBox(minMax))
			return;

		if (entity.MemoryPointExists("box_placing_min") && entity.MemoryPointExists("box_placing_max"))
		{
			minMax[0] = entity.GetMemoryPointPos("box_placing_min");
			minMax[1] = entity.GetMemoryPointPos("box_placing_max");
			return;
		}

		minMax[0] = Vector(-0.4, 0, -0.4);
		minMax[1] = Vector(0.4, 0.8, 0.4);
	}

	//! Can target (carried on the player's truck) be set down with its origin at pos, rotated to ori?
	//! ignore: extra objects to leave out of the tests (the client's ghost).
	static bool CheckSpot(PlayerBase player, EntityAI target, vector pos, vector ori, array<Object> ignore, out string reason)
	{
		HandCartSettings settings = HandCartSettings.Get();
		if (vector.Distance(player.GetPosition(), pos) > settings.MaxUnloadDistance)
		{
			reason = "Too far away";
			return false;
		}

		array<Object> excluded = new array<Object>;
		excluded.Insert(player);
		excluded.Insert(target);
		if (player.GetItemInHands())
			excluded.Insert(player.GetItemInHands());
		if (ignore)
			excluded.InsertAll(ignore);

		vector minMax[2];
		GetBox(target, minMax);
		vector rot[3];
		Math3D.YawPitchRollMatrix(ori, rot);
		vector offset = (minMax[0] + minMax[1]) * 0.5;
		vector centre = pos + offset.Multiply3(rot);
		vector bottom = Vector(centre[0], pos[1] + minMax[0][1], centre[2]);

		// Something solid underneath, and not too steep: ground heights under the centre
		// and the four bottom corners. (A raycast result's dir is not the surface normal.)
		float groundCentre;
		if (!GroundHeight(bottom, excluded, groundCentre))
		{
			reason = "There's nothing to set it down on";
			return false;
		}
		float maxTan = Math.Tan(settings.MaxSlopeDegrees * Math.DEG2RAD);
		for (int c = 0; c < 4; c++)
		{
			vector corner = Vector(minMax[c % 2][0], minMax[0][1], minMax[c / 2][2]);
			vector cornerWS = pos + corner.Multiply3(rot);
			float ground;
			if (!GroundHeight(cornerWS, excluded, ground))
				continue;   // overhanging an edge is fine; the centre is supported
			float run = Math.Sqrt(Math.SqrFloat(cornerWS[0] - bottom[0]) + Math.SqrFloat(cornerWS[2] - bottom[2]));
			if (run > 0.05 && Math.AbsFloat(ground - groundCentre) / run > maxTan)
			{
				reason = "The ground is too steep";
				return false;
			}
		}

		if (g_Game.GetWaterDepth(bottom) > 0)
		{
			reason = "It can't go in water";
			return false;
		}

		// Box lifted off the floor and shrunk slightly, so resting on the floor or
		// touching a neighbour doesn't count as a collision (same idea as vanilla)
		vector size = (minMax[1] - minMax[0]) * 0.95;
		if (g_Game.IsBoxCollidingGeometry(centre + Vector(0, 0.05, 0), ori, size, ObjIntersectFire, ObjIntersectGeom, excluded))
		{
			reason = "Something is in the way";
			return false;
		}

		if (settings.RequireLineOfSight)
		{
			vector eye = player.GetBonePositionWS(player.GetBoneIndexByName("Head"));
			RaycastRVParams los = new RaycastRVParams(eye, bottom + Vector(0, 0.3, 0));
			los.type = ObjIntersectView;
			array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
			if (DayZPhysics.RaycastRVProxy(los, hits, excluded) && hits.Count() > 0)
			{
				reason = "You can't see that spot";
				return false;
			}
		}

		return true;
	}

	//! Height of whatever solid is under point (from 0.3 above to 0.3 below it).
	static bool GroundHeight(vector point, array<Object> excluded, out float height)
	{
		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		RaycastRVParams down = new RaycastRVParams(point + Vector(0, 0.3, 0), point - Vector(0, 0.3, 0));
		down.type = ObjIntersectGeom;
		down.sorted = true;
		if (!DayZPhysics.RaycastRVProxy(down, hits, excluded) || hits.Count() == 0)
			return false;
		height = hits[0].pos[1];
		return true;
	}

	//! Server: put target on the truck, contents and all, synced to every client. Moved
	//! directly: the player's inventory routes the move through a juncture, which the Load
	//! action's own reservation of the target refuses.
	static bool LoadServer(EntityAI truck, EntityAI target)
	{
		int slotId = HandTruckLoad.FindFreeSlot(truck, target);
		if (slotId == InventorySlots.INVALID)
			return false;

		InventoryLocation src = new InventoryLocation();
		if (!target.GetInventory().GetCurrentInventoryLocation(src))
			return false;
		InventoryLocation dst = new InventoryLocation();
		dst.SetAttachment(truck, target, slotId);

		if (!GameInventory.LocationSyncMoveEntity(src, dst))
			return false;
		InventoryInputUserData.SendServerMove(null, InventoryCommandType.SYNC_MOVE, src, dst);
		return true;
	}

	//! Server: take target off the truck and set it down at pos/ori, cargo and all,
	//! synced to every client.
	static bool UnloadServer(HandCart_HandTruck_Base truck, EntityAI target, vector pos, vector ori)
	{
		vector rot[3];
		Math3D.YawPitchRollMatrix(ori, rot);
		vector transform[4];
		transform[0] = rot[0];
		transform[1] = rot[1];
		transform[2] = rot[2];
		transform[3] = pos;

		InventoryLocation src = new InventoryLocation();
		target.GetInventory().GetCurrentInventoryLocation(src);
		InventoryLocation dst = new InventoryLocation();
		dst.SetGround(target, transform);

		// Moved here and then sent to the clients, as GameInventory.TakeToDst does in SERVER
		// mode (a bare LocationSyncMoveEntity only moves it on the server). Not through the
		// holder's ServerTakeToDst, which queues the move as a juncture.
		bool moved = GameInventory.LocationSyncMoveEntity(src, dst);
		if (moved)
			InventoryInputUserData.SendServerMove(null, InventoryCommandType.SYNC_MOVE, src, dst);
		return moved;
	}
}
