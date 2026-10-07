# Contributing

Start from the revisions in `compatibility.json`. Changes to core code must be delivered in the core patch;
module-only changes cannot silently depend on unpublished core changes. Refresh the patch hash when updating
a patch. Preserve upstream copyright and license notices.

Describe the user-visible problem and reproduction steps. For reward changes, explain generation, ownership,
inventory/gold claims, recovery, and failure behavior. Test transaction outcomes and relevant gameplay before
proposing a release. Avoid changes to unrelated loot probabilities or Dungeon Finder completion rules.

Keep credentials, realm dumps, player data, binaries, and live configuration out of commits and issues.
