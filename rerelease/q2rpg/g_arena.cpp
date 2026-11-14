// q2rpg: arena waves, a horde mode. The vendor's Workshop item 6 sends the party to one of the
// raid arenas (shared used mask, once per unit) for up to ARENA_WAVES escalating waves. Each
// cleared wave drops a cash pile; every 3rd wave adds a gear piece (Rare+, Epic+ from wave 9,
// Legendary+ from wave 15). Leave any time through the vendor and keep what you picked up;
// dying restarts the map at wave 1. Monster drops are halved here so it can't be farmed.

#include "../g_local.h"

constexpr int32_t ARENA_MIN_LEVEL = 15;
constexpr int32_t ARENA_WAVES = 15;
constexpr gtime_t ARENA_FIRST_DELAY = 5_sec;
constexpr gtime_t ARENA_BREAK = 8_sec;
constexpr gtime_t ARENA_SPAWN_INTERVAL = 700_ms;
constexpr gtime_t ARENA_RETURN_DELAY = 30_sec;
constexpr int32_t ARENA_MAX_ALIVE = 12;
constexpr float ARENA_HEALTH_PER_WAVE = 0.10f;
constexpr float ARENA_DROP_RATE = 0.5f;

// monster pools by wave: each wave draws from its pool and the ones before it
static const char *const arena_pool_early[] = {"monster_soldier_light", "monster_soldier", "monster_infantry", "monster_flyer", "monster_parasite"};
static const char *const arena_pool_mid[] = {"monster_soldier_ss", "monster_gunner", "monster_berserk", "monster_chick", "monster_brain", "monster_floater"};
static const char *const arena_pool_late[] = {"monster_gladiator", "monster_tank", "monster_medic", "monster_mutant", "monster_hover", "monster_guncmdr"};
static const char *const arena_omegas[] = {"monster_gladiator", "monster_tank", "monster_berserk", "monster_guncmdr"};

bool Arena_InArena()
{
	return !deathmatch->integer && game.loot_arena_active && !Q_strcasecmp(level.mapname, game.loot_arena_map);
}

int32_t Arena_OpenPrice(edict_t *ent)
{
	return 500 + 50 * std::max(1, ent->client->pers.loot_level);
}

static void Arena_Banner(const std::string &text)
{
	for (auto player : active_players())
		gi.Center_Print(player, text.c_str());
}

//==========================================================================
// Vendor (Workshop item 6)
//==========================================================================

std::string Arena_VendorLine(edict_t *ent, bool &can_use)
{
	can_use = false;
	if (Arena_InArena())
	{
		can_use = true;
		return G_Fmt("Leave arena (wave {})", level.loot_arena_wave).data();
	}
	if (ent->client->pers.loot_level < ARENA_MIN_LEVEL)
		return G_Fmt("Arena opens at Lv {}", ARENA_MIN_LEVEL).data();
	can_use = true;
	return G_Fmt("Arena: {} waves  (best {})  ${}", ARENA_WAVES, game.loot_arena_best, Arena_OpenPrice(ent)).data();
}

bool Arena_CanOpen(edict_t *ent)
{
	if (deathmatch->integer || Arena_InArena() || Raid_InRaid() || Rift_InRift() || level.intermissiontime || ent->health <= 0 ||
	    ent->client->pers.loot_level < ARENA_MIN_LEVEL)
		return false;
	for (int32_t i = 0; i < RAID_MAP_COUNT; i++)
		if (!(game.loot_raid_used_mask & (1u << i)))
			return true;
	gi.Center_Print(ent, "No arenas left in this unit.\nFinish the unit to reopen them.");
	return false;
}

