// q2rpg: character looks. Page 3 of the challenges screen (J) picks a stock player skin and
// an aura (a pulsing color shell or a trail). Both are free; better ones unlock with player
// level, challenge tiers, NG+ cycles or a raid clear. Choices are saved with the character.
// Skin 0 / aura 0 = your own userinfo skin, no aura.

#include "../g_local.h"

enum looks_unlock_t : uint8_t
{
	LOOKS_FREE,
	LOOKS_LEVEL,  // player level >= value
	LOOKS_TIERS,  // challenge tiers completed (all challenges added up) >= value
	LOOKS_NGPLUS, // NG+ cycles completed >= value
	LOOKS_RAID,   // raid boss beaten at least once
};

struct looks_skin_t
{
	const char *path; // model/skin
	looks_unlock_t unlock;
	int32_t value;
};

// index 0 is "your own skin" and is not in this table
static const looks_skin_t looks_skins[] = {
    {"male/grunt", LOOKS_FREE, 0},
    {"male/major", LOOKS_FREE, 0},
    {"male/scout", LOOKS_FREE, 0},
    {"male/recon", LOOKS_FREE, 0},
    {"male/sniper", LOOKS_FREE, 0},
    {"female/athena", LOOKS_FREE, 0},
    {"female/brianna", LOOKS_FREE, 0},
    {"female/cobalt", LOOKS_FREE, 0},
    {"female/ensign", LOOKS_FREE, 0},
    {"male/pointman", LOOKS_LEVEL, 10},
    {"female/jezebel", LOOKS_LEVEL, 10},
    {"male/cipher", LOOKS_LEVEL, 15},
    {"female/jungle", LOOKS_LEVEL, 15},
    {"male/claymore", LOOKS_LEVEL, 20},
    {"female/lotus", LOOKS_LEVEL, 20},
    {"cyborg/oni911", LOOKS_LEVEL, 20},
    {"male/flak", LOOKS_LEVEL, 25},
    {"female/stiletto", LOOKS_LEVEL, 25},
    {"male/howitzer", LOOKS_LEVEL, 30},
    {"female/venus", LOOKS_LEVEL, 30},
    {"male/nightops", LOOKS_LEVEL, 35},
    {"male/viper", LOOKS_LEVEL, 40},
    {"male/psycho", LOOKS_TIERS, 10},
    {"female/voodoo", LOOKS_TIERS, 15},
    {"male/rampage", LOOKS_TIERS, 20},
    {"cyborg/ps9000", LOOKS_TIERS, 25},
    {"male/razor", LOOKS_TIERS, 30},
    {"male/deaddude", LOOKS_NGPLUS, 1},
    {"cyborg/tyr574", LOOKS_RAID, 0},
};
constexpr int32_t LOOKS_SKIN_COUNT = (int32_t)q_countof(looks_skins) + 1; // + own skin

struct looks_aura_t
{
	const char *name;
	renderfx_t shell; // pulsing color shell (0 = none)
	effects_t trail;  // steady trail effect (0 = none)
	looks_unlock_t unlock;
	int32_t value;
};

// index 0 is "none"
static const looks_aura_t looks_auras[] = {
    {"Ember", RF_SHELL_RED, EF_NONE, LOOKS_TIERS, 5},
    {"Frost", RF_SHELL_BLUE | RF_SHELL_GREEN, EF_NONE, LOOKS_TIERS, 15},
    {"Venom", RF_SHELL_GREEN, EF_NONE, LOOKS_TIERS, 25},
    {"Storm", RF_SHELL_BLUE, EF_NONE, LOOKS_LEVEL, 30},
    {"Gilded", RF_SHELL_RED | RF_SHELL_GREEN, EF_NONE, LOOKS_NGPLUS, 1},
    {"Comet", RF_NONE, EF_TAGTRAIL, LOOKS_LEVEL, 50},
    {"Void", RF_NONE, EF_TRACKERTRAIL, LOOKS_NGPLUS, 2},
    {"Kingslayer", RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE, EF_NONE, LOOKS_RAID, 0},
};
constexpr int32_t LOOKS_AURA_COUNT = (int32_t)q_countof(looks_auras) + 1; // + none

static int32_t Looks_TiersDone(const client_persistant_t &pers)
{
	int32_t total = 0;
	for (int32_t tier : pers.loot_challenge_tier)
		total += tier;
	return total;
}

static bool Looks_Unlocked(const edict_t *ent, looks_unlock_t unlock, int32_t value)
{
	const client_persistant_t &pers = ent->client->pers;
	switch (unlock)
	{
		case LOOKS_LEVEL:
			return pers.loot_level >= value;
		case LOOKS_TIERS:
			return Looks_TiersDone(pers) >= value;
		case LOOKS_NGPLUS:
			return game.loot_ngplus >= value;
		case LOOKS_RAID:
			return game.loot_raid_cleared_cycle > 0;
		default:
			return true;
	}
}

