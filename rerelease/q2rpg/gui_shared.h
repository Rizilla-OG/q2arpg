// q2rpg: drawn menu widgets, shared by the server (g_gui.h builds the layout) and the HUD
// (cg_gui.cpp draws it). Sizes are in virtual 320x240 menu pixels.
//
// Layout tokens (all strings quoted; the engine caps a layout at 1024 chars, so rows don't
// carry positions: each one stacks under the last):
//   gpanel W H "TITLE" "RIGHT"   panel at the current xv/yv; title left, RIGHT (money) right;
//                                no title bar when both are ""
//   gtabs ACTIVE "A|B|C"         page tabs under the title bar; ACTIVE = highlighted index
//   gitem KEY RAR STATE ICON "NAME" "PRICE" "SUB"
//                                one row: key badge ("" = none), ICON (gui_icon_t, 0 = none),
//                                NAME in rarity color (RAR -1 = plain),
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
//   ghero ICON RAR "NAME" "SUB"  card header: big icon, NAME in rarity color, SUB under it
//   gprog STATE CUR MAX "NAME" "GOAL" "RIGHT"
//                                progress row: NAME, GOAL (dim), a bar filled CUR/MAX with
//                                RIGHT written on it (STATE: 0 open, 1 some done, 2 all done)
//   loot_tabs "a b"              (cg_loot.cpp) tab stops for '|' in later gtext lines
#pragma once

#include <cstdint>

constexpr int32_t GUI_Y = 8;        // panel top edge (yv)
constexpr int32_t GUI_W = 304;      // default panel width; panels are centered on the 320-wide menu area
constexpr int32_t GUI_W_WIDE = 360; // skills: still fits a 16:10 screen

constexpr int32_t GUI_PAD = 4;            // inner padding
constexpr int32_t GUI_TITLE_H = 14;       // title bar
constexpr int32_t GUI_TABS_H = 13;        // tab strip
constexpr int32_t GUI_ROW_H = 12;         // gitem without SUB
constexpr int32_t GUI_ROW_SUB_H = 20;     // gitem with SUB
constexpr int32_t GUI_ROW_GAP = 1;        // between rows
constexpr int32_t GUI_TEXT_H = 11;        // gtext
constexpr int32_t GUI_GAP_H = 5;          // ggap
constexpr int32_t GUI_KEYS_H = 16;        // footer strip
constexpr int32_t GUI_HEAD_H = 11;        // ghead
constexpr int32_t GUI_SKILL_H = 12;       // gskill
constexpr int32_t GUI_SKILL_GAP = 1;      // between gskill rows
constexpr int32_t GUI_SKILL_DESC_X = 128; // DESC column, from the row's left edge
constexpr int32_t GUI_HERO_H = 28;        // ghero
constexpr int32_t GUI_PROG_H = 12;        // gprog
constexpr int32_t GUI_PROG_GAP = 1;       // between gprog rows
constexpr int32_t GUI_PROG_GOAL_X = 112;  // GOAL column, from the row's left edge
constexpr int32_t GUI_PROG_BAR_W = 120;   // bar at the row's right end
constexpr int32_t GUI_CARD_X = 24;        // loot cards sit left of the crosshair (xv / yv)
constexpr int32_t GUI_CARD_Y = 56;
constexpr int32_t GUI_CARD_W = 248;

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

// Item icons. Weapons and gear use the weapon wheel art (pics/wheel/<name>.png), falling back
// to the classic HUD icon (pics/<name>.pcx); a name starting with '#' is drawn as that letter
// in a rarity-colored box (no stock art). The order is part of the wire format: append only.
enum gui_icon_t : int32_t
{
	GUI_ICON_NONE,
	GUI_ICON_BLASTER,
	GUI_ICON_SHOTGUN,
	GUI_ICON_SSHOTGUN,
	GUI_ICON_MACHINEGUN,
	GUI_ICON_CHAINGUN,
	GUI_ICON_GLAUNCHER,
	GUI_ICON_RLAUNCHER,
	GUI_ICON_HYPERBLASTER,
	GUI_ICON_RAILGUN,
	GUI_ICON_BFG,
	GUI_ICON_RIPPER,
	GUI_ICON_PHALANX,
	GUI_ICON_ETF_RIFLE,
	GUI_ICON_PROXLAUNCH,
	GUI_ICON_CHAINFIST,
	GUI_ICON_HEATBEAM,
	GUI_ICON_DISINTEGRATOR,
	GUI_ICON_GRAPPLE,
	GUI_ICON_SHIELD,
	GUI_ICON_GRENADE,
	GUI_ICON_CLASSMOD,
	GUI_ICON_RELIC,
	GUI_ICON_COUNT
};

constexpr const char *gui_icon_names[GUI_ICON_COUNT] = {
    "",
    "w_blaster",
    "w_shotgun",
    "w_sshotgun",
    "w_machinegun",
    "w_chaingun",
    "w_glauncher",
    "w_rlauncher",
    "w_hyperblaster",
    "w_railgun",
    "w_bfg",
    "w_ripper",
    "w_phallanx",
    "w_etf_rifle",
    "w_proxlaunch",
    "w_chainfist",
    "w_heatbeam",
    "w_disintegrator",
    "w_grapple",
    "i_powershield",
    "a_grenades",
    "#C",
    "#R",
};
