// q2rpg: drawn menu widgets, shared by the server (g_gui.h builds the layout) and the HUD
// (cg_gui.cpp draws it). Sizes are in virtual 320x240 menu pixels.
//
// Layout tokens (all strings quoted; the engine caps a layout at 1024 chars, so rows don't
// carry positions: each one stacks under the last):
//   gpanel W H "TITLE" "RIGHT"   panel at the current xv/yv; title left, RIGHT (money) right
//   gtabs ACTIVE "A|B|C"         page tabs under the title bar; ACTIVE = highlighted index
//   gitem KEY RAR STATE "NAME" "PRICE" "SUB"
//                                one row: key badge, NAME in rarity color (RAR -1 = plain),
//                                PRICE right-aligned (STATE: 0 can't afford, 1 can, 2 off).
//                                SUB is a loot_str line under it (^g/^r colors); "" for none.
//                                SUB "#3/5 text" draws 5 rank pips (3 lit) then text.
//   gtext "TEXT"                 info line (loot_str colors)
//   ggap                         small vertical gap
//   gkeys "K|label|K|label"      footer hint chips, pinned to the panel bottom
#pragma once

#include <cstdint>

constexpr int32_t GUI_X = 8;   // panel left edge (xv)
constexpr int32_t GUI_Y = 8;   // panel top edge (yv)
constexpr int32_t GUI_W = 304; // panel width

constexpr int32_t GUI_PAD = 4;        // inner padding
constexpr int32_t GUI_TITLE_H = 14;   // title bar
constexpr int32_t GUI_TABS_H = 13;    // tab strip
constexpr int32_t GUI_ROW_H = 12;     // gitem without SUB
constexpr int32_t GUI_ROW_SUB_H = 21; // gitem with SUB
constexpr int32_t GUI_ROW_GAP = 2;    // between rows
constexpr int32_t GUI_TEXT_H = 11;    // gtext
constexpr int32_t GUI_GAP_H = 5;      // ggap
constexpr int32_t GUI_KEYS_H = 16;    // footer strip

enum gui_state_t : int32_t
{
	GUI_CANT_AFFORD,
	GUI_AFFORD,
	GUI_OFF, // sold, maxed or not usable
};

constexpr int32_t GUI_NO_RARITY = -1;
