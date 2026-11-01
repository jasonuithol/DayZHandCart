class CfgPatches
{
	class HandCart
	{
		units[] = {"HandCart_HandTruck_Blue", "HandCart_HandTruck_Red", "HandCart_HandTruck_Yellow"};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data", "DZ_Scripts", "DZ_Gear_Camping", "DZ_Vehicles_Wheeled", "DZ_Sounds_Effects"};
	};
};

class CfgMods
{
	class HandCart
	{
		dir = "HandCart";
		name = "Hand Truck";
		type = "mod";
		inputs = "HandCart/data/inputs.xml";
		dependencies[] = {"Game", "World", "Mission"};

		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"HandCart/scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"HandCart/scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"HandCart/scripts/5_Mission"};
			};
		};
	};
};

class CfgVehicles
{
	class Inventory_Base;

	class HandCart_HandTruck_Base: Inventory_Base
	{
		scope = 0;
		displayName = "Hand Truck";
		descriptionShort = "A battered steel hand truck. Loads a safe, two sea chests, four wooden crates, a dual truck wheel, two truck wheels or three car wheels, contents and all.";
		model = "\HandCart\handcart.p3d";
		weight = 15000;
		itemSize[] = {10, 15};
		itemBehaviour = 0;            // heavy: carried in both hands, never in cargo (movement rules in HandTruckMovement.c)
		physLayer = "item_large";
		rotationFlags = 0;
		hiddenSelections[] = {"camo"};
		hiddenSelectionsTextures[] = {"HandCart\data\handcart_frame_blue_co.paa"};
		attachments[] =
		{
			"HandCart_Safe",
			"HandCart_SeaChest1",
			"HandCart_SeaChest2",
			"HandCart_Crate1",
			"HandCart_Crate2",
			"HandCart_Crate3",
			"HandCart_Crate4",
			"HandCart_DualWheel",
			"HandCart_TruckWheel1",
			"HandCart_TruckWheel2",
			"HandCart_CarWheel1",
			"HandCart_CarWheel2",
			"HandCart_CarWheel3",
			"HandCart_Generator",
			"HandCart_CanopyTent1",
			"HandCart_CanopyTent2",
			"HandCart_CanopyTent3",
			"HandCart_Barrel",
			"HandCart_Stove",
			"HandCart_SewingMachine"
		};
		hiddenSelectionsMaterials[] = {"HandCart\data\handcart_frame.rvmat"};

		class DamageSystem
		{
			class GlobalHealth
			{
				class Health
				{
					hitpoints = 400;
					// Pristine and worn look new; damaged and badly damaged get vanilla's generic
					// scratches and dents, ruined its generic wreck overlay
					healthLevels[] =
					{
						{1.0, {"HandCart\data\handcart_frame.rvmat", "HandCart\data\handcart_wheels.rvmat"}},
						{0.7, {"HandCart\data\handcart_frame.rvmat", "HandCart\data\handcart_wheels.rvmat"}},
						{0.5, {"HandCart\data\handcart_frame_damage.rvmat", "HandCart\data\handcart_wheels_damage.rvmat"}},
						{0.3, {"HandCart\data\handcart_frame_damage.rvmat", "HandCart\data\handcart_wheels_damage.rvmat"}},
						{0.0, {"HandCart\data\handcart_frame_destruct.rvmat", "HandCart\data\handcart_wheels_destruct.rvmat"}}
					};
				};
			};
		};
	};

	// What players see while a truck is held (HandTruckVisual.c): a local, client-only copy
	// stood in front of the player, carrying copies of the load. No collision LODs.
	class HandCart_HandTruckVisual: Inventory_Base
	{
		scope = 1;
		displayName = "Hand Truck";
		model = "\HandCart\handcart_visual.p3d";
		hiddenSelections[] = {"camo"};
		hiddenSelectionsTextures[] = {"HandCart\data\handcart_frame_blue_co.paa"};
		hiddenSelectionsMaterials[] = {"HandCart\data\handcart_frame.rvmat"};
		attachments[] =
		{
			"HandCart_Safe",
			"HandCart_SeaChest1",
			"HandCart_SeaChest2",
			"HandCart_Crate1",
			"HandCart_Crate2",
			"HandCart_Crate3",
			"HandCart_Crate4",
			"HandCart_DualWheel",
			"HandCart_TruckWheel1",
			"HandCart_TruckWheel2",
			"HandCart_CarWheel1",
			"HandCart_CarWheel2",
			"HandCart_CarWheel3",
			"HandCart_Generator",
			"HandCart_CanopyTent1",
			"HandCart_CanopyTent2",
			"HandCart_CanopyTent3",
			"HandCart_Barrel",
			"HandCart_Stove",
			"HandCart_SewingMachine"
		};
	};

