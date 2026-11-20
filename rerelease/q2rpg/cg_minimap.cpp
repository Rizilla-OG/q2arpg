// q2rpg: minimap, HUD side. Draws the explored grid (g_minimap.cpp) as a green radar: north-up,
// floors shaded by height difference from your feet, solid lines for walls, dashed lines for
// ledges, red for lava / slime, and colored markers for nearby items and loot. Corner radar or
// full-screen map, switched with M. Only axis-aligned rects can be drawn, so everything is rects.

#include "../cg_local.h"
#include "minimap_shared.h"

namespace
{
struct grid_view_t
{
	bool valid;
	float origin_x, origin_y, cell_size, z_min;
	std::array<int32_t, MINIMAP_SIZE * MINIMAP_SIZE> cells; // packed, 0 = unexplored
};

grid_view_t grid;

// screen panel and world -> screen mapping
struct panel_t
{
	float left, top, size;    // pixels
	float center_x, center_y; // pixels
	float world_x, world_y;   // world point at the panel center
	float pixels_per_unit;
	int32_t line; // line thickness in pixels
};

const rgba_t color_background = {4, 30, 24, 210};
const rgba_t color_scanline = {0, 0, 0, 50};
const rgba_t color_frame = {60, 160, 120, 255};
const rgba_t color_wall = {110, 230, 170, 255};
const rgba_t color_ledge = {80, 170, 130, 200};
const rgba_t color_lava = {200, 70, 30, 200};
const rgba_t color_water = {40, 90, 160, 170};
const rgba_t color_player = {235, 255, 245, 255};
const rgba_t color_label = {110, 230, 170, 255};

const rgba_t item_colors[MINIMAP_ITEM_COUNT] = {
    {120, 255, 140, 255}, // health
    {240, 220, 90, 255},  // armor
    {170, 170, 170, 255}, // ammo
    {220, 220, 220, 255}, // stock weapon
    {90, 220, 255, 255},  // key
    {255, 110, 255, 255}, // powerup
    {230, 230, 230, 255}, // loot: common
    {90, 220, 90, 255},   // uncommon
    {80, 140, 255, 255},  // rare
    {180, 110, 255, 255}, // epic
    {255, 190, 40, 255},  // legendary
    {255, 80, 160, 255},  // mythic
    {240, 210, 120, 255}, // chest
    {240, 60, 60, 255},   // red chest
};

bool ReadGrid()
{
	const char *info = cgi.get_configstring(CONFIG_LOOT_MAP_INFO);
	if (!info || !*info)
		return false;
	int32_t origin_x, origin_y, cell_size, z_min;
	if (sscanf(info, "%d %d %d %d", &origin_x, &origin_y, &cell_size, &z_min) != 4 || cell_size <= 0)
		return false;
	grid.origin_x = (float)origin_x;
	grid.origin_y = (float)origin_y;
	grid.cell_size = (float)cell_size;
	grid.z_min = (float)z_min;

	grid.cells.fill(0);
	for (int32_t index = 0; index < MINIMAP_GRID_STRINGS; index++)
	{
		const char *text = cgi.get_configstring(CONFIG_LOOT_MAP_GRID + index);
		if (!text)
			continue;
		for (int32_t i = 0; i < MINIMAP_CELLS_PER_STRING && text[i * 2] && text[i * 2 + 1]; i++)
		{
			const int32_t cell_index = index * MINIMAP_CELLS_PER_STRING + i;
			if (cell_index >= MINIMAP_SIZE * MINIMAP_SIZE)
				break;
			grid.cells[cell_index] = Minimap_DecodeDigit(text[i * 2]) * MINIMAP_BASE + Minimap_DecodeDigit(text[i * 2 + 1]);
		}
	}
	return true;
}

int32_t CellAt(int32_t x, int32_t y)
{
	if (x < 0 || y < 0 || x >= MINIMAP_SIZE || y >= MINIMAP_SIZE)
		return 0;
	return grid.cells[y * MINIMAP_SIZE + x];
}

bool IsFloor(int32_t packed)
{
	return packed > 0 && !((packed - 1) % 32 & MINIMAP_SOLID);
}

int32_t CellFlags(int32_t packed)
{
	return (packed - 1) % 32;
}

float CellFloorZ(int32_t packed)
{
	return grid.z_min + ((packed - 1) / 32) * MINIMAP_HEIGHT_STEP;
}

float ScreenX(const panel_t &panel, float world_x)
{
	return panel.center_x + (world_x - panel.world_x) * panel.pixels_per_unit;
}

float ScreenY(const panel_t &panel, float world_y)
{
	return panel.center_y - (world_y - panel.world_y) * panel.pixels_per_unit; // north is up
}

// a rect clipped to the panel
void Fill(const panel_t &panel, float x, float y, float w, float h, const rgba_t &color)
{
	const float right = std::min(x + w, panel.left + panel.size);
	const float bottom = std::min(y + h, panel.top + panel.size);
	x = std::max(x, panel.left);
	y = std::max(y, panel.top);
	if (right - x < 1.f || bottom - y < 1.f)
		return;
	cgi.SCR_DrawColorPic((int)x, (int)y, (int)(right - x), (int)(bottom - y), "_white", color);
}

// a wall or ledge on a cell edge: vertical from (x, y0) to (x, y1) or horizontal from (x0, y) to (x1, y)
void EdgeLine(const panel_t &panel, float x0, float y0, float x1, float y1, const rgba_t &color, bool dashed)
{
	const float half = panel.line * 0.5f;
	const bool vertical = x0 == x1;
	const float length = vertical ? fabsf(y1 - y0) : fabsf(x1 - x0);
	const int32_t pieces = dashed ? 3 : 1;
	const float piece = dashed ? length / 5.f : length;
	for (int32_t i = 0; i < pieces; i++)
	{
		const float offset = dashed ? piece * (i * 2) : 0.f;
		if (vertical)
			Fill(panel, x0 - half, std::min(y0, y1) + offset, (float)panel.line, piece, color);
		else
			Fill(panel, std::min(x0, x1) + offset, y0 - half, piece, (float)panel.line, color);
	}
}

void DrawCells(const panel_t &panel, float feet_z)
{
	const float cell_pixels = grid.cell_size * panel.pixels_per_unit;
	for (int32_t y = 0; y < MINIMAP_SIZE; y++)
		for (int32_t x = 0; x < MINIMAP_SIZE; x++)
		{
			const int32_t packed = CellAt(x, y);
			if (!packed)
				continue;
			const float left = ScreenX(panel, grid.origin_x + x * grid.cell_size);
			const float top = ScreenY(panel, grid.origin_y + (y + 1) * grid.cell_size);
			if (left > panel.left + panel.size || top > panel.top + panel.size || left + cell_pixels < panel.left || top + cell_pixels < panel.top)
				continue;

			const bool floor = IsFloor(packed);
			const int32_t flags = CellFlags(packed);
			const float floor_z = CellFloorZ(packed);
			if (floor)
			{
				// fade with height difference: your floor bright, others dimmer
				const float dz = fabsf(floor_z - feet_z);
				const float brightness = 1.f - std::min(dz, 120.f) / 120.f * 0.7f;
				rgba_t shade = {20, 120, 85, (uint8_t)(170 * brightness)};
				if (flags & MINIMAP_LAVA)
					shade = color_lava;
				else if (flags & MINIMAP_WATER)
					shade = color_water;
				Fill(panel, left, top, cell_pixels + 0.5f, cell_pixels + 0.5f, shade);
			}

			// east edge: wall bit, floor next to a solid cell, or a ledge between two floors
			const int32_t east = CellAt(x + 1, y);
			const float right = left + cell_pixels;
			if (floor && ((flags & MINIMAP_WALL_EAST) || (east && !IsFloor(east))))
				EdgeLine(panel, right, top, right, top + cell_pixels, color_wall, false);
			else if (!floor && IsFloor(east))
				EdgeLine(panel, right, top, right, top + cell_pixels, color_wall, false);
			else if (floor && IsFloor(east) && fabsf(CellFloorZ(east) - floor_z) > 18.f)
				EdgeLine(panel, right, top, right, top + cell_pixels, color_ledge, true);

			// north edge (top of the cell on screen)
			const int32_t north = CellAt(x, y + 1);
			if (floor && ((flags & MINIMAP_WALL_NORTH) || (north && !IsFloor(north))))
				EdgeLine(panel, left, top, right, top, color_wall, false);
			else if (!floor && IsFloor(north))
				EdgeLine(panel, left, top, right, top, color_wall, false);
			else if (floor && IsFloor(north) && fabsf(CellFloorZ(north) - floor_z) > 18.f)
				EdgeLine(panel, left, top, right, top, color_ledge, true);
		}
}

void DrawItems(const panel_t &panel, const char *items, int32_t scale)
{
	const float size = 4.f * scale;
	for (const char *p = items; p[0] && p[1] && p[2]; p += 3)
	{
		const int32_t cell_x = Minimap_DecodeDigit(p[0]);
		const int32_t cell_y = Minimap_DecodeDigit(p[1]);
		const int32_t code = Minimap_DecodeDigit(p[2]);
		const int32_t type = std::clamp(code / 3, 0, MINIMAP_ITEM_COUNT - 1);
		const int32_t item_level = code % 3;

		const float x = ScreenX(panel, grid.origin_x + (cell_x + 0.5f) * grid.cell_size) - size * 0.5f;
		const float y = ScreenY(panel, grid.origin_y + (cell_y + 0.5f) * grid.cell_size) - size * 0.5f;
		const rgba_t &color = item_colors[type];
		const bool chest = type == MINIMAP_ITEM_CHEST || type == MINIMAP_ITEM_RED_CHEST;
		const float edge = (float)std::max(1, scale);

		if (item_level == MINIMAP_LEVEL_SAME && !chest)
			Fill(panel, x, y, size, size, color);
		else
		{
			// hollow: chests always, other items when on another floor
			const float box = chest ? size + 2.f * scale : size;
			const float bx = chest ? x - scale : x, by = chest ? y - scale : y;
			Fill(panel, bx, by, box, edge, color);
			Fill(panel, bx, by + box - edge, box, edge, color);
			Fill(panel, bx, by, edge, box, color);
			Fill(panel, bx + box - edge, by, edge, box, color);
		}
		// small tick above or below for items on another floor
		if (item_level == MINIMAP_LEVEL_ABOVE)
			Fill(panel, x + size * 0.25f, y - 3.f * scale, size * 0.5f, edge, color);
		else if (item_level == MINIMAP_LEVEL_BELOW)
			Fill(panel, x + size * 0.25f, y + size + 2.f * scale, size * 0.5f, edge, color);
	}
}

void DrawPlayer(const panel_t &panel, float player_x, float player_y, float yaw, int32_t scale)
{
	const float x = ScreenX(panel, player_x);
	const float y = ScreenY(panel, player_y);
	const float size = 4.f * scale;
	Fill(panel, x - size * 0.5f, y - size * 0.5f, size, size, color_player);
	// facing: a dot ahead of the player
	const float radians = DEG2RAD(yaw);
	const float ahead = 6.f * scale;
	Fill(panel, x + cosf(radians) * ahead - scale, y - sinf(radians) * ahead - scale, 2.f * scale, 2.f * scale, color_player);
}

void DrawFrame(const panel_t &panel, int32_t scale)
{
	cgi.SCR_DrawColorPic((int)panel.left, (int)panel.top, (int)panel.size, (int)panel.size, "_white", color_background);
	for (float y = panel.top; y < panel.top + panel.size; y += 3.f * scale)
		cgi.SCR_DrawColorPic((int)panel.left, (int)y, (int)panel.size, std::max(1, scale / 2), "_white", color_scanline);
	const int32_t edge = std::max(1, scale);
	cgi.SCR_DrawColorPic((int)panel.left, (int)panel.top, (int)panel.size, edge, "_white", color_frame);
	cgi.SCR_DrawColorPic((int)panel.left, (int)(panel.top + panel.size - edge), (int)panel.size, edge, "_white", color_frame);
	cgi.SCR_DrawColorPic((int)panel.left, (int)panel.top, edge, (int)panel.size, "_white", color_frame);
	cgi.SCR_DrawColorPic((int)(panel.left + panel.size - edge), (int)panel.top, edge, (int)panel.size, "_white", color_frame);
	cgi.SCR_DrawFontString("N", (int)(panel.left + panel.size * 0.5f), (int)(panel.top + 2 * scale), scale, color_label, true, text_align_t::CENTER);
}
} // namespace

