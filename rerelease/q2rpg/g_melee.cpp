// q2rpg: melee. The loot_melee command (default Z) strikes whatever is right in front of you.
// Damage scales with your level like gun damage, the held gun's stock maker changes it
// (sturdy Brask stocks hit hardest), Juggernauts hit +50% harder, and a head hit crits.
// Plays no first-person animation: a view punch, a sound and knockback sell it.

#include "../g_local.h"

constexpr float MELEE_BASE_DAMAGE = 100.f; // at level 1, before the level scaling
constexpr float MELEE_RANGE = 72.f;
constexpr gtime_t MELEE_COOLDOWN = 600_ms;
constexpr int32_t MELEE_KNOCKBACK = 250;
constexpr float MELEE_JUGGERNAUT_MULT = 1.5f;
constexpr float MELEE_CRIT_MULT = 1.5f;
static const vec3_t MELEE_BOX_MINS = {-12, -12, -12};
static const vec3_t MELEE_BOX_MAXS = {12, 12, 12};

// held gun's stock maker (loot_maker_t order): Hardline, Vexa, Brask, Kessler, Orbitek, Salvage
static const float melee_stock_mult[LOOT_MAKER_COUNT] = {1.25f, 0.85f, 1.4f, 0.9f, 1.0f, 1.1f};

static float Melee_Damage(edict_t *ent)
{
	const int32_t player_level = std::max(1, ent->client->pers.loot_level);
	float damage = MELEE_BASE_DAMAGE * (1.0f + 0.06f * (player_level - 1));
	if (const loot_weapon_t *gun = Loot_Active(ent))
	{
		const uint8_t stock_maker = gun->parts[LOOT_PART_STOCK];
		damage *= melee_stock_mult[stock_maker < LOOT_MAKER_COUNT ? stock_maker : 0];
	}
	if (ent->client->pers.loot_tree == LOOT_TREE_JUGGERNAUT)
		damage *= MELEE_JUGGERNAUT_MULT;
	return damage;
}

// Loot_ModifyDamage: melee hits skip the held gun's stats; a hit on the top quarter crits
int32_t Melee_ModifyDamage(edict_t *targ, const vec3_t &point, int32_t damage, bool &crit)
{
	crit = point[2] >= targ->absmax[2] - targ->size[2] * 0.25f;
	return crit ? (int32_t)ceilf(damage * MELEE_CRIT_MULT) : damage;
}

void Melee_Cmd(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (deathmatch->integer || ent->health <= 0 || level.intermissiontime || cl->loot_shop_open || level.time < cl->loot_melee_ready)
		return;
	cl->loot_melee_ready = level.time + MELEE_COOLDOWN;

	vec3_t forward, right, up;
	AngleVectors(cl->v_angle, forward, right, up);
	const vec3_t start = ent->s.origin + vec3_t{0, 0, (float)ent->viewheight};
	const vec3_t end = start + forward * MELEE_RANGE;
	const trace_t tr = gi.trace(start, MELEE_BOX_MINS, MELEE_BOX_MAXS, end, ent, MASK_SHOT);

	P_AddWeaponKick(ent, forward * 2.f, {-4.f, crandom() * 2.f, 0.f}); // view punch
	PlayerNoise(ent, start, PNOISE_SELF);

	if (tr.fraction >= 1.0f || !tr.ent || !tr.ent->takedamage)
	{
		gi.sound(ent, CHAN_WEAPON, gi.soundindex("infantry/melee2.wav"), 1, ATTN_NORM, 0); // swing and miss
		return;
	}

	gi.sound(ent, CHAN_WEAPON, gi.soundindex("mutant/thud1.wav"), 1, ATTN_NORM, 0);
	const int32_t damage = std::max(1, (int32_t)Melee_Damage(ent));
	T_Damage(tr.ent, ent, ent, forward, tr.endpos, tr.plane.normal, damage, MELEE_KNOCKBACK, DAMAGE_NONE, MOD_LOOT_MELEE);

	// sparks where it landed
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_SPARKS);
	gi.WritePosition(tr.endpos);
	gi.WriteDir(tr.plane.normal);
	gi.multicast(tr.endpos, MULTICAST_PVS, false);
}