	class HandCart_HandTruck_Blue: HandCart_HandTruck_Base
	{
		scope = 2;
		hiddenSelectionsTextures[] = {"HandCart\data\handcart_frame_blue_co.paa"};
	};
	class HandCart_HandTruck_Red: HandCart_HandTruck_Base
	{
		scope = 2;
		hiddenSelectionsTextures[] = {"HandCart\data\handcart_frame_red_co.paa"};
	};
	class HandCart_HandTruck_Yellow: HandCart_HandTruck_Base
	{
		scope = 2;
		hiddenSelectionsTextures[] = {"HandCart\data\handcart_frame_yellow_co.paa"};
	};

	// Loadable items: their own slots plus the hand truck's. Vanilla slots are repeated
	// in full rather than appended with +=, which has been unreliable in DayZ configs.
	class Container_Base;
	class SeaChest: Container_Base
	{
		inventorySlot[] = {"HandCart_SeaChest1", "HandCart_SeaChest2"};
	};
	class WoodenCrate: Container_Base
	{
		inventorySlot[] = {"Truck_01_WoodenCrate1", "Truck_01_WoodenCrate2", "Truck_01_WoodenCrate3", "Truck_01_WoodenCrate4", "HandCart_Crate1", "HandCart_Crate2", "HandCart_Crate3", "HandCart_Crate4"};
	};

	class CarWheel;
	class HatchbackWheel: CarWheel
	{
		inventorySlot[] = {"NivaWheel_1_1", "NivaWheel_1_2", "NivaWheel_2_1", "NivaWheel_2_2", "NivaWheel_Spare_1", "HandCart_CarWheel1", "HandCart_CarWheel2", "HandCart_CarWheel3"};
	};
	class CivSedanWheel: CarWheel
	{
		inventorySlot[] = {"CivSedanWheel_1_1", "CivSedanWheel_1_2", "CivSedanWheel_2_1", "CivSedanWheel_2_2", "CivSedanWheel_Spare_1", "HandCart_CarWheel1", "HandCart_CarWheel2", "HandCart_CarWheel3"};
	};
	class Hatchback_02_Wheel: CarWheel
	{
		inventorySlot[] = {"Hatchback_02_Wheel_1_1", "Hatchback_02_Wheel_1_2", "Hatchback_02_Wheel_2_1", "Hatchback_02_Wheel_2_2", "Hatchback_02_Wheel_Spare_1", "HandCart_CarWheel1", "HandCart_CarWheel2", "HandCart_CarWheel3"};
	};
	class Sedan_02_Wheel: CarWheel
	{
		inventorySlot[] = {"Sedan_02_Wheel_1_1", "Sedan_02_Wheel_1_2", "Sedan_02_Wheel_2_1", "Sedan_02_Wheel_2_2", "Sedan_02_Wheel_Spare_1", "HandCart_CarWheel1", "HandCart_CarWheel2", "HandCart_CarWheel3"};
	};
	// MS3 (V3S) single and dual wheels, M1025 wheel
	class Truck_01_Wheel: CarWheel
	{
		inventorySlot[] = {"Truck_01_Wheel_1_1", "Truck_01_Wheel_2_1", "Truck_01_Wheel_Spare_1", "Truck_01_Wheel_Spare_2", "HandCart_TruckWheel1", "HandCart_TruckWheel2"};
	};
	class Truck_01_WheelDouble: Truck_01_Wheel
	{
		inventorySlot[] = {"Truck_01_Wheel_1_2", "Truck_01_Wheel_1_3", "Truck_01_Wheel_2_2", "Truck_01_Wheel_2_3", "HandCart_DualWheel"};
	};
	// Barrels and fire barrels: the MS3's barrel slots plus the hand truck's. Full or
	// empty, lid on (HandTruckBarrels.c).
	class Barrel_ColorBase: Container_Base
	{
		inventorySlot[] = {"Truck_01_Barrel1", "Truck_01_Barrel2", "Truck_01_Barrel3", "Truck_01_Barrel4", "HandCart_Barrel"};
	};
	class FireplaceBase;
	class BarrelHoles_ColorBase: FireplaceBase
	{
		inventorySlot[] = {"Truck_01_Barrel1", "Truck_01_Barrel2", "Truck_01_Barrel3", "Truck_01_Barrel4", "HandCart_Barrel"};
	};
	// Neither has an inventory slot in vanilla. The canopy tent (PartyTent and its colours)
	// only loads packed (HandTruckLoad.GetKindOf).
	class PowerGenerator: Inventory_Base
	{
		inventorySlot[] = {"HandCart_Generator"};
	};
	class TentBase;
	class PartyTent: TentBase
	{
		inventorySlot[] = {"HandCart_CanopyTent1", "HandCart_CanopyTent2", "HandCart_CanopyTent3"};
	};
	class Offroad_02_Wheel: CarWheel
	{
		inventorySlot[] = {"Offroad_02_Wheel_1_1", "Offroad_02_Wheel_1_2", "Offroad_02_Wheel_2_1", "Offroad_02_Wheel_2_2", "Offroad_02_Wheel_Spare_1", "HandCart_TruckWheel1", "HandCart_TruckWheel2"};
	};
};

