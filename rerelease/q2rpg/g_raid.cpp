// q2rpg: the raid boss, a Lv 60 endgame fight. The vendor's Workshop item 5 opens a raid: a
// deathmatch arena (no monsters, not part of a campaign unit) where a huge Makron arrives.
// Phases at 75 / 50 / 25% health:
//   1  calls in 2 Alphas and raises an element shield: only that element hits fully
//   2  enrages (Swift trait) and sends a ground shockwave every 8 s: jump over it
//   3  the shield drops, Alpha waves keep coming, and players deal +20% damage to it
// Killing it pays a loot burst (2 Legendaries, a relic, a class mod, maybe a raid unique) and
// counts as this NG+ cycle's clear. Leaving uses the rift return path (Rift_OnExitLevel).
// Each arena map is used once per campaign unit, like rift maps.

#include "../g_local.h"

//==========================================================================
// Tuning
//==========================================================================

const char *const raid_maps[] = {"q2dm1", "q2dm2", "q2dm4", // q2dm3 dropped: lava arena
                                 "q2dm5", "q2dm6", "q2dm7", "q2dm8"};
const int32_t RAID_MAP_COUNT = (int32_t)q_countof(raid_maps);
static_assert(q_countof(raid_maps) <= 32, "arena used mask and free_maps are 32 wide");

constexpr int32_t RAID_MIN_LEVEL = LOOT_MAX_LEVEL + LOOT_NGPLUS_LEVEL_CAP_BONUS; // 60
constexpr int32_t RAID_PRICE = 25000;
constexpr const char *RAID_BOSS_CLASS = "monster_makron";
constexpr const char *RAID_BOSS_NAME = "The Hollow King";
constexpr float RAID_HEALTH_MULT = 4.0f; // on top of normal boss scaling
constexpr float RAID_COOP_HEALTH_PER_PLAYER = 0.75f;
constexpr gtime_t RAID_ARRIVAL_DELAY = 5_sec;
constexpr gtime_t RAID_RETURN_DELAY = 45_sec;
constexpr gtime_t RAID_SHOCK_INTERVAL = 8_sec;
constexpr gtime_t RAID_SHOCK_WARNING = 1500_ms;
constexpr float RAID_SHOCK_RANGE = 1000.f;
constexpr float RAID_SHOCK_HEALTH_SHARE = 0.30f; // of the player's max health
constexpr gtime_t RAID_WAVE_INTERVAL = 12_sec;
constexpr int32_t RAID_MAX_ADDS = 6;
constexpr float RAID_SHIELD_OFF_ELEMENT_MULT = 0.5f;
constexpr float RAID_LAST_STAND_MULT = 1.2f;
static const char *const raid_add_classes[] = {"monster_gladiator", "monster_gunner", "monster_infantry", "monster_soldier_ss", "monster_berserk", "monster_arachnid"};

//==========================================================================
// State
//==========================================================================

bool Raid_InRaid()
{
	return !deathmatch->integer && game.loot_raid_active && !Q_strcasecmp(level.mapname, game.loot_raid_map);
}

static bool Raid_ClearedThisCycle()
{
	return game.loot_raid_cleared_cycle == game.loot_ngplus + 1;
}

static edict_t *Raid_Boss()
{
	edict_t *boss = level.loot_raid_boss;
	if (!boss || !boss->inuse || boss->spawn_count != level.loot_raid_boss_count)
		return nullptr;
	return boss;
}

bool Raid_IsBoss(const edict_t *monster)
{
	return monster && Raid_InRaid() && monster == Raid_Boss();
}

// damage players deal to the boss: element shield and last stand
float Raid_DamageMult(const edict_t *targ, loot_element_t hit_element)
{
	if (!Raid_IsBoss(targ))
		return 1.0f;
	if (level.loot_raid_phase >= 3)
		return RAID_LAST_STAND_MULT;
	if (level.loot_raid_shield_element && hit_element != level.loot_raid_shield_element)
		return RAID_SHIELD_OFF_ELEMENT_MULT;
	return 1.0f;
}

//==========================================================================
// Vendor line and actions (g_vendor.cpp Workshop item 5)
//==========================================================================

