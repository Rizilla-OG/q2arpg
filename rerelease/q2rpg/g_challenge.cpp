// q2rpg: challenges. Kill / headshot / element / weapon goals with three tiers each,
// plus a fourth tier in NG+. Progress is per player and persistent (client_persistant_t);
// each tier pays XP and cash. Tier 4 also drops a Mythic reward and grants a badge: a
// small permanent bonus, read from the saved tier (no extra save field).
// The J key ("challenges") toggles the screen.

#include "../g_local.h"

enum challenge_kind_t
{
	CH_KILLS,
	CH_HEADSHOT_KILLS,
	CH_FIRE_KILLS,
	CH_SHOCK_KILLS,
	CH_CORROSIVE_KILLS,
	CH_ELITE_KILLS,
	CH_BOSS_KILLS,
	CH_PISTOL_KILLS,
	CH_SHOTGUN_KILLS, // shotgun + scattergun
	CH_SKILL_KILLS,   // turret / slam
	CH_CASH,
	CH_GRENADE_KILLS,
	CH_OMEGA_KILLS,
};

// What tier 4 drops at the player's feet
enum challenge_reward_t
{
	RW_MYTHIC_GUN,     // random archetype (or `arch` when set)
	RW_MYTHIC_ELEMENT, // random archetype with `element`
	RW_MYTHIC_SHIELD,
	RW_MYTHIC_GRENADE,
	RW_SECOND_UNIQUE, // a random archetype's second unique
};

struct challenge_def_t
{
	challenge_kind_t kind;
	const char *name;
	const char *goal;                    // what counts, shown on the screen
	int32_t tiers[LOOT_CHALLENGE_TIERS]; // count needed for tier 1, 2, 3, 4 (NG+)
	challenge_reward_t reward;
	int32_t arch; // RW_MYTHIC_GUN: -1 = random
	loot_element_t element;
	const char *badge; // tier-4 badge, shown on the screen
};

static const challenge_def_t challenges[LOOT_NUM_CHALLENGES] = {
    {CH_KILLS, "Exterminator", "Kills", {50, 250, 1000, 4000}, RW_MYTHIC_GUN, -1, LOOT_ELEM_NONE, "+3% gun damage"},
    {CH_HEADSHOT_KILLS, "Sharpshooter", "Headshot kills", {25, 100, 400, 1500}, RW_MYTHIC_GUN, LOOT_ARCH_RAIL, LOOT_ELEM_NONE, "+0.05x crit damage"},
    {CH_FIRE_KILLS, "Pyromaniac", "Kills while burning", {25, 150, 500, 2000}, RW_MYTHIC_ELEMENT, -1, LOOT_ELEM_FIRE, "+3% burn damage"},
    {CH_SHOCK_KILLS, "Live Wire", "Kills while shocked", {25, 150, 500, 2000}, RW_MYTHIC_ELEMENT, -1, LOOT_ELEM_SHOCK, "+3% shock damage"},
    {CH_CORROSIVE_KILLS, "Meltdown", "Kills while melting", {25, 150, 500, 2000}, RW_MYTHIC_ELEMENT, -1, LOOT_ELEM_CORROSIVE, "+3% melt damage"},
    {CH_ELITE_KILLS, "Alpha Hunter", "Alpha kills", {5, 25, 100, 300}, RW_MYTHIC_SHIELD, -1, LOOT_ELEM_NONE, "+3% shield capacity"},
    {CH_BOSS_KILLS, "Giant Slayer", "Boss kills", {1, 5, 15, 40}, RW_SECOND_UNIQUE, -1, LOOT_ELEM_NONE, "+3% damage to bosses"},
    {CH_PISTOL_KILLS, "Sidearm", "Pistol kills", {20, 100, 300, 1000}, RW_MYTHIC_GUN, LOOT_ARCH_PISTOL, LOOT_ELEM_NONE, "+3% pistol damage"},
    {CH_SHOTGUN_KILLS, "Close Encounters", "Shotgun kills", {30, 150, 500, 1500}, RW_MYTHIC_GUN, LOOT_ARCH_SHOTGUN, LOOT_ELEM_NONE, "+3% shotgun damage"},
    {CH_SKILL_KILLS, "Show Off", "Action skill kills", {10, 50, 200, 750}, RW_MYTHIC_GRENADE, -1, LOOT_ELEM_NONE, "+3% action skill damage"},
    {CH_CASH, "Payday", "Cash picked up", {1000, 10000, 50000, 250000}, RW_MYTHIC_GUN, -1, LOOT_ELEM_NONE, "+3% cash pickups"},
    {CH_GRENADE_KILLS, "Grenadier", "Grenade kills", {25, 100, 300, 1000}, RW_MYTHIC_GRENADE, -1, LOOT_ELEM_NONE, "+3% grenade damage"},
    {CH_OMEGA_KILLS, "Omega Hunter", "Omega kills (NG+)", {2, 8, 20, 50}, RW_MYTHIC_GUN, -1, LOOT_ELEM_NONE, "+3% XP"},
};

