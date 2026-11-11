// q2rpg: New Game+. Finishing the campaign (the "victory.pcx" exit) restarts it at
// the first map with the same characters. Each cycle adds LOOT_NGPLUS_MONSTER_LEVELS
// monster levels (g_loot.cpp Loot_ScaleMonster) and LOOT_NGPLUS_LUCK rarity luck
// (Loot_ZoneLuck). Keys and power cubes are cleared; guns, levels, skills, money,
// upgrades and challenges carry over.

#include "../g_local.h"

#include <vector>
#include <random>

static void NGPlus_SpawnBounty();

static constexpr const char *NGPLUS_FALLBACK_START = "base1"; // saves from before NG+ existed

// Saves made before NG+ existed have no start map; they are base campaign runs.
void NGPlus_OnGameLoaded()
{
	if (!game.loot_start_map[0])
		Q_strlcpy(game.loot_start_map, NGPLUS_FALLBACK_START, sizeof(game.loot_start_map));
}

// A new game spawns its first map with an empty start map (loads go through NGPlus_OnGameLoaded).
void NGPlus_OnMapSpawn()
{
	if (deathmatch->integer)
		return;
	if (!game.loot_start_map[0])
		Q_strlcpy(game.loot_start_map, level.mapname, sizeof(game.loot_start_map));
	NGPlus_SpawnBounty();
}

// Campaign end: the last part of the changemap is "victorXXX.pcx", optionally
// '*'-prefixed. The base campaign's boss2 exits to "*end.cin+victory.pcx" (ending
// video, then the victory screen); N64 changes straight to "victorXXX.pcx".
static bool NGPlus_IsVictory(const char *changemap)
{
	const char *last_part = strrchr(changemap, '+');
	last_part = last_part ? last_part + 1 : changemap;
	if (last_part[0] == '*')
		last_part++;
	const size_t len = strlen(last_part);
	return len > 6 && !Q_strncasecmp(last_part, "victor", 6) && !Q_strncasecmp(last_part + len - 4, ".pcx", 4);
}

static void NGPlus_ClearKeys(client_persistant_t &pers)
{
	for (const gitem_t &item : itemlist)
		if (item.flags & IF_KEY)
			pers.inventory[item.id] = 0;
	pers.power_cubes = 0;
}

bool NGPlus_TryRestart(const char *changemap)
{
	if (deathmatch->integer || !changemap || !NGPlus_IsVictory(changemap) || !game.loot_start_map[0])
		return false;

	game.loot_ngplus++;
	game.loot_ngplus_announce = true;

	// fresh campaign state: cross-level triggers and unit tracking
	game.cross_level_flags = 0;
	game.cross_unit_flags = 0;
	game.level_entries = {};

	for (uint32_t i = 0; i < game.maxclients; i++)
	{
		NGPlus_ClearKeys(game.clients[i].pers);
		NGPlus_ClearKeys(game.clients[i].resp.coop_respawn);
	}

	// '*' starts a new unit, so the engine drops the old hub level saves
	gi.AddCommandString(G_Fmt("gamemap \"*{}\"\n", game.loot_start_map).data());
	return true;
}

//==========================================================================
// Map modifiers: one per NG+ map, rolled before monsters spawn, saved with the level
//==========================================================================

struct ngplus_map_mod_info_t
{
	const char *name;
	const char *effect;
};

static const ngplus_map_mod_info_t map_mods[LOOT_MAPMOD_COUNT] = {
    {"", ""},
    {"Alpha Surge", "3x Alphas"},
    {"Gold Rush", "Double cash"},
    {"Loot Frenzy", "Better loot, tougher monsters"},
    {"Glass Cannon", "Monsters hit harder, die faster"},
    {"Reinforcements", "3x death spawns"},
    {"Elemental Storm", "2x element procs"},
    {"Bounty", "An Alpha Omega roams here, 2x drops"},
    {"Fortified", "2x monster shields, more shield drops"},
    {"Bloodthirsty", "Monsters heal off you, +50% XP"},
};

// Never the same modifier as the map before (revisits keep their saved one).
void NGPlus_RollMapMod()
{
	if (deathmatch->integer || game.loot_ngplus <= 0)
	{
		level.loot_map_mod = LOOT_MAPMOD_NONE;
		return;
	}

	// q2rpg: roll from the save's seed + NG+ cycle + map name, so reloading an
	// autosave from before the roll gives the same modifier (no rerolling)
	if (!game.loot_map_mod_seed)
		game.loot_map_mod_seed = (uint32_t)irandom(1, INT32_MAX);
	uint32_t map_hash = 2166136261u; // FNV-1a
	for (const char *c = level.mapname; *c; c++)
		map_hash = (map_hash ^ (uint8_t)*c) * 16777619u;
	std::mt19937 map_rng(game.loot_map_mod_seed ^ map_hash ^ ((uint32_t)game.loot_ngplus * 0x9e3779b9u));

	int32_t mod;
	do
		mod = 1 + (int32_t)(map_rng() % (LOOT_MAPMOD_COUNT - 1));
	while (mod == game.loot_last_map_mod);
	level.loot_map_mod = mod;
	game.loot_last_map_mod = mod;
}

