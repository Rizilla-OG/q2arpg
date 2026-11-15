// q2rpg: side missions. The vendor's Missions page (5th, after the stash) offers 2 per map,
// rolled from the map seed. Taking one is free; one runs at a time, and leaving the map drops
// it. Finishing pays XP, cash and a gear piece with a minimum rarity set by the difficulty.
// Progress shows on the level / XP HUD line.

#include "../g_local.h"

#include <algorithm>
#include <random>
#include <vector>

constexpr int32_t MISSION_MIN_MONSTERS = 8;        // skip tiny maps
constexpr int32_t MISSION_KIND_MIN_ON_MAP = 4;     // a monster kind needs this many on the map
constexpr int32_t MISSION_ALPHA_MIN_MONSTERS = 25; // enough monsters for Alphas to show up
constexpr gtime_t MISSION_SPREE_WINDOW = 60_sec;

static const char *const mission_difficulty_names[] = {"easy", "medium", "hard"};
static const float mission_xp_share[] = {0.10f, 0.15f, 0.22f}; // of the XP to the next level
static const float mission_cash_mult[] = {1.0f, 1.5f, 2.2f};
static const loot_rarity_t mission_min_rarity[] = {LOOT_RARITY_UNCOMMON, LOOT_RARITY_RARE, LOOT_RARITY_EPIC};

// a monster whose kill counts for missions: alive, and pays on death (no mapper loot_nodrop, not medic-revived)
static bool Mission_CountsAsTarget(const edict_t *e)
{
	return Loot_IsLiveMonster(e) && !e->loot_map_nodrop && !e->loot_revived;
}

//==========================================================================
// Rolling the offers (end of SpawnEntities, after the chests)
//==========================================================================

void Mission_MapSpawn()
{
	for (loot_mission_t &offer : level.loot_missions)
		offer = {};
	level.loot_mission_done_by.fill(0);
	if (deathmatch->integer || Rift_InRift() || level.total_monsters < MISSION_MIN_MONSTERS)
		return;

	// monster kinds on this map (no bosses)
	std::vector<std::pair<const char *, int32_t>> kinds;
	for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; i++)
	{
		const edict_t *e = &g_edicts[i];
		if (!Mission_CountsAsTarget(e) || !e->classname || Loot_IsBossTier(e))
			continue;
		bool found = false;
		for (auto &kind : kinds)
			if (!strcmp(kind.first, e->classname))
			{
				kind.second++;
				found = true;
			}
		if (!found)
			kinds.push_back({e->classname, 1});
	}
	kinds.erase(std::remove_if(kinds.begin(), kinds.end(), [](const auto &kind)
	                           { return kind.second < MISSION_KIND_MIN_ON_MAP; }),
	            kinds.end());
	const bool has_chest = G_FindByString<&edict_t::classname>(nullptr, "loot_chest") != nullptr;

	std::vector<loot_mission_type_t> types;
	if (!kinds.empty())
		types.push_back(LOOT_MISSION_KILL_KIND);
	if (level.total_monsters >= MISSION_ALPHA_MIN_MONSTERS) // from the seeded count only, so offers stay the same after a restart
		types.push_back(LOOT_MISSION_ALPHAS);
	types.push_back(LOOT_MISSION_HEADSHOTS);
	types.push_back(LOOT_MISSION_ELEMENT);
	if (has_chest)
		types.push_back(LOOT_MISSION_CHEST);
	if (level.total_monsters >= 20)
		types.push_back(LOOT_MISSION_SPREE);

	// same offers after a restart or reload
	std::mt19937 rng(NGPlus_MapSeed() ^ 0x6d155109u);
	std::shuffle(types.begin(), types.end(), rng);

	for (int32_t i = 0; i < LOOT_MISSION_OFFERS && i < (int32_t)types.size(); i++)
	{
		loot_mission_t &offer = level.loot_missions[i];
		offer.valid = true;
		offer.type = types[i];
		switch (offer.type)
		{
			case LOOT_MISSION_KILL_KIND:
			{
				const auto &kind = kinds[rng() % kinds.size()];
				offer.goal = std::clamp((int32_t)(kind.second * 0.6f), 3, 12); // 4+ on the map, so some can die first
				offer.difficulty = 0;
				Q_strlcpy(offer.monster_class, kind.first, sizeof(offer.monster_class));
				Loot_ClassDisplayName(kind.first, offer.short_label, sizeof(offer.short_label));
				G_FmtTo(offer.label, "Kill {} {}", offer.goal, offer.short_label);
				break;
			}
			case LOOT_MISSION_ALPHAS:
				offer.goal = level.total_monsters >= 40 ? 3 : 2;
				offer.difficulty = 2;
				Q_strlcpy(offer.short_label, "Alphas", sizeof(offer.short_label));
				G_FmtTo(offer.label, "Kill {} Alphas", offer.goal);
				break;
			case LOOT_MISSION_HEADSHOTS:
				offer.goal = 10;
				offer.difficulty = 1;
				Q_strlcpy(offer.short_label, "Headshots", sizeof(offer.short_label));
				G_FmtTo(offer.label, "{} headshot kills", offer.goal);
				break;
			case LOOT_MISSION_ELEMENT:
				offer.goal = 10;
				offer.difficulty = 1;
				offer.element = (loot_element_t)(1 + rng() % (LOOT_ELEM_COUNT - 1));
				G_FmtTo(offer.short_label, "{} kills", Loot_ElementName(offer.element));
				G_FmtTo(offer.label, "{} {} kills", offer.goal, Loot_ElementName(offer.element));
				break;
			case LOOT_MISSION_CHEST:
				offer.goal = 1;
				offer.difficulty = 0;
				Q_strlcpy(offer.short_label, "Chest", sizeof(offer.short_label));
				Q_strlcpy(offer.label, "Open a loot chest", sizeof(offer.label));
				break;
			case LOOT_MISSION_SPREE:
				offer.goal = 10;
				offer.difficulty = 1;
				Q_strlcpy(offer.short_label, "Spree", sizeof(offer.short_label));
				G_FmtTo(offer.label, "{} kills inside {:.0f}s", offer.goal, MISSION_SPREE_WINDOW.seconds());
				break;
			default:
				break;
		}
	}
}

