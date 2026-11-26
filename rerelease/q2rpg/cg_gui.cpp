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
constexpr rgba_t col_bar{60, 130, 50, 255};

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
	const bool title_bar = !title.empty() || !right.empty();
	// rows start a pad under the title bar (tabs, when sent, slot in above that pad)
	panel = {x, y, w * u, h * u, y + ((title_bar ? GUI_TITLE_H : 0) + GUI_PAD) * u};
	Fill(x + 3 * u, y + 3 * u, panel.w, panel.h, col_shadow);
	Fill(x, y, panel.w, panel.h, col_body);
	if (!title_bar)
	{
		Frame(x, y, panel.w, panel.h, col_border);
		return;
	}
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
	const int y = panel.cursor - GUI_PAD * u + 2 * u;
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
	panel.cursor += GUI_TABS_H * u;
}

// Item icon in a size x size box. Wheel art first, classic HUD icon as fallback; "#X" is a letter.
void Icon(int x, int y, int size, int icon, const rgba_t &tint)
{
	if (icon <= GUI_ICON_NONE || icon >= GUI_ICON_COUNT)
		return;
	const char *name = gui_icon_names[icon];
	if (name[0] == '#')
	{
		Fill(x, y, size, size, col_badge);
		Frame(x, y, size, size, tint);
		const char letter[2] = {name[1], 0};
		Text(x + (size - TextWidth(letter)) / 2, y + (size - 8 * u) / 2, letter, tint);
		return;
	}

	// resolved once per icon: wheel art when it exists, else the classic pic
	static std::array<std::string, GUI_ICON_COUNT> pics;
	std::string &pic = pics[icon];
	if (pic.empty())
	{
		const std::string wheel = std::string("wheel/") + name;
		int w = 0, h = 0;
		if (cgi.Draw_RegisterPic(wheel.c_str()))
			cgi.Draw_GetPicSize(&w, &h, wheel.c_str());
		pic = w > 0 ? wheel : name;
	}
	cgi.SCR_DrawPic(x, y, size, size, pic.c_str());
}

// key in a small box; returns its width
int Badge(int x, int y, const char *key)
{
	const int w = std::max(10 * u, TextWidth(key) + 4 * u);
	Fill(x, y, w, 10 * u, col_badge);
	Text(x + (w - TextWidth(key)) / 2, y + u, key, rgba_white);
	return w;
}

