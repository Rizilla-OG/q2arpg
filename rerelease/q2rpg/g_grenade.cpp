// q2rpg: grenade mods. A third gear piece next to guns and shields: rolled like a shield
// (rarity, maker, level), one equipped, thrown with the "grenade" command (Q) without
// switching weapons. Each throw costs one stock hand grenade (IT_AMMO_GRENADES).
// With no mod equipped, Q throws a plain frag at your level.

#include "../g_local.h"

//==========================================================================
// Tables
//==========================================================================

struct grenade_type_info_t
{
	const char *name;
	const char *text; // card line
	int32_t weight;   // roll weight (uncommon+; commons are Standard or Sticky)
	float damage;     // damage multiplier for the type
};

static const grenade_type_info_t grenade_types[LOOT_GRENADE_TYPE_COUNT] = {
    {"Frag", "Explodes on fuse or on contact", 35, 1.00f},
    {"Cluster", "Bursts into 4 bomblets", 20, 0.60f}, // main blast; bomblets add ~4 x 0.35
    {"Sticky", "Sticks to the first enemy it hits", 20, 1.10f},
    {"Vortex", "Pulls enemies in for 1s, then blows", 12, 0.90f},
    {"Leech", "Heals you for 25% of the damage", 13, 0.85f},
    {"Hopper", "Blasts on each of 3 bounces", 12, 0.55f},       // per hop
    {"Airburst", "Bursts overhead, rains 6 shards", 10, 0.60f}, // main blast; shards add ~6 x 0.30
};

// Per maker (loot_maker_t order): damage, radius, fuse, throw speed, type lean
struct grenade_maker_info_t
{
	float damage, radius, fuse, speed;
	loot_grenade_type_t lean; // 40% of uncommon+ rolls (Standard = no lean)
};

static const grenade_maker_info_t grenade_makers[LOOT_MAKER_COUNT] = {
    {1.15f, 0.95f, 1.00f, 1.10f, LOOT_GRENADE_STICKY},   // Hardline: hits hard, throws far
    {0.95f, 1.00f, 1.00f, 1.00f, LOOT_GRENADE_HOPPER},   // Vexa: shock lean, hopper lean
    {1.05f, 1.25f, 1.10f, 0.90f, LOOT_GRENADE_CLUSTER},  // Brask: big blasts, fire lean
    {1.10f, 0.90f, 0.65f, 1.00f, LOOT_GRENADE_AIRBURST}, // Kessler: short fuse, airburst lean
    {0.95f, 1.10f, 1.00f, 1.00f, LOOT_GRENADE_VORTEX},   // Orbitek: vortex lean
    {1.00f, 1.00f, 1.00f, 1.00f, LOOT_GRENADE_LEECH},    // Salvage: wild rolls, corrosive lean
};

static const char *const grenade_prefixes[3][3] = {
    {"Heavy", "Brutal", "Mean"},   // damage
    {"Wide", "Sweeping", "Broad"}, // radius
    {"Quick", "Hasty", "Snappy"},  // fuse
};

static const char *const grenade_legendary_names[] = {"Hellrain", "Doorknocker", "Last Word", "Pocket Sun", "Housewarmer", "Strogg Popper"};
static const char *const grenade_mythic_names[] = {"Godsplitter", "Big Crunch", "Worldender", "Nova Heart"};

constexpr float GRENADE_BASE_DAMAGE = 120.f;
constexpr float GRENADE_BASE_RADIUS = 160.f;
constexpr float GRENADE_BASE_FUSE = 2.0f;
constexpr float GRENADE_BASE_SPEED = 650.f;
constexpr gtime_t GRENADE_COOLDOWN = 800_ms;
constexpr float LEECH_FRACTION = 0.25f;
constexpr gtime_t VORTEX_PULL_TIME = 1_sec;
constexpr float VORTEX_PULL_SPEED = 320.f;
constexpr int32_t HOPPER_HOPS = 3;
constexpr float HOPPER_HOP_SPEED = 300.f;
constexpr int32_t AIRBURST_SHARDS = 6;
constexpr float AIRBURST_FUSE_MULT = 0.6f; // goes off while still in the air
constexpr float AIRBURST_ARC = 350.f;      // extra upward throw speed

