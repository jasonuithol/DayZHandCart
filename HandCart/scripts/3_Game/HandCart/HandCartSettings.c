//! Server-side configuration, loaded from $profile:HandCart/HandCartSettings.json
//! (written with defaults on first start) and sent to each client on connect so
//! the client applies the same limits the server enforces.
class HandCartSettings
{
	static const string DIR = "$profile:HandCart";
	static const string PATH = DIR + "/HandCartSettings.json";
	static const int RPC_SYNC = 48201731;
	static const int VERSION = 9;

	int Version;
	float MaxUnloadDistance;    //!< metres from the player to where an unloaded item is set down
	float MaxSlopeDegrees;      //!< steepest ground an item may be set down on
	bool RequireLineOfSight;    //!< player must be able to see the spot (no unloading through walls)
	float LoadTime;             //!< unused: loading is instant (kept so existing settings files still read)
	float UnloadTime;           //!< seconds to set one item down
	float SoftGroundSpeed;      //!< loaded movement speed off-road: 1 = walk, lower is slower
	float HoldTiltDegrees;      //!< how far the held truck leans back towards the player
	float HoldGripForward;      //!< metres in front of the player the held truck's grip is
	float HoldLift;             //!< metres the held truck is raised above the player's feet
	float SoundVolume;          //!< rolling and clanking sounds: 1 = normal, 0 = off
	bool AllowPeople;           //!< tied-up players can be loaded
	float PersonTiltDegrees;    //!< how far a truck carrying a person leans back (bodies stay upright)
	ref array<string> HardSurfaces;   //!< surface names always treated as road
	ref array<string> SoftSurfaces;   //!< surface names always treated as dirt
	ref array<string> BlockedItems;   //!< class names (subclasses too) that may never be loaded

	protected static ref HandCartSettings s_Instance;

	void HandCartSettings()
	{
		Version = VERSION;
		MaxUnloadDistance = 2.0;
		MaxSlopeDegrees = 30.0;
		RequireLineOfSight = true;
		LoadTime = 2.0;
		UnloadTime = 1.5;
		SoftGroundSpeed = 0.6;
		HoldTiltDegrees = 50.0;
		HoldGripForward = 0.35;
		HoldLift = 0.7;
		SoundVolume = 1.0;
		AllowPeople = true;
		PersonTiltDegrees = 15.0;
		HardSurfaces = {};
		SoftSurfaces = {};
		BlockedItems = {};
	}

	static HandCartSettings Get()
	{
		if (!s_Instance)
			s_Instance = new HandCartSettings();
		return s_Instance;
	}

	static void LoadServer()
	{
		HandCartSettings settings = new HandCartSettings();
		string error;
		if (FileExist(PATH))
		{
			if (!JsonFileLoader<HandCartSettings>.LoadFile(PATH, settings, error))
			{
				ErrorEx("[HandCart] " + error + " - using defaults");
				settings = new HandCartSettings();
			}
			else if (settings.Version < VERSION)
			{
				// Fields added since the file was written keep their defaults; save so they show up
				if (settings.Version < 4 && settings.MaxUnloadDistance == 10.0)
					settings.MaxUnloadDistance = 2.0;   // the old default
				if (settings.Version < 8 && Math.AbsFloat(settings.HoldGripForward - 0.45) < 0.001)
					settings.HoldGripForward = 0.35;    // the old default
				if (settings.Version < 8 && Math.AbsFloat(settings.HoldLift - 1.0) < 0.001)
					settings.HoldLift = 0.7;
				settings.Version = VERSION;
				JsonFileLoader<HandCartSettings>.SaveFile(PATH, settings, error);
			}
		}
		else
		{
			MakeDirectory(DIR);
			JsonFileLoader<HandCartSettings>.SaveFile(PATH, settings, error);
		}
		s_Instance = settings;
	}

	void Write(ParamsWriteContext ctx)
	{
		ctx.Write(MaxUnloadDistance);
		ctx.Write(MaxSlopeDegrees);
		ctx.Write(RequireLineOfSight);
		ctx.Write(LoadTime);
		ctx.Write(UnloadTime);
		ctx.Write(SoftGroundSpeed);
		ctx.Write(HoldTiltDegrees);
		ctx.Write(HoldGripForward);
		ctx.Write(HoldLift);
		ctx.Write(SoundVolume);
		ctx.Write(AllowPeople);
		ctx.Write(PersonTiltDegrees);
		ctx.Write(HardSurfaces);
		ctx.Write(SoftSurfaces);
		ctx.Write(BlockedItems);
	}

	bool Read(ParamsReadContext ctx)
	{
		// One read per statement: Enforce won't parse an expression continued over lines
		if (!ctx.Read(MaxUnloadDistance) || !ctx.Read(MaxSlopeDegrees) || !ctx.Read(RequireLineOfSight))
			return false;
		if (!ctx.Read(LoadTime) || !ctx.Read(UnloadTime) || !ctx.Read(SoftGroundSpeed))
			return false;
		if (!ctx.Read(HoldTiltDegrees) || !ctx.Read(HoldGripForward) || !ctx.Read(HoldLift))
			return false;
		if (!ctx.Read(SoundVolume) || !ctx.Read(AllowPeople) || !ctx.Read(PersonTiltDegrees))
			return false;
		return ctx.Read(HardSurfaces) && ctx.Read(SoftSurfaces) && ctx.Read(BlockedItems);
	}
}
