// q2rpg: loot system interface.
// Included at the end of g_local.h; stock code calls these hooks.
#pragma once

// cvars
extern cvar_t *g_loot_droprate;
extern cvar_t *g_loot_rarity_bonus;
extern cvar_t *g_loot_monster_health;
extern cvar_t *g_loot_monster_damage;

void Loot_InitCvars();

// rolling
loot_weapon_t Loot_Roll(loot_archetype_t archetype, loot_rarity_t rarity, uint32_t seed);
loot_weapon_t Loot_RollRandom(float luck);
loot_archetype_t Loot_ArchetypeForItem(item_id_t item);
bool Loot_IsBaseWeapon(item_id_t item);
const char *Loot_RarityName(loot_rarity_t rarity);
const char *Loot_MakerName(loot_maker_t maker);

// player loadout
void Loot_InitClient(gclient_t *client);
loot_weapon_t *Loot_Active(edict_t *ent);
void Loot_SyncInventory(edict_t *ent);
void Loot_OnWeaponChanged(edict_t *ent);
void Loot_SelectSlot(edict_t *ent, int32_t slot);
bool Loot_CycleSameBase(edict_t *ent, const gitem_t *item, int32_t dir, bool wrap); // wheel / weapnext with two same-base slots

// firing hooks (p_weapon.cpp)
int32_t Loot_WeaponAmmo(edict_t *ent);
bool Loot_CanFire(edict_t *ent);
bool Loot_ConsumeAmmo(edict_t *ent, int32_t quantity);
bool Loot_TryReload(edict_t *ent);
bool Loot_IsReloading(edict_t *ent);
float Loot_DamageMult(edict_t *ent);
float Loot_RateMult(edict_t *ent);
float Loot_RecoilMult(edict_t *ent);
float Loot_HandlingMult(edict_t *ent);
const char *Loot_PartName(loot_part_t part);
int32_t Loot_Spread(edict_t *ent, int32_t spread);
float Loot_ProjSpeed(edict_t *ent);
void Loot_RecordDamage(edict_t *targ, edict_t *attacker, int32_t damage, const mod_t &mod);
int32_t Loot_Pellets(edict_t *ent, int32_t count);
void Loot_Think(edict_t *ent);
float Loot_ReloadDip(edict_t *ent);

// damage hooks (g_combat.cpp, g_phys.cpp)
int32_t Loot_ModifyDamage(edict_t *targ, edict_t *inflictor, edict_t *attacker, const vec3_t &point, int32_t damage, damageflags_t dflags, const mod_t &mod);
void Loot_RunStatus(edict_t *ent);
void Loot_RunMonsterFrame(edict_t *ent);
int32_t Loot_ShieldAbsorb(edict_t *targ, edict_t *inflictor, edict_t *attacker, int32_t damage, damageflags_t dflags, const mod_t &mod);
const char *Loot_ElementName(loot_element_t element);
void Loot_ApplyStatus(edict_t *targ, edict_t *attacker, loot_element_t element, int32_t hit);
void Loot_StatusSparks(edict_t *ent, loot_element_t element);

// world items
edict_t *Loot_SpawnDrop(edict_t *source, const loot_weapon_t &gun, bool from_player);
void Loot_MonsterDrop(edict_t *monster);
void Loot_OnItemSpawned(edict_t *ent);
bool Loot_Touch(edict_t *item, edict_t *other);

// levels and xp
int32_t Loot_ZoneLevel();
int32_t Loot_LevelCap(); // player level cap: 50, 60 in NG+