constexpr float BADGE_BONUS = 0.03f;
constexpr float BADGE_CRIT_BONUS = 0.05f;

// Tier 4 exists in NG+ only; earlier tiers keep their goals.
static int32_t Challenge_MaxTier()
{
	return game.loot_ngplus > 0 ? LOOT_CHALLENGE_TIERS : LOOT_CHALLENGE_TIERS - 1;
}

static bool Challenge_HasBadge(const edict_t *ent, challenge_kind_t kind)
{
	if (!ent || !ent->client)
		return false;
	for (int32_t i = 0; i < LOOT_NUM_CHALLENGES; i++)
		if (challenges[i].kind == kind)
			return ent->client->pers.loot_challenge_tier[i] >= LOOT_CHALLENGE_TIERS;
	return false;
}

static void Challenge_GiveReward(edict_t *ent, const challenge_def_t &def)
{
	const int32_t level = std::max(1, ent->client->pers.loot_level);
	const uint32_t seed = (uint32_t)mt_rand();

	switch (def.reward)
	{
		case RW_MYTHIC_GUN:
		case RW_MYTHIC_ELEMENT:
		{
			const loot_archetype_t arch = (loot_archetype_t)(def.arch >= 0 ? def.arch : irandom(LOOT_ARCH_COUNT));
			loot_weapon_t gun = Loot_Roll(arch, LOOT_RARITY_MYTHIC, seed);
			if (def.element)
				gun.element = def.element; // mythics are always elemental, so the damage trade is already in
			Loot_SetLevel(gun, level);
			Loot_SpawnDrop(ent, gun, true);
			break;
		}
		case RW_MYTHIC_SHIELD:
			Loot_SpawnShieldDrop(ent, Loot_RollShield(LOOT_RARITY_MYTHIC, level, seed), true);
			break;
		case RW_MYTHIC_GRENADE:
			Grenade_SpawnDrop(ent, Grenade_Roll(LOOT_RARITY_MYTHIC, level, seed), true);
			break;
		case RW_SECOND_UNIQUE:
		{
			loot_weapon_t gun = Loot_Roll((loot_archetype_t)irandom(LOOT_ARCH_COUNT), LOOT_RARITY_LEGENDARY, seed);
			Unique_Make(gun, 1);
			Loot_SetLevel(gun, level);
			Loot_SpawnDrop(ent, gun, true);
			break;
		}
	}
}

// Tier reward: 10% of a level's XP per tier step, plus cash.
static int32_t Challenge_RewardXP(edict_t *ent, int32_t tier)
{
	return Loot_XPToNext(std::max(1, ent->client->pers.loot_level)) * tier / 10;
}

static int32_t Challenge_RewardCash(edict_t *ent, int32_t tier)
{
	return tier * (25 + 10 * std::max(1, ent->client->pers.loot_level));
}

