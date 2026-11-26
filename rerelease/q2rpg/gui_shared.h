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
//   ghead "TEXT" ON              section header with a rule; ON 0 draws it dimmed (locked)
//   gskill KEY STATE "NAME" "RANK" "DESC"
//                                one-line skill row: NAME, DESC in a second column, RANK
//                                (loot_str colors, e.g. "3^g+1^w/5") right-aligned
#pragma once

#include <cstdint>

constexpr int32_t GUI_Y = 8;        // panel top edge (yv)
constexpr int32_t GUI_W = 304;      // default panel width; panels are centered on the 320-wide menu area
constexpr int32_t GUI_W_WIDE = 360; // skills: still fits a 16:10 screen

constexpr int32_t GUI_PAD = 4;            // inner padding
constexpr int32_t GUI_TITLE_H = 14;       // title bar
constexpr int32_t GUI_TABS_H = 13;        // tab strip
constexpr int32_t GUI_ROW_H = 12;         // gitem without SUB
constexpr int32_t GUI_ROW_SUB_H = 21;     // gitem with SUB
constexpr int32_t GUI_ROW_GAP = 2;        // between rows
constexpr int32_t GUI_TEXT_H = 11;        // gtext
constexpr int32_t GUI_GAP_H = 5;          // ggap
constexpr int32_t GUI_KEYS_H = 16;        // footer strip
constexpr int32_t GUI_HEAD_H = 11;        // ghead
constexpr int32_t GUI_SKILL_H = 12;       // gskill
constexpr int32_t GUI_SKILL_GAP = 1;      // between gskill rows
constexpr int32_t GUI_SKILL_DESC_X = 128; // DESC column, from the row's left edge

enum gui_state_t : int32_t
{
	GUI_CANT_AFFORD,
	GUI_AFFORD,
	GUI_OFF, // sold, maxed or not usable
};

constexpr int32_t GUI_NO_RARITY = -1;

enum gui_skill_state_t : int32_t
{
	GUI_SKILL_LOCKED,  // tier not reached
	GUI_SKILL_CAN_ADD, // a point can go here now
	GUI_SKILL_MAXED,
	GUI_SKILL_OPEN, // open, but no points to spend
};
