# Changelog

All notable changes to the project are recorded in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and the project follows [Semantic Versioning](https://semver.org/) (the `VERSION`
file at the repository root).

## [Unreleased]

## [1.7.4] — 2026-07-20

### Changed

- Fixed a broken relative link in `docs/ADR-0001-desktop-maitre-esp32-satellite.md`:
  `ARCHITECTURE.md` now points to `fr/ARCHITECTURE.md`.
- Updated user-facing changelog wording to use canonical production naming.

## [1.7.3] — 2026-07-19

### Changed
- **Copie vendorée de morfBeacon resynchronisée en 0.2.0** (champ `capabilities`
  du heartbeat). Ajout purement additif et facultatif : ce projet n'annonce
  aucune capacité et son comportement est strictement inchangé. La
  resynchronisation évite que la copie embarquée ne dérive de l'amont.
- **`scripts/sync-morf.sh` : résolution du dépôt source corrigée.** Le script
  cherchait exclusivement `morfBeacon` / `morfUpdate` et échouait donc sur une
  organisation où les clones portaient un suffixe de développement — c'est-à-dire
  qu'il ne fonctionnait tout simplement pas. Il accepte désormais les deux conventions.

  morfUpdate reste en 0.1.0, déjà aligné sur l'amont.

## [1.7.2] — 2026-07-15

### Changed
- The synchronization hub was renamed **HomeServerHub → morfSync**. Default hub
  address is now `http://morfsync.local:8080` (was `homeserverhub.local`). Only
  a default/placeholder — an already-configured address in Settings is untouched.
  Docs updated (`docs/fr/SYNCHRONISATION.md`).

## [1.7.1] — 2026-07-15

### Fixed
- **Synchronization now recovers automatically if the hub's journal is reset**
  (its data folder moved or wiped). The client tracks the hub's journal identity
  (`journalId`); when it changes, it **resets its cursor and re-synchronizes
  fully**, instead of silently missing changes. Fixes the case where a deletion
  made on one machine wasn't propagated to the others after the hub's data was
  relocated. Requires morfSync ≥ 0.2.5.

## [1.7.0] — 2026-07-15

### Added
- **LAN synchronization with morfSync (offline-first).** ComponentHub can now
  keep the same data across several machines through a lightweight local-network
  hub, without any cloud. The **local database stays sovereign**: the app always
  works on its local copy; the hub is only used to reconcile copies when reachable.
  Configure the hub address in **Settings → Synchronization**, then:
  - **automatic sync at startup** if the hub is reachable (short, non-blocking probe
    with on-screen feedback so it never feels frozen);
  - **on-demand** button ("Synchroniser maintenant");
  - **sync proposed on quit** — both automatic triggers are toggleable;
  - an explicit **"local only"** mode that disables all synchronization.
  See [docs/fr/SYNCHRONISATION.md](docs/fr/SYNCHRONISATION.md) *(FR)*.
- **Last-sync metrics** in Settings (date + items received/sent).
- **Configuration backup/restore**: the full `.tar` backup now includes the
  configuration; a new "Configuration only" backup/restore exports just the
  settings (hub address, options, appearance) as JSON, to replicate them on
  another machine.

### Changed
- **Permanent identity is now a UUID** for every entity (was an auto-increment
  integer). Required for conflict-free identity across machines; internal
  references (location, project, BOM links, document owners) were migrated too.
  Deletion is now **logical (tombstone)** so it propagates through sync.
- **Migration note:** an existing integer-id database is not auto-converted.
  Start from a fresh database and re-import your data (Bomist or native CSV).
- Synchronization is **incremental**: only entities changed since the last push
  are sent (scales to large inventories).

## [1.6.0] — 2026-07-13

