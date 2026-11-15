# Security Policy

## Supported versions

Q2ARPG is a prerelease mod. Only the latest release gets fixes.

| Version          | Supported |
| ---------------- | --------- |
| Latest release   | Yes       |
| Older releases   | No        |

## What counts

Q2ARPG is a game DLL (`game_x64.dll`) for the Quake II rerelease. Report it if the mod can:

- crash or run code through a crafted save, map, `.cfg`, or loot/stash file
- let a coop guest crash, freeze, or take over the host's game
- write files outside the `q2rpg/` mod folder
- ship anything harmful in a release zip

Not security issues (open a normal issue or Discussion instead): balance, gameplay bugs, ordinary crashes with no outside input, and bugs in the Quake II rerelease engine itself (report those to its makers).

## How to report

Please **don't** open a public issue for a security problem.

Use GitHub's private reporting: go to the repo's **Security** tab and click **Report a vulnerability**.

Include the version (see `VERSION` or the in-game console logo), steps to reproduce, and any file that triggers it.

## What happens next

- You'll get a reply within about a week.
- If it's real, the fix ships in the next release and the CHANGELOG notes it.
- You'll be credited unless you'd rather not be.

This is a hobby project, so there's no bug bounty.
