# LDDE D7 — Application Launcher Architecture

## 1. Overview

The **LDDE Application Launcher (D7)** provides a standalone, Linux-native, mobile-first application launcher for the LinuxDroid Desktop Environment.

D7 presents installed Linux desktop applications discovered by **D6 (Application Discovery)**, adapts dynamically to mobile screen dimensions and safe areas via **D4 (Mobile Display Policy)**, handles touch and keyboard navigation through **D0/D5 (Input)**, renders directly into the **D1 (Wayland Shell)** overlay surface, and cleanly dispatches launch requests via a structured POSIX process launcher without synthesizing fake windows or using shell interpreters (`/bin/sh -c`).

```text
       ┌───────────────────────────────┐
       │   D6 Application Catalog      │
       └──────────────┬────────────────┘
                      │ ApplicationMetadata events
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                   D7 Application Launcher                   │
│                                                             │
│  ┌──────────────────────┐        ┌───────────────────────┐  │
│  │    LauncherModel     │◄───────┤ LauncherIconResolver  │  │
│  │ (Catalog sync, filter│        │(XDG icons & cache)    │  │
│  │  & search ranking)   │        └───────────────────────┘  │
│  └──────────┬───────────┘                                   │
│             │                                               │
│             ▼                                               │
│  ┌──────────────────────┐        ┌───────────────────────┐  │
│  │    LauncherLayout    │◄───────┤   D4 DisplayPolicy    │  │
│  │ (Dynamic responsive  │        │(Form factor, cutouts, │  │
│  │  grid & safe insets) │        │ scale, metrics)       │  │
│  └──────────┬───────────┘        └───────────────────────┘  │
│             │                                               │
│             ▼                                               │
│  ┌──────────────────────┐        ┌───────────────────────┐  │
│  │     LauncherView     │───────►│   D1 ShellOverlay     │  │
│  │ (Cairo overlay card, │        │(Double-buffered shm   │  │
│  │  grid, chips, search)│        │ Cairo rendering)      │  │
│  └──────────▲───────────┘        └───────────────────────┘  │
│             │                                               │
│  ┌──────────┴───────────┐        ┌───────────────────────┐  │
│  │  LauncherController  │◄───────┤ D0 Input / D5 Touch   │  │
│  │(Key nav, touch tap,  │        │(Tap, scroll, keys)    │  │
│  │ scroll & launch)     │        └───────────────────────┘  │
│  └──────────┬───────────┘                                   │
│             │                                               │
│             ▼                                               │
│  ┌──────────────────────┐                                   │
│  │  ApplicationLauncher │                                   │
│  │ (fork + execvp POSIX │                                   │
│  │  structured launch)  │                                   │
│  └──────────┬───────────┘                                   │
└─────────────┼───────────────────────────────────────────────┘
              │ Process created (PID)
              ▼
   Linux Application Process
              │ Connects to Wayland
              ▼
    D2 WindowRegistry / D3 WindowManager
```

---

## 2. Deterministic State Machine

The launcher follows a formal, deterministic state machine (`LauncherState`):

```text
             open()
  [Closed] ──────────► [Opening]
     ▲                     │ animation/overlay ready
     │                     ▼
     │  close()         [Open] ◄────────────────┐
     │ ◄───────────────    │                    │
     │                     │ type text          │ clear text
     │                     ▼                    │
     │                 [Searching] ─────────────┘
     │                     │
     │                     │ launch item
     │                     ▼
     │                 [Launching]
     │                 ┌───┴────────────────┐
     │  launch success │                    │ launch failure
     │                 ▼                    ▼
     └── [Closing] ◄───┘             [LaunchFailed]
            │                               │
            │ close finish                  │ user dismisses / retries
            ▼                               ▼
         [Closed]                         [Open]
```

### Invariants:
1. **Idempotence**: Calling `open()` while already `Open` or `Opening` returns `Status::ok()` with no duplicate side effects. Calling `close()` while `Closed` is a safe no-op.
2. **Re-entrant Safety**: State transitions cannot corrupt active iteration; listeners receive deterministic notifications on every valid state change.
3. **Failure Isolation**: A launch failure moves to `LaunchFailed` and displays a non-blocking error banner in the launcher; the desktop environment never crashes, and the launcher remains fully interactive.

---

## 3. Responsive Grid Layout (D4 Integration)

The launcher layout adapts dynamically to the display metrics provided by `DisplayPolicy`:

- **Dynamic Columns**: Columns are calculated at runtime based on available width, margins, and minimum item width (80 dp):
  $$\text{columns} = \max\left(1, \lfloor \text{content\_width} / (\text{min\_item\_width} + \text{spacing}) \rfloor\right)$$
  - Typical portrait phone ($360\times 800\,\text{dp}$): 3–4 columns.
  - Typical landscape phone ($800\times 360\,\text{dp}$): 5–6 columns.
  - Tablet ($900\times 1280\,\text{dp}$): 6–8 columns.
