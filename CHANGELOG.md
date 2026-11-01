# Changelog

## 0.3 (unreleased)

- The truck steers like one (`SteerLikeACart`): forward and back only, turning no faster
  than `TurnRateDegrees` (mouse, or A and D), pivoting on its wheels as the player steps
  round the axle. It tips and rolls with the ground (`FollowTerrain`).
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
