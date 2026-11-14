// q2rpg: loot chests. Each map gets 1-2 regular chests and sometimes a red one, placed next to
// random map items away from the player start. Placement uses the map seed, so a restart or
// reload puts them in the same spots. Touch a chest to open it (Loot_ChestDrops pays out).
// In coop every player opens it once for their own instanced loot.
// Mappers: item_loot_chest (loot_rarity 2+ = red chest), worldspawn loot_nochests 1.

#include "../g_local.h"

#include <random>
#include <vector>

constexpr float CHEST_RED_CHANCE = 0.35f;      // chance a map has a red chest
constexpr float CHEST_SECOND_CHANCE = 0.5f;    // chance of a second regular chest
constexpr float CHEST_START_CLEARANCE = 400.f; // no chests this close to a player start
constexpr float CHEST_SPACING = 800.f;         // chests at least this far apart
constexpr float CHEST_OFFSET = 40.f;           // beside the item, not on top of it
constexpr int32_t CHEST_MIN_MONSTERS = 8;      // skip tiny maps (cutscene / ending maps)
constexpr float CHEST_MODEL_SCALE = 2.5f;
static const vec3_t CHEST_MINS = {-16, -16, -16};
static const vec3_t CHEST_MAXS = {16, 16, 16};

static bool Chest_IsRed(const edict_t *chest)
{
	return chest->style != 0;
}

TOUCH(Chest_Touch)(edict_t *self, edict_t *other, const trace_t &tr, bool other_touching_self)->void
{
	if (!other->client || other->health <= 0 || Loot_HiddenFrom(self, other))
		return;

	const bool per_player = Loot_PerPlayerDrops();
	Loot_ChestDrops(self->s.origin, Chest_IsRed(self), per_player ? other : nullptr, other);
	gi.sound(other, CHAN_AUTO, gi.soundindex(Chest_IsRed(self) ? "misc/secret.wav" : "items/pkup.wav"), 1, ATTN_NORM, 0);
	Loot_LogEvent(other, Chest_IsRed(self) ? "CHEST opened  red" : "CHEST opened");
	Mission_OnChestOpened(other);

	if (!per_player)
	{
		G_FreeEdict(self);
		return;
	}

	// coop: hide it from this player; gone once every player in the game opened it
	self->svflags |= SVF_INSTANCED;
	self->item_picked_up_by[other->s.number - 1] = true;
	for (auto player : active_players())
		if (!self->item_picked_up_by[player->s.number - 1])
			return;
	G_FreeEdict(self);
}

static void Chest_Setup(edict_t *chest, bool red_chest)
{
	chest->classname = "loot_chest";
	chest->style = red_chest ? 1 : 0;
	gi.setmodel(chest, "models/items/ammo/bullets/medium/tris.md2");
	chest->s.scale = CHEST_MODEL_SCALE;
	chest->s.effects = EF_COLOR_SHELL;
	chest->s.renderfx = RF_GLOW | (red_chest ? RF_SHELL_RED : (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE)); // red or white
	chest->mins = CHEST_MINS;
	chest->maxs = CHEST_MAXS;
	chest->solid = SOLID_TRIGGER;
	chest->movetype = MOVETYPE_NONE;
	chest->touch = Chest_Touch;
	for (uint32_t i = 0; i < game.maxclients; i++)
		chest->item_picked_up_by[i] = false;
	gi.linkentity(chest);
}

// item_loot_chest: a mapper-placed chest. loot_rarity 2 (Rare) or higher makes it a red chest.
void SP_item_loot_chest(edict_t *self)
{
	if (deathmatch->integer)
	{
		G_FreeEdict(self);
		return;
	}
	Chest_Setup(self, st.loot_rarity >= LOOT_RARITY_RARE);
	// sit on the floor
	const trace_t floor = gi.trace(self->s.origin, CHEST_MINS, CHEST_MAXS, self->s.origin - vec3_t{0, 0, 128}, self, MASK_SOLID);
	if (!floor.startsolid && floor.fraction < 1.0f)
		self->s.origin = floor.endpos;
	gi.linkentity(self);
}

