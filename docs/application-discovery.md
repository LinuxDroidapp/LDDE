# LDDE D6 — Application Discovery Architecture

## 1. Overview

The **LDDE Application Discovery subsystem (D6)** establishes the authoritative catalog of installed Linux desktop applications available to the LinuxDroid Desktop Environment.

D6 reads, parses, validates, indexes, and monitors standard freedesktop `.desktop` files located across standard XDG filesystem locations, producing a normalized application catalog consumed by current and future desktop components:

```text
Linux Filesystem ($XDG_DATA_HOME/applications, $XDG_DATA_DIRS/applications)
                            │
                            ▼
                  ApplicationDiscovery
                            │
               ┌────────────┴────────────┐
               ▼                         ▼
      DesktopEntryReader       DesktopEntryParser
      (UTF-8 & File I/O)       (Groups, Locales, Escapes)
               │                         │
               └────────────┬────────────┘
                            │
                            ▼
                   ApplicationMetadata
               (Normalized representation)
                            │
                            ▼
              ApplicationDiscoveryPolicy
           (Precedence, Overrides, Filtering)
                            │
                            ▼
                    ApplicationCatalog
        (Authoritative storage, queries, events)
          ┌─────────────────┼─────────────────┐
          ▼                 ▼                 ▼
       Launcher           Dock             Switcher
       (future)         (future)           (future)
```

---

## 2. Core Invariants & Boundaries

1. **Strict Read/Index Boundary**:
   - D6 is **strictly a metadata indexing and discovery subsystem**.
   - D6 **never executes** `Exec=` fields, never forks or launches processes, and never calls shell interpreters (`/bin/sh -c`).
   - Application execution is strictly deferred to future execution phases (D7/D8).

2. **Application vs. Window Separation**:
   - D6 represents **installed applications** (static filesystem metadata).
   - D2 (`WindowRegistry`) and D3 (`WindowManager`) represent **running client windows** on Wayland.
   - An application exists in D6 even if it is not running.
   - A Wayland window in D2 is not synthesized or assumed to belong to an application without explicit protocol correlation.

3. **Zero Android & Zero PRoot**:
   - All discovery strictly follows Linux desktop filesystem conventions.
   - Zero Android APIs, zero Android package managers (`pm`), and no PRoot process scanning.

4. **ARM64 Native Performance**:
   - Robust, cache-friendly data structures with deterministic linear iterations.
   - High resilience against corrupt files, permission errors, and disappearing files during scanning.

---

## 3. Subsystem Components

### 3.1 ApplicationId

`ApplicationId` provides a strongly typed wrapper around the desktop file identity:
- For top-level desktop files: e.g. `org.gnome.Calculator.desktop`, `firefox.desktop`.
- For desktop files in subdirectories of application search paths: e.g. `sub/editor.desktop` becomes `sub-editor.desktop` per the freedesktop Desktop Entry Specification.
- Validates extension (`.desktop`), non-empty basename, and disallows path separators.

### 3.2 DesktopEntryReader & DesktopEntryParser

- **DesktopEntryReader**:
  - Validates file existence and regular file status.
  - Implements strict UTF-8 validation across multi-byte code sequences.
  - Enforces safety limits (e.g. 1 MB file size ceiling to prevent denial-of-service).
- **DesktopEntryParser**:
  - Parses INI-style groups (`[Desktop Entry]`, `[Desktop Action <name>]`).
  - Ignores comments (`#`) and empty lines.
  - Decodes standard escape sequences: `\s` (space), `\n` (newline), `\t` (tab), `\r` (carriage return), `\\` (backslash), `\;` (semicolon in lists).
  - Parses semicolon-delimited lists with escape tolerance.
  - Parses structured `Exec=` fields into executable binary, argument list, and recognized field codes (`%f`, `%F`, `%u`, `%U`, `%i`, `%c`, `%k`).

### 3.3 Localization & Language Selection

The parser supports localized keys (e.g. `Name[fr_FR]`, `GenericName[de]`, `Comment[ja]`).
When querying a localized string for locale `lang_COUNTRY.ENCODING@MODIFIER`:
1. `lang_COUNTRY@MODIFIER`
2. `lang_COUNTRY`
3. `lang@MODIFIER`
4. `lang`
5. Default unlocalized key (fallback)

### 3.4 ApplicationMetadata

