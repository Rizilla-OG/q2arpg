// q2rpg: skill trees and action skills.
// Three trees act as classes. Picking one locks it until respec / new game.
// Each tree has tier 1 (skills 0-2), tier 2 (3-5, needs 5 points), tier 3
// (6-7, needs 10 points) and a capstone (8, needs 15 points, 1 rank).
// NG+ adds tier 4 (9-10, needs 20 points) and a second capstone (11), shown on
// page 2 of the skills screen.

#include "../g_local.h"

//==========================================================================
// Definitions
//==========================================================================

struct skill_def_t
{
	const char *name;
	const char *desc; // per rank
	int32_t max_rank;
};

constexpr int32_t SKILL_FIRST_TIER4 = 9; // skills 9-11: NG+ only

static bool Skill_Tier4Open()
{
	return game.loot_ngplus > 0;
}

static int32_t Skill_MaxRank(const skill_def_t &sk, int32_t skill)
{
	return (skill >= SKILL_FIRST_TIER4 && !Skill_Tier4Open()) ? 0 : sk.max_rank;
}

struct tree_def_t
{
	const char *name;
	const char *action;
	const char *blurb;
	float cooldown; // seconds
	skill_def_t skills[LOOT_NUM_SKILLS];
};

static const tree_def_t tree_defs[LOOT_TREE_COUNT] = {
    {},
    {"Gunner", "Turret", "Guns and a deployable turret", 40.f, {
                                                                   {"Steady Hands", "-6% spread", 5},
                                                                   {"Trigger Discipline", "+4% fire rate", 5},
                                                                   {"Quick Mags", "-6% reload time", 5},
                                                                   {"Hollow Points", "+5% gun damage", 5},
                                                                   {"Deep Pockets", "+8% magazine size", 5},
                                                                   {"Marksman", "+0.1x crit damage", 5},
                                                                   {"Turret Overclock", "turret +20% dmg, +2s", 5},
                                                                   {"Bullet Hose", "5% chance: free shot", 5},
                                                                   {"Twin Turrets", "deploy two turrets", 1},
                                                                   {"Scavenger", "kills refill 4% of mag", 5},
                                                                   {"Crossfire", "+4% dmg on turret's target", 5},
                                                                   {"Overwatch", "turret crits, uses gun element", 1},
                                                               }},
    {"Juggernaut", "Slam", "Toughness and a ground slam", 20.f, {
                                                                    {"Thick Skin", "+8 max health", 5},
                                                                    {"Capacitor", "+8% shield capacity", 5},
                                                                    {"Quick Charge", "-8% shield delay", 5},
                                                                    {"Bruiser", "-4% damage taken", 5},
                                                                    {"Regeneration", "+0.5 hp/s out of combat", 5},
                                                                    {"Close Quarters", "+6% dmg within 256u", 5},
                                                                    {"Shockwave", "slam +20% dmg, +32u", 5},
                                                                    {"Unstoppable", "slam restores 10% shield", 5},
                                                                    {"Aftershock", "slam hits twice", 1},
                                                                    {"Iron Will", "-5% dmg taken below 30% hp", 5},
                                                                    {"Retaliation", "shield break: 10% cap blast", 5},
                                                                    {"Earthbreaker", "slam shocks all it hits", 1},
                                                                }},
    {"Elementalist", "Overcharge", "Elements and an overcharge burst", 35.f, {
                                                                                 {"Catalyst", "+10% proc chance", 5},
                                                                                 {"Kindling", "+10% status damage", 5},
                                                                                 {"Lingering", "+10% status duration", 5},
                                                                                 {"Conductor", "+6% dmg vs afflicted", 5},
                                                                                 {"Attunement", "plain guns: 2% proc", 5},
                                                                                 {"Chain Reaction", "10%: afflicted kills burst", 5},
                                                                                 {"Overload", "overcharge +1s, +5% proc", 5},
                                                                                 {"Shield Battery", "5% status dmg -> shield", 5},
                                                                                 {"Avatar", "overcharge: every hit procs", 1},
                                                                                 {"Fusion", "+10% proc dmg, mixed element", 5},
                                                                                 {"Spread", "10%: status jumps on death", 5},
                                                                                 {"Prism Heart", "procs rotate all 5 elements", 1},
                                                                             }},
};

enum skill_id_t
{
	// gunner
	SK_STEADY = 0,
	SK_TRIGGER = 1,
	SK_QUICKMAGS = 2,
	SK_HOLLOW = 3,
	SK_POCKETS = 4,
	SK_MARKSMAN = 5,
	SK_OVERCLOCK = 6,
	SK_HOSE = 7,
	SK_TWIN = 8,
	SK_SCAVENGER = 9,
	SK_CROSSFIRE = 10,
	SK_OVERWATCH = 11,
	// juggernaut
	SK_SKIN = 0,
	SK_CAPACITOR = 1,
	SK_QUICKCHARGE = 2,
	SK_BRUISER = 3,
	SK_REGEN = 4,
	SK_CLOSE = 5,
	SK_SHOCKWAVE = 6,
	SK_UNSTOPPABLE = 7,
	SK_AFTERSHOCK = 8,
	SK_IRONWILL = 9,
	SK_RETALIATION = 10,
	SK_EARTHBREAKER = 11,
	// elementalist
	SK_CATALYST = 0,
	SK_KINDLING = 1,
	SK_LINGERING = 2,
	SK_CONDUCTOR = 3,
	SK_ATTUNE = 4,
	SK_CHAIN = 5,
	SK_OVERLOAD = 6,
	SK_BATTERY = 7,
	SK_AVATAR = 8,
	SK_FUSION = 9,
	SK_SPREAD = 10,
	SK_PRISM = 11,
};

constexpr int32_t SKILL_TIER2_POINTS = 5;
constexpr int32_t SKILL_TIER3_POINTS = 10;
constexpr int32_t SKILL_CAPSTONE_POINTS = 15;
constexpr int32_t SKILL_TIER4_POINTS = 20;

//==========================================================================
// Ranks and points
//==========================================================================

static loot_tree_t Skill_Tree(edict_t *ent)
{
	if (!ent || !ent->client)
		return LOOT_TREE_NONE;
	const int32_t t = ent->client->pers.loot_tree;
	return (t > LOOT_TREE_NONE && t < LOOT_TREE_COUNT) ? (loot_tree_t)t : LOOT_TREE_NONE;
}