### Added
- **LAN supervision (morfBeacon) and update check (morfUpdate).** ComponentHub now
  announces its presence on the local network (UDP heartbeat, port 45454) and
  exposes live metrics over a small local HTTP endpoint (`/status`, port 8787), so
  it can be watched from a central dashboard (RaspberryDashboard). It also checks
  GitHub Releases for a newer version — silently at startup, and on demand via
  **Help → "Check for updates…"**. Both are shared modules vendored under
  `third_party/morf/` (compiled into the binary, no external dependency). See
  [docs/fr/SUPERVISION_ET_MAJ.md](docs/fr/SUPERVISION_ET_MAJ.md) *(FR)*.

## [1.5.4] — 2026-07-12

### Changed
- **Dropped the "business core shared with the ESP32" wording** from source-file
  headers, user-facing strings and docs. Since the split, the desktop keeps its
  own copy of `src/domain/` and evolves independently, so claims of a *shared*
  core/logic or a *common* file format no longer apply. References that document
  the *separation itself* (satellite role, separate repository, ADR-0001) are
  kept on purpose. Also removed stray embedded-only mentions (LittleFS,
  ArduinoJson, `/api/files/*`) from domain comments.
- **Data-folder paths in the docs made universal**: the account name is now shown
  as a `<compte>` placeholder, with a note clarifying that `morfredus` is the
  application's *publisher* folder (the same on every machine, from the app's
  organization name), not the user's account name.

## [1.5.3] — 2026-07-12

### Changed
- **Documentation overhaul and internationalization.** By GitHub convention, the
  root documents are now in **English** (`README.md`, `CHANGELOG.md`,
  `ROADMAP.md`); a French `README.fr.md` is kept for French speakers. The
  in-depth guides moved under `docs/fr/` (French, the reference language), with
  an English index at `docs/en/README.md` linking to them until translated.

### Added
- **`CONTRIBUTING.md`** — project philosophy, the layered domain/storage/UI
  separation, and a strict **portability policy**: cross-platform libraries
  only, no OS-proprietary dependency, and no `#ifdef _WIN32 / #else / #endif`
  for behavior (the sole exception is isolated in `src/platform/` and must render
  identically on the three OSes).
- **`docs/fr/GETTING_STARTED.md`** — a step-by-step, beginner-friendly guide to
  clone, install the tools, build and run on Windows, Linux and Raspberry Pi.
- **`docs/fr/USER_MANUAL.md`** — a beginner's user manual for the application.
- **`docs/fr/ARCHITECTURE.md`** — the desktop app's layered architecture and how
  to read the code.

## [1.5.2] — 2026-07-12

### Fixed
- **Debian package (`scripts/linux/package-deb.sh`)**: creating the `.desktop`
  shortcut failed with "No such file or directory" because
  `usr/share/applications/` was not created before the `>` redirection. Added an
  explicit `install -d`. The `.deb` builds again (tested on Raspberry Pi,
  ARM64).

## [1.5.1] — 2026-07-12

### Added
- **Application screenshots** in the documentation: Inventory view, component
  sheet, Projects (BOM) and Import/Export in the README; the Settings screen in
  `docs/fr/BUILD_DESKTOP.md`. Images under `docs/pictures/`.

## [1.5.0] — 2026-07-12

### Added
- **Native menu bar** (File · Go to · View · Help), in addition to the sidebar —
  for mouse **and** keyboard access, in line with desktop conventions.
  - **File**: *Open the data folder*, *Quit* (Ctrl+Q).
  - **Go to**: jump straight to each section, shortcuts **Ctrl+1 … Ctrl+7**.
  - **View → Theme**: System / Light / Dark, **synchronized** with Settings and
    the OS theme.
  - **Help**: *Help* (**F1**) — a "Getting started" guide (sections, shortcuts,
    data location); *About* (version, Qt version, GPL-3.0-only license); *About
    Qt*.
- **Version shown in the window title** ("ComponentHub 1.5.0"), read from CMake
  (`CH_APP_VERSION`) — no hard-coded value.
- **QSS styles** for `QMenuBar` / `QMenu` (matching the light and dark themes).

