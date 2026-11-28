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

### `item_loot_chest` (point)

A loot chest; players open it by walking into it. `loot_rarity` `2` or higher
makes it a red chest (a guaranteed Rare-or-better gun, always some gear, 3x
cash); anything lower is a white chest. The loot rolls at the party's level
when it's opened. In coop each player opens it once for their own loot.

### `target_loot_drop` (point)

Drops a gun every time it's triggered, popping out like a monster drop. Give
it a `targetname` and point a button, trigger or secret at it. Takes the same
keys as `item_loot_gun`. To drop only once, trigger it from a `trigger_once`.

### `monster_loot_random` (point)

Becomes a random monster when the map loads, so each playthrough meets something
different. Every monster key above works on it (`loot_elite`, `loot_boss`,
`loot_nodrop`, `angle`, spawn flags, `targetname`, `deathtarget`), and it drops
the normal random XP, cash and loot.

| Key | Values |
|---|---|
| `loot_monsters` | Monsters to pick from, separated by `;`, e.g. `monster_gunner;monster_berserk;monster_tank`. Leave out for the default pool: light soldier, soldier, SS soldier, infantry, gunner, berserker, parasite, iron maiden, brain and gladiator (ground monsters that fit most rooms). Names that aren't monsters, and cutscene props that can't die (`monster_commander_body`, `monster_boss3_stand`, `monster_tank_stand`), are skipped with a console warning. |
| `loot_drop` | `1` also drops a guaranteed gun when it dies, on top of the normal loot. |
| `loot_rarity`, `loot_weapon`, `loot_unique` | Shape the guaranteed gun, same values as `item_loot_gun`. Setting any of them turns the guaranteed gun on. `loot_nodrop` turns it off. |