//==========================================================================
// Grenade tie-ins: one free passive per tree
//==========================================================================

static const char *const tree_grenade_text[LOOT_TREE_COUNT] = {
    "",
    "Grenades: kills cut turret cooldown 2s",
    "Grenades: 5% of damage dealt charges shield",
    "Grenades: +15% proc chance; plain ones may proc",
};

constexpr gtime_t GUNNER_GRENADE_COOLDOWN_CUT = 2_sec;
constexpr float JUGGERNAUT_GRENADE_SHIELD = 0.05f;
constexpr float ELEMENTALIST_GRENADE_PROC = 0.15f;

// Gunner: grenade kills cut the turret cooldown.
void Skill_OnGrenadeKill(edict_t *ent)
{
	if (deathmatch->integer || !ent || !ent->client || Skill_Tree(ent) != LOOT_TREE_GUNNER)
		return;
	gclient_t *cl = ent->client;
	if (cl->loot_action_ready > level.time)
		cl->loot_action_ready = std::max(level.time, cl->loot_action_ready - GUNNER_GRENADE_COOLDOWN_CUT);
}

// Juggernaut: grenade damage dealt charges the shield.
void Skill_OnGrenadeHit(edict_t *ent, int32_t damage)
{
	if (deathmatch->integer || !ent || !ent->client || Skill_Tree(ent) != LOOT_TREE_JUGGERNAUT)
		return;
	client_persistant_t &pers = ent->client->pers;
	if (!pers.loot_shield.valid)
		return;
	const float cap = pers.loot_shield.capacity * Loot_ShieldCapMult(ent);
	pers.loot_shield_points = std::min(cap, pers.loot_shield_points + damage * JUGGERNAUT_GRENADE_SHIELD);
}

// Elementalist: grenades proc more often; plain grenades proc a random element.
void Skill_GrenadeElement(edict_t *ent, loot_element_t &element, float &chance)
{
	if (deathmatch->integer || !ent || !ent->client || Skill_Tree(ent) != LOOT_TREE_ELEMENTALIST)
		return;
	if (!element)
	{
		element = (loot_element_t)(1 + irandom(LOOT_ELEM_COUNT - 1));
		chance = 0.f;
	}
	chance = std::min(1.f, chance + ELEMENTALIST_GRENADE_PROC);
}

int32_t Skill_Rank(edict_t *ent, loot_tree_t tree, int32_t skill)
{
	if (deathmatch->integer || Skill_Tree(ent) != tree || skill < 0 || skill >= LOOT_NUM_SKILLS)
		return 0;
	int32_t class_mod_bonus = ClassMod_Bonus(ent, tree, skill);
	if (skill >= SKILL_FIRST_TIER4 && !Skill_Tier4Open())
		class_mod_bonus = 0; // tier 4 stays NG+ only
	return ent->client->pers.loot_skill_ranks[skill] + class_mod_bonus;
}

// q2rpg: skill table access for class mods (g_classmod.cpp)
const char *Skill_TreeName(int32_t tree)
{
	return (tree > LOOT_TREE_NONE && tree < LOOT_TREE_COUNT) ? tree_defs[tree].name : "None";
}

const char *Skill_Name(int32_t tree, int32_t skill)
{
	if (tree <= LOOT_TREE_NONE || tree >= LOOT_TREE_COUNT || skill < 0 || skill >= LOOT_NUM_SKILLS)
		return "?";
	return tree_defs[tree].skills[skill].name;
}

// class mods boost multi-rank skills only; Thick Skin is left out because its max health is
// applied once per rank point (Skill_ApplyRankEffects), not read live
bool Skill_Boostable(int32_t tree, int32_t skill)
{
	if (tree <= LOOT_TREE_NONE || tree >= LOOT_TREE_COUNT || skill < 0 || skill >= LOOT_NUM_SKILLS)
		return false;
	if (tree == LOOT_TREE_JUGGERNAUT && skill == SK_SKIN)
		return false;
	return tree_defs[tree].skills[skill].max_rank > 1;
}

static int32_t Skill_Spent(edict_t *ent)
{
	int32_t n = 0;
	for (uint8_t r : ent->client->pers.loot_skill_ranks)
		n += r;
	return n;
}

// points earned (one per level after the first) minus points in skills
static int32_t Skill_SparePoints(edict_t *ent)
{
	if (deathmatch->integer || !ent->client)
		return 0;
	return std::max(0, std::max(1, ent->client->pers.loot_level) - 1 - Skill_Spent(ent));
}

// every skill in the chosen tree is at its max rank (tier 4 counts only once NG+ opens it)
static bool Skill_TreeFull(edict_t *ent)
{
	const loot_tree_t tree = Skill_Tree(ent);
	if (!tree)
		return false;
	for (int32_t i = 0; i < LOOT_NUM_SKILLS; i++)
		if (ent->client->pers.loot_skill_ranks[i] < Skill_MaxRank(tree_defs[tree].skills[i], i))
			return false;
	return true;
}

// points that can still go into skills
int32_t Skill_Unspent(edict_t *ent)
{
	return Skill_TreeFull(ent) ? 0 : Skill_SparePoints(ent);
}

// Mastery: once the tree is full, every spare point adds MASTERY_PER_POINT to the
// tree's main stat, with no cap (high levels never run out of things to earn)
constexpr float MASTERY_PER_POINT = 0.01f;
static const char *const tree_mastery_text[LOOT_TREE_COUNT] = {"", "gun damage", "less damage taken", "status damage"};

static int32_t Skill_Mastery(edict_t *ent)
{
	return Skill_TreeFull(ent) ? Skill_SparePoints(ent) : 0;
}

static float Skill_MasteryBonus(edict_t *ent, loot_tree_t tree)
{
	return Skill_Tree(ent) == tree ? MASTERY_PER_POINT * Skill_Mastery(ent) : 0.f;
}

static int32_t Skill_TierRequirement(int32_t skill)
{
	if (skill >= SKILL_FIRST_TIER4)
		return SKILL_TIER4_POINTS;
	if (skill >= 8)
		return SKILL_CAPSTONE_POINTS;
	if (skill >= 6)
		return SKILL_TIER3_POINTS;
	if (skill >= 3)
		return SKILL_TIER2_POINTS;
	return 0;
}

