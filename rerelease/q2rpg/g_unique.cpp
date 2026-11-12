// q2rpg: unique legendaries, two per archetype (the second drops in NG+ only).
// While a player's weapon fires (Weapon_RunThink), G_Spawn tags every new
// edict with the active gun's unique id; Unique_EndFire then changes the
// tagged projectiles (homing bolts, splitting rockets, ...).

#include "../g_local.h"

#include <vector>

struct unique_def_t
{
	const char *name;
	const char *desc;
};

static const unique_def_t unique_defs[LOOT_UNIQUE_COUNT] = {
    {"", ""},
    {"Echo", "each shot fires a second bolt"},
    {"Ricochet", "pellets bounce off walls"},
    {"Dragonmouth", "always fire, every hit ignites"},
    {"Swarm", "every 5th shot adds a homing bolt"},
    {"Spin Cycle", "fire rate ramps up while held"},
    {"Cluster", "grenades burst into bomblets"},
    {"Hydra", "rockets split into three"},
    {"Seeker", "bolts home in on monsters"},
    {"Lancer", "hits chain-shock nearby monsters"},
    {"Singularity", "impact pulls monsters in"},
    // second uniques (NG+)
    {"Last Word", "last round in the magazine hits x3 and crits"},
    {"Bulwark", "pellet hits restore 1% shield each"},
    {"Prism", "element cycles fire/shock/corrosive each shot"},
    {"Tick Tock", "every 10th hit marks: +25% damage taken for 4s"},
    {"Furnace", "damage ramps +5%/s while held, up to +50%"},
    {"Tar Pit", "grenades leave a corrosive puddle that bogs monsters down"},
    {"Afterburner", "rockets speed up and ignite monsters they pass"},
    {"Pinball", "bolts bounce once off walls"},
    {"Executioner", "non-boss targets left under 15% health die"},
    {"Supernova", "no tendrils; bigger blast with all three elements"},
    // memorial unique: never rolls, only the Lost Station duck gives it
    {"Ava", "Zoomies: kills give a burst of speed, chain kills to keep running"},
};
static_assert(LOOT_UNIQUE_SUPERNOVA == LOOT_ARCH_COUNT * 2, "second uniques follow the first ones in archetype order");

static const char *const unique_arch_names[LOOT_ARCH_COUNT] = {
    "Pistol", "Shotgun", "Scattergun", "SMG", "Minigun", "Launcher", "Rocket Launcher", "Plasma Caster", "Rail Rifle", "Annihilator"};

const char *Unique_Desc(int32_t unique)
{
	return (unique > 0 && unique < LOOT_UNIQUE_COUNT) ? unique_defs[unique].desc : "";
}

// Turns a legendary roll into one of its archetype's uniques. variant -1 picks
// at random: the second unique only in NG+, 50/50 with the first.
void Unique_Make(loot_weapon_t &gun, int32_t variant)
{
	if (gun.archetype >= LOOT_ARCH_COUNT)
		return;

	if (variant < 0)
		variant = (game.loot_ngplus > 0 && frandom() < 0.5f) ? 1 : 0;

	gun.unique = gun.archetype + 1 + (variant ? LOOT_ARCH_COUNT : 0);
	G_FmtTo(gun.name, "{} {}", unique_defs[gun.unique].name, unique_arch_names[gun.archetype]);

	// element uniques: plain guns take the usual elemental damage trade
	const bool dragonmouth = gun.unique == LOOT_UNIQUE_DRAGONMOUTH;
	if ((dragonmouth || gun.unique == LOOT_UNIQUE_PRISM) && (!gun.element || (dragonmouth && gun.element != LOOT_ELEM_FIRE)))
	{
		if (!gun.element)
			gun.dmg_mult *= 0.9f;
		gun.element = LOOT_ELEM_FIRE; // Prism cycles from here
		gun.elem_chance = std::max(gun.elem_chance, 0.3f);
	}
}