// Legendary and Mythic twists, one per type (Grenade_Twist)
constexpr gtime_t FRAG_ECHO_DELAY = 500_ms; // Frag: second blast
constexpr float FRAG_ECHO_DAMAGE = 0.5f;
constexpr int32_t CLUSTER_BOMBLETS = 4;
constexpr int32_t CLUSTER_TWIST_BOMBLETS = 8;
constexpr gtime_t VORTEX_TWIST_PULL_TIME = 2_sec;
constexpr float VORTEX_TWIST_RADIUS = 2.0f;  // pull radius, x blast radius (normally 1.5)
constexpr float LEECH_TWIST_FRACTION = 0.5f; // overflow heal goes to the shield
constexpr int32_t HOPPER_TWIST_HOPS = 5;
constexpr int32_t AIRBURST_TWIST_SHARDS = 12;

static const char *const grenade_twist_text[LOOT_GRENADE_TYPE_COUNT] = {
    "Echo: blasts again 0.5s later (50%)",
    "8 bomblets",
    "Blast staggers everything it hits",
    "Pulls for 2s, wider",
    "Heals 50%, overflow charges shield",
    "5 bounces",
    "12 shards",
};

// Legendary and Mythic grenade mods get their type's twist.
static bool Grenade_Twist(const loot_grenade_t &grenade)
{
	return grenade.rarity >= LOOT_RARITY_LEGENDARY;
}

const char *Grenade_TypeName(loot_grenade_type_t type)
{
	return grenade_types[type < LOOT_GRENADE_TYPE_COUNT ? type : 0].name;
}

//==========================================================================
// Rolling
//==========================================================================

int32_t Grenade_TypeByName(const char *name)
{
	if (!name || !*name)
		return -1;
	for (int32_t i = 0; i < LOOT_GRENADE_TYPE_COUNT; i++)
		if (!Q_strcasecmp(name, grenade_types[i].name))
			return i;
	gi.Com_PrintFmt("loot_grenade: unknown \"{}\", rolling a random grenade mod\n", name);
	return -1;
}

