# Changelog. Dark Angels Fork



## [3.8.0]

All changes below are relative to the last stable GalaxyRP release (3.7.2) this fork started from.

### Added
- Native TaystJK engine support: builds and runs directly on TaystJK (EternalJK's modern successor) instead of OpenJK, with the old EternalJK/JAPro detection and "switch to OpenJK" warning removed.
- Multi-platform CI and build support: Windows (x86/x86_64), Linux (x86/x86_64/arm64), and macOS (x86_64/Apple Silicon), including a missing arm64 branch in the platform-detection headers and updated bundled SDL2 (2.0.12 -> 2.32.4).
- RGB lightsaber colors, persisted per character. Shaders and textures are courtesy of JAPro/TaystJK.
- `rp_loginRequired` cvar: optionally forces any player who isn't logged into an account to stay in Spectator.
- `/admmap <gametype> <map>` admin command to change map and gametype together, gated by a repurposed admin bit.
- `/training` command: toggles a reduced-damage training saber mode. This one is courtesy of Alex.
- Client-side sync commands (`supdatemodel`, `supdatename`, `supdatesaber`): push server-corrected model/name/saber values back to the client's own cvars so the console and menus no longer show stale values after a database-driven login, character switch, or rejected saber choice.
- `/help` output rewritten to reflect the current command list, including several commands that existed but were undocumented.
- `/updatesaber` and `/updateforce` commands: apply lightsaber pick or force power (logged-out players only) pick instantly without needing to respawn first.
- A new in-game Settings panel showing each `/settings` toggle's live value.
- Armor skill now gives a chance to deflect an incoming blaster shot.
- `/adminup`, `/admindown`, and the credit commands (`/spendcredits`, `/createcredits`, `/givecredits`) now write an audit-log entry recording who did what to whom.

### Changed
- Default admin account's `AdminLevel` is now `-1` (all bits) instead of a hardcoded bitmask, so it automatically gains any admin command added in the future.
- The server is now always treated as being in RP Mode: the `zyk_rp_mode` cvar and the mode-switching `/mode`-style dead code tied to it were removed, along with several other unused quest/RPG cvars.
- Command usage messages made consistent with their actual names (`/new`, `/spendcredits`, `/newsadd`, `/helpup`, etc., which had drifted from earlier renames).
- Saber color slider dragging now snaps to whole numbers instead of floats.
- Distributable packaging: the mod's game/cgame/ui modules now install only into their own `GalaxyRP` folder, matching how GalaxyRP is actually distributed.
- CI build artifacts now zip with a top-level `GalaxyRP` folder wrapping their contents.
- Refactored `/buy` and `/stuff` into clean `/buy item <n>` / `/buy upgrade <n>` subcommands (and `/stuff` equivalents).
- Removed a lot of of the legacy per-class RPG content.
- `/settings` renumbered from a sparse 0-15 range (several of which toggled nothing any more) down to a clean 1-7 list.
- Jetpack: unified the `/jetpack` command's and the RPG auto-grant's availability checks, added a logged-in-only tier to `rp_allow_jetpack_command`.
- A logged-in player can now pick up either Force Enlightenment color regardless of their current alignment.
- Redesigned parts of the UI/UX.

### Fixed
- **Account/character persistence**: database access now goes through a shared open helper that enables SQLite WAL mode and a busy-retry timeout, fixing intermittent "database is locked" errors on map change that could leave a returning player with no weapons or inventory until a manual respawn. The database path is also now resolved against the engine's home path instead of the process's working directory, so it no longer depends on how the server was launched.
- **Saber persistence**: a saved dual/staff saber configuration was being silently discarded and replaced with a single saber on every login or character load, due to a counting bug in the database-load path; fixed so dual/staff sabers round-trip correctly.
- **Saber switching**: a logged-in player's saber choice made through the UI was never detected as changed (the client's live selection was unconditionally overwritten by the database value on every spawn), leaving the saber selection menu stuck; the database read is now scoped to first spawn only.
- Model, name, and saber changes made via the client console or menu could silently revert or fail to reflect on the client's own screen for logged-in players, since server-side corrections never reached the client's local cvars — fixed via the new sync commands above.
- First spawn after a map change used blank skill/inventory data because the account/character reload happened after starting equipment was already granted; reload now happens first.
- Force Sense played no activation sound at all (the sound calls were left commented out).
- Upgrading Absorb/Protect/Lightning to skill level 4 or 5 could make Force Lightning render as Force Drain until the next respawn, due to a missing cap that the database-load path already applied; the immediate skill-up/down path now applies the same cap.
- A UI listbox click on an empty row past the last real item could select an out-of-range index, and the staff-saber hilt list was validated against the wrong array; both fixed.
- A vehicle's model scale could reset to default for its own rider (though not for observers) due to a field not carried over in client-side prediction.
- A copy-paste bug in the saber-columns database migration compared against the wrong error string, printing a spurious SQL error and skipping the rest of table initialization (including admin account setup) on every server restart after the first.
- A cosmetic string-mismatch bug where `/players` could show a fully-admin account as "(logged)" instead of "(admin)".
- `Accounts.PlayerSettings` (the `/settings` bitmask) was hardcoded to `0` on every save instead of being bound, so every `/settings` toggle was silently reset to off on the very next save and came back off on the player's next login; it's now actually persisted.
- Ammo counts on character load were reading the wrong database columns (an enum/schema-offset mismatch that grew worse as the Skills table gained columns over time), so ammo restored on relogin or character switch could be wrong.
- A stack buffer overflow in `create_new_character()`: its query buffer was sized before the Armor/Flamethrower/ShieldRegen/HealthRegen skill columns existed, so the SQL text it holds was being silently truncated with no NUL terminator, in practice causing `/char new` to run garbage SQL and fail to create the new character's Skills/Weapons rows.
- **Character/account persistence**: neither a map change nor a player disconnecting (quit, timeout, kick/ban) ever explicitly saved a logged-in player's progress, so the next login silently discarded any credits, skill points, XP, saber colors, or settings changes made since the last incidental save. Both paths, and `/char use`/`/char new`/`/char remove` switching character, now save first.
- The 3 kept shop upgrades (Holdable Items, Impact Reducer, Stun Baton) now persist correctly: their state was moved off a field that was never written to the database onto one that is.
- `/levelup`, `/leveldown`, and `/scale` (when targeting another player) were saving the admin's own database row instead of the target's, so the change applied live but silently reverted on the target's next login or logout.
- Downgrading a weapon-granting skill to 0 now immediately switches a player away from that weapon if they're actively holding it, instead of leaving it usable until they switch weapons themselves.
- The Heal and Force Field skills now take effect immediately on `/skillup`/`/skilldown` instead of only on the player's next respawn (a zero-value table collision had been masking the immediate-apply path).
- Downgrading Max Shield or Max Force Power now clamps the player's current shield/force pool down to the new cap immediately, instead of leaving it above the cap until spent in combat.
- `/skillup`, `/skilldown`, `/levelup`, and `/leveldown` now reject a zero or negative count instead of silently doing nothing while still reporting success.
- `player_statuses` bitfield is now reset on login and character switch instead of carrying stale flags over.
- `/login`, `/new`, and `/char use` now apply the new character's force powers and weapons immediately and synchronously, instead of relying entirely on the deferred kill/respawn cycle, which silently never applied them at all for a paralyzed or mid-duel player.
- `/logout` now actually clears the player's weapon list, instead of leaving any RPG-unlocked weapon equipped after logging out.
- `/killother` now shares admin permission with `/admkick` instead of requiring the much broader Give Admin permission with no entry of its own; `/teleport`'s Admin Protect check can now be bypassed against yourself like `/give`/`/scale` already could; `/telemark` now requires the same permission `/teleport` does.
- `/adminlist show <player>` and its numeric command-help lookup now report clear errors instead of misparsing a missing player name or silently failing on an out-of-range command number.
- `/char` now prints a proper usage message for a malformed invocation, no longer leaks its database handle, refreshes the client's character list after a removal, and reports clearly when a target character doesn't exist or is already active, instead of silently doing nothing on any of these.
- `/giveitem` now rejects a connected-but-not-logged-in target instead of orphaning the item permanently; `/createitem` now only logs a creation when the insert actually succeeded.
- `/news`/`/newsremove`/`/newsadd` no longer accept a negative count, news ID 0, or a blank channel/text; `/newschannels` now groups channel names case-insensitively, matching `/news` itself.
- `/emote` fixes: worded emote names now match case-insensitively, animation ID 0 is now playable, the duration argument is no longer ignored in favor of a hardcoded value, and a missing `break` that made the Force-sensitive back-flip get-up animation unreachable is fixed.
- `/helpup`'s revive-range check was unreachable, so it had no range requirement at all; the intended ~65-unit range is now actually enforced.
- Several timer/cooldown cvars (`rp_downed_timer`, `rp_downed_invulnerability_timer`, `rp_screen_message_timer`, the flamethrower cooldown, and the maplist/duelboard page size) are now clamped to a safe minimum instead of accepting a zero or negative value that could soft-lock a downed player or remove the flamethrower's cooldown entirely.
- The 3 credit commands now validate their arguments consistently, and `/createcredits` no longer reports success against a target who isn't actually logged in while silently discarding the credited amount.
- A stale mouse-cursor focus in the menu system could silently swallow a click if the cursor hadn't moved since the last mouse-move event; clicks now always re-check what's actually under the cursor.
- Force Drain's visual effect no longer fails to show at skill levels 1-2.

