// Optional add-on: lets the hand truck carry Life Machines' (huntermods.online) stove and
// sewing machine, which otherwise can't be moved once deployed. Load it alongside
// HandCart only on servers running Life Machines.
class CfgPatches
{
	class HandCart_LifeMachines
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"HandCart", "HM_Life_Machines_Scripts"};
	};
};

class CfgMods
{
	class HandCart_LifeMachines
	{
		dir = "HandCart_LifeMachines";
		name = "Hand Truck - Life Machines";
		type = "mod";
		dependencies[] = {"World"};

		class defs
		{
			class worldScriptModule
			{
				value = "";
				files[] = {"HandCart_LifeMachines/scripts/4_World"};
			};
		};
	};
};

class CfgVehicles
{
	// Neither has an inventory slot of its own. Delete a class here to keep that machine
	// off the truck. They may also go into a vehicle's cargo (LifeMachines.c), so they get
	// a size there: neither had one, so each took a single cell.
	class Container_Base;
	class HM_Stove: Container_Base
	{
		inventorySlot[] = {"HandCart_Stove"};
		itemSize[] = {8, 8};
	};
	class HM_Sewing_Machine: Container_Base
	{
		inventorySlot[] = {"HandCart_SewingMachine"};
		itemSize[] = {10, 6};
	};
};
