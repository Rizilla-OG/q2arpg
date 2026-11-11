// q2rpg: loot visual effects. Rarity beams over floor loot, colored tracers and
// projectile glows for elemental guns, and a colored light at the gun when an
// element procs. Beams use palette colors: the engine picks one of the four
// skinnum bytes at random each frame, so mixed bytes shimmer.

#include "../g_local.h"

//==========================================================================
// Colors
//==========================================================================

// four palette indices per beam; the palette has no purple or cyan, so Epic
// mixes lavender/red/blue and Mythic mixes white/pale cyan/blue/green
static const uint32_t rarity_beam_colors[LOOT_RARITY_COUNT] = {
    0,          // Common: no beam
    0xd0d1d2d3, // Uncommon: green
    0xf1f3f1f3, // Rare: blue
    0xb0f2b1f1, // Epic: purple-ish
    0xdcdddedf, // Legendary: gold
    0xd76ff1d1, // Mythic: shimmer
};

static const uint32_t element_tracer_colors[LOOT_ELEM_COUNT] = {
    0,          // none
    0xe0e1e2e3, // fire: orange
    0xf1f3b0f1, // shock: blue
    0xd0d1d2d3, // corrosive: green
};

static const rgba_t element_light_colors[LOOT_ELEM_COUNT] = {
    {0, 0, 0, 0},
    {255, 120, 20, 255}, // fire
    {80, 140, 255, 255}, // shock
    {90, 255, 40, 255},  // corrosive
};

static const renderfx_t element_shells[LOOT_ELEM_COUNT] = {
    RF_NONE,
    RF_SHELL_RED,   // fire
    RF_SHELL_BLUE,  // shock
    RF_SHELL_GREEN, // corrosive
};

static edict_t *LootFx_SpawnBeam(uint32_t color, int32_t width)
{
	edict_t *beam = G_Spawn();
	beam->movetype = MOVETYPE_NONE;
	beam->solid = SOLID_NOT;
	beam->s.renderfx = RF_BEAM;
	beam->s.modelindex = MODELINDEX_WORLD; // beams need a non-zero model
	beam->s.frame = width;
	beam->s.skinnum = (int32_t)color;
	return beam;
}

//==========================================================================
// Rarity beams
//==========================================================================

static void LootFx_PlaceRarityBeam(edict_t *beam, const edict_t *item)
{
	const float height = 48.f + 16.f * (float)beam->count; // rarer = taller
	beam->s.origin = item->s.origin;
	beam->s.old_origin = item->s.origin + vec3_t{0, 0, height};
	beam->s.instance_bits = item->s.instance_bits; // coop per-player drops
	// q2rpg: hidden while its item is (trigger-spawned map weapons)
	if (item->svflags & SVF_NOCLIENT)
		beam->svflags |= SVF_NOCLIENT;
	else
		beam->svflags &= ~SVF_NOCLIENT;
	gi.linkentity(beam);
}

THINK(lootfx_rarity_beam_think)(edict_t *self)->void
{
	edict_t *item = self->owner;

	// item picked up, freed, or its slot reused by another entity
	if (!item || !item->inuse || item->spawn_count != self->style || !Loot_IsLootItem(item))
	{
		G_FreeEdict(self);
		return;
	}

	LootFx_PlaceRarityBeam(self, item); // follow tossed drops while they fall
	self->nextthink = level.time + FRAME_TIME_MS;
}

void LootFx_RarityBeam(edict_t *item, loot_rarity_t rarity)
{
	// one beam per item; Loot_ApplyVisuals can run more than once
	for (edict_t *e = nullptr; (e = G_FindByString<&edict_t::classname>(e, "loot_beam")) != nullptr;)
		if (e->owner == item)
			G_FreeEdict(e);

	if (rarity >= LOOT_RARITY_COUNT || !rarity_beam_colors[rarity])
		return;

	edict_t *beam = LootFx_SpawnBeam(rarity_beam_colors[rarity], 4);
	beam->classname = "loot_beam";
	beam->owner = item;
	beam->style = item->spawn_count;
	beam->count = rarity;
	beam->think = lootfx_rarity_beam_think;
	beam->nextthink = level.time + FRAME_TIME_MS;
	LootFx_PlaceRarityBeam(beam, item);
}

//==========================================================================
// Element shots
//==========================================================================