static int32_t Unique_Active(edict_t *ent)
{
	const loot_weapon_t *gun = Loot_Active(ent);
	return gun ? gun->unique : LOOT_UNIQUE_NONE;
}

//==========================================================================
// q2rpg: Ava, a memorial Legendary pistol for a very good dog.
// Lost Station (train): ride the tram 13 times and the rubber duck replaces the
// station sign (func_wall "t433"). Stand in front of the duck for 10 seconds:
// a chime plays, the tribute shows and Ava pops out. Once per playthrough.

constexpr const char *AVA_MAP = "train";
constexpr const char *AVA_DUCK_TARGETNAME = "t433";
constexpr const char *AVA_TRIBUTE = "Run free, sweet soul.\nIn Memory of my faithful friend.";
constexpr gtime_t AVA_STAND_TIME = 10_sec;   // how long to stand in front of the duck
constexpr gtime_t AVA_STAND_GRACE = 1500_ms; // stepping out briefly keeps the progress
constexpr gtime_t AVA_ZOOMIES_TIME = 3_sec;  // speed burst per kill; each kill restarts it
constexpr gtime_t AVA_SHRINE_TICK = 100_ms;
// generous box in front of the duck wall: from the wall back past the tram stop
const vec3_t ava_zone_mins = {-1110.f, -830.f, -300.f};
const vec3_t ava_zone_maxs = {-870.f, -500.f, -150.f};
const vec3_t ava_drop_spot = {-992.f, -560.f, -230.f}; // just in front of the duck

void Unique_MakeAva(loot_weapon_t &gun)
{
	gun.unique = LOOT_UNIQUE_AVA;
	Q_strlcpy(gun.name, unique_defs[LOOT_UNIQUE_AVA].name, sizeof(gun.name));
}

const char *Unique_Tribute(int32_t unique)
{
	return unique == LOOT_UNIQUE_AVA ? AVA_TRIBUTE : nullptr;
}

// kill with Ava in hand: start (or restart) the Zoomies burst
void Unique_OnKill(edict_t *killer)
{
	if (!killer || !killer->client || Unique_Active(killer) != LOOT_UNIQUE_AVA)
		return;
	killer->client->loot_zoomies_end = level.time + AVA_ZOOMIES_TIME;
}

static bool Ava_GivenThisPlaythrough()
{
	return game.loot_ava_cycle == game.loot_ngplus + 1;
}

static bool Ava_DuckShowing()
{
	const edict_t *duck_wall = G_FindByString<&edict_t::targetname>(nullptr, AVA_DUCK_TARGETNAME);
	for (; duck_wall; duck_wall = G_FindByString<&edict_t::targetname>((edict_t *)duck_wall, AVA_DUCK_TARGETNAME))
		if (!strcmp(duck_wall->classname, "func_wall"))
			return duck_wall->solid == SOLID_BSP;
	return false;
}

static bool Ava_PlayerInZone()
{
	for (edict_t *player : active_players())
	{
		if (player->health <= 0)
			continue;
		const vec3_t &pos = player->s.origin;
		if (pos.x >= ava_zone_mins.x && pos.x <= ava_zone_maxs.x && pos.y >= ava_zone_mins.y && pos.y <= ava_zone_maxs.y &&
		    pos.z >= ava_zone_mins.z && pos.z <= ava_zone_maxs.z)
			return true;
	}
	return false;
}

static void Ava_Give(edict_t *shrine)
{
	game.loot_ava_cycle = game.loot_ngplus + 1;

	loot_weapon_t gun = Loot_Roll(LOOT_ARCH_PISTOL, LOOT_RARITY_LEGENDARY, (uint32_t)mt_rand());
	Unique_MakeAva(gun);
	Loot_SetLevel(gun, Loot_ZoneLevel());

	gi.positioned_sound(ava_drop_spot, world, CHAN_AUTO, gi.soundindex("misc/secret.wav"), 1.f, ATTN_NONE, 0);
	for (edict_t *player : active_players())
		gi.Center_Print(player, AVA_TRIBUTE);

	edict_t *drop = Loot_SpawnSharedDrop(shrine, gun);
	drop->velocity = {crandom() * 40.f, -160.f, 260.f}; // hop out of the duck toward the player
}