// New Game+ (g_ngplus.cpp)
constexpr int32_t LOOT_NGPLUS_MONSTER_LEVELS = 10; // extra monster levels per NG+ cycle
constexpr float LOOT_NGPLUS_LUCK = 0.5f;           // extra rarity luck per NG+ cycle
void NGPlus_OnGameLoaded();
void NGPlus_OnMapSpawn();
bool NGPlus_TryRestart(const char *changemap); // true if the campaign end became NG+
void NGPlus_Frame(edict_t *ent);
void NGPlus_RemixMonster(edict_t *ent); // ED_CallSpawn, map load only
void NGPlus_RollMapMod();               // start of SpawnEntities, before monsters spawn
float NGPlus_EliteChanceMult();
float NGPlus_CashMult();
float NGPlus_LuckBonus();
float NGPlus_MonsterHealthMult();
float NGPlus_MonsterDamageMult();
float NGPlus_DeathSpawnMult();
float NGPlus_ProcChanceMult();                                            // Elemental Storm
float NGPlus_DropMult();                                                  // Bounty
float NGPlus_MonsterShieldMult();                                         // Fortified
float NGPlus_ShieldDropMult();                                            // Fortified
float NGPlus_XPMult();                                                    // Bloodthirsty
void NGPlus_OnMonsterHitPlayer(edict_t *monster, int32_t damage);         // Bloodthirsty
bool Loot_PromoteToOmega(edict_t *monster);                               // after monster_start (Bounty); false if it won't fit
void NGPlus_RollAlphaTraits(edict_t *monster);                            // Alpha just rolled (NG+ only)
void NGPlus_TraitNames(const edict_t *monster, char (&names)[24]);        // "Vampiric " / "Vamp Swift " / ""
const char *NGPlus_MapModName();                                          // "" when none
void NGPlus_OnMonsterDeath(edict_t *monster, edict_t *killer);            // Volatile
edict_t *NGPlus_KillCredit(edict_t *attacker);                            // Volatile blast -> the player who killed the Alpha
float Loot_MonsterDamageScale(const edict_t *monster);                    // g_loot.cpp: monster -> player damage multiplier
void NGPlus_OnHealthLost(edict_t *targ, edict_t *attacker, int32_t take); // T_Damage after health drops
void Loot_SpawnAdds(edict_t *monster, int32_t count, edict_t *enemy);     // g_loot.cpp death-spawn ring
edict_t *CreateTargetChangeLevel(const char *map);                        // g_main.cpp
bool Loot_PerPlayerDrops();                                               // coop with 2+ players
void Loot_InstanceFor(edict_t *ent, edict_t *owner);                      // only owner sees / takes ent
bool Loot_HiddenFrom(const edict_t *ent, const edict_t *player);
float Loot_ZoneLuck(); // extra rarity luck that grows with zone level
int32_t Loot_XPToNext(int32_t level);
void Loot_ScaleMonster(edict_t *monster);
bool Loot_EliteFire(edict_t *self, const vec3_t &start, const vec3_t &dir, monster_muzzleflash_id_t flashtype, loot_elite_fire_t original);
gtime_t Loot_EliteFrameTime(const edict_t *monster);
void Loot_AwardXP(edict_t *monster, edict_t *attacker);
void Loot_GiveXP(edict_t *ent, int32_t xp);

// skills (g_skills.cpp)
int32_t Skill_Rank(edict_t *ent, loot_tree_t tree, int32_t skill);
int32_t Skill_Unspent(edict_t *ent);
float Skill_SpreadMult(edict_t *ent);
float Skill_RateMult(edict_t *ent);
float Skill_ReloadMult(edict_t *ent);
float Skill_GunDamageMult(edict_t *ent);
float Skill_MagMult(edict_t *ent);
float Skill_CritBonus(edict_t *ent);
bool Skill_SaveAmmo(edict_t *ent);
float Skill_ShieldCapMult(edict_t *ent);
float Skill_ShieldDelayMult(edict_t *ent);
float Skill_DamageTakenMult(edict_t *ent);
float Skill_OutgoingMult(edict_t *ent, edict_t *targ);
float Skill_ProcChanceMult(edict_t *ent);
float Skill_ProcChanceBonus(edict_t *ent);
float Skill_DotMult(edict_t *ent);
float Skill_DurationMult(edict_t *ent);
bool Skill_Attune(edict_t *ent);
bool Skill_Overcharged(edict_t *ent);
void Skill_OnShieldDot(edict_t *ent, float dmg);
float Skill_FusionMult(edict_t *ent, edict_t *targ, loot_element_t element); // NG+ tier 4
loot_element_t Skill_ProcElement(edict_t *ent, loot_element_t element);      // Prism Heart
void Skill_OnShieldBreak(edict_t *ent, float capacity);                      // Retaliation
void Loot_RefillMag(edict_t *ent, float fraction);                           // Scavenger
void Skill_OnMonsterKilled(edict_t *monster, edict_t *attacker);
void Skill_OnHurt(edict_t *ent);
bool Skill_IgnoreFall(edict_t *ent);
void Skill_Frame(edict_t *ent);
bool Skill_Layout(edict_t *ent); // true if the skills screen owns the layout this frame