// A free floor spot beside the item, or false.
static bool Chest_FindSpot(const edict_t *item, std::mt19937 &rng, vec3_t &spot)
{
	const int32_t first_angle = std::uniform_int_distribution<int32_t>(0, 7)(rng);
	for (int32_t step = 0; step < 8; step++)
	{
		const float yaw = DEG2RAD((float)((first_angle + step) % 8) * 45.f);
		const vec3_t beside = item->s.origin + vec3_t{cosf(yaw), sinf(yaw), 0} * CHEST_OFFSET + vec3_t{0, 0, 8};

		const trace_t side = gi.trace(item->s.origin + vec3_t{0, 0, 8}, CHEST_MINS, CHEST_MAXS, beside, item, MASK_SOLID);
		if (side.startsolid || side.fraction < 1.0f)
			continue;
		const trace_t floor = gi.trace(beside, CHEST_MINS, CHEST_MAXS, beside - vec3_t{0, 0, 128}, item, MASK_SOLID);
		if (floor.startsolid || floor.fraction >= 1.0f || floor.plane.normal.z < 0.7f)
			continue;
		if (gi.pointcontents(floor.endpos) & (CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER))
			continue;
		spot = floor.endpos;
		return true;
	}
	return false;
}

static bool Chest_NearAny(const vec3_t &spot, const std::vector<vec3_t> &points, float range)
{
	for (const vec3_t &point : points)
		if ((point - spot).length() < range)
			return true;
	return false;
}

void Chest_MapSpawn()
{
	if (deathmatch->integer || level.loot_nochests || Rift_InRift() || level.total_monsters < CHEST_MIN_MONSTERS)
		return;

	std::vector<vec3_t> player_starts;
	for (const char *start_class : {"info_player_start", "info_player_coop"})
		for (edict_t *e = nullptr; (e = G_FindByString<&edict_t::classname>(e, start_class));)
			player_starts.push_back(e->s.origin);

	// candidate items: plain map pickups, not keys, not trigger-spawned or hidden
	std::vector<edict_t *> items;
	for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; i++)
	{
		edict_t *e = &g_edicts[i];
		if (!e->inuse || !e->item || e->targetname || (e->item->flags & IF_KEY))
			continue;
		if (e->spawnflags.has(SPAWNFLAG_ITEM_TRIGGER_SPAWN | SPAWNFLAG_ITEM_NO_TOUCH))
			continue;
		if (Chest_NearAny(e->s.origin, player_starts, CHEST_START_CLEARANCE))
			continue;
		items.push_back(e);
	}
	if (items.empty())
		return;

	// same chests after a restart or reload: everything here rolls from the map seed
	std::mt19937 rng(NGPlus_MapSeed() ^ 0xc4e57c4eu);
	std::shuffle(items.begin(), items.end(), rng);
	auto chance = [&](float p)
	{ return std::uniform_real_distribution<float>(0.f, 1.f)(rng) < p; };

	std::vector<bool> chest_is_red = {false};
	if (chance(CHEST_SECOND_CHANCE))
		chest_is_red.push_back(false);
	if (chance(CHEST_RED_CHANCE))
		chest_is_red.push_back(true);

	std::vector<vec3_t> placed;
	size_t next_item = 0;
	for (const bool red_chest : chest_is_red)
	{
		for (; next_item < items.size(); next_item++)
		{
			vec3_t spot;
			if (Chest_NearAny(items[next_item]->s.origin, placed, CHEST_SPACING) || !Chest_FindSpot(items[next_item], rng, spot))
				continue;
			edict_t *chest = G_Spawn();
			chest->s.origin = spot;
			Chest_Setup(chest, red_chest);
			placed.push_back(spot);
			next_item++;
			break;
		}
	}
}

// dev: loot_chests lists this map's unopened chests (cheats in multiplayer)
void Chest_CmdList(edict_t *ent)
{
	int32_t chest_count = 0;
	for (edict_t *e = nullptr; (e = G_FindByString<&edict_t::classname>(e, "loot_chest"));)
	{
		if (Loot_HiddenFrom(e, ent))
			continue;
		gi.Client_Print(ent, PRINT_HIGH, G_Fmt("{} chest at {:.0f} {:.0f} {:.0f}  ({:.0f} away)\n", Chest_IsRed(e) ? "Red" : "Regular", e->s.origin.x, e->s.origin.y, e->s.origin.z, (e->s.origin - ent->s.origin).length()).data());
		chest_count++;
	}
	if (!chest_count)
		gi.Client_Print(ent, PRINT_HIGH, "No unopened chests on this map\n");
}
