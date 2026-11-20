// q2rpg: co-op trading. "loot_give" (H): aim at a teammate within reach and hand over the gun you
// hold. It lands in their first empty gun slot without switching their weapon, else in their
// stash; if both are full nothing changes. Your last gun can't be given. Co-op only.

#include "../g_local.h"

constexpr float TRADE_REACH = 160.f;

// the live teammate under the crosshair, within reach
static edict_t *Trade_Target(edict_t *ent)
{
	vec3_t forward;
	AngleVectors(ent->client->v_angle, forward, nullptr, nullptr);
	const vec3_t eye = ent->s.origin + vec3_t{0, 0, (float)ent->viewheight};
	const trace_t aim = gi.trace(eye, vec3_t{-4, -4, -4}, vec3_t{4, 4, 4}, eye + forward * TRADE_REACH, ent, MASK_SHOT);
	edict_t *target = aim.ent;
	if (!target || target == ent || !target->client || target->health <= 0 || target->client->resp.spectator)
		return nullptr;
	return target;
}

void Trade_Give(edict_t *ent)
{
	gclient_t *cl = ent->client;
	if (deathmatch->integer || ent->health <= 0)
		return;
	if (!coop->integer)
	{
		gi.Center_Print(ent, "Trading is for co-op");
		return;
	}

	const loot_weapon_t *held = Loot_Active(ent);
	if (!held)
	{
		gi.Center_Print(ent, "Hold the gun you want to give");
		return;
	}
	if (Loot_CountGuns(cl) <= 1)
	{
		gi.Center_Print(ent, "Can't give away your last gun");
		return;
	}

	edict_t *receiver = Trade_Target(ent);
	if (!receiver)
	{
		gi.Center_Print(ent, "Look at a teammate up close to give your gun");
		return;
	}

	const loot_weapon_t gift = *held;
	gclient_t *receiver_cl = receiver->client;
	const int32_t slot = Loot_FindEmptySlot(receiver_cl);
	const char *where = "";
	if (slot >= 0)
	{
		receiver_cl->pers.loot_slots[slot] = gift;
		Loot_SyncInventory(receiver);
	}
	else if (Vendor_StashAddGun(receiver, gift))
		where = " (to stash)";
	else
	{
		gi.Center_Print(ent, G_Fmt("{}'s guns and stash are full", receiver_cl->pers.netname).data());
		return;
	}

	Loot_RemoveActiveGun(ent, true);

	gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/w_pkup.wav"), 1, ATTN_NORM, 0);
	gi.sound(receiver, CHAN_ITEM, gi.soundindex("misc/w_pkup.wav"), 1, ATTN_NORM, 0);
	gi.Center_Print(ent, G_Fmt("Gave {} to {}{}", gift.name, receiver_cl->pers.netname, where).data());
	gi.Center_Print(receiver, G_Fmt("{} gave you {} ({}){}", cl->pers.netname, gift.name, Loot_RarityName(gift.rarity), where).data());
	Loot_LogEvent(ent, G_Fmt("TRADE gave {} to {}", gift.name, receiver_cl->pers.netname).data());
}
