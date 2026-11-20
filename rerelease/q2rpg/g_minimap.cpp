// q2rpg: minimap, server side. A 64x64 grid is laid over the map's extent at map start. Each
// frame every player probes a few nearby cells with traces: line of sight from the eye, then
// down for the floor height, then sideways for walls. The grid is shared by all players (co-op
// maps fill in together) and sent as configstrings, so remote clients draw it too. Each player
// also gets a list of the nearest items in explored cells and their display mode (M cycles).
// No monsters are shown. Drawing lives in cg_minimap.cpp; the wire format in minimap_shared.h.

#include "../g_local.h"
#include "minimap_shared.h"

constexpr int32_t MINIMAP_PROBES_PER_FRAME = 12;  // cells probed per player per server frame
constexpr int32_t MINIMAP_PROBE_RADIUS = 10;      // in cells
constexpr int32_t MINIMAP_FLUSHES_PER_FRAME = 6;  // grid configstrings sent per server frame
constexpr gtime_t MINIMAP_ITEM_INTERVAL = 250_ms; // item list refresh
constexpr float MINIMAP_OTHER_FLOOR = 64.f;       // item this far above / below your feet is on another floor

struct minimap_cell_t
{
	bool known;
	int32_t height_step;
	int32_t flags;
};

struct minimap_state_t
{
	char map_name[MAX_QPATH];
	vec3_t origin; // grid corner (min x, min y) and lowest floor height (z)
	float cell_size;
	std::array<minimap_cell_t, MINIMAP_SIZE * MINIMAP_SIZE> cells;
	std::array<bool, MINIMAP_GRID_STRINGS> dirty;
	std::array<int32_t, MAX_CLIENTS> probe_cursor;
};

static minimap_state_t minimap;

// probe order: cells around the player, nearest first
static const std::vector<std::pair<int32_t, int32_t>> &Minimap_ProbeOrder()
{
	static const std::vector<std::pair<int32_t, int32_t>> order = []
	{
		std::vector<std::pair<int32_t, int32_t>> offsets;
		for (int32_t dy = -MINIMAP_PROBE_RADIUS; dy <= MINIMAP_PROBE_RADIUS; dy++)
			for (int32_t dx = -MINIMAP_PROBE_RADIUS; dx <= MINIMAP_PROBE_RADIUS; dx++)
				if (dx * dx + dy * dy <= MINIMAP_PROBE_RADIUS * MINIMAP_PROBE_RADIUS)
					offsets.push_back({dx, dy});
		std::stable_sort(offsets.begin(), offsets.end(), [](const auto &a, const auto &b)
		                 { return a.first * a.first + a.second * a.second < b.first * b.first + b.second * b.second; });
		return offsets;
	}();
	return order;
}

static void Minimap_Grow(vec3_t &mins, vec3_t &maxs, const vec3_t &point)
{
	for (int32_t axis = 0; axis < 3; axis++)
	{
		mins[axis] = std::min(mins[axis], point[axis]);
		maxs[axis] = std::max(maxs[axis], point[axis]);
	}
}

static void Minimap_SendInfo()
{
	gi.configstring(CONFIG_LOOT_MAP_INFO,
	                G_Fmt("{} {} {} {}", (int32_t)minimap.origin[0], (int32_t)minimap.origin[1], (int32_t)minimap.cell_size, (int32_t)minimap.origin[2]).data());
}

// Lays the grid over everything placed in the map (spawns, items, monsters, brush entities).
static void Minimap_Reset()
{
	Q_strlcpy(minimap.map_name, level.mapname, sizeof(minimap.map_name));
	minimap.cells.fill({});
	minimap.dirty.fill(true);
	minimap.probe_cursor.fill(0);

	vec3_t mins = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
	vec3_t maxs = -mins;
	for (uint32_t i = 1; i < globals.num_edicts; i++)
	{
		const edict_t *e = &g_edicts[i];
		if (!e->inuse)
			continue;
		if (e->solid == SOLID_BSP)
		{
			Minimap_Grow(mins, maxs, e->absmin);
			Minimap_Grow(mins, maxs, e->absmax);
		}
		else
			Minimap_Grow(mins, maxs, e->s.origin);
	}
	if (mins[0] > maxs[0]) // nothing placed: a 4096-unit square around the origin
	{
		mins = {-2048, -2048, -512};
		maxs = {2048, 2048, 512};
	}

	constexpr float padding = 256.f;
	const float extent = std::max(maxs[0] - mins[0], maxs[1] - mins[1]) + padding * 2;
	minimap.cell_size = std::max(48.f, ceilf(extent / MINIMAP_SIZE));
	const vec3_t center = (mins + maxs) * 0.5f;
	const float half = minimap.cell_size * MINIMAP_SIZE * 0.5f;
	minimap.origin = {floorf(center[0] - half), floorf(center[1] - half), floorf(mins[2] - padding)};

	Minimap_SendInfo();
}

