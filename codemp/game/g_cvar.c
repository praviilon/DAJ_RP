/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#include "g_local.h"

//
// Cvar callbacks
//

/*
static void CVU_Derpity( void ) {
	// ...
}
*/

// GalaxyRP fix: [validation] rp_downed_timer, rp_downed_invulnerability_timer and
// rp_screen_message_timer are all read as plain countdown lengths (a value is copied out of the
// cvar once, then only ever decremented toward 0 -- see downedTime in g_combat.c/g_active.c and
// motdTime in g_client.c/g_active.c). None of them validated their value, so a negative setting
// (e.g. a server admin fat-fingering "set rp_downed_timer -30") produced a counter that counted
// away from zero forever instead of toward it, since decrementing a negative number never reaches
// 0 -- permanently soft-locking a downed player (Cmd_Getup_f/can_player_get_up() both gate on
// downedTime == 0) or leaving a MOTD stuck on-screen indefinitely. Clamp back to 0 the moment the
// cvar changes, using this codebase's existing XCVAR update-callback mechanism (see
// G_UpdateCvars() below) rather than re-validating at every read site.
static void RP_ClampNonNegativeCvar(vmCvar_t* cvar, const char* cvarName)
{
	if (cvar->integer < 0)
	{
		trap->Cvar_Set(cvarName, "0");
		trap->Cvar_Update(cvar);
	}
}

void RP_CVU_downedTimer(void)
{
	RP_ClampNonNegativeCvar(&rp_downed_timer, "rp_downed_timer");
}

void RP_CVU_downedInvulnerabilityTimer(void)
{
	RP_ClampNonNegativeCvar(&rp_downed_invulnerability_timer, "rp_downed_invulnerability_timer");
}

void RP_CVU_screenMessageTimer(void)
{
	RP_ClampNonNegativeCvar(&rp_screen_message_timer, "rp_screen_message_timer");
}

// GalaxyRP fix: [validation] zyk_flame_thrower_cooldown is read unclamped in Player_FireFlameThrower()
// (g_main.c) as self->client->cloakDebReduce = level.time + zyk_flame_thrower_cooldown.integer -- a
// negative value pushes cloakDebReduce into the past, so the "cloakDebReduce < level.time" cooldown
// gate is satisfied on effectively every server frame instead of respecting any cooldown at all,
// letting the flamethrower re-fire (and re-deal damage) as fast as the server tick rate allows. Clamp
// back to 0 the moment the cvar changes, same as the timer cvars above.
void RP_CVU_flameThrowerCooldown(void)
{
	RP_ClampNonNegativeCvar(&zyk_flame_thrower_cooldown, "zyk_flame_thrower_cooldown");
}

// GalaxyRP fix: [validation] zyk_list_cmds_results_per_page is read as results_per_page in both
// Cmd_MapList_f and Cmd_DuelBoard_f (g_cmds.c), where it gates both pagination loop bounds:
// results_per_page*(page-1) and results_per_page*page. When results_per_page is 0 (or negative),
// both bounds evaluate to <= 0, so neither the skip-loop nor the read-loop ever runs for any page
// number -- the commands silently print a blank page instead of an error, for every page, until the
// cvar is corrected. Unlike the timer cvars above, 0 is not a safe floor here since it reproduces
// the exact same bug those loops have with a negative value -- clamp to a minimum of 1 instead.
void RP_CVU_listCmdsResultsPerPage(void)
{
	if (zyk_list_cmds_results_per_page.integer < 1)
	{
		trap->Cvar_Set("zyk_list_cmds_results_per_page", "1");
		trap->Cvar_Update(&zyk_list_cmds_results_per_page);
	}
}

// GalaxyRP fix: [validation] same class as the timer cvars above, for the duel/minigame cvars that
// were missed by that pass. Each of these has a value range below which its feature does not merely
// behave oddly but becomes permanently unusable, so each clamps to the lowest value that still
// works rather than to 0 (which for most of them IS the broken value). A clamped-away setting is
// reported the same way the others are -- by simply correcting the cvar, so an admin who inspects it
// sees the value actually in force.
static void RP_ClampCvarMinimum(vmCvar_t* cvar, const char* cvarName, int minimum)
{
	if (cvar->integer < minimum)
	{
		trap->Cvar_Set(cvarName, va("%i", minimum));
		trap->Cvar_Update(cvar);
	}
}

// zyk_duel_radius is compared against the distance between the two duelists every frame
// (g_active.c): at 0 or below, "too far apart" is true immediately, so every private duel ends on
// its very first frame and duelling is impossible. 100 units is close quarters but functional.
void RP_CVU_duelRadius(void)
{
	RP_ClampCvarMinimum(&zyk_duel_radius, "zyk_duel_radius", 100);
}

