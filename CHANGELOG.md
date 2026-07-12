# Changelog

All notable changes to this project are documented here. Format loosely follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Nothing has been tagged
as a numbered release yet — everything below is grouped under Unreleased as a
starting point; rename the heading to a version number when you cut the first one.

## [Unreleased]

### Fixed
- **Crash on Daily FCC import (double free).** `fcc_parse_en/hd/am()` registered a
  plain `g_free` as each `GHashTable`'s value-destroy callback, but the values are
  structs with several owned string fields inside, not bare strings.
  `fcc_en/hd/am_table_free()` then manually freed every field *and* the struct,
  and afterward called `g_hash_table_destroy()` — which ran `g_free` again on the
  same already-freed pointers. This corrupted the heap and aborted the process,
  almost always on the Daily import path specifically (small file, so it reached
  the teardown code fast) rather than Weekly (large enough to usually hit other
  issues first). Fixed by registering the real per-row free function as each
  table's value destructor at creation time, so there's a single owner of that
  cleanup logic. Reproduced and confirmed under AddressSanitizer before and after
  the fix.
- **Full Weekly import failing with "Could not connect: Timeout was reached."**
  `data.fcc.gov` is documented to be intermittently slow/unresponsive (independent
  reports of it hanging for tens of minutes at a time). The download logic now
  retries automatically (up to 4 attempts, 5s backoff, cancellable) on transient
  failures — timeouts, connection resets, 5xx responses — instead of failing on
  the first hiccup. The old fixed 300s total-transfer cap was also replaced with
  stall detection (abort only if the transfer is under 1 byte/sec for 90s
  straight), since a healthy-but-slow ~100MB+ transfer could legitimately take
  longer than 5 minutes. Verified against a local server that simulates the same
  failure pattern (connection reset → 500 → success).
- **Font size setting not applied on startup.** The saved font size was only
  applied when the settings spin button's `value-changed` signal fired, but the
  initial value was set *before* the signal handler was connected — so the saved
  size silently never took effect until you manually touched the control. Font
  size is now folded into the same theme CSS provider used for dark/light mode,
  which is (re)applied once at startup after settings load.
- Wasted duplicate query in `fccdb_import_zip_bytes()`: `fccdb_get_record_count()`
  ran a full query whose result was immediately discarded and recomputed with a
  direct `COUNT(*)`. Removed the discarded call.
- Redundant `g_snprintf()` call in the FCC Database tab's result-count label —
  formatted once, then immediately overwritten in the common case.

### Added
- FCC Database tab's filter fields (Name/Callsign/Class/Status/Address/City) now
  stretch to fill the available width instead of clustering on the left with a lot
  of dead space; State stays fixed-width as a 2-letter box.
- Lookup tab detail cards expanded to show more of what's actually in the local
  FCC record: Grant Date, Previous Callsign, Trustee (club calls), and FRN, in
  addition to the existing License Class, Address, and License Expiry.

### Removed
- Coordinate/grid-square scaffolding that was never actually wired up: the
  `has_coords`/`latitude`/`longitude`/`grid_square` fields on `CallsignData`, the
  Coordinates and Grid Square info cards, the corresponding CSV/JSON export
  columns, and `grid.c`/`grid.h` entirely (nothing else referenced them once the
  display code was gone). None of this was ever populated by the live lookup
  path, so it was always empty in practice.
- Entity Type and Vanity Group info cards on the Lookup tab (kept on the FCC
  Database tab's results table, which has its own separate mapping — only the
  per-callsign detail cards were removed).

### Known issues (not yet fixed)
- Settings tab's "Home Grid" field is now dead weight — it still saves/loads but
  nothing reads it since the grid-square feature above was removed. See README
  "Known issues" for options.
- `GtkTreeView`/`GtkListStore`/`GtkCellRenderer` usage triggers GTK4 deprecation
  warnings at build time. Cosmetic for now; a future pass could migrate to
  `GtkColumnView`/`GListStore`.