loot_grenade_t Grenade_Roll(loot_rarity_t rarity, int32_t level, uint32_t seed, int32_t forced_type)
{
	std::mt19937 rng(seed);
	auto rf = [&](float lo, float hi)
	{ return std::uniform_real_distribution<float>(lo, hi)(rng); };

	loot_grenade_t grenade{};
	grenade.valid = true;
	grenade.seed = seed;
	grenade.level = std::clamp(level, 1, Loot_LevelCap() + 5);
	grenade.rarity = rarity;
	grenade.maker = (loot_maker_t)(rng() % LOOT_MAKER_COUNT);
	const grenade_maker_info_t &mk = grenade_makers[grenade.maker];
	const float noise = grenade.maker == LOOT_MAKER_SALVAGE ? 0.30f : 0.10f;
	const float r = (float)rarity;

	// type
	if (rarity == LOOT_RARITY_COMMON)
		grenade.type = rf(0.f, 1.f) < 0.7f ? LOOT_GRENADE_STANDARD : LOOT_GRENADE_STICKY;
	else if (mk.lean != LOOT_GRENADE_STANDARD && rf(0.f, 1.f) < 0.4f)
		grenade.type = mk.lean;
	else
	{
		int32_t total = 0;
		for (const auto &type : grenade_types)
			total += type.weight;
		int32_t pick = (int32_t)(rng() % total);
		int32_t t = 0;
		while (pick >= grenade_types[t].weight)
			pick -= grenade_types[t++].weight;
		grenade.type = (loot_grenade_type_t)t;
	}
	if (forced_type >= 0 && forced_type < LOOT_GRENADE_TYPE_COUNT)
		grenade.type = (loot_grenade_type_t)forced_type; // q2rpg: mapper or dev asked for this type, any rarity

	// stats: level scales damage like guns (+6% per level), rarity like everything else
	const float damage_noise = rf(1 - noise, 1 + noise);
	const float radius_noise = rf(1 - noise, 1 + noise);
	const float fuse_noise = rf(1 - noise, 1 + noise);
	grenade.damage = GRENADE_BASE_DAMAGE * (1.0f + 0.06f * (grenade.level - 1)) * Loot_RarityDamage(rarity) * mk.damage * damage_noise *
	                 grenade_types[grenade.type].damage;
	grenade.radius = GRENADE_BASE_RADIUS * mk.radius * radius_noise * (1 + 0.04f * r);
	grenade.fuse = std::max(0.8f, GRENADE_BASE_FUSE * mk.fuse * fuse_noise);
	if (grenade.type == LOOT_GRENADE_AIRBURST)
		grenade.fuse = std::max(0.5f, grenade.fuse * AIRBURST_FUSE_MULT);
	grenade.speed = GRENADE_BASE_SPEED * mk.speed;

	// element: same odds as guns; Brask fire, Vexa shock, Salvage corrosive, Kessler cryo, Hardline explosive lean
	static const float elem_odds[LOOT_RARITY_COUNT] = {0.10f, 0.25f, 0.40f, 0.60f, 0.80f, 1.00f};
	if (rf(0.f, 1.f) < elem_odds[rarity < LOOT_RARITY_COUNT ? rarity : 0])
	{
		grenade.element = (loot_element_t)(1 + rng() % (LOOT_ELEM_COUNT - 1));
		if (rf(0.f, 1.f) < 0.5f)
		{
			if (grenade.maker == LOOT_MAKER_BRASK)
				grenade.element = LOOT_ELEM_FIRE;
			else if (grenade.maker == LOOT_MAKER_VEXA)
				grenade.element = LOOT_ELEM_SHOCK;
			else if (grenade.maker == LOOT_MAKER_SALVAGE)
				grenade.element = LOOT_ELEM_CORROSIVE;
			else if (grenade.maker == LOOT_MAKER_KESSLER)
				grenade.element = LOOT_ELEM_CRYO;
			else if (grenade.maker == LOOT_MAKER_HARDLINE)
				grenade.element = LOOT_ELEM_EXPLOSIVE;
		}
		grenade.elem_chance = std::min(1.f, 0.5f + 0.08f * r);
	}

	// name
	const char *type_name = grenade_types[grenade.type].name;
	if (rarity >= LOOT_RARITY_MYTHIC)
		G_FmtTo(grenade.name, "{} {} Grenade", grenade_mythic_names[rng() % q_countof(grenade_mythic_names)], type_name);
	else if (rarity == LOOT_RARITY_LEGENDARY)
		G_FmtTo(grenade.name, "{} {} Grenade", grenade_legendary_names[rng() % q_countof(grenade_legendary_names)], type_name);
	else
	{
		const float scores[3] = {damage_noise * mk.damage, radius_noise * mk.radius, 1.f / (fuse_noise * mk.fuse)};
		int32_t best = 0;
		for (int32_t i = 1; i < 3; i++)
			if (scores[i] > scores[best])
				best = i;
		G_FmtTo(grenade.name, "{} {} {} Grenade", grenade_prefixes[best][rng() % 3], Loot_MakerShortName(grenade.maker), type_name);
	}

	return grenade;
}

//==========================================================================
// Cards, compare lines, giving
//==========================================================================

static std::string Grenade_ElementText(const loot_grenade_t &grenade)
{
	if (!grenade.element)
		return "";
	return G_Fmt("{} {:.0f}%", Loot_ElementName(grenade.element), grenade.elem_chance * 100.f).data();
}