// counts time a player spends in front of the shown duck
THINK(Ava_ShrineThink)(edict_t *self)->void
{
	if (Ava_GivenThisPlaythrough())
	{
		G_FreeEdict(self);
		return;
	}
	self->nextthink = level.time + AVA_SHRINE_TICK;

	if (Ava_DuckShowing() && Ava_PlayerInZone())
	{
		self->count++;
		self->timestamp = level.time;
		if (AVA_SHRINE_TICK * self->count >= AVA_STAND_TIME)
		{
			Ava_Give(self);
			G_FreeEdict(self);
		}
	}
	else if (level.time - self->timestamp > AVA_STAND_GRACE)
		self->count = 0;
}

void Unique_AvaMapSpawn()
{
	if (deathmatch->integer || Q_strcasecmp(level.mapname, AVA_MAP) || Ava_GivenThisPlaythrough())
		return;

	edict_t *shrine = G_Spawn();
	shrine->classname = "loot_ava_shrine";
	shrine->s.origin = ava_drop_spot;
	shrine->mins = {-8, -8, -8};
	shrine->maxs = {8, 8, 8};
	shrine->svflags |= SVF_NOCLIENT;
	shrine->think = Ava_ShrineThink;
	shrine->nextthink = level.time + AVA_SHRINE_TICK;
	gi.linkentity(shrine); // drops spawn at the center of absmin/absmax
}

//==========================================================================
// Fire tagging
//==========================================================================

static int32_t fire_tag;
static bool fire_active; // any rolled gun firing (projectile speed)
static edict_t *fire_owner;
static std::vector<edict_t *> fired;

void Unique_BeginFire(edict_t *ent)
{
	fire_tag = deathmatch->integer ? 0 : Unique_Active(ent);
	fire_active = !deathmatch->integer && Loot_Active(ent);
	fire_owner = ent;
	fired.clear();
}

void Unique_TagSpawn(edict_t *e)
{
	if (!fire_active)
		return;
	e->loot_unique = fire_tag;
	if (const loot_weapon_t *gun = Loot_Active(fire_owner))
		e->loot_shot_gun = *gun; // element / crit snapshot for hits after a weapon switch
	fired.push_back(e);
}

// --- homing (Swarm, Seeker) ---

THINK(unique_homing_think)(edict_t *self)->void
{
	if (level.time >= self->timestamp)
	{
		G_FreeEdict(self);
		return;
	}

	const float speed = self->velocity.length();
	vec3_t dir = self->velocity;
	dir.normalize();

	edict_t *best = nullptr;
	float best_score = 0.6f; // must be roughly ahead
	edict_t *e = nullptr;
	while ((e = findradius(e, self->s.origin, 1024.f)) != nullptr)
	{
		if (!(e->svflags & SVF_MONSTER) || e->health <= 0 || e->deadflag || (e->monsterinfo.aiflags & AI_GOOD_GUY))
			continue;
		vec3_t to = ((e->absmin + e->absmax) * 0.5f) - self->s.origin;
		const float dist = to.normalize();
		const float score = dir.dot(to) - dist / 4096.f;
		if (score > best_score)
		{
			best_score = score;
			best = e;
		}
	}

	if (best)
	{
		vec3_t to = ((best->absmin + best->absmax) * 0.5f) - self->s.origin;
		to.normalize();
		dir = dir + to * 0.3f;
		dir.normalize();
		self->velocity = dir * speed;
		self->s.angles = vectoangles(dir);
	}

	self->nextthink = level.time + 50_ms;
}

