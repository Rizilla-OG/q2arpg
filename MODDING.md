# Modding Q2ARPG

Q2ARPG works with custom maps and campaigns out of the box. This guide covers
what happens automatically, the extra keys and entities mappers can use, and
how to set up TrenchBroom and a compiler.

## What works automatically

Put your `.bsp` files in the `q2rpg/maps/` folder and load them with
`map <name>` (or link them into a campaign with `target_changelevel` as usual).

- Every monster gets a level, can roll as an Alpha, and drops XP, cash and loot.
- Challenges, the vendor (`V`), skills and grenade mods all work.
- **New Game+** starts from the first map of your campaign and kicks in when
  the campaign ends on a victory screen: a `target_changelevel` whose map is
  `victory.pcx` (or anything ending in `victorXXX.pcx`, optionally after a
  cinematic like `*end.cin+victory.pcx`). If your campaign ends some other
  way, use `target_ngplus` (below).
- In New Game+ every map rolls a map modifier, unless you turn it off.

## Monster keys

Add these to any `monster_*` entity.

| Key | Values | Effect |
|---|---|---|
| `loot_elite` | `0` random (default), `1` always an Alpha, `2` always an Alpha Omega | Elites are bigger, tougher and drop better loot. The monster needs room to grow; in a tight spot it stays normal. |
| `loot_boss` | `0` no (default), `1` mini-boss, `2` unit boss | Boss levels, cash, XP, boss loot and Giant Slayer credit. Health doesn't change: use the stock `health_multiplier` key. |
| `loot_nodrop` | `0` normal (default), `1` none | No XP, cash, loot or challenge credit. Use it for cutscene monsters, or put it on a `target_spawner` for endless fodder, so players can't farm them. |
| `loot_boss_weapons` | weapon names separated by `;`, e.g. `Rail Rifle;Rocket Launcher` | The boss's signature gun is one of these. Works on any boss: a stock boss or a monster with `loot_boss`. |

## Loot entities

All loot rolls at the party's level when it appears.

### `item_loot_gun` (point)

A gun lying in the map, for example a Legendary in a secret.

| Key | Values |
|---|---|
| `loot_rarity` | `0` Common, `1` Uncommon, `2` Rare, `3` Epic, `4` Legendary, `5` Mythic (New Game+ only, else Legendary). Leave out for random. |
| `loot_weapon` | `Pistol`, `Shotgun`, `Scattergun`, `SMG`, `Minigun`, `Launcher`, `Rocket Launcher`, `Plasma Caster`, `Rail Rifle`, `Annihilator`, or a weapon classname like `weapon_railgun`. Leave out for random. |
| `loot_unique` | `1` the weapon's unique, `2` its second unique (New Game+ only, else the first). Forces Legendary. Set `loot_weapon` too, or the weapon is random. |

### `item_loot_shield` (point)

A shield. Key: `loot_rarity` (as above).

### `item_loot_grenade` (point)

A grenade mod. Keys: `loot_rarity` (as above) and `loot_grenade`: `Frag`,
`Cluster`, `Sticky`, `Vortex`, `Leech`, `Hopper` or `Airburst` (leave out for
random).

### `target_loot_drop` (point)

Drops a gun every time it's triggered, popping out like a monster drop. Give
it a `targetname` and point a button, trigger or secret at it. Takes the same
keys as `item_loot_gun`. To drop only once, trigger it from a `trigger_once`.

## Map and campaign keys

