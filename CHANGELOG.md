# Changelog

## Unreleased (next update)

- Fixes from a code review of the new features: rift reward loot now drops at the guardian instead of the map origin; a rift map's exit can no longer clear your inventory on the way home; the tank guardian (it could never become an Omega) was swapped for a regular tank; explosive blasts are limited to one per enemy every 0.3 s, so explosive shield novas and grenades can't chain into huge blast stacks; the cryo refreeze wait survives the chill wearing off; coop players return from a rift on free spots instead of inside walls; Conductor counts chilled and frozen enemies; the target line shows Chilled / Frozen; the loot filter's hidden drops no longer count in the drop stats; and selling your held gun cancels its reload.
- 2 new challenges: **Cold Snap** (kills while chilled or frozen; badge: +3% damage to chilled enemies) and **Demolition** (kills with explosive guns or blasts; badge: +3% blast damage). Each tier 4 drops a Mythic gun of that element. The J screen now has 2 pages: press J again for page 2.
- New: a loot filter. Press L to cycle through off, hide Common, hide Uncommon and below, and hide Rare and below. Filtered monster drops just don't appear. Epic and up always drop, and boss loot, map-placed loot and rift rewards are never filtered. In coop each player has their own setting, and it's saved with your character.
- New endgame: **Rifts**, from level 60. Item 4 on the vendor's Workshop page opens one for $5k + $2.5k per tier. It sends you to a random Quake 64 map at your rift tier, where each tier adds monster health (+15%), damage (+8%), Alpha chance, loot luck and a monster level every 2 tiers. Kill 70% of the monsters and an Alpha Omega guardian appears. Killing it drops a loot burst (a guaranteed Legendary, with a Mythic chance, plus a class mod and more) and unlocks the next tier. You return to the exact spot you left after 30 s, or when you choose "Leave rift" or use the map's exit. Each rift map is used once per unit.
- Two new elements. **Cryo**: chills (the enemy slows to 60%, bosses to 80%). 3 procs on a chilled enemy freeze it solid for 1.5 s, and frozen enemies take 30% more damage. Bosses never freeze, and an enemy can't be refrozen for 3 s after it thaws. Killing a frozen enemy shatters it and chills everything nearby. **Explosive**: a proc sets off a small blast around the target that also hits enemies next to it. Kessler makers lean cryo and Hardline lean explosive. Elementalist skills (Fusion, Spread, Chain Reaction, Prism Heart) and the Prism unique use both.
- New: a stash on the vendor's 4th page (press 9 past the Workshop). It holds any item (guns, shields, grenade mods, class mods) and keeps it across maps and NG+. It has 4 slots at level 1 and 4 more every 10 levels (28 at level 60). At level 60 you can buy 4 more slots up to 4 times ($25k, $50k, $100k, $200k). On an empty slot, a number key stores the item you stand on, or else the gun you hold. On a full slot it takes the item out, swapping with your gun if your gun slots are full. Press B while standing on loot to send it straight to the first free stash slot. With nothing underfoot, B opens the stash for free, anywhere.
- New gear piece: class mods. One equipped; each adds bonus ranks to up to 3 skills of one tree (+1 on Common up to +3/+2/+2 on Mythic), and can push a skill past its max rank. Capstones are never boosted. They drop from monsters (mostly for your tree), and the vendor's Workshop page sells one. The skills screen shows the bonus in green.
- The vendor's stock no longer rerolls when you die and the map restarts; only a paid reroll changes it.
- Reinforcements that other monsters call in (Medic Commander, Carrier, Widow) give half XP and cash, and stop paying after 40 per map, so they can't be farmed.
- The level's monster count no longer goes up for death spawns that had no room to appear, so "all monsters killed" can be reached again.

## 0.9.4

- Data file for modders: `loot_data.json` in the mod folder can change gun stats, manufacturers, rarities and vendor prices without touching C++. `loot_data.example.json` lists every built-in value; see MODDING.md.
- Example map for mappers: `map q2rpg_example` walks through every mapper feature, one room each; its source ships in the `fgd` folder.
- Mastery: once your skill tree is full, every spare skill point adds +1% to the tree's main stat (Gunner gun damage, Juggernaut less damage taken, Elementalist status damage), with no cap. Shown at the top of the skills screen. Before, points past level 53 went to waste.
- Map modifiers no longer change after dying and reloading the map-start autosave. (It happened when a map's first roll was skipped for repeating the previous map's modifier.)
- Tougher, more frequent elites: Alphas have x3 health (was x2.5) and Alpha Omegas x5 (was x4); from level 25, 25% of monsters spawn as Alphas (was 20%); in New Game+, 15% of Alphas are Omegas (was 10%).
- Balance stats: `loot_stats` and `loot_stats.csv` now show the average time to kill (first hit to death) for normal monsters, Alphas, Omegas and bosses. An older CSV is kept as `loot_stats_before_ttk1.csv` and a fresh one starts.
- Harmless NPCs (the crazed marines) no longer get monster levels, shields or Alpha upgrades, and show no health bar.
- Gun, shield and grenade cards: stat values and changes now line up in neat columns.
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