static void Unique_MakeHoming(edict_t *bolt)
{
	bolt->timestamp = level.time + 2500_ms;
	bolt->think = unique_homing_think;
	bolt->nextthink = level.time + 50_ms;
}

// --- Hydra ---

THINK(unique_hydra_think)(edict_t *self)->void
{
	edict_t *owner = self->owner;
	if (owner && owner->inuse)
	{
		const float speed = self->velocity.length();
		const vec3_t ang = vectoangles(self->velocity);
		for (float yaw : {-12.f, 12.f})
		{
			vec3_t forward;
			AngleVectors(vec3_t{ang[PITCH], ang[YAW] + yaw, 0}, forward, nullptr, nullptr);
			fire_rocket(owner, self->s.origin, forward, self->dmg, (int32_t)speed, self->dmg_radius, self->radius_dmg);
		}
	}

	// back to the stock rocket lifetime
	self->think = G_FreeEdict;
	self->nextthink = self->timestamp;
}

// --- Echo ---

THINK(unique_echo_think)(edict_t *self)->void
{
	edict_t *owner = self->owner;
	if (owner && owner->inuse && owner->client && owner->health > 0)
	{
		vec3_t forward, right;
		AngleVectors(owner->client->v_angle, forward, right, nullptr);
		const vec3_t start = owner->s.origin + vec3_t{0, 0, (float)owner->viewheight - 8.f} + forward * 16.f + right * 6.f;
		const int32_t dmg = std::max(1, (int32_t)(LOOT_PISTOL_DAMAGE * P_DamageModifier(owner)));
		fire_blaster(owner, start, forward, dmg, 1500, EF_BLASTER, MOD_BLASTER);
		gi.sound(owner, CHAN_AUTO, gi.soundindex("weapons/blastf1a.wav"), 0.6f, ATTN_NORM, 0);
	}
	G_FreeEdict(self);
}

// --- Afterburner ---

THINK(unique_afterburner_think)(edict_t *self)->void
{
	if (level.time >= self->timestamp)
	{
		G_FreeEdict(self); // stock rocket lifetime
		return;
	}

	// speed up to twice the launch speed
	const float max_speed = self->speed * 2.f;
	if (self->velocity.length() < max_speed)
	{
		self->velocity *= 1.08f;
		if (self->velocity.length() > max_speed)
			self->velocity = self->velocity.normalized() * max_speed;
	}

	// ignite monsters the rocket passes near
	edict_t *owner = self->owner;
	if (owner && owner->inuse)
	{
		edict_t *e = nullptr;
		while ((e = findradius(e, self->s.origin, 96.f)) != nullptr)
		{
			if (!(e->svflags & SVF_MONSTER) || e->health <= 0 || e->deadflag || (e->monsterinfo.aiflags & AI_GOOD_GUY))
				continue;
			if (e->loot_status.active && e->loot_status.burn_end > level.time + 2_sec)
				continue; // already freshly lit
			Loot_ApplyStatus(e, owner, LOOT_ELEM_FIRE, self->dmg / 2);
		}
	}

	self->nextthink = level.time + 100_ms;
}

// --- Pinball ---

void blaster_touch(edict_t *self, edict_t *other, const trace_t &tr, bool other_touching_self);

// First wall hit: return without freeing, so MOVETYPE_WALLBOUNCE reflects the
// bolt. After that the stock touch takes over.
TOUCH(unique_pinball_touch)(edict_t *self, edict_t *other, const trace_t &tr, bool other_touching_self)->void
{
	if (other == self->owner)
		return;

	if (other->takedamage || (tr.surface && (tr.surface->flags & SURF_SKY)))
	{
		blaster_touch(self, other, tr, other_touching_self);
		return;
	}

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_BLASTER);
	gi.WritePosition(self->s.origin);
	gi.WriteDir(tr.plane.normal);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	self->touch = blaster_touch;
	self->nextthink = level.time + 1_sec; // fresh lifetime after the bounce
}