void Grenade_SendCard(edict_t *ent, const loot_grenade_t &grenade)
{
	const loot_grenade_t &other = ent->client->pers.loot_grenade;
	const bool cmp = other.valid;

	std::string s;
	int32_t y = 64;
	s += G_Fmt("xv 24 yv {} string2 \"{}\" ", y, grenade.name).data();
	y += 10;
	s += G_Fmt("xv 24 yv {} string \"Lv {} {} {} Grenade Mod\" ", y, grenade.level, Loot_RarityName(grenade.rarity), Loot_MakerShortName(grenade.maker)).data();
	y += 14;

	auto add = [&](const std::string &line)
	{
		s += G_Fmt("xv 24 yv {} loot_str \"{}\" ", y, line).data();
		y += 10;
	};
	s += LOOT_CARD_TABS;
	add(Loot_StatLine("Damage", grenade.damage, other.damage, cmp, false, "num"));
	add(Loot_StatLine("Radius", grenade.radius, other.radius, cmp, false, "num"));
	add(Loot_StatLine("Fuse", grenade.fuse, other.fuse, cmp, true, "sec"));
	if (grenade.element)
		add(Grenade_ElementText(grenade));
	add(grenade_types[grenade.type].text);
	if (Grenade_Twist(grenade))
		add(G_Fmt("TWIST: {}", grenade_twist_text[grenade.type]).data());

	y += 4;
	if (cmp)
	{
		s += G_Fmt("xv 24 yv {} string \"vs {}\" ", y, other.name).data();
		y += 10;
	}
	s += G_Fmt("xv 24 yv {} string2 \"[E] take  [B] stash  [X] sell\" ", y).data();
	Loot_SendLayout(ent, s);
}

std::string Grenade_BriefCompare(const loot_grenade_t &grenade, const loot_grenade_t &other)
{
	const bool cmp = other.valid;
	auto col = [&](float value, float other_value, bool lower_better)
	{
		if (!cmp || fabsf(value - other_value) <= 0.005f)
			return "";
		return (lower_better ? value < other_value : value > other_value) ? "^g" : "^r";
	};
	std::string brief = G_Fmt("L{} {} D{}{:.0f}^w R{}{:.0f}^w F{}{:.1f}s^w", grenade.level, Grenade_TypeName(grenade.type), col(grenade.damage, other.damage, false), grenade.damage,
	                          col(grenade.radius, other.radius, false), grenade.radius, col(grenade.fuse, other.fuse, true), grenade.fuse)
	                        .data();
	if (grenade.element)
		brief += " " + Grenade_ElementText(grenade);
	return brief;
}

std::string Grenade_LoadoutLine(const loot_grenade_t &grenade)
{
	std::string line = G_Fmt("L{} {}   Damage {:.0f}   Radius {:.0f}   Fuse {:.1f}s", grenade.level, Grenade_TypeName(grenade.type), grenade.damage, grenade.radius, grenade.fuse).data();
	if (grenade.element)
		line += "   " + Grenade_ElementText(grenade);
	return line;
}

void Grenade_Give(edict_t *ent, const loot_grenade_t &grenade)
{
	gclient_t *cl = ent->client;
	if (cl->pers.loot_grenade.valid)
		Grenade_SpawnDrop(ent, cl->pers.loot_grenade, true);

	cl->pers.loot_grenade = grenade;
	gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/am_pkup.wav"), 1, ATTN_NORM, 0);
	gi.Center_Print(ent, G_Fmt("{} ({} Grenade Mod)", grenade.name, Loot_RarityName(grenade.rarity)).data());
}

//==========================================================================
// Thrown grenades
//==========================================================================

