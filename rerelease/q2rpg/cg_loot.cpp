// q2rpg: client-side overhead enemy health bar.
// The server packs the target's world position and health into
// STAT_LOOT_BAR_* (see Loot_SetBarStats); this projects it to the screen.

#include "../cg_local.h"

static cvar_t *cl_loot_healthbar;

// Loot stats share slots with CTF stats; only trust them in SP/coop.
static bool CG_Loot_Enabled()
{
	const char *cs = cgi.get_configstring(CONFIG_LOOT_ENABLED);
	return cs && cs[0] == '1';
}
static cvar_t *cl_loot_fov43; // 1 = ps->fov is horizontal fov at 4:3 (hor+), 0 = at screen aspect

// World point -> screen pixels. Returns false when behind the view.
static bool CG_Loot_Project(const player_state_t *ps, const vec3_t &point, const vrect_t &hud_vrect, int32_t scale, float &sx, float &sy)
{
	const vec3_t eye = ps->pmove.origin + ps->viewoffset + vec3_t{0, 0, (float)ps->pmove.viewheight};

	vec3_t forward, right, up;
	AngleVectors(ps->viewangles, forward, right, up);

	const vec3_t d = point - eye;
	const float z = d.dot(forward);
	if (z < 8.f)
		return false;

	const float x = d.dot(right);
	const float y = d.dot(up);

	const float w = (float)hud_vrect.width * scale;
	const float h = (float)hud_vrect.height * scale;
	const float tan_half = tanf(DEG2RAD(ps->fov) * 0.5f);

	float tan_x, tan_y;
	if (cl_loot_fov43->integer)
	{
		tan_y = tan_half * 0.75f;
		tan_x = tan_y * (w / h);
	}
	else
	{
		tan_x = tan_half;
		tan_y = tan_half * (h / w);
	}

	sx = hud_vrect.x * scale + w * 0.5f + (x / z) / tan_x * (w * 0.5f);
	sy = hud_vrect.y * scale + h * 0.5f - (y / z) / tan_y * (h * 0.5f);

	return sx >= 0 && sy >= 0 && sx <= w + hud_vrect.x * scale && sy <= h + hud_vrect.y * scale;
}

void CG_Loot_DrawHealthBar(const player_state_t *ps, const vrect_t &hud_vrect, int32_t scale)
{
	if (!cl_loot_healthbar)
	{
		cl_loot_healthbar = cgi.cvar("cl_loot_healthbar", "1", CVAR_ARCHIVE);
		cl_loot_fov43 = cgi.cvar("cl_loot_fov43", "1", CVAR_ARCHIVE);
	}

	if (!cl_loot_healthbar->integer || !CG_Loot_Enabled())
		return;

	const uint16_t info = (uint16_t)ps->stats[STAT_LOOT_BAR_INFO];
	if (!(info & 0x8000))
		return;

	const float frac = ((info >> 8) & 0x7f) / 127.f;
	const int32_t level = info & 0x7f;
	const bool elite = (info & 0x80) != 0;
	const vec3_t pos = {(float)ps->stats[STAT_LOOT_BAR_X], (float)ps->stats[STAT_LOOT_BAR_Y], (float)ps->stats[STAT_LOOT_BAR_Z]};

	float sx, sy;
	if (!CG_Loot_Project(ps, pos, hud_vrect, scale, sx, sy))
		return;

	const float bar_w = 48.f * scale;
	const float bar_h = 4.f * scale;
	const float x = sx - bar_w * 0.5f;
	const float y = sy - bar_h;

	const rgba_t fill = frac > 0.5f ? rgba_t{60, 220, 60, 255} : frac > 0.25f ? rgba_t{230, 200, 40, 255}
	                                                                          : rgba_t{220, 40, 40, 255};

	cgi.SCR_DrawColorPic((int)(x - scale), (int)(y - scale), (int)(bar_w + scale * 2), (int)(bar_h + scale * 2), "_white", rgba_black);
	cgi.SCR_DrawColorPic((int)x, (int)y, (int)(bar_w * frac), (int)bar_h, "_white", fill);
	cgi.SCR_DrawColorPic((int)(x + bar_w * frac), (int)y, (int)(bar_w * (1.f - frac)), (int)bar_h, "_white", {60, 60, 60, 255});

	// shield strip above the health bar
	const int32_t shield = ps->stats[STAT_LOOT_BAR_SHIELD];
	float label_y = y;
	if (shield > 0)
	{
		const float sfrac = (shield - 1) / 254.f;
		const float sy2 = y - bar_h - scale;
		cgi.SCR_DrawColorPic((int)(x - scale), (int)(sy2 - scale), (int)(bar_w + scale * 2), (int)(bar_h + scale), "_white", rgba_black);
		cgi.SCR_DrawColorPic((int)x, (int)sy2, (int)(bar_w * sfrac), (int)(bar_h * 0.75f), "_white", {70, 160, 255, 255});
		label_y = sy2;
	}

	const char *label = elite ? G_Fmt("Lv {} Alpha", level).data() : G_Fmt("Lv {}", level).data();
	cgi.SCR_DrawFontString(label, (int)sx, (int)(label_y - cgi.SCR_FontLineHeight(scale) - scale), scale, elite ? rgba_t{255, 150, 30, 255} : rgba_white, true, text_align_t::CENTER);
}

