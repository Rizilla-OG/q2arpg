// q2rpg: relics. A fifth gear piece next to guns, shields, grenade mods and class mods: one
// equipped, it gives one passive effect for any tree. Strength steps up with rarity
// (step 1 Common .. 6 Mythic):
//   Element       matching element procs: +6% chance and +6% proc damage per step
//   Fortune       +5% cash picked up and +0.05 loot luck per step
//   Quartermaster every 2 s, +2% of the held gun's magazine per step into its reserve ammo
//   Leech         heal 0.5% of gun damage dealt per step
//   Demolition    +6% grenade damage per step

#include "../g_local.h"

constexpr float RELIC_ELEMENT_PER_STEP = 0.06f;
constexpr float RELIC_CASH_PER_STEP = 0.05f;
constexpr float RELIC_LUCK_PER_STEP = 0.05f;
constexpr float RELIC_AMMO_MAG_SHARE_PER_STEP = 0.02f;
constexpr gtime_t RELIC_AMMO_INTERVAL = 2_sec;
constexpr float RELIC_LEECH_PER_STEP = 0.005f;
constexpr float RELIC_GRENADE_PER_STEP = 0.06f;

static const char *const relic_rarity_words[LOOT_RARITY_COUNT] = {"Worn", "Polished", "Etched", "Ancient", "Legendary", "Mythic"};
static const char *const relic_element_nouns[LOOT_ELEM_COUNT] = {"", "Ember Idol", "Static Idol", "Acid Idol", "Frost Idol", "Blast Idol"};
static const char *const relic_type_nouns[LOOT_RELIC_TYPE_COUNT] = {"Idol", "Lucky Coin", "Supply Token", "Blood Fang", "Powder Core"};
static const char *const relic_type_names[LOOT_RELIC_TYPE_COUNT] = {"Element", "Fortune", "Quartermaster", "Leech", "Demolition"};

static int32_t Relic_Step(const loot_relic_t &relic)
{
	return (relic.rarity < LOOT_RARITY_COUNT ? relic.rarity : 0) + 1;
}

// equipped relic of this type, or nullptr
static const loot_relic_t *Relic_Equipped(const edict_t *ent, loot_relic_type_t type)
{
	if (!ent || !ent->client || deathmatch->integer)
		return nullptr;
	const loot_relic_t &relic = ent->client->pers.loot_relic;
	return relic.valid && relic.type == type ? &relic : nullptr;
}

//==========================================================================
// Rolling
//==========================================================================

loot_relic_t Relic_Roll(loot_rarity_t rarity, int32_t level, uint32_t seed, int32_t forced_type)
{
	std::mt19937 rng(seed);

	loot_relic_t relic{};
	relic.valid = true;
	relic.seed = seed;
	relic.level = std::clamp(level, 1, Loot_LevelCap() + 5);
	relic.rarity = rarity < LOOT_RARITY_COUNT ? rarity : LOOT_RARITY_COMMON;
	relic.type = (forced_type >= 0 && forced_type < LOOT_RELIC_TYPE_COUNT) ? (loot_relic_type_t)forced_type
	                                                                       : (loot_relic_type_t)(rng() % LOOT_RELIC_TYPE_COUNT);
	relic.element = relic.type == LOOT_RELIC_ELEMENT ? (loot_element_t)(1 + rng() % (LOOT_ELEM_COUNT - 1)) : LOOT_ELEM_NONE;

	const char *noun = relic.type == LOOT_RELIC_ELEMENT ? relic_element_nouns[relic.element] : relic_type_nouns[relic.type];
	G_FmtTo(relic.name, "{} {}", relic_rarity_words[relic.rarity], noun);
	return relic;
}

//==========================================================================
// Effects
//==========================================================================

float Relic_ElementMult(const edict_t *ent, loot_element_t element)
{
	const loot_relic_t *relic = Relic_Equipped(ent, LOOT_RELIC_ELEMENT);
	if (!relic || element == LOOT_ELEM_NONE || relic->element != element)
		return 1.0f;
	return 1.0f + RELIC_ELEMENT_PER_STEP * Relic_Step(*relic);
}

float Relic_CashMult(const edict_t *ent)
{
	const loot_relic_t *relic = Relic_Equipped(ent, LOOT_RELIC_FORTUNE);
	return relic ? 1.0f + RELIC_CASH_PER_STEP * Relic_Step(*relic) : 1.0f;
}

float Relic_LuckBonus(const edict_t *ent)
{
	// shared drops (SP): the first player's relic
	if (!ent || !ent->client)
		ent = &g_edicts[1];
	if (!ent->inuse)
		return 0.f;
	const loot_relic_t *relic = Relic_Equipped(ent, LOOT_RELIC_FORTUNE);
	return relic ? RELIC_LUCK_PER_STEP * Relic_Step(*relic) : 0.f;
}

float Relic_GrenadeMult(const edict_t *ent)
{
	const loot_relic_t *relic = Relic_Equipped(ent, LOOT_RELIC_DEMOLITION);
	return relic ? 1.0f + RELIC_GRENADE_PER_STEP * Relic_Step(*relic) : 1.0f;
}