// challenges (g_challenge.cpp)
void Challenge_OnKill(edict_t *monster, edict_t *attacker);
void Challenge_OnCash(edict_t *player, int32_t amount);
float Challenge_BadgeDamageMult(edict_t *attacker, edict_t *targ, edict_t *inflictor, const mod_t &mod);
float Challenge_BadgeCritBonus(edict_t *ent);
float Challenge_BadgeShieldMult(edict_t *ent);
float Challenge_BadgeCashMult(edict_t *ent);
float Challenge_BadgeXPMult(edict_t *ent);
bool Challenge_Layout(edict_t *ent); // true if the challenges screen owns the layout this frame
bool Challenge_ClientCommand(edict_t *ent, const char *cmd);
bool Skill_ClientCommand(edict_t *ent, const char *cmd);
void Skill_OnLevelUp(edict_t *ent);
void Skill_SetActionStat(edict_t *ent, int32_t playernum);
void Skill_Respec(edict_t *ent);

// shared helpers (g_loot.cpp)
int32_t Loot_GunLevel(const loot_weapon_t &gun);
void Loot_SetLevel(loot_weapon_t &gun, int32_t level);
void Loot_RelevelGun(loot_weapon_t &gun, int32_t level);
bool Loot_MenuPaused();
bool Loot_HasLoadedGun(edict_t *ent, item_id_t base);
void Loot_PausedFrame();
float Loot_UpgradeMult(edict_t *ent, loot_upgrade_t upgrade, float per_rank);
bool Loot_SecondChanceSave(edict_t *targ, edict_t *attacker, const mod_t &mod);
void Loot_SecondChanceKill(edict_t *ent);
void Loot_SecondChanceFrame(edict_t *ent);
bool Loot_SecondChanceActive(const edict_t *ent);
loot_rarity_t Loot_RollRarity(float luck);
loot_shield_t Loot_RollShield(loot_rarity_t rarity, int32_t level, uint32_t seed);
constexpr int32_t LOOT_START_MONEY = 100;
// Pistols are precision sidearms: stock blaster does 15, which left them far behind
// the SMG (~38 vs ~80 dps). Damage per bolt in SP/coop, and extra headshot multiplier.
constexpr int32_t LOOT_PISTOL_DAMAGE = 35;
constexpr float LOOT_PISTOL_CRIT_BONUS = 0.75f;
float Loot_CritMult(const loot_weapon_t &gun); // headshot multiplier incl. the pistol bonus
float Loot_MonsterBaseHealth(const edict_t *monster);
void Loot_DeathSpawn(edict_t *monster); // death hook: maybe spawn 1-3 more of its kind
bool Loot_ShieldsAvailable();
std::string Loot_GunBriefCompare(const loot_weapon_t &g, const loot_weapon_t *other);
std::string Loot_ShieldBriefCompare(const loot_shield_t &sh, const loot_shield_t &other);
void Loot_StatsLogMap();                                   // map end: console summary + loot_stats.csv row
void Loot_LogEvent(edict_t *ent, const std::string &text); // append a line to q2rpg/q2rpg_events.log
edict_t *Loot_SpawnShieldDrop(edict_t *source, const loot_shield_t &shield, bool from_player);
edict_t *Loot_TouchTarget(edict_t *ent);
void Loot_SendLayout(edict_t *ent, const std::string &s);
std::string Loot_GunBrief(const loot_weapon_t &g);
void Loot_GiveGun(edict_t *ent, const loot_weapon_t &gun);
void Loot_GiveShield(edict_t *ent, const loot_shield_t &sh);

