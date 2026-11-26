#!/usr/bin/env python3
"""Generates q2rpg_tutorial.map: a straight run of rooms, one lesson each.

Every room has a message strip at its entrance and a sign pad (arrow floor) that shows
the message again. Fight rooms close behind a door that opens when their monsters die.
Run:  python3 make_tutorial.py  (writes q2rpg_tutorial.map next to this script)
Then compile it like any map (see MODDING.md).
"""

import os

FLOOR = "e1u1/floor1_3"
WALL = "e1u1/wslt1_5"
PAD = "e1u1/arrowup0"
TRIGGER = "e1u1/trigger"

ROOM_W = 640    # along x
HALF_Y = 320    # rooms span y -320..320
HEIGHT = 256
T = 16          # wall thickness
DOOR_HALF = 64  # doorways are 128 wide
DOOR_H = 128


def brush(x1, y1, z1, x2, y2, z2, tex):
    lo, hi = (x1, y1, z1), (x2, y2, z2)
    face = lambda a, b, c: f"( {a[0]} {a[1]} {a[2]} ) ( {b[0]} {b[1]} {b[2]} ) ( {c[0]} {c[1]} {c[2]} ) {tex} 0 0 0 1 1 0 0 0"
    x1, y1, z1 = lo
    x2, y2, z2 = hi
    return "\n".join([
        "{",
        face((x1, y1, z1), (x1, y1 + 1, z1), (x1, y1, z1 + 1)),
        face((x1, y1, z1), (x1, y1, z1 + 1), (x1 + 1, y1, z1)),
        face((x1, y1, z1), (x1 + 1, y1, z1), (x1, y1 + 1, z1)),
        face((x2, y2, z2), (x2, y2 + 1, z2), (x2 + 1, y2, z2)),
        face((x2, y2, z2), (x2 + 1, y2, z2), (x2, y2, z2 + 1)),
        face((x2, y2, z2), (x2, y2, z2 + 1), (x2, y2 + 1, z2)),
        "}",
    ])


def entity(keys, brushes=()):
    lines = ["{"] + [f'"{k}" "{v}"' for k, v in keys.items()] + list(brushes) + ["}"]
    return "\n".join(lines)


def msg(*lines):
    return "\\n".join(lines)