//==========================================================================
// Player state
//==========================================================================

// Mission state belongs to one map; entering another drops it. Rifts, raids and the arena are
// side trips: the mission waits for the way back. Returns true when the state is this map's.
static bool Mission_CheckMap(edict_t *ent)
{
	client_persistant_t &pers = ent->client->pers;
	if (!Q_strcasecmp(pers.loot_mission_map, level.mapname))
		return true;
	if (Loot_OnSideTrip())
		return false;
	Q_strlcpy(pers.loot_mission_map, level.mapname, sizeof(pers.loot_mission_map));
	pers.loot_mission = {};
	return true;
}

static uint32_t Mission_PlayerBit(const edict_t *ent)
{
	return 1u << ((ent->s.number - 1) & 31);
}

static bool Mission_DoneBy(const edict_t *ent, int32_t offer)
{
	return offer >= 0 && offer < LOOT_MISSION_OFFERS && (level.loot_mission_done_by[offer] & Mission_PlayerBit(ent));
}

void Mission_Frame(edict_t *ent)
{
	if (deathmatch->integer)
		return;
	if (!Mission_CheckMap(ent))
		return;
	// a Spree run that ran out of time starts over (a start in the future: the map was reloaded)
	loot_mission_t &mission = ent->client->pers.loot_mission;
	if (mission.valid && mission.type == LOOT_MISSION_SPREE && mission.progress > 0 &&
	    (level.time > ent->client->loot_mission_spree_start + MISSION_SPREE_WINDOW || ent->client->loot_mission_spree_start > level.time))
		mission.progress = 0;
}

static int32_t Mission_RewardXP(edict_t *ent, const loot_mission_t &mission)
{
	return (int32_t)(Loot_XPToNext(std::max(1, ent->client->pers.loot_level)) * mission_xp_share[std::clamp(mission.difficulty, 0, 2)]);
}

static int32_t Mission_RewardCash(const loot_mission_t &mission)
{
	return (int32_t)((100.f + 40.f * Loot_ZoneLevel()) * mission_cash_mult[std::clamp(mission.difficulty, 0, 2)] * NGPlus_CashMult());
}