static void Challenge_Add(edict_t *ent, challenge_kind_t kind, int32_t amount)
{
	client_persistant_t &pers = ent->client->pers;

	for (int32_t i = 0; i < LOOT_NUM_CHALLENGES; i++)
	{
		const challenge_def_t &def = challenges[i];
		if (def.kind != kind)
			continue;

		pers.loot_challenge_progress[i] += amount;

		// pay out every tier crossed (cash pickups can cross more than one)
		while (pers.loot_challenge_tier[i] < Challenge_MaxTier() && pers.loot_challenge_progress[i] >= def.tiers[pers.loot_challenge_tier[i]])
		{
			const int32_t tier = ++pers.loot_challenge_tier[i];
			const int32_t xp = Challenge_RewardXP(ent, tier);
			const int32_t cash = Challenge_RewardCash(ent, tier);
			pers.loot_money += cash;
			gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/secret.wav"), 1, ATTN_NORM, 0);
			if (tier == LOOT_CHALLENGE_TIERS)
			{
				Challenge_GiveReward(ent, def);
				gi.Center_Print(ent, G_Fmt("CHALLENGE MASTERED\n{} tier {}\n+{} XP  +${}  + Mythic reward\nBadge: {}", def.name, tier, xp, cash, def.badge).data());
				Loot_LogEvent(ent, G_Fmt("CHALLENGE {} tier {} MASTERED, badge: {}", def.name, tier, def.badge).data());
			}
			else
			{
				gi.Center_Print(ent, G_Fmt("CHALLENGE COMPLETE\n{} tier {}\n+{} XP  +${}", def.name, tier, xp, cash).data());
				Loot_LogEvent(ent, G_Fmt("CHALLENGE {} tier {}", def.name, tier).data());
			}
			Loot_GiveXP(ent, xp); // after the print, so a level-up message wins
		}
	}
}

void Challenge_OnKill(edict_t *monster, edict_t *attacker)
{
	if (deathmatch->integer || !attacker || !attacker->client || !monster)
		return;

	Challenge_Add(attacker, CH_KILLS, 1);
	if (monster->loot_hit_crit)
		Challenge_Add(attacker, CH_HEADSHOT_KILLS, 1);
	if (monster->loot_hit_elements & (1 << LOOT_ELEM_FIRE))
		Challenge_Add(attacker, CH_FIRE_KILLS, 1);
	if (monster->loot_hit_elements & (1 << LOOT_ELEM_SHOCK))
		Challenge_Add(attacker, CH_SHOCK_KILLS, 1);
	if (monster->loot_hit_elements & (1 << LOOT_ELEM_CORROSIVE))
		Challenge_Add(attacker, CH_CORROSIVE_KILLS, 1);
	if (monster->loot_elite)
		Challenge_Add(attacker, CH_ELITE_KILLS, 1);
	if (monster->loot_omega)
		Challenge_Add(attacker, CH_OMEGA_KILLS, 1);
	if (Loot_IsBossTier(monster))
		Challenge_Add(attacker, CH_BOSS_KILLS, 1);

	const mod_id_t mod = monster->monsterinfo.damage_mod.id;
	if (mod == MOD_LOOT_GRENADE)
		Challenge_Add(attacker, CH_GRENADE_KILLS, 1);
	// Overcharge deals no damage itself, so Elementalist kills while overcharged count
	if (mod == MOD_LOOT_TURRET || mod == MOD_LOOT_SLAM || Skill_Overcharged(attacker))
		Challenge_Add(attacker, CH_SKILL_KILLS, 1);

	const int32_t arch = monster->loot_hit_arch - 1;
	if (arch == LOOT_ARCH_PISTOL)
		Challenge_Add(attacker, CH_PISTOL_KILLS, 1);
	else if (arch == LOOT_ARCH_SHOTGUN || arch == LOOT_ARCH_SCATTERGUN)
		Challenge_Add(attacker, CH_SHOTGUN_KILLS, 1);
}

void Challenge_OnCash(edict_t *player, int32_t amount)
{
	if (deathmatch->integer || !player || !player->client)
		return;
	Challenge_Add(player, CH_CASH, amount);
}

//==========================================================================
// Badges (tier 4)
//==========================================================================

