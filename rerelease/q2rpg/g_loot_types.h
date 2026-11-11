// q2rpg: loot data types.
// Included from g_local.h after item_id_t / gitem_t so persistent structs can embed them.
#pragma once

// Number of weapon slots a player carries.
constexpr int32_t LOOT_NUM_SLOTS = 4;

enum loot_rarity_t : uint8_t
{
	LOOT_RARITY_COMMON,
	LOOT_RARITY_UNCOMMON,
	LOOT_RARITY_RARE,
	LOOT_RARITY_EPIC,
	LOOT_RARITY_LEGENDARY,
	LOOT_RARITY_MYTHIC, // NG+ only

	LOOT_RARITY_COUNT
};

// Gun families. Each maps to a stock weapon item that provides
// models, ammo type and firing logic.
enum loot_archetype_t : uint8_t
{
	LOOT_ARCH_PISTOL,     // blaster
	LOOT_ARCH_SHOTGUN,    // shotgun
	LOOT_ARCH_SCATTERGUN, // super shotgun
	LOOT_ARCH_SMG,        // machinegun
	LOOT_ARCH_MINIGUN,    // chaingun
	LOOT_ARCH_LAUNCHER,   // grenade launcher
	LOOT_ARCH_ROCKET,     // rocket launcher
	LOOT_ARCH_PLASMA,     // hyperblaster
	LOOT_ARCH_RAIL,       // railgun
	LOOT_ARCH_BFG,        // bfg10k

	LOOT_ARCH_COUNT
};

// Fictional manufacturers; each biases the stat rolls.
enum loot_maker_t : uint8_t
{
	LOOT_MAKER_HARDLINE, // heavy hitter: damage up, fire rate and mag down
	LOOT_MAKER_VEXA,     // spray: fire rate and mag up, accuracy down
	LOOT_MAKER_BRASK,    // extra pellets/projectile damage, wide spread
	LOOT_MAKER_KESSLER,  // precision: accuracy and crit up, slow reload
	LOOT_MAKER_ORBITEK,  // logistics: big mag, fast reload, damage down
	LOOT_MAKER_SALVAGE,  // strogg salvage: wild rolls in every direction

	LOOT_MAKER_COUNT
};

// Gun parts. Each part comes from a manufacturer and shifts stats.
enum loot_part_t : uint8_t
{
	LOOT_PART_STOCK,    // recoil, handling (switch speed)
	LOOT_PART_BARREL,   // damage, spread, pellets
	LOOT_PART_TRIGGER,  // fire rate
	LOOT_PART_MAGAZINE, // magazine size, reload time
	LOOT_PART_SCOPE,    // accuracy, crit

	LOOT_PART_COUNT
};

// Elemental damage applied as a status effect on hit.
enum loot_element_t : uint8_t
{
	LOOT_ELEM_NONE,
	LOOT_ELEM_FIRE,      // burn damage over time
	LOOT_ELEM_SHOCK,     // stagger, bonus vs power armor
	LOOT_ELEM_CORROSIVE, // melt damage over time, target takes extra damage

	LOOT_ELEM_COUNT
};

// Active elemental effects on a monster.
struct loot_status_t
{
	bool active;
	gtime_t next_tick;
	edict_t *attacker;

	gtime_t burn_end;
	float burn_dps;

	gtime_t melt_end;
	float melt_dps;

	gtime_t shock_end;
	float shock_dmg; // per tick

	int32_t shell; // RF_SHELL_* bits this system added
};

// Alternate attacks for badass monsters.
enum loot_elite_fire_t : uint8_t
{
	LOOT_EFIRE_NONE,
	LOOT_EFIRE_BULLET, // original kinds only; never rolled as alternate
	LOOT_EFIRE_SHOTGUN,
	LOOT_EFIRE_BLASTER,
	LOOT_EFIRE_GRENADE,
	LOOT_EFIRE_ROCKET,
	LOOT_EFIRE_RAIL,

	LOOT_EFIRE_COUNT
};

// Shield specials.
enum loot_shield_special_t : uint8_t
{
	LOOT_SHIELD_PLAIN,
	LOOT_SHIELD_NOVA,  // elemental blast when the shield breaks
	LOOT_SHIELD_SPIKE, // melee attackers take damage back

	LOOT_SHIELD_SPECIAL_COUNT
};

// One rolled shield. Equipped on the player or lying on a floor item.
struct loot_shield_t
{
	bool valid;
	uint32_t seed;
	int32_t level;
	loot_rarity_t rarity;
	loot_maker_t maker;

	float capacity;
	float delay; // seconds after the last hit before recharging
	float rate;  // points per second while recharging

	loot_shield_special_t special;
	loot_element_t element; // nova element

	char name[48];
};

