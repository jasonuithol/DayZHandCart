# HandCart

A hand truck (sack truck) for DayZ. Wheel heavy things around instead of carrying
them one at a time: safes, sea chests, crates, barrels, wheels, a generator, tents.

![The hand truck in its three colours](models/handcart_variants.png)

## In game

- **Holding it.** Take the hand truck into your hands and you push it in front of
  you, tilted back on its wheels. With an empty truck you can jog. You can't
  sprint, jump, climb or vault with one, loaded or not. Loaded, you walk, and
  slower off-road.
- **Loading.** Look at something it can carry and press **F** (Load), or drag the
  item onto the truck in your hands in the inventory. The truck carries one kind
  of load at a time, stacked:

  | Load | How many |
  |---|---|
  | Expansion safe (Large, Medium or Mini, locked or not; door shut) | 1 |
  | Sea chest | 2 |
  | Wooden crate | 4 |
  | MS3 dual wheel | 1 |
  | MS3 or M1025 wheel | 2 |
  | Car wheel | 3 |
  | Power generator | 1 |
  | Canopy tent, packed | 3 |
  | Barrel or fire barrel (lid goes on; a fire barrel must be out) | 1 |
  | Life Machines stove (not while cooking) | 1 |
  | Life Machines sewing machine (not while sewing) | 1 |
  | A person tied up with tape, rope or handcuffs | 1 |

  Containers keep their contents; a full barrel or sea chest stays full.
- **Steering.** It handles like a real hand truck: push it forward or pull it back (no
  sidestepping), and turn slowly, with the mouse or A and D. Turning pivots it on its
  wheels: you step sideways round the axle, walking the handles round. It tips and rolls
  so its wheels stay on the ground. A loaded truck crunches over gravel; an empty one
  clanks.