// Grenade exploding right now; Loot_ModifyDamage reads its element through
// Grenade_ApplyElement while T_RadiusDamage runs. The type is the one the player threw
// (edict style - 1), so cluster bomblets count as Cluster in loot_stats.
static const loot_grenade_t *grenade_blast;
static int32_t grenade_blast_type;

void Grenade_ApplyElement(edict_t *targ, edict_t *attacker, int32_t damage)
{
	if (!grenade_blast)
		return;
	targ->loot_hit_grenade = grenade_blast_type + 1; // kill credit by type
	Skill_OnGrenadeHit(attacker, damage);            // Juggernaut: shield from grenade damage

	loot_element_t element = grenade_blast->element;
	float chance = grenade_blast->elem_chance;
	Skill_GrenadeElement(attacker, element, chance); // Elementalist
	chance *= NGPlus_ProcChanceMult();               // Elemental Storm
	if (!element || frandom() >= chance)
		return;
	Loot_ApplyStatus(targ, attacker, element, damage);
}

static edict_t *Grenade_Attacker(edict_t *grenade)
{
	edict_t *owner = grenade->owner;
	return (owner && owner->inuse && owner->client) ? owner : grenade;
}

static void Grenade_Spawn(edict_t *owner, const vec3_t &origin, const vec3_t &velocity, const loot_grenade_t &stats, loot_grenade_type_t thrown_type);

// Total health of living monsters in range (Leech measures what the blast took).
static float Grenade_MonsterHealthNear(const vec3_t &origin, float radius)
{
	float total = 0;
	edict_t *ent = nullptr;
	while ((ent = findradius(ent, origin, radius)) != nullptr)
		if ((ent->svflags & SVF_MONSTER) && ent->health > 0 && !ent->deadflag)
			total += ent->health;
	return total;
}

