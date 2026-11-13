// q2rpg: Rifts, the Lv 60 endgame. The vendor's Workshop opens a rift: a random Quake 64
// map (not part of the campaign, so its hub save can't clash with campaign maps) at the
// rift tier. Monsters there scale with the tier. Killing RIFT_CLEAR_SHARE of them summons
// an Omega guardian; killing it pays a loot burst and unlocks the next tier. Leaving (by
// the vendor, the map's own exit or the auto return after a clear) sends the party back to
// the exact spot they left. Each pool map is used once per campaign unit: the engine keeps
// a visited map's state until the unit ends, so a second visit would find it already cleared.

#include "../g_local.h"

//==========================================================================
// Tuning
//==========================================================================

static const char *const rift_maps[] = {
    "q64/outpost",
    "q64/complex",
    "q64/intel",
    "q64/comm",
    "q64/orbit",
    "q64/station",
    "q64/ship",
    "q64/cargo",
    "q64/mines",
    "q64/storage",
    "q64/organic",
    "q64/process",
    "q64/geo-stat",
    "q64/jail",
    "q64/lab",
    "q64/bio",
    "q64/conduits",
    "q64/core",
};
constexpr int32_t RIFT_MAP_COUNT = (int32_t)q_countof(rift_maps);
static_assert(RIFT_MAP_COUNT <= 32, "rift used mask is 32 bits");

constexpr int32_t RIFT_MIN_LEVEL = LOOT_MAX_LEVEL + LOOT_NGPLUS_LEVEL_CAP_BONUS; // 60
constexpr float RIFT_CLEAR_SHARE = 0.7f;                                         // of the map's monsters, then the guardian
constexpr gtime_t RIFT_RETURN_DELAY = 30_sec;                                    // after the guardian dies: time to grab loot
constexpr float RIFT_HEALTH_PER_TIER = 0.15f;                                    // monster health +15% per tier above 1
constexpr float RIFT_DAMAGE_PER_TIER = 0.08f;                                    // monster damage +8% per tier above 1
constexpr float RIFT_LUCK_PER_TIER = 0.25f;
constexpr float RIFT_ELITE_PER_TIER = 0.10f; // Alpha chance x(1 + 0.1 per tier)

// guardians: tough single monsters that fit most rooms
static const char *const rift_guardians[] = {"monster_tank_commander", "monster_gladiator", "monster_guncmdr", "monster_berserk", "monster_chick_heat"};

//==========================================================================
// State
//==========================================================================

bool Rift_InRift()
{
	return !deathmatch->integer && game.loot_rift_active && !Q_strcasecmp(level.mapname, game.loot_rift_map);
}

static int32_t Rift_Tier()
{
	return std::max(1, game.loot_rift_tier);
}

int32_t Rift_LevelBonus()
{
	return Rift_InRift() ? (Rift_Tier() - 1) / 2 : 0; // +1 monster level every 2 tiers
}

float Rift_HealthMult()
{
	return Rift_InRift() ? 1.f + RIFT_HEALTH_PER_TIER * (Rift_Tier() - 1) : 1.f;
}

float Rift_DamageMult()
{
	return Rift_InRift() ? 1.f + RIFT_DAMAGE_PER_TIER * (Rift_Tier() - 1) : 1.f;
}

float Rift_LuckBonus()
{
	return Rift_InRift() ? RIFT_LUCK_PER_TIER * Rift_Tier() : 0.f;
}

float Rift_EliteMult()
{
	return Rift_InRift() ? 1.f + RIFT_ELITE_PER_TIER * Rift_Tier() : 1.f;
}

int32_t Rift_OpenPrice()
{
	return 5000 + 2500 * Rift_Tier();
}

//==========================================================================
// Vendor line and actions (g_vendor.cpp Workshop item 4)
//==========================================================================

std::string Rift_VendorLine(edict_t *ent, bool &can_use)
{
	can_use = false;
	if (Rift_InRift())
	{
		can_use = true;
		return level.loot_rift_cleared ? "Leave rift now" : "Leave rift (no reward)";
	}
	if (ent->client->pers.loot_level < RIFT_MIN_LEVEL)
		return G_Fmt("Rifts open at Lv {}", RIFT_MIN_LEVEL).data();
	can_use = true;
	return G_Fmt("Open rift: tier {}  (best {})  ${}", Rift_Tier(), game.loot_rift_best, Rift_OpenPrice()).data();
}

// "map$spot" back to where the rift was opened
static const char *Rift_ReturnChangemap()
{
	static char changemap[MAX_QPATH * 2];
	if (game.loot_rift_return_spot[0])
		G_FmtTo(changemap, "{}${}", game.loot_rift_return_map, game.loot_rift_return_spot);
	else
		Q_strlcpy(changemap, game.loot_rift_return_map, sizeof(changemap));
	return changemap;
}

