// q2rpg: procedural loot guns.
// Rolled guns are per-instance data layered on stock weapon items:
// the stock item supplies models, ammo and firing code, the roll
// scales damage, rate, spread, magazine and reload.

#include "../g_local.h"

#include <random>
#include <ctime>

cvar_t *g_loot_droprate;
cvar_t *g_loot_rarity_bonus;
cvar_t *g_loot_debug;
cvar_t *g_loot_logs; // q2rpg: 0 = write no local stat files (loot_stats.csv, q2rpg_events.log)
cvar_t *g_loot_monster_health;
cvar_t *g_loot_elite_chance;
cvar_t *g_loot_guard_shield;
cvar_t *g_loot_monster_shield;
cvar_t *g_loot_monster_damage;
cvar_t *g_loot_death_spawn;

void Loot_InitCvars()
{
	g_loot_droprate = gi.cvar("g_loot_droprate", "0.35", CVAR_NOFLAGS);
	g_loot_rarity_bonus = gi.cvar("g_loot_rarity_bonus", "0", CVAR_NOFLAGS);
	g_loot_debug = gi.cvar("g_loot_debug", "0", CVAR_NOFLAGS);
	g_loot_logs = gi.cvar("g_loot_logs", "1", CVAR_ARCHIVE);
	g_loot_monster_health = gi.cvar("g_loot_monster_health", "1.5", CVAR_NOFLAGS);
	g_loot_elite_chance = gi.cvar("g_loot_elite_chance", "0.12", CVAR_NOFLAGS);
	g_loot_guard_shield = gi.cvar("g_loot_guard_shield", "1.0", CVAR_NOFLAGS);
	g_loot_monster_shield = gi.cvar("g_loot_monster_shield", "0.5", CVAR_NOFLAGS); // shield for every monster, x its health
	g_loot_death_spawn = gi.cvar("g_loot_death_spawn", "0.1", CVAR_NOFLAGS);       // chance a dying monster spawns 1-3 more
	g_loot_monster_damage = gi.cvar("g_loot_monster_damage", "1.3", CVAR_NOFLAGS); // monster damage to players before level scaling
}

//==========================================================================
// Tables
//==========================================================================

struct loot_arch_info_t
{
	item_id_t base;
	const char *name;
	int32_t mag;    // base magazine in ammo units (0 = none)
	float reload;   // base reload seconds
	int32_t weight; // drop weight
	loot_rarity_t min_rarity;
	bool shotgun;
};

static const loot_arch_info_t loot_archs[LOOT_ARCH_COUNT] = {
    {IT_WEAPON_BLASTER, "Pistol", 12, 1.2f, 6, LOOT_RARITY_COMMON, false}, // infinite reserve
    {IT_WEAPON_SHOTGUN, "Shotgun", 8, 1.8f, 16, LOOT_RARITY_COMMON, true},
    {IT_WEAPON_SSHOTGUN, "Scattergun", 4, 2.0f, 10, LOOT_RARITY_COMMON, true},
    {IT_WEAPON_MACHINEGUN, "SMG", 30, 1.5f, 16, LOOT_RARITY_COMMON, false},
    {IT_WEAPON_CHAINGUN, "Minigun", 100, 3.0f, 8, LOOT_RARITY_COMMON, false},
    {IT_WEAPON_GLAUNCHER, "Launcher", 6, 2.2f, 8, LOOT_RARITY_COMMON, false},
    {IT_WEAPON_RLAUNCHER, "Rocket Launcher", 4, 2.4f, 7, LOOT_RARITY_COMMON, false},
    {IT_WEAPON_HYPERBLASTER, "Plasma Caster", 40, 2.0f, 7, LOOT_RARITY_COMMON, false},
    {IT_WEAPON_RAILGUN, "Rail Rifle", 4, 2.6f, 6, LOOT_RARITY_UNCOMMON, false},
    {IT_WEAPON_BFG, "Annihilator", 100, 3.0f, 1, LOOT_RARITY_EPIC, false},
};

// Fictional manufacturers. Each part a maker builds shifts the stats it governs.
struct loot_maker_info_t
{
	const char *name;
	const char *short_name;
	float recoil, handling;   // stock
	float dmg, barrel_spread; // barrel
	int32_t pellets;          // barrel (shotguns)
	float rate;               // trigger
	float mag, reload;        // magazine
	float scope_spread, crit; // scope
};

static const loot_maker_info_t loot_makers[LOOT_MAKER_COUNT] = {
    //                                      stock         barrel             trig   magazine      scope
    {"Hardline Arms", "Hardline", 0.85f, 0.90f, 1.20f, 0.95f, 0, 0.88f, 0.80f, 1.00f, 0.95f, 0.10f},
    {"Vexa Dynamics", "Vexa", 1.15f, 1.20f, 0.92f, 1.25f, 0, 1.25f, 1.35f, 1.00f, 1.10f, 0.00f},
    {"Brask & Sons", "Brask", 1.00f, 1.00f, 1.05f, 1.35f, 3, 0.95f, 1.00f, 1.10f, 1.10f, 0.00f},
    {"Kessler Precision", "Kessler", 0.70f, 0.90f, 1.05f, 0.75f, 0, 0.95f, 0.90f, 1.20f, 0.70f, 0.75f},
    {"Orbitek Supply", "Orbitek", 1.00f, 1.15f, 0.92f, 1.00f, 0, 1.05f, 1.60f, 0.70f, 1.00f, 0.00f},
    {"Strogg Salvage", "Salvage", 1.00f, 1.00f, 1.00f, 1.00f, 1, 1.00f, 1.00f, 1.00f, 1.00f, 0.25f},
};

static const char *const loot_part_names[LOOT_PART_COUNT] = {"Stock", "Barrel", "Trigger", "Magazine", "Scope"};

const char *Loot_PartName(loot_part_t part)
{
	return loot_part_names[part < LOOT_PART_COUNT ? part : 0];
}

struct loot_rarity_info_t
{
	const char *name;
	float dmg;
	int32_t weight;
	renderfx_t shell;
};

static const loot_rarity_info_t loot_rarities[LOOT_RARITY_COUNT] = {
    {"Common", 1.00f, 600, RF_NONE},
    {"Uncommon", 1.10f, 250, RF_SHELL_GREEN},
    {"Rare", 1.22f, 100, RF_SHELL_BLUE},
    {"Epic", 1.36f, 40, RF_SHELL_RED | RF_SHELL_BLUE},
    {"Legendary", 1.55f, 10, RF_SHELL_RED | RF_SHELL_GREEN},
    {"Mythic", 1.80f, 3, RF_SHELL_GREEN | RF_SHELL_BLUE}, // weight only applies in NG+ (Loot_RollRarity)
};

// Name prefixes keyed by the gun's standout stat.
enum loot_stat_t
{
	LOOT_STAT_DAMAGE,
	LOOT_STAT_RATE,
	LOOT_STAT_ACCURACY,
	LOOT_STAT_MAG,
	LOOT_STAT_CRIT,
	LOOT_STAT_RELOAD,
	LOOT_STAT_COUNT
};

static const char *const loot_prefixes[LOOT_STAT_COUNT][3] = {
    {"Brutal", "Savage", "Crushing"},
    {"Frantic", "Rapid", "Twitchy"},
    {"Steady", "True", "Keen"},
    {"Hoarding", "Bottomless", "Stocked"},
    {"Lethal", "Cruel", "Vicious"},
    {"Nimble", "Swift", "Slick"},
};

static const char *const loot_legendary_names[] = {
    "Stroggbane", "Last Rites", "Grudge", "Big Iron", "Doomsayer", "Hellmouth", "Old Faithful", "Gutpunch"};

static const char *const loot_mythic_names[] = {
    "Worldbreaker", "Starfall", "Oblivion", "Eventide", "Kingmaker", "Black Sun", "Afterlife", "Godhand"};

const char *Loot_RarityName(loot_rarity_t rarity)
{
	return loot_rarities[rarity < LOOT_RARITY_COUNT ? rarity : 0].name;
}

float Loot_RarityDamage(loot_rarity_t rarity)
{
	return loot_rarities[rarity < LOOT_RARITY_COUNT ? rarity : 0].dmg;
}

renderfx_t Loot_RarityShell(loot_rarity_t rarity)
{
	return loot_rarities[rarity < LOOT_RARITY_COUNT ? rarity : 0].shell;
}

static const char *const loot_element_names[LOOT_ELEM_COUNT] = {"None", "Fire", "Shock", "Corrosive"};
static const char *const loot_element_short[LOOT_ELEM_COUNT] = {"", "Fir", "Shk", "Cor"};

// Per-archetype proc scale: pellet guns hit many times per shot,
// slow single-shot guns hit rarely.
static const float loot_elem_scale[LOOT_ARCH_COUNT] = {
    1.0f,  // pistol
    0.25f, // shotgun (per pellet)
    0.2f,  // scattergun (per pellet)
    0.7f,  // smg
    0.5f,  // minigun
    2.5f,  // launcher
    2.5f,  // rocket
    0.8f,  // plasma
    2.5f,  // rail
    2.5f,  // bfg
};

const char *Loot_ElementName(loot_element_t element)
{
	return loot_element_names[element < LOOT_ELEM_COUNT ? element : 0];
}

// effective proc chance per hit
static float Loot_ElemChance(const loot_weapon_t &gun)
{
	if (!gun.element || gun.archetype >= LOOT_ARCH_COUNT)
		return 0.f;
	return std::min(0.9f, gun.elem_chance * loot_elem_scale[gun.archetype]);
}

int32_t Loot_GunLevel(const loot_weapon_t &gun)
{
	return std::max(1, gun.level);
}

// Item level scales damage; called once right after a roll.
void Loot_SetLevel(loot_weapon_t &gun, int32_t level)
{
	gun.level = std::clamp(level, 1, Loot_LevelCap() + 5);
	gun.dmg_mult *= 1.0f + 0.06f * (gun.level - 1);
}

// Vendor workshop: re-level an owned gun, swapping the old level's damage scaling for the new one.
void Loot_RelevelGun(loot_weapon_t &gun, int32_t level)
{
	gun.dmg_mult /= 1.0f + 0.06f * (Loot_GunLevel(gun) - 1);
	Loot_SetLevel(gun, level);
}

// 1 + per_rank x rank of a vendor upgrade; 1 for non-players.
float Loot_UpgradeMult(edict_t *ent, loot_upgrade_t upgrade, float per_rank)
{
	if (!ent || !ent->client)
		return 1.f;
	return 1.f + per_rank * ent->client->pers.loot_upgrades[upgrade];
}

const char *Loot_MakerShortName(loot_maker_t maker)
{
	return loot_makers[maker < LOOT_MAKER_COUNT ? maker : 0].short_name;
}

const char *Loot_MakerName(loot_maker_t maker)
{
	return loot_makers[maker < LOOT_MAKER_COUNT ? maker : 0].name;
}

loot_archetype_t Loot_ArchetypeForItem(item_id_t item)
{
	for (int32_t i = 0; i < LOOT_ARCH_COUNT; i++)
		if (loot_archs[i].base == item)
			return (loot_archetype_t)i;

	return LOOT_ARCH_COUNT;
}

bool Loot_IsBaseWeapon(item_id_t item)
{
	return Loot_ArchetypeForItem(item) != LOOT_ARCH_COUNT;
}

//==========================================================================
// Rolling
//==========================================================================

loot_weapon_t Loot_Roll(loot_archetype_t archetype, loot_rarity_t rarity, uint32_t seed)
{
	std::mt19937 rng(seed);
	auto rf = [&rng](float lo, float hi)
	{ return std::uniform_real_distribution<float>(lo, hi)(rng); };

	const loot_arch_info_t &arch = loot_archs[archetype];
	const loot_maker_t primary = (loot_maker_t)(rng() % LOOT_MAKER_COUNT);
	const float r = (float)rarity;

	loot_weapon_t gun{};
	gun.valid = true;
	gun.seed = seed;
	gun.base = arch.base;
	gun.archetype = archetype;
	gun.rarity = rarity;

	// pick parts: mostly the primary maker, sometimes mixed
	for (auto &part : gun.parts)
		part = (rng() % 100 < 60) ? primary : (loot_maker_t)(rng() % LOOT_MAKER_COUNT);
	gun.maker = (loot_maker_t)gun.parts[LOOT_PART_BARREL];

	// per-part noise; salvage parts swing wider
	auto noise = [&](loot_part_t part)
	{
		const float v = gun.parts[part] == LOOT_MAKER_SALVAGE ? 0.30f : 0.10f;
		return rf(1 - v, 1 + v);
	};
	auto mk = [&](loot_part_t part) -> const loot_maker_info_t &
	{ return loot_makers[gun.parts[part]]; };

	// stock
	gun.recoil_mult = std::clamp(mk(LOOT_PART_STOCK).recoil * noise(LOOT_PART_STOCK) / (1 + 0.05f * r), 0.3f, 2.0f);
	gun.handling_mult = std::clamp(mk(LOOT_PART_STOCK).handling * noise(LOOT_PART_STOCK) * (1 + 0.03f * r), 0.5f, 2.0f);

	// barrel
	gun.dmg_mult = loot_rarities[rarity].dmg * mk(LOOT_PART_BARREL).dmg * noise(LOOT_PART_BARREL);
	float spread = mk(LOOT_PART_BARREL).barrel_spread * noise(LOOT_PART_BARREL);
	if (arch.shotgun)
		gun.pellets_bonus = mk(LOOT_PART_BARREL).pellets + (rarity >= LOOT_RARITY_RARE ? (int32_t)(rng() % 2) : 0);

	// trigger
	gun.rate_mult = std::clamp(mk(LOOT_PART_TRIGGER).rate * noise(LOOT_PART_TRIGGER) * (1 + 0.03f * r), 0.6f, 2.0f);

	// scope
	spread *= mk(LOOT_PART_SCOPE).scope_spread * noise(LOOT_PART_SCOPE);
	gun.spread_mult = std::clamp(spread / (1 + 0.06f * r), 0.3f, 2.0f);
	gun.crit_mult = 1.5f + mk(LOOT_PART_SCOPE).crit + 0.1f * r + rf(0.f, 0.25f);

	// magazine
	const int32_t per_shot = std::max(1, GetItemByIndex(arch.base)->quantity);
	auto round_mag = [per_shot](float m)
	{ return std::max(per_shot, ((int32_t)roundf(m) / per_shot) * per_shot); };
	if (arch.mag > 0)
	{
		gun.mag_size = round_mag(arch.mag * mk(LOOT_PART_MAGAZINE).mag * noise(LOOT_PART_MAGAZINE) * (1 + 0.08f * r));
		gun.reload_time = std::max(0.5f, arch.reload * mk(LOOT_PART_MAGAZINE).reload * noise(LOOT_PART_MAGAZINE) / (1 + 0.05f * r));
	}

	// epic+ get one stat pushed further, mythic two
	const int32_t boosts = rarity >= LOOT_RARITY_MYTHIC ? 2 : rarity >= LOOT_RARITY_EPIC ? 1
	                                                                                     : 0;
	for (int32_t boost = 0; boost < boosts; boost++)
	{
		switch (rng() % 4)
		{
			case 0:
				gun.dmg_mult *= 1.2f;
				break;
			case 1:
				gun.rate_mult = std::min(2.0f, gun.rate_mult * 1.2f);
				break;
			case 2:
				gun.crit_mult += 0.5f;
				break;
			default:
				if (gun.mag_size)
					gun.mag_size = round_mag(gun.mag_size * 1.5f);
				else
					gun.dmg_mult *= 1.2f;
				break;
		}
	}
	gun.mag_ammo = gun.mag_size;

	// element: odds rise with rarity; some barrel makers lean to one element
	static const float elem_odds[LOOT_RARITY_COUNT] = {0.10f, 0.25f, 0.40f, 0.60f, 0.80f, 1.00f};
	if (rf(0.f, 1.f) < elem_odds[rarity])
	{
		loot_element_t e = (loot_element_t)(1 + rng() % (LOOT_ELEM_COUNT - 1));
		if (rf(0.f, 1.f) < 0.5f)
		{
			if (gun.maker == LOOT_MAKER_BRASK)
				e = LOOT_ELEM_FIRE;
			else if (gun.maker == LOOT_MAKER_VEXA)
				e = LOOT_ELEM_SHOCK;
			else if (gun.maker == LOOT_MAKER_SALVAGE)
				e = LOOT_ELEM_CORROSIVE;
		}
		gun.element = e;
		gun.elem_chance = 0.10f + 0.04f * r + rf(0.f, 0.06f);
		gun.dmg_mult *= 0.9f; // elemental guns trade some raw damage
	}

	// part follow-ups: barrel projectile speed, scope zoom, burst triggers
	static const float barrel_speed[LOOT_MAKER_COUNT] = {1.15f, 1.0f, 0.9f, 1.25f, 1.0f, 1.0f};
	static const float scope_zoom[LOOT_MAKER_COUNT] = {1.6f, 1.3f, 1.3f, 2.2f, 1.5f, 1.8f};
	gun.proj_speed = barrel_speed[gun.parts[LOOT_PART_BARREL]] * noise(LOOT_PART_BARREL);
	gun.zoom = scope_zoom[gun.parts[LOOT_PART_SCOPE]];

	if (archetype == LOOT_ARCH_SMG || archetype == LOOT_ARCH_MINIGUN || archetype == LOOT_ARCH_PLASMA)
	{
		const loot_maker_t trig = (loot_maker_t)gun.parts[LOOT_PART_TRIGGER];
		const float odds = (trig == LOOT_MAKER_HARDLINE || trig == LOOT_MAKER_KESSLER) ? 0.5f : 0.15f;
		if (rf(0.f, 1.f) < odds)
		{
			gun.burst = 3;
			gun.dmg_mult *= 1.2f; // bursts trade sustained fire for damage
		}
	}

	// name from the standout stat
	float scores[LOOT_STAT_COUNT] = {
	    gun.dmg_mult / loot_rarities[rarity].dmg,
	    gun.rate_mult,
	    1.0f / gun.spread_mult,
	    arch.mag ? (float)gun.mag_size / arch.mag : 0.f,
	    gun.crit_mult / 1.75f,
	    arch.mag ? arch.reload / gun.reload_time : 0.f,
	};
	int32_t best = 0;
	for (int32_t i = 1; i < LOOT_STAT_COUNT; i++)
		if (scores[i] > scores[best])
			best = i;

	if (rarity == LOOT_RARITY_MYTHIC)
		G_FmtTo(gun.name, "{} {}", loot_mythic_names[rng() % q_countof(loot_mythic_names)], arch.name);
	else if (rarity == LOOT_RARITY_LEGENDARY && rf(0.f, 1.f) < 0.30f)
		Unique_Make(gun);
	else if (rarity == LOOT_RARITY_LEGENDARY)
		G_FmtTo(gun.name, "{} {}", loot_legendary_names[rng() % q_countof(loot_legendary_names)], arch.name);
	else
		G_FmtTo(gun.name, "{} {} {}", loot_prefixes[best][rng() % 3], loot_makers[gun.maker].short_name, arch.name);

	return gun;
}