// One stack of slots per kind of load; the truck carries one kind at a time (HandTruckLoad.c).
class CfgSlots
{
	class Slot_HandCart_Safe
	{
		name = "HandCart_Safe";
		displayName = "Safe";
		ghostIcon = "";
	};
	class Slot_HandCart_SeaChest1
	{
		name = "HandCart_SeaChest1";
		displayName = "Sea chest";
		ghostIcon = "";
	};
	class Slot_HandCart_SeaChest2
	{
		name = "HandCart_SeaChest2";
		displayName = "Sea chest";
		ghostIcon = "";
	};
	class Slot_HandCart_Crate1
	{
		name = "HandCart_Crate1";
		displayName = "Wooden crate";
		ghostIcon = "";
	};
	class Slot_HandCart_Crate2
	{
		name = "HandCart_Crate2";
		displayName = "Wooden crate";
		ghostIcon = "";
	};
	class Slot_HandCart_Crate3
	{
		name = "HandCart_Crate3";
		displayName = "Wooden crate";
		ghostIcon = "";
	};
	class Slot_HandCart_Crate4
	{
		name = "HandCart_Crate4";
		displayName = "Wooden crate";
		ghostIcon = "";
	};
	class Slot_HandCart_DualWheel
	{
		name = "HandCart_DualWheel";
		displayName = "Dual truck wheel";
		ghostIcon = "";
	};
	class Slot_HandCart_TruckWheel1
	{
		name = "HandCart_TruckWheel1";
		displayName = "Truck wheel";
		ghostIcon = "";
	};
	class Slot_HandCart_TruckWheel2
	{
		name = "HandCart_TruckWheel2";
		displayName = "Truck wheel";
		ghostIcon = "";
	};
	class Slot_HandCart_CarWheel1
	{
		name = "HandCart_CarWheel1";
		displayName = "Car wheel";
		ghostIcon = "";
	};
	class Slot_HandCart_CarWheel2
	{
		name = "HandCart_CarWheel2";
		displayName = "Car wheel";
		ghostIcon = "";
	};
	class Slot_HandCart_CarWheel3
	{
		name = "HandCart_CarWheel3";
		displayName = "Car wheel";
		ghostIcon = "";
	};
	class Slot_HandCart_Barrel
	{
		name = "HandCart_Barrel";
		displayName = "Barrel";
		ghostIcon = "";
	};
	// Life Machines' stove and sewing machine (the HandCart_LifeMachines add-on)
	class Slot_HandCart_Stove
	{
		name = "HandCart_Stove";
		displayName = "Stove";
		ghostIcon = "";
	};
	class Slot_HandCart_SewingMachine
	{
		name = "HandCart_SewingMachine";
		displayName = "Sewing machine";
		ghostIcon = "";
	};
	class Slot_HandCart_Generator
	{
		name = "HandCart_Generator";
		displayName = "Power generator";
		ghostIcon = "";
	};
	class Slot_HandCart_CanopyTent1
	{
		name = "HandCart_CanopyTent1";
		displayName = "Canopy tent";
		ghostIcon = "";
	};
	class Slot_HandCart_CanopyTent2
	{
		name = "HandCart_CanopyTent2";
		displayName = "Canopy tent";
		ghostIcon = "";
	};
	class Slot_HandCart_CanopyTent3
	{
		name = "HandCart_CanopyTent3";
		displayName = "Canopy tent";
		ghostIcon = "";
	};
};

