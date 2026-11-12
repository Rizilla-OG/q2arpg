// q2rpg: money and vendors.
// Monsters drop cash. Each map gets one vendor near the player start with
// stock rolled at map spawn (saved in level_locals_t). Walking up to it opens
// the shop; number keys buy, X sells.

#include "../g_local.h"

//==========================================================================
// Prices
//==========================================================================

static const int32_t rarity_value[LOOT_RARITY_COUNT] = {1, 2, 4, 8, 16, 32};

// Level part of every price. The squared term keeps prices ahead of cash drops,
// which grow with monster level and kill count.
static float Vendor_LevelBase(int32_t item_level)
{
	const float level_f = (float)std::max(1, item_level);
	return 25.f + 10.f * level_f + 0.6f * level_f * level_f;
}

static int32_t Vendor_GunValue(const loot_weapon_t &g)
{
	float v = Vendor_LevelBase(Loot_GunLevel(g)) * rarity_value[g.rarity < LOOT_RARITY_COUNT ? g.rarity : 0];
	if (g.element)
		v *= 1.25f;
	return std::max(1, (int32_t)v);
}

static int32_t Vendor_ShieldValue(const loot_shield_t &sh)
{
	float v = Vendor_LevelBase(sh.level) * rarity_value[sh.rarity < LOOT_RARITY_COUNT ? sh.rarity : 0] * 1.2f;
	if (sh.special)
		v *= 1.25f;
	return std::max(1, (int32_t)v);
}

static int32_t Vendor_GrenadeValue(const loot_grenade_t &grenade)
{
	float v = Vendor_LevelBase(grenade.level) * rarity_value[grenade.rarity < LOOT_RARITY_COUNT ? grenade.rarity : 0] * 1.1f;
	if (grenade.element)
		v *= 1.25f;
	return std::max(1, (int32_t)v);
}

static int32_t Vendor_SellPrice(int32_t value)
{
	return std::max(1, (int32_t)(value * 0.05f));
}

static int32_t Vendor_PlayerLevel(edict_t *ent)
{
	return std::max(1, ent->client->pers.loot_level);
}

static int32_t Vendor_AmmoPrice(edict_t *ent)
{
	const int32_t player_level = Vendor_PlayerLevel(ent);
	return 50 + 8 * player_level + player_level * player_level / 2;
}

// New stock: each reroll on a map costs one more step of (30 + 10 x level).
static int32_t Vendor_RerollPrice(edict_t *ent)
{
	return (level.loot_vendor_rerolls + 1) * (30 + 10 * Vendor_PlayerLevel(ent));
}

static int32_t Vendor_RespecPrice(edict_t *ent)
{
	return 50 * Vendor_PlayerLevel(ent);
}

// Opens of the shop on this map so far (resets when the player is on a different map).
static int32_t &Vendor_OpenCount(edict_t *ent)
{
	client_persistant_t &pers = ent->client->pers;
	if (strcmp(pers.loot_shop_map, level.mapname))
	{
		Q_strlcpy(pers.loot_shop_map, level.mapname, sizeof(pers.loot_shop_map));
		pers.loot_shop_opens = 0;
	}
	return pers.loot_shop_opens;
}

// First open per map is free; each later one costs (opens so far) x (10 + 2 x level).
static int32_t Vendor_OpenPrice(edict_t *ent)
{
	return Vendor_OpenCount(ent) * (10 + 2 * Vendor_PlayerLevel(ent));
}

//==========================================================================
// Upgrades: permanent boosts, the late-game cash sink
//==========================================================================

struct vendor_upgrade_t
{
	const char *label;
	int32_t max_rank;
};

static const vendor_upgrade_t vendor_upgrades[LOOT_UPGRADE_COUNT] = {
    {"Ammo capacity +20%", 5},
    {"Max health +10", 10},
    {"Shield capacity +10%", 5},
    {"Gun damage +3%", 5},
    {"Crit damage +5%", 5},
    {"Reload speed +5%", 5},
    {"Grenade damage +5%", 5},
    {"Second Chance +4%", 5},
};

// Shop pages; 9 cycles stock -> upgrades -> workshop -> stock.
enum vendor_page_t
{
	VENDOR_PAGE_STOCK,
	VENDOR_PAGE_UPGRADES,
	VENDOR_PAGE_WORKSHOP,
	VENDOR_PAGE_COUNT
};