//==========================================================================
// Passive effects
//==========================================================================

#define RANK(tree, sk) Skill_Rank(ent, tree, sk)

float Skill_SpreadMult(edict_t *ent)
{
	return 1.0f - 0.06f * RANK(LOOT_TREE_GUNNER, SK_STEADY);
}

float Skill_RateMult(edict_t *ent)
{
	float m = 1.0f + 0.04f * RANK(LOOT_TREE_GUNNER, SK_TRIGGER);
	if (Skill_Overcharged(ent))
		m *= 1.25f;
	return m;
}

float Skill_ReloadMult(edict_t *ent)
{
	return 1.0f - 0.06f * RANK(LOOT_TREE_GUNNER, SK_QUICKMAGS);
}

float Skill_GunDamageMult(edict_t *ent)
{
	return 1.0f + 0.05f * RANK(LOOT_TREE_GUNNER, SK_HOLLOW) + Skill_MasteryBonus(ent, LOOT_TREE_GUNNER);
}

float Skill_MagMult(edict_t *ent)
{
	return 1.0f + 0.08f * RANK(LOOT_TREE_GUNNER, SK_POCKETS);
}

float Skill_CritBonus(edict_t *ent)
{
	return 0.1f * RANK(LOOT_TREE_GUNNER, SK_MARKSMAN);
}

bool Skill_SaveAmmo(edict_t *ent)
{
	const int32_t r = RANK(LOOT_TREE_GUNNER, SK_HOSE);
	return r && frandom() < 0.05f * r;
}

float Skill_ShieldCapMult(edict_t *ent)
{
	return 1.0f + 0.08f * RANK(LOOT_TREE_JUGGERNAUT, SK_CAPACITOR);
}

float Skill_ShieldDelayMult(edict_t *ent)
{
	return 1.0f - 0.08f * RANK(LOOT_TREE_JUGGERNAUT, SK_QUICKCHARGE);
}

float Skill_DamageTakenMult(edict_t *ent)
{
	float m = (1.0f - 0.04f * RANK(LOOT_TREE_JUGGERNAUT, SK_BRUISER)) * (1.0f - Skill_MasteryBonus(ent, LOOT_TREE_JUGGERNAUT));

	// Iron Will: low health
	const int32_t iron = RANK(LOOT_TREE_JUGGERNAUT, SK_IRONWILL);
	if (iron && ent->health < ent->max_health * 0.3f)
		m *= 1.0f - 0.05f * iron;

	return m;
}

float Skill_OutgoingMult(edict_t *ent, edict_t *targ)
{
	float m = 1.0f;

	const int32_t close = RANK(LOOT_TREE_JUGGERNAUT, SK_CLOSE);
	if (close && (targ->s.origin - ent->s.origin).length() < 256.f)
		m *= 1.0f + 0.06f * close;

	const int32_t cond = RANK(LOOT_TREE_ELEMENTALIST, SK_CONDUCTOR);
	const loot_status_t &st = targ->loot_status;
	if (cond && st.active && (st.burn_end > level.time || st.melt_end > level.time || st.shock_end > level.time))
		m *= 1.0f + 0.06f * cond;

	// Crossfire: the monster your turret is zapping
	const int32_t cross = RANK(LOOT_TREE_GUNNER, SK_CROSSFIRE);
	if (cross && ent->client->loot_crossfire_target == targ && ent->client->loot_crossfire_until > level.time)
		m *= 1.0f + 0.04f * cross;

	return m;
}

// Fusion: a proc on a monster already afflicted by a different element hits harder
float Skill_FusionMult(edict_t *ent, edict_t *targ, loot_element_t element)
{
	const int32_t r = RANK(LOOT_TREE_ELEMENTALIST, SK_FUSION);
	const loot_status_t &st = targ->loot_status;
	if (!r || !st.active)
		return 1.0f;

	const bool other = (element != LOOT_ELEM_FIRE && st.burn_end > level.time) ||
	                   (element != LOOT_ELEM_CORROSIVE && st.melt_end > level.time) ||
	                   (element != LOOT_ELEM_SHOCK && st.shock_end > level.time) ||
	                   (element != LOOT_ELEM_CRYO && st.chill_end > level.time);
	return other ? 1.0f + 0.10f * r : 1.0f;
}

// Prism Heart: each proc takes the next element in fire -> shock -> corrosive -> cryo -> explosive
loot_element_t Skill_ProcElement(edict_t *ent, loot_element_t element)
{
	if (!element || !RANK(LOOT_TREE_ELEMENTALIST, SK_PRISM))
		return element;

	gclient_t *cl = ent->client;
	cl->loot_prism_next = (cl->loot_prism_next % (LOOT_ELEM_COUNT - 1)) + 1;
	return (loot_element_t)cl->loot_prism_next;
}

float Skill_ProcChanceMult(edict_t *ent)
{
	return 1.0f + 0.10f * RANK(LOOT_TREE_ELEMENTALIST, SK_CATALYST);
}

// flat bonus while overcharged; Avatar makes every hit proc
float Skill_ProcChanceBonus(edict_t *ent)
{
	if (!Skill_Overcharged(ent))
		return 0.f;
	if (RANK(LOOT_TREE_ELEMENTALIST, SK_AVATAR))
		return 1.0f;
	return 0.10f + 0.05f * RANK(LOOT_TREE_ELEMENTALIST, SK_OVERLOAD);
}

float Skill_DotMult(edict_t *ent)
{
	return 1.0f + 0.10f * RANK(LOOT_TREE_ELEMENTALIST, SK_KINDLING) + Skill_MasteryBonus(ent, LOOT_TREE_ELEMENTALIST);
}

float Skill_DurationMult(edict_t *ent)
{
	return 1.0f + 0.10f * RANK(LOOT_TREE_ELEMENTALIST, SK_LINGERING);
}

bool Skill_Attune(edict_t *ent)
{
	const int32_t r = RANK(LOOT_TREE_ELEMENTALIST, SK_ATTUNE);
	return r && frandom() < 0.02f * r;
}