// --- Supernova ---

THINK(unique_supernova_think)(edict_t *self)->void
{
	G_FreeEdict(self); // stands in for bfg_think: no tendrils, stock lifetime
}

void Unique_EndFire(edict_t *ent)
{
	const int32_t tag = fire_tag;
	fire_tag = 0;
	fire_active = false;
	fire_owner = nullptr;

	// barrel projectile speed for every rolled gun
	const float speed = Loot_ProjSpeed(ent);
	if (speed != 1.0f)
		for (edict_t *e : fired)
			if (e->inuse && e->movetype != MOVETYPE_NONE && e->velocity)
				e->velocity *= speed;

	for (edict_t *e : fired)
		LootFx_TintProjectile(ent, e);

	if (!tag || fired.empty())
	{
		fired.clear();
		return;
	}

	bool echo_needed = false;
	for (edict_t *e : fired)
	{
		if (!e->inuse || !e->classname)
			continue;

		switch (tag)
		{
			case LOOT_UNIQUE_SWARM:
			case LOOT_UNIQUE_SEEKER:
				if (!strcmp(e->classname, "bolt"))
					Unique_MakeHoming(e);
				break;
			case LOOT_UNIQUE_HYDRA:
				if (!strcmp(e->classname, "rocket"))
				{
					e->timestamp = e->nextthink; // stock free time
					e->think = unique_hydra_think;
					e->nextthink = level.time + 250_ms;
				}
				break;
			case LOOT_UNIQUE_ECHO:
				if (!strcmp(e->classname, "bolt"))
					echo_needed = true;
				break;
			case LOOT_UNIQUE_LASTWORD:
			{
				// only the round that empties the magazine keeps the tag
				const loot_weapon_t *gun = Loot_Active(ent);
				if (!gun || gun->mag_ammo > 0)
					e->loot_unique = LOOT_UNIQUE_NONE;
				else if (!strcmp(e->classname, "bolt"))
					e->s.effects |= EF_HYPERBLASTER; // brighter trail on the big shot
				break;
			}
			case LOOT_UNIQUE_AFTERBURNER:
				if (!strcmp(e->classname, "rocket"))
				{
					e->timestamp = e->nextthink; // stock free time
					e->speed = e->velocity.length();
					e->think = unique_afterburner_think;
					e->nextthink = level.time + 100_ms;
				}
				break;
			case LOOT_UNIQUE_PINBALL:
				if (!strcmp(e->classname, "bolt"))
				{
					e->movetype = MOVETYPE_WALLBOUNCE;
					e->touch = unique_pinball_touch;
				}
				break;
			case LOOT_UNIQUE_SUPERNOVA:
				if (!strcmp(e->classname, "bfg blast") && e->velocity)
				{
					e->think = unique_supernova_think;
					e->nextthink = level.time + gtime_t::from_sec(8000.f / e->velocity.length());
				}
				break;
			default:
				break;
		}
	}
	fired.clear();

	if (echo_needed)
	{
		edict_t *echo = G_Spawn();
		echo->classname = "unique_echo";
		echo->owner = ent;
		echo->svflags |= SVF_NOCLIENT;
		echo->think = unique_echo_think;
		echo->nextthink = level.time + 150_ms;
	}
}

//==========================================================================
// Per-shot / per-frame effects
//==========================================================================