// called from CG_DrawHUD with the other q2rpg HUD pieces
void CG_Loot_DrawMinimap(const player_state_t *ps, const vrect_t &hud_vrect, int32_t scale, int32_t playernum)
{
	if (playernum < 0 || playernum >= MINIMAP_MAX_ITEM_PLAYERS)
		return;
	const char *items = cgi.get_configstring(CONFIG_LOOT_MAP_ITEMS + playernum);
	if (!items || !*items)
		return;
	const int32_t mode = Minimap_DecodeDigit(items[0]);
	if (mode == MINIMAP_MODE_OFF)
		return;
	if (mode == MINIMAP_MODE_FULL && (ps->stats[STAT_LAYOUTS] & LAYOUTS_LAYOUT)) // a menu is up: it wins
		return;
	if (!ReadGrid())
		return;

	const float player_x = ps->pmove.origin[0];
	const float player_y = ps->pmove.origin[1];
	const float feet_z = ps->pmove.origin[2] - 24.f;

	panel_t panel{};
	panel.line = std::max(1, scale);
	if (mode == MINIMAP_MODE_FULL)
	{
		// whole grid, centered on screen
		panel.size = std::min(hud_vrect.width, hud_vrect.height) * 0.8f * scale;
		panel.left = (hud_vrect.x + hud_vrect.width * 0.5f) * scale - panel.size * 0.5f;
		panel.top = (hud_vrect.y + hud_vrect.height * 0.5f) * scale - panel.size * 0.5f;
		// zoom to the explored cells (plus a 2-cell margin) so a half-explored map fills the panel
		int32_t min_x = MINIMAP_SIZE, min_y = MINIMAP_SIZE, max_x = -1, max_y = -1;
		for (int32_t y = 0; y < MINIMAP_SIZE; y++)
			for (int32_t x = 0; x < MINIMAP_SIZE; x++)
				if (CellAt(x, y))
				{
					min_x = std::min(min_x, x);
					min_y = std::min(min_y, y);
					max_x = std::max(max_x, x);
					max_y = std::max(max_y, y);
				}
		if (max_x < 0) // nothing explored yet: the whole grid
		{
			min_x = min_y = 0;
			max_x = max_y = MINIMAP_SIZE - 1;
		}
		const float span_cells = (float)std::max({max_x - min_x + 1, max_y - min_y + 1, 12}) + 4.f;
		panel.world_x = grid.origin_x + (min_x + max_x + 1) * 0.5f * grid.cell_size;
		panel.world_y = grid.origin_y + (min_y + max_y + 1) * 0.5f * grid.cell_size;
		panel.pixels_per_unit = panel.size / (span_cells * grid.cell_size);
	}
	else
	{
		// corner radar: top right, about 20 cells across, following the player
		panel.size = 120.f * scale;
		panel.left = (hud_vrect.x + hud_vrect.width - 128) * scale;
		panel.top = (hud_vrect.y + 8) * scale;
		panel.world_x = player_x;
		panel.world_y = player_y;
		panel.pixels_per_unit = panel.size / (20.f * grid.cell_size);
	}
	panel.center_x = panel.left + panel.size * 0.5f;
	panel.center_y = panel.top + panel.size * 0.5f;

	DrawFrame(panel, scale);
	DrawCells(panel, feet_z);
	DrawItems(panel, items + 1, scale);
	DrawPlayer(panel, player_x, player_y, ps->viewangles[YAW], scale);
}