bool Skill_Overcharged(edict_t *ent)
{
	return ent && ent->client && !deathmatch->integer && ent->client->loot_overcharge_end > level.time &&
	       Skill_Tree(ent) == LOOT_TREE_ELEMENTALIST;
}

void Skill_OnShieldDot(edict_t *ent, float dmg)
{
	const int32_t r = RANK(LOOT_TREE_ELEMENTALIST, SK_BATTERY);
	client_persistant_t &pers = ent->client->pers;
	if (!r || !pers.loot_shield.valid)
		return;

	const float cap = pers.loot_shield.capacity * Loot_ShieldCapMult(ent);
	pers.loot_shield_points = std::min(cap, pers.loot_shield_points + dmg * 0.05f * r);
}

// Spread: the dying monster's statuses jump to the nearest live monster at
// the same strength (hit sizes undo Loot_ApplyStatus's dps formulas).
static void Skill_SpreadStatus(edict_t *monster, edict_t *ent)
{
	const int32_t r = RANK(LOOT_TREE_ELEMENTALIST, SK_SPREAD);
	const loot_status_t st = monster->loot_status;
	if (!r || !st.active || frandom() >= 0.10f * r)
		return;

	const vec3_t org = (monster->absmin + monster->absmax) * 0.5f;
	edict_t *best = nullptr;
	float best_dist = 256.f;
	for (edict_t *e = nullptr; (e = findradius(e, org, 256.f)) != nullptr;)
	{
		if (e == monster || !(e->svflags & SVF_MONSTER) || e->health <= 0 || e->deadflag || !e->takedamage)
			continue;
		const float dist = (e->s.origin - org).length();
		if (dist < best_dist)
		{
			best = e;
			best_dist = dist;
		}
	}
	if (!best)
		return;

	const float dot = Skill_DotMult(ent);
	if (st.burn_end > level.time)
		Loot_ApplyStatus(best, ent, LOOT_ELEM_FIRE, (int32_t)(st.burn_dps / (0.6f * dot)));
	if (st.melt_end > level.time)
		Loot_ApplyStatus(best, ent, LOOT_ELEM_CORROSIVE, (int32_t)(st.melt_dps / (0.35f * dot)));
	if (st.shock_end > level.time)
		Loot_ApplyStatus(best, ent, LOOT_ELEM_SHOCK, (int32_t)(st.shock_dmg / (0.25f * dot)));
	if (st.chill_end > level.time)
		Loot_ApplyStatus(best, ent, LOOT_ELEM_CRYO, 1);
}

// Scavenger: kills top up the active gun's magazine
static void Skill_Scavenge(edict_t *ent)
{
	const int32_t r = RANK(LOOT_TREE_GUNNER, SK_SCAVENGER);
	if (r)
		Loot_RefillMag(ent, 0.04f * r);
}

// Chain Reaction: afflicted monsters may burst when they die.
void Skill_OnMonsterKilled(edict_t *monster, edict_t *ent)
{
	if (!ent || !ent->client)
		return;

	Skill_Scavenge(ent);
	Skill_SpreadStatus(monster, ent);

	const int32_t r = RANK(LOOT_TREE_ELEMENTALIST, SK_CHAIN);
	const loot_status_t &st = monster->loot_status;
	if (!r || !st.active || frandom() >= 0.10f * r)
		return;

	loot_element_t element = LOOT_ELEM_NONE;
	if (st.burn_end > level.time)
		element = LOOT_ELEM_FIRE;
	else if (st.melt_end > level.time)
		element = LOOT_ELEM_CORROSIVE;
	else if (st.shock_end > level.time)
		element = LOOT_ELEM_SHOCK;
	else if (st.chill_end > level.time)
		element = LOOT_ELEM_CRYO;
	if (!element)
		return;

	const int32_t dmg = std::max(10, (int32_t)(monster->max_health * 0.2f));
	const vec3_t org = (monster->absmin + monster->absmax) * 0.5f;

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1_NP);
	gi.WritePosition(org);
	gi.multicast(org, MULTICAST_PHS, false);

	edict_t *e = nullptr;
	while ((e = findradius(e, org, 160.f)) != nullptr)
	{
		if (e == monster || !(e->svflags & SVF_MONSTER) || e->health <= 0 || e->deadflag || !e->takedamage)
			continue;
		T_Damage(e, ent, ent, vec3_origin, e->s.origin, vec3_origin, dmg, 0, DAMAGE_RADIUS, MOD_LOOT_BURN);
		if (e->health > 0)
			Loot_ApplyStatus(e, ent, element, dmg);
	}
}

// Retaliation: a broken shield blasts nearby monsters
void Skill_OnShieldBreak(edict_t *ent, float capacity)
{
	const int32_t r = RANK(LOOT_TREE_JUGGERNAUT, SK_RETALIATION);
	if (!r)
		return;

	const int32_t dmg = std::max(1, (int32_t)(capacity * 0.10f * r));
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1_NP);
	gi.WritePosition(ent->s.origin);
	gi.multicast(ent->s.origin, MULTICAST_PHS, false);

	for (edict_t *e = nullptr; (e = findradius(e, ent->s.origin, 200.f)) != nullptr;)
	{
		if (!(e->svflags & SVF_MONSTER) || e->health <= 0 || e->deadflag || !e->takedamage)
			continue;
		T_Damage(e, ent, ent, vec3_origin, e->s.origin, vec3_origin, dmg, 200, DAMAGE_RADIUS, MOD_LOOT_SLAM);
	}
}

void Skill_OnHurt(edict_t *ent)
{
	if (ent->client)
		ent->client->loot_last_hurt = level.time;
}

bool Skill_IgnoreFall(edict_t *ent)
{
	gclient_t *cl = ent->client;
	// slam_start 0 = never slammed on this map
	return cl->loot_slam_active ||
	       (cl->loot_slam_start && cl->loot_slam_start <= level.time && level.time - cl->loot_slam_start < 3_sec);
}

//==========================================================================
// Action skills
//==========================================================================

static int32_t Skill_PlayerLevel(edict_t *ent)
{
	return std::max(1, ent->client->pers.loot_level);
}

static float Skill_LevelScale(edict_t *ent)
{
	return 1.0f + 0.06f * (Skill_PlayerLevel(ent) - 1);
}

// --- turret ---

