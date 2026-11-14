// q2rpg: weapon proficiency. Kills with a gun type level up that type (0-15). Each level gives
// +1% accuracy, +1% reload speed and +1% crit damage with that type. Saved with the character,
// kept through NG+. Shown on the gun details page (I, I) and by the loot_prof command.

#include "../g_local.h"

constexpr int32_t PROF_MAX_LEVEL = 15;
constexpr float PROF_FIRST_LEVEL_KILLS = 20.f; // kills for level 1
constexpr float PROF_KILL_GROWTH = 1.25f;      // each level needs this much more than the last
constexpr float PROF_BONUS_PER_LEVEL = 0.01f;

// total kills needed to reach this level
int32_t Prof_KillsForLevel(int32_t level)
{
	float total = 0.f, step = PROF_FIRST_LEVEL_KILLS;
	for (int32_t i = 0; i < level; i++, step *= PROF_KILL_GROWTH)
		total += roundf(step);
	return (int32_t)total;
}

int32_t Prof_Level(const edict_t *ent, int32_t archetype)
{
	if (!ent || !ent->client || deathmatch->integer || archetype < 0 || archetype >= LOOT_ARCH_COUNT)
		return 0;
	const int32_t kills = ent->client->pers.loot_prof_kills[archetype];
	int32_t level = 0;
	while (level < PROF_MAX_LEVEL && kills >= Prof_KillsForLevel(level + 1))
		level++;
	return level;
}

float Prof_Mult(const edict_t *ent, int32_t archetype)
{
	return 1.0f + PROF_BONUS_PER_LEVEL * Prof_Level(ent, archetype);
}

void Prof_OnKill(edict_t *attacker, int32_t archetype)
{
	if (deathmatch->integer || !attacker || !attacker->client || archetype < 0 || archetype >= LOOT_ARCH_COUNT)
		return;
	const int32_t old_level = Prof_Level(attacker, archetype);
	attacker->client->pers.loot_prof_kills[archetype]++;
	const int32_t new_level = Prof_Level(attacker, archetype);
	if (new_level == old_level)
		return;

	const char *type_name = Loot_ArchName((loot_archetype_t)archetype);
	gi.Client_Print(attacker, PRINT_HIGH,
	                G_Fmt("{} proficiency {}: +{}% accuracy, reload speed and crit damage\n", type_name, new_level, new_level).data());
	gi.sound(attacker, CHAN_AUTO, gi.soundindex("misc/pc_up.wav"), 1, ATTN_NORM, 0);
	Loot_LogEvent(attacker, G_Fmt("PROFICIENCY {} {}", type_name, new_level).data());
}

// "Proficiency|7/15|Kills|412/530" for the gun details page
std::string Prof_DetailsRow(const edict_t *ent, int32_t archetype)
{
	const int32_t level = Prof_Level(ent, archetype);
	if (level >= PROF_MAX_LEVEL)
		return G_Fmt("Proficiency|{}/{}|Bonus|+{}%|Kills|max", level, PROF_MAX_LEVEL, level).data();
	return G_Fmt("Proficiency|{}/{}|Bonus|+{}%|Kills|{}/{}", level, PROF_MAX_LEVEL, level, ent->client->pers.loot_prof_kills[archetype],
	             Prof_KillsForLevel(level + 1))
	    .data();
}

// loot_prof: every gun type's proficiency
void Prof_CmdList(edict_t *ent)
{
	for (int32_t arch = 0; arch < LOOT_ARCH_COUNT; arch++)
	{
		const int32_t level = Prof_Level(ent, arch);
		gi.Client_Print(ent, PRINT_HIGH,
		                G_Fmt("{:<16} {:>2}/{}  kills {}{}\n", Loot_ArchName((loot_archetype_t)arch), level, PROF_MAX_LEVEL, ent->client->pers.loot_prof_kills[arch],
		                      level < PROF_MAX_LEVEL ? G_Fmt(" (next at {})", Prof_KillsForLevel(level + 1)).data() : "")
		                    .data());
	}
}