static loot_element_t LootFx_ShotElement(edict_t *shooter)
{
	if (!shooter || !shooter->client)
		return LOOT_ELEM_NONE;
	const loot_weapon_t *gun = Loot_Active(shooter);
	return gun ? gun->element : LOOT_ELEM_NONE;
}

THINK(lootfx_free_think)(edict_t *self)->void
{
	G_FreeEdict(self);
}

void LootFx_Tracer(edict_t *shooter, const vec3_t &start, const vec3_t &end)
{
	const loot_element_t element = LootFx_ShotElement(shooter);
	if (!element)
		return;

	edict_t *tracer = LootFx_SpawnBeam(element_tracer_colors[element], 2);
	tracer->classname = "loot_tracer";
	tracer->s.origin = start;
	tracer->s.old_origin = end;
	tracer->think = lootfx_free_think;
	tracer->nextthink = level.time + 100_ms;
	gi.linkentity(tracer);
}

void LootFx_TintProjectile(edict_t *shooter, edict_t *projectile)
{
	const loot_element_t element = LootFx_ShotElement(shooter);
	if (!element || !projectile->inuse)
		return;

	projectile->s.effects |= EF_COLOR_SHELL;
	projectile->s.renderfx |= element_shells[element];
}

//==========================================================================
// Proc light
//==========================================================================

static constexpr gtime_t proc_light_time = 300_ms;
static constexpr int32_t proc_light_radius = 160;

static vec3_t LootFx_GunPoint(const edict_t *player)
{
	vec3_t forward, right;
	AngleVectors(player->client->v_angle, forward, right, nullptr);
	return player->s.origin + vec3_t{0, 0, (float)player->viewheight - 8.f} + forward * 24.f + right * 6.f;
}

THINK(lootfx_proc_light_think)(edict_t *self)->void
{
	edict_t *player = self->owner;
	if (!player || !player->inuse || !player->client || level.time >= self->timestamp)
	{
		G_FreeEdict(self);
		return;
	}

	// follow the gun and fade the radius out
	const float left = (self->timestamp - level.time).seconds() / proc_light_time.seconds();
	self->s.origin = LootFx_GunPoint(player);
	self->s.frame = (int32_t)(proc_light_radius * left);
	gi.linkentity(self);
	self->nextthink = level.time + FRAME_TIME_MS;
}

void LootFx_ProcLight(edict_t *attacker, loot_element_t element)
{
	if (!attacker || !attacker->client || !element || element >= LOOT_ELEM_COUNT)
		return;

	// reuse this player's light so pellet procs don't stack
	edict_t *light = nullptr;
	for (edict_t *e = nullptr; (e = G_FindByString<&edict_t::classname>(e, "loot_proc_light")) != nullptr;)
		if (e->owner == attacker)
		{
			light = e;
			break;
		}

	if (!light)
	{
		light = G_Spawn();
		light->classname = "loot_proc_light";
		light->owner = attacker;
		light->movetype = MOVETYPE_NONE;
		light->solid = SOLID_NOT;
		light->s.modelindex = 1; // must be non-zero to be sent
		light->s.renderfx = RF_CUSTOM_LIGHT;
		light->think = lootfx_proc_light_think;
	}

	// RF_CUSTOM_LIGHT: s.frame is radius, s.skinnum is 0xRRGGBB00
	const rgba_t &c = element_light_colors[element];
	light->s.skinnum = (int32_t)(((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) | ((uint32_t)c.b << 8));
	light->s.frame = proc_light_radius;
	light->s.origin = LootFx_GunPoint(attacker);
	light->timestamp = level.time + proc_light_time;
	light->nextthink = level.time + FRAME_TIME_MS;
	gi.linkentity(light);

	// the dlight alone is faint in open areas; also shell the player, which
	// the engine draws on the view weapon too (like quad damage)
	attacker->client->loot_proc_glow_end = level.time + proc_light_time;
	attacker->client->loot_proc_glow_element = element;
}

void LootFx_ClientEffects(edict_t *player)
{
	gclient_t *cl = player->client;
	if (cl->loot_proc_glow_end <= level.time || !cl->loot_proc_glow_element || cl->loot_proc_glow_element >= LOOT_ELEM_COUNT)
		return;

	player->s.effects |= EF_COLOR_SHELL;
	player->s.renderfx |= element_shells[cl->loot_proc_glow_element];
}