// grenade mods (g_grenade.cpp)
loot_grenade_t Grenade_Roll(loot_rarity_t rarity, int32_t level, uint32_t seed);
edict_t *Grenade_SpawnDrop(edict_t *source, const loot_grenade_t &grenade, bool from_player);
void Grenade_Give(edict_t *ent, const loot_grenade_t &grenade);
void Grenade_SendCard(edict_t *ent, const loot_grenade_t &grenade);
std::string Grenade_BriefCompare(const loot_grenade_t &grenade, const loot_grenade_t &other);
std::string Grenade_LoadoutLine(const loot_grenade_t &grenade);
const char *Grenade_TypeName(loot_grenade_type_t type);
void Grenade_Throw(edict_t *ent);
void Grenade_ApplyElement(edict_t *targ, edict_t *attacker, int32_t damage); // Loot_ModifyDamage, during a blast
void Loot_QueueWelcome();                                                    // InitGame: console logo once the map is up
void Loot_ApplyVisuals(edict_t *item);                                       // rarity glow on a floor item
bool Loot_IsLootItem(const edict_t *item);                                   // floor gun, shield or grenade mod

// loot visual effects (g_lootfx.cpp)
void LootFx_RarityBeam(edict_t *item, loot_rarity_t rarity);                  // replaces any beam the item has
void LootFx_Tracer(edict_t *shooter, const vec3_t &start, const vec3_t &end); // elemental hitscan only
void LootFx_TintProjectile(edict_t *shooter, edict_t *projectile);            // elemental projectile shell
void LootFx_ProcLight(edict_t *attacker, loot_element_t element);             // colored flash at the gun
void LootFx_ClientEffects(edict_t *player);                                   // G_SetClientEffects: proc glow shell

// shared with g_grenade.cpp
float Loot_RarityDamage(loot_rarity_t rarity);
renderfx_t Loot_RarityShell(loot_rarity_t rarity);
const char *Loot_MakerShortName(loot_maker_t maker);
std::string Loot_StatLine(const char *label, float value, float other, bool have_other, bool lower_better, const char *fmt_kind);

// unique legendaries (g_unique.cpp)
void Unique_Make(loot_weapon_t &gun, int32_t variant = -1); // -1 random (second only in NG+), 0 first, 1 second
const char *Unique_Desc(int32_t unique);
void Unique_BeginFire(edict_t *ent);
void Unique_EndFire(edict_t *ent);
void Unique_TagSpawn(edict_t *e);
void Unique_OnShot(edict_t *ent);
float Unique_RateMult(edict_t *ent);
void Unique_Frame(edict_t *ent);
bool Unique_Ricochet(edict_t *self, const mod_t &mod);
void Unique_GrenadeExplode(edict_t *grenade);
void Unique_BfgImpact(edict_t *bfg);
float Unique_DamageMult(edict_t *attacker, edict_t *inflictor, const loot_weapon_t &gun, bool &force_crit);
void Unique_Element(edict_t *attacker, const loot_weapon_t &gun, loot_element_t &element, float &chance);
int32_t Unique_OnHit(edict_t *targ, edict_t *attacker, const loot_weapon_t &gun, int32_t damage, const mod_t &mod);
bool Loot_IsBoss(const edict_t *monster);

// money and vendors (g_vendor.cpp)
void Vendor_DropCash(edict_t *monster, edict_t *attacker);
void Vendor_RollStock();
bool Vendor_Layout(edict_t *ent); // true if the shop owns the layout this frame
void Skill_OnGrenadeKill(edict_t *ent);
void Skill_OnGrenadeHit(edict_t *ent, int32_t damage);
void Skill_GrenadeElement(edict_t *ent, loot_element_t &element, float &chance);
float Loot_ShieldCapMult(edict_t *ent); // Capacitor skill x shield upgrade
bool Vendor_ClientCommand(edict_t *ent, const char *cmd);

// HUD
void Loot_SetStats(edict_t *ent);

// commands
bool Loot_ClientCommand(edict_t *ent, const char *cmd);