// Damage, effects and the type's extras (leech heal, cluster bomblets, airburst shards).
// Does not free the grenade: Hopper detonates several times.
static void Grenade_Detonate(edict_t *self)
{
	const loot_grenade_t &stats = self->loot_grenade;
	edict_t *attacker = Grenade_Attacker(self);
	const float health_before = stats.type == LOOT_GRENADE_LEECH ? Grenade_MonsterHealthNear(self->s.origin, stats.radius) : 0.f;

	grenade_blast = &stats;
	grenade_blast_type = self->style > 0 ? self->style - 1 : stats.type;
	T_RadiusDamage(self, attacker, stats.damage, nullptr, stats.radius, DAMAGE_NONE, MOD_LOOT_GRENADE);
	grenade_blast = nullptr;

	const bool twist = Grenade_Twist(stats);

	if (stats.type == LOOT_GRENADE_LEECH && attacker->client && attacker->health > 0)
	{
		const float taken = health_before - Grenade_MonsterHealthNear(self->s.origin, stats.radius);
		const int32_t heal = (int32_t)(std::max(0.f, taken) * (twist ? LEECH_TWIST_FRACTION : LEECH_FRACTION));
		if (heal > 0)
		{
			const int32_t overflow = attacker->health + heal - attacker->max_health;
			attacker->health = std::min(attacker->max_health, attacker->health + heal);

			client_persistant_t &pers = attacker->client->pers;
			if (twist && overflow > 0 && pers.loot_shield.valid)
			{
				const float cap = pers.loot_shield.capacity * Loot_ShieldCapMult(attacker);
				pers.loot_shield_points = std::min(cap, pers.loot_shield_points + overflow);
			}
		}
	}

	// Sticky twist: everything in the blast flinches (not bosses)
	if (stats.type == LOOT_GRENADE_STICKY && twist)
	{
		edict_t *ent = nullptr;
		while ((ent = findradius(ent, self->s.origin, stats.radius)) != nullptr)
		{
			if (!(ent->svflags & SVF_MONSTER) || ent->health <= 0 || ent->deadflag || !ent->pain || Loot_IsBossTier(ent))
				continue;
			ent->pain_debounce_time = 0_ms;
			ent->pain(ent, attacker, 0, (int32_t)stats.damage, MOD_LOOT_GRENADE);
		}
	}

	if (stats.type == LOOT_GRENADE_CLUSTER)
	{
		loot_grenade_t bomblet = stats;
		bomblet.type = LOOT_GRENADE_STANDARD;
		bomblet.rarity = std::min(stats.rarity, LOOT_RARITY_EPIC); // no Frag twist on bomblets
		bomblet.damage = stats.damage / grenade_types[LOOT_GRENADE_CLUSTER].damage * 0.35f;
		bomblet.radius = stats.radius * 0.6f;
		for (int32_t i = 0; i < (twist ? CLUSTER_TWIST_BOMBLETS : CLUSTER_BOMBLETS); i++)
		{
			bomblet.fuse = frandom(0.5f, 0.9f);
			const vec3_t velocity = {crandom() * 250.f, crandom() * 250.f, 250.f + frandom(150.f)};
			Grenade_Spawn(self->owner, self->s.origin + vec3_t{0, 0, 8}, velocity, bomblet, LOOT_GRENADE_CLUSTER);
		}
	}

	if (stats.type == LOOT_GRENADE_AIRBURST)
	{
		loot_grenade_t shard = stats;
		shard.type = LOOT_GRENADE_STANDARD;
		shard.rarity = std::min(stats.rarity, LOOT_RARITY_EPIC); // no Frag twist on shards
		shard.damage = stats.damage / grenade_types[LOOT_GRENADE_AIRBURST].damage * 0.30f;
		shard.radius = stats.radius * 0.5f;
		for (int32_t i = 0; i < (twist ? AIRBURST_TWIST_SHARDS : AIRBURST_SHARDS); i++)
		{
			shard.fuse = frandom(0.6f, 1.0f);
			const vec3_t offset = {crandom() * stats.radius * 0.4f, crandom() * stats.radius * 0.4f, 0};
			const vec3_t velocity = {crandom() * 120.f, crandom() * 120.f, -250.f};
			Grenade_Spawn(self->owner, self->s.origin + offset, velocity, shard, LOOT_GRENADE_AIRBURST);
		}
	}

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(self->waterlevel ? TE_GRENADE_EXPLOSION_WATER : (self->groundentity ? TE_GRENADE_EXPLOSION : TE_ROCKET_EXPLOSION));
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);
}

static void Grenade_Blast(edict_t *self)
{
	Grenade_Detonate(self);

	// Frag twist: stay put and blast again (count = 1 marks the echo)
	if (self->loot_grenade.type == LOOT_GRENADE_STANDARD && Grenade_Twist(self->loot_grenade) && !self->count)
	{
		self->count = 1;
		self->loot_grenade.damage *= FRAG_ECHO_DAMAGE;
		self->timestamp = level.time + FRAG_ECHO_DELAY;
		self->movetype = MOVETYPE_NONE;
		self->velocity = {};
		self->solid = SOLID_NOT;
		self->touch = nullptr;
		self->loot_stuck_to = nullptr;
		gi.linkentity(self);
		return;
	}

	G_FreeEdict(self);
}

// Hopper: blast, then pop back up; gone after HOPPER_HOPS blasts. count = hops so far.
static void Grenade_Hop(edict_t *self)
{
	if (level.time < self->touch_debounce_time)
		return; // one hop per impact
	self->touch_debounce_time = level.time + 200_ms;

	Grenade_Detonate(self);
	if (++self->count >= (Grenade_Twist(self->loot_grenade) ? HOPPER_TWIST_HOPS : HOPPER_HOPS))
	{
		G_FreeEdict(self);
		return;
	}
	self->velocity[2] = HOPPER_HOP_SPEED;
	self->groundentity = nullptr;
}