| Entity | Key | Effect |
|---|---|---|
| `worldspawn` | `loot_nomodifier` `1` | No New Game+ map modifier on this map. |
| `worldspawn` | `loot_novendor` `1` | The vendor won't open on this map (selling with `X` still works). Good for boss arenas. |
| `worldspawn` | `loot_modifier` | Force a map modifier by name: `Alpha Surge`, `Gold Rush`, `Loot Frenzy`, `Glass Cannon`, `Reinforcements`, `Elemental Storm`, `Bounty`, `Fortified` or `Bloodthirsty`. Works in any game, not just New Game+, and wins over `loot_nomodifier`. Good for a "loot vault" map. |
| `worldspawn` | `loot_level_bonus` | Monsters on this map are N levels higher (for hard maps and secret levels). |
| `worldspawn` | `loot_droprate` | Loot drop multiplier for this map, e.g. `2` for double, `0` for none (Alphas too). Doesn't affect bosses' fixed drops. |
| `worldspawn` | `loot_elite_chance` | Alpha chance multiplier for this map, e.g. `3`, or `0` for no random Alphas (`loot_elite` on a monster still works). |
| `worldspawn` | `loot_start_level` | Players below this level are raised to it when they enter: health, cash and skill points to match, and the starter pistol levels up. For standalone maps or campaigns that expect a set level. Never lowers anyone. |
| `target_ngplus` (point) | `targetname` | When triggered, ends the campaign like beating the final boss: intermission, then New Game+ from your first map. For campaigns that don't end on a victory screen. |

## TrenchBroom setup

The release zip has an `fgd` folder: a Quake 2 game config for TrenchBroom
2025 that knows all the keys above (dropdowns on monsters and worldspawn, and
the new entities in the entity list).

1. Copy the `fgd` folder's files to
   - Linux: `~/.TrenchBroom/games/Quake2-Q2ARPG/`
   - Windows: `%AppData%\TrenchBroom\games\Quake2-Q2ARPG\`
2. Restart TrenchBroom and pick **Quake 2 (Q2ARPG)**.
3. In **Preferences → Games → Quake 2 (Q2ARPG)**, set **Game Path** to the
   game's `rerelease` folder (the one with `baseq2` inside). Textures and
   models load from there.

## Compiling

Any Quake 2 compiler works. These steps use
[ericw-tools 2.0](https://github.com/ericwa/ericw-tools/releases) (`qbsp`,
`vis`, `light`). Keep your `.map` in a folder named `maps` so the tools find
the game's textures:

```
qbsp  -q2bsp -basedir "<game>/rerelease" -gamedir baseq2 maps/mymap.map
vis   maps/mymap.bsp
light -basedir "<game>/rerelease" -gamedir baseq2 maps/mymap.bsp
```

Copy `mymap.bsp` to `<game>/rerelease/q2rpg/maps/` and run `map mymap` from
the console with the mod loaded.

## Example map

The mod comes with a walk-through map: run `map q2rpg_example` from the console.
A hallway leads past 12 rooms, and each room shows one feature. A message
explains the room when you walk in.

| North rooms (left) | South rooms (right) |
|---|---|
| Placed guns: `item_loot_gun` with rarity, weapon and unique keys | Supplies, plus a note on the vendor (`V`) and skills (`K`) |
| Loot pad: a `trigger_multiple` firing `target_loot_drop` | Stock monsters with no keys |
| Alpha and Omega: `loot_elite` 1 and 2 | Grenade mods: `item_loot_grenade` with `loot_grenade` |
| Mini-boss: `loot_boss 1` with `loot_boss_weapons` | Shields: `item_loot_shield` at each rarity |
| Unit boss: a Tank with `loot_boss 2` | Uniques: `loot_unique 1` on five weapons |
| No-reward monster, and a pad firing a `target_spawner` with `loot_nodrop` | The `target_ngplus` pad (starts New Game+) |

Worldspawn sets `loot_start_level 10`. The source, `q2rpg_example.map`, is in
the zip's `fgd` folder. Open it in TrenchBroom to see every key. The repo's
`mapsrc/q2rpg_keys_test.map` is a smaller one-room test of the same keys.

## Questions and showing off

Ask in [Discussions](https://github.com/Rizilla-OG/q2arpg/discussions) or open
an [issue](https://github.com/Rizilla-OG/q2arpg/issues) if you need a key
that doesn't exist yet. Built something? Post it in Show and tell.