// Swarm: every 5th shot adds a homing blaster bolt (fired inside the weapon
// think, so it gets tagged and turned homing by Unique_EndFire).
void Unique_OnShot(edict_t *ent)
{
	const int32_t unique = Unique_Active(ent);
	if (!unique)
		return;
	ent->client->loot_unique_shots++; // Prism cycles on this too
	if (unique != LOOT_UNIQUE_SWARM || ent->client->loot_unique_shots % 5)
		return;

	vec3_t forward, right;
	AngleVectors(ent->client->v_angle, forward, right, nullptr);
	const vec3_t start = ent->s.origin + vec3_t{0, 0, (float)ent->viewheight - 8.f} + forward * 16.f + right * 6.f;
	const int32_t dmg = std::max(1, (int32_t)(10.f * damage_multiplier));
	fire_blaster(ent, start, forward, dmg, 900, EF_BLASTER, MOD_BLASTER);
}

// Spin Cycle: up to +80% fire rate after 2s of continuous fire.
float Unique_RateMult(edict_t *ent)
{
	if (Unique_Active(ent) != LOOT_UNIQUE_SPINCYCLE || ent->client->weaponstate != WEAPON_FIRING)
		return 1.0f;
	const float held = (level.time - ent->client->loot_spin_start).seconds();
	return 1.0f + 0.8f * std::clamp(held / 2.0f, 0.f, 1.f);
}

void Unique_Frame(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (cl->weaponstate != WEAPON_FIRING || cl->loot_spin_start > level.time)
		cl->loot_spin_start = level.time;
}

bool Unique_Ricochet(edict_t *self, const mod_t &mod)
{
	return self && self->client && !deathmatch->integer && mod.id == MOD_SHOTGUN && Unique_Active(self) == LOOT_UNIQUE_RICOCHET;
}

// Tar Pit: a 4s corrosive puddle. Monsters in it melt and keep flinching.
static constexpr float tarpit_radius = 128.f;

THINK(unique_tarpit_think)(edict_t *self)->void
{
	edict_t *owner = self->owner;
	if (level.time >= self->timestamp || !owner || !owner->inuse)
	{
		G_FreeEdict(self);
		return;
	}

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_SPLASH);
	gi.WriteByte(24);
	gi.WritePosition(self->s.origin + vec3_t{crandom() * 48.f, crandom() * 48.f, 0});
	gi.WriteDir({0, 0, 1});
	gi.WriteByte(SPLASH_SLIME);
	gi.multicast(self->s.origin, MULTICAST_PVS, false);

	const bool stagger = (self->count++ % 2) == 0; // once a second
	edict_t *e = nullptr;
	while ((e = findradius(e, self->s.origin, tarpit_radius)) != nullptr)
	{
		if (!(e->svflags & SVF_MONSTER) || e->health <= 0 || e->deadflag || (e->monsterinfo.aiflags & AI_GOOD_GUY))
			continue;
		if (e->s.origin[2] - self->s.origin[2] > 64.f)
			continue; // flying over the puddle
		Loot_ApplyStatus(e, owner, LOOT_ELEM_CORROSIVE, self->dmg);
		if (stagger && e->pain && !Loot_IsBoss(e))
		{
			e->pain_debounce_time = 0_ms;
			e->pain(e, owner, 0, self->dmg, MOD_LOOT_MELT);
		}
	}

	self->nextthink = level.time + 500_ms;
}

static void Unique_TarPit(edict_t *grenade, edict_t *owner)
{
	// drop to the floor below the blast
	const vec3_t below = grenade->s.origin - vec3_t{0, 0, 128};
	const trace_t tr = gi.traceline(grenade->s.origin, below, grenade, MASK_SOLID);

	edict_t *pit = G_Spawn();
	pit->classname = "unique_tarpit";
	pit->owner = owner;
	pit->svflags |= SVF_NOCLIENT;
	pit->s.origin = tr.endpos + vec3_t{0, 0, 4};
	pit->dmg = std::max(1, grenade->dmg / 4);
	pit->timestamp = level.time + 4_sec;
	pit->think = unique_tarpit_think;
	pit->nextthink = level.time + FRAME_TIME_MS;
	gi.linkentity(pit);
}