// Workshop: raise the active gun to the player's level. Costs 1.5x the gain in
// gun value, so it never runs out and scales with rarity: the endless cash sink.
constexpr float RELEVEL_PRICE_MULT = 1.5f;

static int32_t Vendor_RelevelTarget(edict_t *ent)
{
	return std::min(Vendor_PlayerLevel(ent), Loot_LevelCap() + 5);
}

// (rank + 1)^2 x (100 + 20 x level): cheap first rank, steep last ones.
static int32_t Vendor_UpgradePrice(edict_t *ent, int32_t upgrade)
{
	const int32_t next = ent->client->pers.loot_upgrades[upgrade] + 1;
	return next * next * (100 + 20 * Vendor_PlayerLevel(ent));
}

// 0 when the gun is already at (or above) the target level
static int32_t Vendor_RelevelPrice(edict_t *ent, const loot_weapon_t &gun)
{
	const int32_t target = Vendor_RelevelTarget(ent);
	if (Loot_GunLevel(gun) >= target)
		return 0;
	loot_weapon_t releveled = gun;
	Loot_RelevelGun(releveled, target);
	return std::max(1, (int32_t)((Vendor_GunValue(releveled) - Vendor_GunValue(gun)) * RELEVEL_PRICE_MULT));
}

float Loot_ShieldCapMult(edict_t *ent)
{
	return Skill_ShieldCapMult(ent) * (1.0f + 0.1f * ent->client->pers.loot_upgrades[LOOT_UPGRADE_SHIELD]) * Challenge_BadgeShieldMult(ent);
}

static void Vendor_ApplyUpgrade(edict_t *ent, int32_t upgrade)
{
	client_persistant_t &pers = ent->client->pers;
	const int32_t old_rank = pers.loot_upgrades[upgrade]++;

	if (upgrade == LOOT_UPGRADE_AMMO)
	{
		// max_ammo is base x (1 + 0.2 x old rank); add another 20% of base
		for (auto &max : pers.max_ammo)
			max = (int16_t)(max + std::max(1, (int32_t)(max * 0.2f / (1.0f + 0.2f * old_rank))));
	}
	else if (upgrade == LOOT_UPGRADE_HEALTH)
	{
		pers.max_health += 10;
		ent->max_health += 10;
		ent->health += 10;
	}
	// shield: Loot_ShieldCapMult reads the rank
}

//==========================================================================
// Cash drops
//==========================================================================

static void Vendor_TakeCash(edict_t *self, edict_t *other)
{
	const int32_t amount = (int32_t)(self->count * Challenge_BadgeCashMult(other)); // Payday badge
	other->client->pers.loot_money += amount;
	level.loot_stats.cash_taken += amount;
	Challenge_OnCash(other, amount);
	gi.sound(other, CHAN_ITEM, gi.soundindex("misc/ar2_pkup.wav"), 1, ATTN_NORM, 0);
	G_FreeEdict(self);
}

TOUCH(loot_cash_touch)(edict_t *self, edict_t *other, const trace_t &tr, bool other_touching_self)->void
{
	if (!other->client || other->health <= 0 || Loot_HiddenFrom(self, other))
		return;
	Vendor_TakeCash(self, other);
}

// Cash magnet: piles fly to a player in sight within CASH_MAGNET_RANGE. A pile still
// lying around after CASH_STALE_TIME (pit, lava, behind a ledge) flies to anyone within
// CASH_RECALL_RANGE, through walls.
constexpr float CASH_MAGNET_RANGE = 300.f;
constexpr float CASH_RECALL_RANGE = 1500.f;
constexpr float CASH_FLY_SPEED = 700.f;
constexpr float CASH_GRAB_RANGE = 80.f; // one tick of flight, so it never overshoots
constexpr gtime_t CASH_STALE_TIME = 8_sec;
constexpr gtime_t CASH_LIFETIME = 120_sec;

