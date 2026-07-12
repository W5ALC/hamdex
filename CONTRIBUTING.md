# Contributing to HamDex

Thanks for taking an interest in this. It's a small hobby project, so this doc is
short on purpose — mostly just enough to keep changes consistent and to save you a
round-trip on the stuff that isn't obvious from the code.

## Getting set up

See the README for build dependencies and build instructions. Short version:

```bash
sudo apt install build-essential pkg-config \
    libgtk-4-dev libjson-glib-dev libsqlite3-dev libcurl4-openssl-dev
make
./hamdex
```

## Before you open a PR

- **It has to build clean.** `make clean && make` shouldn't produce any new
  warnings beyond the existing GTK deprecation ones (`GtkTreeView`,
  `GtkListStore`, `GtkCellRenderer` — see Known Issues in the README). If your
  change adds a new warning, fix it before submitting.
- **Test against a real FCC import if you touch the parsing/import path.**
  `fcc_parse.c`, `fcc_db.c`, and `download.c` all deal with the FCC's
  pipe-delimited ULS format, which is finicky — a subtly wrong column index
  doesn't crash, it just silently returns empty fields. Run an actual Daily
  import and spot-check a few known callsigns in the Lookup tab before and after
  your change.
- **Check for use-after-free / double-free if you touch ownership-heavy code.**
  This codebase leans on GLib's `GHashTable`/`GPtrArray` destroy-notify callbacks
  a lot (see `db.c`, `fcc_db.c`, `fcc_parse.c`), and it's easy to end up with two
  different code paths both thinking they own the same free. If you're not sure,
  build and run with AddressSanitizer:

  ```bash
  make clean
  CFLAGS="-fsanitize=address -g" make
  ./hamdex
  ```

  Exercise whatever you changed (a lookup, an import, opening/closing dialogs,
  etc.) and watch for ASan output on exit.

## Code style

Nothing enforced by a linter — just match what's already there:

- 4-space indentation, no tabs
- Opening brace on the same line as the function/`if`/`for`
- `snake_case` for functions and variables, `PascalCase` for typedef'd structs
  (`HxApp`, `CallsignData`, `FccRecord`, ...)
- Public functions used across files get an `hx_` prefix (`hx_perform_lookup`,
  `hx_display_callsign`); file-local helpers are `static`
- GLib types and idioms throughout (`GPtrArray`, `GHashTable`, `g_strdup`, etc.)
  rather than raw libc string handling — stay consistent with that rather than
  mixing in `strdup`/`malloc` directly

## Where things live

The README has a full file-by-file breakdown under "Project layout." The rough
mental model:

- `db.c` — *your* data (bookmarks, notes, history). Small, safe to touch.
- `fcc_db.c` / `fcc_parse.c` / `fcc_urls.c` / `download.c` / `zip_util.c` — the FCC
  data pipeline: fetch → unzip → parse → store. Higher blast radius; changes here
  affect every lookup.
- `ui_*.c` — GTK widgets and wiring. `ui_build.c` constructs everything,
  `ui_core.c` handles theme/settings/the details panel, `ui_lookup.c` and
  `ui_fcc_tab.c` handle their respective tabs' logic.

If a change spans both a data-layer file and a UI file, that's normal — just keep
the PR focused on one feature/fix rather than bundling unrelated changes.

## Reporting bugs

Open an issue with:

- What you did, what you expected, what happened instead
- Terminal output if it crashed (run `./hamdex` from a terminal, not a desktop
  launcher, so you actually see stderr)
- Whether it's reproducible, and with what data (e.g. "happens after importing
  the daily file" vs. "happens on any lookup")

If it's a crash, a stack trace helps a lot more than a description. If you can
reproduce it, a debug build is easy:

```bash
CFLAGS="-g -O0" make clean all
gdb ./hamdex
```

## Commit messages

No strict format required. A short summary line plus, if it's not obvious, a line
or two on *why* (not just what — the diff already shows what). If you're fixing a
bug, mention how you confirmed the fix actually works, especially for anything
threading/memory-related — "seems to work" isn't the same as verified.
