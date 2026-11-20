// q2rpg: minimap wire format, shared by the server (g_minimap.cpp) and the HUD (cg_minimap.cpp).
// The explored grid travels in configstrings so remote co-op clients get it too.
#pragma once

#include <cstdint>

constexpr int32_t MINIMAP_SIZE = 64; // grid is MINIMAP_SIZE x MINIMAP_SIZE cells
// 2 chars per cell; 94 chars + NUL fit a 96-byte configstring slot. A full 96-char string has no NUL
// and runs into the next slot when the engine resends configstrings (save load, a client joining).
constexpr int32_t MINIMAP_CELLS_PER_STRING = 47;
constexpr int32_t MINIMAP_GRID_STRINGS = (MINIMAP_SIZE * MINIMAP_SIZE + MINIMAP_CELLS_PER_STRING - 1) / MINIMAP_CELLS_PER_STRING;
constexpr int32_t MINIMAP_HEIGHT_STEP = 24; // floor heights are stored in 24-unit steps
constexpr int32_t MINIMAP_HEIGHT_STEPS = 128;
constexpr int32_t MINIMAP_MAX_ITEM_PLAYERS = 8; // players with an item list (configstrings are scarce)
constexpr int32_t MINIMAP_MAX_ITEMS = 30;       // 3 chars each after the mode char

// cell flags
enum minimap_cell_flag_t : int32_t
{
	MINIMAP_WALL_EAST = 1,  // blocked between this cell and the one at +x
	MINIMAP_WALL_NORTH = 2, // blocked between this cell and the one at +y
	MINIMAP_LAVA = 4,       // lava or slime on the floor
	MINIMAP_WATER = 8,
	MINIMAP_SOLID = 16, // inside a wall: no floor
};

// item marker types
enum minimap_item_t : int32_t
{
	MINIMAP_ITEM_HEALTH,
	MINIMAP_ITEM_ARMOR,
	MINIMAP_ITEM_AMMO,
	MINIMAP_ITEM_WEAPON,
	MINIMAP_ITEM_KEY,
	MINIMAP_ITEM_POWERUP,
	MINIMAP_ITEM_LOOT, // + rarity (0-5)
	MINIMAP_ITEM_CHEST = MINIMAP_ITEM_LOOT + 6,
	MINIMAP_ITEM_RED_CHEST,
	MINIMAP_ITEM_COUNT
};

// item height relative to the player: drawn hollow with a tick when on another floor
enum minimap_item_level_t : int32_t
{
	MINIMAP_LEVEL_SAME,
	MINIMAP_LEVEL_ABOVE,
	MINIMAP_LEVEL_BELOW,
};

// display mode, first char of the player's item string
enum minimap_mode_t : int32_t
{
	MINIMAP_MODE_CORNER,
	MINIMAP_MODE_FULL,
	MINIMAP_MODE_OFF,
	MINIMAP_MODE_COUNT
};

// one char = one base-92 digit: printable ASCII 33-126 without '"' and '\'
constexpr int32_t MINIMAP_BASE = 92;

inline char Minimap_EncodeDigit(int32_t value)
{
	int32_t c = 33 + value;
	if (c >= '"')
		c++;
	if (c >= '\\')
		c++;
	return (char)c;
}

inline int32_t Minimap_DecodeDigit(char c)
{
	int32_t value = (unsigned char)c;
	if (value > '\\')
		value--;
	if (value > '"')
		value--;
	return value - 33;
}

// a cell is 0 when unexplored, else 1 + height_step * 32 + flags
inline int32_t Minimap_PackCell(bool known, int32_t height_step, int32_t flags)
{
	return known ? 1 + height_step * 32 + flags : 0;
}
