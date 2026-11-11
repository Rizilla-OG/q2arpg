# Changelog

## Unreleased (next update)

- Damage numbers: hits of 1000 or more show as `1.2k`, `2.0k` and so on, instead of stopping at 2047. Hits under 1000 still show the exact number.
- Map modifiers stick: reloading a map's autosave keeps the same modifier, so it can't be rerolled.
- Events log: `q2rpg_events.log` in the mod folder records map entries, modifiers, level ups, challenge tiers, Second Chance and deaths (including lava, falling and other world deaths) across sessions.
- New cvar `g_loot_logs`: set it to `0` to turn off both `loot_stats.csv` and the events log.

Bug fixes:

- Shock damage against monster shields now uses the gun that fired the shot, not the gun in your hands.
- Spike shields only hurt monsters that hit you in melee, not ones that shoot you.
- Weapons spawned by map triggers no longer show their rarity beam before they appear.
- When a gun runs dry, the auto-switch no longer skips a weapon type you still have a loaded loot gun for.

## 0.9.1

Bug fixes:

- Monsters brought back by a medic no longer give XP, cash, loot or challenge progress a second time.
- Co-op: a player who is dead when the map changes keeps their guns, skills, upgrades and cash.
- Vendor: number keys no longer buy while the challenges screen covers the shop. `V` brings the shop back, and opening the shop closes challenges.
- Elementalist: Chain Reaction and Spread now trigger on kills of burning, shocked or melting monsters (before, they almost never fired).

## 0.9.0

First public alpha.