std::string Raid_VendorLine(edict_t *ent, bool &can_use)
{
	can_use = false;
	if (Raid_InRaid())
	{
		can_use = true;
		return level.loot_raid_cleared ? "Leave raid now" : "Leave raid (no reward)";
	}
	if (ent->client->pers.loot_level < RAID_MIN_LEVEL)
		return G_Fmt("Raid opens at Lv {}", RAID_MIN_LEVEL).data();
	if (Raid_ClearedThisCycle())
		return "Raid cleared this NG+ cycle";
	can_use = true;
	return G_Fmt("Raid: {}  ${}", RAID_BOSS_NAME, RAID_PRICE).data();
}

int32_t Raid_OpenPrice()
{
	return RAID_PRICE;
}

//==========================================================================
// Arena maps, shared by the raid and the arena waves (g_arena.cpp)
//==========================================================================

// no rift / raid / arena already running, and an unused arena map this unit
bool Arena_TripPossible(edict_t *ent)
{
	if (deathmatch->integer || Loot_OnSideTrip() || level.intermissiontime || ent->health <= 0)
		return false;
	for (int32_t i = 0; i < RAID_MAP_COUNT; i++)
		if (!(game.loot_raid_used_mask & (1u << i)))
			return true;
	gi.Center_Print(ent, "No arenas left in this unit.\nFinish the unit to reopen them.");
	return false;
}

// pick an unused arena map, remember the way home, and go; active / map_name are the trip's game state
void Arena_TravelTo(edict_t *ent, bool &active, char (&map_name)[MAX_QPATH], const char *log_tag)
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
	active = true;
	game.loot_raid_used_mask |= 1u << pick;
	Q_strlcpy(map_name, raid_maps[pick], sizeof(map_name));

	Loot_LogEvent(ent, G_Fmt("{} open  {}", log_tag, map_name).data());
	Rift_CloseMenus();
	BeginIntermission(CreateTargetChangeLevel(map_name));
}

// back to where the trip started (Rift_OnExitLevel clears the trip state)
void Arena_TravelHome(edict_t *ent, const std::string &log_line)
{
	Loot_LogEvent(ent, log_line);
	Rift_CloseMenus();
	BeginIntermission(CreateTargetChangeLevel(Rift_ReturnChangemap()));
}

bool Raid_CanOpen(edict_t *ent)
{
	if (ent->client->pers.loot_level < RAID_MIN_LEVEL || Raid_ClearedThisCycle())
		return false;
	return Arena_TripPossible(ent);
}

void Raid_Open(edict_t *ent)
{
	Arena_TravelTo(ent, game.loot_raid_active, game.loot_raid_map, "RAID");
}

// dev: loot_raid opens a raid now, skipping the level, price and once-per-cycle checks
void Raid_CmdOpen(edict_t *ent)
{
	if (deathmatch->integer || Raid_InRaid() || Rift_InRift() || level.intermissiontime)
		return;
	game.loot_raid_cleared_cycle = 0;
	if (game.loot_raid_used_mask == (1u << RAID_MAP_COUNT) - 1)
		game.loot_raid_used_mask = 0;
	Raid_Open(ent);
}

void Raid_Leave(edict_t *ent)
{
	if (!Raid_InRaid() || level.intermissiontime)
		return;
	Arena_TravelHome(ent, G_Fmt("RAID left{}", level.loot_raid_cleared ? "  (cleared)" : "").data());
}

//==========================================================================
// Spawning
//==========================================================================

// the arena spot farthest from the players that fits the monster's box
bool Raid_FindSpot(const vec3_t &mins, const vec3_t &maxs, bool farthest, vec3_t &spot, float min_player_distance)
{
	float best_score = -1.f;
	bool found = false;
	for (const char *spot_class : {"info_player_deathmatch", "info_player_start"})
	{
		for (edict_t *e = nullptr; (e = G_FindByString<&edict_t::classname>(e, spot_class));)
		{
			vec3_t candidate;
			if (!FindSpawnPoint(e->s.origin, mins, maxs, candidate, 64.f, true))
				continue;
			float nearest_player = 99999.f;
			for (auto player : active_players())
				if (player->health > 0)
					nearest_player = std::min(nearest_player, (player->s.origin - candidate).length());
			if (nearest_player < min_player_distance)
				continue;
			const float score = farthest ? nearest_player : frandom(1.f);
			if (score > best_score)
			{
				best_score = score;
				spot = candidate;
				found = true;
			}
		}
	}
	return found;
}