static edict_t *Skill_TurretTarget(edict_t *self, const vec3_t &muzzle)
{
	edict_t *best = nullptr;
	float best_dist = 1024.f;

	edict_t *e = nullptr;
	while ((e = findradius(e, self->s.origin, 1024.f)) != nullptr)
	{
		if (!(e->svflags & SVF_MONSTER) || e->health <= 0 || e->deadflag || !e->takedamage)
			continue;
		if (e->monsterinfo.aiflags & AI_GOOD_GUY)
			continue;

		const vec3_t center = (e->absmin + e->absmax) * 0.5f;
		const float dist = (center - muzzle).length();
		if (dist >= best_dist)
			continue;

		const trace_t tr = gi.traceline(muzzle, center, self, MASK_SHOT);
		if (tr.ent != e && tr.fraction < 1.0f)
			continue;

		best = e;
		best_dist = dist;
	}

	return best;
}

THINK(skill_turret_think)(edict_t *self)->void
{
	edict_t *owner = self->owner;

	if (!owner || !owner->inuse || !owner->client || level.time >= self->timestamp)
	{
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_TELEPORT_EFFECT);
		gi.WritePosition(self->s.origin);
		gi.multicast(self->s.origin, MULTICAST_PVS, false);
		G_FreeEdict(self);
		return;
	}

	self->nextthink = level.time + 100_ms;

	// opening animation (stock tesla: frames 0-14, skins brighten at 10/12/14)
	if (self->s.frame < 14)
	{
		if (self->s.frame == 0)
			gi.sound(self, CHAN_VOICE, gi.soundindex("weapons/teslaopen.wav"), 1, ATTN_NORM, 0);
		self->s.frame++;
		if (self->s.frame == 10)
			self->s.skinnum = 1;
		else if (self->s.frame == 12)
			self->s.skinnum = 2;
		else if (self->s.frame == 14)
			self->s.skinnum = 3;
		return;
	}

	// zap 4 times a second
	if (level.time < self->monsterinfo.attack_finished)
		return;

	const vec3_t muzzle = self->s.origin + vec3_t{0, 0, 16};
	edict_t *target = Skill_TurretTarget(self, muzzle);
	if (!target)
		return;

	self->monsterinfo.attack_finished = level.time + 250_ms;

	const vec3_t end = (target->absmin + target->absmax) * 0.5f;
	vec3_t dir = end - muzzle;
	dir.normalize();

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_LIGHTNING);
	gi.WriteEntity(self);
	gi.WriteEntity(target);
	gi.WritePosition(muzzle);
	gi.WritePosition(end);
	gi.multicast(muzzle, MULTICAST_PVS, false);

	// Crossfire: mark the target for the owner's gun
	owner->client->loot_crossfire_target = target;
	owner->client->loot_crossfire_until = level.time + 500_ms;

	// Overwatch: 20% crits for double damage, procs the owner's gun element
	int32_t dmg = self->dmg;
	const bool overwatch = Skill_Rank(owner, LOOT_TREE_GUNNER, SK_OVERWATCH) > 0;
	if (overwatch && frandom() < 0.20f)
		dmg *= 2;

	// inflictor is the coil, attacker the owner so kills credit the player
	T_Damage(target, self, owner, dir, end, -dir, dmg, 0, DAMAGE_ENERGY, MOD_LOOT_TURRET);

	const loot_weapon_t *gun = Loot_Active(owner);
	if (overwatch && gun && gun->element && target->health > 0 && !target->deadflag && frandom() < 0.25f)
		Loot_ApplyStatus(target, owner, gun->element, dmg);

	if (self->count++ % 2 == 0)
		gi.sound(self, CHAN_WEAPON, gi.soundindex("weapons/tesla.wav"), 0.6f, ATTN_NORM, 0);
}

static void Skill_SpawnTurret(edict_t *ent, const vec3_t &offset)
{
	vec3_t forward, right;
	AngleVectors(ent->client->v_angle, forward, right, nullptr);
	forward[2] = 0;
	forward.normalize();

	const vec3_t start = ent->s.origin;
	vec3_t want = start + forward * 48.f + right * offset[1];
	trace_t tr = gi.trace(start, vec3_t{-8, -8, 0}, vec3_t{8, 8, 16}, want, ent, MASK_SOLID);
	vec3_t org = tr.endpos;

	// drop to the floor
	tr = gi.trace(org, vec3_t{-8, -8, 0}, vec3_t{8, 8, 16}, org - vec3_t{0, 0, 256}, ent, MASK_SOLID);
	org = tr.endpos;

	edict_t *t = G_Spawn();
	t->classname = "skill_turret";
	t->owner = ent;
	t->s.origin = org;
	t->s.angles = {0.f, ent->client->v_angle[YAW], 0.f};
	t->movetype = MOVETYPE_NONE;
	t->solid = SOLID_NOT;
	// stock tesla mine: a floor deployable with an opening animation
	gi.setmodel(t, "models/weapons/g_tesla/tris.md2");
	t->s.frame = 0;
	t->s.skinnum = 0;

	const int32_t oc = Skill_Rank(ent, LOOT_TREE_GUNNER, SK_OVERCLOCK);
	// 4 zaps/s (old bullet turret: 10 shots of 6)
	t->dmg = std::max(1, (int32_t)(15.f * Skill_LevelScale(ent) * (1.0f + 0.2f * oc)));
	t->timestamp = level.time + gtime_t::from_sec(15.f + 2.f * oc);
	t->think = skill_turret_think;
	t->nextthink = level.time + 100_ms;
	gi.linkentity(t);

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_TELEPORT_EFFECT);
	gi.WritePosition(org);
	gi.multicast(org, MULTICAST_PVS, false);
}

static void Skill_Turret(edict_t *ent)
{
	if (Skill_Rank(ent, LOOT_TREE_GUNNER, SK_TWIN))
	{
		Skill_SpawnTurret(ent, vec3_t{0, -32, 0});
		Skill_SpawnTurret(ent, vec3_t{0, 32, 0});
	}
	else
		Skill_SpawnTurret(ent, vec3_origin);

	gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/spawn1.wav"), 1, ATTN_NORM, 0);
}

// --- slam ---

