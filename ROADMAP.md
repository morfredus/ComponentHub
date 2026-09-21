# Roadmap - ComponentHub

This roadmap is indicative: it lists, by horizon, the features derived from the
specification and from the limits identified during development. Order within a
horizon is not fixed.

It covers the desktop application in this repository. The former ESP32 satellite
path is discontinued; ComponentHub has no embedded companion roadmap.

## Project principles

ComponentHub is built around a few simple principles:

- user data is never overwritten by an update;
- the business core (`src/domain/`) stays independent of the platform;
- **the desktop app owns its data**: it holds the reference database on
  reliable storage, independently of any embedded companion;
- the interface is meant to work equally well with mouse, keyboard and touch;
- features are built to solve a real workshop need before being generalized.

## Already shipped (up to v1.5.x)

The functional base of the workshop, to put the rest in context:

- **Qt 6 / C++17 desktop application**, native on **Windows, Linux (x86_64) and
  Raspberry Pi (ARM64)** via CMake - see
  [docs/fr/BUILD_DESKTOP.md](docs/fr/BUILD_DESKTOP.md).
- Platform-independent business core (`src/domain/`); persistence in a local
  **SQLite** database in the user's data folder.
- Unified inventory (components / modules / tools / consumables), full sheet,
  **global search** (all text fields, several words) and filters.
- Hierarchical locations; managed categories (dedicated list + on-the-fly
  creation); **administrable reference lists** (types, manufacturers, suppliers,
  packages… with **duplicate merging** and propagation to components).
- Stock movements with history and low-threshold alert.
- Projects and bill of materials: inventory components **or** off-inventory
  items ("to buy"), with computation of the *missing* components.
- Attached documentation (datasheets, links…) on components and projects.
- CSV import/export (native and Bomist formats); **full backup / restore** of
  the database in a single `.tar` archive.
- Light/dark theme following the OS; native menu bar (File · Go to · View ·
  Help), version shown in the window title.

## Short term

Features already started or naturally next.

- **Photos** - several images per sheet (manufacturer photo, real photo,
  storage, pinout, label). Reuse the existing document system (a "photo"
  category) with a thumbnail display.
- **QR codes** - generate a QR encoding the link to a component sheet or the
  content of a drawer, printable.
- **Referential integrity** - cascade deletion or orphan cleanup: deleting a
  component should also remove its stock movements and documents; deleting a
  location / category should update (or block) the components that reference it.

## Medium term

Specification features consistent with the current architecture.

- **Label printing** - export to DYMO then Brother P-Touch: a label with name,
  reference, QR code and location.
- **Validation checklist** per component (power tested, pinout checked, library
  validated, working example, datasheet added, photo taken, location assigned) -
  enriches the current "status" field.
- **Full shopping list** - beyond low-stock detection: preferred supplier,
  average price, quantity to order, generation of a dedicated list.
- **Per-module libraries** - Arduino / ESP-IDF / PlatformIO libraries, examples,
  snippets and personal notes attached to a component.
- **Extended history** - beyond stock movements: sheet edits, location and
  quantity changes.
- **Managed sub-category** as an entity (like Category), instead of a free-text
  field.
- **CSV import - encoding**: detect/convert Windows-1252 in addition to the UTF-8
  required today (e.g. a spreadsheet "CSV separated by ;").

## Long term / Vision

Portability and ecosystem - what the architecture was designed for from the
start.

- **Evolving persistence engine on the desktop** - consolidate SQLite queries,
  volume handling and history without compromising the domain interfaces.
- **Touch interface + physical control** - rotary knob or dedicated front panel
  for the desktop application when it answers a concrete workshop need.
- **Acquisition peripherals** - barcode reader, RFID / NFC, camera and automatic
  recognition, added without touching the core (encapsulated behind interfaces,
  like the rest of the platform).
- **Backup & synchronization** - SQLite export, backup to a NAS, multi-station
  sync, public REST API.
- **Domain profiles** - the engine should adapt to different domains
  (electronics, photography, workshop, cooking, modeling…) without changing the
  business core.

---

See [CHANGELOG.md](CHANGELOG.md) for what has already shipped. The former
[ESP32 satellite decision](docs/fr/ADR-0001-desktop-maitre-esp32-satellite.md)
is retained as historical context only.