static std::string Looks_UnlockText(looks_unlock_t unlock, int32_t value)
{
	switch (unlock)
	{
		case LOOKS_LEVEL:
			return G_Fmt("reach Lv {}", value).data();
		case LOOKS_TIERS:
			return G_Fmt("{} challenge tiers", value).data();
		case LOOKS_NGPLUS:
			return value > 1 ? G_Fmt("finish NG+ {}", value - 1).data() : "start NG+";
		case LOOKS_RAID:
			return "beat the raid boss";
		default:
			return "";
	}
}

static bool Looks_SkinUnlocked(const edict_t *ent, int32_t index)
{
	return index <= 0 || (index < LOOKS_SKIN_COUNT && Looks_Unlocked(ent, looks_skins[index - 1].unlock, looks_skins[index - 1].value));
}

static bool Looks_AuraUnlocked(const edict_t *ent, int32_t index)
{
	return index <= 0 || (index < LOOKS_AURA_COUNT && Looks_Unlocked(ent, looks_auras[index - 1].unlock, looks_auras[index - 1].value));
}

static void Looks_RefreshSkin(edict_t *ent)
{
	ClientUserinfoChanged(ent, ent->client->pers.userinfo);
}

// ClientUserinfoChanged: swap the userinfo skin for the chosen one (SP / coop only)
void Looks_OverrideSkin(edict_t *ent, char *skin, size_t skin_size)
{
	if (deathmatch->integer || !ent->client)
		return;
	const int32_t index = ent->client->pers.loot_look_skin;
	if (index <= 0 || index >= LOOKS_SKIN_COUNT || !Looks_SkinUnlocked(ent, index))
		return;
	Q_strlcpy(skin, looks_skins[index - 1].path, skin_size);
}

// G_SetClientEffects: the aura. Shells pulse (0.8 s of every 2 s) and stay off while a
// powerup or cheat shell is already showing.
void Looks_ApplyAura(edict_t *ent)
{
	if (deathmatch->integer || !ent->client)
		return;
	// a map load or save load builds the skin configstring before pers is back; resend once
	if (!ent->client->loot_looks_synced)
	{
		ent->client->loot_looks_synced = true;
		if (ent->client->pers.loot_look_skin > 0)
			Looks_RefreshSkin(ent);
	}
	if (ent->health <= 0)
		return;
	const int32_t index = ent->client->pers.loot_look_aura;
	if (index <= 0 || index >= LOOKS_AURA_COUNT || !Looks_AuraUnlocked(ent, index))
		return;

	const looks_aura_t &aura = looks_auras[index - 1];
	ent->s.effects |= aura.trail;

	const bool other_shell = (ent->s.effects & EF_COLOR_SHELL) || (ent->s.renderfx & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE));
	const bool pulse_on = (level.time.milliseconds() % 2000) < 800;
	if (aura.shell && !other_shell && pulse_on)
	{
		ent->s.effects |= EF_COLOR_SHELL;
		ent->s.renderfx |= aura.shell;
	}
}

// steps a choice to the next unlocked entry (direction +1 / -1); 0 always counts as unlocked
static int32_t Looks_Step(edict_t *ent, int32_t current, int32_t count, int32_t direction, bool (*unlocked)(const edict_t *, int32_t))
{
	int32_t index = current;
	for (int32_t tries = 0; tries < count; tries++)
	{
		index = (index + direction + count) % count;
		if (unlocked(ent, index))
			return index;
	}
	return current;
}

static int32_t Looks_CountUnlocked(const edict_t *ent, int32_t count, bool (*unlocked)(const edict_t *, int32_t))
{
	int32_t total = 0;
	for (int32_t i = 1; i < count; i++)
		total += unlocked(ent, i) ? 1 : 0;
	return total;
}

// key 6: a stand-in of you appears in front of you for 10 s, slowly turning, wearing the
// current skin and aura (the rerelease has no third-person camera)
constexpr gtime_t LOOKS_PREVIEW_TIME = 10_sec;

THINK(Looks_PreviewThink)(edict_t *self)->void
{
	edict_t *owner = self->owner;
	if (!owner || !owner->inuse || !owner->client || level.time >= self->timestamp)
	{
		if (owner && owner->client && owner->client->loot_looks_preview == self)
			owner->client->loot_looks_preview = nullptr;
		G_FreeEdict(self);
		return;
	}
	self->s.skinnum = owner->s.skinnum & 0xFF; // client number picks the skin
	self->s.frame = (self->s.frame + 1) % 40;  // stand01-40
	self->s.angles[YAW] = anglemod(self->s.angles[YAW] + 6.f);

	// the owner's aura bits only
	constexpr renderfx_t shell_bits = RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE;
	self->s.renderfx = (self->s.renderfx & ~shell_bits) | (owner->s.renderfx & shell_bits);
	self->s.effects = owner->s.effects & (EF_COLOR_SHELL | EF_TAGTRAIL | EF_TRACKERTRAIL);
	self->nextthink = level.time + 100_ms;
}