//==========================================================================
// Second Chance: a monster's killing blow can leave the player at 1 HP for
// 5 seconds instead. Monsters can't finish them in that window; a kill heals
// 30% max health, running out of time is death. Vendor upgrade, 4% per rank.
//==========================================================================

static constexpr gtime_t second_chance_time = 5_sec;
static constexpr float second_chance_per_rank = 0.04f;
static constexpr float second_chance_heal = 0.30f;
static bool second_chance_expiring; // the expiry kill must not be saved again

bool Loot_SecondChanceActive(const edict_t *ent)
{
	return ent && ent->client && ent->client->loot_second_chance_end > level.time;
}

bool Loot_SecondChanceSave(edict_t *targ, edict_t *attacker, const mod_t &mod)
{
	gclient_t *cl = targ->client;
	if (second_chance_expiring || deathmatch->integer || !cl)
		return false;

	// enemies only: never the world, triggers, lava, falls or your own splash
	const bool enemy = attacker && attacker != targ && (attacker->svflags & SVF_MONSTER);
	if (!enemy || mod.id == MOD_TELEFRAG || mod.id == MOD_SUICIDE)
		return false;

	if (Loot_SecondChanceActive(targ))
		return true; // immune to enemies during the window

	const int32_t rank = cl->pers.loot_upgrades[LOOT_UPGRADE_SECOND_CHANCE];
	if (!rank || frandom() >= second_chance_per_rank * rank)
		return false;

	cl->loot_second_chance_end = level.time + second_chance_time;
	cl->loot_second_chance_attacker = attacker;
	cl->loot_second_chance_mod = mod;
	gi.Center_Print(targ, "SECOND CHANCE!\nGet a kill!");
	Loot_LogEvent(targ, G_Fmt("SECOND CHANCE triggered by {}", attacker->classname ? attacker->classname : "?").data());
	gi.sound(targ, CHAN_AUTO, gi.soundindex("misc/pc_up.wav"), 1, ATTN_NORM, 0);
	return true;
}

void Loot_SecondChanceKill(edict_t *ent)
{
	if (!Loot_SecondChanceActive(ent) || ent->health <= 0)
		return;

	gclient_t *cl = ent->client;
	cl->loot_second_chance_end = 0_ms;
	cl->loot_second_chance_attacker = nullptr;
	ent->health = std::min(ent->max_health, ent->health + (int32_t)ceilf(ent->max_health * second_chance_heal));
	gi.Center_Print(ent, "Back in the fight!");
	Loot_LogEvent(ent, "SECOND CHANCE saved (got a kill)");
	gi.sound(ent, CHAN_AUTO, gi.soundindex("items/s_health.wav"), 1, ATTN_NORM, 0);
}

void Loot_SecondChanceFrame(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (!cl->loot_second_chance_end)
		return;

	// timer left over from a previous map or a load: drop it
	if (cl->loot_second_chance_end > level.time + second_chance_time || ent->health <= 0)
	{
		cl->loot_second_chance_end = 0_ms;
		cl->loot_second_chance_attacker = nullptr;
		return;
	}

	if (level.time < cl->loot_second_chance_end)
	{
		// pulsing red: the screen reads as "nearly dead"
		const float pulse = 0.5f + 0.5f * sinf(level.time.seconds() * 8.f);
		G_AddBlend(1.f, 0.f, 0.f, 0.15f + 0.15f * pulse, cl->ps.screen_blend);
		return;
	}

	// out of time: the enemy that downed us gets the kill
	edict_t *killer = cl->loot_second_chance_attacker;
	if (!killer || !killer->inuse)
		killer = world;
	const mod_t mod = cl->loot_second_chance_mod;
	cl->loot_second_chance_end = 0_ms;
	cl->loot_second_chance_attacker = nullptr;

	Loot_LogEvent(ent, "SECOND CHANCE ran out");
	second_chance_expiring = true;
	T_Damage(ent, killer, killer, vec3_origin, ent->s.origin, vec3_origin, 9999, 0, DAMAGE_NO_PROTECTION, mod);
	second_chance_expiring = false;
}

loot_rarity_t Loot_RollRarity(float luck)
{
	float weights[LOOT_RARITY_COUNT];
	float total = 0;

	for (int32_t i = 0; i < LOOT_RARITY_COUNT; i++)
	{
		weights[i] = loot_rarities[i].weight * (1.0f + std::max(0.f, luck) * i * 0.5f);
		if (i == LOOT_RARITY_MYTHIC && game.loot_ngplus <= 0)
			weights[i] = 0; // Mythic only drops in NG+
		total += weights[i];
	}

	float pick = frandom(total);
	for (int32_t i = 0; i < LOOT_RARITY_COUNT; i++)
	{
		if (pick < weights[i])
			return (loot_rarity_t)i;
		pick -= weights[i];
	}

	return LOOT_RARITY_COMMON;
}

loot_weapon_t Loot_RollRandom(float luck)
{
	const loot_rarity_t rarity = Loot_RollRarity(luck);

	int32_t total = 0;
	for (const auto &a : loot_archs)
		if (rarity >= a.min_rarity)
			total += a.weight;

	int32_t pick = irandom(total);
	loot_archetype_t arch = LOOT_ARCH_SHOTGUN;
	for (int32_t i = 0; i < LOOT_ARCH_COUNT; i++)
	{
		if (rarity < loot_archs[i].min_rarity)
			continue;
		if (pick < loot_archs[i].weight)
		{
			arch = (loot_archetype_t)i;
			break;
		}
		pick -= loot_archs[i].weight;
	}

	return Loot_Roll(arch, rarity, (uint32_t)mt_rand());
}

//==========================================================================
// Shields
//==========================================================================

struct loot_shield_maker_t
{
	float capacity, delay, rate;
	loot_shield_special_t special_bias;
	loot_element_t nova_bias;
};

static const loot_shield_maker_t loot_shield_makers[LOOT_MAKER_COUNT] = {
    {1.35f, 1.15f, 0.85f, LOOT_SHIELD_PLAIN, LOOT_ELEM_NONE},     // Hardline: big, slow
    {0.80f, 0.85f, 1.40f, LOOT_SHIELD_NOVA, LOOT_ELEM_SHOCK},     // Vexa: fast recharge
    {1.00f, 1.00f, 1.00f, LOOT_SHIELD_NOVA, LOOT_ELEM_FIRE},      // Brask: fire novas
    {1.00f, 0.90f, 1.00f, LOOT_SHIELD_SPIKE, LOOT_ELEM_NONE},     // Kessler: spikes
    {0.90f, 0.70f, 1.10f, LOOT_SHIELD_PLAIN, LOOT_ELEM_NONE},     // Orbitek: short delay
    {1.00f, 1.00f, 1.00f, LOOT_SHIELD_NOVA, LOOT_ELEM_CORROSIVE}, // Salvage: wild, corrosive
};

static const char *const loot_shield_prefixes[3][3] = {
    {"Bulwark", "Heavy", "Fortified"}, // capacity
    {"Eager", "Quick", "Restless"},    // delay
    {"Surging", "Flowing", "Rapid"},   // rate
};

static const char *const loot_shield_legendary_names[] = {"Bastion", "Last Stand", "Thornwall", "Starburst", "Iron Lung", "Second Wind"};

static float Loot_ShieldBaseCapacity(int32_t level)
{
	return 50.f * (1.0f + 0.12f * (std::max(1, level) - 1));
}

loot_shield_t Loot_RollShield(loot_rarity_t rarity, int32_t level, uint32_t seed)
{
	std::mt19937 rng(seed);
	auto rf = [&rng](float lo, float hi)
	{ return std::uniform_real_distribution<float>(lo, hi)(rng); };

	loot_shield_t sh{};
	sh.valid = true;
	sh.seed = seed;
	sh.level = std::clamp(level, 1, Loot_LevelCap() + 5);
	sh.rarity = rarity;
	sh.maker = (loot_maker_t)(rng() % LOOT_MAKER_COUNT);

	const loot_shield_maker_t &mk = loot_shield_makers[sh.maker];
	const float v = sh.maker == LOOT_MAKER_SALVAGE ? 0.30f : 0.10f;
	const float r = (float)rarity;

	sh.capacity = Loot_ShieldBaseCapacity(sh.level) * loot_rarities[rarity].dmg * mk.capacity * rf(1 - v, 1 + v);
	sh.delay = std::max(1.0f, 4.0f * mk.delay * rf(1 - v, 1 + v) / (1 + 0.05f * r));
	sh.rate = sh.capacity * 0.3f * mk.rate * rf(1 - v, 1 + v);

	static const float special_odds[LOOT_RARITY_COUNT] = {0.05f, 0.15f, 0.30f, 0.50f, 0.75f, 1.00f};
	if (rf(0.f, 1.f) < special_odds[rarity])
	{
		sh.special = (rf(0.f, 1.f) < 0.55f) ? LOOT_SHIELD_NOVA : LOOT_SHIELD_SPIKE;
		if (mk.special_bias != LOOT_SHIELD_PLAIN && rf(0.f, 1.f) < 0.5f)
			sh.special = mk.special_bias;

		if (sh.special == LOOT_SHIELD_NOVA)
		{
			sh.element = (loot_element_t)(1 + rng() % (LOOT_ELEM_COUNT - 1));
			if (mk.nova_bias != LOOT_ELEM_NONE && rf(0.f, 1.f) < 0.5f)
				sh.element = mk.nova_bias;
		}
	}

	// name
	const float scores[3] = {
	    sh.capacity / (Loot_ShieldBaseCapacity(sh.level) * loot_rarities[rarity].dmg),
	    4.0f / sh.delay,
	    sh.rate / (sh.capacity * 0.3f),
	};
	int32_t best = 0;
	for (int32_t i = 1; i < 3; i++)
		if (scores[i] > scores[best])
			best = i;

	const char *maker = loot_makers[sh.maker].short_name;
	if (rarity == LOOT_RARITY_MYTHIC)
		G_FmtTo(sh.name, "{} Mythic Shield", loot_shield_legendary_names[rng() % q_countof(loot_shield_legendary_names)]);
	else if (rarity == LOOT_RARITY_LEGENDARY)
		G_FmtTo(sh.name, "{}", loot_shield_legendary_names[rng() % q_countof(loot_shield_legendary_names)]);
	else if (sh.special == LOOT_SHIELD_NOVA)
		G_FmtTo(sh.name, "{} {} Nova", maker, Loot_ElementName(sh.element));
	else if (sh.special == LOOT_SHIELD_SPIKE)
		G_FmtTo(sh.name, "{} {} Spike Shield", loot_shield_prefixes[best][rng() % 3], maker);
	else
		G_FmtTo(sh.name, "{} {} Shield", loot_shield_prefixes[best][rng() % 3], maker);

	return sh;
}

// Shields are available from the first map in SP/coop.
bool Loot_ShieldsAvailable()
{
	return !deathmatch->integer;
}

static std::string Loot_ShieldSpecialText(const loot_shield_t &sh)
{
	if (sh.special == LOOT_SHIELD_NOVA)
		return std::string(Loot_ElementName(sh.element)) + " nova on break";
	if (sh.special == LOOT_SHIELD_SPIKE)
		return "Spikes hurt melee attackers";
	return "";
}

//==========================================================================
// Player loadout
//==========================================================================

// Weapon inventory counts mirror the slots so the stock weapon
// wheel and switching code only see guns the player carries.
static void Loot_SyncPers(client_persistant_t &pers)
{
	for (const auto &a : loot_archs)
		pers.inventory[a.base] = 0;

	for (const auto &slot : pers.loot_slots)
		if (slot.valid)
			pers.inventory[slot.base]++;

	// stock blaster stays as an emergency fallback
	if (!pers.inventory[IT_WEAPON_BLASTER])
		pers.inventory[IT_WEAPON_BLASTER] = 1;
}

void Loot_InitClient(gclient_t *client)
{
	for (auto &slot : client->pers.loot_slots)
		slot = {};

	// starter pistol is always plain
	do
		client->pers.loot_slots[0] = Loot_Roll(LOOT_ARCH_PISTOL, LOOT_RARITY_COMMON, (uint32_t)mt_rand());
	while (client->pers.loot_slots[0].element != LOOT_ELEM_NONE);
	Loot_SetLevel(client->pers.loot_slots[0], 1);
	client->pers.loot_active_slot = 0;
	client->pers.loot_level = 1;
	client->pers.loot_xp = 0;
	client->pers.loot_money = LOOT_START_MONEY; // enough for a starter gun and an ammo refill

	// coop player joining a game in progress: start at the party's level
	if (coop->integer)
	{
		int32_t party_level = 1;
		for (uint32_t i = 0; i < game.maxclients; i++)
			if (&game.clients[i] != client && game.clients[i].pers.connected)
				party_level = std::max(party_level, game.clients[i].pers.loot_level);

		if (party_level > 1)
		{
			client->pers.loot_level = party_level;
			client->pers.max_health += 5 * (party_level - 1); // same as the level-up bonus
			client->pers.health = client->pers.max_health;
			client->pers.loot_money = LOOT_START_MONEY * party_level;
			client->pers.loot_shields_unlocked = true;
			Loot_SetLevel(client->pers.loot_slots[0], party_level);
		}
	}
	client->loot_pending_slot = -1;
	Loot_SyncPers(client->pers);
}

float Loot_CritMult(const loot_weapon_t &gun)
{
	return gun.crit_mult + (gun.base == IT_WEAPON_BLASTER ? LOOT_PISTOL_CRIT_BONUS : 0.f);
}

loot_weapon_t *Loot_Active(edict_t *ent)
{
	if (!ent || !ent->client || deathmatch->integer)
		return nullptr;

	gclient_t *cl = ent->client;
	const int32_t s = cl->pers.loot_active_slot;

	if (s < 0 || s >= LOOT_NUM_SLOTS || !cl->pers.loot_slots[s].valid)
		return nullptr;
	if (!cl->pers.weapon || cl->pers.weapon->id != cl->pers.loot_slots[s].base)
		return nullptr;

	return &cl->pers.loot_slots[s];
}

void Loot_SyncInventory(edict_t *ent)
{
	if (!deathmatch->integer)
		Loot_SyncPers(ent->client->pers);
}

static void Loot_CancelReload(gclient_t *cl)
{
	cl->loot_reloading = false;
}

void Loot_OnWeaponChanged(edict_t *ent)
{
	gclient_t *cl = ent->client;
	Loot_CancelReload(cl);

	if (deathmatch->integer || !cl->pers.weapon)
	{
		cl->loot_pending_slot = -1;
		return;
	}

	const item_id_t id = cl->pers.weapon->id;
	const int32_t pending = cl->loot_pending_slot;
	cl->loot_pending_slot = -1;

	// explicit slot pick
	if (pending >= 0 && pending < LOOT_NUM_SLOTS && cl->pers.loot_slots[pending].valid && cl->pers.loot_slots[pending].base == id)
	{
		cl->pers.loot_active_slot = pending;
		return;
	}

	// keep current slot if it matches
	const int32_t cur = cl->pers.loot_active_slot;
	if (cur >= 0 && cur < LOOT_NUM_SLOTS && cl->pers.loot_slots[cur].valid && cl->pers.loot_slots[cur].base == id)
		return;

	// switched by wheel/weapnext: find a slot with this base
	for (int32_t i = 0; i < LOOT_NUM_SLOTS; i++)
	{
		if (cl->pers.loot_slots[i].valid && cl->pers.loot_slots[i].base == id)
		{
			cl->pers.loot_active_slot = i;
			return;
		}
	}

	cl->pers.loot_active_slot = -1;
}

void Loot_SelectSlot(edict_t *ent, int32_t slot)
{
	gclient_t *cl = ent->client;

	if (slot < 0 || slot >= LOOT_NUM_SLOTS || !cl->pers.loot_slots[slot].valid)
		return;
	if (ent->health <= 0)
		return;
	if (slot == cl->pers.loot_active_slot && Loot_Active(ent) && !cl->newweapon)
		return;

	cl->loot_pending_slot = slot;
	cl->newweapon = GetItemByIndex(cl->pers.loot_slots[slot].base);
}

// The weapon wheel and weapnext/weapprev only know stock weapons, so two slots
// with the same base were unreachable past the first. While holding one of them,
// selecting that weapon again (or next/prev) steps to the other slot. `wrap`
// cycles around (wheel); without it, next/prev fall through to stock at the end.
// q2rpg: true if a loot slot of this base weapon still has rounds in its magazine
bool Loot_HasLoadedGun(edict_t *ent, item_id_t base)
{
	if (!ent || !ent->client || deathmatch->integer)
		return false;
	for (const loot_weapon_t &gun : ent->client->pers.loot_slots)
		if (gun.valid && gun.base == base && gun.mag_size > 0 && gun.mag_ammo > 0)
			return true;
	return false;
}

bool Loot_CycleSameBase(edict_t *ent, const gitem_t *item, int32_t dir, bool wrap)
{
	gclient_t *cl = ent->client;
	if (deathmatch->integer || !item || cl->newweapon)
		return false;

	const int32_t cur = cl->pers.loot_active_slot;
	if (cur < 0 || cur >= LOOT_NUM_SLOTS || !cl->pers.loot_slots[cur].valid || cl->pers.loot_slots[cur].base != item->id)
		return false;

	for (int32_t step = 1; step < LOOT_NUM_SLOTS; step++)
	{
		int32_t slot = cur + dir * step;
		if (wrap)
			slot = (slot + LOOT_NUM_SLOTS) % LOOT_NUM_SLOTS;
		else if (slot < 0 || slot >= LOOT_NUM_SLOTS)
			return false;

		if (cl->pers.loot_slots[slot].valid && cl->pers.loot_slots[slot].base == item->id)
		{
			Loot_SelectSlot(ent, slot);
			return true;
		}
	}
	return false;
}

//==========================================================================
// Firing hooks
//==========================================================================

static int32_t Loot_Reserve(edict_t *ent, const loot_weapon_t &gun)
{
	gitem_t *weapon = GetItemByIndex(gun.base);
	if (!weapon->ammo)
		return 9999;

	gitem_t *ammo = GetItemByIndex(weapon->ammo);
	if (G_CheckInfiniteAmmo(ammo))
		return 9999;

	return ent->client->pers.inventory[weapon->ammo];
}

int32_t Loot_WeaponAmmo(edict_t *ent)
{
	loot_weapon_t *gun = Loot_Active(ent);

	if (gun && gun->mag_size > 0)
		return gun->mag_ammo;

	if (!ent->client->pers.weapon || !ent->client->pers.weapon->ammo)
		return 0;

	return ent->client->pers.inventory[ent->client->pers.weapon->ammo];
}

// Replaces the stock "has ammo for one shot" check in Weapon_HandleReady.
bool Loot_CanFire(edict_t *ent)
{
	gitem_t *weapon = ent->client->pers.weapon;
	loot_weapon_t *gun = Loot_Active(ent);

	if (gun && gun->mag_size > 0)
		return gun->mag_ammo >= std::max(1, weapon->quantity);

	return !weapon->ammo || ent->client->pers.inventory[weapon->ammo] >= weapon->quantity;
}

