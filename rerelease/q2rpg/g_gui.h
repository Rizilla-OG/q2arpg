// q2rpg: builds a drawn menu page (tokens in gui_shared.h, drawn by cg_gui.cpp).
// Rows are collected first, so the panel (drawn first, behind them) knows its height.
// Include after g_local.h.
#pragma once

#include "gui_shared.h"

struct gui_page_t
{
	std::string rows;
	int32_t height = 0;    // of the rows, without title, tabs and footer
	int32_t width = GUI_W; // wider than 320 only for screens that are 16:9-only friendly
	// placement: centered menu with a dimmed screen, or a loot card (fixed spot, no dim)
	bool card = false;
	// rows past this many chars are left out (with every row after them), so the whole
	// layout stays under the engine's 1024-char limit with room for panel and footer
	size_t rows_budget = 840;
	bool full = false;

	// appends one row's tokens unless the page is full; height only counts rows that fit
	void add(const std::string &tokens, int32_t h)
	{
		if (full || rows.size() + tokens.size() > rows_budget)
		{
			full = true;
			return;
		}
		rows += tokens;
		height += h;
	}

	void item(const std::string &key, int32_t rarity, gui_state_t state, const std::string &name, const std::string &price, const std::string &sub = {},
	          gui_icon_t icon = GUI_ICON_NONE)
	{
		add(G_Fmt("gitem \"{}\" {} {} {} \"{}\" \"{}\" \"{}\" ", key, rarity, (int32_t)state, (int32_t)icon, name, price, sub).data(),
		    (sub.empty() ? GUI_ROW_H : GUI_ROW_SUB_H) + GUI_ROW_GAP);
	}

	void text(const std::string &t)
	{
		add(G_Fmt("gtext \"{}\" ", t).data(), GUI_TEXT_H);
	}

	// tab stops (virtual pixels from the text's left edge) for '|' in the gtext lines after it
	void tabs(const char *stops)
	{
		add(G_Fmt("loot_tabs \"{}\" ", stops).data(), 0);
	}

	void head(const std::string &t, bool on)
	{
		add(G_Fmt("ghead \"{}\" {} ", t, on ? 1 : 0).data(), GUI_HEAD_H);
	}

	void skill(const std::string &key, gui_skill_state_t state, const std::string &name, const std::string &rank, const std::string &desc)
	{
		add(G_Fmt("gskill {} {} \"{}\" \"{}\" \"{}\" ", key, (int32_t)state, name, rank, desc).data(), GUI_SKILL_H + GUI_SKILL_GAP);
	}

	void hero(gui_icon_t icon, int32_t rarity, const std::string &name, const std::string &sub)
	{
		add(G_Fmt("ghero {} {} \"{}\" \"{}\" ", (int32_t)icon, rarity, name, sub).data(), GUI_HERO_H);
	}

	// state: 0 open, 1 some done, 2 all done
	void prog(int32_t state, int32_t cur, int32_t max, const std::string &name, const std::string &goal, const std::string &right)
	{
		add(G_Fmt("gprog {} {} {} \"{}\" \"{}\" \"{}\" ", state, cur, max, name, goal, right).data(), GUI_PROG_H + GUI_PROG_GAP);
	}

	void gap()
	{
		add("ggap ", GUI_GAP_H);
	}

	// tab = -1 hides the tab strip; keys alternate key and label: "1-9|buy|V|close"
	std::string finish(const std::string &title, const std::string &right, int32_t tab, const char *tabs, const std::string &keys) const
	{
		const bool title_bar = !title.empty() || !right.empty();
		const int32_t h = (title_bar ? GUI_TITLE_H : 0) + (tab >= 0 ? GUI_TABS_H : 0) + GUI_PAD + height + GUI_KEYS_H;
		std::string s = card ? G_Fmt("xv {} yv {} ", GUI_CARD_X, GUI_CARD_Y).data() : G_Fmt("loot_dim xv {} yv {} ", 160 - width / 2, GUI_Y).data();
		s += G_Fmt("gpanel {} {} \"{}\" \"{}\" ", width, h, title, right).data();
		if (tab >= 0)
			s += G_Fmt("gtabs {} \"{}\" ", tab, tabs).data();
		s += rows;
		s += G_Fmt("gkeys \"{}\"", keys).data();
		if (s.size() > 1000 || full) // engine layout limit is 1024
			gi.Com_PrintFmt("gui: \"{}\" layout {} chars{}\n", title, s.size(), full ? ", rows left out" : ", near limit");
		return s;
	}
};

// vendor tabs, in the order 9 cycles through (stash is B only, so not a tab)
constexpr const char *VENDOR_TABS = "Stock|Upgrades|Workshop|Missions|Recycler";

inline std::string Gui_Money(int32_t amount)
{
	return G_Fmt("${}", amount).data();
}

// icon for a weapon item ("w_railgun" -> GUI_ICON_RAILGUN); none when it has no wheel art
inline gui_icon_t Gui_IconByName(const char *name)
{
	if (name)
		for (int32_t i = 1; i < GUI_ICON_COUNT; i++)
			if (!strcmp(gui_icon_names[i], name))
				return (gui_icon_t)i;
	return GUI_ICON_NONE;
}

inline gui_icon_t Gui_GunIcon(const loot_weapon_t &g)
{
	const gitem_t *item = g.valid ? GetItemByIndex(g.base) : nullptr;
	return item ? Gui_IconByName(item->icon) : GUI_ICON_NONE;
}

// what a stash slot holds, for its row icon
inline gui_icon_t Gui_StashIcon(const loot_stash_slot_t &slot)
{
	return slot.gun.valid ? Gui_GunIcon(slot.gun) : slot.shield.valid ? GUI_ICON_SHIELD
	                                            : slot.grenade.valid  ? GUI_ICON_GRENADE
	                                            : slot.classmod.valid ? GUI_ICON_CLASSMOD
	                                            : slot.relic.valid    ? GUI_ICON_RELIC
	                                                                  : GUI_ICON_NONE;
}

inline gui_state_t Gui_Afford(int32_t price, int32_t money)
{
	return price <= money ? GUI_AFFORD : GUI_CANT_AFFORD;
}
