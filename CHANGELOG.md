# Changelog

## Unreleased (next update)

### Added

- Mapping: more from Paril's PSX update, all opt-in so existing maps are unchanged. Additive `trigger_push` (wind tunnels and updrafts), passive `trigger_hurt` (hurts things standing still), `trigger_key` that becomes a relay, a `noise` sound for `trigger_once`/`trigger_multiple`, `target_laser` Reactor and No Protection flags plus harmless `dmg 0` beams and a `frame` width, `func_door_rotating` No Collision, `func_door_secret` `speed`, `func_wall` Safe Appear, `func_plat2` `wait`, a `vision_cone` that `func_eye` now reads, `misc_explobox` No Move, toggleable `misc_model`, floating items (No Drop) with `effects`/`renderfx` overrides, a No Idle Doors monster flag, a monster `vision_cone` key and an `info_player_start` Ride flag. See MODDING.md; all are in the TrenchBroom config.
- The Arachnid and Guardian join the late game. From player level 30, and on every New Game+ map, a placed Gladiator, Tank or Gunner Commander can come in as an Arachnid (10%, 20% in NG+) and a Tank Commander or Supertank as a Guardian (10%, 25% in NG+), only where the bigger monster fits with floor under it. The Arachnid joins the arena's late waves (wave 8+), and both can be a rift's guardian.

### Changed

- Monster AI from the PSX update. Medics and the Xatrix fixbot share one revive routine, so fixbots actually bring monsters back now (and back off to do it), and revived monsters go after whoever the healer was fighting. Soldiers: shotgun guards take a beat before firing on the run, machinegun and laser guards duck more often on Hard and above, and blind fire aims around corners like the other monsters. Monsters can't see or splash you through projectile-clip brushes, can't see you through windows for the "had sight" check, and pause 50-200 ms before firing when they regain sight of you. Icarus keeps more distance, the Hornet flies with the newer flight code, the Berserker jumps lower and its quick swing knocks you back harder, flying monsters hover higher around you and follow paths more smoothly, and walking monsters are better at reaching path points.

### Fixed

- Monsters a fixbot revived paid out a second time.
- Monsters no longer run in place against a wall their pathing doesn't know about (a `func_wall` that appeared, a pillar they can see you past, maps without navigation data). After a second without getting anywhere they sidestep for a moment, then chase again.
- Medics no longer walk back and forth trying to revive a body behind a `func_wall` or door. A cable blocked by a brush entity now counts like one blocked by the world, so after a couple of tries the medic gives up on that body.
- A monster with a `healthtarget` lost its own `target` after firing it.
- The Gunner Commander could stay standing at exactly 0 health after certain pain animations.
- Medics reviving a `turret_driver` that fell out of its turret now bring it back as a proper infantry.
- `target_spawner` no longer telefrags anything when it spawns a non-solid entity (an item trigger, a light).
- Power armor given by a map's `start_items` wasn't switched on, and the "auto" power-shield setting didn't take effect.

## 0.9.9

### Added

- Arachnid and Guardian behavior from Paril's PSX update. The Arachnid walks with a heavier gait, aims its railguns ahead of you, chains its melee, and after missing you a few times gets angry and fires a rapid rail volley; on Nightmare it calls in Stalkers (these count toward the reinforcement pay cap). The Guardian leads its shots, kicks from farther away, launches homing rockets (which can be shot down) and blind-fires at where it last saw you; it gets a power shield when you carry a Quad, Double Damage or Invulnerability, and has more health on Hard and above and in co-op. Mappers can set the Guardian's new Sleepy flag (it sleeps until triggered). The PSX update's new sounds and damaged skins need its asset pack, so they're left out.
- Mapping: `trigger_safe_fall`, from the PSX update. A player already in the air who touches it takes no damage on their next landing. In the TrenchBroom config.

### Changed

- Your key binds stick. The mod used to re-bind its keys in `autoexec.cfg` every time the game started, undoing any change you made. It now sets its default keys once, the first time you play, and only for commands that have no key yet, so the keys you have now stay as they are. Type `q2rpg_resetbinds` in the console to put the defaults back. Your own `autoexec.cfg` in Saved Games no longer breaks hold-to-zoom or the `tutorial` command.
- Key hints follow your binds. Rebind a key and every screen shows the new one: footer hints, number keys on vendor and skill rows, the `K +N` skill-point chip and `F` action badge, on-screen messages ("Press X again") and the tutorial and example map notes. A command with no key shows `--`.

### Fixed