// Grenade mods (g_grenade.cpp): one equipped, thrown with the "grenade" command (Q)
enum loot_grenade_type_t : uint8_t
{
	LOOT_GRENADE_STANDARD, // frag
	LOOT_GRENADE_CLUSTER,  // bursts into 4 bomblets
	LOOT_GRENADE_STICKY,   // sticks to the first monster it hits
	LOOT_GRENADE_VORTEX,   // pulls monsters in for 1s, then blows
	LOOT_GRENADE_LEECH,    // heals the thrower for part of the damage
	LOOT_GRENADE_HOPPER,   // blasts on each of its first 3 bounces
	LOOT_GRENADE_AIRBURST, // bursts overhead and rains shards
	LOOT_GRENADE_TYPE_COUNT
};

struct loot_grenade_t
{
	bool valid;
	uint32_t seed;
	int32_t level;
	loot_rarity_t rarity;
	loot_maker_t maker;
	loot_grenade_type_t type;
	loot_element_t element;

	float damage;
	float radius;
	float fuse;        // seconds
	float speed;       // throw speed
	float elem_chance; // per monster caught in the blast

	char name[48];
};

// Skill trees (classes). Picking one locks it until respec / new game.
enum loot_tree_t : uint8_t
{
	LOOT_TREE_NONE,
	LOOT_TREE_GUNNER,       // action: turret
	LOOT_TREE_JUGGERNAUT,   // action: slam
	LOOT_TREE_ELEMENTALIST, // action: overcharge

	LOOT_TREE_COUNT
};

// Skills per tree: tier 1 = 0-2, tier 2 = 3-5, tier 3 = 6-7, capstone = 8.
constexpr int32_t LOOT_NUM_SKILLS = 12; // 9 base + NG+ tier-4 row
constexpr int32_t LOOT_MAX_LEVEL = 50;  // player level cap (Loot_LevelCap adds LOOT_NGPLUS_LEVEL_CAP_BONUS in NG+)
constexpr int32_t LOOT_NGPLUS_LEVEL_CAP_BONUS = 10;
constexpr int32_t LOOT_NUM_CHALLENGES = 13; // g_challenge.cpp table size
constexpr int32_t LOOT_CHALLENGE_TIERS = 4; // tier 4 is NG+ only

// NG+ Alpha traits (bit flags on edict_t::loot_alpha_traits, g_ngplus.cpp)
enum loot_alpha_trait_t : int32_t
{
	LOOT_TRAIT_VAMPIRIC = 1 << 0, // heals half the damage it deals
	LOOT_TRAIT_VOLATILE = 1 << 1, // explodes shortly after death
	LOOT_TRAIT_WARDED = 1 << 2,   // double shield
	LOOT_TRAIT_SWIFT = 1 << 3,    // animates even faster
	LOOT_TRAIT_SUMMONER = 1 << 4, // calls 2 of its kind at half health
};
constexpr int32_t LOOT_TRAIT_COUNT = 5;

// NG+ map modifier, one per map (level_locals_t::loot_map_mod, g_ngplus.cpp)
enum loot_map_mod_t : int32_t
{
	LOOT_MAPMOD_NONE,
	LOOT_MAPMOD_ALPHA_SURGE,     // 3x Alpha chance
	LOOT_MAPMOD_GOLD_RUSH,       // 2x cash
	LOOT_MAPMOD_LOOT_FRENZY,     // +1.5 luck, monsters +30% health
	LOOT_MAPMOD_GLASS_CANNON,    // monsters -30% health, +40% damage
	LOOT_MAPMOD_REINFORCEMENTS,  // 3x death spawn chance
	LOOT_MAPMOD_ELEMENTAL_STORM, // 2x element proc chance (guns, grenades)
	LOOT_MAPMOD_BOUNTY,          // one guaranteed Alpha Omega, 2x drops
	LOOT_MAPMOD_FORTIFIED,       // 2x monster shields, 2x shield drops
	LOOT_MAPMOD_BLOODTHIRSTY,    // monsters heal 50% of damage they deal you, +50% XP
	LOOT_MAPMOD_COUNT
};

// Vendor upgrades bought with cash (g_vendor.cpp)
enum loot_upgrade_t
{
	LOOT_UPGRADE_AMMO,          // +20% max ammo per rank
	LOOT_UPGRADE_HEALTH,        // +10 max health per rank
	LOOT_UPGRADE_SHIELD,        // +10% shield capacity per rank
	LOOT_UPGRADE_GUN_DAMAGE,    // +3% gun damage per rank
	LOOT_UPGRADE_CRIT,          // +5% crit damage per rank
	LOOT_UPGRADE_RELOAD,        // -5% reload time per rank
	LOOT_UPGRADE_GRENADE,       // +5% grenade mod damage per rank
	LOOT_UPGRADE_SECOND_CHANCE, // +4% second chance per rank
	LOOT_UPGRADE_COUNT
};