static void Rift_CloseMenus()
{
	for (auto player : active_players())
	{
		player->client->loot_shop_open = false;
		player->client->loot_showinv = false;
		player->client->loot_showskills = false;
		player->client->loot_showchallenges = false;
	}
}

// Leave now: intermission, then ExitLevel sends everyone back (Rift_OnExitLevel).
void Rift_Leave(edict_t *ent)
{
	if (!Rift_InRift() || level.intermissiontime)
		return;
	Loot_LogEvent(ent, G_Fmt("RIFT left  tier {}{}", Rift_Tier(), level.loot_rift_cleared ? "  (cleared)" : "").data());
	Rift_CloseMenus();
	BeginIntermission(CreateTargetChangeLevel(Rift_ReturnChangemap()));
}

// Returns false when no rift can open (the vendor then charges nothing).
bool Rift_CanOpen(edict_t *ent)
{
	if (deathmatch->integer || Rift_InRift() || level.intermissiontime || ent->health <= 0 || ent->client->pers.loot_level < RIFT_MIN_LEVEL)
		return false;
	for (int32_t i = 0; i < RIFT_MAP_COUNT; i++)
		if (!(game.loot_rift_used_mask & (1u << i)))
			return true;
	gi.Center_Print(ent, "No rifts left in this unit.\nFinish the unit to reopen them.");
	return false;
}

void Rift_Open(edict_t *ent)
{
	// pick an unused pool map
	int32_t free_maps[RIFT_MAP_COUNT];
	int32_t free_count = 0;
	for (int32_t i = 0; i < RIFT_MAP_COUNT; i++)
		if (!(game.loot_rift_used_mask & (1u << i)))
			free_maps[free_count++] = i;
	if (!free_count)
		return;
	const int32_t pick = free_maps[irandom(free_count)];

	// remember where to come back to: this map, a start spot that exists here, and the exact position
	Q_strlcpy(game.loot_rift_return_map, level.mapname, sizeof(game.loot_rift_return_map));
	game.loot_rift_return_spot[0] = '\0';
	bool plain_start = false;
	for (edict_t *spot = nullptr; (spot = G_FindByString<&edict_t::classname>(spot, "info_player_start")) != nullptr;)
	{
		if (!spot->targetname)
		{
			plain_start = true;
			break;
		}
		if (!game.loot_rift_return_spot[0])
			Q_strlcpy(game.loot_rift_return_spot, spot->targetname, sizeof(game.loot_rift_return_spot));
	}
	if (plain_start)
		game.loot_rift_return_spot[0] = '\0';
	game.loot_rift_return_origin = ent->s.origin;
	game.loot_rift_return_angles = ent->client->v_angle;

	game.loot_rift_tier = Rift_Tier();
	game.loot_rift_active = true;
	game.loot_rift_used_mask |= 1u << pick;
	Q_strlcpy(game.loot_rift_map, rift_maps[pick], sizeof(game.loot_rift_map));

	Loot_LogEvent(ent, G_Fmt("RIFT open  tier {}  {}", game.loot_rift_tier, game.loot_rift_map).data());
	Rift_CloseMenus();
	BeginIntermission(CreateTargetChangeLevel(game.loot_rift_map));
}

//==========================================================================
// Map changes (ExitLevel)
//==========================================================================

// Leaving the rift map by any route goes back to where the rift was opened.
// A new unit ('*') frees the pool maps again.
void Rift_OnExitLevel()
{
	if (deathmatch->integer || !level.changemap)
		return;

	if (Rift_InRift())
	{
		game.loot_rift_active = false;
		game.loot_rift_return_pending = true;
		level.changemap = Rift_ReturnChangemap();
		return;
	}

	if (level.changemap[0] == '*' || strstr(level.changemap, "+*"))
		game.loot_rift_used_mask = 0;
}

//==========================================================================
// Per frame (Loot_SetStats)
//==========================================================================

// Back on the map the rift was opened from: put the party where the leader stood.
static void Rift_ReturnPlayers()
{
	int32_t index = 0;
	for (auto player : active_players())
	{
		if (player->health <= 0)
			continue;
		gi.unlinkentity(player);
		player->s.origin = game.loot_rift_return_origin + vec3_t{(float)(index % 2) * 40.f, (float)(index / 2) * 40.f, 0}; // coop: side by side
		player->s.old_origin = player->s.origin;
		player->velocity = {};
		player->client->ps.pmove.pm_time = 160;
		player->client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;
		player->s.event = EV_PLAYER_TELEPORT;
		player->client->ps.pmove.delta_angles = game.loot_rift_return_angles - player->client->resp.cmd_angles;
		player->s.angles = {0, game.loot_rift_return_angles[YAW], 0};
		player->client->ps.viewangles = game.loot_rift_return_angles;
		player->client->v_angle = game.loot_rift_return_angles;
		gi.linkentity(player);
		index++;
	}
	game.loot_rift_return_pending = false;
}