Normalizes raw desktop entries into a clean domain model:
- `id`: `ApplicationId`
- `name`, `generic_name`, `comment`: Localized strings based on active locale.
- `exec`, `executable`, `exec_args`, `field_codes`: Structured execution metadata.
- `icon`: `ApplicationIconReference` (distinguishing theme icons like `htop` from absolute paths like `/usr/share/pixmaps/app.png`).
- `terminal`, `no_display`, `hidden`, `startup_notify`: Boolean flags.
- `only_show_in`, `not_show_in`: Desktop environment filtering.
- `categories`, `mime_types`, `keywords`: Categorization and search indexes.
- `actions`: List of auxiliary actions defined in the desktop file.
- `source`: `DesktopEntrySource` tracking file path, source type, and modification timestamp.

### 3.5 Precedence & Overrides (ApplicationDiscoveryPolicy)

Search directories are prioritized:
1. **User**: `$XDG_DATA_HOME/applications` or `~/.local/share/applications` (Priority 0, highest)
2. **Local**: `/usr/local/share/applications` (Priority 1)
3. **System**: `/usr/share/applications` (Priority 2)

**Precedence Rules**:
- When identical `ApplicationId`s exist in multiple directories, the entry in the higher-priority directory completely shadows lower-priority entries.
- If a user desktop entry specifies `Hidden=true`, the application is treated as deleted/hidden, masking any system-wide entry with that ID from the user menu.

### 3.6 ApplicationCatalog

The authoritative in-memory registry of installed applications:
- `all()`: Returns all cataloged applications.
- `visible_applications(desktop_name)`: Returns applications that are user-visible (`!hidden && !no_display && is_visible_in_desktop(desktop_name)`), sorted deterministically by case-insensitive name, then by `ApplicationId`.
- `find(id)`, `contains(id)`: O(1) lookup.
- `search(query)`: Case-insensitive search across name, generic name, comment, keywords, and categories.
- `update_applications()`: Diffing engine computing added, removed, and changed applications, and dispatching catalog events (`on_application_added`, `on_application_removed`, `on_application_changed`, `on_catalog_refreshed`).

### 3.7 Dynamic Monitoring (ApplicationChangeMonitor)

- Utilizes Linux `inotify` on active search directories.
- Watches for `IN_CREATE`, `IN_DELETE`, `IN_MODIFY`, `IN_MOVED_TO`, and `IN_MOVED_FROM`.
- Integrated directly into the LDDE `EventLoop` non-blocking poll loop.
- Automatically triggers `scan_and_refresh()` upon filesystem events without polling loops.

---

## 4. Configuration Reference

Application discovery settings are located in the `[application]` section of `ldde.conf`:

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `desktop_identity` | string | `LinuxDroid` | Current desktop environment name for `OnlyShowIn`/`NotShowIn` filtering |
| `watch_filesystem` | bool | `true` | Enable inotify filesystem monitoring for automatic catalog refresh |
| `system_paths` | string | `/usr/local/share/applications:/usr/share/applications` | Fallback system desktop paths |
| `user_path` | string | `~/.local/share/applications` | Fallback user desktop path |

---

## 5. Verification & Testing

1. **Unit Testing (`ldde_unit_tests`)**:
   - `ApplicationIdTest`: Validation, path conversion, and subdirectory hyphenation.
   - `ApplicationIconTest`: Theme name vs absolute file path distinction.
   - `DesktopEntryParserTest`: Valid entry parsing, comments, whitespace, escapes, localized fallback, actions, invalid entries.
   - `ExecFieldParsingTest`: Command tokens, quotes, and field codes.
   - `ApplicationMetadataTest`: Visibility rules, `Hidden`/`NoDisplay` filtering, search queries.
   - `ApplicationCatalogTest`: Deterministic sorting, additions, removals, modifications, and diff events.
   - `ApplicationDiscoveryTest`: User vs system precedence, `Hidden=true` masking, corrupted file resilience.

2. **Integration Testing (`ldde_integration_tests`)**:
   - `ApplicationIntegrationTest.MultiTierXdgDiscoveryAndRefresh`: Synthetic multi-tier XDG hierarchy, testing precedence, dynamic addition, and dynamic removal.
   - `ApplicationIntegrationTest.HostSystemApplicationsDiscovery`: Validates real installed desktop applications on the host rootfs (`/usr/share/applications`), verifying deterministic ordering and metadata fidelity.

