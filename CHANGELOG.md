# Changelog. Dark Angels Fork

All changes below are relative to the last stable GalaxyRP release (3.7.2) this fork started from. Old changelog is deprecated.

## [3.8.0] - TaystJK port and stability pass

### Added
- Native TaystJK engine support: builds and runs directly on TaystJK (EternalJK's modern successor) instead of requiring OpenJK, with the old EternalJK/JAPro detection and "switch to OpenJK" warning removed.
- Multi-platform CI and build support: Windows (x86/x86_64), Linux (x86/x86_64/arm64), and macOS (x86_64/Apple Silicon), including a missing arm64 branch in the platform-detection headers and updated bundled SDL2 (2.0.12 -> 2.32.4).
- `rp_loginRequired` cvar: optionally forces any player who isn't logged into an account to stay in Spectator.
- `/admmap <gametype> <map>` admin command to change map and gametype together (adapted from JAPro's `ammap`), gated by a repurposed admin bit.
- `/training` command: toggles a reduced-damage training saber mode.
- Client-side sync commands (`supdatemodel`, `supdatename`, `supdatesaber`): push server-corrected model/name/saber values back to the client's own cvars so the console and menus no longer show stale values after a database-driven login, character switch, or rejected saber choice.
- UI right-click context menus and integer-valued sliders (`cvarInt`/`ITEM_TYPE_INTSLIDER`, used by the saber RGB color sliders) and the `accept` menu script keyword, all ported from TaystJK's UI code so its bundled menus parse and work correctly.
- `/help` output rewritten to reflect the current command list, including several commands that existed but were undocumented.

### Changed
- Default admin account's `AdminLevel` is now `-1` (all bits) instead of a hardcoded bitmask, so it automatically gains any admin command added in the future.
- The server is now always treated as being in RP Mode: the `zyk_rp_mode` cvar and the mode-switching `/mode`-style dead code tied to it were removed, along with several other unused quest/RPG cvars (`zyk_allow_quests`, `zyk_allow_rpg_mode`, `zyk_allow_saving_in_rp_mode`, `zyk_quest_afk_timer`, `zyk_guardian_quest_timer`, and about a dozen more class/quest toggle cvars that no longer did anything).
- Command usage messages made consistent with their actual names (`/new`, `/spendcredits`, `/newsadd`, `/helpup`, etc., which had drifted from earlier renames).
- Saber color slider dragging now snaps to whole numbers instead of floats.
- Distributable packaging: the mod's game/cgame/ui modules now install only into their own `GalaxyRP` folder rather than also being copied into generic `OpenJK`/`base` folders, matching how GalaxyRP is actually distributed.
- CI build artifacts now zip with a top-level `GalaxyRP` folder wrapping their contents, instead of the mod's files sitting directly at the zip root.

### Fixed
- **Account/character persistence**: database access now goes through a shared open helper that enables SQLite WAL mode and a busy-retry timeout, fixing intermittent "database is locked" errors on map change that could leave a returning player with no weapons or inventory until a manual respawn. The database path is also now resolved against the engine's home path instead of the process's working directory, so it no longer depends on how the server was launched.
- **Saber persistence**: a saved dual/staff saber configuration was being silently discarded and replaced with a single saber on every login or character load, due to a counting bug in the database-load path; fixed so dual/staff sabers round-trip correctly.
- **Saber switching**: a logged-in player's saber choice made through the UI was never detected as changed (the client's live selection was unconditionally overwritten by the database value on every spawn), leaving the saber selection menu stuck; the database read is now scoped to first spawn only.
- Model, name, and saber changes made via the client console or menu could silently revert or fail to reflect on the client's own screen for logged-in players, since server-side corrections never reached the client's local cvars — fixed via the new sync commands above.
- First spawn after a map change used blank skill/inventory data because the account/character reload happened after starting equipment was already granted; reload now happens first.
- Lava/slime damage was being absorbed by the RPG shield system like a normal attack, making it look inconsistent (sometimes no damage, then damage resuming once shields ran dry); it now bypasses shields like Force Grip already did.
- Force Sense played no activation sound at all (the sound calls were left commented out).
- Upgrading Absorb/Protect/Lightning to skill level 4 or 5 could make Force Lightning render as Force Drain until the next respawn, due to a missing cap that the database-load path already applied; the immediate skill-up/down path now applies the same cap.
- A UI listbox click on an empty row past the last real item could select an out-of-range index, and the staff-saber hilt list was validated against the wrong array; both fixed.
- A vehicle's model scale could reset to default for its own rider (though not for observers) due to a field not carried over in client-side prediction.
- Several build failures under Clang/macOS (implicit function declarations, K&R-style function signatures, a `fread()` return value overwriting a file handle, non-portable `stricmp` usage, a `void`-returning function that wasn't reliably returning void, a non-`void` function that could fall off its end or return garbage on error) were fixed for cross-platform compilation; none were behavior changes on the previously working GCC/Linux build except where separately noted above.
- A copy-paste bug in the saber-columns database migration compared against the wrong error string, printing a spurious SQL error and skipping the rest of table initialization (including admin account setup) on every server restart after the first.
- A cosmetic string-mismatch bug where `/players` could show a fully-admin account as "(logged)" instead of "(admin)".
- Minor: fixed a missing closing quote in the `/removepickups` confirmation message, a stack buffer overflow risk from unbounded saber-model name copies, an indentation inconsistency, and a saber-style-switch debounce that wasn't actually debouncing anything.

### Security
- Fixed a real SQL injection vulnerability in the `/login`, `/new`, and `/changepassword` account commands, and applied the same parameterized-query treatment across the rest of the database layer (item names, chat/news text, character saves, and related queries) as a broader hardening pass.
- Account login/password values sent from the UI are now stripped of embedded quote characters before being passed to the console command parser, preventing a crafted password from breaking out of its quoted argument.