## [1.4.2] — 2026-07-12

### Changed
- **`ROADMAP.md` rewritten from the desktop-master viewpoint**: "Already
  shipped" now reflects the Qt application (not the embedded PROGMEM/LittleFS
  firmware); a new **"Master ↔ satellite link"** section groups the desktop ↔
  ESP32 API work; the long-term vision is recentered (JSON → SQLite persistence
  on the desktop).
- **`docs/fr/BUILD_DESKTOP.md`**: the "core shared with the ESP32" wording is
  corrected — since the split, the desktop keeps **its own copy** of
  `src/domain/` (the `.tar` format remains interchangeable for now).

## [1.4.1] — 2026-07-12

### Changed
- **Desktop repository consolidation: the firmware-specific documentation moved
  to the `ComponentHub-ESP32` repository** (`ARCHITECTURE.md`, `API.md`,
  `BOOT_LOG.md`, `WIFI_SETUP.md`, `pictures/` — screenshots of the embedded web
  UI). The desktop repository keeps only the documents that concern it:
  `BUILD_DESKTOP.md` and the architecture decision `ADR-0001`. `README` and
  `ROADMAP` links updated accordingly.

## [1.4.0] — 2026-07-12

### Changed
- **The ESP32 firmware is split into a standalone repository — the desktop app
  becomes the master project, the ESP32 a "satellite".** Full decision in
  `ADR-0001`. In short: the workshop database cannot stay held by the ESP32-S3
  (constrained RAM/flash, growth over several years) and expanding it via an **SD
  card is ruled out for reduced reliability** (corruption, loose contacts,
  wear). The **reference database therefore lives on the desktop** (reliable
  PC/RPi storage, capacity and evolution — JSON → SQLite — unconstrained). The
  **ESP32 becomes a mobile terminal** for browsing/editing stock (QR scanning)
  that will eventually **read and update the desktop database**. Concretely, the
  firmware is extracted to the sibling repository `ComponentHub-ESP32`, with
  **its own copy** of `src/domain/`; the two projects now evolve separately. The
  JSON / `.tar` format stays interchangeable between board and PC. Both builds
  are verified (desktop CMake → `ComponentHub.exe`, firmware PlatformIO →
  `firmware.bin`).
- **Repository reorganization: the desktop application becomes the main project,
  at the root.** The `src/domain/` business core lives at the root; it was
  referenced by the ESP32 firmware (single source) until the split above.
- **Build and packaging workflow**: `CMakePresets.json` (mingw / linux /
  linux-arm64 / cross) and VS Code tasks at the root, same build commands;
  `scripts/windows/` (VS Code build, DLL deployment, **self-contained ZIP**) and
  `scripts/linux/` (**`.deb`**, **AppImage**, desktop install) scripts;
  application icon (`resources/logo.png`, `app.ico`).

### Added
- **Desktop application**: native **Qt 6 / C++17** port, buildable on
  **Windows, Linux (x86_64) and Raspberry Pi (ARM64)** via CMake, freed from the
  ESP32's memory constraint. Reuses the business core as-is (`src/domain/`:
  services, CSV, import/export) — only the file storage (nlohmann/json, **same
  JSON format as the ESP32**) and the Qt interface are specific. Screens:
  Inventory (search/filters/full sheet with documents and stock movements),
  Categories, Locations (hierarchy), Projects (BOM + missing), Import/Export (CSV
  per table + `.tar` backup **interchangeable with the ESP32**), Settings.
  Light/dark theme following the OS, tinted vector icons, correct accents and
  "€" (UTF-8).

