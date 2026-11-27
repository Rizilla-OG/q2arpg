// q2rpg: drawn menu widgets, HUD side. The server sends a layout with the tokens in
// gui_shared.h (built by g_gui.h); CG_ExecuteLayoutString hands them here. Rows stack
// down the open panel, so the layout only carries positions for the panel itself.

#include "../cg_local.h"
#include "gui_shared.h"

void CG_Loot_DrawColorString(int x, int y, int scale, const char *text, bool kfont, int font_y_offset);

// q2rpg: key hints follow the player's binds. The server writes the default key (the one
// autoexec.cfg binds); the HUD looks up what the command is bound to now and shows that.
// Key badges resolve on their own (a letter, or digits like "1-8" / "1/2" for slot N);
// other text marks a key as ^k plus the default letter ("Press ^kX again").
namespace
{
struct default_key_t
{
	char key;
	const char *command;
};
constexpr default_key_t default_keys[] = {
    {'E', "loot_take"}, {'G', "loot_drop"},   {'I', "loot_inv"}, {'X', "loot_sell"},  {'B', "loot_stash"},
    {'L', "loot_filter"}, {'V', "vendor"},    {'K', "skills"},   {'J', "challenges"}, {'F', "action"},
    {'Q', "grenade"},   {'Z', "loot_melee"}, {'M', "loot_map"}, {'H', "loot_give"},  {'R', "reload"},
};

// what `command` is bound to, cleaned up for drawing; "" when unbound
std::string BoundKey(const char *command)
{
	const char *bound = cgi.CL_GetKeyBinding(command);
	std::string out;
	for (const char *c = bound ? bound : ""; *c; c++)
	{
		if ((unsigned char)*c < ' ' || *c == '"' || *c == '|' || *c == '^' || *c == '%')
			continue;
		out += (char)toupper((unsigned char)*c);
	}
	return out;
}

// the key now doing what default key `key` does; the default itself if it isn't one of ours,
// and "--" if the player unbound the command
std::string ResolveDefaultKey(char key)
{
	key = (char)toupper((unsigned char)key);
	if (key >= '1' && key <= '9')
	{
		const std::string bound = BoundKey(G_Fmt("slot {}", key - '0').data());
		return bound.empty() ? "--" : bound;
	}
	for (const default_key_t &k : default_keys)
		if (k.key == key)
		{
			const std::string bound = BoundKey(k.command);
			return bound.empty() ? "--" : bound;
		}
	return std::string(1, key);
}
} // namespace

// a key badge's label: "V" -> the vendor key, "1-8" / "1/2" -> the slot keys; anything else as is
std::string CG_Gui_KeyLabel(const char *key)
{
	const size_t len = strlen(key);
	if (len == 1 && isalpha((unsigned char)key[0]))
		return ResolveDefaultKey(key[0]);
	if (!len || len > 3 || !(key[0] >= '1' && key[0] <= '9'))
		return key;
	if (len == 1)
		return ResolveDefaultKey(key[0]);
	if (len == 3 && (key[1] == '-' || key[1] == '/') && key[2] >= '1' && key[2] <= '9')
		return ResolveDefaultKey(key[0]) + key[1] + ResolveDefaultKey(key[2]);
	return key;
}

// replaces each ^k<default key> in text with the key bound now
std::string CG_Gui_ResolveKeys(const char *text)
{
	std::string out;
	for (const char *c = text; *c; c++)
	{
		if (c[0] == '^' && c[1] == 'k' && c[2])
		{
			out += ResolveDefaultKey(c[2]);
			c += 2;
			continue;
		}
		out += *c;
	}
	return out;
}

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

// width of a loot_str line: color codes skipped, '|' jumps to the tab stops (virtual px)
int ColorTextWidth(const char *str, const std::vector<int> &tabs = {})
{
	int x = 0;
	size_t next_tab = 0;
	std::string seg;
	auto flush = [&]()
	{
		x += TextWidth(seg.c_str());
		seg.clear();
	};
	for (const char *c = str; *c; c++)
	{
		if (c[0] == '|' && next_tab < tabs.size())
		{
			flush();
			x = std::max(x + 4 * u, tabs[next_tab++] * u);
			continue;
		}
		if (c[0] == '^' && c[1])
		{
			c++;
			continue;
		}
		seg += *c;
	}
	flush();
	return x;
}