static void Skill_Shockwave(edict_t *ent, float scale)
{
	const int32_t sw = Skill_Rank(ent, LOOT_TREE_JUGGERNAUT, SK_SHOCKWAVE);
	const float radius = 200.f + 32.f * sw;
	const int32_t dmg = std::max(1, (int32_t)(40.f * Skill_LevelScale(ent) * (1.0f + 0.2f * sw) * scale));
	const bool earthbreaker = Skill_Rank(ent, LOOT_TREE_JUGGERNAUT, SK_EARTHBREAKER) > 0;

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1_NP);
	gi.WritePosition(ent->s.origin);
	gi.multicast(ent->s.origin, MULTICAST_PHS, false);
	gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/rocklx1a.wav"), 1, ATTN_NORM, 0);

	edict_t *e = nullptr;
	while ((e = findradius(e, ent->s.origin, radius)) != nullptr)
	{
		if (!(e->svflags & SVF_MONSTER) || e->health <= 0 || e->deadflag || !e->takedamage)
			continue;

		vec3_t dir = e->s.origin - ent->s.origin;
		dir[2] = 0;
		dir.normalize();
		dir[2] = 0.5f;
		T_Damage(e, ent, ent, dir, e->s.origin, vec3_origin, dmg, 300, DAMAGE_RADIUS, MOD_LOOT_SLAM);

		// Earthbreaker: shock (stagger) everything the slam hits
		if (earthbreaker && e->health > 0 && !e->deadflag)
			Loot_ApplyStatus(e, ent, LOOT_ELEM_SHOCK, dmg);
	}

	// Unstoppable
	const int32_t un = Skill_Rank(ent, LOOT_TREE_JUGGERNAUT, SK_UNSTOPPABLE);
	client_persistant_t &pers = ent->client->pers;
	if (un && pers.loot_shield.valid && scale >= 1.0f)
	{
		const float cap = pers.loot_shield.capacity * Loot_ShieldCapMult(ent);
		pers.loot_shield_points = std::min(cap, pers.loot_shield_points + cap * 0.10f * un);
	}
}

static void Skill_Slam(edict_t *ent)
{
	gclient_t *cl = ent->client;
	vec3_t forward;
	AngleVectors(cl->v_angle, forward, nullptr, nullptr);
	forward[2] = 0;
	forward.normalize();

	if (ent->groundentity)
	{
		ent->velocity = forward * 500.f;
		ent->velocity[2] = 320.f;
		ent->groundentity = nullptr;
	}
	else
		ent->velocity[2] = -900.f; // slam down from the air

	cl->loot_slam_active = true;
	cl->loot_slam_start = level.time;
	gi.sound(ent, CHAN_VOICE, gi.soundindex("*jump1.wav"), 1, ATTN_NORM, 0);
}

// --- overcharge ---