static bool Minimap_CellAt(const vec3_t &point, int32_t &cell_x, int32_t &cell_y)
{
	cell_x = (int32_t)floorf((point[0] - minimap.origin[0]) / minimap.cell_size);
	cell_y = (int32_t)floorf((point[1] - minimap.origin[1]) / minimap.cell_size);
	return cell_x >= 0 && cell_y >= 0 && cell_x < MINIMAP_SIZE && cell_y < MINIMAP_SIZE;
}

static vec3_t Minimap_CellCenter(int32_t cell_x, int32_t cell_y, float z)
{
	return {minimap.origin[0] + (cell_x + 0.5f) * minimap.cell_size, minimap.origin[1] + (cell_y + 0.5f) * minimap.cell_size, z};
}

static void Minimap_SetCell(int32_t cell_x, int32_t cell_y, const minimap_cell_t &value)
{
	minimap_cell_t &cell = minimap.cells[cell_y * MINIMAP_SIZE + cell_x];
	if (cell.known == value.known && cell.height_step == value.height_step && cell.flags == value.flags)
		return;
	cell = value;
	minimap.dirty[(cell_y * MINIMAP_SIZE + cell_x) / MINIMAP_CELLS_PER_STRING] = true;
}

static int32_t Minimap_HeightStep(float z)
{
	return std::clamp((int32_t)floorf((z - minimap.origin[2]) / MINIMAP_HEIGHT_STEP), 0, MINIMAP_HEIGHT_STEPS - 1);
}

static float Minimap_StepHeight(int32_t height_step)
{
	return minimap.origin[2] + height_step * MINIMAP_HEIGHT_STEP;
}

// One cell: seen from the eye? Then its floor height, liquids and the walls on its east and north edges.
static void Minimap_Probe(edict_t *player, int32_t cell_x, int32_t cell_y)
{
	const vec3_t eye = player->s.origin + vec3_t{0, 0, (float)player->viewheight};
	const vec3_t target = Minimap_CellCenter(cell_x, cell_y, eye[2]);
	const minimap_cell_t &current = minimap.cells[cell_y * MINIMAP_SIZE + cell_x];

	const trace_t sight = gi.traceline(eye, target, player, MASK_SOLID);
	if (sight.fraction < 1.f)
	{
		// the wall we hit is inside this cell: it is a wall cell, unless a floor was already found here
		const float miss = (sight.endpos - target).length();
		if (!current.known && miss < minimap.cell_size * 0.5f)
			Minimap_SetCell(cell_x, cell_y, {true, 0, MINIMAP_SOLID});
		return;
	}

	const trace_t down = gi.traceline(target, target - vec3_t{0, 0, 2048}, player, MASK_SOLID);
	const float floor_z = down.endpos[2];
	const float feet_z = player->s.origin[2] + player->mins[2];

	// stacked floors: keep the one nearest the player who saw it
	if (current.known && !(current.flags & MINIMAP_SOLID) && fabsf(Minimap_StepHeight(current.height_step) - feet_z) < fabsf(floor_z - feet_z) - MINIMAP_HEIGHT_STEP)
		return;

	int32_t flags = 0;
	const contents_t liquid = gi.pointcontents(vec3_t{target[0], target[1], floor_z + 4});
	if (liquid & (CONTENTS_LAVA | CONTENTS_SLIME))
		flags |= MINIMAP_LAVA;
	else if (liquid & CONTENTS_WATER)
		flags |= MINIMAP_WATER;

	// walls: step-height-and-up sideways traces to the next cell east and north
	const vec3_t wall_start = {target[0], target[1], floor_z + 24.f};
	if (gi.traceline(wall_start, wall_start + vec3_t{minimap.cell_size, 0, 0}, player, MASK_SOLID).fraction < 1.f)
		flags |= MINIMAP_WALL_EAST;
	if (gi.traceline(wall_start, wall_start + vec3_t{0, minimap.cell_size, 0}, player, MASK_SOLID).fraction < 1.f)
		flags |= MINIMAP_WALL_NORTH;

	Minimap_SetCell(cell_x, cell_y, {true, Minimap_HeightStep(floor_z), flags});
}