// Magazine size after skills (Deep Pockets), whole shots only.
static int32_t Loot_EffMag(edict_t *ent, const loot_weapon_t &gun)
{
	if (gun.mag_size <= 0)
		return 0;
	const int32_t per_shot = std::max(1, GetItemByIndex(gun.base)->quantity);
	const int32_t mag = (int32_t)roundf(gun.mag_size * Skill_MagMult(ent));
	return std::max(per_shot, (mag / per_shot) * per_shot);
}

// Scavenger: free rounds into the active gun's magazine, whole shots only
void Loot_RefillMag(edict_t *ent, float fraction)
{
	loot_weapon_t *gun = Loot_Active(ent);
	if (!gun || gun->mag_size <= 0)
		return;

	const int32_t eff = Loot_EffMag(ent, *gun);
	const int32_t per_shot = std::max(1, GetItemByIndex(gun->base)->quantity);
	const int32_t add = std::max(per_shot, (int32_t)ceilf(eff * fraction / per_shot) * per_shot);
	gun->mag_ammo = std::min(eff, gun->mag_ammo + add);
}

bool Loot_ConsumeAmmo(edict_t *ent, int32_t quantity)
{
	loot_weapon_t *gun = Loot_Active(ent);

	if (!gun || gun->mag_size <= 0)
		return false;

	Unique_OnShot(ent); // Swarm

	// burst trigger: pause after every burst
	if (gun->burst > 0 && ++ent->client->loot_burst_count >= gun->burst)
	{
		ent->client->loot_burst_count = 0;
		ent->client->loot_burst_pause = level.time + 300_ms;
	}

	// overcharge / bullet hose: shot is free
	if (Skill_Overcharged(ent) || Skill_SaveAmmo(ent))
		return true;

	gun->mag_ammo = std::max(0, gun->mag_ammo - quantity);
	return true;
}

bool Loot_IsReloading(edict_t *ent)
{
	return ent->client && ent->client->loot_reloading;
}

bool Loot_TryReload(edict_t *ent)
{
	gclient_t *cl = ent->client;
	loot_weapon_t *gun = Loot_Active(ent);

	if (!gun || gun->mag_size <= 0)
		return false;
	if (cl->loot_reloading)
		return true;
	if (gun->mag_ammo >= Loot_EffMag(ent, *gun))
		return false;
	if (Loot_Reserve(ent, *gun) <= 0)
		return false;
	if (cl->weaponstate == WEAPON_DROPPING || cl->weaponstate == WEAPON_ACTIVATING || cl->newweapon)
		return false;

	cl->loot_reloading = true;
	cl->loot_reload_start = level.time;
	cl->loot_reload_end = level.time + gtime_t::from_sec(gun->reload_time * Skill_ReloadMult(ent) / Loot_UpgradeMult(ent, LOOT_UPGRADE_RELOAD, 0.05f));
	gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
	return true;
}

static void Loot_FinishReload(edict_t *ent, loot_weapon_t &gun)
{
	gclient_t *cl = ent->client;
	gitem_t *weapon = GetItemByIndex(gun.base);
	const int32_t need = std::max(0, Loot_EffMag(ent, gun) - gun.mag_ammo);
	int32_t take = need;

	if (weapon->ammo && !G_CheckInfiniteAmmo(GetItemByIndex(weapon->ammo)))
	{
		take = std::min(need, cl->pers.inventory[weapon->ammo]);
		cl->pers.inventory[weapon->ammo] -= take;
	}

	gun.mag_ammo += take;
	cl->loot_reloading = false;
	gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/w_pkup.wav"), 1, ATTN_NORM, 0);
}

float Loot_DamageMult(edict_t *ent)
{
	loot_weapon_t *gun = Loot_Active(ent);
	return gun ? gun->dmg_mult * Skill_GunDamageMult(ent) : 1.0f;
}

float Loot_RateMult(edict_t *ent)
{
	loot_weapon_t *gun = Loot_Active(ent);
	return gun ? gun->rate_mult * Skill_RateMult(ent) * Unique_RateMult(ent) : 1.0f;
}

// pre-parts saves have 0 here; treat as neutral
static float Loot_Neutral(float v)
{
	return v > 0 ? v : 1.0f;
}

float Loot_RecoilMult(edict_t *ent)
{
	loot_weapon_t *gun = Loot_Active(ent);
	return gun ? Loot_Neutral(gun->recoil_mult) : 1.0f;
}

float Loot_HandlingMult(edict_t *ent)
{
	loot_weapon_t *gun = Loot_Active(ent);
	return gun ? Loot_Neutral(gun->handling_mult) : 1.0f;
}

int32_t Loot_Spread(edict_t *ent, int32_t spread)
{
	loot_weapon_t *gun = Loot_Active(ent);
	if (!gun)
		return spread;
	const float zoomed = ent->client->loot_zoom_applied ? 0.7f : 1.0f; // steadier while scoped
	return (int32_t)(spread * gun->spread_mult * Skill_SpreadMult(ent) * zoomed);
}

float Loot_ProjSpeed(edict_t *ent)
{
	loot_weapon_t *gun = Loot_Active(ent);
	return (gun && gun->proj_speed > 0) ? gun->proj_speed : 1.0f;
}

int32_t Loot_Pellets(edict_t *ent, int32_t count)
{
	loot_weapon_t *gun = Loot_Active(ent);
	return gun ? count + gun->pellets_bonus : count;
}

void Loot_Think(edict_t *ent)
{
	gclient_t *cl = ent->client;

	Unique_Frame(ent); // Spin Cycle ramp

	// burst trigger: hold fire during the pause; releasing resets the burst
	if (cl->loot_burst_pause > level.time + 1_sec)
		cl->loot_burst_pause = 0_ms;
	if (cl->loot_burst_pause > level.time)
	{
		cl->buttons &= ~BUTTON_ATTACK;
		cl->latched_buttons &= ~BUTTON_ATTACK;
	}
	else if (!(cl->buttons & BUTTON_ATTACK))
		cl->loot_burst_count = 0;

	if (!cl->loot_reloading)
		return;

	loot_weapon_t *gun = Loot_Active(ent);
	if (!gun || ent->health <= 0)
	{
		Loot_CancelReload(cl);
		return;
	}

	if (level.time >= cl->loot_reload_end)
		Loot_FinishReload(ent, *gun);
}

// 0..1 curve used to dip the view model during reloads
float Loot_ReloadDip(edict_t *ent)
{
	gclient_t *cl = ent->client;

	if (!cl || !cl->loot_reloading)
		return 0.f;

	const float total = (cl->loot_reload_end - cl->loot_reload_start).seconds();
	if (total <= 0)
		return 0.f;

	const float frac = std::clamp((level.time - cl->loot_reload_start).seconds() / total, 0.f, 1.f);
	return sinf(frac * PIf);
}

//==========================================================================
// Damage
//==========================================================================

// damage from loot systems themselves: no crits, procs or skill bonuses
static bool Loot_IsStatusMod(const mod_t &mod)
{
	return mod.id == MOD_LOOT_BURN || mod.id == MOD_LOOT_MELT || mod.id == MOD_LOOT_SHOCK || mod.id == MOD_LOOT_TURRET || mod.id == MOD_LOOT_SLAM;
}

// q2rpg: the gun behind a player hit: projectiles carry the gun that fired them,
// hitscan uses the held gun; status ticks and grenade mods have no gun
static loot_weapon_t *Loot_HitGun(edict_t *inflictor, edict_t *attacker, const mod_t &mod)
{
	if (!attacker || !attacker->client || Loot_IsStatusMod(mod) || mod.id == MOD_LOOT_GRENADE)
		return nullptr;
	if (inflictor && inflictor != attacker && inflictor->loot_shot_gun.valid)
		return &inflictor->loot_shot_gun;
	return Loot_Active(attacker);
}

void Loot_StatusSparks(edict_t *ent, loot_element_t element)
{
	static const int32_t colors[LOOT_ELEM_COUNT] = {0, 0xe0, 0x70, 0xd0};

	vec3_t pos = (ent->absmin + ent->absmax) * 0.5f;
	pos[0] += crandom() * ent->size[0] * 0.3f;
	pos[1] += crandom() * ent->size[1] * 0.3f;
	pos[2] += crandom() * ent->size[2] * 0.3f;

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_WELDING_SPARKS);
	gi.WriteByte(12);
	gi.WritePosition(pos);
	gi.WriteDir(vec3_t{0, 0, 1});
	gi.WriteByte(irandom(colors[element], colors[element] + 4));
	gi.multicast(pos, MULTICAST_PVS, false);
}

static void Loot_UpdateShell(edict_t *ent)
{
	loot_status_t &st = ent->loot_status;
	int32_t want = 0;

	if (st.burn_end > level.time)
		want |= (int32_t)RF_SHELL_RED;
	if (st.melt_end > level.time)
		want |= (int32_t)RF_SHELL_GREEN;
	if (st.shock_end > level.time)
		want |= (int32_t)RF_SHELL_BLUE;

	// applied each frame by M_SetEffects, which owns monster shell bits
	st.shell = want;
}

static void Loot_ClearStatus(edict_t *ent)
{
	ent->loot_status = {};
}

void Loot_ApplyStatus(edict_t *targ, edict_t *attacker, loot_element_t element, int32_t hit)
{
	loot_status_t &st = targ->loot_status;

	// Kindling / Lingering
	const float dot = (attacker && attacker->client) ? Skill_DotMult(attacker) : 1.0f;
	const float dur = (attacker && attacker->client) ? Skill_DurationMult(attacker) : 1.0f;
	auto secs = [dur](float s)
	{ return gtime_t::from_ms((int64_t)(s * dur * 1000.f)); };

	if (!st.active)
	{
		st = {};
		st.active = true;
		st.next_tick = level.time + 500_ms;
	}
	st.attacker = attacker;

	const float h = (float)std::max(1, hit);

	switch (element)
	{
		case LOOT_ELEM_FIRE:
			st.burn_dps = std::max({st.burn_end > level.time ? st.burn_dps : 0.f, h * 0.6f * dot, 4.f});
			st.burn_end = level.time + secs(3.f);
			break;
		case LOOT_ELEM_CORROSIVE:
			st.melt_dps = std::max({st.melt_end > level.time ? st.melt_dps : 0.f, h * 0.35f * dot, 3.f});
			st.melt_end = level.time + secs(4.f);
			break;
		case LOOT_ELEM_SHOCK:
			st.shock_dmg = std::max({st.shock_end > level.time ? st.shock_dmg : 0.f, h * 0.25f * dot, 2.f});
			st.shock_end = level.time + secs(1.5f);
			break;
		default:
			return;
	}

	Loot_StatusSparks(targ, element);
	Loot_UpdateShell(targ);
}

// Monster -> player damage: g_loot_monster_damage, +8% per level above 1, NG+ map modifier.
float Loot_MonsterDamageScale(const edict_t *monster)
{
	return std::max(0.f, g_loot_monster_damage->value) * (1.0f + 0.08f * (std::max(1, monster->loot_mlevel) - 1)) * NGPlus_MonsterDamageMult();
}

// Crits, corrosive vulnerability, shock vs power armor, and status procs.
// set by Loot_ModifyDamage for the hit being processed; read by Loot_RecordDamage
static bool loot_last_hit_crit;

int32_t Loot_ModifyDamage(edict_t *targ, edict_t *inflictor, edict_t *attacker, const vec3_t &point, int32_t damage, damageflags_t dflags, const mod_t &mod)
{
	loot_last_hit_crit = false;

	// slam landings take no fall damage
	if (targ->client && mod.id == MOD_FALLING && !deathmatch->integer && Skill_IgnoreFall(targ))
		return 0;

	// monsters hit harder: flat g_loot_monster_damage, +8% per level above 1
	if (damage && !deathmatch->integer && targ->client && attacker && attacker != targ && (attacker->svflags & SVF_MONSTER))
	{
		return (int32_t)ceilf(damage * Loot_MonsterDamageScale(attacker));
	}

	if (!damage || deathmatch->integer || !(targ->svflags & SVF_MONSTER) || targ->health <= 0)
		return damage;

	// corroded targets take extra damage from everything
	if (targ->loot_status.active && targ->loot_status.melt_end > level.time)
		damage = (int32_t)ceilf(damage * 1.25f);

	// Tick Tock marks: same bonus, stacks with corrosion
	if (targ->loot_mark_end > level.time)
		damage = (int32_t)ceilf(damage * 1.25f);

	// overhead health bar follows the last monster this player hit
	if (attacker && attacker->client)
	{
		attacker->client->loot_bar_ent = targ;
		attacker->client->loot_bar_time = level.time;
	}

	// tier-4 challenge badges cover every player damage source
	if (attacker && attacker->client && targ != attacker)
		damage = (int32_t)ceilf(damage * Challenge_BadgeDamageMult(attacker, targ, inflictor, mod));

	if (Loot_IsStatusMod(mod) || !attacker || !attacker->client || targ == attacker)
		return damage;

	// grenade mods carry their own element; the held gun's crit / element / skills don't apply
	if (mod.id == MOD_LOOT_GRENADE)
	{
		damage = (int32_t)ceilf(damage * Loot_UpgradeMult(attacker, LOOT_UPGRADE_GRENADE, 0.05f));
		Grenade_ApplyElement(targ, attacker, damage);
		return damage;
	}

	// projectiles use the gun that fired them, not the one held now
	loot_weapon_t *gun = Loot_HitGun(inflictor, attacker, mod);
	if (!gun)
		return damage;

	damage = (int32_t)ceilf(damage * Loot_UpgradeMult(attacker, LOOT_UPGRADE_GUN_DAMAGE, 0.03f));

	// Last Word / Furnace
	bool force_crit = false;
	damage = (int32_t)ceilf(damage * Unique_DamageMult(attacker, inflictor, *gun, force_crit));

	// top quarter of the bounding box counts as the head
	if (force_crit || (!(dflags & DAMAGE_RADIUS) && point[2] >= targ->absmax[2] - targ->size[2] * 0.25f))
	{
		damage = (int32_t)(damage * (Loot_CritMult(*gun) + Skill_CritBonus(attacker) + Challenge_BadgeCritBonus(attacker) +
		                             0.05f * attacker->client->pers.loot_upgrades[LOOT_UPGRADE_CRIT]));
		loot_last_hit_crit = true;
	}

	// Close Quarters / Conductor
	damage = (int32_t)ceilf(damage * Skill_OutgoingMult(attacker, targ));

	// shock cuts through power armor
	if (gun->element == LOOT_ELEM_SHOCK && targ->monsterinfo.power_armor_type != IT_NULL && targ->monsterinfo.power_armor_power > 0)
		damage = (int32_t)(damage * 1.5f);

	// element: overcharge gives plain guns fire; Attunement adds random procs
	loot_element_t element = gun->element;
	float chance = Loot_ElemChance(*gun);
	if (!element && Skill_Overcharged(attacker))
	{
		element = LOOT_ELEM_FIRE;
		chance = 0.15f;
	}
	chance = chance * Skill_ProcChanceMult(attacker) + (element ? Skill_ProcChanceBonus(attacker) : 0.f);
	Unique_Element(attacker, *gun, element, chance); // Dragonmouth / Prism
	chance *= NGPlus_ProcChanceMult();               // Elemental Storm

	if (!element && Skill_Attune(attacker))
	{
		element = (loot_element_t)(1 + irandom(LOOT_ELEM_COUNT - 1));
		chance = 1.0f;
	}

	if (element && frandom() < chance)
	{
		element = Skill_ProcElement(attacker, element);                              // Prism Heart
		damage = (int32_t)ceilf(damage * Skill_FusionMult(attacker, targ, element)); // Fusion, before the new status lands
		Loot_ApplyStatus(targ, attacker, element, damage);
		LootFx_ProcLight(attacker, element); // gun procs only, not turret / slam / spread

		if (g_loot_debug->integer)
			gi.Com_PrintFmt("[loot] {} proc on {} (hit {}, burn {:.1f} melt {:.1f} shock {:.1f})\n", Loot_ElementName(element),
			                targ->classname, damage, targ->loot_status.burn_dps, targ->loot_status.melt_dps, targ->loot_status.shock_dmg);
	}

	damage = Unique_OnHit(targ, attacker, *gun, damage, mod); // Lancer, Bulwark, Tick Tock, Executioner

	return damage;
}

// Damage numbers: accumulate per frame per target, flushed by Loot_FlushDamageNumber.
static void Loot_FlushDamageNumber(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (cl->loot_dmg_accum <= 0 || !cl->loot_dmg_ent)
	{
		cl->loot_dmg_accum = 0;
		return;
	}

	edict_t *t = cl->loot_dmg_ent;
	const vec3_t pos = {(t->absmin[0] + t->absmax[0]) * 0.5f, (t->absmin[1] + t->absmax[1]) * 0.5f, t->absmax[2] + 12.f};
	cl->ps.stats[STAT_LOOT_DMG_X] = (int16_t)std::clamp((int32_t)pos[0], -32767, 32767);
	cl->ps.stats[STAT_LOOT_DMG_Y] = (int16_t)std::clamp((int32_t)pos[1], -32767, 32767);
	cl->ps.stats[STAT_LOOT_DMG_Z] = (int16_t)std::clamp((int32_t)pos[2], -32767, 32767);

	// q2rpg: 11-bit value; under 1000 sent exact, 1000+ sent as 1000 + hundreds (shown as 1.2k, up to ~104k)
	const int32_t dmg = cl->loot_dmg_accum;
	const int32_t dmg_code = dmg < 1000 ? dmg : std::min(1000 + dmg / 100, 2047);
	const int32_t seq = ++cl->loot_dmg_seq;
	const uint16_t packed = (uint16_t)(dmg_code | ((seq & 7) << 11) | ((cl->loot_dmg_kind & 3) << 14));
	cl->ps.stats[STAT_LOOT_DMG_0 + (seq % 3)] = (int16_t)packed;

	cl->loot_dmg_accum = 0;
	cl->loot_dmg_kind = 0;
	cl->loot_dmg_ent = nullptr;
}

