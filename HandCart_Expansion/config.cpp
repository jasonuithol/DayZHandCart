// Optional add-on: lets the hand truck carry DayZ-Expansion-BaseBuilding safes.
// Load it alongside HandCart only on servers running Expansion BaseBuilding.
class CfgPatches
{
	class HandCart_Expansion
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"HandCart", "DayZExpansion_Objects_BaseBuilding_Safes", "DayZExpansion_BaseBuilding_Scripts"};
	};
};

class CfgMods
{
	class HandCart_Expansion
	{
		dir = "HandCart_Expansion";
		name = "Hand Truck - Expansion safes";
		type = "mod";
		dependencies[] = {"World"};

		class defs
		{
			class worldScriptModule
			{
				value = "";
				files[] = {"HandCart_Expansion/scripts/4_World"};
			};
		};
	};
};

class CfgVehicles
{
	// The safes have no inventory slots of their own. Delete a class here to stop that
	// size of safe going on the truck.
	class ExpansionSafeBase;
	class ExpansionSafeLarge: ExpansionSafeBase
	{
		inventorySlot[] = {"HandCart_Safe"};
	};
	class ExpansionSafeMedium: ExpansionSafeBase
	{
		inventorySlot[] = {"HandCart_Safe"};
	};
	class ExpansionSafeMini: ExpansionSafeBase
	{
		inventorySlot[] = {"HandCart_Safe"};
	};
};
