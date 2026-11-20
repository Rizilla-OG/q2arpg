# Changelog

## Unreleased (next update)

- Added: alien-tech guns. Military intel has picked up rumors that the Strogg have been modifying our guns, and now two new Rare+ gun types are turning up in the field. The **Ripper** fires bouncing ion discs from cells; Brask barrels and some Epic+ rolls fan out an extra disc. The **Tracker** fires homing shots from slugs and locks onto the monster nearest your crosshair when you miss. They take elements, crits and proficiency like any gun, but have no uniques.

## 0.9.666

- Fixed: reloading an SMG, Minigun or Plasma Caster while holding the trigger no longer keeps it firing through the reload.
- Fixes from a code review of the new features: a side mission waits during a trip to a rift, raid or arena (kills there don't count); missions that can't be finished any more (too few of those monsters left, chest opened, no Alphas) say "too few left" and can't be taken, and an active one that gets stuck says so; a Spree that ran out of time shows 0 again; arena monsters that get stuck are moved back into the fight after 45 s so the wave can end; raid adds are always Alphas; the raid boss no longer counts as an Omega kill; a finished side mission stays done when you come back to its map; the raid boss's element shield and last stand cover melee, grenades and status damage too, and the boss no longer starts at Alpha speed; arena waves can't clear without monsters or get stuck, and in coop every player gets their own wave rewards; the ending-video reload only fires on the New Game+ start map right after the video; a very full loadout screen leaves rows out instead of breaking; the last stash page's 9 key now says it goes to Missions.
- New: the **arena**, a horde mode from level 15. Item 6 on the vendor's Workshop page sends you to an arena for up to 15 waves that grow bigger and tougher, with an Alpha Omega every 5th wave. Every cleared wave drops cash and every 3rd wave drops gear (Rare and up, better from wave 9 and 15). Leave any time with item 6 and keep your loot; dying sends you back to wave 1. Monsters there drop half as much loot.
- New: melee. Press Z (`loot_melee`) to strike whatever is right in front of you, every 0.6 s. Damage grows with your level; head hits crit. Your held gun's stock part changes it (Brask stocks hit hardest, Vexa the lightest) and Juggernauts hit 50% harder. Restart the game once so the new Z bind loads.
- New endgame: the **raid**. From level 60, item 5 on the vendor's Workshop page ($25k) takes you to an arena to fight **the Hollow King**, a huge Makron with three phases: at 75% health it calls in Alphas and raises an element shield (only that element hits fully), at 50% it enrages and sends ground shockwaves (jump over them), and at 25% the shield drops for a last stand with Alpha waves and +20% damage for you. It drops 2 Legendaries (30% chance one is Mythic), an Epic+ relic and class mod, a pile of cash, and a 25% chance at one of three raid-only uniques: **Kingslayer** (Rail Rifle, +50% damage to bosses and Omegas), **Bloodmoon** (SMG, kills heal 5% max health) and **Stormheart** (Plasma Caster, hits can chain-shock nearby monsters). One clear per New Game+ cycle; dying restarts the fight. You go back to where you left 45 s after the kill.
- New: side missions. The vendor has a Missions page (press 9 past the stash) with 2 missions per map: kill a number of one monster kind, kill Alphas, headshot kills, kills with an element, open a loot chest, or 10 kills inside 60 seconds. Take one for free (one at a time); progress shows on your level line. Finishing pays XP, cash and a gear piece (gun, relic or class mod) of at least Uncommon, Rare or Epic rarity depending on difficulty. Leaving the map drops an unfinished mission.
- New gear piece: relics. One equipped, each gives one passive effect for any skill tree, stronger with rarity (Common to Mythic): **Element** (one element's procs get +6% to +36% chance and damage), **Fortune** (+5% to +30% cash and better loot), **Quartermaster** (every 2 s, 2% to 12% of your held gun's magazine is added to its reserve ammo), **Leech** (heal 0.5% to 3% of the gun damage you deal) and **Demolition** (+6% to +36% grenade damage). They drop from monsters and chests, show on the loadout screen, and can be stashed and sold.
- New: weapon proficiency. Kills with a gun type level it up, to a max of 15 (level 1 takes 20 kills, each level needs 25% more). Each level gives +1% accuracy, reload speed and crit damage with that gun type. It's saved with your character and kept through New Game+. See it on the gun details page (press I twice), or type `loot_prof` in the console for every gun type.
- New: loot chests. Every map has 1 or 2 white chests and sometimes a red one, tucked beside map items away from the start (they look like big glowing ammo boxes). Walk into one to open it: white chests hold 2 guns, cash, ammo for your guns and sometimes a shield or grenade mod; red chests add a guaranteed Rare-or-better gun, always some gear (maybe a class mod) and 3x cash. In coop every player opens each chest once for their own loot. Mappers can place `item_loot_chest` (`loot_rarity 2`+ = red) or turn random chests off with worldspawn `loot_nochests 1`.
- Semi-auto pistols: each maker has its own click delay (Vexa 160 ms, Orbitek 190, Salvage 200, Brask 220, Hardline 240, Kessler 260), and the gun's fire rate (its trigger part) shortens or stretches it, down to 120 ms. Pistol cards show it as Click ms. Modders can set it per maker with `pistol_click_ms` in `loot_data.json`.
- Fixed: with two guns of the same kind equipped, running one dry could bounce between them instead of switching to the loaded one (or to another gun when both are empty).
- Pistols are now semi-auto: each click fires as soon as a short cooldown (200 ms, shorter on faster guns) is up, instead of waiting for the full fire animation. Holding the trigger fires at the old rate.
- Beating the Makron now plays the ending video again before New Game+ starts (single player). Before, NG+ skipped it.

## 0.9.5

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