THINK(loot_cash_think)(edict_t *self)->void
{
	if (level.time >= self->timestamp + CASH_LIFETIME)
	{
		G_FreeEdict(self);
		return;
	}

	const bool stale = level.time >= self->timestamp + CASH_STALE_TIME;
	edict_t *target = nullptr;
	float best_dist = stale ? CASH_RECALL_RANGE : CASH_MAGNET_RANGE;
	for (auto player : active_players())
	{
		if (player->health <= 0 || Loot_HiddenFrom(self, player))
			continue;
		const float dist = (player->s.origin - self->s.origin).length();
		if (dist >= best_dist)
			continue;
		if (!stale && gi.traceline(self->s.origin, player->s.origin, self, MASK_SOLID).fraction < 1.f)
			continue;
		best_dist = dist;
		target = player;
	}

	if (target && best_dist < CASH_GRAB_RANGE)
	{
		Vendor_TakeCash(self, target);
		return;
	}

	if (target)
	{
		self->movetype = MOVETYPE_NOCLIP; // straight line, out of pits and lava
		self->velocity = (target->s.origin - self->s.origin).normalized() * CASH_FLY_SPEED;
	}
	else if (self->movetype == MOVETYPE_NOCLIP)
	{
		// player moved out of range: drop where it is
		self->movetype = MOVETYPE_TOSS;
		self->velocity = {};
	}

	self->nextthink = level.time + 100_ms;
}

static void Vendor_SpawnCash(edict_t *monster, float amount, edict_t *owner);

void Vendor_DropCash(edict_t *monster, edict_t *attacker)
{
	if (deathmatch->integer || !attacker || !attacker->client)
		return;
	if (frandom() >= 0.7f)
		return;

	const float base_health = Loot_MonsterBaseHealth(monster);
	const int32_t mlevel = std::max(1, monster->loot_mlevel);
	float amount = (5.f + base_health * 0.15f) * (1.0f + 0.1f * (mlevel - 1)) * frandom(0.75f, 1.25f);
	if (monster->loot_elite)
		amount *= monster->loot_omega ? 6.f : 3.f;
	if (Loot_IsBossTier(monster))
		amount *= 5.f;
	amount *= NGPlus_CashMult(); // Gold Rush

	// coop: one pile per player, each only visible to its owner
	if (Loot_PerPlayerDrops())
	{
		for (auto player : active_players())
			Vendor_SpawnCash(monster, amount, player);
	}
	else
		Vendor_SpawnCash(monster, amount, nullptr);
}

static void Vendor_SpawnCash(edict_t *monster, float amount, edict_t *owner)
{
	edict_t *cash = G_Spawn();
	cash->classname = "loot_cash";
	cash->count = std::max(1, (int32_t)amount);
	level.loot_stats.cash_dropped += cash->count;
	cash->s.origin = (monster->absmin + monster->absmax) * 0.5f;
	gi.setmodel(cash, "models/items/armor/shard/tris.md2");
	cash->s.effects = EF_ROTATE | EF_COLOR_SHELL;
	cash->s.renderfx = RF_GLOW | RF_SHELL_RED | RF_SHELL_GREEN; // gold
	cash->mins = {-8, -8, -8};
	cash->maxs = {8, 8, 8};
	cash->solid = SOLID_TRIGGER;
	cash->movetype = MOVETYPE_TOSS;
	cash->velocity = {crandom() * 100.f, crandom() * 100.f, 200.f + frandom(100.f)};
	cash->touch = loot_cash_touch;
	cash->think = loot_cash_think;
	cash->timestamp = level.time;
	cash->nextthink = level.time + 500_ms; // let the drop fountain play first
	Loot_InstanceFor(cash, owner);
	gi.linkentity(cash);
}

//==========================================================================
// Vendor entity
//==========================================================================

// Rolled once per map at spawn; saved with the level, so a revisited map keeps its stock.
void Vendor_RollStock()
{
	const int32_t zone = Loot_ZoneLevel();
	const float luck = 1.0f + Loot_ZoneLuck();

	for (auto &g : level.loot_vendor_guns)
	{
		g = Loot_RollRandom(luck);
		Loot_SetLevel(g, zone);
	}

	level.loot_vendor = nullptr; // world vendor entity retired; field kept for old saves
	level.loot_vendor_shield = {};
	if (Loot_ShieldsAvailable())
		level.loot_vendor_shield = Loot_RollShield(Loot_RollRarity(luck), zone, (uint32_t)mt_rand());
	level.loot_vendor_grenade = Grenade_Roll(Loot_RollRarity(luck), zone, (uint32_t)mt_rand());
}