void Loot_RecordDamage(edict_t *targ, edict_t *inflictor, edict_t *attacker, int32_t damage, const mod_t &mod)
{
	if (damage <= 0 || deathmatch->integer || !attacker || !attacker->client || targ == attacker || !(targ->svflags & SVF_MONSTER))
		return;

	gclient_t *cl = attacker->client;
	if (cl->loot_dmg_ent && cl->loot_dmg_ent != targ)
		Loot_FlushDamageNumber(attacker); // different monster this frame

	int32_t kind = 0;
	if (loot_last_hit_crit)
		kind = 1;
	else if (Loot_IsStatusMod(mod) && mod.id != MOD_LOOT_TURRET && mod.id != MOD_LOOT_SLAM)
		kind = 2;
	else if (targ->loot_mshield > 0)
		kind = 3;

	if (mod.id == MOD_LOOT_GRENADE)
		level.loot_stats.grenade_damage += damage;

	// challenge credit: remember how the living monster was last hit (status ticks keep the gun)
	if (targ->health > 0)
	{
		targ->loot_hit_crit = loot_last_hit_crit;
		if (mod.id == MOD_LOOT_TURRET || mod.id == MOD_LOOT_SLAM || mod.id == MOD_LOOT_GRENADE)
			targ->loot_hit_arch = 0;
		else if (!Loot_IsStatusMod(mod))
		{
			const loot_weapon_t *gun = Loot_HitGun(inflictor, attacker, mod); // the gun that fired, even after a swap
			targ->loot_hit_arch = gun ? gun->archetype + 1 : 0;
		}
		const loot_status_t &st = targ->loot_status;
		targ->loot_hit_elements = (st.burn_end > level.time ? 1 << LOOT_ELEM_FIRE : 0) |
		                          (st.shock_end > level.time ? 1 << LOOT_ELEM_SHOCK : 0) |
		                          (st.melt_end > level.time ? 1 << LOOT_ELEM_CORROSIVE : 0);
	}

	cl->loot_dmg_ent = targ;
	cl->loot_dmg_accum += damage;
	cl->loot_dmg_kind = std::max(cl->loot_dmg_kind, kind);
}

static void Loot_StatusDamage(edict_t *ent, edict_t *attacker, float amount, mod_id_t mod)
{
	if (attacker && attacker->client)
		Skill_OnShieldDot(attacker, amount); // Shield Battery
	T_Damage(ent, attacker, attacker, vec3_origin, ent->s.origin, vec3_origin, std::max(1, (int32_t)roundf(amount)), 0,
	         DAMAGE_NO_KNOCKBACK | DAMAGE_NO_ARMOR, mod);
}

// Ticks elemental effects every 0.5s. Called from G_RunEntity.
void Loot_RunStatus(edict_t *ent)
{
	loot_status_t &st = ent->loot_status;

	if (!ent->inuse || ent->deadflag || !(ent->svflags & SVF_MONSTER))
	{
		Loot_ClearStatus(ent);
		return;
	}

	// q2rpg: killed but death not processed yet: stop ticking, keep the status so on-kill skills (Chain Reaction, Spread) can read it
	if (ent->health <= 0)
		return;

	const bool burn = st.burn_end > level.time;
	const bool melt = st.melt_end > level.time;
	const bool shock = st.shock_end > level.time;

	if (!burn && !melt && !shock)
	{
		Loot_ClearStatus(ent);
		return;
	}

	if (level.time < st.next_tick)
		return;
	st.next_tick = level.time + 500_ms;

	edict_t *attacker = (st.attacker && st.attacker->inuse) ? st.attacker : world;

	if (burn)
	{
		Loot_StatusSparks(ent, LOOT_ELEM_FIRE);
		Loot_StatusDamage(ent, attacker, st.burn_dps * 0.5f, MOD_LOOT_BURN);
	}
	if (melt && ent->health > 0)
	{
		Loot_StatusSparks(ent, LOOT_ELEM_CORROSIVE);
		Loot_StatusDamage(ent, attacker, st.melt_dps * 0.5f, MOD_LOOT_MELT);
	}
	if (shock && ent->health > 0)
	{
		Loot_StatusSparks(ent, LOOT_ELEM_SHOCK);
		Loot_StatusDamage(ent, attacker, st.shock_dmg, MOD_LOOT_SHOCK);

		// stagger: force a pain reaction
		if (ent->health > 0 && ent->pain)
		{
			ent->pain_debounce_time = 0_ms;
			ent->pain(ent, attacker, 0, (int32_t)st.shock_dmg, MOD_LOOT_SHOCK);
		}
	}

	if (ent->health <= 0)
		Loot_ClearStatus(ent);
	else
		Loot_UpdateShell(ent);
}

//==========================================================================
// Shield absorb
//==========================================================================

static void Loot_ShieldNova(edict_t *player, const loot_shield_t &sh)
{
	const float dmg = sh.capacity * 0.6f;
	static const mod_id_t mods[LOOT_ELEM_COUNT] = {MOD_LOOT_BURN, MOD_LOOT_BURN, MOD_LOOT_SHOCK, MOD_LOOT_MELT};

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1_NP);
	gi.WritePosition(player->s.origin);
	gi.multicast(player->s.origin, MULTICAST_PHS, false);

	edict_t *e = nullptr;
	while ((e = findradius(e, player->s.origin, 256.f)) != nullptr)
	{
		if (!(e->svflags & SVF_MONSTER) || e->health <= 0 || e->deadflag || !e->takedamage)
			continue;

		vec3_t dir = e->s.origin - player->s.origin;
		dir.normalize();
		T_Damage(e, player, player, dir, e->s.origin, vec3_origin, (int32_t)dmg, 120, DAMAGE_RADIUS, mods[sh.element]);
		if (e->health > 0)
			Loot_ApplyStatus(e, player, sh.element, (int32_t)dmg);
	}
}

static bool Loot_ShieldBypass(const mod_t &mod)
{
	switch (mod.id)
	{
		case MOD_WATER:
		case MOD_SLIME:
		case MOD_LAVA:
		case MOD_CRUSH:
		case MOD_TELEFRAG:
		case MOD_FALLING:
		case MOD_SUICIDE:
		case MOD_EXIT:
		case MOD_TRIGGER_HURT:
			return true;
		default:
			return false;
	}
}

// Monster shields (guards). Shock deals double damage to shields.
static int32_t Loot_MonsterShieldAbsorb(edict_t *targ, edict_t *inflictor, edict_t *attacker, int32_t damage, damageflags_t dflags, const mod_t &mod)
{
	if ((dflags & DAMAGE_NO_PROTECTION) || Loot_ShieldBypass(mod) || targ->health <= 0)
		return damage;

	targ->loot_mshield_hit = level.time;
	if (targ->loot_mshield <= 0)
		return damage;

	// q2rpg: shock doubles vs shields; checks the gun that fired, not the one held now
	const loot_weapon_t *gun = Loot_HitGun(inflictor, attacker, mod);
	const bool shock = mod.id == MOD_LOOT_SHOCK || (gun && gun->element == LOOT_ELEM_SHOCK);
	const float mult = shock ? 2.0f : 1.0f;

	const float eff = damage * mult;
	const float absorbed = std::min(targ->loot_mshield, eff);
	targ->loot_mshield -= absorbed;
	damage = (int32_t)ceilf((eff - absorbed) / mult);

	targ->loot_mshield_flash = level.time + 150_ms;
	if (frandom() < 0.5f)
		Loot_StatusSparks(targ, LOOT_ELEM_SHOCK);

	if (targ->loot_mshield <= 0)
	{
		targ->loot_mshield = 0;
		gi.sound(targ, CHAN_AUTO, gi.soundindex("misc/mon_power2.wav"), 1, ATTN_NORM, 0);
	}

	return damage;
}

// Per-frame monster upkeep: shield recharge and elemental status.
void Loot_RunMonsterFrame(edict_t *ent)
{
	if (ent->loot_mshield_max > 0 && ent->health > 0 && !ent->deadflag && ent->loot_mshield < ent->loot_mshield_max)
	{
		const bool waited = ent->loot_mshield_hit > level.time || level.time - ent->loot_mshield_hit >= 3_sec;
		if (waited)
			ent->loot_mshield = std::min(ent->loot_mshield_max, ent->loot_mshield + ent->loot_mshield_max * 0.25f * FRAME_TIME_S.seconds());
	}

	if (ent->loot_status.active)
		Loot_RunStatus(ent);
}