void Item(const std::string &key, int rarity, int state, int icon, const std::string &name, const std::string &price, const std::string &sub)
{
	const int x = panel.x + GUI_PAD * u;
	const int w = panel.w - 2 * GUI_PAD * u;
	const int y = panel.cursor;
	const int h = (sub.empty() ? GUI_ROW_H : GUI_ROW_SUB_H) * u;
	const bool off = state == GUI_OFF;
	const bool has_rarity = rarity >= 0 && rarity < (int)q_countof(rarity_colors);

	Fill(x, y, w, h, col_row);
	Fill(x, y, 2 * u, h, has_rarity ? rarity_colors[rarity] : col_border); // rarity stripe

	int text_x = x + 6 * u;
	if (!key.empty() && key != "-")
		text_x += Badge(text_x, y + u, key.c_str()) + 3 * u;
	else
		text_x += 13 * u; // keep names lined up with keyed rows
	if (icon > GUI_ICON_NONE)
	{
		const int size = (sub.empty() ? 10 : 18) * u;
		Icon(text_x, y + (h - size) / 2, size, icon, has_rarity ? rarity_colors[rarity] : rgba_white);
		text_x += size + 4 * u;
	}

	const rgba_t name_color = off ? col_off : has_rarity ? rarity_colors[rarity]
	                                                     : rgba_white;
	Text(text_x, y + 2 * u, name.c_str(), name_color);

	const rgba_t price_color = off ? col_off : state == GUI_AFFORD ? col_green
	                                                               : col_red;
	Text(x + w - 4 * u - TextWidth(price.c_str()), y + 2 * u, price.c_str(), price_color);

	if (!sub.empty())
	{
		int sx = text_x;
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
// width of a loot_str line, color codes skipped
int ColorTextWidth(const char *str)
{
	std::string plain;
	for (const char *c = str; *c; c++)
	{
		if (c[0] == '^' && c[1])
		{
			c++;
			continue;
		}
		plain += *c;
	}
	return TextWidth(plain.c_str());
}

void Head(const std::string &text, bool on)
{
	const int x = panel.x + GUI_PAD * u + u;
	const int y = panel.cursor;
	Text(x, y + u, text.c_str(), on ? col_accent : col_dim);
	const int line_x = x + TextWidth(text.c_str()) + 4 * u;
	const int right = panel.x + panel.w - GUI_PAD * u;
	if (line_x < right)
		Fill(line_x, y + 5 * u, right - line_x, u, on ? col_border : col_pip_off);
	panel.cursor += GUI_HEAD_H * u;
}

void Skill(const std::string &key, int state, const std::string &name, const std::string &rank, const std::string &desc)
{
	const int x = panel.x + GUI_PAD * u;
	const int w = panel.w - 2 * GUI_PAD * u;
	const int y = panel.cursor;
	const int h = GUI_SKILL_H * u;
	const bool locked = state == GUI_SKILL_LOCKED;

	Fill(x, y, w, h, col_row);
	if (state == GUI_SKILL_CAN_ADD)
		Fill(x, y, 2 * u, h, col_green); // a point can go here
	else if (state == GUI_SKILL_MAXED)
		Fill(x, y, 2 * u, h, col_accent);

	if (!key.empty())
	{
		const int bw = std::max(10 * u, TextWidth(key.c_str()) + 4 * u);
		Fill(x + 5 * u, y + u, bw, 10 * u, state == GUI_SKILL_CAN_ADD ? col_tab_on : col_badge);
		Text(x + 5 * u + (bw - TextWidth(key.c_str())) / 2, y + 2 * u, key.c_str(), locked ? col_off : rgba_white);
	}

	const rgba_t name_color = locked ? col_off : state == GUI_SKILL_MAXED ? col_accent
	                                                                      : rgba_white;
	Text(x + 19 * u, y + 2 * u, name.c_str(), name_color);
	Text(x + GUI_SKILL_DESC_X * u, y + 2 * u, desc.c_str(), locked ? col_off : col_dim);
	CG_Loot_DrawColorString(x + w - 4 * u - ColorTextWidth(rank.c_str()), y + 2 * u, u, rank.c_str(), kfont, font_y_offset);

	panel.cursor += h + GUI_SKILL_GAP * u;
}
// card header: 24px icon, name in rarity color, a dim line under it
void Hero(int icon, int rarity, const std::string &name, const std::string &sub)
{
	const int x = panel.x + GUI_PAD * u;
	const int y = panel.cursor;
	const bool has_rarity = rarity >= 0 && rarity < (int)q_countof(rarity_colors);
	const rgba_t color = has_rarity ? rarity_colors[rarity] : rgba_white;
	int text_x = x + 2 * u;
	if (icon > GUI_ICON_NONE)
	{
		Fill(x, y, 26 * u, 26 * u, col_row);
		Frame(x, y, 26 * u, 26 * u, color);
		Icon(x + u, y + u, 24 * u, icon, color);
		text_x = x + 30 * u;
	}
	Text(text_x, y + 4 * u, name.c_str(), color);
	CG_Loot_DrawColorString(text_x, y + 15 * u, u, sub.c_str(), kfont, font_y_offset);
	panel.cursor += GUI_HERO_H * u;
}

void Prog(int state, int cur, int max, const std::string &name, const std::string &goal, const std::string &right)
{
	const int x = panel.x + GUI_PAD * u;
	const int w = panel.w - 2 * GUI_PAD * u;
	const int y = panel.cursor;
	const int h = GUI_PROG_H * u;

	Fill(x, y, w, h, col_row);
	Fill(x, y, 2 * u, h, state == 2 ? col_accent : state == 1 ? col_green
	                                                          : col_border);
	Text(x + 6 * u, y + 2 * u, name.c_str(), state == 2 ? col_accent : rgba_white);
	Text(x + GUI_PROG_GOAL_X * u, y + 2 * u, goal.c_str(), col_dim);

	const int bar_w = GUI_PROG_BAR_W * u;
	const int bar_x = x + w - 3 * u - bar_w;
	Fill(bar_x, y + 2 * u, bar_w, h - 4 * u, col_pip_off);
	const float frac = max > 0 ? std::clamp((float)cur / (float)max, 0.f, 1.f) : 1.f;
	if (frac > 0)
		Fill(bar_x, y + 2 * u, std::max(u, (int)(bar_w * frac)), h - 4 * u, state == 2 ? col_accent : col_bar);
	Text(bar_x + (bar_w - ColorTextWidth(right.c_str())) / 2, y + 2 * u, right.c_str(), rgba_white);

	panel.cursor += h + GUI_PROG_GAP * u;
}
} // namespace

// Called for every layout token; returns true when the token was a widget (parsed and,
// unless skip, drawn). x / y are the current layout position in pixels.
bool CG_Gui_Token(const char *token, const char **s, int x, int y, int scale, bool skip, bool use_kfont, int y_offset)
{
	if (token[0] != 'g')
		return false;

	u = std::max(1, scale);
	kfont = use_kfont;
	font_y_offset = y_offset;

	// every argument is parsed even when skipped, so the layout stays in step
	auto num = [&]()
	{ return atoi(Next(s).c_str()); };

	if (!strcmp(token, "gpanel"))
	{
		const int w = num(), h = num();
		const std::string title = Next(s), right = Next(s);
		if (!skip)
			Panel(x, y, w, h, title, right);
	}
	else if (!strcmp(token, "gtabs"))
	{
		const int active = num();
		const std::string list = Next(s);
		if (!skip)
			Tabs(active, list);
	}
	else if (!strcmp(token, "gitem"))
	{
		const std::string key = Next(s);
		const int rarity = num(), state = num(), icon = num();
		const std::string name = Next(s), price = Next(s), sub = Next(s);
		if (!skip)
			Item(key, rarity, state, icon, name, price, sub);
	}
	else if (!strcmp(token, "gtext"))
	{
		const std::string text = Next(s);
		if (!skip)
		{
			CG_Loot_DrawColorString(panel.x + GUI_PAD * u + 2 * u, panel.cursor + u, u, text.c_str(), kfont, font_y_offset);
			panel.cursor += GUI_TEXT_H * u;
		}
	}
	else if (!strcmp(token, "ggap"))
	{
		if (!skip)
			panel.cursor += GUI_GAP_H * u;
	}
	else if (!strcmp(token, "gkeys"))
	{
		const std::string list = Next(s);
		if (!skip)
			Keys(list);
	}
	else if (!strcmp(token, "ghead"))
	{
		const std::string text = Next(s);
		const bool on = num() != 0;
		if (!skip)
			Head(text, on);
	}
	else if (!strcmp(token, "gskill"))
	{
		const std::string key = Next(s);
		const int state = num();
		const std::string name = Next(s), rank = Next(s), desc = Next(s);
		if (!skip)
			Skill(key, state, name, rank, desc);
	}
	else if (!strcmp(token, "ghero"))
	{
		const int icon = num(), rarity = num();
		const std::string name = Next(s), sub = Next(s);
		if (!skip)
			Hero(icon, rarity, name, sub);
	}
	else if (!strcmp(token, "gprog"))
	{
		const int state = num(), cur = num(), max = num();
		const std::string name = Next(s), goal = Next(s), right = Next(s);
		if (!skip)
			Prog(state, cur, max, name, goal, right);
	}
	else
		return false;
	return true;
}