// Spawns the guardian in front of the player, trying 8 directions until one fits.
static edict_t *Rift_SpawnGuardian(edict_t *ent)
{
	const char *classname = rift_guardians[irandom((int32_t)q_countof(rift_guardians))];
	for (int32_t attempt = 0; attempt < 8; attempt++)
	{
		const float yaw = ent->client->v_angle[YAW] + 45.f * attempt;
		vec3_t forward;
		AngleVectors({0, yaw, 0}, forward, nullptr, nullptr);
		const vec3_t start = ent->s.origin + vec3_t{0, 0, 16};
		const trace_t tr = gi.trace(start, vec3_origin, vec3_origin, start + forward * 256.f, ent, MASK_MONSTERSOLID);
		const vec3_t spot = tr.endpos - forward * 64.f;
		if ((spot - ent->s.origin).length() < 96.f)
			continue;

		edict_t *guardian = Loot_SpawnOmega(spot, {0, yaw + 180.f, 0}, classname);
		if (!guardian)
			continue;
		guardian->enemy = ent;
		FoundTarget(guardian);
		return guardian;
	}
	return nullptr;
}

static void Rift_Reward(edict_t *ent, const vec3_t &where)
{
	const int32_t tier = Rift_Tier();
	Loot_RiftDrops(where, 3 + tier / 3, tier);

	game.loot_rift_best = std::max(game.loot_rift_best, tier);
	game.loot_rift_tier = tier + 1;
	level.loot_rift_cleared = true;
	level.loot_rift_return_time = level.time + RIFT_RETURN_DELAY;

	Loot_LogEvent(ent, G_Fmt("RIFT cleared  tier {}", tier).data());
	gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/secret.wav"), 1, ATTN_NONE, 0);
	for (auto player : active_players())
		gi.Center_Print(player, G_Fmt("RIFT CLEARED - tier {} unlocked\nGrab the loot: back in {:.0f}s", tier + 1, RIFT_RETURN_DELAY.seconds()).data());
}

void Rift_PlayerFrame(edict_t *ent)
{
	if (deathmatch->integer || level.intermissiontime || ent->health <= 0)
		return;

	if (game.loot_rift_return_pending && !Q_strcasecmp(level.mapname, game.loot_rift_return_map))
	{
		Rift_ReturnPlayers();
		return;
	}

	if (!Rift_InRift())
		return;

	// level logic runs once per frame, on the first live player
	for (auto player : active_players())
	{
		if (player->health > 0)
		{
			if (player != ent)
				return;
			break;
		}
	}

	if (!level.loot_rift_announced)
	{
		level.loot_rift_announced = true;
		for (auto player : active_players())
			gi.Center_Print(player, G_Fmt("RIFT - TIER {}\nKill {:.0f}% of the monsters to summon the guardian", Rift_Tier(), RIFT_CLEAR_SHARE * 100.f).data());
	}

	if (level.loot_rift_cleared)
	{
		if (level.time >= level.loot_rift_return_time)
			Rift_Leave(ent);
		return;
	}

	if (!level.loot_rift_guardian)
	{
		const int32_t total = std::max(1, level.total_monsters);
		const float share = (float)level.killed_monsters / total;
		const int32_t step = std::min(10, (int32_t)(share / RIFT_CLEAR_SHARE * 10.f)); // tenths of the way to the guardian
		if (step > level.loot_rift_progress_shown && step < 10)
		{
			level.loot_rift_progress_shown = step;
			for (auto player : active_players())
				gi.Center_Print(player, G_Fmt("Rift {}%", step * 10).data());
		}
		if (share >= RIFT_CLEAR_SHARE)
		{
			edict_t *guardian = Rift_SpawnGuardian(ent);
			if (guardian)
			{
				level.loot_rift_guardian = guardian;
				level.loot_rift_guardian_count = guardian->spawn_count;
				gi.sound(ent, CHAN_AUTO, gi.soundindex("world/klaxon2.wav"), 1, ATTN_NONE, 0);
				for (auto player : active_players())
					gi.Center_Print(player, "THE RIFT GUARDIAN APPEARS");
			}
		}
		return;
	}

	// guardian gone (killed, or its corpse already freed): rift cleared
	edict_t *guardian = level.loot_rift_guardian;
	if (!guardian->inuse || guardian->spawn_count != level.loot_rift_guardian_count || guardian->health <= 0 || guardian->deadflag)
	{
		const vec3_t where = guardian->inuse ? guardian->s.origin : ent->s.origin;
		level.loot_rift_guardian = nullptr;
		Rift_Reward(ent, where);
	}
}
