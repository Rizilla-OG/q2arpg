// q2rpg: the mod's console aliases and default key binds, set by the HUD side of the DLL.
//
// They used to live in q2rpg/autoexec.cfg, which had two problems: it re-bound every key on
// every launch (undoing players' rebinds), and a player's own Saved Games autoexec.cfg runs
// instead of the mod's, which silently dropped the aliases (zoom, tutorial).
//
// Now: the aliases are defined every launch. The default binds are applied once per
// BINDS_VERSION (archived cvar q2rpg_binds), and only for commands that have no key yet, so
// existing players keep what they bound. q2rpg_resetbinds puts every default key back.
// The game saves binds itself (q2rpg/keybinds.cfg), so they stick after that.

#include "../cg_local.h"

namespace
{
struct default_bind_t
{
	const char *key;
	const char *command;
};

// keep in step with the Controls table in README.md and the default keys in cg_gui.cpp
constexpr default_bind_t default_binds[] = {
    {"1", "slot 1"},
    {"2", "slot 2"},
    {"3", "slot 3"},
    {"4", "slot 4"},
    {"5", "slot 5"},
    {"6", "slot 6"},
    {"7", "slot 7"},
    {"8", "slot 8"},
    {"9", "slot 9"},
    {"r", "reload"},
    {"e", "loot_take"},
    {"g", "loot_drop"},
    {"i", "loot_inv"},
    {"k", "skills"},
    {"f", "action"},
    {"x", "loot_sell"},
    {"b", "loot_stash"},
    {"l", "loot_filter"},
    {"v", "vendor"},
    {"j", "challenges"},
    {"q", "grenade"},
    {"z", "loot_melee"},
    {"m", "loot_map"},
    {"h", "loot_give"},
    {"MOUSE2", "+loot_zoom"},
    // controller (split-screen coop); A jump, LS click crouch, RT fire, RB weapon wheel,
    // dpad L/R weapon prev/next stay stock
    {"X_BUTTON", "reload"},
    {"Y_BUTTON", "loot_take"},
    {"B_BUTTON", "loot_sell"},
    {"LEFT_TRIGGER", "+loot_zoom"},
    {"LEFT_SHOULDER", "action"},
    {"DPAD_UP", "loot_inv"},
    {"DPAD_DOWN", "vendor"},
    {"VIEW_BUTTON", "skills"},
    {"RIGHT_STICK", "grenade"},
};

// bump when default_binds gains a command, so players get it bound (if it has no key yet)
constexpr int32_t BINDS_VERSION = 1;

cvar_t *binds_cvar;

bool IsBound(const char *command)
{
	const char *key = cgi.CL_GetKeyBinding(command);
	return key && *key;
}
} // namespace

void CG_Binds_Init()
{
	cgi.AddCommandString(
	    // type "tutorial" in the console to play the tutorial map
	    "alias tutorial \"map q2rpg_tutorial\"\n"
	    // hold-to-zoom: the engine doesn't forward +/- commands to the game, so alias them to plain ones
	    "alias +loot_zoom \"cmd zoom_on\"\n"
	    "alias -loot_zoom \"cmd zoom_off\"\n"
	    "alias q2rpg_resetbinds \"set q2rpg_binds -1\"\n");
	binds_cvar = cgi.cvar("q2rpg_binds", "0", CVAR_ARCHIVE);
}

// once binds are loaded (first HUD frame), apply the defaults if this version hasn't yet,
// or all of them after q2rpg_resetbinds
void CG_Binds_Frame()
{
	static bool checked_this_session;
	if (!binds_cvar)
		return;
	const bool reset = binds_cvar->integer < 0;
	if (!reset && binds_cvar->integer == BINDS_VERSION)
	{
		// q2rpg_binds is saved in the global system.cfg, the binds per mod: if the player cleared
		// their q2rpg folder the version still says done, so apply again when none of ours has a key
		if (checked_this_session)
			return;
		checked_this_session = true;
		for (const default_bind_t &b : default_binds)
			if (IsBound(b.command))
				return;
	}
	checked_this_session = true;
	std::string cmds;
	// every check sees the binds as they were before this pass (the commands run later),
	// so a fresh install gets both the keyboard and controller key for each command
	for (const default_bind_t &b : default_binds)
		if (reset || !IsBound(b.command))
			cmds += G_Fmt("bind {} \"{}\"\n", b.key, b.command).data();
	if (!cmds.empty())
		cgi.AddCommandString(cmds.c_str());
	cgi.cvar_set("q2rpg_binds", G_Fmt("{}", BINDS_VERSION).data());
	if (reset)
		cgi.Com_Print("Q2ARPG: default keys restored.\n");
}