static void Raid_SpawnBoss()
{
	vec3_t spot;
	if (!Raid_FindSpot({-30, -30, 0}, {30, 30, 90}, true, spot))
		return;
	edict_t *target = Loot_FirstLivePlayer();
	const float yaw = target ? vectoyaw(target->s.origin - spot) : 0.f;
	edict_t *boss = CreateMonster(spot, {0, yaw, 0}, RAID_BOSS_CLASS);
	if (!boss || !boss->inuse)
		return;

	// on top of normal boss scaling: x4, +75% per extra coop player
	int32_t players = 0;
	for (auto player : active_players())
		players++;
	const float health_mult = RAID_HEALTH_MULT * (1.f + RAID_COOP_HEALTH_PER_PLAYER * std::max(0, players - 1));
	boss->health = std::max(1, (int32_t)(boss->health * health_mult));
	boss->max_health = boss->health;
	boss->loot_hud_omega = true; // red bar and Omega tag only: not an Alpha or Omega for speed, drops or credit

	level.loot_raid_boss = boss;
	level.loot_raid_boss_count = boss->spawn_count;
	level.loot_raid_start_time = level.time;
	if (target)
	{
		boss->enemy = target;
		FoundTarget(boss);
	}
	gi.sound(boss, CHAN_AUTO, gi.soundindex("world/klaxon2.wav"), 1, ATTN_NONE, 0);
	Loot_BannerAll(G_Fmt("{} HAS COME", RAID_BOSS_NAME).data());
}

int32_t Raid_LiveAdds()
{
	int32_t adds = 0;
	for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; i++)
	{
		const edict_t *e = &g_edicts[i];
		if (Loot_IsLiveMonster(e) && e != level.loot_raid_boss)
			adds++;
	}
	return adds;
}

static void Raid_SpawnAdds(int32_t count)
{
	edict_t *target = Loot_FirstLivePlayer();
	for (int32_t i = 0; i < count && Raid_LiveAdds() < RAID_MAX_ADDS; i++)
	{
		// a few tries per add: Alphas are scaled up, so a spot or kind that doesn't fit gets another go
		edict_t *add = nullptr;
		for (int32_t attempt = 0; attempt < 4 && !add; attempt++)
		{
			const char *classname = raid_add_classes[irandom((int32_t)q_countof(raid_add_classes))];
			vec3_t spot;
			const bool big_box = attempt < 2; // Alpha-sized first, then a regular box on tight maps
			if (!big_box && !strcmp(classname, "monster_arachnid"))
				classname = "monster_gladiator"; // the Arachnid needs the big box
			if (Raid_FindSpot(big_box ? vec3_t{-40, -40, -24} : vec3_t{-24, -24, -24}, big_box ? vec3_t{40, 40, 72} : vec3_t{24, 24, 40}, false, spot))
				add = Loot_SpawnAlpha(spot, {0, frandom(360.f), 0}, classname);
		}
		if (!add)
			continue;
		SpawnGrow_Spawn(add->s.origin, 24.f, 64.f);
		if (target)
		{
			add->enemy = target;
			FoundTarget(add);
		}
	}
}

//==========================================================================
// Fight
//==========================================================================

static void Raid_Shockwave(edict_t *boss)
{
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1_BIG);
	gi.WritePosition(boss->s.origin);
	gi.multicast(boss->s.origin, MULTICAST_PHS, false);
	gi.sound(boss, CHAN_AUTO, gi.soundindex("weapons/rocklx1a.wav"), 1, ATTN_NONE, 0);

	for (auto player : active_players())
	{
		// airborne players jump over it
		if (player->health <= 0 || !player->groundentity || (player->s.origin - boss->s.origin).length() > RAID_SHOCK_RANGE)
			continue;
		const int32_t damage = std::max(1, (int32_t)(player->max_health * RAID_SHOCK_HEALTH_SHARE));
		const vec3_t push = (player->s.origin - boss->s.origin).normalized();
		// from the world, so monster damage scaling doesn't stack on top
		T_Damage(player, boss, world, push, player->s.origin, vec3_origin, damage, 300, DAMAGE_RADIUS, MOD_EXPLOSIVE);
	}
}

