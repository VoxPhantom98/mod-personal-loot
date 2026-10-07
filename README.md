<p align="center">
  <img src="icon.png" alt="Voxville Personal Loot treasure chest" width="160">
</p>

# Voxville Personal Loot

**Independent loot for every eligible player, with optional personal gold and saved-loot recovery.**

Experimental v0.1.0 · World of Warcraft 3.3.5a · GNU AGPL v3

[![Checks](https://github.com/VoxPhantom98/mod-personal-loot/actions/workflows/checks.yml/badge.svg)](https://github.com/VoxPhantom98/mod-personal-loot/actions/workflows/checks.yml)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](LICENSE)

Turn a shared corpse or supported chest into separate personal loot piles. Each eligible character gets
their own roll using the existing loot tables and server rates. Another player or Playerbot collecting
their reward does not consume yours. Independent rolls preserve drop probabilities: they do not guarantee
gear from every boss.

This release includes a module **and required core patches**. It targets the pinned Playerbots fork below;
it is not a drop-in module for an unmodified AzerothCore checkout. Back up your server and character database
before installing. Start with a test realm.

## Features

- Independent creature corpse loot in the open world, dungeons, and raids.
- Encounter-owned boss chests, plus optional ordinary world treasure chests.
- Optional separate rolls for characters controlled by Playerbots.
- Optional personal gold using stock money rates.
- Optional built-in area looting for items **and gold**.
- Optional durable reward journal with automatic recovery mail after despawn or restart.
- Recovery for all eligible items, a minimum quality, or a Postmaster-inspired dungeon/raid profile.
- Quest, inventory, ownership, and unique-item checks; exact generated item properties are saved.

## Supported loot

| Source | Behavior |
| --- | --- |
| Creature corpses | Independent rolls for eligible recipients |
| Encounter-owned boss chests | Captured encounter recipients; controlled by `BossChests` |
| Ordinary outdoor treasure chests | Optional; opener and eligible nearby party members are frozen at first opening |
| Generic instance chests | Stock behavior unless supported as encounter-owned boss chests |
| Gathering nodes, fishing, skinning, pickpocketing, item containers | Stock behavior |
| Quest-specific world chests | Stock behavior |

Recipients are determined by the source's eligibility rules, not simply by everyone in the raid roster.
Reopening a chest or changing parties does not add recipients or reroll rewards. Saved rewards belong to
the original character identity. Offline characters cannot gain a new entitlement merely by joining later.
There is no pity or bad-luck protection in this release.

## Compatibility

| Component | Tested base |
| --- | --- |
| [Playerbots AzerothCore fork](https://github.com/mod-playerbots/azerothcore-wotlk) | `7f12e89ee5f467a50e62eba1d525eac7dc953d03` |
| [Playerbots module](https://github.com/mod-playerbots/mod-playerbots) (optional) | `7bae1b5c58c76a0aa20381155edc08096d1485b2` |

The exact revisions and patch SHA-256 hashes are in [compatibility.json](compatibility.json).
Other revisions require a deliberate port and testing. The installer refuses unsupported revisions and
changed patch targets. Do not enable an unmodified external `mod-aoe-loot` alongside the built-in area loot.
This project does not bundle that module's implementation.

## Installation

Use Git, Python 3, and your normal AzerothCore build prerequisites. These steps apply to a **stopped test server**.
The installer only patches source files; it does not access databases, change live configuration, or restart a server.

1. Back up the character database, server binaries, configuration, and current source checkout.
2. Prepare the supported core revision. If installing Playerbots, prepare its supported revision too.
3. Clone this repository into `azerothcore-wotlk/modules/mod-personal-loot`.

For a new source checkout (these Git commands also work in PowerShell):

```sh
git clone https://github.com/mod-playerbots/azerothcore-wotlk.git
cd azerothcore-wotlk
git checkout -b personal-loot 7f12e89ee5f467a50e62eba1d525eac7dc953d03
git clone https://github.com/VoxPhantom98/mod-personal-loot.git modules/mod-personal-loot
```

If you want Playerbots, add it **before** running the installer:

```sh
git clone https://github.com/mod-playerbots/mod-playerbots.git modules/mod-playerbots
git -C modules/mod-playerbots checkout 7bae1b5c58c76a0aa20381155edc08096d1485b2
```

4. From the core directory, validate and apply the patches:

```sh
python modules/mod-personal-loot/tools/install.py --core . --dry-run
python modules/mod-personal-loot/tools/install.py --core .
```

On Windows, `py -3` can replace `python`. The optional Playerbots patch is checked and applied when
`modules/mod-playerbots` exists. All patches are checked before any are applied. Already applied patches
are recognized. Save the core changes in your own Git branch before upgrading the core.

5. Import [personal_loot.sql](data/sql/db-characters/base/personal_loot.sql) into your **character database**.
   Substitute your connection details and character database name; do not import it into the world or auth database.
   For the MySQL interactive client:

```sql
USE acore_characters;
SOURCE /absolute/path/to/azerothcore-wotlk/modules/mod-personal-loot/data/sql/db-characters/base/personal_loot.sql;
```

On Windows use forward slashes in the `SOURCE` path. The migration creates module tables and can be run
again. Keep these tables in your regular character-database backups.

6. Reconfigure and rebuild AzerothCore with your usual platform build procedure. Install the resulting
   `worldserver` and its required libraries into the stopped server's runtime directory.
7. Copy `conf/mod_personal_loot.conf.dist` to the runtime file
   `configs/modules/mod_personal_loot.conf`. Edit that active `.conf` file, not only the template.
8. Select a configuration below and start the test server. Confirm the personal-loot startup message and
   check the log for database/schema errors before inviting players.

The core discovers the module's `src` scripts during CMake configuration. Adding the module requires
reconfiguration, not just restarting an old binary. For platform prerequisites and general build commands,
use your core fork's build documentation.

## Recommended durable configuration

This enables personal items and gold, boss chests, and recovery for all eligible unclaimed rewards:

```ini
PersonalLoot.Enable = 1
PersonalLoot.AllowExperimental = 1
PersonalLoot.PersonalGold = 1
PersonalLoot.BossChests = 1
PersonalLoot.GenerationJournal = 1
PersonalLoot.RecoveryMode = 1
PersonalLoot.RecoveryGold = 1
```

Also set this in **worldserver.conf** before restarting:

```ini
CharacterDatabase.WorkerThreads = 1
```

The journal requires personal gold and one asynchronous character-database worker. This release is for
one worldserver owning the character database; shared-database multi-worldserver deployments are unsupported.
Without the journal, unclaimed personal results can be lost on despawn or restart.

To include bots, outdoor treasure chests, or area looting, opt in separately:

```ini
PersonalLoot.EnableForPlayerBots = 1
PersonalLoot.WorldChests = 1
PersonalLoot.AreaLoot = 1
PersonalLoot.AreaLootRange = 30
```

## All configuration options

Defaults are deliberately conservative. Both activation gates must be enabled.

| Option (`PersonalLoot.` prefix) | Default | Effect |
| --- | --- | --- |
| `Enable` | `0` | Enables personal loot |
| `AllowExperimental` | `0` | Explicit second activation gate |
| `EnableForPlayerBots` | `0` | Includes eligible bot characters |
| `BossChests` | `1` | Personal loot for encounter-owned boss chests |
| `WorldChests` | `0` | Personal loot for supported outdoor treasure chests |
| `AreaLoot` | `0` | Collect nearby corpse items and gold on opening loot |
| `AreaLootRange` | `30.0` | Yards; clamped to 5–50; at most 50 nearby corpses per opening |
| `PersonalGold` | `0` | Separate gold rolls; off retains stock shared gold; required by the journal |
| `GenerationJournal` | `0` | Durable exact rewards, guarded claims, and recovery mail |
| `RecoveryMode` | `1` | `0` off, `1` all eligible items, `2` quality threshold, `3` Postmaster-inspired |
| `RecoveryMinQuality` | `3` | Minimum item quality for modes 2 and 3 |
| `RecoveryGold` | `1` | Mail unclaimed gold in modes 1 and 2; does not turn ordinary gold looting on/off |
| `RecoveryInstancesOnly` | `0` | Restrict modes 1 and 2 to dungeon/raid sources |

Quality values: `0` poor, `1` common, `2` uncommon, `3` rare, `4` epic, `5` legendary,
`6` artifact, `7` heirloom.

**Restart required:** `GenerationJournal`. With the journal enabled, restart to change `Enable`,
`AllowExperimental`, `PersonalGold`, `BossChests`, or `WorldChests`. Bot inclusion and area-loot settings
can be reloaded; existing generated recipients and rolls stay frozen. Recovery settings can be reloaded
and affect existing unclaimed rewards too. Restarting after configuration edits is the simplest workflow.

For rare-or-better recovery, including gold:

```ini
PersonalLoot.RecoveryMode = 2
PersonalLoot.RecoveryMinQuality = 3
PersonalLoot.RecoveryGold = 1
```

For the Postmaster-inspired profile:

```ini
PersonalLoot.RecoveryMode = 3
PersonalLoot.RecoveryMinQuality = 3
```

Mode 3 recovers qualifying dungeon/raid items and excludes quest items, bind-on-equip items, currency
tokens, and gold. It is a configurable approximation, not a promise of exact modern retail behavior.
Mode 1 ignores the quality threshold. Mode 0 pauses automatic mail without deleting journal rewards.
Items excluded by filters remain saved; relaxing filters later may mail old rewards. Already sent mail
is unaffected.

## Recovery and operational limits

Recovery runs in bounded batches after a source disappears or the server restarts. Quest and unique-item
eligibility can hold rewards until their owner is online and eligible. Full bags do not turn an item into
a successful inventory claim. Recovery mail preserves saved item properties and splits attachments/gold
into valid mail batches.

An uncertain database claim can disconnect the affected character to prevent a later stale character save
from overwriting the durable outcome. Investigate the database log before reconnecting. Compatibility with
third-party modules that directly write character inventory/money or alter reward transactions needs testing.

There is no automatic journal-retention cleanup or administrator reward-editing command in v0.1.0.
Monitor table growth and back up all module tables. Do not manually delete claim/history rows to silence logs:
they participate in duplicate prevention. Player deletion of received mail still uses normal server behavior.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Missing `configs/modules/mod_personal_loot.conf` | Copy the template into the runtime config directory |
| Personal loot disabled at startup | Both activation gates; active config location; rebuilt binary |
| Journal refuses activation | Character SQL installed, personal gold enabled, one character DB worker |
| Players still roll on outdoor chests | `WorldChests=1`, restart, and supported chest type |
| Boss has little/no gear | Existing loot table probabilities still apply; independent loot is not guaranteed loot |
| Recovery does not mail an item | Recovery mode, quality/scope filters, source lifecycle, and quest/unique eligibility |
| Installer rejects checkout | Use pinned revisions and clean patch targets; never force through a conflict |
| Old JSON parsing errors | Rebuild with the supplied complete patch and verify character SQL; preserve affected rows |

Dungeon Finder completion and bonus rewards remain governed by the core's encounter and queue rules.
Personal loot does not change which boss completes a dungeon.

## Verification and reporting

The development server compiled with the patched core and Playerbots. Focused transaction tests and
disposable-database checks covered rollback, duplicate claims, prepared-result types, mail atomicity, and
a lost COMMIT acknowledgement. In-game testing reported working open-world loot and encounter-owned
chests. This remains experimental; these checks do not establish compatibility with every custom module.

Run the portable transaction test after applying the core patch:

```sh
cmake -S modules/mod-personal-loot/tests -B build-personal-loot-tests
cmake --build build-personal-loot-tests --config Release
ctest --test-dir build-personal-loot-tests -C Release --output-on-failure
```

When reporting a problem, include core/bot commits, enabled options, loot source entry/type, reproduction
steps, and redacted relevant logs. Never upload connection strings, passwords, database dumps, or player data.

## Updating and rollback

Stop the server and back up the character database, executable, configuration, and source before changing
versions. Preserve the journal and claim tables. Do not restore an older database independently of its matching
server state after players have received rewards; that can undo claim history.

To return to the prior server, restore the matching backed-up binary and configuration. Source rollback
requires removing this module and reversing its patches on an otherwise matching checkout, then rebuilding.
Keep the additive module tables for recovery/audit planning. Disabling the feature does not collect outstanding
rewards automatically. Resolve outstanding rewards before a permanent uninstall.

## License and acknowledgements

Released under [GNU AGPL v3](LICENSE). Patches preserve notices in modified upstream files.
AzerothCore and the Playerbots community provide the underlying server, loot tables, and bot integration.
Voxville supplied the feature requirements and playtesting. Built-in area looting is part of this implementation;
the external `mod-aoe-loot` module is not included. No World of Warcraft client assets are distributed here.