// Vortex pull: monsters in 1.5x radius slide toward the grenade (not bosses).
static void Grenade_Pull(edict_t *self)
{
	edict_t *ent = nullptr;
	const float pull_radius = self->loot_grenade.radius * (Grenade_Twist(self->loot_grenade) ? VORTEX_TWIST_RADIUS : 1.5f);
	while ((ent = findradius(ent, self->s.origin, pull_radius)) != nullptr)
	{
		if (!(ent->svflags & SVF_MONSTER) || ent->health <= 0 || ent->deadflag || Loot_IsBossTier(ent))
			continue;
		vec3_t dir = self->s.origin - ent->s.origin;
		if (dir.length() < 24.f)
			continue;
		dir = dir.normalized();
		ent->velocity = dir * VORTEX_PULL_SPEED + vec3_t{0, 0, 40};
		ent->groundentity = nullptr;
	}
}

THINK(grenade_mod_think)(edict_t *self)->void
{
	// Sticky: ride along with the monster; fall off if it is gone
	if (self->loot_stuck_to)
	{
		edict_t *target = self->loot_stuck_to;
		if (target->inuse && target->health > 0 && !target->deadflag)
		{
			self->s.origin = target->s.origin + self->loot_stuck_offset;
			gi.linkentity(self);
		}
		else
		{
			self->loot_stuck_to = nullptr;
			self->movetype = MOVETYPE_BOUNCE;
		}
	}

	if (level.time >= self->timestamp)
	{
		// Vortex: first time the fuse runs out it starts pulling (count = 1), then blows
		if (self->loot_grenade.type == LOOT_GRENADE_VORTEX && !self->count)
		{
			self->count = 1;
			self->timestamp = level.time + (Grenade_Twist(self->loot_grenade) ? VORTEX_TWIST_PULL_TIME : VORTEX_PULL_TIME);
			self->movetype = MOVETYPE_NONE;
			self->velocity = {};
			gi.sound(self, CHAN_VOICE, gi.soundindex("weapons/bfg__l1a.wav"), 1, ATTN_NORM, 0);
		}
		else
		{
			Grenade_Blast(self);
			return;
		}
	}

	if (self->count && self->loot_grenade.type == LOOT_GRENADE_VORTEX)
		Grenade_Pull(self);

	self->nextthink = level.time + FRAME_TIME_S;
}

TOUCH(grenade_mod_touch)(edict_t *self, edict_t *other, const trace_t &tr, bool other_touching_self)->void
{
	if (other == self->owner || self->loot_stuck_to)
		return;

	if (tr.surface && (tr.surface->flags & SURF_SKY))
	{
		G_FreeEdict(self);
		return;
	}

	const bool monster = (other->svflags & SVF_MONSTER) && other->takedamage && other->health > 0;

	// Sticky only sticks to enemies; walls and floors bounce it like a frag
	if (monster && self->loot_grenade.type == LOOT_GRENADE_STICKY)
	{
		self->loot_stuck_to = other;
		self->loot_stuck_offset = self->s.origin - other->s.origin;
		self->movetype = MOVETYPE_NONE;
		self->velocity = {};
		self->avelocity = {};
		self->solid = SOLID_NOT;
		gi.linkentity(self);
		gi.sound(self, CHAN_VOICE, gi.soundindex("weapons/grenlb1b.wav"), 1, ATTN_NORM, 0);
		return;
	}

	// Hopper blasts on every impact, enemy or wall
	if (self->loot_grenade.type == LOOT_GRENADE_HOPPER)
	{
		Grenade_Hop(self);
		return;
	}

	// frag, cluster, leech and airburst go off on contact with an enemy; vortex keeps rolling
	if (monster && self->loot_grenade.type != LOOT_GRENADE_VORTEX && !self->count)
	{
		Grenade_Blast(self);
		return;
	}

	if (!other->takedamage)
		gi.sound(self, CHAN_VOICE, gi.soundindex(frandom() > 0.5f ? "weapons/hgrenb1a.wav" : "weapons/hgrenb2a.wav"), 1, ATTN_NORM, 0);
}

