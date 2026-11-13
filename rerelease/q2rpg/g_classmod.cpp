// q2rpg: class mods. A fourth gear piece next to guns, shields and grenade mods: one
// equipped, it adds bonus ranks to up to 3 skills of one tree (Skill_Rank adds them).
// Bonus ranks can go past a skill's max rank; 1-rank capstones are never boosted.
// A mod for another tree does nothing until you respec into that tree.

#include "../g_local.h"

//==========================================================================
// Tables
//==========================================================================

// bonus ranks per rarity, best skill first (0 = no skill in that line)
static const uint8_t classmod_bonus_table[LOOT_RARITY_COUNT][LOOT_CLASSMOD_SKILLS] = {
    {1, 0, 0}, // Common
    {1, 1, 0}, // Uncommon
    {2, 1, 0}, // Rare
    {2, 1, 1}, // Epic
    {2, 2, 1}, // Legendary
    {3, 2, 2}, // Mythic
};

// titles per tree (loot_tree_t order)
static const char *const classmod_titles[LOOT_TREE_COUNT][4] = {
    {"Drifter", "Drifter", "Drifter", "Drifter"},         // none (unused)
    {"Rifleman", "Sharpshooter", "Gunslinger", "Warden"}, // Gunner
    {"Bruiser", "Bulwark", "Ironclad", "Titan"},          // Juggernaut
    {"Conduit", "Alchemist", "Stormcaller", "Pyre"},      // Elementalist
};

// drops roll for the owner's tree this often (else a random tree, worth selling)
constexpr float CLASSMOD_OWN_TREE_CHANCE = 0.7f;

//==========================================================================
// Rolling
//==========================================================================

loot_classmod_t ClassMod_Roll(loot_rarity_t rarity, int32_t level, uint32_t seed, int32_t tree)
{
	std::mt19937 rng(seed);

	loot_classmod_t mod{};
	mod.valid = true;
	mod.seed = seed;
	mod.level = std::clamp(level, 1, Loot_LevelCap() + 5);
	mod.rarity = rarity < LOOT_RARITY_COUNT ? rarity : LOOT_RARITY_COMMON;
	mod.tree = (tree > LOOT_TREE_NONE && tree < LOOT_TREE_COUNT) ? tree : 1 + (int32_t)(rng() % (LOOT_TREE_COUNT - 1));
	mod.skills.fill(-1);
	mod.bonus.fill(0);

	// pick distinct boostable skills; tier 4 only once NG+ has opened it
	std::vector<int32_t> pool;
	for (int32_t skill = 0; skill < LOOT_NUM_SKILLS; skill++)
	{
		const bool tier4 = skill >= 9;
		if (Skill_Boostable(mod.tree, skill) && (!tier4 || game.loot_ngplus > 0))
			pool.push_back(skill);
	}

	for (int32_t line = 0; line < LOOT_CLASSMOD_SKILLS && !pool.empty(); line++)
	{
		const uint8_t bonus = classmod_bonus_table[mod.rarity][line];
		if (!bonus)
			break;
		const size_t pick = rng() % pool.size();
		mod.skills[line] = (int8_t)pool[pick];
		mod.bonus[line] = bonus;
		pool.erase(pool.begin() + pick);
	}

	// name: title from the tree; Epic+ get a rank word
	const char *title = classmod_titles[mod.tree][rng() % 4];
	if (mod.rarity >= LOOT_RARITY_MYTHIC)
		G_FmtTo(mod.name, "Mythic {}", title);
	else if (mod.rarity == LOOT_RARITY_LEGENDARY)
		G_FmtTo(mod.name, "Legendary {}", title);
	else if (mod.rarity == LOOT_RARITY_EPIC)
		G_FmtTo(mod.name, "Elite {} Mod", title);
	else
		G_FmtTo(mod.name, "{} Mod", title);

	return mod;
}