static void Minimap_Explore(edict_t *player)
{
	int32_t center_x, center_y;
	if (!Minimap_CellAt(player->s.origin, center_x, center_y))
		return;

	const auto &order = Minimap_ProbeOrder();
	int32_t &cursor = minimap.probe_cursor[player->s.number - 1];
	for (int32_t probe = 0; probe < MINIMAP_PROBES_PER_FRAME; probe++)
	{
		cursor = (cursor + 1) % (int32_t)order.size();
		const int32_t cell_x = center_x + order[cursor].first;
		const int32_t cell_y = center_y + order[cursor].second;
		if (cell_x < 0 || cell_y < 0 || cell_x >= MINIMAP_SIZE || cell_y >= MINIMAP_SIZE)
			continue;
		Minimap_Probe(player, cell_x, cell_y);
	}
}

static void Minimap_FlushGrid()
{
	int32_t sent = 0;
	for (int32_t index = 0; index < MINIMAP_GRID_STRINGS && sent < MINIMAP_FLUSHES_PER_FRAME; index++)
	{
		if (!minimap.dirty[index])
			continue;
		minimap.dirty[index] = false;
		sent++;

		char text[MINIMAP_CELLS_PER_STRING * 2 + 1];
		int32_t length = 0;
		for (int32_t i = 0; i < MINIMAP_CELLS_PER_STRING; i++)
		{
			const int32_t cell_index = index * MINIMAP_CELLS_PER_STRING + i;
			if (cell_index >= MINIMAP_SIZE * MINIMAP_SIZE)
				break;
			const minimap_cell_t &cell = minimap.cells[cell_index];
			const int32_t packed = Minimap_PackCell(cell.known, cell.height_step, cell.flags);
			text[length++] = Minimap_EncodeDigit(packed / MINIMAP_BASE);
			text[length++] = Minimap_EncodeDigit(packed % MINIMAP_BASE);
		}
		text[length] = '\0';
		gi.configstring(CONFIG_LOOT_MAP_GRID + index, text);
	}
}

// the marker type for an item entity, or -1 to leave it off the map
static int32_t Minimap_ItemType(const edict_t *item, const edict_t *player)
{
	if (item->classname && !strcmp(item->classname, "loot_chest"))
		return Loot_HiddenFrom(item, player) ? -1 : (item->style ? MINIMAP_ITEM_RED_CHEST : MINIMAP_ITEM_CHEST);

	// hidden items (respawning, waiting on a trigger) stay off the map
	if (!item->item || item->solid != SOLID_TRIGGER || (item->svflags & SVF_NOCLIENT) || Loot_HiddenFrom(item, player))
		return -1;

	if (Loot_IsLootItem(item))
	{
		loot_rarity_t rarity = item->loot.rarity;
		if (item->loot_shield.valid)
			rarity = item->loot_shield.rarity;
		else if (item->loot_grenade.valid)
			rarity = item->loot_grenade.rarity;
		else if (item->loot_classmod.valid)
			rarity = item->loot_classmod.rarity;
		else if (item->loot_relic.valid)
			rarity = item->loot_relic.rarity;
		if ((int32_t)rarity < player->client->pers.loot_filter) // follows the loot filter (L)
			return -1;
		return MINIMAP_ITEM_LOOT + std::clamp((int32_t)rarity, 0, 5);
	}

	const item_flags_t flags = item->item->flags;
	if (flags & IF_HEALTH)
		return MINIMAP_ITEM_HEALTH;
	if (flags & IF_ARMOR)
		return MINIMAP_ITEM_ARMOR;
	if (flags & IF_AMMO)
		return MINIMAP_ITEM_AMMO;
	if (flags & IF_WEAPON)
		return MINIMAP_ITEM_WEAPON;
	if (flags & IF_KEY)
		return MINIMAP_ITEM_KEY;
	if (flags & IF_POWERUP)
		return MINIMAP_ITEM_POWERUP;
	return -1;
}