void Arena_Open(edict_t *ent)
{
	int32_t free_maps[32];
	int32_t free_count = 0;
	for (int32_t i = 0; i < RAID_MAP_COUNT; i++)
		if (!(game.loot_raid_used_mask & (1u << i)))
			free_maps[free_count++] = i;
	if (!free_count)
		return;
	const int32_t pick = free_maps[irandom(free_count)];

	Rift_SaveReturnSpot(ent);
	game.loot_arena_active = true;
	game.loot_raid_used_mask |= 1u << pick;
	Q_strlcpy(game.loot_arena_map, raid_maps[pick], sizeof(game.loot_arena_map));

	Loot_LogEvent(ent, G_Fmt("ARENA open  {}", game.loot_arena_map).data());
	Rift_CloseMenus();
	BeginIntermission(CreateTargetChangeLevel(game.loot_arena_map));
}

// dev: loot_arena [wave] opens the arena now (no level or price check); in the arena, jumps to that wave's break
void Arena_CmdOpen(edict_t *ent)
{
	if (Arena_InArena())
	{
		const int32_t wave = gi.argc() >= 2 ? std::clamp(atoi(gi.argv(1)), 1, ARENA_WAVES) : level.loot_arena_wave + 1;
		level.loot_arena_wave = wave - 1;
		level.loot_arena_to_spawn = 0;
		level.loot_arena_break_end = level.time + 1_sec;
		return;
	}
	if (deathmatch->integer || Raid_InRaid() || Rift_InRift() || level.intermissiontime)
		return;
	if (game.loot_raid_used_mask == (1u << RAID_MAP_COUNT) - 1)
		game.loot_raid_used_mask = 0;
	Arena_Open(ent);
}

void Arena_Leave(edict_t *ent)
{
	if (!Arena_InArena() || level.intermissiontime)
		return;
	Loot_LogEvent(ent, G_Fmt("ARENA left  wave {}", level.loot_arena_wave).data());
	Rift_CloseMenus();
	BeginIntermission(CreateTargetChangeLevel(Rift_ReturnChangemap()));
}

//==========================================================================
// Waves
//==========================================================================

static int32_t Arena_WaveSize(int32_t wave)
{
	return 4 + 2 * wave;
}

static const char *Arena_PickMonster(int32_t wave)
{
	const int32_t early = (int32_t)q_countof(arena_pool_early), mid = (int32_t)q_countof(arena_pool_mid), late = (int32_t)q_countof(arena_pool_late);
	const int32_t pool_size = wave >= 8 ? early + mid + late : wave >= 4 ? early + mid
	                                                                     : early;
	int32_t pick = irandom(pool_size);
	if (pick < early)
		return arena_pool_early[pick];
	pick -= early;
	if (pick < mid)
		return arena_pool_mid[pick];
	return arena_pool_late[pick - mid];
}

static edict_t *Arena_FirstLivePlayer()
{
	for (auto player : active_players())
		if (player->health > 0)
			return player;
	return nullptr;
}

static void Arena_SpawnOne(bool omega)
{
	level.loot_arena_to_spawn--; // counts the try, so a spot that never fits can't stall the wave
	vec3_t spot;
	if (!Raid_FindSpot({-24, -24, -24}, {24, 24, 40}, false, spot))
		return;
	const vec3_t angles = {0, frandom(360.f), 0};
	edict_t *monster = omega ? Loot_SpawnOmega(spot, angles, arena_omegas[irandom((int32_t)q_countof(arena_omegas))])
	                         : CreateMonster(spot, angles, Arena_PickMonster(level.loot_arena_wave));
	if (!monster || !monster->inuse)
		return;

	const float health_mult = 1.0f + ARENA_HEALTH_PER_WAVE * (level.loot_arena_wave - 1);
	monster->health = std::max(1, (int32_t)(monster->health * health_mult));
	monster->max_health = monster->health;
	SpawnGrow_Spawn(monster->s.origin, 24.f, 64.f);
	if (edict_t *target = Arena_FirstLivePlayer())
	{
		monster->enemy = target;
		FoundTarget(monster);
	}
}