- `monster_loot_random` with a `delay` dropped its guaranteed gun at the map origin instead of where the monster died.
- `monster_loot_random` with `loot_nodrop` still dropped its guaranteed gun.
- `monster_loot_random` no longer picks cutscene props that can't die (`monster_commander_body`, `monster_boss3_stand`, `monster_tank_stand`) from a `loot_monsters` list, which could leave its deathtarget unfired. Tabs and line breaks in the list are now ignored.
- `monster_loot_random`'s box in the TrenchBroom config is now the size of the biggest default monster (the gladiator), so it can't be placed where a gladiator gets stuck in a wall.
- The dark box behind on-screen messages now fits the text with the classic font (`scr_usekfont 0`) and the alternate typeface, and covers key-binding prompts under tutorial notes.
- With the classic font, lower stats on loot cards now sit on a red box (the classic font can't be drawn in red), so they no longer look the same as unchanged stats.

## 0.9.8

### Added

- Tutorial map. Type `tutorial` in the console (or `map q2rpg_tutorial`). Eleven rooms, one lesson each: the HUD, loot and gun cards, shields and grenade mods, fighting, Alphas, chests, selling and the stash, the vendor, skills and the action skill, and a mini-boss to finish. Each room explains itself as you walk in, and an arrow pad on the floor shows a second tip.
- Mapping: `monster_loot_random`, a point entity that becomes a random monster when the map loads (default pool or your own `loot_monsters` list), with an optional guaranteed gun drop (`loot_drop`, `loot_rarity`, `loot_weapon`, `loot_unique`). All monster keys work on it. In the TrenchBroom config and MODDING.md.

### Changed

- Vendor screens. The quartermaster finally threw out the clipboard. The vendor (V), its upgrades, workshop and missions pages, and the stash (B) are now drawn panels instead of plain text: page tabs across the top, a row per item with its key, the name in its rarity color and the price on the right in green when you can afford it or red when you can't. Upgrades show their ranks as pips. Keys are unchanged.
- Skills screen. The class pick and the skill tree (K) use the same drawn panels: a header per tier that dims while locked, one row per skill with its effect and rank, a green marker on skills that can take a point right now and gold on maxed ones. Class mod bonuses still show as a green +N. Keys are unchanged.
- Loot cards, loadout, challenges and looks. Every remaining text screen is now a drawn panel. Floor cards (guns, shields, grenade mods, class mods, relics) open with the item's icon framed in its rarity color. The loadout (I) lists each gun and piece of gear with its icon, and marks the gun you're holding. Challenges (J) show a progress bar toward the next tier. Guns in the vendor and stash get their icons too. Keys are unchanged.
- Readable messages. On-screen messages (tutorial notes, "Ammo refilled", Second Chance and the rest) now sit on a dark box, so white text stays readable against bright walls.
- HUD: XP bar and action skill meter. The level line under your gun is now a level badge with an XP bar, your cash and a blinking K +N chip when skill points are waiting; side missions still show at the end. The action skill (F) is a meter that fills while it cools down, shows READY in green, drains in gold while Overcharge is active, and turns red with a countdown during Second Chance.

## 0.9.7

### Added

- Alien-tech guns. Military intel has picked up rumors that the Strogg have been modifying our guns, and in NG+ two new Rare+ gun types start turning up in the field. The **Ripper** fires bouncing ion discs from cells; Brask barrels and some Epic+ rolls fan out an extra disc. The **Tracker** fires homing shots from slugs and locks onto the monster nearest your crosshair when you miss. They take elements, crits and proficiency like any gun, but have no uniques.
- Character looks. Supply finally cleared the old Marine uniforms out of storage, and the quartermaster says you've earned the right to pick your own. Page 3 of the challenges screen (J) picks your player skin from 28 stock skins and an aura (a pulsing color shell or a trail) from 8. Both are free; the rarer ones unlock with player level, challenge tiers, NG+ and beating the raid boss. Keys 1/2 skin, 3/4 aura, 5 reset, 6 shows a 10-second preview of you. Teammates see your look in coop.
- Minimap. From our friends at Foxhound, we have acquired new radar tech. A green radar in the top-right corner draws the map as you explore it: floors shaded by height, solid lines for walls, dashed lines for ledges, red for lava and slime. It marks nearby items (health, armor, ammo, weapons, keys, powerups), loot by rarity color (following your loot filter) and chests. Items on another floor show hollow with a tick above or below. No monsters are shown. M cycles corner radar, full-screen map, off. Works on every map, and in co-op for every player; co-op players share the explored map.
- Co-op trading. Command has lifted the ban on sharing weapons between squadmates; what the Strogg can modify, Marines can pass along. Look at a teammate up close and press H to hand them the gun you're holding. It goes into their first empty slot without switching their weapon, or into their stash if their slots are full. You can't give away your last gun. Dropping a gun with G still works too: anyone can pick it up.

### Changed

- Legendary and Mythic drops get half as much boost from luck, so late NG+ is no longer flooded with them. Guaranteed drops (bosses, Omegas, missions) are unchanged.
- The final boss is enraged in NG+. Jorg and Makron get x3 health and x1.5 damage on top of normal scaling, and aim better: Makron's railgun fires at where you are instead of where you were, both bosses lead their BFG shots, Makron's blaster sweep is narrower and centered on you, and Jorg's chainguns are twice as tight. The raid's Makron is unchanged. NG+ 1 final boss went down in about 7 seconds.
- NG+ monsters fight smarter. Each cycle counts as one higher difficulty step for their AI: they react to incoming shots more often (50% +15% per cycle), dodge more readily, open fire sooner after spotting you, and shrug off 20% of pain flinches per cycle (max 60%). Their health and damage scaling is separate.
- NG+ monsters now get x2 health per cycle (was x1.5); damage stays x1.5. Fights at NG+ 1 took 2-4 seconds a kill at Lv 60.
- The vendor no longer has a stash page in its page cycle; B already opens the stash anywhere for free. Extra stash slots (Lv 60) are bought from the Workshop page, item 7. Pressing B with the vendor open still shows the stash, and B again returns to the shop.

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
