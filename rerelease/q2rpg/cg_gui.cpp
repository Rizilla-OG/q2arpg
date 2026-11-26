// q2rpg: drawn menu widgets, HUD side. The server sends a layout with the tokens in
// gui_shared.h (built by g_gui.h); CG_ExecuteLayoutString hands them here. Rows stack
// down the open panel, so the layout only carries positions for the panel itself.

#include "../cg_local.h"
#include "gui_shared.h"

void CG_Loot_DrawColorString(int x, int y, int scale, const char *text, bool kfont, int font_y_offset);

namespace
{
constexpr rgba_t col_shadow{0, 0, 0, 120};
constexpr rgba_t col_body{14, 16, 20, 235};
constexpr rgba_t col_border{70, 80, 92, 255};
constexpr rgba_t col_title{28, 34, 42, 255};
constexpr rgba_t col_accent{220, 160, 40, 255};
constexpr rgba_t col_row{24, 28, 34, 230};
constexpr rgba_t col_badge{50, 60, 74, 255};
constexpr rgba_t col_tab{22, 26, 32, 255};
constexpr rgba_t col_tab_on{52, 62, 78, 255};
constexpr rgba_t col_dim{140, 145, 152, 255};
constexpr rgba_t col_off{100, 102, 106, 255};
constexpr rgba_t col_green{112, 255, 52, 255}; // same as layout string2
constexpr rgba_t col_red{255, 90, 80, 255};
constexpr rgba_t col_pip_off{48, 54, 62, 255};

// Common, Uncommon, Rare, Epic, Legendary, Mythic
constexpr rgba_t rarity_colors[] = {
    {215, 215, 215, 255},
    {90, 220, 90, 255},
    {80, 150, 255, 255},
    {185, 105, 255, 255},
    {255, 170, 40, 255},
    {90, 240, 230, 255},
};

struct panel_t
{
	int x, y, w, h; // pixels
	int cursor;     // next row top, pixels
};

panel_t panel;
int u = 1; // one virtual pixel, in screen pixels
bool kfont;
int font_y_offset;

void Fill(int x, int y, int w, int h, const rgba_t &c)
{
	cgi.SCR_DrawColorPic(x, y, w, h, "_white", c);
}

void Frame(int x, int y, int w, int h, const rgba_t &c)
{
	Fill(x, y, w, u, c);
	Fill(x, y + h - u, w, u, c);
	Fill(x, y, u, h, c);
	Fill(x + w - u, y, u, h, c);
}

int TextWidth(const char *s)
{
	if (kfont)
		return (int)cgi.SCR_MeasureFontString(s, u).x;
	return (int)strlen(s) * 8 * u;
}

void Text(int x, int y, const char *s, const rgba_t &c)
{
	if (kfont)
	{
		cgi.SCR_DrawFontString(s, x, y - font_y_offset * u, u, c, true, text_align_t::LEFT);
		return;
	}
	// classic charset only has white and green
	const bool alt = c.g == col_green.g && c.r == col_green.r;
	for (const char *p = s; *p; p++, x += 8 * u)
		cgi.SCR_DrawChar(x, y, u, *p ^ (alt ? 0x80 : 0), true);
}

std::string Next(const char **s)
{
	return COM_Parse(s);
}

void Panel(int x, int y, int w, int h, const std::string &title, const std::string &right)
{
	panel = {x, y, w * u, h * u, y + GUI_TITLE_H * u};
	Fill(x + 3 * u, y + 3 * u, panel.w, panel.h, col_shadow);
	Fill(x, y, panel.w, panel.h, col_body);
	Fill(x, y, panel.w, GUI_TITLE_H * u, col_title);
	Fill(x, y + (GUI_TITLE_H - 1) * u, panel.w, u, col_accent);
	Frame(x, y, panel.w, panel.h, col_border);
	Text(x + GUI_PAD * u + u, y + 3 * u, title.c_str(), col_accent);
	Text(x + panel.w - GUI_PAD * u - u - TextWidth(right.c_str()), y + 3 * u, right.c_str(), col_green);
}

void Tabs(int active, const std::string &list)
{
	std::vector<std::string> names;
	for (size_t start = 0;;)
	{
		const size_t bar = list.find('|', start);
		names.push_back(list.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
		if (bar == std::string::npos)
			break;
		start = bar + 1;
	}

	const int left = panel.x + GUI_PAD * u;
	const int total = panel.w - 2 * GUI_PAD * u;
	const int y = panel.cursor + 2 * u;
	const int h = (GUI_TABS_H - 2) * u;
	for (size_t i = 0; i < names.size(); i++)
	{
		const int x0 = left + total * (int)i / (int)names.size();
		const int x1 = left + total * (int)(i + 1) / (int)names.size();
		const bool on = (int)i == active;
		Fill(x0 + u, y, x1 - x0 - 2 * u, h, on ? col_tab_on : col_tab);
		if (on)
			Fill(x0 + u, y + h - u, x1 - x0 - 2 * u, u, col_accent);
		const int tw = TextWidth(names[i].c_str());
		Text(x0 + (x1 - x0 - tw) / 2, y + 2 * u, names[i].c_str(), on ? rgba_white : col_dim);
	}
	panel.cursor += GUI_TABS_H * u + GUI_PAD * u;
}

// key in a small box; returns its width
int Badge(int x, int y, const char *key)
{
	const int w = std::max(10 * u, TextWidth(key) + 4 * u);
	Fill(x, y, w, 10 * u, col_badge);
	Text(x + (w - TextWidth(key)) / 2, y + u, key, rgba_white);
	return w;
}

void Item(const std::string &key, int rarity, int state, const std::string &name, const std::string &price, const std::string &sub)
{
	const int x = panel.x + GUI_PAD * u;
	const int w = panel.w - 2 * GUI_PAD * u;
	const int y = panel.cursor;
	const int h = (sub.empty() ? GUI_ROW_H : GUI_ROW_SUB_H) * u;
	const bool off = state == GUI_OFF;
	const bool has_rarity = rarity >= 0 && rarity < (int)q_countof(rarity_colors);

	Fill(x, y, w, h, col_row);
	Fill(x, y, 2 * u, h, has_rarity ? rarity_colors[rarity] : col_border); // rarity stripe

	if (!key.empty() && key != "-")
		Badge(x + 5 * u, y + u, key.c_str());

	const rgba_t name_color = off ? col_off : has_rarity ? rarity_colors[rarity]
	                                                     : rgba_white;
	Text(x + 19 * u, y + 2 * u, name.c_str(), name_color);

	const rgba_t price_color = off ? col_off : state == GUI_AFFORD ? col_green
	                                                               : col_red;
	Text(x + w - 4 * u - TextWidth(price.c_str()), y + 2 * u, price.c_str(), price_color);

	if (!sub.empty())
	{
		int sx = x + 19 * u;
		const int sy = y + 11 * u;
		const char *rest = sub.c_str();
		if (rest[0] == '#') // "#rank/max text": rank pips first
		{
			char *end;
			const int rank = (int)strtol(rest + 1, &end, 10);
			const int max = *end == '/' ? (int)strtol(end + 1, &end, 10) : 0;
			for (int i = 0; i < max; i++, sx += 8 * u)
				Fill(sx, sy + 2 * u, 6 * u, 4 * u, i < rank ? col_accent : col_pip_off);
			sx += 4 * u;
			rest = *end == ' ' ? end + 1 : end;
		}
		CG_Loot_DrawColorString(sx, sy, u, rest, kfont, font_y_offset);
	}
	panel.cursor += h + GUI_ROW_GAP * u;
}

void Keys(const std::string &list)
{
	const int top = panel.y + panel.h - GUI_KEYS_H * u;
	Fill(panel.x + u, top, panel.w - 2 * u, u, col_border);

	int x = panel.x + GUI_PAD * u + u;
	const int y = top + 3 * u;
	bool is_key = true;
	for (size_t start = 0;;)
	{
		const size_t bar = list.find('|', start);
		const std::string part = list.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
		if (is_key)
			x += Badge(x, y, part.c_str()) + 3 * u;
		else
		{
			Text(x, y + u, part.c_str(), col_dim);
			x += TextWidth(part.c_str()) + 9 * u;
		}
		is_key = !is_key;
		if (bar == std::string::npos)
			break;
		start = bar + 1;
	}
}
} // namespace

// Called for every layout token; returns true when the token was a widget (parsed and,
// unless skip, drawn). x / y are the current layout position in pixels.
bool CG_Gui_Token(const char *token, const char **s, int x, int y, int scale, bool skip, bool use_kfont, int y_offset)
{
	enum
	{
		T_PANEL,
		T_TABS,
		T_ITEM,
		T_TEXT,
		T_GAP,
		T_KEYS,
		T_NONE
	} t = !strcmp(token, "gpanel")  ? T_PANEL
	      : !strcmp(token, "gtabs") ? T_TABS
	      : !strcmp(token, "gitem") ? T_ITEM
	      : !strcmp(token, "gtext") ? T_TEXT
	      : !strcmp(token, "ggap")  ? T_GAP
	      : !strcmp(token, "gkeys") ? T_KEYS
	                                : T_NONE;
	if (t == T_NONE)
		return false;

	u = std::max(1, scale);
	kfont = use_kfont;
	font_y_offset = y_offset;

	// parse every argument even when skipped, so the layout stays in step
	switch (t)
	{
		case T_PANEL:
		{
			const int w = atoi(Next(s).c_str());
			const int h = atoi(Next(s).c_str());
			const std::string title = Next(s);
			const std::string right = Next(s);
			if (!skip)
				Panel(x, y, w, h, title, right);
			break;
		}
		case T_TABS:
		{
			const int active = atoi(Next(s).c_str());
			const std::string list = Next(s);
			if (!skip)
				Tabs(active, list);
			break;
		}
		case T_ITEM:
		{
			const std::string key = Next(s);
			const int rarity = atoi(Next(s).c_str());
			const int state = atoi(Next(s).c_str());
			const std::string name = Next(s);
			const std::string price = Next(s);
			const std::string sub = Next(s);
			if (!skip)
				Item(key, rarity, state, name, price, sub);
			break;
		}
		case T_TEXT:
		{
			const std::string text = Next(s);
			if (!skip)
			{
				CG_Loot_DrawColorString(panel.x + GUI_PAD * u + 2 * u, panel.cursor + u, u, text.c_str(), kfont, font_y_offset);
				panel.cursor += GUI_TEXT_H * u;
			}
			break;
		}
		case T_GAP:
			if (!skip)
				panel.cursor += GUI_GAP_H * u;
			break;
		case T_KEYS:
		{
			const std::string list = Next(s);
			if (!skip)
				Keys(list);
			break;
		}
		default:
			break;
	}
	return true;
}