//==========================================================================
// Shop screen
//==========================================================================

static void Vendor_SendUpgrades(edict_t *ent)
{
	gclient_t *cl = ent->client;
	const int32_t money = cl->pers.loot_money;
	std::string s = "loot_dim xl 16 "; // dim backdrop first so text draws on top
	int32_t y = 24;
	auto line = [&](const char *kind, const std::string &text)
	{
		s += G_Fmt("yv {} {} \"{}\" ", y, kind, text).data();
		y += 10;
	};

	line("string2", G_Fmt("UPGRADES        You have ${}", money).data());
	y += 6;
	for (int32_t i = 0; i < LOOT_UPGRADE_COUNT; i++)
	{
		const int32_t rank = cl->pers.loot_upgrades[i];
		const vendor_upgrade_t &up = vendor_upgrades[i];
		if (rank >= up.max_rank)
			line("string", G_Fmt("{}  {}  [{}/{}]  MAX", i + 1, up.label, rank, up.max_rank).data());
		else
		{
			const int32_t price = Vendor_UpgradePrice(ent, i);
			line(price <= money ? "string2" : "string", G_Fmt("{}  {}  [{}/{}]  ${}", i + 1, up.label, rank, up.max_rank, price).data());
		}
	}
	y += 6;
	line("string2", "9  Workshop >");
	y += 6;
	line("string2", "[1-8] buy  [9] next  [V] close");
	Loot_SendLayout(ent, s);
}

static void Vendor_SendWorkshop(edict_t *ent)
{
	gclient_t *cl = ent->client;
	const int32_t money = cl->pers.loot_money;
	std::string s = "loot_dim xl 16 "; // dim backdrop first so text draws on top
	int32_t y = 24;
	auto line = [&](const char *kind, const std::string &text)
	{
		s += G_Fmt("yv {} {} \"{}\" ", y, kind, text).data();
		y += 10;
	};

	line("string2", G_Fmt("WORKSHOP        You have ${}", money).data());
	y += 6;

	// re-level the active gun
	const loot_weapon_t *gun = Loot_Active(ent);
	if (!gun)
		line("string", "1  Level up gun  (no gun held)");
	else
	{
		const int32_t price = Vendor_RelevelPrice(ent, *gun);
		line("string", G_Fmt("   {}", gun->name).data());
		if (!price)
			line("string", G_Fmt("1  Level up gun  L{}  MAX", Loot_GunLevel(*gun)).data());
		else
			line(price <= money ? "string2" : "string", G_Fmt("1  Level up gun  L{} to L{}  ${}", Loot_GunLevel(*gun), Vendor_RelevelTarget(ent), price).data());
	}

	// grenade mod for sale (rerolls with the stock)
	y += 6;
	const loot_grenade_t &grenade = level.loot_vendor_grenade;
	if (grenade.valid)
	{
		const int32_t price = Vendor_GrenadeValue(grenade);
		line(price <= money ? "string2" : "string", G_Fmt("2  {} [{}]  ${}", grenade.name, Loot_RarityName(grenade.rarity), price).data());
		line("loot_str", "   " + Grenade_BriefCompare(grenade, cl->pers.loot_grenade));
	}
	else
		line("string", "2  - sold -");
	line("string", G_Fmt("   Grenades {}  (stock hand grenades are your ammo)", cl->pers.inventory[IT_AMMO_GRENADES]).data());

	y += 6;
	line("string2", "[1-2] buy  [9] back  [V] close");
	Loot_SendLayout(ent, s);
}