// Cluster: tagged grenades burst into 4 bomblets (untagged, so no recursion).
void Unique_GrenadeExplode(edict_t *grenade)
{
	edict_t *owner = grenade->owner;
	if (!owner || !owner->inuse)
		return;

	if (grenade->loot_unique == LOOT_UNIQUE_TARPIT)
	{
		grenade->loot_unique = LOOT_UNIQUE_NONE;
		Unique_TarPit(grenade, owner);
		return;
	}

	if (grenade->loot_unique != LOOT_UNIQUE_CLUSTER)
		return;

	grenade->loot_unique = LOOT_UNIQUE_NONE;
	const vec3_t start = grenade->s.origin + vec3_t{0, 0, 8};
	for (int32_t i = 0; i < 4; i++)
	{
		vec3_t dir = {crandom(), crandom(), 0.8f + frandom(0.4f)};
		dir.normalize();
		fire_grenade(owner, start, dir, std::max(1, grenade->dmg * 2 / 5), 250, gtime_t::from_ms(700 + irandom(400)), 100.f, 0, 0, false);
	}
}

// Singularity: pull monsters toward the impact before the BFG blast.
void Unique_BfgImpact(edict_t *bfg)
{
	if (bfg->loot_unique == LOOT_UNIQUE_SUPERNOVA)
	{
		bfg->loot_unique = LOOT_UNIQUE_NONE; // once
		edict_t *owner = bfg->owner;
		if (!owner || !owner->inuse)
			return;

		// bigger blast on top of the stock one, and every monster in it gets all three elements
		constexpr float nova_radius = 400.f;
		edict_t *e = nullptr;
		while ((e = findradius(e, bfg->s.origin, nova_radius)) != nullptr)
		{
			if (!(e->svflags & SVF_MONSTER) || e->health <= 0 || e->deadflag || (e->monsterinfo.aiflags & AI_GOOD_GUY))
				continue;
			for (int32_t el = LOOT_ELEM_NONE + 1; el < LOOT_ELEM_COUNT; el++)
				Loot_ApplyStatus(e, owner, (loot_element_t)el, bfg->radius_dmg / 4);
		}
		T_RadiusDamage(bfg, owner, bfg->radius_dmg * 0.5f, nullptr, nova_radius, DAMAGE_ENERGY, MOD_BFG_BLAST);

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_BFG_BIGEXPLOSION);
		gi.WritePosition(bfg->s.origin);
		gi.multicast(bfg->s.origin, MULTICAST_PHS, false);
		return;
	}

	if (bfg->loot_unique != LOOT_UNIQUE_SINGULARITY)
		return;

	edict_t *e = nullptr;
	while ((e = findradius(e, bfg->s.origin, 400.f)) != nullptr)
	{
		if (!(e->svflags & SVF_MONSTER) || e->health <= 0 || e->deadflag)
			continue;
		vec3_t to = bfg->s.origin - e->s.origin;
		to.normalize();
		e->velocity = to * 450.f;
		e->velocity[2] += 150.f;
		e->groundentity = nullptr;
	}

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_BFG_BIGEXPLOSION);
	gi.WritePosition(bfg->s.origin);
	gi.multicast(bfg->s.origin, MULTICAST_PHS, false);
}

//==========================================================================
// Damage hooks (Loot_ModifyDamage). gun is the gun that fired the shot.
//==========================================================================

// Last Word: x3 and a forced crit on the magazine's last round.
// Furnace: +5% per second of continuous fire, up to +50%.
float Unique_DamageMult(edict_t *attacker, edict_t *inflictor, const loot_weapon_t &gun, bool &force_crit)
{
	if (gun.unique == LOOT_UNIQUE_LASTWORD && inflictor && inflictor != attacker && inflictor->loot_unique == LOOT_UNIQUE_LASTWORD)
	{
		force_crit = true;
		return 3.0f;
	}
	if (gun.unique == LOOT_UNIQUE_FURNACE && attacker->client->weaponstate == WEAPON_FIRING)
	{
		const float held = (level.time - attacker->client->loot_spin_start).seconds();
		return 1.0f + std::min(0.5f, 0.05f * held);
	}
	return 1.0f;
}