// shortens text to max_w pixels with "..." (color codes kept); '|' lines are left alone
std::string Clip(const std::string &text, int max_w)
{
	if (max_w <= 0 || text.find('|') != std::string::npos || ColorTextWidth(text.c_str()) <= max_w)
		return text;
	std::string out;
	for (size_t i = 0; i < text.size(); i++)
	{
		if (text[i] == '^' && i + 1 < text.size())
		{
			out += text.substr(i, 2);
			i++;
			continue;
		}
		const std::string next = out + text[i] + "...";
		if (ColorTextWidth(next.c_str()) > max_w)
			break;
		out += text[i];
	}
	return out + "...";
}

std::vector<int> ParseTabs(const std::string &list)
{
	std::vector<int> out;
	for (const char *p = list.c_str(); *p;)
	{
		char *end;
		const long v = strtol(p, &end, 10);
		if (end == p)
			break;
		out.push_back((int)v);
		p = end;
	}
	return out;
}

std::vector<std::string> SplitBars(const std::string &list)
{
	std::vector<std::string> out;
	for (size_t start = 0;;)
	{
		const size_t bar = list.find('|', start);
		out.push_back(list.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
		if (bar == std::string::npos)
			break;
		start = bar + 1;
	}
	return out;
}

// key in a small box (shows the player's bound key, see CG_Gui_KeyLabel); returns its width
int BadgeWidth(const char *key)
{
	return std::max(10 * u, TextWidth(CG_Gui_KeyLabel(key).c_str()) + 4 * u);
}

int Badge(int x, int y, const char *key)
{
	const std::string label = CG_Gui_KeyLabel(key);
	const int w = BadgeWidth(key);
	Fill(x, y, w, 10 * u, col_badge);
	Text(x + (w - TextWidth(label.c_str())) / 2, y + u, label.c_str(), rgba_white);
	return w;
}

// where a gitem row's name starts, from the row's left edge
int ItemTextX(const std::string &key, int icon, bool has_sub)
{
	int x = 6 * u + (!key.empty() && key != "-" ? BadgeWidth(key.c_str()) + 3 * u : 13 * u);
	if (icon > GUI_ICON_NONE)
		x += (has_sub ? 18 : 10) * u + 4 * u;
	return x;
}

int KeysWidth(const std::string &list)
{
	int x = GUI_PAD * u + u;
	bool is_key = true;
	for (const std::string &part : SplitBars(list))
	{
		x += is_key ? BadgeWidth(part.c_str()) + 3 * u : TextWidth(part.c_str()) + 9 * u;
		is_key = !is_key;
	}
	return x + GUI_PAD * u;
}

// Columns sized by measuring the panel's rows with the real font (set when the panel opens).
struct columns_t
{
	int skill_desc_x; // from the row's left edge
	int prog_goal_x;
	int prog_bar_w;
};
columns_t columns;

// Reads the rest of the layout after gpanel and returns the panel width (pixels) its rows need.
// Also sizes the skill and progress columns.
int MeasureRows(const char *p)
{
	int need = 0; // row width needed
	int skill_name = 0, skill_desc = 0, skill_rank = 0;
	int prog_name = 0, prog_goal = 0, prog_right = 0;
	int panel_need = 0; // whole-panel extras (tabs, keys)
	std::vector<int> tabs;
	char buf[MAX_TOKEN_CHARS];
	auto next = [&]()
	{ return std::string(COM_Parse(&p, buf, sizeof(buf))); };

	while (p)
	{
		const std::string t = next();
		if (t.empty())
			continue;
		if (t == "gitem")
		{
			const std::string key = next();
			next(); // rarity
			next(); // state
			const int icon = atoi(next().c_str());
			const std::string name = next(), price = next(), sub = next();
			const int tx = ItemTextX(key, icon, !sub.empty());
			need = std::max(need, tx + TextWidth(name.c_str()) + 8 * u + TextWidth(price.c_str()) + 4 * u);
			int sub_w = ColorTextWidth(sub[0] == '#' ? sub.c_str() + sub.find(' ') + 1 : sub.c_str());
			if (sub[0] == '#') // rank pips
				sub_w += 8 * u * atoi(sub.c_str() + sub.find('/') + 1) + 4 * u;
			need = std::max(need, tx + sub_w + 4 * u);
		}
		else if (t == "gtext")
			need = std::max(need, 2 * u + ColorTextWidth(next().c_str(), tabs) + 4 * u);
		else if (t == "loot_tabs")
			tabs = ParseTabs(next());
		else if (t == "gskill")
		{
			const std::string key = next();
			next(); // state
			const std::string name = next(), rank = next(), desc = next();
			skill_name = std::max(skill_name, TextWidth(name.c_str()));
			skill_rank = std::max(skill_rank, ColorTextWidth(rank.c_str()));
			skill_desc = std::max(skill_desc, TextWidth(desc.c_str()));
		}
		else if (t == "gprog")
		{
			next(), next(), next(); // state, cur, max
			const std::string name = next(), goal = next(), right = next();
			prog_name = std::max(prog_name, TextWidth(name.c_str()));
			prog_goal = std::max(prog_goal, TextWidth(goal.c_str()));
			prog_right = std::max(prog_right, ColorTextWidth(right.c_str()));
		}
		else if (t == "ghero")
		{
			const int icon = atoi(next().c_str());
			next(); // rarity
			const std::string name = next(), sub = next();
			need = std::max(need, (icon > GUI_ICON_NONE ? 30 : 2) * u + std::max(TextWidth(name.c_str()), ColorTextWidth(sub.c_str())) + 4 * u);
		}
		else if (t == "ghead")
		{
			need = std::max(need, u + TextWidth(next().c_str()) + 24 * u);
			next();
		}
		else if (t == "gtabs")
		{
			next(); // active
			const std::vector<std::string> names = SplitBars(next());
			int widest = 0;
			for (const std::string &n : names)
				widest = std::max(widest, TextWidth(n.c_str()));
			panel_need = std::max(panel_need, (widest + 14 * u) * (int)names.size() + 2 * GUI_PAD * u);
		}
		else if (t == "gkeys")
			panel_need = std::max(panel_need, KeysWidth(next()));
		else if (t == "gpanel")
			break; // another panel: its own measure
	}

	columns.skill_desc_x = 19 * u + skill_name + 10 * u;
	if (skill_name)
		need = std::max(need, columns.skill_desc_x + skill_desc + 10 * u + skill_rank + 4 * u);
	columns.prog_goal_x = 6 * u + prog_name + 10 * u;
	columns.prog_bar_w = std::max(84 * u, prog_right + 12 * u);
	if (prog_name)
		need = std::max(need, columns.prog_goal_x + prog_goal + 10 * u + columns.prog_bar_w + 3 * u);

	return std::max(need + 2 * GUI_PAD * u, panel_need);
}

// screen area the panel must stay inside (pixels)
int screen_x0, screen_w;

// rest = the layout after this token's arguments (the panel's rows)
void Panel(int x, int y, int w, int h, const std::string &title, const std::string &right, const char *rest)
{
	const bool title_bar = !title.empty() || !right.empty();

	// widen to fit the rows and the title, never past the screen; centered menus stay centered
	int pw = w * u;
	const int title_need = title_bar ? (GUI_PAD + 1) * u * 2 + TextWidth(title.c_str()) + 16 * u + TextWidth(right.c_str()) : 0;
	const int fit = std::min(std::max({pw, MeasureRows(rest), title_need}), screen_w - 8 * u);
	if (fit > pw)
	{
		const bool centered = abs(x + pw / 2 - (screen_x0 + screen_w / 2)) < 8 * u;
		if (centered)
			x -= (fit - pw) / 2;
		pw = fit;
		x = std::clamp(x, screen_x0 + 4 * u, screen_x0 + screen_w - 4 * u - pw);
	}

	// rows start a pad under the title bar (tabs, when sent, slot in above that pad)
	panel = {x, y, pw, h * u, y + ((title_bar ? GUI_TITLE_H : 0) + GUI_PAD) * u};
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
	const int right_w = TextWidth(right.c_str());
	Text(x + GUI_PAD * u + u, y + 3 * u, Clip(title, panel.w - right_w - 2 * (GUI_PAD + 1) * u - 8 * u).c_str(), col_accent);
	Text(x + panel.w - GUI_PAD * u - u - right_w, y + 3 * u, right.c_str(), col_green);
}

void Tabs(int active, const std::string &list)
{
	const std::vector<std::string> names = SplitBars(list);
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
		const std::string name = Clip(names[i], x1 - x0 - 6 * u);
		Text(x0 + (x1 - x0 - TextWidth(name.c_str())) / 2, y + 2 * u, name.c_str(), on ? rgba_white : col_dim);
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

	if (!key.empty() && key != "-")
		Badge(x + 6 * u, y + u, key.c_str());
	const int text_x = x + ItemTextX(key, icon, !sub.empty());
	if (icon > GUI_ICON_NONE)
	{
		const int size = (sub.empty() ? 10 : 18) * u;
		Icon(text_x - size - 4 * u, y + (h - size) / 2, size, icon, has_rarity ? rarity_colors[rarity] : rgba_white);
	}

	const int price_w = TextWidth(price.c_str());
	const rgba_t name_color = off ? col_off : has_rarity ? rarity_colors[rarity]
	                                                     : rgba_white;
	Text(text_x, y + 2 * u, Clip(name, x + w - 4 * u - price_w - 8 * u - text_x).c_str(), name_color);

	const rgba_t price_color = off ? col_off : state == GUI_AFFORD ? col_green
	                                                               : col_red;
	Text(x + w - 4 * u - price_w, y + 2 * u, price.c_str(), price_color);

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
		CG_Loot_DrawColorString(sx, sy, u, Clip(rest, x + w - 4 * u - sx).c_str(), kfont, font_y_offset);
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
	for (const std::string &part : SplitBars(list))
	{
		if (is_key)
			x += Badge(x, y, part.c_str()) + 3 * u;
		else
		{
			Text(x, y + u, part.c_str(), col_dim);
			x += TextWidth(part.c_str()) + 9 * u;
		}
		is_key = !is_key;
	}
}

void Head(const std::string &text, bool on)
{
	const int x = panel.x + GUI_PAD * u + u;
	const int y = panel.cursor;
	const std::string shown = Clip(text, panel.w - 2 * GUI_PAD * u - 2 * u);
	Text(x, y + u, shown.c_str(), on ? col_accent : col_dim);
	const int line_x = x + TextWidth(shown.c_str()) + 4 * u;
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
		const std::string label = CG_Gui_KeyLabel(key.c_str());
		const int bw = BadgeWidth(key.c_str());
		Fill(x + 5 * u, y + u, bw, 10 * u, state == GUI_SKILL_CAN_ADD ? col_tab_on : col_badge);
		Text(x + 5 * u + (bw - TextWidth(label.c_str())) / 2, y + 2 * u, label.c_str(), locked ? col_off : rgba_white);
	}

	const int desc_x = x + columns.skill_desc_x;
	const int rank_w = ColorTextWidth(rank.c_str());
	const int rank_x = x + w - 4 * u - rank_w;
	const rgba_t name_color = locked ? col_off : state == GUI_SKILL_MAXED ? col_accent
	                                                                      : rgba_white;
	Text(x + 19 * u, y + 2 * u, Clip(name, desc_x - 6 * u - (x + 19 * u)).c_str(), name_color);
	Text(desc_x, y + 2 * u, Clip(desc, rank_x - 6 * u - desc_x).c_str(), locked ? col_off : col_dim);
	CG_Loot_DrawColorString(rank_x, y + 2 * u, u, rank.c_str(), kfont, font_y_offset);

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
	const int max_w = panel.x + panel.w - GUI_PAD * u - text_x;
	Text(text_x, y + 4 * u, Clip(name, max_w).c_str(), color);
	CG_Loot_DrawColorString(text_x, y + 15 * u, u, Clip(sub, max_w).c_str(), kfont, font_y_offset);
	panel.cursor += GUI_HERO_H * u;
}

