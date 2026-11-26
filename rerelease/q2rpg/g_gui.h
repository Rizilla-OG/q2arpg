// q2rpg: builds a drawn menu page (tokens in gui_shared.h, drawn by cg_gui.cpp).
// Rows are collected first, so the panel (drawn first, behind them) knows its height.
// Include after g_local.h.
#pragma once

#include "gui_shared.h"

struct gui_page_t
{
	std::string rows;
	int32_t height = 0; // of the rows, without title, tabs and footer

	void item(const std::string &key, int32_t rarity, gui_state_t state, const std::string &name, const std::string &price, const std::string &sub = {})
	{
		rows += G_Fmt("gitem {} {} {} \"{}\" \"{}\" \"{}\" ", key, rarity, (int32_t)state, name, price, sub).data();
		height += (sub.empty() ? GUI_ROW_H : GUI_ROW_SUB_H) + GUI_ROW_GAP;
	}

	void text(const std::string &t)
	{
		rows += G_Fmt("gtext \"{}\" ", t).data();
		height += GUI_TEXT_H;
	}

	void gap()
	{
		rows += "ggap ";
		height += GUI_GAP_H;
	}

	// tab = -1 hides the tab strip; keys alternate key and label: "1-9|buy|V|close"
	std::string finish(const std::string &title, const std::string &right, int32_t tab, const char *tabs, const std::string &keys) const
	{
		const int32_t h = GUI_TITLE_H + (tab >= 0 ? GUI_TABS_H : 0) + GUI_PAD + height + GUI_KEYS_H;
		std::string s = G_Fmt("loot_dim xv {} yv {} gpanel {} {} \"{}\" \"{}\" ", GUI_X, GUI_Y, GUI_W, h, title, right).data();
		if (tab >= 0)
			s += G_Fmt("gtabs {} \"{}\" ", tab, tabs).data();
		s += rows;
		s += G_Fmt("gkeys \"{}\"", keys).data();
		if (s.size() > 1000) // engine layout limit is 1024; warn before rows get cut
			gi.Com_PrintFmt("gui: \"{}\" layout {} chars, near limit\n", title, s.size());
		return s;
	}
};

// vendor tabs, in the order 9 cycles through (stash is B only, so not a tab)
constexpr const char *VENDOR_TABS = "Stock|Upgrades|Workshop|Missions";

inline std::string Gui_Money(int32_t amount)
{
	return G_Fmt("${}", amount).data();
}

inline gui_state_t Gui_Afford(int32_t price, int32_t money)
{
	return price <= money ? GUI_AFFORD : GUI_CANT_AFFORD;
}