static void Raid_EnterPhase(edict_t *boss, int32_t phase)
{
	level.loot_raid_phase = phase;
	edict_t *opener = Loot_FirstLivePlayer();
	if (opener)
		Loot_LogEvent(opener, G_Fmt("RAID phase {}", phase).data());

	if (phase == 1)
	{
		level.loot_raid_shield_element = 1 + irandom(LOOT_ELEM_COUNT - 1);
		Raid_SpawnAdds(2);
		Loot_BannerAll(G_Fmt("{} raises a {} shield\nOnly {} hits fully", RAID_BOSS_NAME, Loot_ElementName((loot_element_t)level.loot_raid_shield_element),
		                     Loot_ElementName((loot_element_t)level.loot_raid_shield_element))
		                   .data());
	}
	else if (phase == 2)
	{
		boss->loot_alpha_traits |= LOOT_TRAIT_SWIFT;
		level.loot_raid_next_shock = level.time + RAID_SHOCK_INTERVAL;
		level.loot_raid_shock_warned = false;
		Loot_BannerAll(G_Fmt("{} is ENRAGED\nJump over the shockwaves!", RAID_BOSS_NAME).data());
	}
	else if (phase == 3)
	{
		level.loot_raid_shield_element = 0;
		level.loot_raid_next_wave = level.time;
		Loot_BannerAll(G_Fmt("LAST STAND\n{}'s shield is down: +20% damage", RAID_BOSS_NAME).data());
	}
}

static void Raid_Reward(edict_t *ent, const vec3_t &where)
{
	level.loot_raid_cleared = true;
	level.loot_raid_return_time = level.time + RAID_RETURN_DELAY;
	level.loot_raid_shield_element = 0;
	game.loot_raid_cleared_cycle = game.loot_ngplus + 1;
	Loot_RaidDrops(where);

	const float clear_seconds = (level.time - level.loot_raid_start_time).seconds();
	Loot_LogEvent(ent, G_Fmt("RAID cleared  {:.0f}s", clear_seconds).data());
	gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/secret.wav"), 1, ATTN_NONE, 0);
	Loot_BannerAll(G_Fmt("{} FALLS\nCleared in {}:{:02}\nGrab the loot: back in {:.0f}s", RAID_BOSS_NAME, (int32_t)clear_seconds / 60,
	                     (int32_t)clear_seconds % 60, RAID_RETURN_DELAY.seconds())
	                   .data());
}

// Loot_SetStats, every player every frame; level logic runs on the first live player
void Raid_PlayerFrame(edict_t *ent)
{
	if (!Raid_InRaid() || level.intermissiontime || ent->health <= 0 || ent != Loot_FirstLivePlayer())
		return;

	if (level.loot_raid_cleared)
	{
		if (level.time >= level.loot_raid_return_time)
			Raid_Leave(ent);
		return;
	}

	// arrival
	if (level.loot_raid_spawn_time == 0_ms)
	{
		level.loot_raid_spawn_time = level.time + RAID_ARRIVAL_DELAY;
		Loot_BannerAll(G_Fmt("RAID\n{} is coming", RAID_BOSS_NAME).data());
		return;
	}
	if (!level.loot_raid_boss)
	{
		if (level.time >= level.loot_raid_spawn_time)
			Raid_SpawnBoss();
		return;
	}

	edict_t *boss = Raid_Boss();
	if (!boss || boss->health <= 0 || boss->deadflag)
	{
		const vec3_t where = boss ? boss->s.origin : ent->s.origin;
		level.loot_raid_boss = nullptr;
		Raid_Reward(ent, where);
		return;
	}

	// phases by health share
	const float health_share = (float)boss->health / std::max(1, boss->max_health);
	const int32_t phase = health_share <= 0.25f ? 3 : health_share <= 0.5f ? 2
	                                              : health_share <= 0.75f  ? 1
	                                                                       : 0;
	while (level.loot_raid_phase < phase)
		Raid_EnterPhase(boss, level.loot_raid_phase + 1);

	// shockwaves from phase 2
	if (level.loot_raid_phase >= 2)
	{
		if (!level.loot_raid_shock_warned && level.time >= level.loot_raid_next_shock - RAID_SHOCK_WARNING)
		{
			level.loot_raid_shock_warned = true;
			gi.sound(boss, CHAN_AUTO, gi.soundindex("world/klaxon2.wav"), 1, ATTN_NONE, 0);
			Loot_BannerAll("SHOCKWAVE - JUMP!");
		}
		if (level.time >= level.loot_raid_next_shock)
		{
			Raid_Shockwave(boss);
			level.loot_raid_next_shock = level.time + RAID_SHOCK_INTERVAL;
			level.loot_raid_shock_warned = false;
		}
	}

	// add waves in the last stand
	if (level.loot_raid_phase >= 3 && level.time >= level.loot_raid_next_wave)
	{
		Raid_SpawnAdds(2);
		level.loot_raid_next_wave = level.time + RAID_WAVE_INTERVAL;
	}
}