static void Vendor_SendShop(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (cl->loot_shop_page == VENDOR_PAGE_UPGRADES)
	{
		Vendor_SendUpgrades(ent);
		return;
	}
	if (cl->loot_shop_page == VENDOR_PAGE_WORKSHOP)
	{
		Vendor_SendWorkshop(ent);
		return;
	}
	const int32_t money = cl->pers.loot_money;
	std::string s;
	int32_t y = 24;

	// x carries over between layout commands, so set it once; keeps the shop under the 1024-char layout limit
	s += "loot_dim xl 16 "; // dim backdrop first so text draws on top
	auto line = [&](const char *kind, const std::string &text)
	{
		s += G_Fmt("yv {} {} \"{}\" ", y, kind, text).data();
		y += 10;
	};

	line("string2", G_Fmt("VENDOR          You have ${}", money).data());
	y += 6;

	for (int32_t i = 0; i < 4; i++)
	{
		const loot_weapon_t &g = level.loot_vendor_guns[i];
		if (!g.valid)
		{
			line("string", G_Fmt("{}  - sold -", i + 1).data());
			y += 10;
			continue;
		}
		const int32_t price = Vendor_GunValue(g);
		line(price <= money ? "string2" : "string", G_Fmt("{}  {}  ${}", i + 1, g.name, price).data());
		line("loot_str", "   " + Loot_GunBriefCompare(g, Loot_Active(ent)));
	}

	const loot_shield_t &sh = level.loot_vendor_shield;
	if (sh.valid)
	{
		const int32_t price = Vendor_ShieldValue(sh);
		line(price <= money ? "string2" : "string", G_Fmt("5  {} [{}]  ${}", sh.name, Loot_RarityName(sh.rarity), price).data());
		line("loot_str", "   " + Loot_ShieldBriefCompare(sh, cl->pers.loot_shield));
	}
	else if (Loot_ShieldsAvailable())
	{
		line("string", "5  - sold -");
		y += 10;
	}

	y += 4;
	line(Vendor_AmmoPrice(ent) <= money ? "string2" : "string", G_Fmt("6  Ammo refill (guns, grenades)  ${}", Vendor_AmmoPrice(ent)).data());
	if (cl->pers.loot_tree)
		line(Vendor_RespecPrice(ent) <= money ? "string2" : "string", G_Fmt("7  Respec skills  ${}", Vendor_RespecPrice(ent)).data());

	line(Vendor_RerollPrice(ent) <= money ? "string2" : "string", G_Fmt("8  Reroll stock  ${}", Vendor_RerollPrice(ent)).data());

	line("string2", "9  Upgrades >");

	y += 6;
	line("string2", G_Fmt("[1-9] buy  [X] sell  [V] close (reopen ${})", Vendor_OpenPrice(ent)).data());

	if (s.size() > 1000) // engine layout limit is 1024; warn before lines get cut
		gi.Com_PrintFmt("vendor: shop layout {} chars, near limit\n", s.size());
	Loot_SendLayout(ent, s);
}

// Shop stays up until the vendor command closes it (or death / map end).
bool Vendor_Layout(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (deathmatch->integer || !cl->loot_shop_open)
		return false;

	if (!cl->loot_shop_open || level.time >= cl->loot_shop_time || cl->loot_shop_time > level.time + 1_sec)
	{
		Vendor_SendShop(ent);
		cl->loot_shop_time = level.time + 500_ms;
	}
	return true;
}

//==========================================================================
// Buying and selling
//==========================================================================

static bool Vendor_Pay(edict_t *ent, int32_t price)
{
	int32_t &money = ent->client->pers.loot_money;
	if (money < price)
	{
		gi.Center_Print(ent, G_Fmt("Need ${}", price).data());
		return false;
	}
	money -= price;
	level.loot_stats.money_spent += price;
	ent->client->loot_shop_time = 0_ms;
	return true;
}

