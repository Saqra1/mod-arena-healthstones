# Arena Healthstones and Refreshments

Automatically supply each arena team during preparation: Healthstones when a warlock is present,
and mage refreshments when a mage knows Conjure Refreshment or Ritual of Refreshment.

- Includes the warlock and all teammates, in rated arenas and skirmishes of any team size.
- Selects the highest learned Create Healthstone rank among participating warlocks on that arena side.
  At equal ranks, selects the strongest active Improved Healthstone talent.
- Gives each teammate up to one stack of 20 refreshments, counting any already carried of the selected type.
  Selects the strongest usable type across the team's mages, respecting the recipient's item requirements:
  Mana Biscuits (Ritual rank 1), Mana Pies (Conjure rank 1), or Mana Strudels (rank 2 of either spell).
  Lower ranks must also be learned; separate Conjure Food and Conjure Water spells are not included.
- Uses arena sides, including matches between teams of the same faction.
- Rechecks when players enter and once per second during preparation, upgrading previously supplied stones
  or supplying stronger refreshments when another caster arrives. Distribution stops when the match starts.
- Supplies one stone per player. Existing stones of the selected type satisfy delivery; consumed, traded,
  or deleted stones are not replenished. Only stones supplied by this module are removed during upgrades.
- Refreshments are supplied once per strength during preparation. Consumed, traded, or deleted food is not
  replenished. Stronger refreshments can be supplied later; lower types already carried are left untouched.
- Full bags delay initial delivery until space is available during preparation. Upgrades reuse the old
  stone's slot, after validating storage and creating its replacement. Stones offered in trade wait until
  the trade ends.
- Each supply requires a participating teammate who knows its creation spell. Battlegrounds and spectators are excluded.

## Installation

Place this directory at `modules/mod-arena-healthstones`, then configure and build AzerothCore with modules
enabled using your normal workflow. The module loader and sources are discovered automatically.
No SQL or configuration changes are required.

The module creates normal conjured items and leaves their use and arena cleanup to the core. Healthstone
selection follows the warlock's rank; in mixed-level teams it can exceed a teammate's level. Refreshments
instead fall back to the highest learned type the recipient can use, or provide nothing if none is usable.

## Manual verification

1. Enter an arena with one warlock. Verify every teammate receives the warlock's highest learned stone,
   including the warlock, and the opposing side receives none unless it also has a warlock.
2. Enter with two warlocks in both arrival orders. Check different spell ranks and equal ranks with different
   Improved Healthstone talents. Previously supplied stones should upgrade without accumulating extra copies.
3. Repeat with teams of the same faction, a skirmish, and another arena size.
4. Fill a player's bags before entry, then free a slot during preparation. Verify delivery retries successfully.
   Fill the bags again, then bring in a stronger warlock. Verify the upgrade reuses the supplied stone's slot.
5. Delete or trade a supplied stone. Verify it is not repeatedly supplied at the same strength.
6. After the gates open, verify no further deliveries or upgrades occur. Check that normal battlegrounds,
   spectators, and teams without a qualifying warlock receive no Healthstones.
7. Test mages knowing only Ritual rank 1, only Conjure rank 1, and rank 2 of either spell. Verify teammates
   receive 20 biscuits, pies, or strudels respectively, and the opposing side receives nothing without a mage.
8. Test mixed-level teammates and multiple mages in both arrival orders. Check learned lower-rank fallbacks,
   late stronger refreshments, and no delivery when the recipient cannot use any available type.
9. Carry fewer than 20 refreshments, then enter: verify only the missing count is added. With 20 or more,
   verify no extras. Consume or trade food afterward and verify no refills. Check retries with full bags,
   healthstones and refreshments together, and no food delivery after the gates open.

## License

GPL-2.0-or-later, matching AzerothCore.