static void Skill_Overcharge(edict_t *ent)
{
	const int32_t ol = Skill_Rank(ent, LOOT_TREE_ELEMENTALIST, SK_OVERLOAD);
	ent->client->loot_overcharge_end = level.time + gtime_t::from_sec(8.f + ol);
	gi.sound(ent, CHAN_AUTO, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
	gi.Center_Print(ent, "OVERCHARGE");
}

static void Skill_UseAction(edict_t *ent)
{
	gclient_t *cl = ent->client;
	const loot_tree_t tree = Skill_Tree(ent);

	if (ent->health <= 0)
		return;
	if (!tree)
	{
		gi.Center_Print(ent, "Pick a skill tree first [K]");
		return;
	}
	if (cl->loot_action_ready > level.time)
	{
		gi.Center_Print(ent, G_Fmt("{} ready in {:.0f}s", tree_defs[tree].action, (cl->loot_action_ready - level.time).seconds()).data());
		return;
	}

	switch (tree)
	{
		case LOOT_TREE_GUNNER:
			Skill_Turret(ent);
			break;
		case LOOT_TREE_JUGGERNAUT:
			Skill_Slam(ent);
			break;
		case LOOT_TREE_ELEMENTALIST:
			Skill_Overcharge(ent);
			break;
		default:
			return;
	}

	cl->loot_action_ready = level.time + gtime_t::from_sec(tree_defs[tree].cooldown);
}

//==========================================================================
// Per frame
//==========================================================================

static void Skill_ApplyRankEffects(edict_t *ent, loot_tree_t tree, int32_t skill, int32_t delta);

// Ranks above the current max (the old NG+ +2 ranks, or tier 4 outside NG+) are
// refunded; unspent points are derived from ranks, so lowering a rank refunds it.
static void Skill_ClampRanks(edict_t *ent)
{
	gclient_t *cl = ent->client;
	const loot_tree_t tree = Skill_Tree(ent);
	if (!tree)
		return;

	for (int32_t i = 0; i < LOOT_NUM_SKILLS; i++)
	{
		uint8_t &rank = cl->pers.loot_skill_ranks[i];
		const int32_t max = Skill_MaxRank(tree_defs[tree].skills[i], i);
		if (rank > max)
		{
			Skill_ApplyRankEffects(ent, tree, i, max - rank);
			rank = (uint8_t)max;
			cl->loot_skills_time = 0_ms;
		}
	}
}

void Skill_Frame(edict_t *ent)
{
	gclient_t *cl = ent->client;

	Skill_ClampRanks(ent);

	// timers left over from a previous map
	if (cl->loot_action_ready > level.time + 120_sec)
		cl->loot_action_ready = 0_ms;
	if (cl->loot_overcharge_end > level.time + 60_sec)
		cl->loot_overcharge_end = 0_ms;
	if (cl->loot_slam_start > level.time)
	{
		cl->loot_slam_start = 0_ms;
		cl->loot_slam_active = false;
		cl->loot_aftershock_time = 0_ms;
	}

	if (ent->health <= 0)
	{
		cl->loot_slam_active = false;
		cl->loot_aftershock_time = 0_ms;
		return;
	}

	// slam landing
	if (cl->loot_slam_active && level.time - cl->loot_slam_start > 200_ms &&
	    (ent->groundentity || level.time - cl->loot_slam_start > 2500_ms))
	{
		cl->loot_slam_active = false;
		Skill_Shockwave(ent, 1.0f);
		if (Skill_Rank(ent, LOOT_TREE_JUGGERNAUT, SK_AFTERSHOCK))
			cl->loot_aftershock_time = level.time + 1_sec;
	}

	if (cl->loot_aftershock_time && level.time >= cl->loot_aftershock_time)
	{
		cl->loot_aftershock_time = 0_ms;
		Skill_Shockwave(ent, 0.6f);
	}

	// regeneration out of combat
	const int32_t regen = Skill_Rank(ent, LOOT_TREE_JUGGERNAUT, SK_REGEN);
	const bool calm = cl->loot_last_hurt > level.time || level.time - cl->loot_last_hurt >= 3_sec;
	if (regen && calm && ent->health < ent->max_health)
	{
		cl->loot_regen_accum += 0.5f * regen * FRAME_TIME_S.seconds();
		if (cl->loot_regen_accum >= 1.0f)
		{
			const int32_t add = (int32_t)cl->loot_regen_accum;
			cl->loot_regen_accum -= add;
			ent->health = std::min(ent->max_health, ent->health + add);
		}
	}
}

void Skill_SetActionStat(edict_t *ent, int32_t playernum)
{
	gclient_t *cl = ent->client;
	const loot_tree_t tree = Skill_Tree(ent);
	char line[48] = "";

	if (tree)
	{
		const tree_def_t &t = tree_defs[tree];
		if (Skill_Overcharged(ent))
			G_FmtTo(line, "[F] {} ACTIVE {:.0f}s", t.action, ceilf((cl->loot_overcharge_end - level.time).seconds()));
		else if (cl->loot_action_ready > level.time)
			G_FmtTo(line, "[F] {} {:.0f}s", t.action, ceilf((cl->loot_action_ready - level.time).seconds()));
		else
			G_FmtTo(line, "[F] {} READY", t.action);
	}

	// Second Chance countdown takes over the action line
	if (Loot_SecondChanceActive(ent))
		G_FmtTo(line, "SECOND CHANCE {:.0f}s - GET A KILL", ceilf((cl->loot_second_chance_end - level.time).seconds()));

	const int32_t cs = CONFIG_LOOT_ACTION + playernum;
	if (strcmp(line, cl->loot_action_cache))
	{
		Q_strlcpy(cl->loot_action_cache, line, sizeof(cl->loot_action_cache));
		gi.configstring(cs, line);
	}
	cl->ps.stats[STAT_LOOT_ACTION] = line[0] ? (int16_t)cs : 0;
}

//==========================================================================
// Skills screen
//==========================================================================

static void Skill_SendScreen(edict_t *ent)
{
	gclient_t *cl = ent->client;
	const loot_tree_t tree = Skill_Tree(ent);
	const int32_t unspent = Skill_Unspent(ent);
	// dim backdrop first so text draws on top; left edge; columns: key, name, rank, effect
	std::string s = "loot_dim xl 16 loot_tabs \"16 166 206\" ";
	int32_t y = 16;

	auto line = [&](const char *kind, const std::string &text)
	{
		s += G_Fmt("yv {} {} \"{}\" ", y, kind, text).data();
		y += 10;
	};

	if (!tree)
	{
		line("string2", "CHOOSE A SKILL TREE");
		line("string", G_Fmt("Locked until respec or new game. Points: {}", unspent).data());
		y += 6;
		for (int32_t t = 1; t < LOOT_TREE_COUNT; t++)
		{
			const tree_def_t &d = tree_defs[t];
			line("string2", G_Fmt("[{}] {}", t, d.name).data());
			line("string", G_Fmt("    {}", d.blurb).data());
			line("string", G_Fmt("    Action: {} ({:.0f}s cooldown)", d.action, d.cooldown).data());
			line("string", G_Fmt("    {}", tree_grenade_text[t]).data());
			y += 4;
		}
		y += 4;
		line("string2", "[1-3] choose  [K] close");
		gi.WriteByte(svc_layout);
		gi.WriteString(s.c_str());
		gi.unicast(ent, true);
		return;
	}

	const tree_def_t &d = tree_defs[tree];
	const int32_t spent = Skill_Spent(ent);
	if (Skill_TreeFull(ent))
		line("string2", G_Fmt("{} - MASTERY {}: +{}% {}", d.name, Skill_Mastery(ent), Skill_Mastery(ent), tree_mastery_text[tree]).data());
	else
		line("string2", G_Fmt("{} - {} points to spend", d.name, unspent).data());
	line("string", G_Fmt("Action [F]: {} ({:.0f}s)   Spent: {}", d.action, d.cooldown, spent).data());
	line("string", tree_grenade_text[tree]);
	y += 4;

	// page 0: tiers 1-3 and capstone, keys 1-9; page 1 (NG+): tier 4, keys 1-3
	static const char *const tier_names[] = {"TIER 1", "TIER 2 (5 pts)", "TIER 3 (10 pts)", "CAPSTONE (15 pts)", "TIER 4 - NG+ (20 pts)"};
	const bool page2 = cl->loot_skills_page == 1;
	const int32_t first = page2 ? SKILL_FIRST_TIER4 : 0;
	const int32_t last = page2 ? LOOT_NUM_SKILLS : SKILL_FIRST_TIER4;
	int32_t last_tier = -1;
	for (int32_t i = first; i < last; i++)
	{
		const int32_t tier = i >= SKILL_FIRST_TIER4 ? 4 : i >= 8 ? 3
		                                              : i >= 6   ? 2
		                                              : i >= 3   ? 1
		                                                         : 0;
		if (tier != last_tier)
		{
			if (last_tier >= 0)
				y += 3;
			const bool open = spent >= Skill_TierRequirement(i);
			line(open ? "string2" : "string", G_Fmt("{}{}", tier_names[tier], open ? "" : " - locked").data());
			last_tier = tier;
		}

		const skill_def_t &sk = d.skills[i];
		const int32_t rank = cl->pers.loot_skill_ranks[i];
		const int32_t class_mod_bonus = Skill_Rank(ent, tree, i) - rank; // shown as green +N
		if (class_mod_bonus > 0)
			line("loot_str", G_Fmt("{}|{}|{}^g+{}^w/{}|{}", i - first + 1, sk.name, rank, class_mod_bonus, Skill_MaxRank(sk, i), sk.desc).data());
		else
			line("loot_str", G_Fmt("{}|{}|{}/{}|{}", i - first + 1, sk.name, rank, Skill_MaxRank(sk, i), sk.desc).data());
	}

	y += 6;
	if (page2)
		line("string2", "[1-3] add point  [K] close");
	else if (Skill_Tier4Open())
		line("string2", "[1-9] add point  [K] tier 4 page");
	else
		line("string2", "[1-9] add point  [K] close");

	gi.WriteByte(svc_layout);
	gi.WriteString(s.c_str());
	gi.unicast(ent, true);
}

bool Skill_Layout(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (!cl->loot_showskills)
		return false;

	if (level.time >= cl->loot_skills_time || cl->loot_skills_time > level.time + 1_sec)
	{
		Skill_SendScreen(ent);
		cl->loot_skills_time = level.time + 500_ms;
	}
	return true;
}

static void Skill_ApplyRankEffects(edict_t *ent, loot_tree_t tree, int32_t skill, int32_t delta)
{
	// Thick Skin: +8 max health per rank
	if (tree == LOOT_TREE_JUGGERNAUT && skill == SK_SKIN)
	{
		client_persistant_t &pers = ent->client->pers;
		pers.max_health = std::max(1, pers.max_health + 8 * delta);
		ent->max_health = pers.max_health;
		if (delta > 0 && ent->health > 0)
			ent->health += 8 * delta;
		ent->health = std::min(ent->health, ent->max_health);
	}
}

static void Skill_PickTree(edict_t *ent, int32_t tree)
{
	gclient_t *cl = ent->client;
	if (Skill_Tree(ent) || tree <= LOOT_TREE_NONE || tree >= LOOT_TREE_COUNT)
		return;

	cl->pers.loot_tree = tree;
	cl->pers.loot_skill_ranks = {};
	cl->loot_action_ready = 0_ms;
	cl->loot_skills_time = 0_ms;
	gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/secret.wav"), 1, ATTN_NORM, 0);
	gi.Center_Print(ent, G_Fmt("{}\nAction skill: {} [F]", tree_defs[tree].name, tree_defs[tree].action).data());
}

static void Skill_AddPoint(edict_t *ent, int32_t skill)
{
	gclient_t *cl = ent->client;
	const loot_tree_t tree = Skill_Tree(ent);
	if (!tree || skill < 0 || skill >= LOOT_NUM_SKILLS)
		return;

	const skill_def_t &sk = tree_defs[tree].skills[skill];
	uint8_t &rank = cl->pers.loot_skill_ranks[skill];

	if (Skill_Unspent(ent) <= 0)
	{
		gi.Center_Print(ent, "No skill points");
		return;
	}
	if (rank >= Skill_MaxRank(sk, skill))
		return;
	if (Skill_Spent(ent) < Skill_TierRequirement(skill))
	{
		gi.Center_Print(ent, G_Fmt("Needs {} points spent", Skill_TierRequirement(skill)).data());
		return;
	}

	rank++;
	Skill_ApplyRankEffects(ent, tree, skill, 1);
	cl->loot_skills_time = 0_ms;
	gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/w_pkup.wav"), 1, ATTN_NORM, 0);
}

void Skill_Respec(edict_t *ent)
{
	gclient_t *cl = ent->client;
	const loot_tree_t tree = Skill_Tree(ent);

	for (int32_t i = 0; i < LOOT_NUM_SKILLS; i++)
		if (cl->pers.loot_skill_ranks[i])
			Skill_ApplyRankEffects(ent, tree, i, -(int32_t)cl->pers.loot_skill_ranks[i]);

	cl->pers.loot_skill_ranks = {};
	cl->pers.loot_tree = LOOT_TREE_NONE;
	cl->loot_overcharge_end = 0_ms;
	cl->loot_skills_time = 0_ms;
	gi.Center_Print(ent, "Skills reset");
}

static bool Skill_CheatCheck(edict_t *ent)
{
	if (game.maxclients > 1 && !sv_cheats->integer)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_need_cheats");
		return false;
	}
	return true;
}