static bool NGPlus_MapMod(loot_map_mod_t mod)
{
	return level.loot_map_mod == mod;
}

float NGPlus_EliteChanceMult()
{
	return NGPlus_MapMod(LOOT_MAPMOD_ALPHA_SURGE) ? 3.f : 1.f;
}

float NGPlus_CashMult()
{
	return NGPlus_MapMod(LOOT_MAPMOD_GOLD_RUSH) ? 2.f : 1.f;
}

float NGPlus_LuckBonus()
{
	return NGPlus_MapMod(LOOT_MAPMOD_LOOT_FRENZY) ? 1.5f : 0.f;
}

// Monsters get x1.5 health and damage per NG+ cycle on top of their levels:
// gear (rarity, Mythics, uniques, tier-4 skills, badges) outscales levels alone.
constexpr float NGPLUS_CYCLE_MONSTER_MULT = 1.5f;

static float NGPlus_CycleMult()
{
	return game.loot_ngplus > 0 ? powf(NGPLUS_CYCLE_MONSTER_MULT, (float)game.loot_ngplus) : 1.f;
}

float NGPlus_MonsterHealthMult()
{
	float mult = NGPlus_CycleMult();
	if (NGPlus_MapMod(LOOT_MAPMOD_LOOT_FRENZY))
		mult *= 1.3f;
	if (NGPlus_MapMod(LOOT_MAPMOD_GLASS_CANNON))
		mult *= 0.7f;
	return mult;
}

float NGPlus_MonsterDamageMult()
{
	return NGPlus_CycleMult() * (NGPlus_MapMod(LOOT_MAPMOD_GLASS_CANNON) ? 1.4f : 1.f);
}

float NGPlus_DeathSpawnMult()
{
	return NGPlus_MapMod(LOOT_MAPMOD_REINFORCEMENTS) ? 3.f : 1.f;
}

float NGPlus_ProcChanceMult()
{
	return NGPlus_MapMod(LOOT_MAPMOD_ELEMENTAL_STORM) ? 2.f : 1.f;
}

float NGPlus_DropMult()
{
	return NGPlus_MapMod(LOOT_MAPMOD_BOUNTY) ? 2.f : 1.f;
}

float NGPlus_MonsterShieldMult()
{
	return NGPlus_MapMod(LOOT_MAPMOD_FORTIFIED) ? 2.f : 1.f;
}

float NGPlus_ShieldDropMult()
{
	return NGPlus_MapMod(LOOT_MAPMOD_FORTIFIED) ? 2.f : 1.f;
}

float NGPlus_XPMult()
{
	return NGPlus_MapMod(LOOT_MAPMOD_BLOODTHIRSTY) ? 1.5f : 1.f;
}

// Bloodthirsty: a monster that hurts a player heals half the damage.
void NGPlus_OnMonsterHitPlayer(edict_t *monster, int32_t damage)
{
	if (!NGPlus_MapMod(LOOT_MAPMOD_BLOODTHIRSTY) || !monster || monster->health <= 0 || damage <= 0)
		return;
	monster->health = std::min(monster->max_health, monster->health + damage / 2);
}

// Bounty: after the map spawns, promote one random monster to an Alpha Omega.
// Monsters waiting on a trigger_spawn are skipped (not in the world yet).
static void NGPlus_SpawnBounty()
{
	if (!NGPlus_MapMod(LOOT_MAPMOD_BOUNTY))
		return;

	std::vector<edict_t *> candidates;
	for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; i++)
	{
		edict_t *e = &g_edicts[i];
		if (e->inuse && (e->svflags & SVF_MONSTER) && !(e->svflags & SVF_NOCLIENT) && e->health > 0 && !e->deadflag && !e->loot_omega &&
		    Loot_MonsterBaseHealth(e) < 1000)
			candidates.push_back(e);
	}

	// some spots are too tight for an Alpha; try a few
	while (!candidates.empty())
	{
		const size_t pick = irandom((int32_t)candidates.size());
		if (Loot_PromoteToOmega(candidates[pick]))
			return;
		candidates.erase(candidates.begin() + pick);
	}
}