void Relic_OnGunHit(edict_t *attacker, int32_t damage)
{
	const loot_relic_t *relic = Relic_Equipped(attacker, LOOT_RELIC_LEECH);
	if (!relic || damage <= 0 || attacker->health <= 0 || attacker->health >= attacker->max_health)
		return;
	// fractions carry over so small hits still add up
	attacker->client->loot_relic_heal_carry += damage * RELIC_LEECH_PER_STEP * Relic_Step(*relic);
	const int32_t heal = (int32_t)attacker->client->loot_relic_heal_carry;
	if (heal <= 0)
		return;
	attacker->client->loot_relic_heal_carry -= heal;
	attacker->health = std::min(attacker->max_health, attacker->health + heal);
}

void Relic_Think(edict_t *ent)
{
	const loot_relic_t *relic = Relic_Equipped(ent, LOOT_RELIC_QUARTERMASTER);
	gclient_t *cl = ent->client;
	if (!relic || ent->health <= 0)
		return;
	if (cl->loot_relic_ammo_time > level.time + RELIC_AMMO_INTERVAL) // a new map reset the clock
		cl->loot_relic_ammo_time = level.time;
	if (level.time < cl->loot_relic_ammo_time)
		return;
	cl->loot_relic_ammo_time = level.time + RELIC_AMMO_INTERVAL;

	const loot_weapon_t *gun = Loot_Active(ent);
	if (!gun || gun->mag_size <= 0 || !cl->pers.weapon || !cl->pers.weapon->ammo)
		return;
	gitem_t *ammo = GetItemByIndex(cl->pers.weapon->ammo);
	if (!ammo || G_CheckInfiniteAmmo(ammo))
		return;
	const int32_t amount = std::max(1, (int32_t)ceilf(gun->mag_size * RELIC_AMMO_MAG_SHARE_PER_STEP * Relic_Step(*relic)));
	Add_Ammo(ent, ammo, amount);
}

//==========================================================================
// Text, cards, giving
//==========================================================================

const char *Relic_TypeName(const loot_relic_t &relic)
{
	return relic_type_names[relic.type < LOOT_RELIC_TYPE_COUNT ? relic.type : 0];
}

std::string Relic_EffectLine(const loot_relic_t &relic)
{
	const int32_t step = Relic_Step(relic);
	switch (relic.type)
	{
		case LOOT_RELIC_ELEMENT:
			return G_Fmt("{} procs: +{:.0f}% chance, +{:.0f}% damage", Loot_ElementName(relic.element), RELIC_ELEMENT_PER_STEP * step * 100.f,
			             RELIC_ELEMENT_PER_STEP * step * 100.f)
			    .data();
		case LOOT_RELIC_FORTUNE:
			return G_Fmt("+{:.0f}% cash, +{:.2f} loot luck", RELIC_CASH_PER_STEP * step * 100.f, RELIC_LUCK_PER_STEP * step).data();
		case LOOT_RELIC_QUARTERMASTER:
			return G_Fmt("+{:.0f}% of a magazine to reserve every {:.0f}s", RELIC_AMMO_MAG_SHARE_PER_STEP * step * 100.f, RELIC_AMMO_INTERVAL.seconds()).data();
		case LOOT_RELIC_LEECH:
			return G_Fmt("Heal {:.1f}% of gun damage dealt", RELIC_LEECH_PER_STEP * step * 100.f).data();
		case LOOT_RELIC_DEMOLITION:
			return G_Fmt("+{:.0f}% grenade damage", RELIC_GRENADE_PER_STEP * step * 100.f).data();
		default:
			return "";
	}
}

void Relic_SendCard(edict_t *ent, const loot_relic_t &relic)
{
	const loot_relic_t &other = ent->client->pers.loot_relic;

	std::string s;
	int32_t y = 64;
	s += G_Fmt("xv 24 yv {} string2 \"{}\" ", y, relic.name).data();
	y += 10;
	s += G_Fmt("xv 24 yv {} string \"Lv {} {} {} Relic\" ", y, relic.level, Loot_RarityName(relic.rarity), Relic_TypeName(relic)).data();
	y += 14;
	// green when it's the same effect and stronger than what you wear
	const bool stronger = other.valid && other.type == relic.type && other.element == relic.element && relic.rarity > other.rarity;
	s += G_Fmt("xv 24 yv {} loot_str \"{}{}^w\" ", y, stronger ? "^g" : "", Relic_EffectLine(relic)).data();
	y += 14;
	if (other.valid)
	{
		s += G_Fmt("xv 24 yv {} string \"vs {}: {}\" ", y, other.name, Relic_EffectLine(other)).data();
		y += 10;
	}
	s += G_Fmt("xv 24 yv {} string2 \"[E] take  [B] stash  [X] sell\" ", y).data();
	Loot_SendLayout(ent, s);
}

void Relic_Give(edict_t *ent, const loot_relic_t &relic)
{
	gclient_t *cl = ent->client;
	if (cl->pers.loot_relic.valid)
		Relic_SpawnDrop(ent, cl->pers.loot_relic, true);

	cl->pers.loot_relic = relic;
	cl->loot_relic_ammo_time = level.time + RELIC_AMMO_INTERVAL;
	gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/am_pkup.wav"), 1, ATTN_NORM, 0);
	gi.Center_Print(ent, G_Fmt("{} ({} {} Relic)\n{}", relic.name, Loot_RarityName(relic.rarity), Relic_TypeName(relic), Relic_EffectLine(relic)).data());
}