// Every damage source a monster takes from a player: gun, status, grenade, skill.
float Challenge_BadgeDamageMult(edict_t *attacker, edict_t *targ, edict_t *inflictor, const mod_t &mod)
{
	if (deathmatch->integer || !attacker || !attacker->client)
		return 1.0f;

	float mult = 1.0f;
	auto badge = [&](challenge_kind_t kind)
	{
		if (Challenge_HasBadge(attacker, kind))
			mult += BADGE_BONUS;
	};

	if (Loot_IsBossTier(targ))
		badge(CH_BOSS_KILLS);

	switch (mod.id)
	{
		case MOD_LOOT_BURN:
			badge(CH_FIRE_KILLS);
			break;
		case MOD_LOOT_SHOCK:
			badge(CH_SHOCK_KILLS);
			break;
		case MOD_LOOT_MELT:
			badge(CH_CORROSIVE_KILLS);
			break;
		case MOD_LOOT_GRENADE:
			badge(CH_GRENADE_KILLS);
			break;
		case MOD_LOOT_TURRET:
		case MOD_LOOT_SLAM:
			badge(CH_SKILL_KILLS);
			break;
		default:
		{
			// gun damage: projectiles use the gun that fired them
			const loot_weapon_t *gun = (inflictor && inflictor != attacker && inflictor->loot_shot_gun.valid) ? &inflictor->loot_shot_gun : Loot_Active(attacker);
			if (!gun)
				break;
			badge(CH_KILLS);
			if (gun->archetype == LOOT_ARCH_PISTOL)
				badge(CH_PISTOL_KILLS);
			else if (gun->archetype == LOOT_ARCH_SHOTGUN || gun->archetype == LOOT_ARCH_SCATTERGUN)
				badge(CH_SHOTGUN_KILLS);
			break;
		}
	}
	return mult;
}

float Challenge_BadgeCritBonus(edict_t *ent)
{
	return Challenge_HasBadge(ent, CH_HEADSHOT_KILLS) ? BADGE_CRIT_BONUS : 0.f;
}

float Challenge_BadgeShieldMult(edict_t *ent)
{
	return Challenge_HasBadge(ent, CH_ELITE_KILLS) ? 1.f + BADGE_BONUS : 1.f;
}

float Challenge_BadgeCashMult(edict_t *ent)
{
	return Challenge_HasBadge(ent, CH_CASH) ? 1.f + BADGE_BONUS : 1.f;
}

float Challenge_BadgeXPMult(edict_t *ent)
{
	return Challenge_HasBadge(ent, CH_OMEGA_KILLS) ? 1.f + BADGE_BONUS : 1.f;
}

static void Challenge_SendScreen(edict_t *ent)
{
	const client_persistant_t &pers = ent->client->pers;
	// dim backdrop first so text draws on top; left edge; columns: name, goal, progress, tier
	std::string s = "loot_dim xl 16 loot_tabs \"140 300 380\" ";
	int32_t y = 24;

	s += G_Fmt("yv {} string2 \"CHALLENGES\" ", y).data();
	y += 16;

	for (int32_t i = 0; i < LOOT_NUM_CHALLENGES; i++)
	{
		const challenge_def_t &def = challenges[i];
		const int32_t tier = pers.loot_challenge_tier[i];
		const int32_t progress = pers.loot_challenge_progress[i];
		std::string line;
		if (tier >= LOOT_CHALLENGE_TIERS)
			line = G_Fmt("{}|{}|BADGE|{}", def.name, def.goal, def.badge).data();
		else if (tier >= Challenge_MaxTier())
			line = G_Fmt("{}|{}|DONE", def.name, def.goal).data();
		else
			line = G_Fmt("{}|{}|{}/{}|T{}", def.name, def.goal, progress, def.tiers[tier], tier + 1).data();
		// green once at least one tier is done
		s += G_Fmt("yv {} loot_str \"{}{}\" ", y, tier > 0 ? "^a" : "", line).data();
		y += 10;
	}

	y += 6;
	s += G_Fmt("yv {} string2 \"Each tier pays XP + cash   [J] close\" ", y).data();
	if (game.loot_ngplus > 0)
	{
		y += 10;
		s += G_Fmt("yv {} string \"Tier 4 (NG+): Mythic reward + permanent badge\" ", y).data();
	}
	if (s.size() > 1000) // engine layout limit is 1024
		gi.Com_PrintFmt("challenges: layout {} chars, near limit\n", s.size());
	Loot_SendLayout(ent, s);
}

bool Challenge_Layout(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (!cl->loot_showchallenges)
		return false;

	if (level.time >= cl->loot_challenges_time || cl->loot_challenges_time > level.time + 1_sec)
	{
		Challenge_SendScreen(ent);
		cl->loot_challenges_time = level.time + 500_ms;
	}
	return true;
}

bool Challenge_ClientCommand(edict_t *ent, const char *cmd)
{
	if (Q_strcasecmp(cmd, "challenges"))
		return false;

	gclient_t *cl = ent->client;
	cl->loot_showchallenges = !cl->loot_showchallenges;
	cl->loot_showinv = false;
	cl->loot_showskills = false;
	cl->loot_challenges_time = 0_ms;
	return true;
}