//==========================================================================
// Monster remix: in NG+, 30% of weak monsters placed in the map spawn as the next
// tougher kind, and 20% of the rest as a random kind of the same size from
// anywhere in the game. Only same-size swaps, so nothing gets stuck in tight spots.
//==========================================================================

struct ngplus_remix_t
{
	const char *from, *to;
};

static const ngplus_remix_t remixes[] = {
    {"monster_soldier_light", "monster_soldier"},
    {"monster_soldier", "monster_soldier_ss"},
    {"monster_soldier_ss", "monster_infantry"},
    {"monster_infantry", "monster_gunner"},
};

constexpr float NGPLUS_REMIX_CHANCE = 0.3f;

// Mix-up: monsters from anywhere in the campaign. A monster may become any member
// of its pool; pools hold kinds with the same footprint, and source-only kinds
// (smaller or oddly shaped) can turn into the pool but nothing turns into them.
// Bosses aren't in any pool.
struct ngplus_mix_pool_t
{
	std::vector<const char *> members;     // can be picked
	std::vector<const char *> source_only; // can be replaced, never picked
};

static const ngplus_mix_pool_t mix_pools[] = {
    // ground, 32 wide, 56-60 tall
    {{"monster_soldier_light", "monster_soldier", "monster_soldier_ss", "monster_infantry", "monster_gunner", "monster_berserk", "monster_brain"},
	 {"monster_parasite", "monster_mutant", "monster_medic"}},
    // ground, 64 wide
    {{"monster_gladiator", "monster_tank"}, {}},
    // flying, 48 wide
    {{"monster_floater", "monster_hover"}, {}},
};

constexpr float NGPLUS_MIX_CHANCE = 0.2f;

static bool NGPlus_InList(const std::vector<const char *> &list, const char *classname)
{
	for (const char *name : list)
		if (!strcmp(name, classname))
			return true;
	return false;
}

static void NGPlus_MixMonster(edict_t *ent)
{
	for (const ngplus_mix_pool_t &pool : mix_pools)
	{
		if (!NGPlus_InList(pool.members, ent->classname) && !NGPlus_InList(pool.source_only, ent->classname))
			continue;
		if (frandom() >= NGPLUS_MIX_CHANCE)
			return;

		const char *pick;
		do
			pick = pool.members[irandom((int32_t)pool.members.size())];
		while (!strcmp(pick, ent->classname));
		ent->classname = pick;
		return;
	}
}

void NGPlus_RemixMonster(edict_t *ent)
{
	// map load only (level.time 0): death spawns and Summoner adds keep their kind
	if (deathmatch->integer || game.loot_ngplus <= 0 || level.time > 0_ms || !ent->classname)
		return;

	// first a chance at one kind tougher, else a chance at a random kind of the same size
	for (const ngplus_remix_t &remix : remixes)
		if (!strcmp(ent->classname, remix.from) && frandom() < NGPLUS_REMIX_CHANCE)
		{
			ent->classname = remix.to;
			return;
		}

	NGPlus_MixMonster(ent);
}

//==========================================================================
// Alpha traits: NG+ Alphas roll one trait, two from NG+ 2 on
//==========================================================================

static const char *const trait_names[LOOT_TRAIT_COUNT] = {"Vampiric", "Volatile", "Warded", "Swift", "Summoner"};

void NGPlus_RollAlphaTraits(edict_t *monster)
{
	if (game.loot_ngplus <= 0 && !monster->loot_omega)
		return;

	const int32_t want = monster->loot_omega ? 3 : std::min(2, game.loot_ngplus); // Omegas: 3
	for (int32_t have = 0; have < want;)
	{
		const int32_t trait = 1 << irandom(LOOT_TRAIT_COUNT);
		if (monster->loot_alpha_traits & trait)
			continue;
		monster->loot_alpha_traits |= trait;
		have++;
	}
}

// Target-line tag. Two traits use short names so the line (48 chars) keeps the status text.
static const char *const trait_short_names[LOOT_TRAIT_COUNT] = {"Vamp", "Volat", "Ward", "Swift", "Summ"};

void NGPlus_TraitNames(const edict_t *monster, char (&names)[24])
{
	names[0] = '\0';
	if (!monster->loot_alpha_traits)
		return;

	int32_t count = 0;
	for (int32_t i = 0; i < LOOT_TRAIT_COUNT; i++)
		if (monster->loot_alpha_traits & (1 << i))
			count++;

	for (int32_t i = 0; i < LOOT_TRAIT_COUNT; i++)
		if (monster->loot_alpha_traits & (1 << i))
		{
			Q_strlcat(names, count > 1 ? trait_short_names[i] : trait_names[i], sizeof(names));
			Q_strlcat(names, " ", sizeof(names));
		}
}