void Prog(int state, int cur, int max, const std::string &name, const std::string &goal, const std::string &right)
{
	const int x = panel.x + GUI_PAD * u;
	const int w = panel.w - 2 * GUI_PAD * u;
	const int y = panel.cursor;
	const int h = GUI_PROG_H * u;

	const int bar_w = columns.prog_bar_w;
	const int bar_x = x + w - 3 * u - bar_w;
	const int goal_x = x + columns.prog_goal_x;

	Fill(x, y, w, h, col_row);
	Fill(x, y, 2 * u, h, state == 2 ? col_accent : state == 1 ? col_green
	                                                          : col_border);
	Text(x + 6 * u, y + 2 * u, Clip(name, goal_x - 6 * u - (x + 6 * u)).c_str(), state == 2 ? col_accent : rgba_white);
	Text(goal_x, y + 2 * u, Clip(goal, bar_x - 6 * u - goal_x).c_str(), col_dim);

	Fill(bar_x, y + 2 * u, bar_w, h - 4 * u, col_pip_off);
	const float frac = max > 0 ? std::clamp((float)cur / (float)max, 0.f, 1.f) : 1.f;
	if (frac > 0)
		Fill(bar_x, y + 2 * u, std::max(u, (int)(bar_w * frac)), h - 4 * u, state == 2 ? col_accent : col_bar);
	const std::string label = Clip(right, bar_w - 4 * u);
	Text(bar_x + (bar_w - ColorTextWidth(label.c_str())) / 2, y + 2 * u, label.c_str(), rgba_white);

	panel.cursor += h + GUI_PROG_GAP * u;
}
} // namespace