static void Vendor_Buy(edict_t *ent, int32_t n)
{
	gclient_t *cl = ent->client;

	// 9 cycles stock -> upgrades -> workshop
	if (n == 9)
	{
		cl->loot_shop_page = (cl->loot_shop_page + 1) % VENDOR_PAGE_COUNT;
		cl->loot_shop_time = 0_ms;
		return;
	}

	if (cl->loot_shop_page == VENDOR_PAGE_WORKSHOP && n == 1)
	{
		loot_weapon_t *gun = Loot_Active(ent);
		if (!gun)
			return;
		const int32_t price = Vendor_RelevelPrice(ent, *gun);
		if (!price || !Vendor_Pay(ent, price))
			return;
		Loot_RelevelGun(*gun, Vendor_RelevelTarget(ent));
		gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/secret.wav"), 1, ATTN_NORM, 0);
		gi.Center_Print(ent, G_Fmt("{}\nnow level {}", gun->name, gun->level).data());
		return;
	}

	if (cl->loot_shop_page == VENDOR_PAGE_WORKSHOP && n == 2)
	{
		loot_grenade_t &grenade = level.loot_vendor_grenade;
		if (!grenade.valid || !Vendor_Pay(ent, Vendor_GrenadeValue(grenade)))
			return;
		const loot_grenade_t bought = grenade;
		grenade = {};
		Grenade_Give(ent, bought);
		cl->loot_shop_time = 0_ms;
		return;
	}

	if (cl->loot_shop_page == VENDOR_PAGE_WORKSHOP)
		return;

	if (cl->loot_shop_page == VENDOR_PAGE_UPGRADES)
	{
		const int32_t upgrade = n - 1;
		if (upgrade < 0 || upgrade >= LOOT_UPGRADE_COUNT || cl->pers.loot_upgrades[upgrade] >= vendor_upgrades[upgrade].max_rank)
			return;
		if (!Vendor_Pay(ent, Vendor_UpgradePrice(ent, upgrade)))
			return;
		Vendor_ApplyUpgrade(ent, upgrade);
		gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/secret.wav"), 1, ATTN_NORM, 0);
		gi.Center_Print(ent, G_Fmt("{} rank {}", vendor_upgrades[upgrade].label, cl->pers.loot_upgrades[upgrade]).data());
		cl->loot_shop_time = 0_ms;
		return;
	}

	if (n >= 1 && n <= 4)
	{
		loot_weapon_t &g = level.loot_vendor_guns[n - 1];
		if (!g.valid || !Vendor_Pay(ent, Vendor_GunValue(g)))
			return;
		const loot_weapon_t bought = g;
		g = {};
		Loot_GiveGun(ent, bought);
		return;
	}

	if (n == 5)
	{
		loot_shield_t &sh = level.loot_vendor_shield;
		if (!sh.valid || !Vendor_Pay(ent, Vendor_ShieldValue(sh)))
			return;
		const loot_shield_t bought = sh;
		sh = {};
		Loot_GiveShield(ent, bought);
		return;
	}

	if (n == 6)
	{
		if (!Vendor_Pay(ent, Vendor_AmmoPrice(ent)))
			return;
		for (const auto &g : cl->pers.loot_slots)
		{
			if (!g.valid)
				continue;
			gitem_t *weapon = GetItemByIndex(g.base);
			if (weapon->ammo)
				Add_Ammo(ent, GetItemByIndex(weapon->ammo), 9999);
		}
		Add_Ammo(ent, GetItemByIndex(IT_AMMO_GRENADES), 9999); // grenade mods throw these
		gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/am_pkup.wav"), 1, ATTN_NORM, 0);
		gi.Center_Print(ent, "Ammo refilled");
		return;
	}

	if (n == 8)
	{
		const int32_t price = Vendor_RerollPrice(ent);
		if (!Vendor_Pay(ent, price))
			return;
		level.loot_vendor_rerolls++;
		Vendor_RollStock();
		gi.Center_Print(ent, "New stock");
		return;
	}

	if (n == 7 && cl->pers.loot_tree)
	{
		if (!Vendor_Pay(ent, Vendor_RespecPrice(ent)))
			return;
		Skill_Respec(ent);
		return;
	}
}

static int32_t Vendor_CountGuns(gclient_t *cl)
{
	int32_t n = 0;
	for (const auto &g : cl->pers.loot_slots)
		if (g.valid)
			n++;
	return n;
}