### Security
- Fixed a real SQL injection vulnerability in the `/login`, `/new`, and `/changepassword` account commands, and applied the same parameterized-query treatment across the rest of the database layer (item names, chat/news text, character saves, and related queries) as a broader hardening pass.
- Values of Account and Character-related commands sent from the UI are now stripped of embedded quote characters before being passed to the console command parser, preventing a crafted password from breaking out of its quoted argument.
- Fixed a SQL injection vector in `/createitem`'s item-name query, which spliced the raw name into SQL text instead of using a parameter binding like its sibling item/character queries.
- Fixed a server crash: any connected player, even one not logged in, could crash the whole server with a couple hundred `/playsound` calls using unique nonsense paths, filling the 256-slot sound table that the lookup function otherwise crashes on the instant it's full. It now degrades to an error message instead, `/playsound` requires being logged in, and both `/playsound` and `/playmusic` reject an empty path.
- `/entsave`, `/entload`, and `/entdeletefile`'s filename argument was spliced directly into a filesystem path with no validation; it is now restricted to letters and digits.
- Fixed a stack buffer overflow risk from unbounded string copies in `/emote`'s animation-name argument and in `/maplist`/`/duelboard`'s result-page builders; all now use bounds-checked copies.
- Account/character data loaded from the database is now copied into its fixed-size in-memory buffers with bounds-checked copies instead of unbounded ones; `/register` also now rejects a username or password too long to fit those buffers.
