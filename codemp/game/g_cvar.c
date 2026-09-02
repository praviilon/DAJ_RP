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