// Sells the gun/shield you stand on, else your equipped gun. Press twice to confirm.
static void Vendor_Sell(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (ent->health <= 0)
		return;

	edict_t *floor = Loot_TouchTarget(ent);
	int32_t id;
	int32_t price;
	const char *name;

	if (floor)
	{
		id = (int32_t)(floor - g_edicts);
		if (floor->loot_shield.valid)
		{
			price = Vendor_SellPrice(Vendor_ShieldValue(floor->loot_shield));
			name = floor->loot_shield.name;
		}
		else if (floor->loot_grenade.valid)
		{
			price = Vendor_SellPrice(Vendor_GrenadeValue(floor->loot_grenade));
			name = floor->loot_grenade.name;
		}
		else
		{
			price = Vendor_SellPrice(Vendor_GunValue(floor->loot));
			name = floor->loot.name;
		}
	}
	else
	{
		const int32_t slot = cl->pers.loot_active_slot;
		if (slot < 0 || slot >= LOOT_NUM_SLOTS || !Loot_Active(ent))
			return;
		if (Vendor_CountGuns(cl) <= 1)
		{
			gi.Center_Print(ent, "Can't sell your last gun");
			return;
		}
		id = -(slot + 1);
		price = Vendor_SellPrice(Vendor_GunValue(cl->pers.loot_slots[slot]));
		name = cl->pers.loot_slots[slot].name;
	}

	// first press asks, second press within 2s sells
	if (cl->loot_sell_confirm_id != id || level.time > cl->loot_sell_confirm_time || cl->loot_sell_confirm_time > level.time + 3_sec)
	{
		cl->loot_sell_confirm_id = id;
		cl->loot_sell_confirm_time = level.time + 2_sec;
		gi.Center_Print(ent, G_Fmt("Sell {} for ${}?\nPress X again", name, price).data());
		return;
	}

	cl->loot_sell_confirm_id = 0;
	cl->loot_sell_confirm_time = 0_ms;
	cl->pers.loot_money += price;
	level.loot_stats.money_sold += price;
	cl->loot_shop_time = 0_ms;

	if (floor)
	{
		G_FreeEdict(floor);
		cl->loot_touch_ent = nullptr;
		cl->loot_card_sent = nullptr;
	}
	else
	{
		const int32_t slot = -id - 1;
		cl->pers.loot_slots[slot] = {};
		Loot_SyncInventory(ent);
		for (int32_t i = 0; i < LOOT_NUM_SLOTS; i++)
		{
			if (cl->pers.loot_slots[i].valid)
			{
				cl->pers.loot_active_slot = -1;
				Loot_SelectSlot(ent, i);
				break;
			}
		}
	}

	gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/ar2_pkup.wav"), 1, ATTN_NORM, 0);
	gi.Center_Print(ent, G_Fmt("Sold for ${}", price).data());
}

static bool Vendor_Pay(edict_t *ent, int32_t price);

// "vendor": toggle the shop anywhere.
static void Vendor_Toggle(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (cl->loot_shop_open)
	{
		// q2rpg: shop hidden under challenges: bring it back instead of closing
		if (cl->loot_showchallenges)
		{
			cl->loot_showchallenges = false;
			cl->loot_shop_time = 0_ms;
			return;
		}
		cl->loot_shop_open = false;
		return;
	}
	if (ent->health <= 0 || level.intermissiontime)
		return;

	const int32_t price = Vendor_OpenPrice(ent);
	if (price > 0 && !Vendor_Pay(ent, price))
		return;

	Vendor_OpenCount(ent)++;
	cl->loot_showinv = false;
	cl->loot_showskills = false;
	cl->loot_showchallenges = false;
	cl->loot_shop_open = true;
	cl->loot_shop_page = 0;
	cl->loot_shop_time = 0_ms; // draw now
	if (price > 0)
		gi.Center_Print(ent, G_Fmt("Vendor opened for ${}", price).data());
}

bool Vendor_ClientCommand(edict_t *ent, const char *cmd)
{
	gclient_t *cl = ent->client;

	if (!Q_strcasecmp(cmd, "vendor"))
	{
		Vendor_Toggle(ent);
		return true;
	}

	if (!Q_strcasecmp(cmd, "slot") && cl->loot_shop_open && !cl->loot_showinv && !cl->loot_showskills && !cl->loot_showchallenges)
	{
		Vendor_Buy(ent, atoi(gi.argv(1)));
		return true;
	}
	if (!Q_strcasecmp(cmd, "loot_sell"))
	{
		Vendor_Sell(ent);
		return true;
	}
	if (!Q_strcasecmp(cmd, "loot_money"))
	{
		// dev: loot_money <amount>
		if (game.maxclients > 1 && !sv_cheats->integer)
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_need_cheats");
		else
			cl->pers.loot_money = std::max(0, cl->pers.loot_money + atoi(gi.argv(1)));
		return true;
	}

	return false;
}