int32_t Loot_ShieldAbsorb(edict_t *targ, edict_t *inflictor, edict_t *attacker, int32_t damage, damageflags_t dflags, const mod_t &mod)
{
	if (!damage || deathmatch->integer)
		return damage;

	if (targ->loot_mshield_max > 0 && (targ->svflags & SVF_MONSTER))
		return Loot_MonsterShieldAbsorb(targ, inflictor, attacker, damage, dflags, mod);

	if (!targ->client || (dflags & DAMAGE_NO_PROTECTION))
		return damage;

	// environmental damage goes straight through shields and skills
	if (Loot_ShieldBypass(mod))
		return damage;

	Skill_OnHurt(targ);
	// Bruiser
	damage = std::max(1, (int32_t)ceilf(damage * Skill_DamageTakenMult(targ)));

	gclient_t *cl = targ->client;
	client_persistant_t &pers = cl->pers;
	if (!pers.loot_shield.valid)
		return damage;

	cl->loot_shield_hit_time = level.time; // restarts the recharge delay

	if (pers.loot_shield_points <= 0)
		return damage;

	const float before = pers.loot_shield_points;
	const int32_t absorbed = std::min(damage, (int32_t)ceilf(pers.loot_shield_points));
	pers.loot_shield_points = std::max(0.f, pers.loot_shield_points - absorbed);
	damage -= absorbed;

	if (level.time >= cl->loot_shield_snd_time)
	{
		gi.sound(targ, CHAN_AUTO, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
		cl->loot_shield_snd_time = level.time + 200_ms;
	}

	// spikes: melee hits bounce back. Monster hitscan also passes the monster as inflictor,
	// so q2rpg: bullets, splash, rails and beams don't count
	const bool melee = inflictor == attacker && !(dflags & (DAMAGE_BULLET | DAMAGE_RADIUS)) && mod.id != MOD_RAILGUN &&
	                   mod.id != MOD_HEATBEAM && mod.id != MOD_BLASTOFF;
	if (pers.loot_shield.special == LOOT_SHIELD_SPIKE && attacker && attacker != targ && (attacker->svflags & SVF_MONSTER) && melee &&
	    attacker->health > 0 && (attacker->s.origin - targ->s.origin).length() < 160.f)
	{
		const int32_t spike = std::max(5, (int32_t)(pers.loot_shield.capacity * 0.3f));
		T_Damage(attacker, targ, targ, vec3_origin, attacker->s.origin, vec3_origin, spike, 0, DAMAGE_NO_KNOCKBACK, MOD_LOOT_SHOCK);
		Loot_StatusSparks(attacker, LOOT_ELEM_SHOCK);
	}

	// broke
	if (before > 0 && pers.loot_shield_points <= 0)
	{
		gi.sound(targ, CHAN_AUTO, gi.soundindex("misc/mon_power2.wav"), 1, ATTN_NORM, 0);
		if (pers.loot_shield.special == LOOT_SHIELD_NOVA)
			Loot_ShieldNova(targ, pers.loot_shield);
		Skill_OnShieldBreak(targ, pers.loot_shield.capacity * Loot_ShieldCapMult(targ)); // Retaliation
	}

	return damage;
}

//==========================================================================
// World items
//==========================================================================

bool Loot_IsLootItem(const edict_t *item)
{
	return item->loot.valid || item->loot_shield.valid || item->loot_grenade.valid;
}

void Loot_ApplyVisuals(edict_t *item)
{
	const loot_rarity_t rarity = item->loot_shield.valid ? item->loot_shield.rarity : item->loot_grenade.valid ? item->loot_grenade.rarity
	                                                                                                           : item->loot.rarity;
	// rarity shows as a beam so the shell doesn't hide the gun model
	item->s.renderfx &= ~(RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
	item->s.effects &= ~EF_COLOR_SHELL;
	LootFx_RarityBeam(item, rarity);
}

// Coop with 2+ players: every player gets their own monster drops and cash,
// seen and taken only by them.
bool Loot_PerPlayerDrops()
{
	if (!coop->integer)
		return false;
	int32_t players = 0;
	for (auto player : active_players())
		players++;
	return players > 1;
}

void Loot_InstanceFor(edict_t *ent, edict_t *owner)
{
	if (!owner || !owner->client)
		return;
	ent->svflags |= SVF_INSTANCED;
	for (uint32_t i = 0; i < game.maxclients; i++)
		ent->item_picked_up_by[i] = (g_edicts + 1 + i) != owner; // true = hidden from that player
}

bool Loot_HiddenFrom(const edict_t *ent, const edict_t *player)
{
	return (ent->svflags & SVF_INSTANCED) && player->client && ent->item_picked_up_by[player->s.number - 1];
}

// owner of the monster drops being spawned right now (nullptr = shared)
static edict_t *loot_drop_owner;

edict_t *Loot_SpawnDrop(edict_t *source, const loot_weapon_t &gun, bool from_player)
{
	edict_t *drop = Drop_Item(source, GetItemByIndex(gun.base));

	drop->loot = gun;
	drop->svflags &= ~SVF_INSTANCED;

	if (from_player)
		drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	else
	{
		// loot fountain
		drop->velocity = {crandom() * 150.f, crandom() * 150.f, 250.f + frandom(150.f)};
		Loot_InstanceFor(drop, loot_drop_owner);
	}

	Loot_ApplyVisuals(drop);
	gi.linkentity(drop);
	return drop;
}

edict_t *Loot_SpawnShieldDrop(edict_t *source, const loot_shield_t &shield, bool from_player)
{
	edict_t *drop = Drop_Item(source, GetItemByIndex(IT_ITEM_POWER_SCREEN));

	drop->loot_shield = shield;
	drop->svflags &= ~SVF_INSTANCED;

	if (from_player)
		drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	else
	{
		drop->velocity = {crandom() * 150.f, crandom() * 150.f, 250.f + frandom(150.f)};
		Loot_InstanceFor(drop, loot_drop_owner);
	}

	Loot_ApplyVisuals(drop);
	gi.linkentity(drop);
	return drop;
}

edict_t *Grenade_SpawnDrop(edict_t *source, const loot_grenade_t &grenade, bool from_player)
{
	edict_t *drop = Drop_Item(source, GetItemByIndex(IT_AMMO_GRENADES));

	drop->loot_grenade = grenade;
	drop->svflags &= ~SVF_INSTANCED;

	if (from_player)
		drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	else
	{
		drop->velocity = {crandom() * 150.f, crandom() * 150.f, 250.f + frandom(150.f)};
		Loot_InstanceFor(drop, loot_drop_owner);
	}

	Loot_ApplyVisuals(drop);
	gi.linkentity(drop);
	return drop;
}

//==========================================================================
// Boss loot tables
//==========================================================================

// Fixed drops per campaign boss: `guns` guns at `min_rarity`+; one of them is a
// `favored` archetype at `favored_min`+ that may become its unique. Bosses not
// listed fall back to the generic 1000+ health boss roll.
struct loot_boss_info_t
{
	const char *classname;
	int32_t guns;
	loot_rarity_t min_rarity;
	loot_rarity_t favored_min;
	loot_archetype_t favored[2];
	float unique_chance; // favored gun becomes legendary + its archetype's unique
	float shield_chance;
	loot_rarity_t shield_min;
};

static const loot_boss_info_t loot_bosses[] = {
    // mini-bosses
    {"monster_tank_commander", 2, LOOT_RARITY_UNCOMMON, LOOT_RARITY_RARE, {LOOT_ARCH_ROCKET, LOOT_ARCH_MINIGUN}, 0.10f, 0.35f, LOOT_RARITY_UNCOMMON},
    {"monster_arachnid", 2, LOOT_RARITY_UNCOMMON, LOOT_RARITY_RARE, {LOOT_ARCH_RAIL, LOOT_ARCH_PLASMA}, 0.10f, 0.35f, LOOT_RARITY_UNCOMMON},
    // unit bosses
    {"monster_supertank", 3, LOOT_RARITY_RARE, LOOT_RARITY_EPIC, {LOOT_ARCH_MINIGUN, LOOT_ARCH_ROCKET}, 0.25f, 1.f, LOOT_RARITY_RARE},
    {"monster_boss5", 3, LOOT_RARITY_RARE, LOOT_RARITY_EPIC, {LOOT_ARCH_MINIGUN, LOOT_ARCH_ROCKET}, 0.25f, 1.f, LOOT_RARITY_RARE},
    {"monster_boss2", 3, LOOT_RARITY_RARE, LOOT_RARITY_EPIC, {LOOT_ARCH_SMG, LOOT_ARCH_ROCKET}, 0.25f, 1.f, LOOT_RARITY_RARE},
    {"monster_guardian", 3, LOOT_RARITY_RARE, LOOT_RARITY_EPIC, {LOOT_ARCH_PLASMA, LOOT_ARCH_LAUNCHER}, 0.25f, 1.f, LOOT_RARITY_RARE},
    {"monster_carrier", 3, LOOT_RARITY_RARE, LOOT_RARITY_EPIC, {LOOT_ARCH_RAIL, LOOT_ARCH_LAUNCHER}, 0.25f, 1.f, LOOT_RARITY_RARE},
    {"monster_widow", 3, LOOT_RARITY_RARE, LOOT_RARITY_EPIC, {LOOT_ARCH_PLASMA, LOOT_ARCH_RAIL}, 0.25f, 1.f, LOOT_RARITY_RARE},
    // finals
    {"monster_widow2", 4, LOOT_RARITY_EPIC, LOOT_RARITY_LEGENDARY, {LOOT_ARCH_PLASMA, LOOT_ARCH_BFG}, 0.50f, 1.f, LOOT_RARITY_EPIC},
    {"monster_jorg", 4, LOOT_RARITY_EPIC, LOOT_RARITY_LEGENDARY, {LOOT_ARCH_MINIGUN, LOOT_ARCH_BFG}, 0.50f, 1.f, LOOT_RARITY_EPIC},
    {"monster_makron", 4, LOOT_RARITY_EPIC, LOOT_RARITY_LEGENDARY, {LOOT_ARCH_BFG, LOOT_ARCH_PLASMA}, 0.50f, 1.f, LOOT_RARITY_EPIC},
};

static const loot_boss_info_t *Loot_BossInfo(const edict_t *monster)
{
	if (!monster->classname)
		return nullptr;
	for (const auto &boss : loot_bosses)
		if (!Q_strcasecmp(monster->classname, boss.classname))
			return &boss;
	return nullptr;
}

// Spawn one boss gun and count it in loot_stats.
static void Loot_BossGun(edict_t *monster, loot_weapon_t gun)
{
	Loot_SetLevel(gun, std::max(1, monster->loot_mlevel));
	level.loot_stats.gun_drops[gun.rarity]++;
	Loot_SpawnDrop(monster, gun, false);
}

bool Loot_IsBoss(const edict_t *monster)
{
	return Loot_BossInfo(monster) != nullptr;
}

static void Loot_BossDrop(edict_t *monster, const loot_boss_info_t &boss, float luck)
{
	// signature gun: a favored archetype, maybe its unique
	{
		const loot_archetype_t arch = boss.favored[irandom(2)];
		const bool unique = frandom() < boss.unique_chance;
		loot_rarity_t rarity = unique ? LOOT_RARITY_LEGENDARY : std::max({Loot_RollRarity(luck), boss.favored_min, loot_archs[arch].min_rarity});
		if (game.loot_ngplus > 0)
			rarity = std::max(rarity, LOOT_RARITY_LEGENDARY); // NG+: signature is legendary or better
		loot_weapon_t gun = Loot_Roll(arch, rarity, (uint32_t)mt_rand());
		if (unique && !gun.unique)
			Unique_Make(gun);
		Loot_BossGun(monster, gun);
	}

	// the rest: any archetype at the boss's rarity floor or better
	for (int32_t i = 1; i < boss.guns; i++)
	{
		loot_weapon_t gun = Loot_RollRandom(luck);
		if (gun.rarity < boss.min_rarity)
			gun = Loot_Roll((loot_archetype_t)gun.archetype, std::max(boss.min_rarity, loot_archs[gun.archetype].min_rarity), (uint32_t)mt_rand());
		Loot_BossGun(monster, gun);
	}

	// NG+: one extra legendary (20% Mythic), any archetype
	if (game.loot_ngplus > 0)
	{
		const loot_rarity_t rarity = frandom() < 0.2f ? LOOT_RARITY_MYTHIC : LOOT_RARITY_LEGENDARY;
		Loot_BossGun(monster, Loot_Roll((loot_archetype_t)irandom(LOOT_ARCH_COUNT), rarity, (uint32_t)mt_rand()));
	}

	if (Loot_ShieldsAvailable() && frandom() < boss.shield_chance)
	{
		const loot_shield_t shield = Loot_RollShield(std::max(Loot_RollRarity(luck), boss.shield_min), std::max(1, monster->loot_mlevel), (uint32_t)mt_rand());
		level.loot_stats.shield_drops[shield.rarity]++;
		Loot_SpawnShieldDrop(monster, shield, false);
	}
}

static void Loot_MonsterDropRoll(edict_t *monster)
{
	if (deathmatch->integer || !g_loot_droprate || g_loot_droprate->value <= 0)
		return;

	const float hp = Loot_MonsterBaseHealth(monster);
	const bool boss = hp >= 1000;
	float luck = hp / 400.f + g_loot_rarity_bonus->value + Loot_ZoneLuck() + NGPlus_LuckBonus();
	if (monster->loot_elite)
		luck += 1.5f;
	int32_t count;

	if (const loot_boss_info_t *boss_info = Loot_BossInfo(monster))
	{
		Loot_BossDrop(monster, *boss_info, luck + 2.f);
		return;
	}

	if (boss)
	{
		count = 2 + irandom(3);
		luck += 2.f;
	}
	else
	{
		float chance = g_loot_droprate->value * std::clamp(hp / 100.f, 0.5f, 4.f);

		// reinforcements spawned by other monsters drop less
		if ((monster->monsterinfo.aiflags & AI_SPAWNED_MASK) || monster->loot_death_spawned)
			chance *= 0.5f;

		chance *= NGPlus_DropMult(); // Bounty

		count = (int32_t)chance;
		if (frandom() < chance - count)
			count++;

		if (monster->loot_elite)
			count = std::max(count, 1);
	}

	for (int32_t i = 0; i < count; i++)
	{
		// some drops are shields
		if (Loot_ShieldsAvailable() && frandom() < 0.15f * NGPlus_ShieldDropMult()) // Fortified
		{
			const loot_shield_t shield = Loot_RollShield(Loot_RollRarity(luck), std::max(1, monster->loot_mlevel), (uint32_t)mt_rand());
			level.loot_stats.shield_drops[shield.rarity]++;
			Loot_SpawnShieldDrop(monster, shield, false);
			continue;
		}

		// and some are grenade mods
		if (frandom() < 0.12f)
		{
			Grenade_SpawnDrop(monster, Grenade_Roll(Loot_RollRarity(luck), std::max(1, monster->loot_mlevel), (uint32_t)mt_rand()), false);
			level.loot_stats.grenade_mod_drops++;
			continue;
		}

		loot_weapon_t gun = Loot_RollRandom(luck);
		Loot_SetLevel(gun, std::max(1, monster->loot_mlevel));
		level.loot_stats.gun_drops[gun.rarity]++;
		Loot_SpawnDrop(monster, gun, false);
	}

	// Alpha Omega: a guaranteed Legendary (25% Mythic) on top
	if (monster->loot_omega)
	{
		const loot_rarity_t rarity = frandom() < 0.25f ? LOOT_RARITY_MYTHIC : LOOT_RARITY_LEGENDARY;
		loot_weapon_t gun = Loot_Roll((loot_archetype_t)irandom(LOOT_ARCH_COUNT), rarity, (uint32_t)mt_rand());
		Loot_SetLevel(gun, std::max(1, monster->loot_mlevel));
		level.loot_stats.gun_drops[gun.rarity]++;
		Loot_SpawnDrop(monster, gun, false);
	}
}

// Coop rolls the whole drop once per player (instanced); otherwise one shared roll.
void Loot_MonsterDrop(edict_t *monster)
{
	if (!Loot_PerPlayerDrops())
	{
		Loot_MonsterDropRoll(monster);
		return;
	}
	for (auto player : active_players())
	{
		loot_drop_owner = player;
		Loot_MonsterDropRoll(monster);
	}
	loot_drop_owner = nullptr;
}

// Map-placed stock weapons become rolled guns (common-biased).
void Loot_OnItemSpawned(edict_t *ent)
{
	if (deathmatch->integer || !ent->item || ent->loot.valid)
		return;

	const loot_archetype_t arch = Loot_ArchetypeForItem(ent->item->id);
	if (arch == LOOT_ARCH_COUNT)
		return;

	loot_rarity_t rarity = std::max(Loot_RollRarity(0.f), loot_archs[arch].min_rarity);
	ent->loot = Loot_Roll(arch, rarity, (uint32_t)mt_rand());
	Loot_SetLevel(ent->loot, Loot_ZoneLevel());
	Loot_ApplyVisuals(ent);
}

// Called from Touch_Item. Returns true when the item is loot (never auto-picked up).
bool Loot_Touch(edict_t *item, edict_t *other)
{
	if (deathmatch->integer || !item->item)
		return false;

	// another coop player's drop: invisible and untouchable for this one
	if (Loot_HiddenFrom(item, other) && Loot_IsLootItem(item))
		return true;

	if (!Loot_IsLootItem(item))
	{
		if (!Loot_IsBaseWeapon(item->item->id))
			return false;
		Loot_OnItemSpawned(item);
	}

	if (other->client && other->health > 0)
	{
		other->client->loot_touch_ent = item;
		other->client->loot_touch_time = level.time;
	}

	return true;
}

edict_t *Loot_TouchTarget(edict_t *ent)
{
	gclient_t *cl = ent->client;
	edict_t *t = cl->loot_touch_ent;

	// touch_time > level.time means it is left over from a previous map
	if (!t || !t->inuse || !Loot_IsLootItem(t) || cl->loot_touch_time > level.time || level.time - cl->loot_touch_time > 200_ms)
		return nullptr;

	return t;
}

//==========================================================================
// Levels and XP
//==========================================================================

// Highest player level; drives monster and map-loot levels.
// Ignores pers.connected: SpawnEntities clears it before monsters spawn
// on a map change, while pers (and the level) carries over.
int32_t Loot_ZoneLevel()
{
	int32_t level = 1;

	for (uint32_t i = 0; i < game.maxclients; i++)
		level = std::max(level, game.clients[i].pers.loot_level);

	return level;
}

int32_t Loot_LevelCap()
{
	return LOOT_MAX_LEVEL + (game.loot_ngplus > 0 ? LOOT_NGPLUS_LEVEL_CAP_BONUS : 0);
}

// Rarity luck bonus that grows as the game goes on, so later maps roll better loot.
constexpr float LOOT_ZONE_LUCK_PER_LEVEL = 0.02f;

float Loot_ZoneLuck()
{
	return LOOT_ZONE_LUCK_PER_LEVEL * (Loot_ZoneLevel() - 1) + LOOT_NGPLUS_LUCK * game.loot_ngplus;
}

int32_t Loot_XPToNext(int32_t level)
{
	return 100 + (int32_t)(100.f * powf((float)std::max(1, level), 1.6f));
}

constexpr float LOOT_ELITE_SCALE = 1.3f;
constexpr float LOOT_OMEGA_CHANCE = 0.10f; // NG+: share of Alphas that spawn as Alpha Omegas
constexpr float LOOT_ELITE_SPEED = 1.35f;

gtime_t Loot_EliteFrameTime(const edict_t *monster)
{
	float speed = LOOT_ELITE_SPEED;
	if (monster->loot_alpha_traits & LOOT_TRAIT_SWIFT)
		speed *= 1.25f;
	return gtime_t::from_ms((int64_t)(100.f / speed));
}

// Would the monster still fit where it stands at elite size?
// Scaling the box also pushes its bottom down (mins.z < 0); lifting the origin by
// this keeps an elite's feet on the floor instead of sunk into it.
static float Loot_EliteLift(const edict_t *monster)
{
	return std::max(0.f, -monster->mins[2] * (LOOT_ELITE_SCALE - 1.f));
}

// Can the monster grow to elite size where it stands? On success `out_origin` is
// where the bigger box rests. Mappers often sink monsters a little into the floor
// (stock droptofloor fixes that later), so ground monsters are tested by dropping
// the bigger box from a step above their spot instead of in place.
static bool Loot_EliteFits(edict_t *monster, vec3_t &out_origin)
{
	const vec3_t mins = monster->mins * LOOT_ELITE_SCALE;
	const vec3_t maxs = monster->maxs * LOOT_ELITE_SCALE;
	const vec3_t lifted = monster->s.origin + vec3_t{0, 0, Loot_EliteLift(monster)};
	const bool flies = monster->flags & (FL_FLY | FL_SWIM);
	const vec3_t start = flies ? lifted : lifted + vec3_t{0, 0, STEPSIZE};

	const trace_t tr = gi.trace(start, mins, maxs, flies ? start : lifted - vec3_t{0, 0, STEPSIZE}, monster, MASK_MONSTERSOLID);
	if (tr.startsolid || tr.allsolid)
	{
		if (g_loot_debug->integer)
			gi.Com_PrintFmt("q2rpg: elite won't fit: {} at {} (blocked by {})\n", monster->classname, monster->s.origin, tr.ent && tr.ent != world ? tr.ent->classname : "world");
		return false;
	}

	out_origin = flies ? start : tr.endpos;
	return true;
}

// Health multiplier Loot_ScaleMonster applied: health sponge cvar, +12% per
// monster level, x2.5 for elites.
static float Loot_MonsterHealthScale(const edict_t *monster)
{
	float scale = std::max(0.1f, g_loot_monster_health->value) * (1.0f + 0.12f * (std::max(1, monster->loot_mlevel) - 1)) * NGPlus_MonsterHealthMult();
	if (monster->loot_elite)
		scale *= monster->loot_omega ? 4.f : 2.5f;
	return scale;
}

// Stock max health before level/elite/sponge scaling. Drop luck, drop count,
// boss checks and cash use this so they don't creep up as monsters level.
float Loot_MonsterBaseHealth(const edict_t *monster)
{
	if (!monster->loot_mlevel)
		return (float)std::max(1, monster->max_health); // never scaled (deathmatch)
	return std::max(1.f, monster->max_health / Loot_MonsterHealthScale(monster));
}

//==========================================================================
// Death spawns: a dying monster may bring 1-3 more of its kind
//==========================================================================

// Spawner entity left at the corpse: `target` = monster classname, `count` = how many,
// `activator` = the player who made the kill (the new monsters hunt them).
THINK(loot_death_spawner_think)(edict_t *self)->void
{
	vec3_t forward;
	int32_t spawned = 0;

	// try 8 spots in a ring around the corpse; skip any where the new monster would be stuck
	for (int32_t spot_index = 0; spot_index < 8 && spawned < self->count; spot_index++)
	{
		const float yaw = self->s.angles[YAW] + spot_index * 45.f;
		AngleVectors({0, yaw, 0}, forward, nullptr, nullptr);
		const vec3_t spot = self->s.origin + forward * self->wait + vec3_t{0, 0, 8};

		edict_t *monster = CreateMonster(spot, {0, yaw, 0}, self->target);
		if (!monster || !monster->inuse)
			break; // unknown classname
		const trace_t tr = gi.trace(monster->s.origin, monster->mins, monster->maxs, monster->s.origin, monster, MASK_MONSTERSOLID);
		if (tr.startsolid || tr.allsolid)
		{
			G_FreeEdict(monster);
			continue;
		}

		monster->loot_death_spawned = true;
		spawned++;

		// finish monster_start now so it can take an enemy this frame (same as medic reinforcements)
		if (monster->think)
		{
			monster->nextthink = level.time;
			monster->think(monster);
		}
		if (self->activator && self->activator->inuse && self->activator->health > 0)
		{
			monster->enemy = self->activator;
			FoundTarget(monster);
		}
	}

	G_FreeEdict(self);
}

// Death spawns ramp with the party level (roughly Unit 4 = L20, Unit 8 = L28):
// base 10% -> 15% at L20, 20% at L25, 25% at L30. Scales the cvar, so
// g_loot_death_spawn still tunes the whole curve.
static float Loot_DeathSpawnLevelMult()
{
	const int32_t level = Loot_ZoneLevel();
	if (level >= 30)
		return 2.5f;
	if (level >= 25)
		return 2.0f;
	if (level >= 20)
		return 1.5f;
	return 1.0f;
}

void Loot_DeathSpawn(edict_t *monster)
{
	if (deathmatch->integer || !g_loot_death_spawn || g_loot_death_spawn->value <= 0)
		return;
	if (monster->loot_death_spawned || (monster->monsterinfo.aiflags & (AI_SPAWNED_MASK | AI_GOOD_GUY)))
		return;
	if (!monster->classname || Q_strncasecmp(monster->classname, "monster_", 8) || !strcmp(monster->classname, "monster_turret"))
		return;
	if (Loot_MonsterBaseHealth(monster) >= 1000) // bosses
		return;
	if (frandom() >= g_loot_death_spawn->value * Loot_DeathSpawnLevelMult() * NGPlus_DeathSpawnMult())
		return;

	// 1 (60%), 2 (30%) or 3 (10%)
	const float roll = frandom();
	const int32_t count = roll < 0.6f ? 1 : roll < 0.9f ? 2
	                                                    : 3;

	Loot_SpawnAdds(monster, count, monster->monsterinfo.damage_attacker);
}

// Spawns count more of monster's kind in a ring around it after 1s (death spawns, Summoner).
void Loot_SpawnAdds(edict_t *monster, int32_t count, edict_t *enemy)
{
	edict_t *spawner = G_Spawn();
	spawner->classname = "loot_death_spawner";
	spawner->target = monster->classname;
	spawner->count = count;
	spawner->activator = enemy;
	spawner->s.origin = monster->s.origin;
	// ring clears the source's box plus an add of the same size (a living Summoner is still solid)
	const float half_width = std::max({monster->maxs[0], monster->maxs[1], -monster->mins[0], -monster->mins[1]});
	spawner->wait = std::max(64.f, half_width * 2.f + 8.f);
	spawner->s.angles[YAW] = frandom(360.f);
	spawner->think = loot_death_spawner_think;
	spawner->nextthink = level.time + 1_sec;

	SpawnGrow_Spawn(monster->s.origin + vec3_t{0, 0, 16}, 16.f, 64.f);
}

// Alpha chance ramps with the party level: 12% default -> 20% from L25.
// Scales the cvar, so g_loot_elite_chance still tunes the curve.
static float Loot_EliteLevelMult()
{
	return Loot_ZoneLevel() >= 25 ? 0.20f / 0.12f : 1.0f;
}

static bool loot_force_omega; // set around CreateMonster by loot_spawnmonster

// Called from monster_start before max_health is set; the stock code
// right after scales the bounding box by s.scale.
static int32_t Loot_ClampMonsterLevel(int32_t level)
{
	const int32_t ngplus_levels = LOOT_NGPLUS_MONSTER_LEVELS * game.loot_ngplus;
	return std::clamp(level, 1, Loot_LevelCap() + 5 + ngplus_levels);
}

// Makes the monster an Alpha (or Omega) at elite_origin with the elite scale.
// Returns the levels it gains.
static int32_t Loot_MakeElite(edict_t *monster, const vec3_t &elite_origin, bool omega)
{
	monster->loot_elite = true;
	monster->loot_omega = omega;
	monster->s.origin = elite_origin;
	monster->s.scale = LOOT_ELITE_SCALE;

	static const loot_elite_fire_t alts[] = {LOOT_EFIRE_SHOTGUN, LOOT_EFIRE_BLASTER, LOOT_EFIRE_GRENADE, LOOT_EFIRE_ROCKET, LOOT_EFIRE_RAIL};
	monster->loot_elite_fire = alts[irandom(q_countof(alts))];
	NGPlus_RollAlphaTraits(monster);
	return omega ? 3 : 1;
}

// Every monster carries a rechargeable shield sized from its (scaled) health;
// light and shotgun guards get a bigger one, Warded Alphas double it, Fortified maps double it.
static void Loot_SetupMonsterShield(edict_t *monster)
{
	float shield_fraction = std::max(0.f, g_loot_monster_shield->value);
	if (monster->classname && (!strcmp(monster->classname, "monster_soldier_light") || !strcmp(monster->classname, "monster_soldier")))
		shield_fraction = std::max(shield_fraction, g_loot_guard_shield->value);
	monster->loot_mshield_max = monster->health * shield_fraction;

	// Warded Alphas: double shield (half their health if they had none)
	if (monster->loot_alpha_traits & LOOT_TRAIT_WARDED)
		monster->loot_mshield_max = monster->loot_mshield_max > 0 ? monster->loot_mshield_max * 2.f : monster->health * 0.5f;

	monster->loot_mshield_max *= NGPlus_MonsterShieldMult();
	monster->loot_mshield = monster->loot_mshield_max;
}

void Loot_ScaleMonster(edict_t *monster)
{
	if (deathmatch->integer)
		return;

	const int32_t ngplus_levels = LOOT_NGPLUS_MONSTER_LEVELS * game.loot_ngplus;
	int32_t level = Loot_ZoneLevel() + irandom(0, 4) + ngplus_levels; // +0..+3 over the highest player, more in NG+
	if (skill->integer >= 2)
		level++;

	const bool boss = monster->health >= 1000;
	if (boss)
		level += 2;

	// badass variant
	vec3_t elite_origin;
	const bool force_omega = loot_force_omega; // loot_spawnmonster ... omega
	if (!boss && !monster->s.scale && (force_omega || frandom() < g_loot_elite_chance->value * Loot_EliteLevelMult() * NGPlus_EliteChanceMult()) &&
	    Loot_EliteFits(monster, elite_origin))
	{
		// Alpha Omega: NG+ only, 10% of Alphas; +2 more levels, 3 traits, x4 health, guaranteed Legendary
		const bool omega = force_omega || (game.loot_ngplus > 0 && frandom() < LOOT_OMEGA_CHANCE);
		level += Loot_MakeElite(monster, elite_origin, omega); // before monster_start scales mins/maxs
	}

	monster->loot_mlevel = Loot_ClampMonsterLevel(level);
	monster->health = std::max(1, (int32_t)(monster->health * Loot_MonsterHealthScale(monster)));
	Loot_SetupMonsterShield(monster);
}

// Bounty: turn a monster that already ran monster_start into an Alpha Omega,
// rescaling what Loot_ScaleMonster and monster_start already applied.
bool Loot_PromoteToOmega(edict_t *monster)
{
	vec3_t elite_origin;
	if (monster->loot_elite || monster->s.scale || !Loot_EliteFits(monster, elite_origin))
		return false;

	const float old_health_scale = Loot_MonsterHealthScale(monster);
	monster->loot_mlevel = Loot_ClampMonsterLevel(monster->loot_mlevel + Loot_MakeElite(monster, elite_origin, true));

	const float health_ratio = Loot_MonsterHealthScale(monster) / old_health_scale;
	monster->health = std::max(1, (int32_t)(monster->health * health_ratio));
	monster->max_health = monster->health;
	Loot_SetupMonsterShield(monster);

	// monster_start's box scaling, which already ran at scale 1
	monster->monsterinfo.scale *= LOOT_ELITE_SCALE;
	monster->mins *= LOOT_ELITE_SCALE;
	monster->maxs *= LOOT_ELITE_SCALE;
	gi.linkentity(monster);
	return true;
}

// Alpha (elite) alternate attack. Returns true when the original attack is replaced
// (fired or suppressed by cooldown).
bool Loot_EliteFire(edict_t *self, const vec3_t &start, const vec3_t &dir, monster_muzzleflash_id_t flashtype, loot_elite_fire_t original)
{
	if (deathmatch->integer || !self->loot_elite || !self->loot_elite_fire || self->loot_elite_fire == original)
		return false;

	// between alternate shots the original rapid fire is suppressed
	if (level.time < self->loot_elite_next)
		return true;

	switch ((loot_elite_fire_t)self->loot_elite_fire)
	{
		case LOOT_EFIRE_SHOTGUN:
			fire_shotgun(self, start, dir, 4, 2, 600, 400, 10, MOD_UNKNOWN);
			self->loot_elite_next = level.time + 900_ms;
			break;
		case LOOT_EFIRE_BLASTER:
			fire_blaster(self, start, dir, 10, 1000, EF_HYPERBLASTER, MOD_BLASTER);
			self->loot_elite_next = level.time + 150_ms;
			break;
		case LOOT_EFIRE_GRENADE:
			fire_grenade(self, start, dir, 40, 600, 2.5_sec, 80.f, crandom_open() * 10.0f, 200.f + crandom_open() * 10.0f, true);
			self->loot_elite_next = level.time + 1_sec;
			break;
		case LOOT_EFIRE_ROCKET:
			fire_rocket(self, start, dir, 40, 650, 60.f, 40);
			self->loot_elite_next = level.time + 1200_ms;
			break;
		case LOOT_EFIRE_RAIL:
			fire_rail(self, start, dir, 30, 50);
			self->loot_elite_next = level.time + 2_sec;
			break;
		default:
			return false;
	}

	monster_muzzleflash(self, start, flashtype);
	return true;
}

void Loot_GiveXP(edict_t *ent, int32_t xp)
{
	auto &pers = ent->client->pers;

	if (pers.loot_level <= 0)
		pers.loot_level = 1;
	if (pers.loot_level >= Loot_LevelCap())
		return;

	pers.loot_xp += std::max(0, xp);

	bool leveled = false;
	while (pers.loot_level < Loot_LevelCap() && pers.loot_xp >= Loot_XPToNext(pers.loot_level))
	{
		pers.loot_xp -= Loot_XPToNext(pers.loot_level);
		pers.loot_level++;
		pers.max_health += 5;
		leveled = true;
	}

	if (pers.loot_level >= Loot_LevelCap())
		pers.loot_xp = 0;

	if (leveled)
	{
		ent->max_health = pers.max_health;
		if (ent->health > 0)
			ent->health = std::max(ent->health, ent->max_health);

		gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/secret.wav"), 1, ATTN_NORM, 0);
		gi.Center_Print(ent, G_Fmt("LEVEL UP!\nLevel {}  (+5 max health)\n[K] skill point", pers.loot_level).data());
		Loot_LogEvent(ent, G_Fmt("LEVEL UP {}", pers.loot_level).data());
	}
}

// Shared XP for every player when a player-caused kill happens.
void Loot_AwardXP(edict_t *monster, edict_t *attacker)
{
	if (deathmatch->integer || !attacker || !attacker->client)
		return;

	const int32_t mlevel = std::max(1, monster->loot_mlevel);
	// stock health, +8% per monster level (not the 12% health scaling, so XP doesn't snowball)
	float xp_scale = 1.0f + 0.08f * (mlevel - 1);
	if (monster->loot_elite)
		xp_scale *= monster->loot_omega ? 4.f : 2.5f;
	xp_scale *= 1 + game.loot_ngplus; // NG+: early maps' weak monsters still level a high character
	xp_scale *= NGPlus_XPMult();      // Bloodthirsty
	const int32_t xp = 5 + (int32_t)(Loot_MonsterBaseHealth(monster) * 0.4f * xp_scale);

	loot_stats_t &stats = level.loot_stats;
	stats.kills++;
	if (monster->loot_elite)
		stats.elite_kills++;
	if (Loot_MonsterBaseHealth(monster) >= 1000)
		stats.boss_kills++;
	if (monster->monsterinfo.damage_mod.id == MOD_LOOT_GRENADE && monster->loot_hit_grenade > 0 && monster->loot_hit_grenade <= LOOT_GRENADE_TYPE_COUNT)
	{
		stats.grenade_kills[monster->loot_hit_grenade - 1]++;
		Skill_OnGrenadeKill(attacker); // Gunner: turret cooldown
	}
	stats.xp += xp;

	for (auto player : active_players())
	{
		// far under-level kills are worth half
		const bool low = mlevel <= player->client->pers.loot_level - 4;
		const int32_t player_xp = (int32_t)((low ? xp / 2 : xp) * Challenge_BadgeXPMult(player)); // Omega Hunter badge
		Loot_GiveXP(player, player_xp);
	}
}

//==========================================================================
// HUD
//==========================================================================

// "^g" / "^r" color switch for a stat vs the equipped item ("" when equal or nothing to compare).
// Drawn by the "loot_str" layout token (cg_loot.cpp).
static const char *Loot_CompareColor(float value, float other, bool have_other, bool lower_better)
{
	const float diff = value - other;
	if (!have_other || fabsf(diff) <= 0.005f)
		return "";
	return (lower_better ? diff < 0 : diff > 0) ? "^g" : "^r";
}

std::string Loot_StatLine(const char *label, float value, float other, bool have_other, bool lower_better, const char *fmt_kind)
{
	std::string line = G_Fmt("{:<10}{}", label, Loot_CompareColor(value, other, have_other, lower_better)).data();

	if (!strcmp(fmt_kind, "pct"))
		line += G_Fmt("{:>5.0f}%", value * 100.f).data();
	else if (!strcmp(fmt_kind, "sec"))
		line += G_Fmt("{:>5.1f}s", value).data();
	else if (!strcmp(fmt_kind, "x"))
		line += G_Fmt("{:>5.2f}x", value).data();
	else
		line += G_Fmt("{:>5.0f} ", value).data();

	if (have_other)
	{
		const float diff = value - other;
		const bool better = lower_better ? diff < 0 : diff > 0;

		if (fabsf(diff) > 0.005f)
		{
			if (!strcmp(fmt_kind, "pct"))
				line += G_Fmt("  {}{:.0f}", better ? "+" : "-", fabsf(diff) * 100.f).data();
			else if (!strcmp(fmt_kind, "sec") || !strcmp(fmt_kind, "x"))
				line += G_Fmt("  {}{:.1f}", better ? "+" : "-", fabsf(diff)).data();
			else
				line += G_Fmt("  {}{:.0f}", better ? "+" : "-", fabsf(diff)).data();
		}
	}

	return line;
}

// Single-player only: an open menu (vendor, skills, challenges, loadout) freezes
// the world. G_RunFrame skips the frame and ClientThink freezes the player.
bool Loot_MenuPaused()
{
	if (deathmatch->integer || coop->integer || level.intermissiontime)
		return false;
	for (auto player : active_players())
	{
		const gclient_t *cl = player->client;
		if (player->health > 0 && (cl->loot_shop_open || cl->loot_showskills || cl->loot_showchallenges || cl->loot_showinv))
			return true;
	}
	return false;
}

void ClientEndServerFrames(); // g_main.cpp

// Called instead of a world frame while paused: level.time is frozen, so force
// the open screen to resend each frame to show purchases and page changes.
void Loot_PausedFrame()
{
	for (auto player : active_players())
	{
		gclient_t *cl = player->client;
		cl->loot_shop_time = cl->loot_skills_time = cl->loot_challenges_time = cl->loot_inv_time = 0_ms;
	}
	ClientEndServerFrames();
}

void Loot_SendLayout(edict_t *ent, const std::string &s)
{
	gi.WriteByte(svc_layout);
	gi.WriteString(s.c_str());
	gi.unicast(ent, true);
}

static void Loot_SendShieldCard(edict_t *ent, const loot_shield_t &sh)
{
	const loot_shield_t &o = ent->client->pers.loot_shield;
	const bool cmp = o.valid;

	std::string s;
	int32_t y = 64;

	s += G_Fmt("xv 24 yv {} string2 \"{}\" ", y, sh.name).data();
	y += 10;
	s += G_Fmt("xv 24 yv {} string \"Lv {} {} {} Shield\" ", y, sh.level, Loot_RarityName(sh.rarity), loot_makers[sh.maker].short_name).data();
	y += 14;

	auto add = [&](const std::string &line)
	{
		s += G_Fmt("xv 24 yv {} loot_str \"{}\" ", y, line).data();
		y += 10;
	};

	add(Loot_StatLine("Capacity", sh.capacity, o.capacity, cmp, false, "num"));
	add(Loot_StatLine("Delay", sh.delay, o.delay, cmp, true, "sec"));
	add(Loot_StatLine("Recharge", sh.rate, o.rate, cmp, false, "num"));
	if (sh.special)
		add(Loot_ShieldSpecialText(sh));

	y += 4;
	if (cmp)
	{
		s += G_Fmt("xv 24 yv {} string \"vs {}\" ", y, o.name).data();
		y += 10;
	}
	s += G_Fmt("xv 24 yv {} string2 \"[E] take  [I] loadout\" ", y).data();

	Loot_SendLayout(ent, s);
}

// Floor card: only the stats that decide a swap, compared to the equipped gun.
static void Loot_SendCard(edict_t *ent, const loot_weapon_t &gun)
{
	const loot_weapon_t *cur = Loot_Active(ent);
	const bool cmp = cur != nullptr;
	static const loot_weapon_t none{};
	const loot_weapon_t &o = cmp ? *cur : none;

	std::string s;
	int32_t y = 64;

	s += G_Fmt("xv 24 yv {} string2 \"{}\" ", y, gun.name).data();
	y += 10;
	s += G_Fmt("xv 24 yv {} string \"Lv {} {} {}\" ", y, Loot_GunLevel(gun), Loot_RarityName(gun.rarity), Loot_MakerName(gun.maker)).data();
	y += 14;

	auto add = [&](const std::string &line)
	{
		s += G_Fmt("xv 24 yv {} loot_str \"{}\" ", y, line).data();
		y += 10;
	};

	add(Loot_StatLine("Damage", gun.dmg_mult, o.dmg_mult, cmp, false, "pct"));
	add(Loot_StatLine("Fire Rate", gun.rate_mult, o.rate_mult, cmp, false, "pct"));
	add(Loot_StatLine("Accuracy", 1.0f / gun.spread_mult, cmp ? 1.0f / o.spread_mult : 0, cmp, false, "pct"));
	if (gun.mag_size > 0)
		add(Loot_StatLine("Magazine", (float)gun.mag_size, (float)o.mag_size, cmp && o.mag_size > 0, false, "num"));
	if (gun.pellets_bonus)
		add(G_Fmt("{:<10}{:>5}", "Pellets", G_Fmt("+{}", gun.pellets_bonus).data()).data());
	if (gun.element)
		add(G_Fmt("{:<10}{} {:.0f}%", "Element", Loot_ElementName(gun.element), Loot_ElemChance(gun) * 100.f).data());
	if (gun.burst)
		add(G_Fmt("{:<10}{}-round burst", "Trigger", gun.burst).data());
	if (gun.unique)
		add(G_Fmt("UNIQUE: {}", Unique_Desc(gun.unique)).data());

	y += 4;
	if (cmp)
	{
		s += G_Fmt("xv 24 yv {} string \"vs {}\" ", y, o.name).data();
		y += 10;
	}
	s += G_Fmt("xv 24 yv {} string2 \"[E] take  [I] loadout\" ", y).data();

	Loot_SendLayout(ent, s);
}

// Loadout screen: all slots in brief, full details and parts of the equipped gun.
// "L13 Rare D135% R110% A90% M30 Fir25"
std::string Loot_GunBrief(const loot_weapon_t &g)
{
	std::string brief = G_Fmt("L{} {} D{:.0f}% R{:.0f}% A{:.0f}%", Loot_GunLevel(g), Loot_RarityName(g.rarity), g.dmg_mult * 100.f, g.rate_mult * 100.f, 100.f / g.spread_mult).data();
	if (g.mag_size > 0)
		brief += G_Fmt(" M{}", g.mag_size).data();
	if (g.element)
		brief += G_Fmt(" {}{:.0f}", loot_element_short[g.element], Loot_ElemChance(g) * 100.f).data();
	return brief;
}

// Vendor line: like Loot_GunBrief, each stat green/red vs the equipped gun (other may be null).
std::string Loot_GunBriefCompare(const loot_weapon_t &g, const loot_weapon_t *other)
{
	const bool cmp = other && other->valid;
	auto col = [&](float value, float other_value)
	{ return Loot_CompareColor(value, other_value, cmp, false); };

	std::string brief = G_Fmt("{}L{}^w {}", col((float)Loot_GunLevel(g), cmp ? (float)Loot_GunLevel(*other) : 0), Loot_GunLevel(g), Loot_RarityName(g.rarity)).data();
	brief += G_Fmt(" {}D{:.0f}%^w", col(g.dmg_mult, cmp ? other->dmg_mult : 0), g.dmg_mult * 100.f).data();
	brief += G_Fmt(" {}R{:.0f}%^w", col(g.rate_mult, cmp ? other->rate_mult : 0), g.rate_mult * 100.f).data();
	brief += G_Fmt(" {}A{:.0f}%^w", col(1.0f / g.spread_mult, cmp ? 1.0f / other->spread_mult : 0), 100.f / g.spread_mult).data();
	if (g.mag_size > 0)
		brief += G_Fmt(" {}M{}^w", Loot_CompareColor((float)g.mag_size, cmp ? (float)other->mag_size : 0, cmp && other->mag_size > 0, false), g.mag_size).data();
	if (g.element)
		brief += G_Fmt(" {}{:.0f}", loot_element_short[g.element], Loot_ElemChance(g) * 100.f).data();
	return brief;
}

// Vendor line for a shield, green/red vs the equipped shield.
std::string Loot_ShieldBriefCompare(const loot_shield_t &sh, const loot_shield_t &other)
{
	const bool cmp = other.valid;
	return G_Fmt("L{} Cap {}{:.0f}^w Dly {}{:.1f}s^w Rch {}{:.0f}/s^w", sh.level,
	             Loot_CompareColor(sh.capacity, other.capacity, cmp, false), sh.capacity,
	             Loot_CompareColor(sh.delay, other.delay, cmp, true), sh.delay,
	             Loot_CompareColor(sh.rate, other.rate, cmp, false), sh.rate)
	    .data();
}

// Loadout screen. Page 0: table of guns, shield and grenade. Page 1: full stats
// and parts of the held gun. Two pages because one doesn't fit the 1024-char
// layout limit. Columns use loot_tabs + '|' (see cg_loot.cpp).
static void Loot_SendInventory(edict_t *ent)
{
	gclient_t *cl = ent->client;
	std::string s = "loot_dim xl 16 "; // dim backdrop first so text draws on top
	int32_t y = 16;

	auto row = [&](const std::string &text, int32_t height = 10)
	{
		s += G_Fmt("yv {} loot_str \"{}\" ", y, text).data();
		y += height;
	};

	if (cl->loot_inv_page == 0)
	{
		// columns: key, level, rarity, damage, rate, accuracy, mag, element
		s += "loot_tabs \"56 96 168 224 268 332 368\" ";
		row("^aLOADOUT", 14);
		row("^aGUNS^w|Level|Rarity|Damage|Rate|Accuracy|Mag|Element", 12);

		for (int32_t i = 0; i < LOOT_NUM_SLOTS; i++)
		{
			const loot_weapon_t &g = cl->pers.loot_slots[i];
			const bool active = i == cl->pers.loot_active_slot && Loot_Active(ent);
			if (!g.valid)
			{
				row(G_Fmt(" {}|- empty -", i + 1).data(), 13);
				continue;
			}

			// held gun in green with a marker
			row(G_Fmt("{}{}{}|{}", active ? "^a" : "", active ? ">" : " ", i + 1, g.name).data());
			std::string stats = G_Fmt("|L{}|{}|{:.0f}%|{:.0f}%|{:.0f}%|{}|", Loot_GunLevel(g), Loot_RarityName(g.rarity), g.dmg_mult * 100.f, g.rate_mult * 100.f,
			                          100.f / g.spread_mult, g.mag_size > 0 ? std::to_string(g.mag_size) : "-")
			                        .data();
			if (g.element)
				stats += G_Fmt("{} {:.0f}%", Loot_ElementName(g.element), Loot_ElemChance(g) * 100.f).data();
			row(stats, 13);
		}

		const loot_shield_t &sh = cl->pers.loot_shield;
		if (sh.valid)
		{
			row("^aSHIELD", 12);
			row(G_Fmt("|{}", sh.name).data());
			row(G_Fmt("|L{} {}   Capacity {:.0f}/{:.0f}   Delay {:.1f}s   Recharge {:.0f}/s", sh.level, Loot_RarityName(sh.rarity), cl->pers.loot_shield_points,
			          sh.capacity, sh.delay, sh.rate)
			        .data());
			if (sh.special)
				row(G_Fmt("|{}", Loot_ShieldSpecialText(sh)).data());
			y += 3;
		}

		const loot_grenade_t &grenade = cl->pers.loot_grenade;
		if (grenade.valid)
		{
			row("^aGRENADE", 12);
			row(G_Fmt("|{} ({})", grenade.name, Loot_RarityName(grenade.rarity)).data());
			row("|" + Grenade_LoadoutLine(grenade), 13);
		}

		row("^a[1-4] equip  [G] drop  [I] gun details");
	}
	else
	{
		// columns: label, value, label, value, label, value
		s += "loot_tabs \"72 128 200 256 328 384\" ";
		const loot_weapon_t *g = Loot_Active(ent);
		row("^aGUN DETAILS", 14);
		if (g)
		{
			row(G_Fmt("^a{}^w   L{} {}", g->name, Loot_GunLevel(*g), Loot_RarityName(g->rarity)).data(), 13);
			row(G_Fmt("Damage|{:.0f}%|Rate|{:.0f}%|Accuracy|{:.0f}%", g->dmg_mult * 100.f, g->rate_mult * 100.f, 100.f / g->spread_mult).data());
			if (g->mag_size > 0)
				row(G_Fmt("Magazine|{}/{}|Reload|{:.1f}s|Crit|{:.2f}x", g->mag_ammo, g->mag_size, g->reload_time, Loot_CritMult(*g)).data());
			else
				row(G_Fmt("Crit|{:.2f}x", Loot_CritMult(*g)).data());
			row(G_Fmt("Recoil|{:.0f}%|Handling|{:.0f}%|Zoom|{:.1f}x", Loot_Neutral(g->recoil_mult) * 100.f, Loot_Neutral(g->handling_mult) * 100.f,
			          g->zoom > 0 ? g->zoom : 1.5f)
			        .data());
			std::string extra = G_Fmt("Velocity|{:.0f}%", Loot_Neutral(g->proj_speed) * 100.f).data();
			if (g->burst)
				extra += G_Fmt("|Burst|{}", g->burst).data();
			if (g->pellets_bonus)
				extra += G_Fmt("|Pellets|+{}", g->pellets_bonus).data();
			row(extra, 13);
			if (g->element)
				row(G_Fmt("Element|{}  {:.0f}% chance per hit", Loot_ElementName(g->element), Loot_ElemChance(*g) * 100.f).data());
			if (g->unique)
				row(G_Fmt("Unique|{}", Unique_Desc(g->unique)).data());
			if (g->element || g->unique)
				y += 3;

			row("^aPARTS", 12);
			for (int32_t p = 0; p < LOOT_PART_COUNT; p += 3)
			{
				std::string line;
				for (int32_t q = p; q < p + 3 && q < LOOT_PART_COUNT; q++)
					line += G_Fmt("{}{}|{}", q > p ? "|" : "", Loot_PartName((loot_part_t)q), loot_makers[g->parts[q]].short_name).data();
				row(line);
			}
			y += 3;
		}
		else
			row("No rolled gun equipped", 13);

		row("^a[1-4] equip  [G] drop  [I] close");
	}

	if (s.size() > 1000) // engine layout limit is 1024
		gi.Com_PrintFmt("loadout: layout {} chars, near limit\n", s.size());
	Loot_SendLayout(ent, s);
}

static void Loot_SetHudString(edict_t *ent, int32_t cs, char *cache, size_t cache_size, const char *line, player_stat_t stat)
{
	if (strcmp(line, cache))
	{
		Q_strlcpy(cache, line, cache_size);
		gi.configstring(cs, line);
	}
	ent->client->ps.stats[stat] = line[0] ? (int16_t)cs : 0;
}

// "monster_soldier_light" -> "Soldier Light"
static void Loot_MonsterName(const edict_t *mon, char (&out)[32])
{
	const char *src = mon->classname ? mon->classname : "";
	if (!strncmp(src, "monster_", 8))
		src += 8;

	size_t n = 0;
	bool upper = true;
	for (; *src && n < sizeof(out) - 1; src++)
	{
		char c = *src == '_' ? ' ' : *src;
		out[n++] = upper ? (char)toupper((unsigned char)c) : c;
		upper = c == ' ';
	}
	out[n] = '\0';
}

static void Loot_TargetLine(edict_t *ent, char (&line)[48])
{
	line[0] = '\0';

	vec3_t forward;
	AngleVectors(ent->client->v_angle, forward, nullptr, nullptr);
	const vec3_t start = ent->s.origin + vec3_t{0, 0, (float)ent->viewheight};
	const vec3_t end = start + forward * 2048.f;

	trace_t tr = gi.traceline(start, end, ent, MASK_SHOT);
	edict_t *t = tr.ent;
	ent->client->loot_aim_ent = nullptr;
	if (!t || !(t->svflags & SVF_MONSTER) || t->health <= 0 || t->deadflag)
		return;
	ent->client->loot_aim_ent = t;

	char name[32];
	Loot_MonsterName(t, name);

	const loot_status_t &st = t->loot_status;
	const char *status = "";
	if (st.active && st.burn_end > level.time)
		status = "  Burning";
	else if (st.active && st.melt_end > level.time)
		status = "  Melting";
	else if (st.active && st.shock_end > level.time)
		status = "  Shocked";

	char traits[24];
	NGPlus_TraitNames(t, traits);
	G_FmtTo(line, "Lv {} {}{}{}{}", std::max(1, t->loot_mlevel), t->loot_omega ? "^oOmega^w " : t->loot_elite ? "Alpha "
	                                                                                                          : "",
	        traits, name, status);
}

// Scope zoom while +zoom is held: fov divided by the scope's zoom factor.
// Welcome logo in the console once per game start / save load (InitGame
// queues it), printed 1s into the map so it lands after the load messages.
// One print per line: a single long print was cut short by the engine.
static bool loot_welcome_pending;

void Loot_QueueWelcome()
{
	loot_welcome_pending = true;
}

static void Loot_WelcomeFrame()
{
	if (!loot_welcome_pending || level.time < 1_sec)
		return;
	loot_welcome_pending = false;

	static const char *const lines[] = {
	    "     .,o'       `o,.     ",
	    "   ,o8'           `8o.   ",
	    "  o8'               `8o  ",
	    " o8:                 ;8o ",
	    ".88                   88.",
	    ":88.                 ,88:",
	    "`888                 888'",
	    " 888o   `888 888'   o888 ",
	    " `888o,. `88 88' .,o888' ",
	    "  `8888888888888888888'  ",
	    "    `888888888888888'    ",
	    "       `::88;88;:'       ",
	    "          88 88          ",
	    "          88 88 ARPG     ",
	    "          `8 8'          ",
	    "           ` '           ",
	    "",
	    "Quake II: The Action RPG",
	    "Kill. Loot. Level up. Repeat.",
	    "Version " Q2RPG_VERSION " (build " __DATE__ ")",
	    "",
	};
	for (const char *line : lines)
		gi.Com_PrintFmt("{}\n", line);

	// TEMP for playtesting: command list. Remove before release.
	static const char *const commands[] = {
	    "",
	    "Commands (keys from autoexec.cfg):",
	    "  loot_take E   loot_drop G   loot_inv I   loot_sell X",
	    "  vendor V   skills K   challenges J   grenade Q",
	    "  action F   reload R   zoom: hold mouse2",
	    "  skill_pick <1-3>   skill_add <n>",
	    "Dev (cheats in multiplayer):",
	    "  loot_spawn [arch rarity]   loot_unique <arch> [2]",
	    "  loot_shield [rarity]   loot_grenade [type rarity]",
	    "  loot_spawnmonster <classname> [omega]   loot_xp <n>   loot_money <n>",
	    "  loot_ngplus   loot_stats   loot_dump   skill_respec",
	    "Cvars: g_loot_droprate g_loot_rarity_bonus g_loot_monster_health",
	    "  g_loot_elite_chance g_loot_guard_shield g_loot_debug",
	    "  cl_loot_healthbar cl_loot_fov43 cl_loot_damagenums",
	    "",
	};
	for (const char *line : commands)
		gi.Com_PrintFmt("{}\n", line);
}

static void Loot_ZoomFrame(edict_t *ent)
{
	gclient_t *cl = ent->client;
	loot_weapon_t *gun = Loot_Active(ent);
	const bool want = cl->loot_zoom_held && gun && ent->health > 0 && !cl->loot_reloading && !level.intermissiontime;

	if (want && !cl->loot_zoom_applied)
	{
		cl->loot_base_fov = cl->ps.fov;
		cl->ps.fov = cl->loot_base_fov / (gun->zoom > 0 ? gun->zoom : 1.5f);
		cl->loot_zoom_applied = true;
	}
	else if (!want && cl->loot_zoom_applied)
	{
		cl->ps.fov = cl->loot_base_fov;
		cl->loot_zoom_applied = false;
	}
}

// Unlock (first map), recharge and HUD stats for the player's shield.
static void Loot_ShieldFrame(edict_t *ent)
{
	gclient_t *cl = ent->client;
	client_persistant_t &pers = cl->pers;

	if (!pers.loot_shields_unlocked && Loot_ShieldsAvailable() && ent->health > 0)
	{
		pers.loot_shields_unlocked = true;
		if (!pers.loot_shield.valid)
		{
			pers.loot_shield = Loot_RollShield(LOOT_RARITY_COMMON, std::max(1, pers.loot_level), (uint32_t)mt_rand());
			pers.loot_shield_points = pers.loot_shield.capacity;
		}
		gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power1.wav"), 1, ATTN_NORM, 0);
		gi.Center_Print(ent, G_Fmt("SHIELD ONLINE\n{}", pers.loot_shield.name).data());
	}

	if (!pers.loot_shield.valid)
	{
		cl->ps.stats[STAT_LOOT_SHIELD] = 0;
		cl->ps.stats[STAT_LOOT_SHIELD_MAX] = 0;
		return;
	}

	const loot_shield_t &sh = pers.loot_shield;
	// Capacitor / Quick Charge
	const float cap = sh.capacity * Loot_ShieldCapMult(ent);
	const float delay = sh.delay * Skill_ShieldDelayMult(ent);
	// hit_time > level.time means it is left over from a previous map
	const bool waited = cl->loot_shield_hit_time > level.time || (level.time - cl->loot_shield_hit_time).seconds() >= delay;
	if (ent->health > 0 && waited && pers.loot_shield_points < cap)
		pers.loot_shield_points = std::min(cap, pers.loot_shield_points + sh.rate * FRAME_TIME_S.seconds());
	pers.loot_shield_points = std::min(pers.loot_shield_points, cap);

	cl->ps.stats[STAT_LOOT_SHIELD] = (int16_t)std::clamp((int32_t)ceilf(pers.loot_shield_points), 0, 32767);
	cl->ps.stats[STAT_LOOT_SHIELD_MAX] = (int16_t)std::clamp((int32_t)cap, 1, 32767);
}

static bool Loot_BarValid(const edict_t *t)
{
	return t && t->inuse && (t->svflags & SVF_MONSTER) && t->health > 0 && !t->deadflag;
}

// Packs the overhead health bar target into stats for cg_loot.cpp.
static void Loot_SetBarStats(edict_t *ent)
{
	gclient_t *cl = ent->client;
	edict_t *t = nullptr;

	// bar_time > level.time means it is left over from a previous map
	if (Loot_BarValid(cl->loot_bar_ent) && cl->loot_bar_time <= level.time && level.time - cl->loot_bar_time < 3_sec)
		t = cl->loot_bar_ent;
	else if (Loot_BarValid(cl->loot_aim_ent))
		t = cl->loot_aim_ent;

	if (!t)
	{
		cl->ps.stats[STAT_LOOT_BAR_INFO] = 0;
		cl->ps.stats[STAT_LOOT_BAR_SHIELD] = 0;
		return;
	}

	cl->ps.stats[STAT_LOOT_BAR_SHIELD] = t->loot_mshield_max > 0 ? (int16_t)(1 + std::clamp(t->loot_mshield / t->loot_mshield_max, 0.f, 1.f) * 254.f) : 0;

	const vec3_t pos = {(t->absmin[0] + t->absmax[0]) * 0.5f, (t->absmin[1] + t->absmax[1]) * 0.5f, t->absmax[2] + 6.f};
	cl->ps.stats[STAT_LOOT_BAR_X] = (int16_t)std::clamp((int32_t)pos[0], -32767, 32767);
	cl->ps.stats[STAT_LOOT_BAR_Y] = (int16_t)std::clamp((int32_t)pos[1], -32767, 32767);
	cl->ps.stats[STAT_LOOT_BAR_Z] = (int16_t)std::clamp((int32_t)pos[2], -32767, 32767);

	const float frac = std::clamp((float)t->health / std::max(1, t->max_health), 0.f, 1.f);
	const uint16_t info = 0x8000 | ((uint16_t)(frac * 127.f) << 8) | (t->loot_elite ? 0x80 : 0) | (uint16_t)std::clamp(t->loot_mlevel, 1, 127);
	cl->ps.stats[STAT_LOOT_BAR_INFO] = (int16_t)info;
}

void Loot_SetStats(edict_t *ent)
{
	gclient_t *cl = ent->client;

	// loot stats alias the CTF slots; in deathmatch they belong to CTF
	if (deathmatch->integer)
		return;

	Loot_ShieldFrame(ent);
	Skill_Frame(ent);
	Loot_SecondChanceFrame(ent);
	Loot_WelcomeFrame();
	NGPlus_Frame(ent);
	Loot_ZoomFrame(ent);
	Loot_FlushDamageNumber(ent);

	loot_weapon_t *gun = Loot_Active(ent);

	// magazine replaces the ammo counter
	if (gun && gun->mag_size > 0)
		cl->ps.stats[STAT_AMMO] = gun->mag_ammo;

	// HUD line: name, rarity, reserve
	const int32_t playernum = ent->s.number - 1;
	if (playernum >= 0 && playernum < (CONFIG_LOOT_HUD_END - CONFIG_LOOT_HUD))
	{
		char line[64];
		char tag[32] = "";
		if (gun)
			G_FmtTo(tag, "L{} {}{}{}", Loot_GunLevel(*gun), Loot_RarityName(gun->rarity), gun->element ? " " : "", gun->element ? Loot_ElementName(gun->element) : "");

		if (!gun)
			line[0] = '\0';
		else if (cl->loot_reloading)
			G_FmtTo(line, "{}  RELOADING", gun->name);
		else if (gun->mag_size > 0)
		{
			const int32_t reserve = Loot_Reserve(ent, *gun);
			if (reserve >= 9999)
				G_FmtTo(line, "{} [{}]  inf", gun->name, tag);
			else
				G_FmtTo(line, "{} [{}]  {}", gun->name, tag, reserve);
		}
		else
			G_FmtTo(line, "{} [{}]", gun->name, tag);

		if (strcmp(line, cl->loot_hud_cache))
		{
			Q_strlcpy(cl->loot_hud_cache, line, sizeof(cl->loot_hud_cache));
			gi.configstring(CONFIG_LOOT_HUD + playernum, line);
		}

		cl->ps.stats[STAT_LOOT_HUD] = line[0] ? (int16_t)(CONFIG_LOOT_HUD + playernum) : 0;

		// level / xp line
		char xpline[48];
		const int32_t plevel = std::max(1, cl->pers.loot_level);
		if (plevel >= Loot_LevelCap())
			G_FmtTo(xpline, "Lv {}  MAX  ${}", plevel, cl->pers.loot_money);
		else
			G_FmtTo(xpline, "Lv {}  XP {}/{}  ${}", plevel, cl->pers.loot_xp, Loot_XPToNext(plevel), cl->pers.loot_money);
		const int32_t unspent = Skill_Unspent(ent);
		if (unspent > 0)
		{
			char base[48];
			Q_strlcpy(base, xpline, sizeof(base));
			G_FmtTo(xpline, "{}  [K] +{} pt{}", base, unspent, unspent > 1 ? "s" : "");
		}
		Loot_SetHudString(ent, CONFIG_LOOT_XP + playernum, cl->loot_xp_cache, sizeof(cl->loot_xp_cache), xpline, STAT_LOOT_XP);
		Skill_SetActionStat(ent, playernum);

		// aimed-at monster: level, name, status
		cl->ps.stats[STAT_LOOT_TARGET] = cl->loot_target_cache[0] ? (int16_t)(CONFIG_LOOT_TARGET + playernum) : 0;
		if (level.time >= cl->loot_target_time || cl->loot_target_time > level.time + 1_sec)
		{
			cl->loot_target_time = level.time + 100_ms;
			char tline[48];
			Loot_TargetLine(ent, tline);
			Loot_SetHudString(ent, CONFIG_LOOT_TARGET + playernum, cl->loot_target_cache, sizeof(cl->loot_target_cache), tline, STAT_LOOT_TARGET);
		}
	}

	Loot_SetBarStats(ent);

	// loadout screen or item card
	if (cl->showscores || cl->showhelp || cl->showeou || level.intermissiontime || ent->health <= 0)
	{
		cl->loot_card_sent = nullptr;
		cl->loot_showinv = false;
		cl->loot_showskills = false;
		cl->loot_showchallenges = false;
		if (ent->health <= 0 || level.intermissiontime)
			cl->loot_shop_open = false; // paid-for shop survives scoreboard/help, not death
		return;
	}

	if (Challenge_Layout(ent))
	{
		cl->loot_card_sent = nullptr;
		cl->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
		return;
	}

	if (Skill_Layout(ent))
	{
		cl->loot_card_sent = nullptr;
		cl->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
		return;
	}

	if (!cl->loot_showinv && Vendor_Layout(ent))
	{
		cl->loot_card_sent = nullptr;
		cl->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
		return;
	}

	if (cl->loot_showinv)
	{
		// refresh twice a second so magazine counts stay current
		if (level.time >= cl->loot_inv_time || cl->loot_inv_time > level.time + 1_sec)
		{
			Loot_SendInventory(ent);
			cl->loot_inv_time = level.time + 500_ms;
		}
		cl->loot_card_sent = nullptr;
		cl->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
		return;
	}

	// item card for the gun the player stands on

	edict_t *target = Loot_TouchTarget(ent);
	if (!target)
	{
		cl->loot_card_sent = nullptr;
		return;
	}

	if (target != cl->loot_card_sent || level.time - cl->loot_card_time > 1_sec)
	{
		if (target->loot_shield.valid)
			Loot_SendShieldCard(ent, target->loot_shield);
		else if (target->loot_grenade.valid)
			Grenade_SendCard(ent, target->loot_grenade);
		else
			Loot_SendCard(ent, target->loot);
		cl->loot_card_sent = target;
		cl->loot_card_time = level.time;
	}

	cl->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
}

//==========================================================================
// Commands
//==========================================================================

static int32_t Loot_FindEmptySlot(gclient_t *cl)
{
	for (int32_t i = 0; i < LOOT_NUM_SLOTS; i++)
		if (!cl->pers.loot_slots[i].valid)
			return i;
	return -1;
}

static int32_t Loot_CountGuns(gclient_t *cl)
{
	int32_t n = 0;
	for (const auto &slot : cl->pers.loot_slots)
		if (slot.valid)
			n++;
	return n;
}

// Equip a shield; the old one drops at the player's feet.
void Loot_GiveShield(edict_t *ent, const loot_shield_t &sh)
{
	gclient_t *cl = ent->client;

	if (cl->pers.loot_shield.valid)
		Loot_SpawnShieldDrop(ent, cl->pers.loot_shield, true);

	cl->pers.loot_shield = sh;
	cl->pers.loot_shield_points = 0;
	cl->loot_shield_hit_time = level.time;

	gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/power1.wav"), 1, ATTN_NORM, 0);
	gi.Center_Print(ent, G_Fmt("{} ({} Shield)", sh.name, Loot_RarityName(sh.rarity)).data());
}

// Put a gun in an empty slot, else swap with the active gun (old one drops). Equips it.
void Loot_GiveGun(edict_t *ent, const loot_weapon_t &gun)
{
	gclient_t *cl = ent->client;
	int32_t slot = Loot_FindEmptySlot(cl);

	if (slot < 0)
	{
		slot = cl->pers.loot_active_slot;
		if (slot < 0 || slot >= LOOT_NUM_SLOTS)
			slot = 0;

		Loot_SpawnDrop(ent, cl->pers.loot_slots[slot], true);
	}

	cl->pers.loot_slots[slot] = gun;

	Loot_SyncInventory(ent);
	cl->pers.loot_active_slot = -1;
	Loot_SelectSlot(ent, slot);

	gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/w_pkup.wav"), 1, ATTN_NORM, 0);
	gi.Center_Print(ent, G_Fmt("{} ({})", gun.name, Loot_RarityName(gun.rarity)).data());
}

static void Loot_Cmd_Take(edict_t *ent)
{
	gclient_t *cl = ent->client;
	edict_t *target = Loot_TouchTarget(ent);

	if (!target || ent->health <= 0)
		return;

	const bool is_shield = target->loot_shield.valid;
	const loot_shield_t sh = target->loot_shield;
	const bool is_grenade = target->loot_grenade.valid;
	const loot_grenade_t grenade = target->loot_grenade;
	const loot_weapon_t gun = target->loot;

	if (target->target)
		G_UseTargets(target, ent);
	G_FreeEdict(target);
	cl->loot_touch_ent = nullptr;
	cl->loot_card_sent = nullptr;

	if (is_shield)
		Loot_GiveShield(ent, sh);
	else if (is_grenade)
		Grenade_Give(ent, grenade);
	else
		Loot_GiveGun(ent, gun);
}

static void Loot_Cmd_Drop(edict_t *ent)
{
	gclient_t *cl = ent->client;
	const int32_t slot = cl->pers.loot_active_slot;

	if (ent->health <= 0 || !Loot_Active(ent))
		return;

	if (Loot_CountGuns(cl) <= 1)
	{
		gi.Center_Print(ent, "Can't drop your last gun");
		return;
	}

	Loot_SpawnDrop(ent, cl->pers.loot_slots[slot], true);
	cl->pers.loot_slots[slot] = {};
	Loot_CancelReload(cl);
	Loot_SyncInventory(ent);

	for (int32_t i = 0; i < LOOT_NUM_SLOTS; i++)
	{
		if (cl->pers.loot_slots[i].valid)
		{
			cl->pers.loot_active_slot = -1;
			Loot_SelectSlot(ent, i);
			break;
		}
	}
}

static void Loot_Cmd_Dump(edict_t *ent)
{
	gclient_t *cl = ent->client;

	for (int32_t i = 0; i < LOOT_NUM_SLOTS; i++)
	{
		const loot_weapon_t &g = cl->pers.loot_slots[i];
		if (!g.valid)
		{
			gi.Client_Print(ent, PRINT_HIGH, G_Fmt("{}{}: empty\n", i == cl->pers.loot_active_slot ? "*" : " ", i + 1).data());
			continue;
		}

		gi.Client_Print(ent, PRINT_HIGH, G_Fmt("{}{}: {} [L{} {} {}] dmg {:.2f} rate {:.2f} spread {:.2f} crit {:.2f} mag {}/{} reload {:.1f}s pellets +{}\n", i == cl->pers.loot_active_slot ? "*" : " ", i + 1, g.name, Loot_GunLevel(g), Loot_RarityName(g.rarity), Loot_MakerName(g.maker), g.dmg_mult, g.rate_mult, g.spread_mult, Loot_CritMult(g), g.mag_ammo, g.mag_size, g.reload_time, g.pellets_bonus).data());
		gi.Client_Print(ent, PRINT_HIGH, G_Fmt("    parts: stock {} barrel {} trigger {} mag {} scope {} | recoil {:.2f} handling {:.2f}\n", loot_makers[g.parts[LOOT_PART_STOCK]].short_name, loot_makers[g.parts[LOOT_PART_BARREL]].short_name, loot_makers[g.parts[LOOT_PART_TRIGGER]].short_name, loot_makers[g.parts[LOOT_PART_MAGAZINE]].short_name, loot_makers[g.parts[LOOT_PART_SCOPE]].short_name, Loot_Neutral(g.recoil_mult), Loot_Neutral(g.handling_mult)).data());
	}
}

static bool Loot_CheatCheck(edict_t *ent)
{
	if (game.maxclients > 1 && !sv_cheats->integer)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_need_cheats");
		return false;
	}
	return true;
}

// loot_spawn [archetype 0-9] [rarity 0-4]
// loot_spawnmonster <classname>: spawn a monster in front of you for fight testing.
// It goes through monster_start like a map monster (level, shield, elite roll, drops).
static void Loot_Cmd_SpawnMonster(edict_t *ent)
{
	if (!Loot_CheatCheck(ent))
		return;

	const char *classname = gi.argc() >= 2 ? gi.argv(1) : "";
	if (Q_strncasecmp(classname, "monster_", 8))
	{
		gi.Client_Print(ent, PRINT_HIGH, "usage: loot_spawnmonster monster_<name> [omega]  (e.g. monster_gunner)\n");
		return;
	}

	// a spot up to 160 units ahead, pulled back from any wall
	vec3_t forward;
	AngleVectors({0, ent->client->v_angle[YAW], 0}, forward, nullptr, nullptr);
	const vec3_t start = ent->s.origin + vec3_t{0, 0, 16};
	const trace_t tr = gi.trace(start, vec3_origin, vec3_origin, start + forward * 160.f, ent, MASK_MONSTERSOLID);
	const vec3_t spot = tr.endpos - forward * 48.f;

	loot_force_omega = gi.argc() >= 3 && !Q_strcasecmp(gi.argv(2), "omega");
	edict_t *monster = CreateMonster(spot, {0, ent->client->v_angle[YAW] + 180.f, 0}, G_CopyString(classname, TAG_LEVEL));
	loot_force_omega = false;
	if (!monster || !monster->inuse)
	{
		gi.Client_Print(ent, PRINT_HIGH, G_Fmt("loot_spawnmonster: unknown monster {}\n", classname).data());
		return;
	}

	gi.Client_Print(ent, PRINT_HIGH, G_Fmt("Spawned {} Lv {}{}\n", classname, monster->loot_mlevel, monster->loot_omega ? " (Omega)" : monster->loot_elite ? " (Alpha)"
	                                                                                                                                                       : "")
	                                     .data());
}

static void Loot_Cmd_Spawn(edict_t *ent)
{
	if (!Loot_CheatCheck(ent))
		return;

	loot_weapon_t gun;

	if (gi.argc() >= 3)
	{
		const int32_t a = std::clamp(atoi(gi.argv(1)), 0, LOOT_ARCH_COUNT - 1);
		const int32_t r = std::clamp(atoi(gi.argv(2)), 0, LOOT_RARITY_COUNT - 1);
		gun = Loot_Roll((loot_archetype_t)a, (loot_rarity_t)r, (uint32_t)mt_rand());
	}
	else
		gun = Loot_RollRandom(gi.argc() >= 2 ? (float)atof(gi.argv(1)) : 0.f);

	Loot_SetLevel(gun, std::max(1, ent->client->pers.loot_level));
	Loot_SpawnDrop(ent, gun, true);
}

//==========================================================================
// Balance stats
//==========================================================================

static std::string Loot_StatsRarityCounts(const std::array<int32_t, LOOT_RARITY_COUNT> &counts)
{
	std::string text;
	for (int32_t r = 0; r < LOOT_RARITY_COUNT; r++)
		text += G_Fmt("{}{} {}", r ? ", " : "", counts[r], Loot_RarityName((loot_rarity_t)r)).data();
	return text;
}

// Human-readable summary of this map's numbers (several lines).
static std::string Loot_StatsSummary()
{
	const loot_stats_t &stats = level.loot_stats;
	const char *map_mod = NGPlus_MapModName();
	std::string text = G_Fmt("loot_stats {} ({:.0f}s){}{}\n", level.mapname, level.time.seconds(), map_mod[0] ? "  modifier: " : "", map_mod).data();
	text += G_Fmt("  kills {} (elite {}, boss {})  xp {}\n", stats.kills, stats.elite_kills, stats.boss_kills, stats.xp).data();
	text += "  guns: " + Loot_StatsRarityCounts(stats.gun_drops) + "\n";
	text += "  shields: " + Loot_StatsRarityCounts(stats.shield_drops) + "\n";
	text += G_Fmt("  cash dropped ${} taken ${}  spent ${}  sold ${}\n", stats.cash_dropped, stats.cash_taken, stats.money_spent, stats.money_sold).data();
	text += G_Fmt("  grenade mods dropped {}  grenade damage {}\n  thrown / kills:", stats.grenade_mod_drops, stats.grenade_damage).data();
	for (int32_t t = 0; t < LOOT_GRENADE_TYPE_COUNT; t++)
		text += G_Fmt(" {} {}/{}", Grenade_TypeName((loot_grenade_type_t)t), stats.grenades_thrown[t], stats.grenade_kills[t]).data();
	text += "\n";
	for (auto player : active_players())
	{
		const client_persistant_t &pers = player->client->pers;
		text += G_Fmt("  {}: Lv {} XP {}/{} ${} hp {}\n", pers.netname, pers.loot_level, pers.loot_xp, Loot_XPToNext(pers.loot_level), pers.loot_money, pers.max_health).data();
	}
	return text;
}

// q2rpg: playtest events log. One line per event, appended and never cleared
// (the engine's stdout.txt is wiped every launch):
// "2026-11-10 21:13  fact1  L52  CHALLENGE Payday tier 4"
void Loot_LogEvent(edict_t *ent, const std::string &text)
{
	if (deathmatch->integer || !g_loot_logs->integer)
		return;

	FILE *log = fopen("q2rpg/q2rpg_events.log", "a");
	if (!log)
		return;

	char stamp[32] = "";
	const time_t now = time(nullptr);
	if (const tm *local = localtime(&now))
		strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", local);

	std::string who;
	if (ent && ent->client)
	{
		who = G_Fmt("L{}", ent->client->pers.loot_level).data();
		if (game.maxclients > 1)
			who += G_Fmt(" P{}", ent->s.number).data();
	}

	fprintf(log, "%s  %s  %s  %s\n", stamp, level.mapname, who.c_str(), text.c_str());
	fclose(log);
}

// Map end: print the summary and append one CSV row per map to q2rpg/loot_stats.csv.
void Loot_StatsLogMap()
{
	if (deathmatch->integer)
		return;

	gi.Com_Print(Loot_StatsSummary().c_str());
	if (!g_loot_logs->integer)
		return;

	const loot_stats_t &stats = level.loot_stats;
	const char *csv_path = "q2rpg/loot_stats.csv";
	FILE *csv = fopen(csv_path, "a");
	if (!csv)
	{
		gi.Com_PrintFmt("loot_stats: can't open {}\n", csv_path);
		return;
	}

	fseek(csv, 0, SEEK_END);
	if (ftell(csv) == 0)
	{
		std::string header = "map,seconds,kills,elite_kills,boss_kills,"
		                     "guns_common,guns_uncommon,guns_rare,guns_epic,guns_legendary,guns_mythic,"
		                     "shields_common,shields_uncommon,shields_rare,shields_epic,shields_legendary,shields_mythic,"
		                     "xp,cash_dropped,cash_taken,money_spent,money_sold,"
		                     "player_level,player_xp,player_money,player_max_health,"
		                     "grenade_mod_drops,grenade_damage";
		// thrown_<type> and kills_<type> columns follow the grenade type table
		for (const char *prefix : {"thrown_", "kills_"})
			for (int32_t t = 0; t < LOOT_GRENADE_TYPE_COUNT; t++)
			{
				std::string name = Grenade_TypeName((loot_grenade_type_t)t);
				for (char &c : name)
					c = (char)tolower((unsigned char)c);
				header += std::string(",") + prefix + name;
			}
		header += "\n";
		fputs(header.c_str(), csv);
	}

	// player columns come from the first player (SP); coop rows still show party totals above
	int32_t player_level = 0, player_xp = 0, player_money = 0, player_max_health = 0;
	for (auto player : active_players())
	{
		player_level = player->client->pers.loot_level;
		player_xp = player->client->pers.loot_xp;
		player_money = player->client->pers.loot_money;
		player_max_health = player->client->pers.max_health;
		break;
	}

	std::string row = G_Fmt("{},{:.0f},{},{},{}", level.mapname, level.time.seconds(), stats.kills, stats.elite_kills, stats.boss_kills).data();
	for (int32_t count : stats.gun_drops)
		row += G_Fmt(",{}", count).data();
	for (int32_t count : stats.shield_drops)
		row += G_Fmt(",{}", count).data();
	row += G_Fmt(",{},{},{},{},{},{},{},{},{}", stats.xp, stats.cash_dropped, stats.cash_taken, stats.money_spent, stats.money_sold, player_level, player_xp, player_money, player_max_health).data();
	row += G_Fmt(",{},{}", stats.grenade_mod_drops, stats.grenade_damage).data();
	for (int32_t count : stats.grenades_thrown)
		row += G_Fmt(",{}", count).data();
	for (int32_t count : stats.grenade_kills)
		row += G_Fmt(",{}", count).data();
	row += "\n";
	fputs(row.c_str(), csv);
	fclose(csv);
}

bool Loot_ClientCommand(edict_t *ent, const char *cmd)
{
	if (deathmatch->integer)
		return false;

	if (Skill_ClientCommand(ent, cmd))
		return true;
	if (Challenge_ClientCommand(ent, cmd))
		return true;
	if (Vendor_ClientCommand(ent, cmd))
		return true;

	if (!Q_strcasecmp(cmd, "slot"))
	{
		Loot_SelectSlot(ent, atoi(gi.argv(1)) - 1);
		return true;
	}
	// "+"/"-" commands are not forwarded by the engine; autoexec aliases +loot_zoom to these
	if (!Q_strcasecmp(cmd, "zoom_on"))
	{
		ent->client->loot_zoom_held = true;
		return true;
	}
	if (!Q_strcasecmp(cmd, "zoom_off"))
	{
		ent->client->loot_zoom_held = false;
		return true;
	}
	if (!Q_strcasecmp(cmd, "zoom"))
	{
		ent->client->loot_zoom_held = !ent->client->loot_zoom_held;
		return true;
	}
	if (!Q_strcasecmp(cmd, "reload"))
	{
		Loot_TryReload(ent);
		return true;
	}
	if (!Q_strcasecmp(cmd, "loot_take"))
	{
		Loot_Cmd_Take(ent);
		return true;
	}
	if (!Q_strcasecmp(cmd, "loot_drop"))
	{
		Loot_Cmd_Drop(ent);
		return true;
	}
	if (!Q_strcasecmp(cmd, "loot_inv"))
	{
		// I cycles: loadout table -> held gun details -> closed
		gclient_t *cl = ent->client;
		if (!cl->loot_showinv)
		{
			cl->loot_showinv = true;
			cl->loot_inv_page = 0;
		}
		else if (cl->loot_inv_page == 0)
			cl->loot_inv_page = 1;
		else
			cl->loot_showinv = false;
		ent->client->loot_showskills = false;
		ent->client->loot_showchallenges = false;
		ent->client->loot_inv_time = 0_ms;
		return true;
	}
	if (!Q_strcasecmp(cmd, "loot_shield"))
	{
		// loot_shield [rarity 0-4]
		if (Loot_CheatCheck(ent))
		{
			const int32_t r = gi.argc() >= 2 ? std::clamp(atoi(gi.argv(1)), 0, LOOT_RARITY_COUNT - 1) : (int32_t)Loot_RollRarity(0.f);
			Loot_SpawnShieldDrop(ent, Loot_RollShield((loot_rarity_t)r, std::max(1, ent->client->pers.loot_level), (uint32_t)mt_rand()), true);
		}
		return true;
	}
	if (!Q_strcasecmp(cmd, "loot_unique"))
	{
		// loot_unique <archetype 0-9> [2]: 2 = the second (NG+) unique
		if (Loot_CheatCheck(ent))
		{
			const int32_t a = std::clamp(atoi(gi.argv(1)), 0, LOOT_ARCH_COUNT - 1);
			loot_weapon_t gun = Loot_Roll((loot_archetype_t)a, LOOT_RARITY_LEGENDARY, (uint32_t)mt_rand());
			Unique_Make(gun, atoi(gi.argv(2)) == 2 ? 1 : 0);
			Loot_SetLevel(gun, std::max(1, ent->client->pers.loot_level));
			Loot_SpawnDrop(ent, gun, true);
		}
		return true;
	}
	if (!Q_strcasecmp(cmd, "loot_xp"))
	{
		if (Loot_CheatCheck(ent))
			Loot_GiveXP(ent, atoi(gi.argv(1)));
		return true;
	}
	// dev: finish the campaign now, which starts the next NG+ cycle
	if (!Q_strcasecmp(cmd, "loot_ngplus"))
	{
		// same path as the real ending (boss2's exit): intermission, then ExitLevel hands it to NGPlus_TryRestart
		if (Loot_CheatCheck(ent))
			BeginIntermission(CreateTargetChangeLevel("*end.cin+victory.pcx"));
		return true;
	}
	if (!Q_strcasecmp(cmd, "grenade"))
	{
		Grenade_Throw(ent);
		return true;
	}
	// dev: grenade mod at your feet; loot_grenade [type 0-4] [rarity 0-5]
	if (!Q_strcasecmp(cmd, "loot_grenade"))
	{
		if (Loot_CheatCheck(ent))
		{
			const int32_t r = gi.argc() >= 3 ? std::clamp(atoi(gi.argv(2)), 0, LOOT_RARITY_COUNT - 1) : (int32_t)Loot_RollRarity(0.f);
			const int32_t want_type = gi.argc() >= 2 ? std::clamp(atoi(gi.argv(1)), 0, LOOT_GRENADE_TYPE_COUNT - 1) : -1;
			loot_grenade_t grenade;
			for (int32_t tries = 0; tries < 200; tries++) // reroll for the asked type so the name matches
			{
				grenade = Grenade_Roll((loot_rarity_t)r, std::max(1, ent->client->pers.loot_level), (uint32_t)mt_rand());
				if (want_type < 0 || grenade.type == want_type)
					break;
			}
			Grenade_SpawnDrop(ent, grenade, true);
		}
		return true;
	}
	if (!Q_strcasecmp(cmd, "loot_stats"))
	{
		gi.Client_Print(ent, PRINT_HIGH, Loot_StatsSummary().c_str());
		return true;
	}
	if (!Q_strcasecmp(cmd, "loot_dump"))
	{
		Loot_Cmd_Dump(ent);
		return true;
	}
	if (!Q_strcasecmp(cmd, "loot_spawnmonster"))
	{
		Loot_Cmd_SpawnMonster(ent);
		return true;
	}
	if (!Q_strcasecmp(cmd, "loot_spawn"))
	{
		Loot_Cmd_Spawn(ent);
		return true;
	}

	return false;
}