// Shield bar above the health number (statusbar health is at xv 0, yb -24).
void CG_Loot_DrawShield(const player_state_t *ps, const vrect_t &hud_vrect, const vrect_t &hud_safe, int32_t scale)
{
	if (!CG_Loot_Enabled())
		return;

	const int32_t max = ps->stats[STAT_LOOT_SHIELD_MAX];
	if (max <= 0)
		return;

	const int32_t cur = std::clamp((int32_t)ps->stats[STAT_LOOT_SHIELD], 0, max);
	const float frac = (float)cur / max;

	const float x = (hud_vrect.x + hud_vrect.width / 2 + (0 - 160)) * scale;
	const float y = ((hud_vrect.y + hud_vrect.height - 24 - 7) * scale) - hud_safe.y;
	const float w = 74.f * scale;
	const float h = 4.f * scale;

	cgi.SCR_DrawColorPic((int)(x - scale), (int)(y - scale), (int)(w + scale * 2), (int)(h + scale * 2), "_white", rgba_black);
	cgi.SCR_DrawColorPic((int)x, (int)y, (int)(w * frac), (int)h, "_white", {70, 160, 255, 255});
	cgi.SCR_DrawColorPic((int)(x + w * frac), (int)y, (int)(w * (1.f - frac)), (int)h, "_white", {40, 40, 60, 255});

	cgi.SCR_DrawFontString(G_Fmt("{}", cur).data(), (int)(x + w + 4 * scale), (int)(y - 3 * scale), scale, {140, 200, 255, 255}, true, text_align_t::LEFT);
}

// Floating damage numbers. The server packs a ring of 3 numbers into
// STAT_LOOT_DMG_0..2 (value, sequence, kind) plus the anchor position of the
// latest one; a changed slot spawns a number that rises and fades.
struct cg_dmg_number_t
{
	bool active;
	uint64_t start;
	int32_t value;
	int32_t kind;
	vec3_t pos;
};

static cvar_t *cl_loot_damagenums;
static cg_dmg_number_t cg_dmg_numbers[16];
static uint16_t cg_dmg_last[3];

void CG_Loot_DrawDamageNumbers(const player_state_t *ps, const vrect_t &hud_vrect, int32_t scale)
{
	if (!cl_loot_damagenums)
		cl_loot_damagenums = cgi.cvar("cl_loot_damagenums", "1", CVAR_ARCHIVE);

	if (!CG_Loot_Enabled())
		return;

	const uint64_t now = cgi.CL_ClientTime();

	// new numbers
	for (int32_t i = 0; i < 3; i++)
	{
		const uint16_t v = (uint16_t)ps->stats[STAT_LOOT_DMG_0 + i];
		if (v == cg_dmg_last[i])
			continue;
		cg_dmg_last[i] = v;
		if (!v || !cl_loot_damagenums->integer)
			continue;

		cg_dmg_number_t *slot = &cg_dmg_numbers[0];
		for (auto &n : cg_dmg_numbers)
		{
			if (!n.active)
			{
				slot = &n;
				break;
			}
			if (n.start < slot->start)
				slot = &n; // reuse the oldest
		}

		slot->active = true;
		slot->start = now;
		slot->value = v & 0x7ff;
		slot->kind = (v >> 14) & 3;
		// spread numbers sideways a little, keyed off the sequence (no RNG in cgame)
		const float jx = (float)((((v >> 11) & 7) * 37 + i * 11) % 21 - 10);
		const float jy = (float)((((v >> 11) & 7) * 53 + i * 7) % 21 - 10);
		slot->pos = vec3_t{(float)ps->stats[STAT_LOOT_DMG_X] + jx, (float)ps->stats[STAT_LOOT_DMG_Y] + jy, (float)ps->stats[STAT_LOOT_DMG_Z]};
	}

	if (!cl_loot_damagenums->integer)
		return;

	constexpr uint64_t life = 900;
	for (auto &n : cg_dmg_numbers)
	{
		if (!n.active)
			continue;
		if (now < n.start || now - n.start > life)
		{
			n.active = false;
			continue;
		}

		const float t = (float)(now - n.start) / life;
		const vec3_t world = n.pos + vec3_t{0, 0, 32.f * t};

		float sx, sy;
		if (!CG_Loot_Project(ps, world, hud_vrect, scale, sx, sy))
			continue;

		static const rgba_t colors[4] = {
		    {255, 255, 255, 255}, // hit
		    {255, 220, 40, 255},  // crit
		    {255, 140, 30, 255},  // element
		    {110, 180, 255, 255}, // shield
		};
		rgba_t c = colors[n.kind];
		c.a = (uint8_t)(255.f * std::clamp(1.f - (t - 0.55f) / 0.45f, 0.f, 1.f));

		const int32_t text_scale = n.kind == 1 ? scale + 1 : scale;
		cgi.SCR_DrawFontString(G_Fmt("{}", n.value).data(), (int)sx, (int)sy, text_scale, c, true, text_align_t::CENTER);
	}
}