// Proxy model filename -> slots; proxy index N in handcart.p3d is the Nth slot listed.
class CfgNonAIVehicles
{
	class ProxyAttachment;
	// One proxy model per slot: a proxy class listing several slots draws nothing
	class Proxyhandcart_safe: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_Safe"};
		model = "\HandCart\proxies\handcart_safe.p3d";
	};
	class Proxyhandcart_seachest1: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_SeaChest1"};
		model = "\HandCart\proxies\handcart_seachest1.p3d";
	};
	class Proxyhandcart_seachest2: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_SeaChest2"};
		model = "\HandCart\proxies\handcart_seachest2.p3d";
	};
	class Proxyhandcart_crate1: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_Crate1"};
		model = "\HandCart\proxies\handcart_crate1.p3d";
	};
	class Proxyhandcart_crate2: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_Crate2"};
		model = "\HandCart\proxies\handcart_crate2.p3d";
	};
	class Proxyhandcart_crate3: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_Crate3"};
		model = "\HandCart\proxies\handcart_crate3.p3d";
	};
	class Proxyhandcart_crate4: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_Crate4"};
		model = "\HandCart\proxies\handcart_crate4.p3d";
	};
	class Proxyhandcart_dualwheel: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_DualWheel"};
		model = "\HandCart\proxies\handcart_dualwheel.p3d";
	};
	class Proxyhandcart_truckwheel1: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_TruckWheel1"};
		model = "\HandCart\proxies\handcart_truckwheel1.p3d";
	};
	class Proxyhandcart_truckwheel2: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_TruckWheel2"};
		model = "\HandCart\proxies\handcart_truckwheel2.p3d";
	};
	class Proxyhandcart_carwheel1: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_CarWheel1"};
		model = "\HandCart\proxies\handcart_carwheel1.p3d";
	};
	class Proxyhandcart_carwheel2: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_CarWheel2"};
		model = "\HandCart\proxies\handcart_carwheel2.p3d";
	};
	class Proxyhandcart_carwheel3: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_CarWheel3"};
		model = "\HandCart\proxies\handcart_carwheel3.p3d";
	};
	class Proxyhandcart_barrel: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_Barrel"};
		model = "\HandCart\proxies\handcart_barrel.p3d";
	};
	class Proxyhandcart_stove: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_Stove"};
		model = "\HandCart\proxies\handcart_stove.p3d";
	};
	class Proxyhandcart_sewingmachine: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_SewingMachine"};
		model = "\HandCart\proxies\handcart_sewingmachine.p3d";
	};
	class Proxyhandcart_generator: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_Generator"};
		model = "\HandCart\proxies\handcart_generator.p3d";
	};
	class Proxyhandcart_canopytent1: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_CanopyTent1"};
		model = "\HandCart\proxies\handcart_canopytent1.p3d";
	};
	class Proxyhandcart_canopytent2: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_CanopyTent2"};
		model = "\HandCart\proxies\handcart_canopytent2.p3d";
	};
	class Proxyhandcart_canopytent3: ProxyAttachment
	{
		scope = 2;
		inventorySlot[] = {"HandCart_CanopyTent3"};
		model = "\HandCart\proxies\handcart_canopytent3.p3d";
	};
};

// Sounds of a held truck (HandTruckVisual.c): gravel crunching while it rolls loaded,
// clanks while it rolls empty, a creak when it's swung round hard. Cut from Pixabay
// sounds (tools/make_sounds.sh, Pixabay Content License).
class CfgSoundShaders
{
	class baseCharacter_SoundShader;
	class HandCart_Gravel_SoundShader: baseCharacter_SoundShader
	{
		samples[] = {{"HandCart\data\sounds\handcart_gravel", 1}};
		volume = 0.7;
		range = 30;
	};
	class HandCart_Clank_SoundShader: baseCharacter_SoundShader
	{
		samples[] = {{"HandCart\data\sounds\handcart_clank1", 1}, {"HandCart\data\sounds\handcart_clank2", 1}, {"HandCart\data\sounds\handcart_clank3", 1}};
		volume = 0.5;
		range = 40;
	};
	class HandCart_Creak_SoundShader: baseCharacter_SoundShader
	{
		samples[] = {{"HandCart\data\sounds\handcart_creak", 1}};
		volume = 0.5;
		range = 25;
	};
};

class CfgSoundSets
{
	class baseCharacter_SoundSet;
	class HandCart_Gravel_SoundSet: baseCharacter_SoundSet
	{
		soundShaders[] = {"HandCart_Gravel_SoundShader"};
		loop = 1;
	};
	class HandCart_Clank_SoundSet: baseCharacter_SoundSet
	{
		soundShaders[] = {"HandCart_Clank_SoundShader"};
	};
	class HandCart_Creak_SoundSet: baseCharacter_SoundSet
	{
		soundShaders[] = {"HandCart_Creak_SoundShader"};
	};
};