// "<mode><x><y><type*3+level>..." for the nearest items in explored cells
static void Minimap_SendItems(edict_t *player)
{
	const int32_t player_index = player->s.number - 1;
	if (player_index >= MINIMAP_MAX_ITEM_PLAYERS)
		return;

	struct marker_t
	{
		float distance;
		int32_t cell_x, cell_y, code;
	};
	std::vector<marker_t> markers;
	const float feet_z = player->s.origin[2] + player->mins[2];

	for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; i++)
	{
		const edict_t *item = &g_edicts[i];
		if (!item->inuse)
			continue;
		const int32_t type = Minimap_ItemType(item, player);
		if (type < 0)
			continue;
		int32_t cell_x, cell_y;
		if (!Minimap_CellAt(item->s.origin, cell_x, cell_y) || !minimap.cells[cell_y * MINIMAP_SIZE + cell_x].known)
			continue;
		const float dz = item->s.origin[2] - feet_z;
		const int32_t item_level = dz > MINIMAP_OTHER_FLOOR ? MINIMAP_LEVEL_ABOVE : dz < -MINIMAP_OTHER_FLOOR ? MINIMAP_LEVEL_BELOW
		                                                                                                      : MINIMAP_LEVEL_SAME;
		markers.push_back({(item->s.origin - player->s.origin).length(), cell_x, cell_y, type * 3 + item_level});
	}
	std::sort(markers.begin(), markers.end(), [](const marker_t &a, const marker_t &b)
	          { return a.distance < b.distance; });

	std::string text(1, Minimap_EncodeDigit(std::clamp(player->client->pers.loot_map_mode, 0, MINIMAP_MODE_COUNT - 1)));
	for (size_t i = 0; i < markers.size() && i < MINIMAP_MAX_ITEMS; i++)
	{
		text += Minimap_EncodeDigit(markers[i].cell_x);
		text += Minimap_EncodeDigit(markers[i].cell_y);
		text += Minimap_EncodeDigit(markers[i].code);
	}

	gclient_t *cl = player->client;
	if (text != cl->loot_map_items_cache)
	{
		Q_strlcpy(cl->loot_map_items_cache, text.c_str(), sizeof(cl->loot_map_items_cache));
		gi.configstring(CONFIG_LOOT_MAP_ITEMS + player_index, text.c_str());
	}
}

// Loot_SetStats, every server frame for every player
void Minimap_Frame(edict_t *player)
{
	if (deathmatch->integer || !player->client)
		return;

	if (strcmp(minimap.map_name, level.mapname))
		Minimap_Reset();
	else if (!*gi.get_configstring(CONFIG_LOOT_MAP_INFO))
	{
		// same map loaded again (death reload, save load): the engine cleared the configstrings;
		// keep what was explored and resend it all
		Minimap_SendInfo();
		minimap.dirty.fill(true);
		for (auto other : active_players())
			other->client->loot_map_items_cache[0] = '\0';
	}

	if (player->health > 0 && !player->client->resp.spectator)
		Minimap_Explore(player);

	// one flush per server frame, done by the first player to get here
	static gtime_t last_flush;
	if (last_flush != level.time)
	{
		last_flush = level.time;
		Minimap_FlushGrid();
	}

	if (level.time >= player->client->loot_map_items_time || player->client->loot_map_items_time > level.time + 1_sec)
	{
		player->client->loot_map_items_time = level.time + MINIMAP_ITEM_INTERVAL;
		Minimap_SendItems(player);
	}
}

// "loot_map" (M): corner radar -> full-screen map -> off
void Minimap_CycleMode(edict_t *player)
{
	int32_t &mode = player->client->pers.loot_map_mode;
	mode = (mode + 1) % MINIMAP_MODE_COUNT;
	player->client->loot_map_items_time = 0_ms; // send the new mode now
}