### Added (desktop UI)
- **Administrable reference lists** (`Reference lists` in the navigation):
  normalized value lists (Component types, Manufacturers, Packages, Suppliers,
  Technologies, States, Keywords) for a consistent nomenclature. Per list: add,
  rename, delete, reorder, **merge** duplicates, import/export as CSV. For lists
  **bound to a field** (type, manufacturer, supplier, state), rename and merge
  **propagate the update to the affected components**. The component sheet's
  *Type* field is offered from the reference list with **inline add** ("type X
  does not exist, add it?"). The `referentials.json` file is included in the full
  `.tar` backup.
- **Global search**: the inventory search now sweeps **all** text fields
  (reference, designation, type, manufacturer, characteristics, supplier,
  notes…) and handles several words (logical AND), without picking a field.

### Fixed (desktop UI)
- **Inventory**: column sorting on header click, with correct **numeric** sort
  for Qty and Price ("10" after "9", price compared on value not text).
- **Component sheet**: the +/− buttons of the spin boxes (quantity, thresholds,
  price) get an **enlarged, clearly delimited click area** — the default Qt
  rendering, degraded as soon as a stylesheet targets a `QSpinBox`, produced tiny
  misaligned buttons. Full-height buttons + tinted PNG arrows.

## [1.3.0]

### Added
- **Export/import of secondary tables**: categories, locations (with the
  hierarchy — `parentId` + readable path "Workshop > Cabinet A > Drawer") and
  projects each get a CSV export and a CSV import that re-imports identically
  (the internal `id` is preserved, keeping cross-references intact).
- **Full database backup / restore** in a single TAR archive
  (`componenthub_backup.tar`) carrying **everything** in the workshop — all
  tables (components, categories, locations, stock movements, documents,
  projects, BOMs) **and** the uploaded files (datasheets, PDFs, photos…),
  byte for byte. The format to prefer to fully recover the workshop after an
  incident. Archive built/streamed on the fly (minimal RAM footprint, no size
  limit other than the filesystem); restore in multipart with format check and
  atomic per-file write. Restore overwrites the current data and asks for
  confirmation (atomic tmp+rename per table). New routes `/api/backup`,
  `/api/restore`, `/api/{categories,locations,projects}/{export,import}`.

### Fixed
- **Encoding of exported CSVs**: added a UTF-8 BOM at the start of the file so
  spreadsheets (Excel/LibreOffice on Windows) correctly display "€" and accented
  characters instead of garbled text. An existing BOM is ignored on re-import.

### Changed
- Top banner **sticky**: the navigation header and the inline status area
  (messages, confirmations, progress) stay pinned at the top of the window when
  scrolling. A delete confirmation triggered at the bottom of a long list is thus
  always visible, without scrolling back up. The status area no longer reserves
  space when empty (`display:none` instead of merely hidden).
- Inventory page: the toolbar (search, filters, "low stock", Add button) and the
  **column headers** also stay pinned, stacked under the banner — the list
  scrolls under always-visible markers. The banner and toolbar heights are
  measured at runtime (CSS variables `--topbar-h` / `--inv-toolbar-h`) for exact
  stacking despite their variable heights.

## [1.2.0]

Minor version consolidating into one stable release the changes and fixes
shipped in small touches across 1.1.1 → 1.1.10: category management, enriched
project BOM (off-inventory items, availability check, printing/PDF), navigation
rework (flat menu + tabs), dashboard, and various fixes. The per-increment detail
is below.

## [1.1.10]

### Fixed
- Inventory `⋮` action menu: now opens **upward** on the last rows when space is
  missing below — no more overflow at the bottom of the page or spurious
  scrollbar.
- Routing collision: `GET /api/projects/bom` (and `/missing`) was captured by
  the list handler `GET /api/projects` (ESPAsyncWebServer matches by prefix),
  returning the project list instead of the BOM — hence empty ("undefined") rows
  and an add/remove with no effect. Specific routes are registered before the
  generic one.

## [1.1.9]

### Fixed
- mDNS re-announced cleanly on every WiFi reconnection (`MDNS.end()` before
  `MDNS.begin()`) — a second `begin()` could leave the responder silent. Access
  via `componenthub.local` still depends on the client (see `WIFI_SETUP.md`: on
  Windows, Bonjour is often required; access by IP is always reliable).
- Embedded web assets served with `Cache-Control: no-cache`: prevents a browser
  from serving stale HTML/JS after a firmware update (stable URLs but content
  varying from one version to the next).

## [1.1.8]

### Added
- Project BOM — **off-inventory** items: add to a project an item not yet owned
  (free name), automatically counted "to buy". The add field accepts an
  inventory component (suggestions) or a new name.
- Project BOM — **availability check**: global summary and a per-row status
  column (✅ available / 🛒 to buy).
- Project BOM — **printing / PDF export**: printable page (needed components +
  "Left to buy" with estimated cost), via browser printing or "Save as PDF".

## [1.1.7]

### Changed
- **Tab** navigation: the Settings and System sections present their sub-pages
  via a persistent tab bar (injected on each page of the section, current tab
  highlighted), replacing the large buttons.

## [1.1.6]

### Changed
- Home page (the `Lab` menu) titled "Dashboard" and stripped of its redundant
  welcome card — the inventory status is shown directly.

## [1.1.5]

### Added
- `Settings` (Locations / Categories / Import-Export) and `System` (Files /
  Update / Logs, plus Debug if `ENABLE_BOOT_LOG` is active; `/system` keeps its
  system information) sections grouping their sub-pages.

## [1.1.4]

### Changed
- Main menu flattened to 5 entries (Lab, Inventory, Projects, Settings, System)
  — dropdown submenus dropped to stay navigable later with a rotary knob (see
  `ARCHITECTURE.md`, "steerable" navigation).

## [1.1.3]

### Added
- Component form — **Interface** field with common suggestions (I2C, SPI, UART,
  USB, GPIO, PWM, Analog, CAN, 1-Wire, Bluetooth, WiFi, Ethernet, RS232, RS485,
  JTAG, SWD), while remaining free-text.
- Component form — **Category** field populated from the server-managed list,
  rather than from the displayed components only.

## [1.1.2]

### Added
- Component form — editable **Location** field: text field + suggestions
  (existing paths), with on-the-fly creation of a new root location if the typed
  text matches no known path.

## [1.1.1]

### Added
- `Category` entity (flat list, no hierarchy): admin page `/categories`, API
  `/api/inventory/categories`, and automatic creation whenever a component is
  saved with an unknown category.

## [1.1.0]

### Added
- Per-row `⋮` action menu on the inventory (Edit, Movement, Documents, Photos,
  QR Code, Duplicate, Delete), replacing the 4 fixed buttons — designed to absorb
  future actions without redoing the layout. Photos and QR Code show a "coming
  soon" message (features not yet built).
- Validation status icons (🟡 to test, 🚧 in validation, 🟢 validated, 🛑 faulty,
  📦 archived) instead of text, in the Status column and the drop-downs — full
  label kept as a tooltip.
- Status filter on the Inventory page (was missing so the "to test" dashboard
  tile could point to a useful view).

### Changed — architecture: embedded web UI in PROGMEM
- The UI (`web_src/`) is no longer served from LittleFS: it is now compiled into
  the firmware (PROGMEM), regenerated automatically before each build
  (`tools/minify_web.py` then `tools/generate_web_assets.py`, chained via
  `extra_scripts`). `pio run --target upload` therefore updates firmware and web
  UI at once — an OTA update too. LittleFS then holds only the real inventory
  data and user documents (uploaded via `/api/files/upload`), never again erased
  by a UI update. See `ARCHITECTURE.md#embedded-web-ui-progmem`.
- New service `src/services/web_assets/` (`WebAssets::find`/`send`): the only
  layer that knows the generated asset table.
- Renamed the minification output folder `data/` → `web/`.

### Removed
- `tools/sync_web.py` and `tools/package_web.py`: the problem they worked around
  (data loss on a UI update) no longer exists structurally, so these tools are
  moot.

## [1.0.0]

First functional ComponentHub inventory. The project starts from the
ESP32-Foundation base (WiFi/OTA/storage/logs services) and builds, on top, a
business core dedicated to managing an electronics workshop.

### Added — business core (`src/domain/`)
- Platform-independent domain (no Arduino/ESP32 dependency): entities
  `Component`, `Location`, `StockMovement`, `Project`, `ProjectComponent`,
  `Document`, repository interfaces, and the services `InventoryService`,
  `ProjectService`, `DocumentService`, `ImportExportService`. Designed for a
  future Raspberry Pi/Linux port without major rewrite (see `ARCHITECTURE.md`).
- JSON persistence on LittleFS (`src/storage/`), atomic writes (temp file +
  rename) and explicit corrupt-file detection instead of a silent wipe of the
  inventory.

### Added — inventory
- Complete component sheet (reference, manufacturer, electrical characteristics,
  price/supplier, dates, warranty, state, provenance, notes), with a `kind`
  field (component / module / tool / consumable) treating these families as
  filterable facets of the same inventory rather than separate systems.
- Customizable hierarchical locations (free tree).
- Stock movements (in/out/correction/inventory) with per-component history, and
  automatic detection of stock below the minimum threshold.
- "Recovered parts" workflow: recovery mode in the add form, keeping
  provenance/location/state from one addition to the next.
- Instant search and filters (type, category, status, low stock).

### Added — projects
- Project sheet (version, firmware, Git repo, status) and BOM of needed
  components, with immediate computation of missing quantities against real
  stock.

### Added — attached documentation
- Datasheets, manuals, schematics, pinouts or useful links, attachable to a
  component or a project (reuses the existing file manager rather than
  duplicating an upload pipeline).

### Added — import / export
- CSV export/import in the native format (full, re-importable backup) and in the
  [Bomist](https://bomist.com/)-compatible format (matching by reference,
  automatic location resolution/creation by name).
- Tracking of the last import / export / backup dates, exposed on the dashboard.

### Added — web UI
- Switch from the synchronous web server (`WebServer`) to **ESPAsyncWebServer**,
  the only layer (`web_manager`, `api_router`, `ota_manager`) that knows this
  choice — the rest of the framework (WiFi, storage, logs) unchanged.
- New dashboard (the **Lab** page, home page): detailed inventory status
  (references, parts, value, low stock, pending shopping list, components to
  test, projects, import/export/backup dates) and quick access to common
  actions.
- Inventory, Locations, Projects, Import/Export pages.
- Layout capped at 1060px (menu included), input forms reorganized into a
  responsive grid (less vertical scrolling, never horizontal scrolling).
- Replacement of blocking popups (`alert`/`confirm`) with a shared inline status
  area (confirmations, success/error messages, real import/export progress via
  `XMLHttpRequest`), permanently reserved in the layout so it never shifts the
  interface.
- Footer (project name + version) on every page.

### Fixed
- CSV import: matching and updating were done row by row, each re-reading/
  re-writing the whole components JSON file and rescanning locations — a
  quadratic cost that caused a watchdog reset (task blocked ~150s) when importing
  a 39-line file. Matching is now done entirely in memory from a single disk
  snapshot, with a batched write (`saveAll`).
- Data-loss risk: `pio run --target uploadfs` reflashes the whole LittleFS
  partition (hence the inventory) on each web UI update. Added `tools/sync_web.py`,
  which pushes the web files one by one via the existing HTTP API without ever
  touching the data; `tools/minify_web.py` now warns before offering `uploadfs`.

### Removed
- `examples/` and `src/modules/example_module/` (generic demos of the
  ESP32-Foundation base, unused in ComponentHub).
- `docs/INTEGRATION_GUIDE.md` (generic "start a new project from the framework"
  guide) and the generic screenshots in `docs/pictures/` — content specific to
  ESP32-Foundation, unrelated to the real ComponentHub interface.