static void Grenade_Spawn(edict_t *owner, const vec3_t &origin, const vec3_t &velocity, const loot_grenade_t &stats, loot_grenade_type_t thrown_type)
{
	edict_t *grenade = G_Spawn();
	grenade->classname = "loot_grenade_live";
	grenade->s.origin = origin;
	grenade->velocity = velocity;
	grenade->avelocity = {crandom() * 360, crandom() * 360, crandom() * 360};
	grenade->movetype = MOVETYPE_BOUNCE;
	grenade->clipmask = MASK_PROJECTILE;
	if (!G_ShouldPlayersCollide(true))
		grenade->clipmask &= ~CONTENTS_PLAYER;
	grenade->solid = SOLID_BBOX;
	grenade->svflags |= SVF_PROJECTILE;
	grenade->flags |= FL_DODGE;
	grenade->s.effects |= EF_GRENADE;
	grenade->s.modelindex = gi.modelindex("models/objects/grenade3/tris.md2");
	grenade->owner = owner;
	grenade->loot_grenade = stats;
	grenade->style = thrown_type + 1; // loot_stats credit (bomblets keep Cluster)
	grenade->timestamp = level.time + gtime_t::from_sec(stats.fuse);
	grenade->touch = grenade_mod_touch;
	grenade->think = grenade_mod_think;
	grenade->nextthink = level.time + FRAME_TIME_S;

	// rarity glow on the thrown grenade too
	const renderfx_t shell = Loot_RarityShell(stats.rarity);
	if (shell)
	{
		grenade->s.renderfx |= shell;
		grenade->s.effects |= EF_COLOR_SHELL;
	}
	gi.linkentity(grenade);
}

void Grenade_Throw(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (cl->loot_grenade_next > level.time + 2_sec)
		cl->loot_grenade_next = 0_ms; // left over from a previous map
	if (deathmatch->integer || ent->health <= 0 || level.intermissiontime || level.time < cl->loot_grenade_next)
		return;

	int32_t &ammo = cl->pers.inventory[IT_AMMO_GRENADES];
	if (ammo < 1)
	{
		gi.Center_Print(ent, "No grenades");
		return;
	}

	// no mod: a plain frag at your level
	loot_grenade_t stats = cl->pers.loot_grenade;
	if (!stats.valid)
	{
		stats = Grenade_Roll(LOOT_RARITY_COMMON, std::max(1, cl->pers.loot_level), 1);
		stats.type = LOOT_GRENADE_STANDARD;
		stats.element = LOOT_ELEM_NONE;
		stats.damage = GRENADE_BASE_DAMAGE * (1.0f + 0.06f * (stats.level - 1));
		stats.radius = GRENADE_BASE_RADIUS;
		stats.fuse = GRENADE_BASE_FUSE;
		stats.speed = GRENADE_BASE_SPEED;
	}

	ammo--;
	cl->loot_grenade_next = level.time + GRENADE_COOLDOWN;

	vec3_t forward, right, up;
	AngleVectors(cl->v_angle, forward, right, up);
	const vec3_t eye = ent->s.origin + vec3_t{0, 0, (float)ent->viewheight};
	const trace_t tr = gi.traceline(eye, eye + forward * 16.f, ent, MASK_SOLID);
	const vec3_t start = tr.endpos - forward * 2.f;
	const float arc = 200.f + (stats.type == LOOT_GRENADE_AIRBURST ? AIRBURST_ARC : 0.f); // airburst lobs high
	const vec3_t velocity = forward * stats.speed + up * arc * (level.gravity / 800.f);

	Grenade_Spawn(ent, start, velocity, stats, stats.type);
	level.loot_stats.grenades_thrown[stats.type]++;
	gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/hgrent1a.wav"), 1, ATTN_NORM, 0);
}