# One lesson per room. "items" are point entities placed in the room (x, y relative to the
# room's left edge and center line). "gate": monsters that must die before the exit opens.
ROOMS = [
    dict(
        title="welcome",
        message=msg("WELCOME TO Q2ARPG", "Every gun is rolled. Kill, loot,", "level up, gear up.",
                    "Walk on: each room teaches one thing."),
        sign=msg("YOUR HUD (top left)", "Gun name, level badge and XP bar,", "your cash, and the F action meter.",
                 "M: minimap, full map or off."),
        items=[],
    ),
    dict(
        title="loot",
        message=msg("LOOT", "Beams show rarity: white Common,", "green Uncommon, blue Rare,",
                    "purple Epic, gold Legendary.", "Walk over a gun to see its card."),
        sign=msg("TAKING GUNS", "Green stats beat your gun, red lose.", "E takes it. You carry 4 guns: keys 1-4.",
                 "I: loadout   G: drop   R: reload", "Hold right mouse to zoom."),
        items=[
            ("item_loot_gun", 320, -160, {"loot_rarity": "0"}),
            ("item_loot_gun", 320, 0, {"loot_rarity": "1"}),
            ("item_loot_gun", 320, 160, {"loot_rarity": "2"}),
        ],
    ),
    dict(
        title="gear",
        message=msg("GEAR", "Shields soak damage first and recharge:", "the blue bar above your health.",
                    "Grenade mods change what Q throws."),
        sign=msg("MORE GEAR", "Class mods add skill ranks.", "Relics give one passive bonus.",
                 "Hand grenades are your grenade ammo."),
        items=[
            ("item_loot_shield", 256, -96, {"loot_rarity": "1"}),
            ("item_loot_grenade", 256, 96, {"loot_rarity": "1", "loot_grenade": "Frag"}),
            ("ammo_grenades", 448, 0, {}),
        ],
    ),
    dict(
        title="fight",
        message=msg("FIGHT", "Monsters drop XP, cash and loot.", "Numbers show your damage: aim for heads.",
                    "Cash flies to you. Z is a melee strike.", "Clear the room to open the door."),
        sign=msg("ELEMENTS", "Fire burns, shock staggers,", "corrosive melts armor, cryo freezes,",
                 "explosive blasts. Hit monsters glow."),
        items=[
            ("monster_soldier_light", 448, -160, {"angle": "180"}),
            ("monster_soldier_light", 512, 0, {"angle": "180"}),
            ("monster_soldier_light", 448, 160, {"angle": "180"}),
        ],
        gate=3,
    ),
    dict(
        title="alpha",
        message=msg("ALPHA", "Some monsters roll as Alphas:", "bigger, tougher, better loot.",
                    "In New Game+ watch for Omegas.", "Kill it to open the door."),
        sign=msg("ALPHA", "Their health bar says Alpha", "in orange. Bring your best gun."),
        items=[
            ("monster_gunner", 480, 0, {"angle": "180", "loot_elite": "1"}),
            ("item_health_large", 160, -224, {}),
            ("item_health_large", 160, 224, {}),
        ],
        gate=1,
    ),
    dict(
        title="chest",
        message=msg("CHESTS", "Walk into a chest to open it.", "Every map hides one or two.",
                    "Red chests always hold a Rare or better."),
        sign=msg("CHESTS", "In co-op each player opens a chest", "once and gets their own loot."),
        items=[("item_loot_chest", 384, 0, {"loot_rarity": "2"})],
    ),
    dict(
        title="stash",
        message=msg("TOO MUCH LOOT?", "Stand on the pad for a free gun.", "Standing on an item:",
                    "X sells it (press twice),", "B sends it to your stash."),
        sign=msg("STASH AND FILTER", "B on empty floor opens your stash.", "It keeps items across maps and NG+.",
                 "L hides low-rarity drops."),
        items=[],
        loot_pad=True,
    ),
    dict(
        title="vendor",
        message=msg("THE VENDOR", "Press V anywhere to shop.", "First visit per map is free.",
                    "9 switches tabs: Stock, Upgrades,", "Workshop and Missions."),
        sign=msg("VENDOR TABS", "Upgrades are permanent boosts.", "The workshop levels your gun up to you.",
                 "Missions pay XP, cash and gear."),
        items=[],
    ),
    dict(
        title="skills",
        message=msg("SKILLS", "Each level gives a skill point.", "Press K and pick a class:",
                    "Gunner, Juggernaut or Elementalist.", "Then press 1-9 to spend points."),
        sign=msg("ACTION SKILL", "F fires your class's action skill.", "The meter top left shows the cooldown.",
                 "Try it on these monsters."),
        items=[
            ("monster_soldier", 480, -128, {"angle": "180"}),
            ("monster_soldier", 480, 128, {"angle": "180"}),
        ],
        gate=2,
    ),
    dict(
        title="boss",
        message=msg("BOSS", "Bosses drop a signature gun.", "J: challenges pay XP and cash.",
                    "Page 3 picks your look.", "Beat the boss to finish."),
        sign=msg("CHALLENGES", "Challenge tiers unlock skins and auras.", "In co-op, H hands your gun to a friend."),
        items=[
            ("monster_gladiator", 480, 0, {"angle": "180", "loot_boss": "1"}),
            ("item_health_large", 160, -224, {}),
            ("item_armor_combat", 160, 224, {}),
        ],
        gate=1,
    ),
    dict(
        title="done",
        message=msg("TUTORIAL COMPLETE", "You know it all. Start a New Game", "from the menu to play the campaign.",
                    "Good hunting, Marine."),
        sign=msg("TUTORIAL COMPLETE", "Start a New Game from the menu", "to play the campaign."),
        items=[],
    ),
]