constexpr float VOLATILE_DAMAGE = 60.f; // before monster damage scaling
constexpr float VOLATILE_RADIUS = 200.f;

THINK(ngplus_volatile_think)(edict_t *self)->void
{
	T_RadiusDamage(self, self, (float)self->dmg, nullptr, VOLATILE_RADIUS, DAMAGE_NONE, MOD_EXPLOSIVE);

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	G_FreeEdict(self);
}

// Volatile: a blast 1s after death. The bomb is its own attacker (the corpse may be gone),
// scaled like a monster hit; it remembers the Alpha's killer so blast kills credit them.
void NGPlus_OnMonsterDeath(edict_t *monster, edict_t *killer)
{
	if (!(monster->loot_alpha_traits & LOOT_TRAIT_VOLATILE))
		return;

	edict_t *bomb = G_Spawn();
	bomb->classname = "ngplus_volatile";
	bomb->s.origin = (monster->absmin + monster->absmax) * 0.5f;
	bomb->dmg = (int32_t)(VOLATILE_DAMAGE * Loot_MonsterDamageScale(monster));
	bomb->activator = (killer && killer->client) ? killer : nullptr;
	bomb->think = ngplus_volatile_think;
	bomb->nextthink = level.time + 1_sec;
	gi.sound(monster, CHAN_AUTO, gi.soundindex("weapons/grenlb1b.wav"), 1, ATTN_NORM, 0); // warning tick
}

// Kill credit for rewards: a Volatile blast counts as the player who killed that Alpha.
edict_t *NGPlus_KillCredit(edict_t *attacker)
{
	if (attacker && attacker->classname && !strcmp(attacker->classname, "ngplus_volatile"))
	{
		edict_t *killer = attacker->activator;
		return (killer && killer->inuse && killer->client) ? killer : attacker;
	}
	return attacker;
}

// Called from T_Damage right after health drops by take (after shields, armor, godmode).
// Vampiric: the monster heals half of the health it took from a living player.
// Summoner: the first time its health drops to half or below, 2 of its kind spawn
// around it; adds never summon (no chains).
void NGPlus_OnHealthLost(edict_t *targ, edict_t *attacker, int32_t take)
{
	if (take <= 0 || !attacker || deathmatch->integer)
		return;

	if (targ->client && (attacker->svflags & SVF_MONSTER) && (attacker->loot_alpha_traits & LOOT_TRAIT_VAMPIRIC) &&
	    attacker->health > 0 && targ->health + take > 0)
		attacker->health = std::min(attacker->max_health, attacker->health + take / 2);

	if ((targ->svflags & SVF_MONSTER) && (targ->loot_alpha_traits & LOOT_TRAIT_SUMMONER) && !targ->loot_summoned &&
	    !targ->loot_death_spawned && targ->health > 0 && targ->health <= targ->max_health / 2)
	{
		targ->loot_summoned = true;
		Loot_SpawnAdds(targ, 2, attacker->client ? attacker : targ->enemy);
	}
}

//==========================================================================
// Banners
//==========================================================================

// NG+ cycle banner and map modifier, a moment after the map loads.
void NGPlus_Frame(edict_t *ent)
{
	if (level.time < 2_sec)
		return;

	// q2rpg: events log, once per map visit (and again after a reload)
	if (!level.loot_events_entered)
	{
		level.loot_events_entered = true;
		const char *mod_name = NGPlus_MapModName();
		Loot_LogEvent(ent, G_Fmt("MAP enter  NG+ {}{}{}  bank ${}", game.loot_ngplus, mod_name[0] ? "  modifier: " : "", mod_name,
		                         ent->client->pers.loot_money)
		                       .data());
	}

	std::string banner;
	if (game.loot_ngplus_announce)
	{
		game.loot_ngplus_announce = false;
		banner = G_Fmt("NEW GAME+ {}\nMonsters +{} levels\nBetter loot\n", game.loot_ngplus, LOOT_NGPLUS_MONSTER_LEVELS * game.loot_ngplus).data();
	}
	if (NGPlus_MapModName()[0] && !level.loot_map_mod_announced)
	{
		level.loot_map_mod_announced = true;
		banner += G_Fmt("\nMap modifier: {}\n{}", map_mods[level.loot_map_mod].name, map_mods[level.loot_map_mod].effect).data();
	}
	if (banner.empty())
		return;

	for (auto player : active_players())
		gi.Center_Print(player, banner.c_str());
}

// Map modifier name for loot_stats ("" when none).
const char *NGPlus_MapModName()
{
	return (level.loot_map_mod > LOOT_MAPMOD_NONE && level.loot_map_mod < LOOT_MAPMOD_COUNT) ? map_mods[level.loot_map_mod].name : "";
}
