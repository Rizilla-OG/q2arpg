# Changelog

## Unreleased (next update)

- Mapper fixes:
  - `loot_start_level` above the level cap no longer heals players every frame (it made them unkillable).
  - Monsters and placed loot now roll at the map's `loot_start_level`, not the player's old level.
  - `item_loot_grenade` keeps its `loot_grenade` type at any rarity (Common used to turn it into Standard or Sticky).
  - `loot_nodrop` on a `target_spawner` now covers the monsters it spawns, and a no-reward monster's Summoner and death-spawn adds pay nothing either.
  - `loot_droprate 0` and `loot_elite_chance 0` now turn drops or Alphas off (they used to mean "normal").
  - A map with `loot_nomodifier` no longer bars the next map from a modifier the player never saw.
  - `loot_boss_weapons` is read once when the boss spawns; a typo prints one "skipped" line instead of a misleading line on every kill.

## 0.9.3

- Health bars: Omegas are labeled "Omega" in red over their head (they showed "Alpha").
- Mapper keys and entities: `loot_elite`, `loot_boss`, `loot_boss_weapons` and `loot_nodrop` on monsters; `item_loot_gun` (with `loot_unique`), `item_loot_shield`, `item_loot_grenade`, `target_loot_drop` and `target_ngplus`; on worldspawn `loot_nomodifier`, `loot_novendor`, `loot_modifier`, `loot_level_bonus`, `loot_droprate`, `loot_elite_chance` and `loot_start_level`. The zip ships a TrenchBroom game config (`fgd` folder) and the new `MODDING.md` guide.
- README: the logo is now an image, so it shows correctly on GitHub.

## 0.9.2 (first public alpha)

- Release zip ships `README.md`, `CHANGELOG.md` and `LICENSE.txt` in the `q2rpg` folder; the playtest command list no longer prints at startup.
- Damage numbers: hits of 1000 or more show as `1.2k`, `2.0k` and so on, instead of stopping at 2047. Hits under 1000 still show the exact number.
- Map modifiers stick: reloading a map's autosave keeps the same modifier, so it can't be rerolled.
- Events log: `q2rpg_events.log` in the mod folder records map entries, modifiers, level ups, challenge tiers, Second Chance and deaths (including lava, falling and other world deaths) across sessions.
- New cvar `g_loot_logs`: set it to `0` to turn off both `loot_stats.csv` and the events log.

Bug fixes:

- Fire Rate is now exact. Before, real fire speed snapped to a few steps (a 130% gun fired like a 100% one, and anything from 140% to 199% fired at 133%). Every percent of Fire Rate now counts.
- Pistol and shotgun challenges: a kill counts for the gun that fired the shot. Before, a rocket or grenade-launcher kill landing after you swapped to a pistol or shotgun counted for the gun in your hands.
- Bloodthirsty: monsters heal from the health you actually lose, not from damage your shield or armor blocked.
- Shock damage against monster shields now uses the gun that fired the shot, not the gun in your hands.
- Spike shields only hurt monsters that hit you in melee, not ones that shoot you.
- Weapons spawned by map triggers no longer show their rarity beam before they appear.
- When a gun runs dry, the auto-switch no longer skips a weapon type you still have a loaded loot gun for.

## 0.9.1 (private test build)

Bug fixes:

- Monsters brought back by a medic no longer give XP, cash, loot or challenge progress a second time.
- Co-op: a player who is dead when the map changes keeps their guns, skills, upgrades and cash.
- Vendor: number keys no longer buy while the challenges screen covers the shop. `V` brings the shop back, and opening the shop closes challenges.
- Elementalist: Chain Reaction and Spread now trigger on kills of burning, shocked or melting monsters (before, they almost never fired).

## 0.9.0 (private test build)

First complete build: loot, skills, vendor, challenges, New Game+.