static void Looks_Preview(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (cl->loot_looks_preview && cl->loot_looks_preview->inuse)
		G_FreeEdict(cl->loot_looks_preview);
	cl->loot_looks_preview = nullptr;

	vec3_t forward;
	AngleVectors({0, ent->client->v_angle[YAW], 0}, forward, nullptr, nullptr);
	const vec3_t spot = ent->s.origin + forward * 96.f;
	const trace_t room = gi.trace(ent->s.origin, ent->mins, ent->maxs, spot, ent, MASK_PLAYERSOLID);
	if (room.fraction < 1.f)
	{
		gi.Center_Print(ent, "No room for the preview here");
		return;
	}

	edict_t *preview = G_Spawn();
	preview->classname = "loot_looks_preview";
	preview->owner = ent;
	preview->s.origin = spot;
	preview->s.old_origin = spot;
	preview->s.angles = {0, anglemod(ent->client->v_angle[YAW] + 180.f), 0};
	preview->s.modelindex = MODELINDEX_PLAYER;
	preview->s.skinnum = ent->s.skinnum & 0xFF;
	preview->movetype = MOVETYPE_NONE;
	preview->solid = SOLID_NOT;
	preview->timestamp = level.time + LOOKS_PREVIEW_TIME;
	preview->think = Looks_PreviewThink;
	preview->nextthink = level.time + FRAME_TIME_MS;
	gi.linkentity(preview);
	cl->loot_looks_preview = preview;
}

// keys on the looks page: 1/2 skin back/next, 3/4 aura back/next, 5 back to your own skin, no aura, 6 preview
void Looks_Select(edict_t *ent, int32_t key)
{
	client_persistant_t &pers = ent->client->pers;
	switch (key)
	{
		case 1:
		case 2:
			pers.loot_look_skin = Looks_Step(ent, pers.loot_look_skin, LOOKS_SKIN_COUNT, key == 1 ? -1 : 1, Looks_SkinUnlocked);
			Looks_RefreshSkin(ent);
			break;
		case 3:
		case 4:
			pers.loot_look_aura = Looks_Step(ent, pers.loot_look_aura, LOOKS_AURA_COUNT, key == 3 ? -1 : 1, Looks_AuraUnlocked);
			break;
		case 5:
			pers.loot_look_skin = 0;
			pers.loot_look_aura = 0;
			Looks_RefreshSkin(ent);
			break;
		case 6:
			Looks_Preview(ent);
			break;
		default:
			return;
	}
	gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/menu2.wav"), 1, ATTN_NONE, 0);
	ent->client->loot_challenges_time = 0_ms;
}

// the next locked skin or aura, so the page can say what to chase
template <typename T>
static const T *Looks_NextLocked(const edict_t *ent, const T *table, int32_t table_size)
{
	for (int32_t i = 0; i < table_size; i++)
		if (!Looks_Unlocked(ent, table[i].unlock, table[i].value))
			return &table[i];
	return nullptr;
}

std::string Looks_PageLayout(edict_t *ent)
{
	const client_persistant_t &pers = ent->client->pers;
	std::string s = "loot_dim xl 16 loot_tabs \"140\" ";
	int32_t y = 24;
	auto line = [&](const char *kind, const std::string &text)
	{
		s += G_Fmt("yv {} {} \"{}\" ", y, kind, text).data();
		y += 10;
	};

	line("string2", "LOOKS  3/3");
	y += 6;

	const int32_t skin = std::clamp(pers.loot_look_skin, 0, LOOKS_SKIN_COUNT - 1);
	const int32_t aura = std::clamp(pers.loot_look_aura, 0, LOOKS_AURA_COUNT - 1);
	line("loot_str", G_Fmt("Skin|{}", skin ? looks_skins[skin - 1].path : "your own (menu setting)").data());
	line("loot_str", G_Fmt("Aura|{}", aura ? looks_auras[aura - 1].name : "none").data());
	line("loot_str", G_Fmt("Unlocked|{}/{} skins, {}/{} auras", Looks_CountUnlocked(ent, LOOKS_SKIN_COUNT, Looks_SkinUnlocked), LOOKS_SKIN_COUNT - 1,
	                       Looks_CountUnlocked(ent, LOOKS_AURA_COUNT, Looks_AuraUnlocked), LOOKS_AURA_COUNT - 1)
	                     .data());
	line("loot_str", G_Fmt("Tiers done|{}", Looks_TiersDone(pers)).data());
	y += 6;

	if (const looks_skin_t *next = Looks_NextLocked(ent, looks_skins, (int32_t)q_countof(looks_skins)))
		line("loot_str", G_Fmt("Next skin|{}: {}", next->path, Looks_UnlockText(next->unlock, next->value)).data());
	if (const looks_aura_t *next = Looks_NextLocked(ent, looks_auras, (int32_t)q_countof(looks_auras)))
		line("loot_str", G_Fmt("Next aura|{}: {}", next->name, Looks_UnlockText(next->unlock, next->value)).data());
	y += 6;

	line("string2", "[1/2] skin  [3/4] aura  [5] reset  [6] preview  [J] close");
	line("string", "Others see your skin and aura in coop");
	return s;
}
