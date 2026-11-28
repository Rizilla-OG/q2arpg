#!/usr/bin/env python3
"""Generates q2rpg_psx_more_test.map: a test hall for the second batch of PSX-update ports.

Not shipped. Walk east through the stations:
  1. Updraft: an additive trigger_push lifts you to a ledge (no gravity while inside).
  2. Passive hurt: a trigger_hurt (Passive + Slow) hurts you while you stand still; a No Move barrel.
  3. Lasers: a harmless Reactor beam (dmg 0, you walk through it) and a No Protection red beam
     (body armor on the floor before it; the beam should ignore the armor).
  4. Key relay: a floating data CD (No Drop). The button only works with the CD; after the first
     use the trigger_key is a relay, so every press toggles the quad model (misc_model Toggle).
     The button's trigger_multiple plays a custom noise.
  5. Safe wall: stand on the arrow pad; 2 s later a func_wall (Safe Appear) appears on you and
     pushes you out instead of telefragging you.
  6. Arena: medic, fixbot and soldiers (shotgun guard, machinegun guard, light). Kill the infantry
     near the medic and fixbot; they should revive it, and it should come after you.
Run:  python3 make_psx_more_test.py  (writes q2rpg_psx_more_test.map next to this script)
"""

import os

from make_tutorial import brush, entity

FLOOR = "e1u1/floor1_3"
WALL = "e1u1/wslt1_5"
PAD = "e1u1/arrowup0"
TRIGGER = "e1u1/trigger"
BUTTON = WALL

LENGTH = 3584   # hall spans 0..LENGTH in x
HW = 320        # half width in y
HEIGHT = 640
T = 16


def build():
    world = [
        brush(-T, -HW - T, -T, LENGTH + T, HW + T, 0, FLOOR),               # floor
        brush(-T, -HW - T, HEIGHT, LENGTH + T, HW + T, HEIGHT + T, WALL),   # ceiling
        brush(-T, -HW - T, 0, 0, HW + T, HEIGHT, WALL),                     # west
        brush(LENGTH, -HW - T, 0, LENGTH + T, HW + T, HEIGHT, WALL),        # east
        brush(0, -HW - T, 0, LENGTH, -HW, HEIGHT, WALL),                    # south
        brush(0, HW, 0, LENGTH, HW + T, HEIGHT, WALL),                      # north
        # 1. ledge the updraft lifts you to (north side)
        brush(256, 160, 352, 512, HW, 368, FLOOR),
        brush(256, -128, 0, 384, 128, 2, PAD),                              # updraft marker
        # 5. safe-wall pad
        brush(2240, -64, 0, 2368, 64, 2, PAD),
        # 4. button wall stub
        brush(1792, HW - 32, 0, 1920, HW, 160, WALL),
    ]
    ents = [
        entity({"classname": "info_player_start", "origin": "96 0 24", "angle": "0"}),

        # 1. updraft: additive push straight up, 600 u/s cap
        entity({"classname": "trigger_push", "angles": "-90 0 0", "speed": "60", "spawnflags": "32"},
               [brush(256, -128, 0, 384, 128, 560, TRIGGER)]),

        # 2. passive hurt (Passive 65536 + Slow 16): 2 damage a second, even standing still
        entity({"classname": "trigger_hurt", "dmg": "2", "spawnflags": str(65536 | 16)},
               [brush(704, -192, 0, 896, 192, 96, TRIGGER)]),
        entity({"classname": "misc_explobox", "origin": "800 256 0", "spawnflags": "1"}),   # No Move
        entity({"classname": "misc_explobox", "origin": "800 -256 0"}),                      # normal, for comparison

        # 3. lasers across the hall
        entity({"classname": "item_armor_body", "origin": "1056 0 16"}),
        entity({"classname": "target_laser", "origin": f"1184 {-HW + 16} 48", "target": "laser_harmless_end",
                "spawnflags": str(1 | 131072), "dmg": "0"}),                                 # Reactor, harmless
        entity({"classname": "info_notnull", "origin": f"1184 {HW - 16} 48", "targetname": "laser_harmless_end"}),
        entity({"classname": "target_laser", "origin": f"1376 {-HW + 16} 32", "target": "laser_np_end",
                "spawnflags": str(1 | 2 | 262144), "dmg": "4"}),                             # red, No Protection
        entity({"classname": "info_notnull", "origin": f"1376 {HW - 16} 32", "targetname": "laser_np_end"}),

        # 4. key relay
        entity({"classname": "key_data_cd", "origin": "1600 0 64", "spawnflags": "8"}),      # No Drop: floats
        entity({"classname": "func_button", "angle": "-1", "target": "keyuse", "wait": "1"},
               [brush(1824, HW - 48, 48, 1888, HW - 32, 112, BUTTON)]),
        entity({"classname": "trigger_key", "origin": "1856 160 64", "targetname": "keyuse",
                "item": "key_data_cd", "target": "keymodel", "spawnflags": "1"}),            # Become Relay
        entity({"classname": "misc_model", "origin": "1856 128 48", "targetname": "keymodel",
                "model": "models/items/quaddama/tris.md2", "spawnflags": "1"}),             # Toggle, starts hidden
        entity({"classname": "trigger_multiple", "noise": "misc/keyuse.wav", "wait": "1"},
               [brush(1792, 96, 0, 1920, HW - 48, 96, TRIGGER)]),

        # 5. safe wall: appears 2 s after you step on the pad
        entity({"classname": "trigger_once", "target": "safewall", "delay": "2",
                "message": "Stay on the pad: a wall appears in 2 seconds."},
               [brush(2240, -64, 0, 2368, 64, 64, TRIGGER)]),
        entity({"classname": "func_wall", "targetname": "safewall", "spawnflags": str(2 | 32)},  # Toggle, Safe Appear
               [brush(2272, -32, 0, 2336, 32, 128, WALL)]),

        # 6. arena
        entity({"classname": "monster_infantry", "origin": "2944 160 24", "angle": "180"}),
        entity({"classname": "monster_medic", "origin": "3328 192 24", "angle": "180"}),
        entity({"classname": "monster_fixbot", "origin": "3328 -192 96", "angle": "180"}),
        entity({"classname": "monster_soldier", "origin": "3072 -192 24", "angle": "180"}),     # shotgun guard
        entity({"classname": "monster_soldier_ss", "origin": "3200 0 24", "angle": "180"}),     # machinegun guard
        entity({"classname": "monster_soldier_light", "origin": "3072 0 24", "angle": "180"}),
    ]
    for lx in range(256, LENGTH, 512):
        for ly in (-160, 160):
            ents.append(entity({"classname": "light", "origin": f"{lx} {ly} {HEIGHT - 64}", "light": "450"}))

    worldspawn = entity({
        "classname": "worldspawn",
        "message": "PSX ports test 2",
        "sky": "unit1_",
        "_minlight": "24",
        "loot_nochests": "1",
        "loot_nomodifier": "1",
        "loot_elite_chance": "0",
    }, world)
    header = ("// Q2ARPG test map for the second batch of PSX-update ports. Not shipped.\n"
              "// Generated by make_psx_more_test.py; edit that, not this file.\n")
    return header + worldspawn + "\n" + "\n".join(ents) + "\n"


if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "q2rpg_psx_more_test.map")
    with open(out, "w") as f:
        f.write(build())
    print(f"wrote {out}")