static void Mission_Complete(edict_t *ent)
{
	client_persistant_t &pers = ent->client->pers;
	const loot_mission_t mission = pers.loot_mission;
	pers.loot_mission = {};
	if (pers.loot_mission_taken >= 0 && pers.loot_mission_taken < LOOT_MISSION_OFFERS)
		level.loot_mission_done_by[pers.loot_mission_taken] |= Mission_PlayerBit(ent);

	const int32_t xp = Mission_RewardXP(ent, mission);
	const int32_t cash = Mission_RewardCash(mission);
	const bool max_level = pers.loot_level >= Loot_LevelCap();
	if (!max_level)
		Loot_GiveXP(ent, xp);
	pers.loot_money += cash;
	Loot_MissionDrop(ent, mission_min_rarity[std::clamp(mission.difficulty, 0, 2)]);

	gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/secret.wav"), 1, ATTN_NORM, 0);
	gi.Center_Print(ent, G_Fmt("MISSION COMPLETE\n{}\n{}${}  +gear", mission.label, max_level ? "" : G_Fmt("+{} XP  ", xp).data(), cash).data());
	Loot_LogEvent(ent, G_Fmt("MISSION done  {}  ${}", mission.label, cash).data());
}

static void Mission_Progress(edict_t *ent, int32_t amount)
{
	loot_mission_t &mission = ent->client->pers.loot_mission;
	mission.progress = std::min(mission.goal, mission.progress + amount);
	if (mission.progress >= mission.goal)
		Mission_Complete(ent);
}

void Mission_OnKill(edict_t *monster, edict_t *attacker)
{
	if (deathmatch->integer || !attacker || !attacker->client || !monster)
		return;
	loot_mission_t &mission = attacker->client->pers.loot_mission;
	// only on the mission's own map: rift, raid and arena kills don't count
	if (!Mission_CheckMap(attacker) || !mission.valid)
		return;

	switch (mission.type)
	{
		case LOOT_MISSION_KILL_KIND:
			if (monster->classname && !strcmp(monster->classname, mission.monster_class))
				Mission_Progress(attacker, 1);
			break;
		case LOOT_MISSION_ALPHAS:
			if (monster->loot_elite)
				Mission_Progress(attacker, 1);
			break;
		case LOOT_MISSION_HEADSHOTS:
			if (monster->loot_hit_crit)
				Mission_Progress(attacker, 1);
			break;
		case LOOT_MISSION_ELEMENT:
			if (monster->loot_hit_elements & (1 << mission.element))
				Mission_Progress(attacker, 1);
			break;
		case LOOT_MISSION_SPREE:
			// a new run starts when the window ran out
			if (mission.progress == 0 || level.time > attacker->client->loot_mission_spree_start + MISSION_SPREE_WINDOW ||
			    attacker->client->loot_mission_spree_start > level.time)
			{
				mission.progress = 0;
				attacker->client->loot_mission_spree_start = level.time;
			}
			Mission_Progress(attacker, 1);
			break;
		default:
			break;
	}
}

void Mission_OnChestOpened(edict_t *player)
{
	if (deathmatch->integer || !player || !player->client)
		return;
	if (Mission_CheckMap(player) && player->client->pers.loot_mission.valid && player->client->pers.loot_mission.type == LOOT_MISSION_CHEST)
		Mission_Progress(player, 1);
}

// HUD: "  Mission 4/8 Gunner", or "" with no mission
std::string Mission_HudText(edict_t *ent)
{
	const loot_mission_t &mission = ent->client->pers.loot_mission;
	if (!mission.valid || Q_strcasecmp(ent->client->pers.loot_mission_map, level.mapname))
		return "";
	if (mission.type == LOOT_MISSION_SPREE && mission.progress > 0)
	{
		const float seconds_left = std::max(0.f, (ent->client->loot_mission_spree_start + MISSION_SPREE_WINDOW - level.time).seconds());
		return G_Fmt("  Mission {}/{} {} {:.0f}s", mission.progress, mission.goal, mission.short_label, seconds_left).data();
	}
	return G_Fmt("  Mission {}/{} {}", mission.progress, mission.goal, mission.short_label).data();
}