Give it room for the biggest monster in its list (the editor box fits the default pool's biggest, the gladiator). Flying monsters work if you
list them, but the default pool is ground-only.

## Map and campaign keys

| Entity | Key | Effect |
|---|---|---|
| `worldspawn` | `loot_nomodifier` `1` | No New Game+ map modifier on this map. |
| `worldspawn` | `loot_novendor` `1` | The vendor won't open on this map (selling with `X` still works). Good for boss arenas. |
| `worldspawn` | `loot_nochests` `1` | No random loot chests on this map (placed `item_loot_chest` entities still appear). |
| `worldspawn` | `loot_modifier` | Force a map modifier by name: `Alpha Surge`, `Gold Rush`, `Loot Frenzy`, `Glass Cannon`, `Reinforcements`, `Elemental Storm`, `Bounty`, `Fortified` or `Bloodthirsty`. Works in any game, not just New Game+, and wins over `loot_nomodifier`. Good for a "loot vault" map. |
| `worldspawn` | `loot_level_bonus` | Monsters on this map are N levels higher (for hard maps and secret levels). |
| `worldspawn` | `loot_droprate` | Loot drop multiplier for this map, e.g. `2` for double, `0` for none (Alphas too). Doesn't affect bosses' fixed drops. |
| `worldspawn` | `loot_elite_chance` | Alpha chance multiplier for this map, e.g. `3`, or `0` for no random Alphas (`loot_elite` on a monster still works). |
| `worldspawn` | `loot_start_level` | Players below this level are raised to it when they enter: health, cash and skill points to match, and the starter pistol levels up. For standalone maps or campaigns that expect a set level. Never lowers anyone. |
| `target_ngplus` (point) | `targetname` | When triggered, ends the campaign like beating the final boss: intermission, then New Game+ from your first map. For campaigns that don't end on a victory screen. |

## More for mappers (from the PSX update)

Paril's PSX update added these to the stock entities. All are opt-in, so
existing maps behave as before. All are in the TrenchBroom config.

| Entity | Key / flag | Effect |
|---|---|---|
| `trigger_safe_fall` (brush) | | A player already in the air who touches it takes no damage on their next landing. |
| `trigger_push` | flag 32 Additive | Speeds things up toward `speed` instead of setting it, with no gravity while inside. Good for wind tunnels and updrafts. |
| `trigger_hurt` | flag 65536 Passive | Hurts everything inside every 0.1 s (1 s with Slow), even things standing still. Works with Start Off / Toggle and Clipped. |
| `trigger_key` | flag 1 Become Relay | After the key is used it fires its targets every time, like a `trigger_relay`. |
| `trigger_once` / `trigger_multiple` | `noise` | Sound to play when it fires, instead of `sounds`. |
| `target_laser` | flag 131072 Reactor, flag 262144 No Protection, `frame`, `dmg` `0` | Reactor is drawn as lightning. No Protection ignores armor. `frame` sets the beam width. `dmg` `0` is a harmless beam that passes through monsters and players. |
| `func_door_rotating` | flag 262144 No Collision | Never pushes or gets blocked by anything in its way (for decorative doors and fans). Players still can't walk through it. |
| `func_door_secret` | `speed` | Movement speed (default 500). |
| `func_wall` | flag 32 Safe Appear | When it appears, pushes things out of the way instead of telefragging them. |
| `func_plat2` | `wait` | Seconds it waits at the top or bottom (default 2). |
| `func_eye` | `vision_cone` | How wide it looks (now read; it was ignored before). |
| `misc_explobox` | flag 1 No Move | Can't be pushed or knocked around. |
| `misc_model` | flag 1 Toggle, flag 2 Start On | Triggering shows or hides the model. |
| Items | flag 8 No Drop, `effects`, `renderfx` | No Drop floats the item where it's placed instead of dropping it to the floor. `effects` and `renderfx` override the item's glow and spin. |
| Monsters | flag 1048576 No Idle Doors | Only opens doors once it has an enemy, so patrolling monsters don't open doors on their own. |
| Monsters | `vision_cone` | How wide the monster sees, as a dot product: `-1` all around, `0` 180 degrees, `0.5` 120 degrees. Leave it out for the stock cone. |
| `info_player_start` | flag 1 Ride | The start drops onto whatever is under it at load, so players spawn riding a lift. |

## Data file: guns, makers, rarities and prices

Gun stats, manufacturers, rarities and vendor prices can be changed without
touching C++. The mod folder has `loot_data.example.json` with every built-in
value. Copy it to `loot_data.json` in the same folder, change what you want and
delete the rest: anything left out keeps its built-in value. The file is read
when a map starts, and the console prints how many values changed and any
problems (typos, values out of range).

Entries are found by their built-in name (`"Shotgun"`, `"Hardline Arms"`,
`"Rare"`), so they still match after you rename them. Numbers marked "x" are
multipliers, where `1` is normal.

| Section | Fields |
|---|---|
| `guns` | `name`, `magazine` (base size), `reload` (seconds), `drop_weight` (higher drops more often, `0` never), `min_rarity` (`Common` .. `Mythic`) |
| `makers` | `name`, `short_name` (shown on the HUD), and x per part: `recoil` and `handling` (stock), `damage` and `barrel_spread` (barrel), `pellets` (extra shotgun pellets, a whole number), `fire_rate` (trigger), `magazine` and `reload` (magazine; reload is time, so lower is faster), `scope_spread` (scope), `crit` (added to the crit multiplier, base 1.5), `pistol_click_ms` (semi-auto pistol delay between clicks, in ms at fire rate 100%; the gun's maker sets it, fire rate scales it, floor 120) |
| `rarities` | `name`, `damage` (x), `drop_weight` (Mythic only drops in New Game+) |
| `prices` | `rarity_value` (6 numbers, Common to Mythic), `base`, `per_level`, `per_level_squared` (price = (base + per_level x level + per_level_squared x level²) x rarity value), `element_mult` (elemental guns and grenades), `sell_fraction` (share of the price paid when selling) |

Example: shotguns twice as common, Legendary hits harder, everything cheaper:

```json
{
  "guns": { "Shotgun": { "drop_weight": 32 } },
  "rarities": { "Legendary": { "damage": 1.8 } },
  "prices": { "base": 15, "per_level": 6 }
}
```

Changes apply to loot rolled after the map starts. Guns already in a save keep
their stats.

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

### Key hints in messages

To name a key in a `message`, write `^k` and the default key, e.g.
`Press ^kV to shop`. Players see the key they bound to that command, so
`^kV` shows `N` for someone who moved the vendor to N. Works for `E G I X B L
V K J F Q Z M H R` and `^k1`–`^k9` (weapon slots).

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

## Tutorial map

`q2rpg_tutorial` (type `tutorial` in the console) is the player tutorial. Its
source is generated: edit `mapsrc/make_tutorial.py` in the repo (room order,
messages, items and monsters), run it to write `q2rpg_tutorial.map`, then
compile as above.

## Questions and showing off

Ask in [Discussions](https://github.com/Rizilla-OG/q2arpg/discussions) or open
an [issue](https://github.com/Rizilla-OG/q2arpg/issues) if you need a key
that doesn't exist yet. Built something? Post it in Show and tell.