bool Skill_ClientCommand(edict_t *ent, const char *cmd)
{
	gclient_t *cl = ent->client;

	if (!Q_strcasecmp(cmd, "skills"))
	{
		// K cycles: closed -> page 1 -> tier 4 page (NG+, tree picked) -> closed
		if (!cl->loot_showskills)
		{
			cl->loot_showskills = true;
			cl->loot_skills_page = 0;
		}
		else if (cl->loot_skills_page == 0 && Skill_Tier4Open() && Skill_Tree(ent))
			cl->loot_skills_page = 1;
		else
			cl->loot_showskills = false;
		cl->loot_showinv = false;
		cl->loot_showchallenges = false;
		cl->loot_skills_time = 0_ms;
		return true;
	}

	// while the skills screen is open, number keys pick / spend
	if (!Q_strcasecmp(cmd, "slot") && cl->loot_showskills)
	{
		const int32_t n = atoi(gi.argv(1));
		if (!Skill_Tree(ent))
			Skill_PickTree(ent, n);
		else if (cl->loot_skills_page == 1)
		{
			if (n >= 1 && n <= LOOT_NUM_SKILLS - SKILL_FIRST_TIER4)
				Skill_AddPoint(ent, SKILL_FIRST_TIER4 + n - 1);
		}
		else if (n >= 1 && n <= SKILL_FIRST_TIER4)
			Skill_AddPoint(ent, n - 1);
		return true;
	}

	if (!Q_strcasecmp(cmd, "skill_pick"))
	{
		Skill_PickTree(ent, atoi(gi.argv(1)));
		return true;
	}
	if (!Q_strcasecmp(cmd, "skill_add"))
	{
		Skill_AddPoint(ent, atoi(gi.argv(1)) - 1);
		return true;
	}
	if (!Q_strcasecmp(cmd, "action"))
	{
		Skill_UseAction(ent);
		return true;
	}
	if (!Q_strcasecmp(cmd, "skill_respec"))
	{
		// costs money once vendors exist (M6); cheat-only until then
		if (Skill_CheatCheck(ent))
			Skill_Respec(ent);
		return true;
	}

	return false;
}

void Skill_OnLevelUp(edict_t *ent)
{
	ent->client->loot_skills_time = 0_ms;
}