// Layout token "loot_str": text with inline color switches.
// ^g = green (better than equipped), ^r = red (worse), ^o = Omega bright red
// (dangerous monster), ^w = back to white.
// Tab stops for loot_str, in virtual pixels from the string's start; set by the
// layout token loot_tabs "a b c" and used by '|' in loot_str (column alignment,
// since the kex font is proportional and space padding never lines up).
static constexpr int max_tab_stops = 8;
static int tab_stops[max_tab_stops];
static int tab_stop_count;

void CG_Loot_SetTabs(const char *list)
{
	tab_stop_count = 0;
	for (const char *p = list; *p && tab_stop_count < max_tab_stops;)
	{
		char *end;
		const long stop = strtol(p, &end, 10);
		if (end == p)
			break;
		tab_stops[tab_stop_count++] = (int)stop;
		p = end;
	}
}

void CG_Loot_DrawColorString(int x, int y, int scale, const char *text, bool kfont, int font_y_offset)
{
	static constexpr rgba_t menu_green{112, 255, 52, 255}; // same as layout string2 (alt_color)
	const int start_x = x;
	int next_tab = 0;
	static constexpr rgba_t better_green{80, 255, 80, 255};
	static constexpr rgba_t worse_red{255, 80, 80, 255};
	static constexpr rgba_t omega_red{255, 0, 0, 255};

	rgba_t color = rgba_white;
	std::string segment;

	auto flush = [&]()
	{
		if (segment.empty())
			return;
		if (kfont)
		{
			cgi.SCR_DrawFontString(segment.c_str(), x, y - (font_y_offset * scale), scale, color, true, text_align_t::LEFT);
			x += (int)cgi.SCR_MeasureFontString(segment.c_str(), scale).x;
		}
		else
		{
			// classic charset only has white and green: green for better and Omega, plain for worse
			const bool alt = (color.g == better_green.g && color.r == better_green.r) || (color.g == omega_red.g && color.r == omega_red.r) ||
			                 (color.g == menu_green.g && color.r == menu_green.r);
			for (char c : segment)
			{
				cgi.SCR_DrawChar(x, y, scale, c ^ (alt ? 0x80 : 0), true);
				x += 8 * scale;
			}
		}
		segment.clear();
	};

	for (const char *c = text; *c; c++)
	{
		// '|' jumps to the next tab stop
		if (c[0] == '|' && next_tab < tab_stop_count)
		{
			flush();
			x = std::max(x + 4 * scale, start_x + tab_stops[next_tab++] * scale);
			continue;
		}
		if (c[0] == '^' && (c[1] == 'g' || c[1] == 'r' || c[1] == 'o' || c[1] == 'w' || c[1] == 'a'))
		{
			flush();
			color = c[1] == 'g' ? better_green : c[1] == 'r' ? worse_red
			                                 : c[1] == 'o'   ? omega_red
			                                 : c[1] == 'a'   ? menu_green
			                                                 : rgba_white;
			c++;
			continue;
		}
		segment += *c;
	}
	flush();
}