// Unique legendaries: two per archetype (first = archetype + 1; second, NG+ only,
// = archetype + 1 + LOOT_ARCH_COUNT).
enum loot_unique_t : uint8_t
{
	LOOT_UNIQUE_NONE,
	LOOT_UNIQUE_ECHO,        // pistol: delayed second bolt
	LOOT_UNIQUE_RICOCHET,    // shotgun: pellets bounce once
	LOOT_UNIQUE_DRAGONMOUTH, // scattergun: always fire, every hit ignites
	LOOT_UNIQUE_SWARM,       // smg: every 5th shot adds a homing bolt
	LOOT_UNIQUE_SPINCYCLE,   // minigun: fire rate ramps while held
	LOOT_UNIQUE_CLUSTER,     // launcher: grenades split into bomblets
	LOOT_UNIQUE_HYDRA,       // rocket: splits into three
	LOOT_UNIQUE_SEEKER,      // plasma: bolts home
	LOOT_UNIQUE_LANCER,      // rail: chain-shocks nearby monsters
	LOOT_UNIQUE_SINGULARITY, // bfg: impact pulls monsters in

	// second uniques (NG+): value = archetype + 1 + LOOT_ARCH_COUNT
	LOOT_UNIQUE_LASTWORD,    // pistol: last round in the magazine x3, always crits
	LOOT_UNIQUE_BULWARK,     // shotgun: pellet hits restore shield
	LOOT_UNIQUE_PRISM,       // scattergun: element cycles each shot
	LOOT_UNIQUE_TICKTOCK,    // smg: every 10th hit marks the target
	LOOT_UNIQUE_FURNACE,     // minigun: damage ramps while held
	LOOT_UNIQUE_TARPIT,      // launcher: grenades leave a corrosive puddle
	LOOT_UNIQUE_AFTERBURNER, // rocket: speeds up, ignites monsters it passes
	LOOT_UNIQUE_PINBALL,     // plasma: bolts bounce once off walls
	LOOT_UNIQUE_EXECUTIONER, // rail: finishes non-boss targets under 15%
	LOOT_UNIQUE_SUPERNOVA,   // bfg: no tendrils, bigger blast, all three statuses

	LOOT_UNIQUE_COUNT
};

// One rolled gun instance. Lives in a player slot or on a floor item.
struct loot_weapon_t
{
	bool valid;
	uint32_t seed;
	item_id_t base; // stock weapon item that drives the gun
	loot_archetype_t archetype;
	loot_rarity_t rarity;
	loot_maker_t maker; // barrel maker; names the gun
	// loot_maker_t per part; uint8_t because the save system cannot store arrays of enums
	std::array<uint8_t, LOOT_PART_COUNT> parts;

	float dmg_mult;      // multiplies stock damage
	float rate_mult;     // multiplies animation rate while firing
	float spread_mult;   // multiplies stock spread
	float crit_mult;     // damage multiplier on head hits
	float reload_time;   // seconds
	float recoil_mult;   // view kick; 0 in pre-parts saves = 1
	float handling_mult; // raise/lower speed; 0 in pre-parts saves = 1

	int32_t mag_size; // in ammo units; 0 = no magazine (infinite)
	int32_t mag_ammo;
	int32_t pellets_bonus; // extra pellets for shotguns

	loot_element_t element;
	float elem_chance; // proc chance per hit, 0..1

	int32_t level;  // item level; 0 in pre-M3 saves = 1
	int32_t unique; // loot_unique_t

	// part follow-ups; 0 in older saves = neutral
	float proj_speed; // barrel: projectile speed multiplier
	float zoom;       // scope: fov divisor while zoomed
	int32_t burst;    // trigger: shots per burst (0 = full auto)

	char name[48];
};

// Per-map balance numbers for the loot_stats command and loot_stats.csv.
// Lives in level_locals_t, so it resets each map and rewinds with savegames.
struct loot_stats_t
{
	int32_t kills; // player-caused monster kills
	int32_t elite_kills;
	int32_t boss_kills;
	std::array<int32_t, LOOT_RARITY_COUNT> gun_drops;             // monster drops only
	std::array<int32_t, LOOT_RARITY_COUNT> shield_drops;          // monster drops only
	int32_t xp;                                                   // XP per player from kills (before the under-level halving)
	int32_t cash_dropped;                                         // value of cash pickups spawned
	int32_t cash_taken;                                           // value of cash pickups collected
	int32_t money_spent;                                          // vendor purchases
	int32_t money_sold;                                           // vendor sales
	int32_t grenade_mod_drops;                                    // grenade mods from monsters
	std::array<int32_t, LOOT_GRENADE_TYPE_COUNT> grenades_thrown; // by type (cluster bomblets count as cluster)
	std::array<int32_t, LOOT_GRENADE_TYPE_COUNT> grenade_kills;   // by type
	int32_t grenade_damage;                                       // grenade damage dealt to monsters
};
