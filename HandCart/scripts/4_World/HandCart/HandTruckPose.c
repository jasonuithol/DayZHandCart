//! Where a held hand truck sits relative to its pusher, shared by the client (which draws
//! it, HandTruckVisual) and the server (which puts a carried person on its nose plate,
//! HandTruckRider). Worked out in the pusher's model space: x right, y up, z forward.
//!
//! The pusher's hands stay where the animation puts them, so the truck is solved around
//! them: the hands hold the truck, its wheels are on the ground, and it leans back by its
//! tilt. At the normal tilt (HoldTiltDegrees) they hold the handle; a truck carrying a
//! person stands more upright (PersonTiltDegrees), and the hands slide down the rails.
//! A client also tips (lift) and rolls it to the ground under its wheels.
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

	//! The truck's transform in the pusher's space, before any turn: wheels lift metres
	//! above their level-ground height. A truck carrying a person keeps PersonTiltDegrees
	//! and the hands slide down the rails; otherwise the hands keep the handle and the
	//! lean changes.
	static void Solve(EntityAI truck, bool person, float lift, out vector tm[4])
	{
		HandCartSettings settings = HandCartSettings.Get();
		float axleHeight;
		vector hands;
		GetHands(truck, axleHeight, hands);
		vector axle = GetAxle(truck);
		vector handleFromAxle = GetHandle(truck) - axle;
		float rise = hands[1] - (axleHeight + lift);   // hands above the axle

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
			// a*c - b*s = r*cos(tilt + alpha) = rise
			float a = handleFromAxle[1];
			float b = handleFromAxle[2];
			float r = Math.Sqrt(a * a + b * b);
			float alpha = Math.Atan2(b, a);
			tilt = (Math.Acos(Math.Clamp(rise / r, -1, 1)) - alpha) * Math.RAD2DEG;
			tilt = Math.Clamp(tilt, settings.HoldTiltDegrees - 30, Math.Min(85, settings.HoldTiltDegrees + 30));
		}

		vector rot[3];
		LeanRotation(tilt, rot);
		vector grip = GetHandle(truck) - Vector(0, slide, 0);
		tm[0] = rot[0];
		tm[1] = rot[1];
		tm[2] = rot[2];
		tm[3] = hands - Rotate(grip, rot);
	}

	//! tm turned by yaw degrees about the vertical through the hands, and rolled by roll
	//! degrees about the forward axis through them (positive raises the truck's +x side
	//! in the pusher's space)
	static void Turn(EntityAI truck, float yaw, float roll, inout vector tm[4])
	{
		float axleHeight;
		vector hands;
		GetHands(truck, axleHeight, hands);

		float cr = Math.Cos(roll * Math.DEG2RAD);
		float sr = Math.Sin(roll * Math.DEG2RAD);
		float cy = Math.Cos(yaw * Math.DEG2RAD);
		float sy = Math.Sin(yaw * Math.DEG2RAD);
		vector m[3];
		// roll about z, then yaw about y (positive yaw turns forward towards +x)
		m[0] = Vector(cr * cy, sr, -cr * sy);
		m[1] = Vector(-sr * cy, cr, sr * sy);
		m[2] = Vector(sy, 0, cy);

		for (int i = 0; i < 3; i++)
			tm[i] = Rotate(tm[i], m);
		tm[3] = hands + Rotate(tm[3] - hands, m);
	}

	//! How far in front of the pusher the axle is: what they step round when they turn
	static float GetPivot(HandCart_HandTruck_Base truck)
	{
		vector tm[4];
		Solve(truck, truck.HasRider(), 0, tm);
		vector axle = Apply(GetAxle(truck), tm);
		return axle[2];
	}

	//! Horizontal distance from the hands to the axle in a pose
	static float Reach(EntityAI truck, vector tm[4])
	{
		float axleHeight;
		vector hands;
		GetHands(truck, axleHeight, hands);
		vector axle = Apply(GetAxle(truck), tm);
		return Math.Sqrt(Math.SqrFloat(axle[0] - hands[0]) + Math.SqrFloat(axle[2] - hands[2]));
	}

	//! Server and clients alike: where a person carried on pusher's truck stands, in the
	//! world. Upright on the nose plate, facing the way the truck points; on whatever
	//! floor is under it.
	static void GetRiderTransform(PlayerBase pusher, HandCart_HandTruck_Base truck, PlayerBase rider, out vector tm[4])
	{
		vector pose[4];
		Solve(truck, true, 0, pose);
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