// Whether the offer can still be done: kill-kind and Alphas need enough fitting monsters alive for
// the goal, the chest one an unopened chest; headshot, element and spree kills need any target.
static bool Mission_Reachable(edict_t *ent, const loot_mission_t &offer)
{
	if (offer.type == LOOT_MISSION_CHEST)
	{
		for (edict_t *e = nullptr; (e = G_FindByString<&edict_t::classname>(e, "loot_chest"));)
			if (!Loot_HiddenFrom(e, ent))
				return true;
		return false;
	}
	const bool needs_count = offer.type == LOOT_MISSION_KILL_KIND || offer.type == LOOT_MISSION_ALPHAS;
	const int32_t needed = needs_count ? std::max(1, offer.goal - (offer.valid ? offer.progress : 0)) : 1;
	int32_t left = 0;
	for (uint32_t i = game.maxclients + 1; i < globals.num_edicts && left < needed; i++)
	{
		const edict_t *e = &g_edicts[i];
		if (!Mission_CountsAsTarget(e))
			continue;
		if (offer.type == LOOT_MISSION_KILL_KIND && !(e->classname && !strcmp(e->classname, offer.monster_class)))
			continue;
		if (offer.type == LOOT_MISSION_ALPHAS && !e->loot_elite)
			continue;
		left++;
	}
	return left >= needed;
}

//==========================================================================
// Vendor page
//==========================================================================

void Mission_SendPage(edict_t *ent)
{
	Mission_CheckMap(ent);
	const client_persistant_t &pers = ent->client->pers;
	std::string s = "loot_dim xl 16 ";
	int32_t y = 24;
	auto line = [&](const char *kind, const std::string &text)
	{
		s += G_Fmt("yv {} {} \"{}\" ", y, kind, text).data();
		y += 10;
	};

	line("string2", "MISSIONS");
	y += 6;
	if (pers.loot_mission.valid)
	{
		const bool stuck = !Mission_Reachable(ent, pers.loot_mission);
		line("string", G_Fmt("Active: {}  ({}/{}){}", pers.loot_mission.label, pers.loot_mission.progress, pers.loot_mission.goal, stuck ? "  can't finish: take another" : "").data());
		y += 6;
	}

	bool any_offer = false;
	for (int32_t i = 0; i < LOOT_MISSION_OFFERS; i++)
	{
		const loot_mission_t &offer = level.loot_missions[i];
		if (!offer.valid)
			continue;
		any_offer = true;
		const bool done = Mission_DoneBy(ent, i);
		const bool active = pers.loot_mission.valid && pers.loot_mission_taken == i;
		const char *state = done ? "  DONE" : active                       ? "  ACTIVE"
		                                  : !Mission_Reachable(ent, offer) ? "  (too few left)"
		                                                                   : "";
		line(done || active ? "string" : "string2", G_Fmt("{}  {}  ({}){}", i + 1, offer.label, mission_difficulty_names[std::clamp(offer.difficulty, 0, 2)], state).data());
		line("loot_str", G_Fmt("   Reward: {}${}, {}+ gear", pers.loot_level >= Loot_LevelCap() ? "" : G_Fmt("{} XP, ", Mission_RewardXP(ent, offer)).data(),
		                       Mission_RewardCash(offer), Loot_RarityName(mission_min_rarity[std::clamp(offer.difficulty, 0, 2)]))
		                     .data());
		y += 4;
	}
	if (!any_offer)
		line("string", "No missions on this map");

	y += 6;
	line("string", "1-2 take (replaces the active one)  9 next page");
	Loot_SendLayout(ent, s);
}

void Mission_Take(edict_t *ent, int32_t n)
{
	Mission_CheckMap(ent);
	client_persistant_t &pers = ent->client->pers;
	const int32_t index = n - 1;
	if (index < 0 || index >= LOOT_MISSION_OFFERS || !level.loot_missions[index].valid)
		return;
	if (Mission_DoneBy(ent, index))
	{
		gi.Center_Print(ent, "Already done");
		return;
	}
	if (pers.loot_mission.valid && pers.loot_mission_taken == index)
		return;

	if (!Mission_Reachable(ent, level.loot_missions[index]))
	{
		gi.Center_Print(ent, "Too few left on this map for that one");
		return;
	}

	pers.loot_mission = level.loot_missions[index];
	pers.loot_mission.progress = 0;
	pers.loot_mission_taken = index;
	ent->client->loot_mission_spree_start = level.time;
	gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/am_pkup.wav"), 1, ATTN_NORM, 0);
	gi.Center_Print(ent, G_Fmt("Mission: {}", pers.loot_mission.label).data());
}
