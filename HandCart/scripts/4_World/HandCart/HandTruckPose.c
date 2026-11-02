//! Where a held hand truck sits relative to its pusher, shared by the client (which draws
//! it, HandTruckVisual) and the server (which puts a carried person on its nose plate,
//! HandTruckRider). Worked out in the pusher's model space: x right, y up, z forward.
//! Fixed to the pusher: no terrain following, no steering of its own.
//!
//! The hands hold the truck, its wheels HoldLift above the pusher's origin, leaning back by
//! HoldTiltDegrees with the hands on the handle. A truck carrying a person stands more
//! upright (PersonTiltDegrees, a body can't be tilted) and the hands slide down the rails.
class HandTruckPose
{
	//! Nose plate centre in the truck model, where a carried person stands (see
	//! blender/build_handcart.py: the load sits 0.03 in front of the rails)
	static const vector NOSE = "0 0.008 -0.2";
	static const float MAX_RAIL_SLIDE = 0.9;   //!< metres the hands may slide down the rails

	//! In the truck model the nose plate points down -z and up is +y
	static vector GetAxle(EntityAI truck)
	{
		return (truck.GetMemoryPointPos("wheel_l_axis_1") + truck.GetMemoryPointPos("wheel_r_axis_2")) * 0.5;
	}

	static vector GetHandle(EntityAI truck)
	{
		return truck.GetMemoryPointPos("handle");
	}

	//! Rotation of a truck leaning back by tilt degrees, nose forward (the model's -z to +z)
	static void LeanRotation(float tilt, out vector rot[3])
	{
		float c = Math.Cos(tilt * Math.DEG2RAD);
		float s = Math.Sin(tilt * Math.DEG2RAD);
		rot[0] = "-1 0 0";            // truck x: completes the frame (up x z)
		rot[1] = Vector(0, c, -s);    // truck up: leaning back towards the pusher
		rot[2] = Vector(0, -s, -c);   // truck z: nose (-z) forward and tipped up
	}

	static vector Rotate(vector v, vector rot[3])
	{
		return rot[0] * v[0] + rot[1] * v[1] + rot[2] * v[2];
	}

	//! A point in the truck model to where pose tm puts it
	static vector Apply(vector v, vector tm[4])
	{
		return tm[3] + tm[0] * v[0] + tm[1] * v[1] + tm[2] * v[2];
	}

	//! Axle height and hand position in the pusher's space: the normal pose on level
	//! ground, wheels HoldLift above the pusher's origin, handle HoldGripForward ahead
	static void GetHands(EntityAI truck, out float axleHeight, out vector hands)
	{
		HandCartSettings settings = HandCartSettings.Get();
		vector rot[3];
		LeanRotation(settings.HoldTiltDegrees, rot);
		vector axle = GetAxle(truck);
		axleHeight = axle[1] + settings.HoldLift;
		vector handleFromAxle = Rotate(GetHandle(truck) - axle, rot);
		hands = Vector(0, axleHeight + handleFromAxle[1], settings.HoldGripForward);
	}

	//! The truck's transform in the pusher's space. Carrying a person it leans back
	//! PersonTiltDegrees with the hands down the rails; otherwise HoldTiltDegrees with the
	//! hands on the handle.
	static void Solve(EntityAI truck, bool person, out vector tm[4])
	{
		HandCartSettings settings = HandCartSettings.Get();
		float axleHeight;
		vector hands;
		GetHands(truck, axleHeight, hands);
		vector axle = GetAxle(truck);
		vector handleFromAxle = GetHandle(truck) - axle;
		float rise = hands[1] - axleHeight;   // hands above the axle

		float tilt;
		float slide = 0;
		if (person)
		{
			// The rotated handle-to-axle's vertical part is a*c - b*s (LeanRotation), with
			// a, b the handle's height and depth from the axle; slide shortens a
			tilt = settings.PersonTiltDegrees;
			float c = Math.Cos(tilt * Math.DEG2RAD);
			float s = Math.Sin(tilt * Math.DEG2RAD);
			slide = (handleFromAxle[1] * c - handleFromAxle[2] * s - rise) / c;
			slide = Math.Clamp(slide, 0, MAX_RAIL_SLIDE);
		}
		else
		{
			tilt = settings.HoldTiltDegrees;
		}

		vector rot[3];
		LeanRotation(tilt, rot);
		vector grip = GetHandle(truck) - Vector(0, slide, 0);
		tm[0] = rot[0];
		tm[1] = rot[1];
		tm[2] = rot[2];
		tm[3] = hands - Rotate(grip, rot);
	}

	//! Server and clients alike: where a person carried on pusher's truck stands, in the
	//! world. Upright on the nose plate, facing the way the truck points; on whatever
	//! floor is under it.
	static void GetRiderTransform(PlayerBase pusher, HandCart_HandTruck_Base truck, PlayerBase rider, out vector tm[4])
	{
		vector pose[4];
		Solve(truck, true, pose);
		vector nose = Apply(NOSE, pose);

		vector p[4];
		pusher.GetTransform(p);
		vector spot = p[3] + p[0] * nose[0] + p[2] * nose[2];
		vector dir = p[2];
		dir[1] = 0;
		dir.Normalize();

		vector feet = p[3];
		float floor = FloorHeight(spot, feet[1], pusher, truck, rider);
		spot[1] = floor;
		tm[0] = Vector(dir[2], 0, -dir[0]);
		tm[1] = Vector(0, 1, 0);
		tm[2] = dir;
		tm[3] = spot;
	}

	//! Height of the floor or ground at point, looking from near the pusher's feet height
	//! (so a floor above or a cellar below doesn't count); near if there's nothing there
	static float FloorHeight(vector point, float near, PlayerBase pusher, EntityAI truck, EntityAI rider)
	{
		array<Object> excluded = new array<Object>;
		excluded.Insert(pusher);
		excluded.Insert(truck);
		if (rider)
			excluded.Insert(rider);

		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		RaycastRVParams down = new RaycastRVParams(Vector(point[0], near + 0.8, point[2]), Vector(point[0], near - 0.8, point[2]));
		down.type = ObjIntersectGeom;
		down.sorted = true;
		if (DayZPhysics.RaycastRVProxy(down, hits, excluded) && hits.Count() > 0)
			return hits[0].pos[1];
		return near;
	}
}