// Dragonmouth: every hit ignites. Prism: element cycles each shot, half proc chance.
void Unique_Element(edict_t *attacker, const loot_weapon_t &gun, loot_element_t &element, float &chance)
{
	if (gun.unique == LOOT_UNIQUE_DRAGONMOUTH)
		chance = 1.0f;
	else if (gun.unique == LOOT_UNIQUE_PRISM)
	{
		element = (loot_element_t)(1 + attacker->client->loot_unique_shots % (LOOT_ELEM_COUNT - 1));
		chance *= 0.5f;
	}
}

static void Unique_Lancer(edict_t *targ, edict_t *attacker, int32_t damage)
{
	Loot_ApplyStatus(targ, attacker, LOOT_ELEM_SHOCK, damage);

	const vec3_t from = (targ->absmin + targ->absmax) * 0.5f;
	int32_t chained = 0;
	edict_t *e = nullptr;
	while ((e = findradius(e, from, 160.f)) != nullptr && chained < 4)
	{
		if (e == targ || !(e->svflags & SVF_MONSTER) || e->health <= 0 || e->deadflag)
			continue;

		const vec3_t to = (e->absmin + e->absmax) * 0.5f;
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_LIGHTNING);
		gi.WriteEntity(targ);
		gi.WriteEntity(e);
		gi.WritePosition(from);
		gi.WritePosition(to);
		gi.multicast(from, MULTICAST_PVS, false);

		Loot_ApplyStatus(e, attacker, LOOT_ELEM_SHOCK, damage / 2);
		chained++;
	}
}

// Runs before the damage lands; returns the (maybe raised) damage.
int32_t Unique_OnHit(edict_t *targ, edict_t *attacker, const loot_weapon_t &gun, int32_t damage, const mod_t &mod)
{
	if (targ->health <= 0)
		return damage;

	switch (gun.unique)
	{
		case LOOT_UNIQUE_LANCER:
			// Lancer: rail hits shock the target and chain to monsters within 160 units
			if (mod.id == MOD_RAILGUN)
				Unique_Lancer(targ, attacker, damage);
			break;

		case LOOT_UNIQUE_BULWARK:
			if (mod.id == MOD_SHOTGUN)
			{
				client_persistant_t &pers = attacker->client->pers;
				if (pers.loot_shield.valid)
				{
					const float cap = pers.loot_shield.capacity * Loot_ShieldCapMult(attacker);
					pers.loot_shield_points = std::min(cap, pers.loot_shield_points + cap * 0.01f);
				}
			}
			break;

		case LOOT_UNIQUE_TICKTOCK:
			if (++attacker->client->loot_unique_hits % 10 == 0)
			{
				targ->loot_mark_end = level.time + 4_sec;
				gi.WriteByte(svc_temp_entity);
				gi.WriteByte(TE_SPLASH);
				gi.WriteByte(32);
				gi.WritePosition((targ->absmin + targ->absmax) * 0.5f);
				gi.WriteDir({0, 0, 1});
				gi.WriteByte(SPLASH_SPARKS);
				gi.multicast(targ->s.origin, MULTICAST_PVS, false);
				gi.sound(targ, CHAN_AUTO, gi.soundindex("misc/talk1.wav"), 1.f, ATTN_NORM, 0);
			}
			break;

		case LOOT_UNIQUE_EXECUTIONER:
			// finish non-boss targets this hit leaves under 15% health
			if (mod.id == MOD_RAILGUN && !Loot_IsBoss(targ) && targ->max_health > 0 &&
			    targ->health - damage < (int32_t)(targ->max_health * 0.15f))
				damage = std::max(damage, targ->health + 1);
			break;

		default:
			break;
	}
	return damage;
}