// The Duel Tournament arena's kill radius is DUEL_TOURNAMENT_ARENA_SIZE * scale / 100 (g_main.c),
// while duelists are teleported to +/-125 units from the arena centre. Below a scale of 200 that
// radius is smaller than the distance they spawn at, so both duelists are killed for leaving the
// arena on the first frame of every match.
void RP_CVU_duelTournamentArenaScale(void)
{
	RP_ClampCvarMinimum(&zyk_duel_tournament_arena_scale, "zyk_duel_tournament_arena_scale", 200);
}

// Duelists are frozen in place for the first DUEL_TOURNAMENT_PROTECT_TIME (2000ms) of a match
// (bg_pmove.c). A duel_time at or below that leaves them frozen for the entire match, so every
// match runs its clock out at full health and is scored as a tie -- no duel is ever actually
// fought. 5000ms gives a (very short) 3 seconds of real duelling.
void RP_CVU_duelTournamentDuelTime(void)
{
	RP_ClampCvarMinimum(&zyk_duel_tournament_duel_time, "zyk_duel_tournament_duel_time", 5000);
}

// Both of these set "<timer> = level.time + cvar" when the first player signs up, and the mode's
// per-frame handler ends the event the moment that timer elapses with too few players. At 0 or
// below the timer is already in the past, so the first person to join instantly ends the event they
// just started and the mode can never be entered at all.
void RP_CVU_duelTournamentTimeToStart(void)
{
	RP_ClampCvarMinimum(&zyk_duel_tournament_time_to_start, "zyk_duel_tournament_time_to_start", 1000);
}

void RP_CVU_sniperBattleTimeToStart(void)
{
	RP_ClampCvarMinimum(&zyk_sniper_battle_time_to_start, "zyk_sniper_battle_time_to_start", 1000);
}

// GalaxyRP: [Dice] rp_dice_roll_cooldown is added to level.time to schedule pers.dice_roll_timer.
// 0 is a legitimate setting (it means "no cooldown"), and a negative value would mean the same thing
// while reading as if it did something else -- so the floor is 0 rather than a minimum that works.
// The ceiling matters more: level.time is milliseconds since the map loaded, so on a server that has
// been up for a day an unbounded cooldown could push level.time + cooldown past INT_MAX and wrap the
// timer into the past, silently disabling the very cooldown it was set to enforce. A minute is far
// beyond any sane RP setting and leaves the sum nowhere near the limit.
void RP_CVU_diceRollCooldown(void)
{
	if (rp_dice_roll_cooldown.integer < 0)
	{
		trap->Cvar_Set("rp_dice_roll_cooldown", "0");
		trap->Cvar_Update(&rp_dice_roll_cooldown);
	}
	else if (rp_dice_roll_cooldown.integer > 60000)
	{
		trap->Cvar_Set("rp_dice_roll_cooldown", "60000");
		trap->Cvar_Update(&rp_dice_roll_cooldown);
	}
}


//
// Cvar table
//

typedef struct cvarTable_s {
	vmCvar_t	*vmCvar;
	char		*cvarName;
	char		*defaultString;
	void		(*update)( void );
	uint32_t	cvarFlags;
	qboolean	trackChange; // announce if value changes
} cvarTable_t;

#define XCVAR_DECL
	#include "g_xcvar.h"
#undef XCVAR_DECL

static const cvarTable_t gameCvarTable[] = {
	#define XCVAR_LIST
		#include "g_xcvar.h"
	#undef XCVAR_LIST
};
static const size_t gameCvarTableSize = ARRAY_LEN( gameCvarTable );

void G_RegisterCvars( void ) {
	size_t i = 0;
	const cvarTable_t *cv = NULL;

	for ( i=0, cv=gameCvarTable; i<gameCvarTableSize; i++, cv++ ) {
		trap->Cvar_Register( cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags );
		if ( cv->update )
			cv->update();
	}
}

void G_UpdateCvars( void ) {
	size_t i = 0;
	const cvarTable_t *cv = NULL;

	for ( i=0, cv=gameCvarTable; i<gameCvarTableSize; i++, cv++ ) {
		if ( cv->vmCvar ) {
			int modCount = cv->vmCvar->modificationCount;
			trap->Cvar_Update( cv->vmCvar );
			if ( cv->vmCvar->modificationCount != modCount ) {
				if ( cv->update )
					cv->update();

				if ( cv->trackChange )
					trap->SendServerCommand( -1, va("print \"Server: %s changed to %s\n\"", cv->cvarName, cv->vmCvar->string ) );
			}
		}
	}
}