int32_t ClassMod_DropTree(edict_t *owner)
{
	// SP / shared drops: the first player's tree
	if (!owner || !owner->client)
		owner = &g_edicts[1];
	if (!owner->inuse || !owner->client)
		return LOOT_TREE_NONE;
	const int32_t tree = owner->client->pers.loot_tree;
	if (tree > LOOT_TREE_NONE && tree < LOOT_TREE_COUNT && frandom() < CLASSMOD_OWN_TREE_CHANCE)
		return tree;
	return LOOT_TREE_NONE; // random
}

//==========================================================================
// Bonus lookup (Skill_Rank)
//==========================================================================

int32_t ClassMod_Bonus(edict_t *ent, int32_t tree, int32_t skill)
{
	if (!ent || !ent->client)
		return 0;
	const loot_classmod_t &mod = ent->client->pers.loot_classmod;
	if (!mod.valid || mod.tree != tree)
		return 0;
	int32_t bonus = 0;
	for (int32_t line = 0; line < LOOT_CLASSMOD_SKILLS; line++)
		if (mod.skills[line] == skill)
			bonus += mod.bonus[line];
	return bonus;
}

//==========================================================================
// Cards, compare lines, giving
//==========================================================================

std::string ClassMod_BriefLine(const loot_classmod_t &mod)
{
	std::string line;
	for (int32_t i = 0; i < LOOT_CLASSMOD_SKILLS; i++)
	{
		if (mod.skills[i] < 0)
			continue;
		if (!line.empty())
			line += " ";
		line += G_Fmt("+{} {}", mod.bonus[i], Skill_Name(mod.tree, mod.skills[i])).data();
	}
	return line;
}

void ClassMod_SendCard(edict_t *ent, const loot_classmod_t &mod)
{
	const loot_classmod_t &other = ent->client->pers.loot_classmod;
	const bool wrong_tree = ent->client->pers.loot_tree && ent->client->pers.loot_tree != mod.tree;

	std::string s;
	int32_t y = 64;
	s += G_Fmt("xv 24 yv {} string2 \"{}\" ", y, mod.name).data();
	y += 10;
	s += G_Fmt("xv 24 yv {} string \"Lv {} {} {} Class Mod\" ", y, mod.level, Loot_RarityName(mod.rarity), Skill_TreeName(mod.tree)).data();
	y += 14;

	for (int32_t i = 0; i < LOOT_CLASSMOD_SKILLS; i++)
	{
		if (mod.skills[i] < 0)
			continue;
		// green when it beats what your current mod gives that skill
		const int32_t current = other.valid && other.tree == mod.tree ? ClassMod_Bonus(ent, mod.tree, mod.skills[i]) : 0;
		const char *color = mod.bonus[i] > current ? "^g" : "";
		s += G_Fmt("xv 24 yv {} loot_str \"{}+{} {}^w\" ", y, color, mod.bonus[i], Skill_Name(mod.tree, mod.skills[i])).data();
		y += 10;
	}
	if (wrong_tree)
	{
		s += G_Fmt("xv 24 yv {} loot_str \"^rNot your tree: no effect\" ", y).data();
		y += 10;
	}

	y += 4;
	if (other.valid)
	{
		s += G_Fmt("xv 24 yv {} string \"vs {}\" ", y, other.name).data();
		y += 10;
	}
	s += G_Fmt("xv 24 yv {} string2 \"[E] take  [X] sell\" ", y).data();
	Loot_SendLayout(ent, s);
}

void ClassMod_Give(edict_t *ent, const loot_classmod_t &mod)
{
	gclient_t *cl = ent->client;
	if (cl->pers.loot_classmod.valid)
		ClassMod_SpawnDrop(ent, cl->pers.loot_classmod, true);

	cl->pers.loot_classmod = mod;
	cl->loot_skills_time = 0_ms; // skills screen redraws with the new bonus
	gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/am_pkup.wav"), 1, ATTN_NORM, 0);
	gi.Center_Print(ent, G_Fmt("{} ({} {} Class Mod)\n{}", mod.name, Loot_RarityName(mod.rarity), Skill_TreeName(mod.tree), ClassMod_BriefLine(mod)).data());
}
