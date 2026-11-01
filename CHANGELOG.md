# Changelog

## 0.3 (unreleased)

- The held truck rolls on its wheels: turning pivots it on them, and it swings back round
  behind the player (`TurnRateDegrees`, at most `MaxTurnDegrees` off). It tips and rolls
  with the ground (`FollowTerrain`).
- Sounds: gravel crunching under a loaded truck, clanks from an empty one, a creak when
  it's dragged round (`SoundVolume`).
- Tied-up players (tape, rope, handcuffs) can be loaded with F and set down with a click.
  They ride upright on the nose plate, the truck leaning back `PersonTiltDegrees`, and
  get off by themselves when freed, killed, knocked out or the truck leaves the pusher's
  hands (`AllowPeople`).
- Settings file version 9 (new fields get their defaults).

## 0.2

- Life Machines add-on (stove and sewing machine), loot entries shipped in
  `extras/types_handcart.xml`, first Workshop release.

## 0.1

- The hand truck: loads, unloading with a ghost, vehicles, barrels, damage looks.
