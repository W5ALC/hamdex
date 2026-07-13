# HamDex

A fast, offline-first ham radio callsign lookup tool for Linux. HamDex keeps a local
copy of the FCC's Universal Licensing System (ULS) amateur radio database and lets you
search it instantly — no network round-trip per lookup, no rate limits, no ads.

Written in C with GTK4 and SQLite. Originally started life as a Python/PyQt6 app,
rewritten in C for speed and a smaller footprint.

## Features

- **Callsign lookup** — instant local lookup against the full FCC amateur database
  (name, address, license class, grant/expiry dates, previous callsign, club trustee
  info, FRN, and more)
- **FCC Database tab** — search/filter the raw ULS data by name, callsign, address,
  city, state, class, or status; double-click a result to look it up
- **Daily / Weekly FCC data import** — pulls the official ULS zip files directly from
  `data.fcc.gov` and imports them into the local SQLite database. Daily files are
  incremental; the weekly file is a full rebuild. You can also import an already-
  downloaded zip from disk.
- **Bookmarks & lookup history** — save callsigns you care about, and the last 100
  lookups are kept automatically
- **Per-callsign notes** — jot down a note on any callsign, it's saved locally
- **Dark/light theme + adjustable font size**
- **Export** — bookmarks/lookup results to CSV or JSON

## Requirements

Build dependencies (Debian/Ubuntu package names — adjust for your distro):

```bash
sudo apt install build-essential pkg-config \
    libgtk-4-dev libjson-glib-dev libsqlite3-dev libcurl4-openssl-dev
```

Everything else (GLib, Pango, Cairo, etc.) comes along as GTK4's own dependencies.
ZIP reading is handled by a vendored copy of [miniz](https://github.com/richgel999/miniz)
under `third_party/`, so there's no separate zip library to install.

Tested against GTK 4.14 on Ubuntu 24.04 (Noble). Should build fine on any reasonably
current distro with GTK4 dev packages available.

## Building

```bash
git clone https://github.com/w5alc/hamdex hamdex
cd hamdex
make
```

This produces a `hamdex` binary in the project root.

Other targets:

```bash
make run        # build (if needed) and run straight from the source tree
make clean      # remove build artifacts (.o/.d files and the binary)
make install    # install to $PREFIX/bin (default /usr/local/bin), see below
make uninstall  # remove it again
```

## Installing

```bash
make
sudo make install
```

This installs the binary to `/usr/local/bin/hamdex` by default. To install
somewhere else (a user-local prefix, a packaging staging directory, etc.), override
`PREFIX` and/or `DESTDIR`:

```bash
# install under ~/.local instead of /usr/local (no sudo needed)
make install PREFIX=$HOME/.local

# stage into a packaging root without touching the real filesystem
make install DESTDIR=/tmp/pkgroot PREFIX=/usr
```

`make uninstall` respects the same `PREFIX`/`DESTDIR` you installed with.

**Optional: add a desktop launcher.** Create
`~/.local/share/applications/net.n5rr.hamdex.desktop`:

```ini
[Desktop Entry]
Type=Application
Name=HamDex
Comment=Offline FCC amateur radio callsign lookup
Exec=/usr/local/bin/hamdex
Icon=utilities-terminal
Terminal=false
Categories=Network;HamRadio;
```

(Adjust the `Exec=` path if you installed with a different `PREFIX`, and swap the
`Icon=` line for a real icon if you make one — there's no bundled icon yet.)

## First run

On first launch HamDex creates `~/.config/hamdex/` with:

- `hamdex.db` — your local database: the imported FCC data, bookmarks, notes, and
  lookup history
- `settings.ini` — theme, font size, and other UI preferences

**The callsign database starts empty.** Go to the **FCC Database** tab and click
**Update Daily** (fast, small file) or **Full Weekly** (complete rebuild, much
larger — currently around 150–200MB compressed) to populate it before Lookup will
find anything. After that, run Daily periodically to stay current, and Weekly
occasionally to fully resync.

Note: `data.fcc.gov` is documented to be intermittently slow or unresponsive
(this is a known issue with their server, not something wrong on your end). The
download logic retries automatically a few times before giving up — if it still
fails, it's usually worth just trying again in a few minutes.

## Project layout

| File | What it does |
|---|---|
| `main.c` | Entry point, `GtkApplication` lifecycle |
| `hamdex_app.h` | Shared `HxApp`/`CallsignData` structs and function declarations |
| `db.c` | SQLite storage for bookmarks, notes, and lookup history (`hamdex.db`) |
| `fcc_db.c` | The imported FCC ULS database: schema, import, search, single-callsign lookup |
| `fcc_parse.c` | Parses the pipe-delimited `EN`/`HD`/`AM` ULS record types out of the raw data files |
| `fcc_urls.c` | Builds the daily/weekly download URLs |
| `download.c` | libcurl wrapper with retry/backoff for pulling the ULS zip files |
| `zip_util.c` | Pulls a named `.dat` file out of the downloaded zip (via miniz) |
| `ui_core.c` | Theme, settings persistence, status bar, the lookup details panel |
| `ui_build.c` | Builds the whole window: tabs, panels, menus, export |
| `ui_lookup.c` | Background-threaded callsign lookup against the local FCC DB |
| `ui_fcc_tab.c` | FCC Database tab: search, filters, results table, import wiring |
| `third_party/miniz.c` | Vendored single-file zip library |

## Known issues / TODO

- The Settings tab has a **"Home Grid"** field (Maidenhead grid square) left over
  from a planned distance/bearing feature. The feature was never wired up to a real
  data source and has since been removed from the display code — the field still
  saves/loads but does nothing useful right now. Either finish wiring it up (would
  need a callsign → grid-square geocoding step, since the FCC ULS data doesn't
  include coordinates) or remove the field from the Settings tab.
- Several GTK APIs used here (`GtkTreeView`, `GtkListStore`, `GtkCellRenderer`) are
  deprecated in GTK4 in favor of `GtkColumnView`/`GListStore`. They still work fine
  and produce build warnings, not errors — a future cleanup could migrate the
  results tables over.
- No `make install` target or packaging (deb/rpm/flatpak) yet.
- No app icon.

## Updating your own fork/release

If you're maintaining your own copy of this:

1. Pull/merge upstream changes into your source tree.
2. `make clean && make` and confirm it builds warning-free (aside from the known
   GTK deprecation warnings above).
3. Bump the `HAMDEX_VERSION` string if you're tagging a release — it's not passed
   in via the Makefile, so it's a `#define` somewhere in `app.h` (the one header in
   this project that hasn't come up in review yet).
4. Sanity-check a Daily import and a couple of lookups before calling it good — the
   FCC data format has had subtle column-layout differences between file types
   before, and a parsing regression there fails silently (rows just come back empty)
   rather than crashing.

## License

*(Not yet decided — add your preferred license here, e.g. MIT/GPL-3.0, before
publishing a release.)*

## Credits

- FCC ULS data: [Federal Communications Commission](https://www.fcc.gov/uls) —
  public domain, updated by the FCC on their own daily/weekly schedule
- ZIP handling: [miniz](https://github.com/richgel999/miniz) (vendored)
- Built with [GTK4](https://www.gtk.org/) and [SQLite](https://sqlite.org/)
- Maintained by Jon, N5RR