- **Safe Area Inset Respect**: Avoids display notches, status bars, and navigation gestural areas by applying `DisplayCutouts` and edge insets.
- **Touch Targets**: Every grid item satisfies the $\ge 48\,\text{dp}$ touch target requirement.
- **Touch Scrolling**: Smooth scrolling with bounding and clamping so content cannot be scrolled into void space.

---

## 4. Freedesktop Icon Resolution & Caching

The `LauncherIconResolver` implements standard freedesktop icon discovery:

1. **Direct Absolute Paths**: If `Icon=/path/to/icon.png`, directly loaded if existent.
2. **Standard Theme Hierarchy**:
   - `~/.local/share/icons/`
   - `/usr/local/share/icons/`
   - `/usr/share/icons/hicolor/`
   - `/usr/share/pixmaps/`
3. **Icon Extensions Checked**: `.png`, `.svg`, `.xpm`.
4. **Thread-Safe LRU Cache**: Resolves icon paths once and caches results in memory to minimize filesystem I/O during launcher rendering.
5. **Fallback Badges**: When no valid icon file is found, the Cairo renderer synthesizes a crisp, elegant vector badge with the first letter of the application name on a theme-derived rounded container.

---

## 5. In-Memory Search & Relevance Scoring

Search queries are evaluated in-memory over the `ApplicationCatalog` with zero disk access:

### Multi-Tier Scoring Hierarchy:
1. **Tier 1 (Exact Match, Score 1000)**: Case-insensitive query matches application `Name` exactly.
2. **Tier 2 (Prefix Match, Score 800)**: Query matches start of application `Name`.
3. **Tier 3 (Word Prefix Match, Score 600)**: Query matches start of any word within application `Name`.
4. **Tier 4 (Substring Match, Score 400)**: Query is contained anywhere within application `Name`.
5. **Tier 5 (GenericName Match, Score 300)**: Query matches `GenericName` prefix or substring.
6. **Tier 6 (Keywords Match, Score 200)**: Query matches any defined keyword in the desktop entry.
7. **Tier 7 (Comment Match, Score 100)**: Query is found in `Comment` description.

### Stable Deterministic Tie-Breaking:
For items with identical relevance scores, ordering is strictly tie-broken by:
1. Localized `Name` ascending (alphabetical).
2. Desktop entry `ApplicationId` ascending.

---

## 6. Category Navigation & Filtering

Categories are extracted from the freedesktop `.desktop` `Categories` field:
- Canonical categories: `AudioVideo`, `Development`, `Education`, `Game`, `Graphics`, `Network`, `Office`, `Settings`, `System`, `Utility`.
- Custom and unrecognized categories map deterministically to `Other`.
- Dynamic item counts are computed per category based on currently discoverable applications.
- Selection is filtered by composite query: `CategoryFilter` $\cap$ `SearchQuery`.

---

## 7. Input Handling & Navigation

The `LauncherController` accepts both keyboard and touch events:

- **Keyboard Navigation**:
  - `Arrow Left / Right`: Move selection horizontally across grid items.
  - `Arrow Up / Down`: Move selection vertically across grid rows.
  - `Enter / Return`: Launch the currently selected item.
  - `Escape`: First Esc clears any active search query; second Esc closes the launcher.
  - `Tab`: Cycle forward through category chips.
  - `Backspace`: Delete the last character of the search query.
  - Printable characters: Automatically focus and append to the search query.
- **Touch Gestures**:
  - `Tap`: Hit-test category chips, search bar, and grid items; tapping an item triggers immediate launch.
  - `Drag / Pan`: Vertical scrolling with velocity damping and boundary clamping.
  - `Tap Outside`: Dismisses the launcher modal.

---

## 8. Clean Launch Boundary (No Shell Invocation)

Application execution is strictly isolated:

1. **No `/bin/sh -c`**: Shell string execution is strictly prohibited to prevent arbitrary command injection, environment pollution, and zombie processes.
2. **Structured Execution (`execvp`)**:
   - Arguments parsed from structured `ApplicationMetadata::exec_arguments()`.
   - Executable path resolved and verified using `access(X_OK)`.
3. **Child Isolation (`fork()`)**:
   - `pipe2(O_CLOEXEC)` transmits `execvp` failure `errno` back to LDDE parent before execution replaces memory.
   - Child process detaches (`setsid()`), resets signals, and unblocks standard descriptors.
4. **Zero Synthetic Windows**:
   - LDDE **never** fabricates synthetic `WindowId`s or placeholder surfaces when an application is launched.
   - Genuine Wayland window tracking remains 100% owned by the compositor, **D2 (WindowRegistry)**, and **D3 (WindowManager)** when the client establishes its connection.