- **People.** Holding an empty truck, look at a tied-up player and press **F**. They ride
  standing on the nose plate; the truck leans back less (a body can't be tilted). Click
  to set them down where they stand, if there's room and you can see the spot. They get
  off by themselves if they're freed, killed or knocked out, or if the truck leaves your
  hands.
- **Unloading.** Click to bring up a ghost of the top item, aim it (mouse wheel
  or Z/X to turn it), and hold to set it down. You stand still while choosing a
  spot. The spot must be within 2 m, not too steep, and in sight. Or drag the
  load off the truck in the inventory.
- **Vehicles.** Looking at a vehicle, **F** offers *Load …* (from its slots,
  like an MS3's barrel and crate slots, then its cargo) and *Put … in vehicle*.
  Dragging between the vehicle and the truck works too. Vanilla's rules decide
  where things fit: a full barrel only goes in a barrel slot.
- The truck itself fits in any vehicle's cargo, crossways.
- Three colours (blue, red, yellow), with damaged and ruined looks.

## Installing

Up to three mods, each needed on the server and the clients:

- **@HandCart** ([Workshop](https://steamcommunity.com/sharedfiles/filedetails/?id=3830655667)):
  the hand truck. Needs nothing but DayZ (1.30).
- **@HandCart_Expansion** ([Workshop](https://steamcommunity.com/sharedfiles/filedetails/?id=3830656388)):
  lets it carry Expansion's safes. Needs DayZ-Expansion-BaseBuilding. Leave it out if
  you don't run Expansion.
- **@HandCart_LifeMachines** ([Workshop](https://steamcommunity.com/sharedfiles/filedetails/?id=3830657046)):
  lets it carry the stove and sewing machine from
  [Life Machines](https://steamcommunity.com/sharedfiles/filedetails/?id=3741693087),
  which otherwise can't be moved once deployed. They can also go into a vehicle's
  cargo (no other). Needs Life Machines. Leave it out if you don't run it.

On the server (paths relative to its install folder, `DayZServer/`):

1. Put each mod's folder in the install folder: `DayZServer/@HandCart`, and the add-ons
   as `DayZServer/@HandCart_Expansion`, `DayZServer/@HandCart_LifeMachines`.
2. Add them to the launch parameter, after your other mods:
   `-mod=...;@HandCart;@HandCart_Expansion;@HandCart_LifeMachines`.
3. Copy the key file `DayZServer/@HandCart/keys/tarbaby.bikey` into `DayZServer/keys/`
   (next to `dayz.bikey`); without it, players using the mod are kicked for unsigned
   files. The add-ons are signed with the same key, so that one file covers all three.
4. Copy the loot entries `DayZServer/@HandCart/extras/types_handcart.xml` (also
   [`server_files/types_handcart.xml`](server_files/types_handcart.xml) here) into a
   folder of your mission, e.g.
   `DayZServer/mpmissions/dayzOffline.chernarusplus/handcart/types_handcart.xml`, and
   register it in that mission's `cfgeconomycore.xml`, inside `<economycore>`:

   ```xml
   <ce folder="handcart">
       <file name="types_handcart.xml" type="types" />
   </ce>
   ```

   In a folder that already has a `<ce>` block, add just the `<file>` line. Or paste the
   three `<type>` entries into the mission's `db/types.xml` instead.
5. Restart the server.

Players only need to subscribe on the Workshop. Hand trucks spawn where power generators
do (tools, Industrial), but only at floor level: three colours, about 9 in all.

## Settings

The server writes `$profile:HandCart/HandCartSettings.json` with defaults on
first start and sends it to every client.

| Setting | Default | |
|---|---|---|
| `MaxUnloadDistance` | 2 | metres from the player to where an item is set down |
| `MaxSlopeDegrees` | 30 | steepest ground an item may be set down on |
| `RequireLineOfSight` | true | no unloading through walls |
| `UnloadTime` | 1.5 | seconds to set an item down |
| `SoftGroundSpeed` | 0.6 | loaded speed off-road (1 = walking) |
| `HoldTiltDegrees`, `HoldGripForward`, `HoldLift` | 50, 0.35, 0.7 | how the held truck sits in front of the player |
| `SteerLikeACart` | true | holding a truck: forward and back only, slow turns that pivot on its wheels |
| `TurnRateDegrees` | 60 | fastest a player holding a truck turns (degrees a second) |
| `FollowTerrain` | true | the held truck tips and rolls with the ground |
| `SoundVolume` | 1 | rolling and clanking (0 = off) |
| `AllowPeople` | true | tied-up players can be loaded |
| `PersonTiltDegrees` | 15 | how far a truck carrying a person leans back |
| `HardSurfaces`, `SoftSurfaces` | empty | surface names to always treat as road or as dirt |
| `BlockedItems` | empty | class names (and their subclasses) that may never be loaded |
| `LoadTime` | | unused: loading is instant |

Another mod's item becomes loadable by adding one of the hand truck's slot
names (`HandCart_Crate1`, `HandCart_Barrel` and so on, in `HandCart/config.cpp`)
to its `inventorySlot[]`.

## Building

The models are generated in Blender with the
[Arma 3 Object Builder](https://github.com/MrClock8163/Arma3ObjectBuilder) add-on:

```sh
blender --background --python blender/build_handcart.py   # the truck, model.cfg, previews
blender --background --python blender/build_proxies.py    # the load slots' proxy models
blender --background --python blender/bake_textures.py    # weathered textures
tools/img2paa.sh textures/*.png HandCart/data              # textures to .paa
tools/make_sounds.sh                                      # sounds (sources: see the script)
tools/build.sh                                            # binarize into build/HandCart
```

`tools/build.sh` runs DayZ Tools' Binarize under Proton on Linux and stages the
mod in `build/HandCart`. Packing, signing and publishing are done separately, with the
author's DayZMissions tooling (`tools/build_mod.py`, `tools/workshop_publish.sh`, which
read each mod's `workshop.*` files and `preview.png`), which also runs the test mission.

## Releasing

The Workshop keeps only the latest upload of each mod, with no version number of its own.
The version comes from a git tag: `workshop_publish.sh` adds a `Version:` line to the end of
the description and starts the change note with it (`git describe --tags`, so an untagged
upload shows up as e.g. `v0.2-3-gabc1234`).

1. Date the version's entry in `CHANGELOG.md` (it was "unreleased"), commit everything,
   then tag the release and push the tag:

   ```sh
   git tag -a v0.3 -m "HandCart 0.3: what changed"
   git push origin main v0.3
   ```

2. Build the hand truck (the add-ons have no models and need no build step):
   `tools/build.sh`.
3. From DayZMissions, upload all three mods so their versions match. SteamCMD asks for the
   Steam password and Steam Guard code, so run these yourself, not from a script:

   ```sh
   tools/workshop_publish.sh HandCart <steam login>
   tools/workshop_publish.sh HandCart_Expansion <steam login>
   tools/workshop_publish.sh HandCart_LifeMachines <steam login>
   ```

   SteamCMD logging in knocks the desktop Steam client offline: don't publish while
   playing, and click Steam > Go Online afterwards.
4. Tell server admins to update the mods and restart. Until they do, players have the new
   upload and the server the old one, and the launcher reports a missing or mismatched
   mod. The key stays `tarbaby.bikey`, so `keys/` needs nothing; if
   `server_files/types_handcart.xml` changed, they need to copy it again.

Players get the update automatically. To roll back, check out the old tag, build and
upload again (`workshop_publish.sh` takes the version as an optional third argument if it
should say something other than the tag).

## Testing

[`mission/handcart.chernarusplus`](mission/handcart.chernarusplus) is a test
mission. It lays out one of every load by a road near Balota and parks a loaded
MS3 on the Chernogorsk docks, where players spawn with a hand truck beside two
handcuffed survivors to load.

Started with `-missiontest`, it runs headless checks (loading and unloading,
limits, movement rules, vehicles, dragging) and shuts the server down, logging
`[HandCartTest]` lines. `tools/restart_test.sh` rebuilds the mod and restarts a
local server and client on it.