static void Arena_StartWave()
{
	level.loot_arena_wave++;
	level.loot_arena_to_spawn = Arena_WaveSize(level.loot_arena_wave);
	level.loot_arena_next_spawn = level.time;
	level.loot_arena_break_end = 0_ms;
	gi.sound(world, CHAN_AUTO, gi.soundindex("world/klaxon2.wav"), 1, ATTN_NONE, 0);
	Arena_Banner(G_Fmt("WAVE {}/{}{}", level.loot_arena_wave, ARENA_WAVES, level.loot_arena_wave % 5 == 0 ? "\nAn Alpha Omega joins the fight" : "").data());

	// every 5th wave: an Omega first
	if (level.loot_arena_wave % 5 == 0)
	{
		level.loot_arena_to_spawn++;
		Arena_SpawnOne(true);
	}
}

static void Arena_WaveCleared(edict_t *ent)
{
	const int32_t wave = level.loot_arena_wave;
	game.loot_arena_best = std::max(game.loot_arena_best, wave);
	Loot_LogEvent(ent, G_Fmt("ARENA wave {} cleared", wave).data());

	// cash every wave, gear every 3rd
	const float cash = (50.f + 20.f * Loot_ZoneLevel()) * 0.5f * wave * NGPlus_CashMult();
	Vendor_SpawnCashPile(ent, cash, nullptr);
	const bool gear_wave = wave % 3 == 0;
	if (gear_wave)
		Loot_MissionDrop(ent, wave >= 15 ? LOOT_RARITY_LEGENDARY : wave >= 9 ? LOOT_RARITY_EPIC
		                                                                     : LOOT_RARITY_RARE);

	if (wave >= ARENA_WAVES)
	{
		level.loot_arena_done = true;
		level.loot_arena_return_time = level.time + ARENA_RETURN_DELAY;
		gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/secret.wav"), 1, ATTN_NONE, 0);
		Arena_Banner(G_Fmt("ARENA CHAMPION\nAll {} waves cleared\nBack in {:.0f}s", ARENA_WAVES, ARENA_RETURN_DELAY.seconds()).data());
		Loot_LogEvent(ent, "ARENA champion");
		return;
	}
	level.loot_arena_break_end = level.time + ARENA_BREAK;
	Arena_Banner(G_Fmt("Wave {} cleared{}\nNext wave in {:.0f}s  (vendor item 6: leave)", wave, gear_wave ? "  +gear" : "", ARENA_BREAK.seconds()).data());
}

// Loot_SetStats, every player every frame; level logic runs on the first live player
void Arena_PlayerFrame(edict_t *ent)
{
	if (!Arena_InArena() || level.intermissiontime || ent->health <= 0 || ent != Arena_FirstLivePlayer())
		return;

	if (level.loot_arena_done)
	{
		if (level.time >= level.loot_arena_return_time)
			Arena_Leave(ent);
		return;
	}

	// first frame: halve monster drops, then a short lead-in
	if (level.loot_arena_wave == 0 && level.loot_arena_break_end == 0_ms)
	{
		level.loot_droprate_mult = ARENA_DROP_RATE;
		level.loot_arena_break_end = level.time + ARENA_FIRST_DELAY;
		Arena_Banner(G_Fmt("ARENA\n{} waves. Leave any time with vendor item 6", ARENA_WAVES).data());
		return;
	}

	// break between waves
	if (level.loot_arena_break_end != 0_ms)
	{
		if (level.time >= level.loot_arena_break_end)
			Arena_StartWave();
		return;
	}

	// trickle the wave in, a few alive at a time
	if (level.loot_arena_to_spawn > 0 && level.time >= level.loot_arena_next_spawn && Raid_LiveAdds() < ARENA_MAX_ALIVE)
	{
		Arena_SpawnOne(false);
		level.loot_arena_next_spawn = level.time + ARENA_SPAWN_INTERVAL;
	}

	if (level.loot_arena_to_spawn <= 0 && Raid_LiveAdds() == 0)
		Arena_WaveCleared(ent);
}