// Called for every layout token; returns true when the token was a widget (parsed and,
// unless skip, drawn). x / y are the current layout position in pixels; screen_left /
// screen_width bound the panel (pixels).
bool CG_Gui_Token(const char *token, const char **s, int x, int y, int scale, bool skip, bool use_kfont, int y_offset, int screen_left, int screen_width)
{
	if (token[0] != 'g')
		return false;

	u = std::max(1, scale);
	kfont = use_kfont;
	font_y_offset = y_offset;
	screen_x0 = screen_left;
	screen_w = screen_width;

	// every argument is parsed even when skipped, so the layout stays in step
	auto num = [&]()
	{ return atoi(Next(s).c_str()); };

	if (!strcmp(token, "gpanel"))
	{
		const int w = num(), h = num();
		const std::string title = Next(s), right = Next(s);
		if (!skip)
			Panel(x, y, w, h, title, right, *s);
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
			const int tx = panel.x + GUI_PAD * u + 2 * u;
			CG_Loot_DrawColorString(tx, panel.cursor + u, u, Clip(text, panel.x + panel.w - GUI_PAD * u - tx).c_str(), kfont, font_y_offset);
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

//==========================================================================
// HUD: XP bar and action skill meter (top-left, under the gun line)
//==========================================================================

namespace
{
constexpr rgba_t col_xp{120, 95, 255, 255};
constexpr rgba_t col_ready{80, 220, 80, 255};
constexpr rgba_t col_cooldown{90, 110, 140, 255};
constexpr rgba_t col_danger{255, 60, 50, 255};

std::vector<std::string> Fields(const char *cs)
{
	std::vector<std::string> out;
	std::string cur;
	for (const char *c = cs; *c; c++)
	{
		if (*c == '|')
		{
			out.push_back(cur);
			cur.clear();
		}
		else
			cur += *c;
	}
	out.push_back(cur);
	return out;
}

// bar with a centered label; frac 0-1
void Meter(int x, int y, int w, int h, float frac, const rgba_t &fill, const char *label)
{
	Fill(x - u, y - u, w + 2 * u, h + 2 * u, rgba_black);
	Fill(x, y, w, h, col_pip_off);
	frac = std::clamp(frac, 0.f, 1.f);
	if (frac > 0)
		Fill(x, y, std::max(u, (int)(w * frac)), h, fill);
	if (label && *label)
		Text(x + (w - TextWidth(label)) / 2, y + (h - 8 * u) / 2, label, rgba_white);
}

// "level|xp|next|money|unspent|mission"
void DrawXP(int x, int y, const char *cs)
{
	const std::vector<std::string> f = Fields(cs);
	if (f.size() < 6)
		return;
	const int level = atoi(f[0].c_str()), xp = atoi(f[1].c_str()), next = atoi(f[2].c_str());
	const int money = atoi(f[3].c_str()), unspent = atoi(f[4].c_str());

	const std::string badge = G_Fmt("Lv {}", level).data();
	x += Badge(x, y, badge.c_str()) + 3 * u;

	constexpr int bar_w = 110;
	const std::string label = next > 0 ? G_Fmt("{} / {}", xp, next).data() : std::string("MAX");
	Meter(x, y + u, bar_w * u, 8 * u, next > 0 ? (float)xp / next : 1.f, next > 0 ? col_xp : col_accent, label.c_str());
	x += (bar_w + 6) * u;

	const std::string cash = G_Fmt("${}", money).data();
	Text(x, y + u, cash.c_str(), col_green);
	x += TextWidth(cash.c_str()) + 6 * u;

	if (unspent > 0) // skill points waiting: blinking chip
	{
		const bool lit = (cgi.CL_ClientTime() % 1000) < 600;
		const std::string chip = G_Fmt("{} +{}", CG_Gui_KeyLabel("K"), unspent).data();
		const int w = TextWidth(chip.c_str()) + 6 * u;
		Fill(x, y, w, 10 * u, lit ? rgba_t{40, 120, 40, 255} : col_badge);
		Text(x + 3 * u, y + u, chip.c_str(), lit ? rgba_white : col_green);
		x += w + 6 * u;
	}

	if (!f[5].empty())
		CG_Loot_DrawColorString(x, y + u, u, f[5].c_str(), kfont, font_y_offset);
}

// "phase|name|phase length in tenths"; tenths left in STAT_LOOT_ACTION_TIME
void DrawAction(int x, int y, const char *cs, int tenths_left)
{
	const std::vector<std::string> f = Fields(cs);
	if (f.size() < 3 || f[0].empty())
		return;
	const char phase = f[0][0];
	const std::string &name = f[1];
	const int total = std::max(1, atoi(f[2].c_str()));
	const int seconds = (tenths_left + 9) / 10;
	const bool pulse = (cgi.CL_ClientTime() % 500) < 250;

	if (phase != 'X')
		x += Badge(x, y, "F") + 3 * u;
	Text(x, y + u, name.c_str(), phase == 'X' ? col_danger : phase == 'R' ? rgba_white
	                                                                      : col_dim);
	x += std::max(TextWidth(name.c_str()), 58 * u) + 4 * u;

	constexpr int bar_w = 70;
	switch (phase)
	{
		case 'R':
			Meter(x, y + u, bar_w * u, 8 * u, 1.f, col_ready, "READY");
			break;
		case 'C':
			Meter(x, y + u, bar_w * u, 8 * u, 1.f - (float)tenths_left / total, col_cooldown, G_Fmt("{}s", seconds).data());
			break;
		case 'A':
			Meter(x, y + u, bar_w * u, 8 * u, (float)tenths_left / total, col_accent, G_Fmt("ACTIVE {}s", seconds).data());
			break;
		case 'X':
			Meter(x, y + u, bar_w * u, 8 * u, (float)tenths_left / total, pulse ? col_danger : rgba_t{150, 30, 30, 255}, G_Fmt("GET A KILL {}s", seconds).data());
			break;
		default:
			break;
	}
}
} // namespace

// called from CG_DrawHUD with the rest of the q2rpg HUD
void CG_Gui_DrawHud(const player_state_t *ps, const vrect_t &hud_vrect, const vrect_t &hud_safe, int32_t scale, bool use_kfont, int y_offset)
{
	const char *enabled = cgi.get_configstring(CONFIG_LOOT_ENABLED);
	if (!enabled || enabled[0] != '1') // loot stats share CTF slots; only trust them in SP/coop
		return;

	u = std::max(1, scale);
	kfont = use_kfont;
	font_y_offset = y_offset;
	const int x = (hud_vrect.x + 8) * u + hud_safe.x;

	if (const int cs = ps->stats[STAT_LOOT_XP]; cs > 0 && cs < MAX_CONFIGSTRINGS)
		DrawXP(x, (hud_vrect.y + 18) * u + hud_safe.y, cgi.get_configstring(cs));
	if (const int cs = ps->stats[STAT_LOOT_ACTION]; cs > 0 && cs < MAX_CONFIGSTRINGS)
		DrawAction(x, (hud_vrect.y + 46) * u + hud_safe.y, cgi.get_configstring(cs), ps->stats[STAT_LOOT_ACTION_TIME]);
}

// Centerprints: one dark box behind all lines (sized to the full text, so it doesn't grow
// while the message types out). Only in SP/coop, like the rest of the q2rpg HUD.
// kfont / alt_typeface match how cg_screen draws the text: the classic font is fixed 8px chars,
// and the alt typeface is measured with it on and drawn 8px higher (like the contrast bars).
// extra_height covers what's drawn under the lines (key-bind prompts).
void CG_Gui_CenterBackdrop(const std::vector<std::string> &lines, int y, int line_height, int center_x, int scale,
                           bool kfont, bool alt_typeface, int extra_height)
{
	const char *enabled = cgi.get_configstring(CONFIG_LOOT_ENABLED);
	if (!enabled || enabled[0] != '1' || lines.empty())
		return;
	int widest = 0;
	cgi.SCR_SetAltTypeface(alt_typeface);
	for (const std::string &line : lines)
	{
		const int lw = kfont ? (int)cgi.SCR_MeasureFontString(line.c_str(), scale).x : (int)line.size() * 8 * scale;
		widest = std::max(widest, lw);
	}
	cgi.SCR_SetAltTypeface(false);
	if (!widest)
		return;
	if (alt_typeface)
		y -= 8;
	const int pad = 6 * scale;
	const int w = widest + 2 * pad;
	const int h = (int)lines.size() * line_height + extra_height + pad;
	cgi.SCR_DrawColorPic(center_x - w / 2, y - pad / 2 - scale, w, h, "_white", {0, 0, 0, 170});
}