def build():
    n = len(ROOMS)
    x_end = n * ROOM_W
    world = []

    # shell: floor, ceiling, side walls, end walls
    world.append(brush(-T, -HALF_Y - T, -T, x_end + T, HALF_Y + T, 0, FLOOR))
    world.append(brush(-T, -HALF_Y - T, HEIGHT, x_end + T, HALF_Y + T, HEIGHT + T, WALL))
    world.append(brush(-T, -HALF_Y - T, 0, x_end + T, -HALF_Y, HEIGHT, WALL))
    world.append(brush(-T, HALF_Y, 0, x_end + T, HALF_Y + T, HEIGHT, WALL))
    world.append(brush(-T, -HALF_Y, 0, 0, HALF_Y, HEIGHT, WALL))
    world.append(brush(x_end, -HALF_Y, 0, x_end + T, HALF_Y, HEIGHT, WALL))

    ents = []
    for i, room in enumerate(ROOMS):
        x0 = i * ROOM_W
        # partition wall at the room's right end, with a doorway (not after the last room)
        if i < n - 1:
            wx = x0 + ROOM_W - T
            world.append(brush(wx, -HALF_Y, 0, x0 + ROOM_W, -DOOR_HALF, HEIGHT, WALL))
            world.append(brush(wx, DOOR_HALF, 0, x0 + ROOM_W, HALF_Y, HEIGHT, WALL))
            world.append(brush(wx, -DOOR_HALF, DOOR_H, x0 + ROOM_W, DOOR_HALF, HEIGHT, WALL))
            if room.get("gate"):
                gate = f"gate_{room['title']}"
                ents.append(entity({"classname": "func_door", "angle": "-1", "targetname": gate, "speed": "120", "wait": "-1", "lip": "8"},
                                   [brush(wx, -DOOR_HALF, 0, x0 + ROOM_W, DOOR_HALF, DOOR_H, WALL)]))
                ents.append(entity({"classname": "trigger_counter", "targetname": f"kills_{room['title']}", "count": str(room["gate"]),
                                    "target": gate, "origin": f"{x0 + 64} 0 64"}))

        # entrance strip: shows the lesson once as you walk in
        ents.append(entity({"classname": "trigger_once", "message": room["message"]},
                           [brush(x0 + 8, -HALF_Y + 8, 0, x0 + 48, HALF_Y - 8, 128, TRIGGER)]))
        # sign pad (arrow floor) at the room's near side: shows the second note, again on revisit
        world.append(brush(x0 + 96, -HALF_Y + 32, 0, x0 + 160, -HALF_Y + 96, 2, PAD))
        ents.append(entity({"classname": "trigger_multiple", "wait": "6", "message": room["sign"]},
                           [brush(x0 + 96, -HALF_Y + 32, 2, x0 + 160, -HALF_Y + 96, 64, TRIGGER)]))

        # lights
        for lx in (x0 + ROOM_W // 4, x0 + 3 * ROOM_W // 4):
            ents.append(entity({"classname": "light", "origin": f"{lx} 0 {HEIGHT - 32}", "light": "350"}))

        for cls, ix, iy, keys in room["items"]:
            z = 24 if cls.startswith("monster_") else 16
            e = {"classname": cls, "origin": f"{x0 + ix} {iy} {z}"}
            e.update(keys)
            if cls.startswith("monster_") and room.get("gate"):
                e["deathtarget"] = f"kills_{room['title']}"
            ents.append(entity(e))

        if room.get("loot_pad"):
            # free Common gun every 3 seconds while you stand on the pad (for selling / stashing practice)
            world.append(brush(x0 + 288, -32, 0, x0 + 352, 32, 2, PAD))
            ents.append(entity({"classname": "trigger_multiple", "wait": "3", "target": "tutorial_lootpad"},
                               [brush(x0 + 288, -32, 2, x0 + 352, 32, 64, TRIGGER)]))
            ents.append(entity({"classname": "target_loot_drop", "targetname": "tutorial_lootpad", "loot_rarity": "0",
                                "origin": f"{x0 + 400} 0 32"}))

    # start inside the first room's entrance strip, so the welcome shows on spawn
    ents.insert(0, entity({"classname": "info_player_start", "origin": "32 0 24", "angle": "0"}))

    worldspawn = entity({
        "classname": "worldspawn",
        "message": "Q2ARPG Tutorial",
        "sky": "unit1_",
        "_minlight": "24",
        "loot_start_level": "3",      # a couple of skill points for the skills room
        "loot_nochests": "1",         # only the tutorial's own chest
        "loot_nomodifier": "1",
        "loot_elite_chance": "0",     # the only Alpha is the one in the Alpha room
    }, world)
    header = ("// Q2ARPG tutorial: one lesson per room. Generated by make_tutorial.py; edit that, not this file.\n"
              "// Compile (see MODDING.md), copy the .bsp to q2rpg/maps/, then: map q2rpg_tutorial\n")
    return header + worldspawn + "\n" + "\n".join(ents) + "\n"


if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "q2rpg_tutorial.map")
    with open(out, "w") as f:
        f.write(build())
    print(f"wrote {out}")
