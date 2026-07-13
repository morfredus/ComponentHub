# Contributing to ComponentHub

Thanks for your interest! This document explains **how the project is built, why
it is built that way, and the rules that keep it maintainable**. Read it once
before your first change — it will save you (and reviewers) time.

It applies to the **desktop application** (this repository). The ESP32 firmware
lives in the separate `ComponentHub-ESP32` repository and has its own rules.

---

## 1. Philosophy

ComponentHub is the **technical memory of an electronics workshop**. A few
principles drive every decision:

- **User data is sacred.** It is never overwritten by an update. Writes are
  atomic (temp file + rename), and a corrupt file is reported, never silently
  treated as empty.
- **The business core knows nothing about the platform.** No Qt, no operating
  system in `src/domain/`. This is what lets the same logic run unchanged on
  Windows, Linux and a Raspberry Pi.
- **Solve a real workshop need first, generalize later.** Features come from
  actual use, not speculation.
- **One behavior, three operating systems.** What the user sees and does must be
  identical on Windows, Linux (x86_64) and Raspberry Pi (ARM64). See
  [§4 Portability](#4-portability-the-most-important-rule) — this is the rule we
  are strictest about.

## 2. Architecture in one picture

The code is split into layers that depend **inward only** (UI → storage →
domain; the domain depends on nothing):

```
src/
├── domain/     Business core: entities + repository interfaces + services.
│               NO dependency on Qt, Arduino, or the OS. Pure C++17
│               (<string>, <vector>, <optional>, <map>). Portable as-is.
├── storage/    Desktop persistence: JSON files (nlohmann/json) implementing
│               the repository interfaces the domain declares.
├── platform/   OS-facing helpers with a portable API (.tar archive, clock).
├── ui/         The ONLY layer that knows Qt (Theme, Icons, pages, dialogs).
└── main.cpp    Wiring: build the AppContext, hand it to the MainWindow.
```

Full explanation: [docs/fr/ARCHITECTURE.md](docs/fr/ARCHITECTURE.md) *(FR)*.

**Where does my change go?**

- A new *rule / calculation* (e.g. "how missing components are counted") →
  a **service in `src/domain/`**, never in a UI page.
- A new *stored field or table* → an entity + repository interface in
  `src/domain/`, its JSON implementation in `src/storage/`.
- A new *screen, button, dialog* → `src/ui/`, calling the domain services.
- Something the OS provides (files, time, archives) → wrap it behind a portable
  helper in `src/platform/`, don't call OS APIs from the UI or the domain.

Keeping these boundaries is what makes the project easy to maintain: you can
change the UI without touching the logic, and change storage (JSON → SQLite one
day) without touching either.

## 3. Building and running

See the step-by-step guide (beginners included, all three OSes):
[docs/fr/GETTING_STARTED.md](docs/fr/GETTING_STARTED.md) *(FR)*. In short:

```sh
cmake --preset mingw   && cmake --build --preset mingw     # Windows (MSYS2/MinGW)
cmake --preset linux   && cmake --build --preset linux     # Linux x86_64
cmake --preset linux-arm64 && cmake --build --preset linux-arm64   # Raspberry Pi
```

Always build **and run** your change on at least one OS before opening a PR. If
your change touches anything platform-facing (files, paths, fonts, rendering),
try to test on a second OS.

## 4. Portability — the most important rule

> **The application must build, run and behave identically on Windows x64,
> Linux x64 and Raspberry Pi (ARM64). No exceptions the user can notice.**

Concretely:

1. **Cross-platform libraries only.** Any new dependency must be available and
   behave the same on the three targets. **No OS-proprietary library** (no
   Win32-only, macOS-only, or Linux-only API) when a portable option exists.
   Prefer, in order: (1) the C++17 standard library, (2) Qt (already a
   dependency), (3) a small, well-maintained, permissively licensed
   header/library that ships on all three. Adding a dependency is a design
   decision — open an issue first.

2. **No `#ifdef _WIN32 / #else / #endif` for behavior.** Do not fork the code
   per OS. This kind of block is **forbidden by default**:

   ```cpp
   #ifdef _WIN32
       // Windows-specific behavior
   #else
       // something else
   #endif
   ```

   If you catch yourself writing one, it almost always means you reached for an
   OS API where a portable one exists (use Qt / the standard library / a
   `src/platform/` helper instead).

3. **The only allowed exception**, when *no* portable solution exists:
   - keep the platform-specific code **isolated inside `src/platform/`** behind a
     single portable function/interface — never sprinkled through the UI or the
     domain;
   - **guarantee identical results** on all three OSes (same output, same
     formatting, same on-screen rendering, same file bytes);
   - **document why** in a comment (what portable option was tried and why it
     failed) and mention it in the pull request.

   Example of an acceptable case: a Windows-only deployment step in a *build
   script* (`scripts/windows/…`) — that is packaging, not application behavior.

4. **No hard-coded, OS-specific paths.** Use `QStandardPaths` for user data,
   `resources/` (Qt resource system) for bundled assets, and forward slashes in
   code. Never assume `C:\…` or `/home/…`.

Why so strict? Because the promise of ComponentHub is that it is the *same tool*
everywhere — and that **Linux/Raspberry Pi is a first-class target, not an
afterthought** (see the Getting Started guide: it is as easy there as on
Windows, often faster). Per-OS branches quietly break that promise.

## 5. Coding conventions

- **Language:** C++17. No compiler extensions; it must build with MinGW-w64 and
  GCC. Keep warnings clean.
- **Style:** match the surrounding code (indentation, naming, comment density).
  Comments explain *why*, not *what*.
- **UTF-8 everywhere.** Source files are UTF-8; accented characters and `€` must
  render correctly. On MSVC builds the `/utf-8` flag is already set.
- **Comments and user-facing strings** in the app are currently in **French**
  (the app's audience). Keep new UI strings in French for consistency.
- **No hard-coded version.** The version comes from the `VERSION` file via CMake
  (`CH_APP_VERSION`). Never type a version number into the code.
- **Theme-aware UI.** Colors come from the theme tokens (`*.theme` + `app.qss`),
  never hard-coded in widgets, so light and dark both work.

## 6. Commits, versioning and changelog

- **Branch** off the current working branch; don't commit straight to it.
- **Semantic Versioning** (`MAJOR.MINOR.PATCH`) in the `VERSION` file:
  - **PATCH** — bug fix or docs, no behavior change (e.g. 1.5.1 → 1.5.2);
  - **MINOR** — new backward-compatible feature (e.g. 1.4.2 → 1.5.0);
  - **MAJOR** — a breaking change (e.g. the data format stops being
    interchangeable with the firmware).
- **Update `CHANGELOG.md`** in the same commit, following the
  [Keep a Changelog](https://keepachangelog.com/) format (`Added` / `Changed` /
  `Fixed` / `Removed`).
- **Commit messages:** a concise summary line (the version and the gist), then a
  short body explaining *why*. English or French are both fine, but be
  consistent within a commit.
- **Keep the docs in sync.** If you change behavior, update the relevant
  `docs/fr/` page (and, if you can, its English counterpart under `docs/en/`).

## 7. Documentation language

By GitHub convention, **root files are in English** (`README.md`,
`CHANGELOG.md`, `ROADMAP.md`, `CONTRIBUTING.md`). A French `README.fr.md` is kept
for French speakers. The in-depth guides under `docs/` are the **French**
reference (`docs/fr/`); `docs/en/` holds an English index and any translations.

Translations are very welcome: copy `docs/fr/<page>.md` to `docs/en/<page>.md`,
translate, and link it from [docs/en/README.md](docs/en/README.md).

## 8. Reporting bugs / proposing features

Open an issue with:

- what you did, what you expected, what happened;
- your OS and version (Windows / Linux / Raspberry Pi), and the app version
  (Help → About);
- for a crash, the steps to reproduce and, if possible, the data folder path
  (Help → *Open the data folder*).

Thank you for helping keep ComponentHub simple, portable and reliable.
