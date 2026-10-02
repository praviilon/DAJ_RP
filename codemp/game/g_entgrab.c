/*
===========================================================================
GalaxyRP: [Entity System] picking entities up, turning them and putting them down

  /entcopy [id]    picks up a copy of the entity aimed at (or of that id); the original stays as it is.
                   /entcopy again drops the copy where the admin is aiming.
  /entcut [id]     picks up the entity itself: it disappears (unlinked and inert, its record untouched)
                   until /entcut again moves it where the admin is aiming.
  /entrotate       turns what is held, or else the entity aimed at: 45 degrees of yaw with no
                   arguments, "/entrotate <yaw>" by that much yaw, "/entrotate <pitch> <yaw> <roll>" to
                   exactly those angles.
  /entcancel       lets go: a copy is simply not made, a cut entity goes back where it was.
  /entaddaim       is /entadd placing the new entity on the surface aimed at (Cmd_EntAddAim_f, g_cmds.c,
                   through RP_EntAddAimPoint() and RP_EntGrabPlace() below).

Adapted from Lugormod's grab and clone tools, with the guards this mod's entity system needs:

  - only an entity with a key/value record (the map's, or one an entity command or file made) can be
    picked up -- the same line /entedit and /entremove draw (RP_EntityRefusalReason, g_spawn.c);
  - the map's own entities -- from its entity string or its per-map fixes, marked M in /entlist -- can be
    copied but never cut or rotated in place, nor edited or removed (gentity_t::rpMapEntity, set by
    RP_MarkMapEntities() below) -- except its pickups, dispensers and decor that nothing in the map links
    to, tagged E (RP_MapEntityExempt());
  - a brush entity can be copied but never cut or rotated: its angles are the direction it moves in,
    not a facing, and a brush entity of the map is protected anyway;
  - an entity from a misc_bsp's sub-BSP goes with its misc_bsp, and a permanent (neverFree) entity is
    the game's, so neither is picked up;
  - one hold per admin, and an entity one admin holds is refused to every other entity command
    (gentity_t::rpHeldBy), so nothing edits, removes or picks up an entity out from under a hold.

Where it lands: the trace from the admin's eye along the view, up to 2048 units, and the entity's box
set against the surface hit so it rests on it (RP_EntGrabPlace). With nothing within reach it floats
256 units in front. While held, the admin -- and only the admin -- sees where it would land: the
entity's own model following the aim when it has one to show (a "ghost", a record-less entity with no
contents, see RP_GrabMakeGhost), and its box drawn in lines either way (blue for a copy, orange for a
cut entity), with a short line showing which way it faces.

The hold lives in the admin's clientPersistant_t (pers.entHold*), so dying does not drop it. It is let go
of -- a cut entity put back -- when the admin disconnects, logs out, loses the Entity System power, or
the entity goes (an /entload); checked every frame by RP_EntGrabFrame().
===========================================================================
*/

#include "g_local.h"

extern qboolean check_admin_command( gentity_t *ent, int admin_command, qboolean with_message );
extern void zyk_main_set_entity_field( gentity_t *ent, char *key, char *value );
extern void zyk_main_spawn_entity( gentity_t *ent );
extern qboolean zyk_spawn_strings_full( gentity_t *ent );

#define RP_GRAB_RANGE			2048.0f	// how far the aim reaches (/entcopy, /entcut)
#define RP_ADDAIM_RANGE			32768.0f	// how far /entaddaim's aim reaches: across any map
#define RP_ADDAIM_MASK			( CONTENTS_SOLID | CONTENTS_TERRAIN )	// /entaddaim: through players, NPCs and corpses
#define RP_GRAB_FLOAT			256.0f	// where a held entity floats with nothing within reach
#define RP_GRAB_PREVIEW_MSEC	200		// the preview lines are redrawn this often...
#define RP_GRAB_PREVIEW_LIFE	250		// ...and last a little longer, so they never blink
#define RP_GRAB_FACING			24.0f	// length of the line showing which way it faces
#define RP_GRAB_CROSS			8.0f	// half the length of each arm of a point entity's cross
#define RP_GRAB_ANGLE_LIMIT		100000.0f
#define RP_GRAB_COLOR_COPY		SABER_BLUE
#define RP_GRAB_COLOR_CUT		SABER_ORANGE

// zyk: the preview entity's classname. Compared by address, so nothing else can pass for one.
static char rp_hold_ghost_classname[] = "rp_hold_ghost";

/*
=============================================================================

MAP ENTITIES

=============================================================================
*/

// zyk: FNV-1a over every key and value of the entity's record, in order. The step after each string
// is a multiply with nothing folded in, which no character does, so "ab" "c" and "a" "bc" differ.
static uint64_t RP_RecordFingerprint( int num )
{
	uint64_t h = 1469598103934665603ULL;
	int count = level.zyk_spawn_strings_values_count[num];
	int i;

	if ( count > ZYK_MAX_SPAWN_STRING_SLOTS )
		count = ZYK_MAX_SPAWN_STRING_SLOTS;

	for ( i = 0; i < count; i++ )
	{
		const unsigned char *p = (const unsigned char *)( level.zyk_spawn_strings[num][i] ? level.zyk_spawn_strings[num][i] : "" );

		for ( ; *p; p++ )
		{
			h ^= *p;
			h *= 1099511628211ULL;
		}
		h *= 1099511628211ULL;
	}

	return h;
}

static int RP_CompareFingerprints( const void *a, const void *b )
{
	uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;

	return ( x < y ) ? -1 : ( x > y ) ? 1 : 0;
}

/*
==================
RP_MarkMapEntities

Once the map has loaded -- its entity string spawned and every per-map fix applied, before the default
entity file clears anything (G_InitGame, next to RP_RecordFallbackSpawnPoint) -- every entity with a
key/value record is one the map put there: marked rpMapEntity, and its record fingerprinted.

The default entity file and /entload free those and spawn the file's lines instead, and a line /entsave
wrote for a map entity is that entity's record, key for key. The loader gives rpMapEntity back to a line
whose fingerprint is one of these (RP_RecordIsMapEntity), so a preset does not strip the protection.
/entedit and /entremove refuse map entities as well now (g_cmds.c); a map entity edited before they
did, in a preset saved then, no longer matches, and loads as an ordinary entity -- the file is its
record now.
==================
*/
void RP_MarkMapEntities( void )
{
	gentity_t *e;
	int n = 0;

	RP_FOR_EACH_ENTITY( e )
	{
		int num = e - g_entities;

		if ( !e->inuse || num < MAX_CLIENTS + BODY_QUEUE_SIZE || num == ENTITYNUM_WORLD || num == ENTITYNUM_NONE )
			continue;
		if ( level.zyk_spawn_strings_values_count[num] <= 0 )
			continue;

		e->rpMapEntity = qtrue;
		if ( n < MAX_ENTITIESTOTAL )
			level.rp_map_fingerprints[n++] = RP_RecordFingerprint( num );
	}

	qsort( level.rp_map_fingerprints, n, sizeof( level.rp_map_fingerprints[0] ), RP_CompareFingerprints );
	level.rp_num_map_fingerprints = n;

	// zyk: from here on, an entity with a record that is not one of these is the Entity System's
	level.rp_map_loaded = qtrue;
}

qboolean RP_RecordIsMapEntity( int num )
{
	uint64_t h;

	if ( num < 0 || num >= MAX_ENTITIESTOTAL || level.zyk_spawn_strings_values_count[num] <= 0 || level.rp_num_map_fingerprints <= 0 )
		return qfalse;

	h = RP_RecordFingerprint( num );

	return bsearch( &h, level.rp_map_fingerprints, level.rp_num_map_fingerprints, sizeof( level.rp_map_fingerprints[0] ),
		RP_CompareFingerprints ) ? qtrue : qfalse;
}

/*
=============================================================================

RECORD HELPERS

=============================================================================
*/

static const char *RP_GrabRecordValue( int num, const char *key )
{
	int count = level.zyk_spawn_strings_values_count[num];
	int i;

	if ( count > ZYK_MAX_SPAWN_STRING_SLOTS )
		count = ZYK_MAX_SPAWN_STRING_SLOTS;

	for ( i = 0; i + 1 < count; i += 2 )
	{
		if ( level.zyk_spawn_strings[num][i] && Q_stricmp( level.zyk_spawn_strings[num][i], key ) == 0 )
			return level.zyk_spawn_strings[num][i + 1] ? level.zyk_spawn_strings[num][i + 1] : "";
	}

	return NULL;
}

/*
==================
RP_MapEntityExempt / RP_MapEntityProtected / RP_MapEntityLinkKey

GalaxyRP: [Entity System] the map's own entities (rpMapEntity) are left as the map made them by every
command that changes an entity -- /entedit, /entremove, /entcut, /entrotate -- except the map's pickups,
dispensers and decor that nothing in the map is linked to: those are tagged E in /entlist instead of M,
and the commands treat them as any other entity.

The class decides first (rp_map_exempt_classes): the self-contained ones, that spawn no other entity and
keep no state anywhere else. Then the record, as the map wrote it: an entity with a targetname or a
script_targetname is one something in the map uses or a script drives (a pickup with one is hidden until
it is triggered); one with a team belongs to a group (items: only one of the group is out at a time);
and one with a target fires it -- a pickup when it is taken, a breakable when it breaks. Any of those,
with a value, keeps it protected. RP_MapEntityLinkKey() names the first such key, for the messages.

An E entity an admin changes -- an edit applied, a rotation, a cut dropped somewhere -- stops being the
map's (rpMapEntity cleared), as a changed map entity in an entity file always has: it is an ordinary
entity from then on, and the Entity System's spawn checks (RP_EntitySystemSpawnRefused) apply to it.
==================
*/
static const char *rp_map_exempt_classes[] = {
	// weapons
	"weapon_stun_baton", "weapon_melee", "weapon_saber", "weapon_bryar_pistol", "weapon_blaster",
	"weapon_disruptor", "weapon_bowcaster", "weapon_repeater", "weapon_demp2", "weapon_flechette",
	"weapon_concussion_rifle", "weapon_rocket_launcher", "weapon_thermal", "weapon_trip_mine", "weapon_det_pack",
	// ammo
	"ammo_force", "ammo_blaster", "ammo_powercell", "ammo_metallic_bolts", "ammo_rockets", "ammo_thermal",
	"ammo_tripmine", "ammo_detpack", "ammo_all",
	// health and shields
	"item_medpak_instant", "item_shield_sm_instant", "item_shield_lrg_instant",
	// holdables
	"item_seeker", "item_shield", "item_medpac", "item_medpac_big", "item_binoculars", "item_sentry_gun",
	"item_jetpack", "item_healthdisp", "item_ammodisp", "item_eweb_holdable", "item_cloak",
	// boons and powerups
	"item_force_boon", "item_ysalimari", "item_force_enlighten_light", "item_force_enlighten_dark",
	// dispensers and racks
	"misc_ammo_floor_unit", "misc_shield_floor_unit", "misc_model_health_power_converter",
	"misc_model_shield_power_converter", "misc_model_ammo_power_converter", "misc_model_gun_rack",
	"misc_model_ammo_rack",
	// decor
	"misc_model_breakable", "fx_runner", "target_speaker", "misc_exploding_crate", "misc_gas_tank",
	NULL
};

static const char *rp_map_link_keys[] = {
	"targetname", "script_targetname", "team",
	"target", "target2", "target3", "target4", "target5", "target6",
	NULL
};

static qboolean RP_MapExemptClass( const char *classname )
{
	int i;

	if ( !classname )
		return qfalse;

	for ( i = 0; rp_map_exempt_classes[i]; i++ )
	{
		if ( Q_stricmp( classname, rp_map_exempt_classes[i] ) == 0 )
			return qtrue;
	}

	return qfalse;
}

const char *RP_MapEntityLinkKey( const gentity_t *ent )
{
	int num, i;

	if ( !ent )
		return NULL;

	num = (int)( ent - g_entities );
	if ( num < 0 || num >= MAX_ENTITIESTOTAL )
		return NULL;

	for ( i = 0; rp_map_link_keys[i]; i++ )
	{
		const char *value = RP_GrabRecordValue( num, rp_map_link_keys[i] );

		if ( value && value[0] )
			return rp_map_link_keys[i];
	}

	return NULL;
}

qboolean RP_MapEntityExempt( const gentity_t *ent )
{
	if ( !ent || !ent->inuse || !ent->rpMapEntity )
		return qfalse;

	return ( RP_MapExemptClass( ent->classname ) && !RP_MapEntityLinkKey( ent ) ) ? qtrue : qfalse;
}

qboolean RP_MapEntityProtected( const gentity_t *ent )
{
	return ( ent && ent->rpMapEntity && !RP_MapEntityExempt( ent ) ) ? qtrue : qfalse;
}

// GalaxyRP: [Entity System] the class is on the exempt list but the map links it to something: the
// reason it is M, for a refusal message (", the map links it to other entities (<key>)"), or "".
const char *RP_MapEntityRefusalNote( const gentity_t *ent )
{
	const char *key;

	if ( !ent || !RP_MapExemptClass( ent->classname ) || !( key = RP_MapEntityLinkKey( ent ) ) )
		return "";

	return va( ": the map links it to other entities (%s)", key );
}

// zyk: sets a key of the record, and says so -- zyk_main_set_entity_field() silently drops a new key
// when the record is full
static qboolean RP_GrabSetKey( gentity_t *e, const char *key, const char *value )
{
	if ( !RP_GrabRecordValue( e->s.number, key ) && zyk_spawn_strings_full( e ) )
		return qfalse;

	zyk_main_set_entity_field( e, (char *)key, (char *)value );
	return qtrue;
}

/*
==================
RP_GrabAnglesKey / RP_GrabRecordAngles

Which key turns the entity. A mover class that carries an md3 model (func_door, func_plat, func_usable
and the other classes zyk_set_brush_model() gives one) faces by angles2 -- that is what it copies into
the model's orientation, and a door's or button's angles are its direction of movement. func_static and
func_pendulum are the two that set the orientation from angles themselves afterwards (G_SetAngles, the
pendulum's swing base), so angles2 would do nothing on them. Everything else faces by angles, and the
older "angle" (yaw alone) is folded into it.
==================
*/
static const char *RP_GrabAnglesKey( const gentity_t *e )
{
	const char *classname = RP_GrabRecordValue( e->s.number, "classname" );
	const char *model = RP_GrabRecordValue( e->s.number, "model" );

	if ( !e->r.bmodel && classname && Q_stricmpn( classname, "func_", 5 ) == 0 &&
		Q_stricmp( classname, "func_static" ) != 0 && Q_stricmp( classname, "func_pendulum" ) != 0 &&
		model && model[0] && model[0] != '*' && model[0] != '#' )
		return "angles2";

	return "angles";
}

static void RP_GrabRecordAngles( const gentity_t *e, const char *key, vec3_t out )
{
	int num = e->s.number;
	const char *value = RP_GrabRecordValue( num, key );

	VectorClear( out );

	if ( value )
	{
		if ( sscanf( value, "%f %f %f", &out[0], &out[1], &out[2] ) != 3 )
			VectorClear( out );
	}
	else if ( Q_stricmp( key, "angles" ) == 0 && ( value = RP_GrabRecordValue( num, "angle" ) ) != NULL )
	{
		out[YAW] = atof( value );
	}
	else
	{
		// zyk: no key: the angles it spawned with, which a class may have given it by default (an
		// fx_runner with none points up) -- turning from zero would throw that default away
		VectorCopy( Q_stricmp( key, "angles2" ) == 0 ? e->s.angles2 : e->s.angles, out );
	}

	// zyk: a hand-edited file can hold anything
	if ( !( out[0] > -RP_GRAB_ANGLE_LIMIT && out[0] < RP_GRAB_ANGLE_LIMIT ) ||
		!( out[1] > -RP_GRAB_ANGLE_LIMIT && out[1] < RP_GRAB_ANGLE_LIMIT ) ||
		!( out[2] > -RP_GRAB_ANGLE_LIMIT && out[2] < RP_GRAB_ANGLE_LIMIT ) )
	{
		VectorClear( out );
	}
}

// zyk: writes the angles into the record under key, and takes out an "angle" that would otherwise be
// read after them. False if the record has no room for the key.
static qboolean RP_GrabWriteAngles( gentity_t *e, const char *key, const vec3_t angles )
{
	if ( !RP_GrabSetKey( e, key, va( "%g %g %g", angles[0], angles[1], angles[2] ) ) )
		return qfalse;

	if ( Q_stricmp( key, "angles" ) == 0 && RP_GrabRecordValue( e->s.number, "angle" ) )
		zyk_main_set_entity_field( e, "angle", "zykremovekey" );

	return qtrue;
}

// zyk: an angle in [0, 360). AngleNormalize360() goes through a 16-bit short and would write 29.9982
// for 30 into the record.
static float RP_GrabNormalizeAngle( float a )
{
	a = fmod( a, 360.0f );
	if ( a < 0.0f )
		a += 360.0f;
	if ( a >= 360.0f )
		a = 0.0f;
	return a;
}

// zyk: how many new keys the record must still have room for: origin, and the angles key
static qboolean RP_GrabRecordHasRoom( const gentity_t *e )
{
	int needed = 0;

	if ( !RP_GrabRecordValue( e->s.number, "origin" ) )
		needed += 2;
	if ( !RP_GrabRecordValue( e->s.number, RP_GrabAnglesKey( e ) ) )
		needed += 2;

	return ( level.zyk_spawn_strings_values_count[e->s.number] + needed <= ZYK_MAX_SPAWN_STRING_SLOTS ) ? qtrue : qfalse;
}

/*
==================
RP_EntGrabRespawnInPlace

Spawn an entity again from its record, in the slot it has -- what /entedit does. The trigger a door or
platform made for itself goes first (it makes a new one), and so do a misc_bsp's sub-BSP entities (it
rebuilds them), or there would be two of each, and a Ghoul2 model. What a spawn function does beyond
setting the entity up happens again, as it does for /entedit: a spawn script runs a second time, and an
NPC spawner that spawns at once spawns again.
==================
*/
void RP_EntGrabRespawnInPlace( gentity_t *e )
{
	if ( !e || !e->inuse )
		return;

	if ( e->rpBSPInstance > 0 )
	{
		RP_FreeSubBSPEntities( e->rpBSPInstance );
		e->rpBSPInstance = 0;
	}

	RP_FreeEntityTriggers( e );

	// zyk: a class with a Ghoul2 model (misc_turretG2) builds it again in its spawn function, onto the
	// instance it already has -- a second model on it, and the first never freed. G_FreeEntity() frees
	// it the same way.
	if ( e->ghoul2 )
		trap->G2API_CleanGhoul2Models( &e->ghoul2 );

	zyk_main_spawn_entity( e );
}

// zyk: room for a new entity in the region this route leads to. A networked one keeps /entadd's margin,
// for /entadd's reason -- some classes take more than the one slot. G_SpawnLogical() ends the server when
// the logical region is full, as G_Spawn() does, so a logical one asks too.
static qboolean RP_GrabRouteHasRoom( const rpSpawnRoute_t *route )
{
	if ( RP_SpawnRouteIsLogical( route ) )
		return ( G_FreeLogicalEntityCount() > 16 ) ? qtrue : qfalse;

	return G_EntitySlotsAvailable( 4 );
}

// zyk: an entity spawned again from a copy of its old record, in a new slot, for when the spawn from
// the new one removed it. NULL if there is no room or it does not survive this either.
static gentity_t *RP_GrabRebuild( char **pairs, int count )
{
	rpSpawnRoute_t route;
	gentity_t *e;
	int i;

	RP_SpawnRouteInit( &route );
	for ( i = 0; i + 1 < count; i += 2 )
		RP_SpawnRouteNoteKey( &route, pairs[i], pairs[i + 1] );

	if ( !RP_GrabRouteHasRoom( &route ) )
		return NULL;

	e = RP_SpawnForRoute( &route );
	if ( !e )
		return NULL;

	for ( i = 0; i < count; i++ )
		level.zyk_spawn_strings[e->s.number][i] = pairs[i];
	level.zyk_spawn_strings_values_count[e->s.number] = count;

	// GalaxyRP: [Entity System] the old record may be one of the map's (an E entity whose move or turn
	// did not survive): put back, it is the map's again, as an entity file's line for it would be
	e->rpMapEntity = RP_RecordIsMapEntity( e->s.number );

	zyk_main_spawn_entity( e );

	return e->inuse ? e : NULL;
}

/*
=============================================================================

PLACEMENT

=============================================================================
*/

/*
==================
RP_EntGrabAimPoint / RP_EntAddAimPoint

Where the admin is aiming: the first thing the trace stops at along the view from the eye, and the
normal of the surface there. qfalse, with the point 256 units ahead and no normal, when there is none.
/entcopy and /entcut drop within 2048 units, on anything a shot would hit (RP_EntGrabAimPoint).
GalaxyRP: [Entity System] /entaddaim reaches across any map and sees only solid world, solid entities
and terrain, so a player or NPC in the way is aimed through rather than built on (RP_EntAddAimPoint).
==================
*/
static qboolean RP_AimPoint( gentity_t *ent, float range, int mask, vec3_t point, vec3_t normal )
{
	vec3_t eye, dir, end;
	trace_t tr;

	VectorCopy( ent->client->ps.origin, eye );
	eye[2] += ent->client->ps.viewheight;
	AngleVectors( ent->client->ps.viewangles, dir, NULL, NULL );
	VectorMA( eye, range, dir, end );

	trap->Trace( &tr, eye, vec3_origin, vec3_origin, end, ent->s.number, mask, qfalse, 0, 0 );

	if ( !tr.startsolid && !tr.allsolid && tr.fraction < 1.0f )
	{
		VectorCopy( tr.endpos, point );
		VectorCopy( tr.plane.normal, normal );

		// zyk: a hit with no plane to speak of faces back along the view
		if ( VectorLengthSquared( normal ) < 0.25f )
			VectorScale( dir, -1.0f, normal );

		return qtrue;
	}

	VectorMA( eye, RP_GRAB_FLOAT, dir, point );
	VectorClear( normal );
	return qfalse;
}

qboolean RP_EntGrabAimPoint( gentity_t *ent, vec3_t point, vec3_t normal )
{
	return RP_AimPoint( ent, RP_GRAB_RANGE, MASK_SHOT, point, normal );
}

qboolean RP_EntAddAimPoint( gentity_t *ent, vec3_t point, vec3_t normal )
{
	return RP_AimPoint( ent, RP_ADDAIM_RANGE, RP_ADDAIM_MASK, point, normal );
}

/*
==================
RP_EntGrabPlaceBox

The box the entity is placed by. A spawn point, an NPC spawner and a teleport destination put a player
or an NPC where they are, so they are placed by a player's box -- standing on the floor rather than
buried in it. Anything else by its own collision box; a point entity, and anything not in the world,
has none and is placed by its origin. So is an entity whose box collides with nothing -- neither solid
nor a trigger nor a brush model: an fx_runner's box only makes it reach the players near it, and set on
a floor by that box its effect would play 33 units above it; a non-solid model is drawn from its origin.
==================
*/
void RP_EntGrabPlaceBox( const char *classname, const gentity_t *e, vec3_t mins, vec3_t maxs )
{
	if ( classname && ( Q_stricmpn( classname, "info_player_", 12 ) == 0 || Q_stricmpn( classname, "NPC_", 4 ) == 0 ||
		Q_stricmp( classname, "misc_teleporter_dest" ) == 0 ) )
	{
		VectorSet( mins, -16, -16, DEFAULT_MINS_2 );
		VectorSet( maxs, 16, 16, DEFAULT_MAXS_2 );
		return;
	}

	if ( e && e->inuse && !e->isLogical )
	{
		if ( !e->r.bmodel && !( e->r.contents & ( MASK_SHOT | CONTENTS_TRIGGER ) ) )
		{
			VectorClear( mins );
			VectorClear( maxs );
			return;
		}

		// zyk: a turned brush entity collides as its model turned, and the engine's world box for it
		// (less the unit SV_LinkEntity() adds each way) encloses that; its own bounds are unturned
		if ( e->r.bmodel && e->r.linked && ( e->r.currentAngles[0] || e->r.currentAngles[1] || e->r.currentAngles[2] ) )
		{
			VectorSubtract( e->r.absmin, e->r.currentOrigin, mins );
			VectorSubtract( e->r.absmax, e->r.currentOrigin, maxs );
			mins[0] += 1.0f; mins[1] += 1.0f; mins[2] += 1.0f;
			maxs[0] -= 1.0f; maxs[1] -= 1.0f; maxs[2] -= 1.0f;
			return;
		}

		VectorCopy( e->r.mins, mins );
		VectorCopy( e->r.maxs, maxs );
		return;
	}

	VectorClear( mins );
	VectorClear( maxs );
}

/*
==================
RP_EntGrabPlace

The origin that sets a box with these bounds against the surface at point: along the surface normal's
strongest axis the box's near face sits one unit off the surface, and across it the box is centred on
the point. Whole units, rounded away from the surface -- but a trace's end point is a hair off the plane
either way, so a hundredth of a unit of that is forgiven first, or a box on a floor at height 0 would
float at 18 rather than 17 as often as not. With no normal (nothing was hit) the box is centred on the
point.
==================
*/
void RP_EntGrabPlace( const vec3_t point, const vec3_t normal, const vec3_t mins, const vec3_t maxs, vec3_t origin )
{
	int axis = -1;
	float best = 0.0f;
	int k;

	for ( k = 0; k < 3; k++ )
	{
		if ( fabs( normal[k] ) > best )
		{
			best = fabs( normal[k] );
			axis = k;
		}
	}

	for ( k = 0; k < 3; k++ )
	{
		if ( k == axis && normal[k] > 0.0f )
			origin[k] = ceil( point[k] - mins[k] + 1.0f - 0.01f );
		else if ( k == axis )
			origin[k] = floor( point[k] - maxs[k] - 1.0f + 0.01f );
		else
			origin[k] = floor( point[k] - ( mins[k] + maxs[k] ) * 0.5f + 0.5f );
	}
}

// zyk: a living player or NPC inside the box at origin -- a solid entity is not dropped on them
static qboolean RP_GrabOccupied( const vec3_t origin, const vec3_t mins, const vec3_t maxs )
{
	int list[MAX_GENTITIES];
	vec3_t absmin, absmax;
	int count, i;

	VectorAdd( origin, mins, absmin );
	VectorAdd( origin, maxs, absmax );

	count = trap->EntitiesInBox( absmin, absmax, list, MAX_GENTITIES );

	for ( i = 0; i < count; i++ )
	{
		gentity_t *e;

		if ( list[i] < 0 || list[i] >= MAX_GENTITIES )
			continue;

		e = &g_entities[list[i]];
		if ( !e->inuse || !e->client || e->health <= 0 )
			continue;
		if ( e->s.number < MAX_CLIENTS && e->client->sess.sessionTeam == TEAM_SPECTATOR )
			continue;

		return qtrue;
	}

	return qfalse;
}

// GalaxyRP: [Entity System] whether this entity, just spawned, is solid and has a living player or NPC
// inside it -- for the warning /entadd and /entaddaim give (its world box, less the unit the engine adds
// to it each way)
qboolean RP_EntitySolidAroundSomeone( const gentity_t *e )
{
	vec3_t mins, maxs;

	if ( !e || !e->inuse || e->isLogical || !e->r.linked || !( e->r.contents & ( CONTENTS_SOLID | CONTENTS_BODY ) ) )
		return qfalse;

	mins[0] = e->r.absmin[0] + 1.0f; mins[1] = e->r.absmin[1] + 1.0f; mins[2] = e->r.absmin[2] + 1.0f;
	maxs[0] = e->r.absmax[0] - 1.0f; maxs[1] = e->r.absmax[1] - 1.0f; maxs[2] = e->r.absmax[2] - 1.0f;

	return RP_GrabOccupied( vec3_origin, mins, maxs );
}

/*
==================
RP_EntGrabSettle

An entity just spawned where the aim put it, placed by the box it was expected to have: now that its real
box is known (a misc_model_breakable's comes from its model file, a brush entity's from its model), set
that box against the surface at point and make the entity again there -- freed and spawned anew in
another slot from its record with the new origin, rather than spawned again in place, so nothing its
spawn function set up (a Ghoul2 model, ICARUS state) is set up twice on one entity. /entaddaim uses it
for every new entity, and a dropped copy for one whose box is not the original's (a copy of one of the
map's models gets its model's own box, the original keeps the map's).

Left as it is when it has no box it is placed by (RP_EntGrabPlaceBox), is already where that box puts it,
there is no room, or its spawn made other entities than a trigger of its own -- which goes with it, but
the others a second spawn would make twice (a misc_bsp's sub-BSP). freeBefore is G_FreeEntityCount()
from just before that spawn. Returns the entity as it now is: the same one, the one made anew, or NULL if
the new one did not survive its spawn.
==================
*/
/*
==================
RP_EntGrabScaleShift

How far a misc_model_breakable's spawn function will lift the origin it is given: with a modelscale it
raises it by what the scale took off the bottom of its box, as single player does, to keep the box's
bottom where it was. The commands that put a model somewhere give it that much less, so it ends up
exactly where they put it.
==================
*/
static float RP_EntGrabScaleShift( const gentity_t *e )
{
	float scale = e->modelScale[2];

	if ( !e->classname || Q_stricmp( e->classname, "misc_model_breakable" ) != 0 )
		return 0.0f;
	if ( !( scale > 0.0f ) || scale == 1.0f )
		return 0.0f;

	return e->r.mins[2] / scale - e->r.mins[2];
}

// zyk: an origin key for an entity that is to end up at origin
static const char *RP_EntGrabOriginKey( const gentity_t *e, const vec3_t origin )
{
	return va( "%i %i %i", (int)origin[0], (int)origin[1], (int)floor( origin[2] - RP_EntGrabScaleShift( e ) + 0.5f ) );
}

gentity_t *RP_EntGrabSettle( gentity_t *e, const vec3_t point, const vec3_t normal, int freeBefore )
{
	float shift;
	static char *pairs[ZYK_MAX_SPAWN_STRING_SLOTS];
	vec3_t mins, maxs, origin;
	rpSpawnRoute_t route;
	gentity_t *other, *moved;
	int triggers = 0;
	int count, k;

	if ( !e || !e->inuse || e->isLogical )
		return e;

	RP_EntGrabPlaceBox( NULL, e, mins, maxs );
	if ( VectorCompare( mins, vec3_origin ) && VectorCompare( maxs, vec3_origin ) )
		return e;

	RP_EntGrabPlace( point, normal, mins, maxs, origin );
	if ( VectorCompare( origin, e->s.origin ) )
		return e;

	// zyk: a solid one is not made again around someone standing there
	if ( ( e->r.contents & ( CONTENTS_SOLID | CONTENTS_BODY ) ) && RP_GrabOccupied( origin, mins, maxs ) )
		return e;

	RP_FOR_EACH_ENTITY( other )
	{
		if ( other != e && other->inuse && other->parent == e && ( other->r.contents & CONTENTS_TRIGGER ) && !RP_EntityHasSpawnKeys( other ) )
			triggers++;
	}

	if ( freeBefore - G_FreeEntityCount() > triggers || !G_EntitySlotsAvailable( 4 ) )
		return e;

	shift = RP_EntGrabScaleShift( e );

	count = level.zyk_spawn_strings_values_count[e->s.number];
	if ( count > ZYK_MAX_SPAWN_STRING_SLOTS )
		count = ZYK_MAX_SPAWN_STRING_SLOTS;
	memcpy( pairs, level.zyk_spawn_strings[e->s.number], sizeof( pairs[0] ) * count );

	RP_SpawnRouteInit( &route );
	for ( k = 0; k + 1 < count; k += 2 )
		RP_SpawnRouteNoteKey( &route, pairs[k], pairs[k + 1] );

	if ( e->rpBSPInstance > 0 )
		RP_FreeSubBSPEntities( e->rpBSPInstance );
	RP_FreeEntityTriggers( e );
	G_FreeEntity( e );

	moved = RP_SpawnForRoute( &route );
	if ( !moved )
		return NULL;

	for ( k = 0; k < count; k++ )
		level.zyk_spawn_strings[moved->s.number][k] = pairs[k];
	level.zyk_spawn_strings_values_count[moved->s.number] = count;

	if ( !RP_GrabSetKey( moved, "origin", va( "%i %i %i", (int)origin[0], (int)origin[1], (int)floor( origin[2] - shift + 0.5f ) ) ) )
	{
		G_FreeEntity( moved );
		return NULL;
	}

	zyk_main_spawn_entity( moved );

	return moved->inuse ? moved : NULL;
}

/*
=============================================================================

THE HOLD

=============================================================================
*/

// zyk: may this client hold anything at all: connected, logged in, with the Entity System power
static qboolean RP_GrabHolderActive( const gentity_t *ent )
{
	const gclient_t *client = ent->client;

	if ( !client || ent->s.number >= MAX_CLIENTS || client->pers.connected != CON_CONNECTED )
		return qfalse;

	if ( client->sess.loggedin != qtrue )
		return qfalse;

	return ( client->pers.bitvalue & ( 1 << ADM_ENTITYSYSTEM ) ) ? qtrue : qfalse;
}

// zyk: the view is someone else's while following them
static qboolean RP_GrabFollowing( const gentity_t *ent )
{
	return ( ( ent->client->ps.pm_flags & PMF_FOLLOW ) || ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) ? qtrue : qfalse;
}

// zyk: the entity this client holds, if it still is that entity
static gentity_t *RP_GrabHeld( gentity_t *ent )
{
	int num = ent->client->pers.entHoldNum;
	gentity_t *held;

	if ( num < MAX_CLIENTS + BODY_QUEUE_SIZE || num >= MAX_ENTITIESTOTAL )
		return NULL;

	held = &g_entities[num];
	if ( !held->inuse || held->rpHeldBy != ent->s.number + 1 )
		return NULL;

	return held;
}

// zyk: the preview model, if it still is that one
static gentity_t *RP_GrabGhost( gentity_t *ent )
{
	int num = ent->client->pers.entHoldGhost;
	gentity_t *ghost;

	if ( num < MAX_CLIENTS + BODY_QUEUE_SIZE || num >= MAX_GENTITIES )
		return NULL;

	ghost = &g_entities[num];
	if ( !ghost->inuse || ghost->classname != rp_hold_ghost_classname || ghost->rpHeldBy != ent->s.number + 1 )
		return NULL;

	return ghost;
}

// zyk: forget the hold: the preview model goes, the entity is no longer marked held. Nothing is spawned.
static void RP_GrabClear( gentity_t *ent )
{
	clientPersistant_t *pers = &ent->client->pers;
	gentity_t *ghost = RP_GrabGhost( ent );
	gentity_t *held = RP_GrabHeld( ent );

	if ( ghost )
		G_FreeEntity( ghost );

	if ( held )
		held->rpHeldBy = 0;

	pers->entHoldNum = 0;
	pers->entHoldMode = 0;
	pers->entHoldRotated = qfalse;
	VectorClear( pers->entHoldAngles );
	VectorClear( pers->entHoldOrigin );
	pers->entHoldNextBox = 0;
	pers->entHoldGhost = 0;
}

/*
==================
RP_GrabMakeGhost

The preview model: an entity that looks like the held one -- a copy of its entityState -- sent to the
holder only, with no contents (nothing collides with it, no trace finds it) and no think, use or touch.
It has no key/value record, so /entsave never writes it and the entity commands refuse it. Only for a
networked general entity or mover with a model that is not a brush model: a brush model the client
would collide against in its own prediction (its s.solid is SOLID_BMODEL whatever its contents), and an
item it would predict picking up. Those, and everything else, are shown by the box alone.
==================
*/
static void RP_GrabMakeGhost( gentity_t *ent, gentity_t *held )
{
	gentity_t *ghost;
	int num;

	if ( held->isLogical || held->r.bmodel || !held->s.modelindex )
		return;
	if ( held->s.eType != ET_GENERAL && held->s.eType != ET_MOVER )
		return;
	if ( !G_EntitySlotsAvailable( 16 ) )
		return;

	ghost = G_Spawn();
	if ( !ghost )
		return;

	num = ghost->s.number;
	ghost->s = held->s;
	ghost->s.number = num;
	ghost->s.event = 0;
	ghost->s.eventParm = 0;
	ghost->s.loopSound = 0;
	ghost->s.loopIsSoundset = qfalse;
	ghost->s.eFlags &= ~EF_NODRAW;
	ghost->s.pos.trType = TR_STATIONARY;
	ghost->s.apos.trType = TR_STATIONARY;

	ghost->classname = rp_hold_ghost_classname;
	ghost->rpHeldBy = ent->s.number + 1;
	VectorCopy( held->r.mins, ghost->r.mins );
	VectorCopy( held->r.maxs, ghost->r.maxs );
	ghost->r.contents = 0;
	ghost->r.bmodel = qfalse;
	ghost->r.svFlags = SVF_SINGLECLIENT;
	ghost->r.singleClient = ent->s.number;

	ent->client->pers.entHoldGhost = num;
}

/*
==================
RP_GrabHide

A cut entity while it is held: out of the world, and inert -- nothing runs on it and nothing can use,
touch or damage it. The trigger a door or platform made for itself goes now too, or walking into it would
work the hidden entity (and relink it). Its record is not touched, so /entsave still writes it where it
was, and spawning it again from the record -- the drop, or /entcancel -- brings back all of it.
==================
*/
static void RP_GrabHide( gentity_t *e )
{
	RP_FreeEntityTriggers( e );

	if ( !e->isLogical )
		trap->UnlinkEntity( (sharedEntity_t *)e );

	e->think = NULL;
	e->nextthink = 0;
	e->reached = NULL;
	e->blocked = NULL;
	e->touch = NULL;
	e->use = NULL;
	e->pain = NULL;
	e->die = NULL;
	e->takedamage = qfalse;
	e->s.pos.trType = TR_STATIONARY;
	e->s.apos.trType = TR_STATIONARY;
}

// zyk: the preview lines: the box where it would land (a cross for a point entity) and a line showing
// which way it will face
static void RP_GrabDrawPreview( gentity_t *ent, const vec3_t origin, const vec3_t mins, const vec3_t maxs, const vec3_t angles, int color )
{
	qboolean sized = ( maxs[0] - mins[0] >= 1.0f || maxs[1] - mins[1] >= 1.0f || maxs[2] - mins[2] >= 1.0f ) ? qtrue : qfalse;
	vec3_t centre, fwd, facingAngles, end;
	int k;

	if ( !G_EntitySlotsAvailable( sized ? 13 : 4 ) )
		return;

	if ( sized )
	{
		RP_EntBoundsDrawBoxAt( ent->s.number, origin, mins, maxs, NULL, color, RP_GRAB_PREVIEW_LIFE );
	}
	else
	{
		for ( k = 0; k < 3; k++ )
		{
			vec3_t a, b;

			VectorCopy( origin, a );
			VectorCopy( origin, b );
			a[k] -= RP_GRAB_CROSS;
			b[k] += RP_GRAB_CROSS;
			RP_EntBoundsLine( a, b, color, RP_GRAB_PREVIEW_LIFE, ent->s.number );
		}
	}

	for ( k = 0; k < 3; k++ )
		centre[k] = origin[k] + ( mins[k] + maxs[k] ) * 0.5f;

	VectorSet( facingAngles, 0, angles[YAW], 0 );
	AngleVectors( facingAngles, fwd, NULL, NULL );
	VectorMA( centre, RP_GRAB_FACING, fwd, end );
	RP_EntBoundsLine( centre, end, color, RP_GRAB_PREVIEW_LIFE, ent->s.number );
}

// zyk: where the held entity would land now -- the ghost is moved there, and the lines redrawn when due
static void RP_GrabUpdate( gentity_t *ent, gentity_t *held )
{
	clientPersistant_t *pers = &ent->client->pers;
	vec3_t point, normal, mins, maxs, origin;
	gentity_t *ghost;

	RP_EntGrabAimPoint( ent, point, normal );
	RP_EntGrabPlaceBox( held->classname, held, mins, maxs );
	RP_EntGrabPlace( point, normal, mins, maxs, origin );
	VectorCopy( origin, pers->entHoldOrigin );

	ghost = RP_GrabGhost( ent );
	if ( ghost )
	{
		G_SetOrigin( ghost, origin );
		VectorCopy( pers->entHoldAngles, ghost->s.angles );
		VectorCopy( pers->entHoldAngles, ghost->s.apos.trBase );
		VectorCopy( pers->entHoldAngles, ghost->r.currentAngles );
		trap->LinkEntity( (sharedEntity_t *)ghost );
	}

	// zyk: a stamp far in the future can only have come from an older, longer-running level.time
	if ( pers->entHoldNextBox > level.time + 10000 )
		pers->entHoldNextBox = 0;

	if ( level.time >= pers->entHoldNextBox )
	{
		RP_GrabDrawPreview( ent, origin, mins, maxs, pers->entHoldAngles,
			pers->entHoldMode == RP_HOLD_CUT ? RP_GRAB_COLOR_CUT : RP_GRAB_COLOR_COPY );
		pers->entHoldNextBox = level.time + RP_GRAB_PREVIEW_MSEC;
	}
}

/*
==================
RP_EntGrabCancel

Let go of whatever this client holds: a copy is simply not made, a cut entity is spawned again from its
record -- where it was, as it was. With tell, the client is told. Called by /entcancel, and when the
holder logs out, disconnects (quietly) or loses the Entity System power.
==================
*/
void RP_EntGrabCancel( gentity_t *ent, qboolean tell )
{
	gentity_t *held;
	int mode, num;

	if ( !ent || !ent->client || !ent->client->pers.entHoldNum )
		return;

	held = RP_GrabHeld( ent );
	mode = ent->client->pers.entHoldMode;
	num = ent->client->pers.entHoldNum;

	RP_GrabClear( ent );

	if ( !held )
	{
		if ( tell )
			trap->SendServerCommand( ent->s.number, va( "print \"The entity you were holding (%d) is gone.\n\"", num ) );
		return;
	}

	if ( mode == RP_HOLD_CUT )
	{
		RP_EntGrabRespawnInPlace( held );

		if ( !held->inuse )
			G_LogPrintf( "Entity %d, put back when %s let go of it, did not survive being spawned again\n", num, ent->client->pers.netname );

		if ( tell )
		{
			if ( held->inuse )
				trap->SendServerCommand( ent->s.number, va( "print \"Entity %d is back where it was.\n\"", num ) );
			else
				trap->SendServerCommand( ent->s.number, va( "print \"Entity %d did not survive being spawned again, so it is gone.\n\"", num ) );
		}
	}
	else if ( tell )
	{
		trap->SendServerCommand( ent->s.number, va( "print \"Let go of the copy of entity %d.\n\"", num ) );
	}
}

/*
==================
RP_EntGrabFrame

Once per client per server frame, from ClientEndFrame(): lets go if the holder may no longer hold or the
entity is gone, keeps a cut entity out of the world, and moves the preview with the aim.
==================
*/
void RP_EntGrabFrame( gentity_t *ent )
{
	gclient_t *client = ent->client;
	gentity_t *held;

	if ( !client || !client->pers.entHoldNum )
		return;

	if ( !RP_GrabHolderActive( ent ) )
	{
		RP_EntGrabCancel( ent, qtrue );
		return;
	}

	held = RP_GrabHeld( ent );
	if ( !held )
	{
		int num = client->pers.entHoldNum;

		RP_GrabClear( ent );
		trap->SendServerCommand( ent->s.number, va( "print \"The entity you were holding (%d) is gone.\n\"", num ) );
		return;
	}

	// zyk: a script, or a team of movers, can link an entity again; a held cut one stays out
	if ( client->pers.entHoldMode == RP_HOLD_CUT && !held->isLogical && held->r.linked )
		trap->UnlinkEntity( (sharedEntity_t *)held );

	if ( RP_GrabFollowing( ent ) || level.intermissiontime )
		return;

	RP_GrabUpdate( ent, held );
}

/*
=============================================================================

COMMANDS

=============================================================================
*/

/*
==================
RP_GrabRefusal

Why this admin may not pick up (moving: cut, or rotate in place) this entity, worded to follow
"Entity <id> ", or NULL if it may.
==================
*/
static const char *RP_GrabRefusal( const gentity_t *ent, const gentity_t *target, qboolean moving )
{
	const char *why = RP_EntityRefusalReason( target );
	gentity_t *bsp;

	if ( why )
		return why;

	if ( target->rpHeldBy )
	{
		int holder = target->rpHeldBy - 1;

		if ( holder == ent->s.number )
			return "is the one you are holding";

		return va( "is being held by %s^7", ( holder >= 0 && holder < MAX_CLIENTS && g_entities[holder].client ) ?
			g_entities[holder].client->pers.netname : "another admin" );
	}

	if ( target->rpSubBSPOf > 0 && ( bsp = RP_MiscBspForInstance( target->rpSubBSPOf ) ) != NULL )
		return va( "belongs to misc_bsp %d: copy or edit the misc_bsp instead", bsp->s.number );

	if ( target->neverFree )
		return "is a permanent game entity";

	// GalaxyRP: [Entity System] all but the map's pickups, dispensers and decor nothing links to (E)
	if ( moving && RP_MapEntityProtected( target ) )
		return va( "is part of the map (marked M in /entlist) and cannot be cut or rotated%s. Use ^3/entcopy^7 to place a copy of it instead",
			RP_MapEntityRefusalNote( target ) );

	if ( moving && target->r.bmodel )
		return "is a brush entity and cannot be cut or rotated (a brush entity's angles are the direction it moves in). Use ^3/entcopy^7 to place another one instead";

	return NULL;
}

/*
==================
RP_GrabAim

The entity aimed at, as the Entity Bounds display finds it -- except that the trigger a door or platform
made for itself stands for its door or platform: it reaches well out in front of the door, so from most
places aiming at the door lands on it first, and it is never something that can be picked up.
==================
*/
static gentity_t *RP_GrabAim( gentity_t *ent )
{
	gentity_t *aimed = RP_EntBoundsAim( ent );

	if ( aimed && ( aimed->r.contents & CONTENTS_TRIGGER ) && !RP_EntityHasSpawnKeys( aimed ) &&
		aimed->parent && aimed->parent != aimed && aimed->parent->inuse && aimed->parent - g_entities >= MAX_CLIENTS + BODY_QUEUE_SIZE )
	{
		return aimed->parent;
	}

	return aimed;
}

/*
==================
RP_EntAimTarget / RP_EntAimFollowing

GalaxyRP: [Entity System] the same pick for /entedit and /entremove with no id (g_cmds.c): the entity
aimed at as RP_GrabAim() finds it, and whether the admin is following another player, when the view --
and so the aim -- is that player's.
==================
*/
gentity_t *RP_EntAimTarget( gentity_t *ent )
{
	return RP_GrabAim( ent );
}

qboolean RP_EntAimFollowing( const gentity_t *ent )
{
	return RP_GrabFollowing( ent );
}

// zyk: the entity an /entcopy or /entcut picks up: the id given, or the one aimed at
static gentity_t *RP_GrabPickTarget( gentity_t *ent, const char *cmd )
{
	gentity_t *aimed;

	if ( trap->Argc() >= 2 )
	{
		char arg[MAX_STRING_CHARS];
		int id;

		trap->Argv( 1, arg, sizeof( arg ) );
		id = atoi( arg );

		// GalaxyRP: [Logical Entities] valid in either region, as /entedit takes them
		if ( arg[0] < '0' || arg[0] > '9' || id < 0 || id >= MAX_GENTITIES + level.num_logicalents ||
			( id >= level.num_entities && id < MAX_GENTITIES ) )
		{
			trap->SendServerCommand( ent->s.number, "print \"Invalid Entity ID.\n\"" );
			return NULL;
		}

		return &g_entities[id];
	}

	aimed = RP_GrabAim( ent );
	if ( !aimed )
	{
		trap->SendServerCommand( ent->s.number, va( "print \"You are not aiming at an entity. Aim at one, or give its id: ^3/%s <entity id>^7.\n\"", cmd ) );
		return NULL;
	}

	return aimed;
}

/*
==================
RP_GrabDrop

The second /entcopy or /entcut: the held entity lands where the admin is aiming now. A copy is a new
entity made from the original's record with the new origin (and angles); a cut entity gets them in its
own record and is spawned again in its own slot. If that spawn removes it, it is rebuilt where it was
from its old record, so a drop never loses an entity.
==================
*/
static void RP_GrabDrop( gentity_t *ent, gentity_t *held )
{
	clientPersistant_t *pers = &ent->client->pers;
	vec3_t point, normal, mins, maxs, origin, angles;
	vec3_t playerMins, playerMaxs;
	char originText[64];
	const char *anglesKey;
	qboolean rotated = pers->entHoldRotated;
	int num = held->s.number;
	int mode = pers->entHoldMode;

	RP_EntGrabAimPoint( ent, point, normal );
	RP_EntGrabPlaceBox( held->classname, held, mins, maxs );
	RP_EntGrabPlace( point, normal, mins, maxs, origin );
	VectorCopy( pers->entHoldAngles, angles );
	// zyk: the player's box a spawn point or NPC spawner is placed by, or none
	RP_EntGrabPlaceBox( held->classname, NULL, playerMins, playerMaxs );

	if ( ( held->r.contents & ( CONTENTS_SOLID | CONTENTS_BODY ) ) && RP_GrabOccupied( origin, mins, maxs ) )
	{
		trap->SendServerCommand( ent->s.number, "print \"Someone is standing where it would land. Aim somewhere else.\n\"" );
		return;
	}

	Com_sprintf( originText, sizeof( originText ), "%i %i %i", (int)origin[0], (int)origin[1], (int)origin[2] );
	anglesKey = RP_GrabAnglesKey( held );

	if ( mode == RP_HOLD_COPY )
	{
		rpSpawnRoute_t route;
		gentity_t *copy;
		int count = level.zyk_spawn_strings_values_count[num];
		int i;

		if ( count > ZYK_MAX_SPAWN_STRING_SLOTS )
			count = ZYK_MAX_SPAWN_STRING_SLOTS;

		RP_SpawnRouteInit( &route );
		for ( i = 0; i + 1 < count; i += 2 )
			RP_SpawnRouteNoteKey( &route, level.zyk_spawn_strings[num][i], level.zyk_spawn_strings[num][i + 1] );

		if ( !RP_GrabRouteHasRoom( &route ) )
		{
			trap->SendServerCommand( ent->s.number, va( "print \"Cannot drop the copy: the server is near its entity limit (%d networked slots free, %d held in reserve; %d logical free).\n\"",
				G_FreeEntityCount(), ZYK_ENTITY_RESERVE, G_FreeLogicalEntityCount() ) );
			return;
		}

		copy = RP_SpawnForRoute( &route );
		if ( !copy )
		{
			trap->SendServerCommand( ent->s.number, "print \"Cannot drop the copy: there is no free entity slot for it.\n\"" );
			return;
		}

		// zyk: the original's record, pointer for pointer -- the strings live as long as the map and
		// are never written to, and copying them this way keeps a value from an entity file exactly as
		// it was read (G_NewString() would translate a backslash in it again)
		for ( i = 0; i < count; i++ )
			level.zyk_spawn_strings[copy->s.number][i] = level.zyk_spawn_strings[num][i];
		level.zyk_spawn_strings_values_count[copy->s.number] = count;

		if ( !RP_GrabSetKey( copy, "origin", RP_EntGrabOriginKey( held, origin ) ) || ( rotated && !RP_GrabWriteAngles( copy, anglesKey, angles ) ) )
		{
			G_FreeEntity( copy );
			trap->SendServerCommand( ent->s.number, "print \"Cannot drop the copy: its record has no room for its origin and angles.\n\"" );
			return;
		}

		RP_GrabClear( ent );
		{
			int freeBefore = G_FreeEntityCount();

			zyk_main_spawn_entity( copy );

			// zyk: a copy can come out with a different box than the original it was placed by -- a
			// copy of one of the map's models gets its model's own box (SP_misc_model_breakable)
			if ( copy->inuse && VectorCompare( playerMins, vec3_origin ) && VectorCompare( playerMaxs, vec3_origin ) )
			{
				gentity_t *settled = RP_EntGrabSettle( copy, point, normal, freeBefore );

				if ( !settled )
				{
					trap->SendServerCommand( ent->s.number, va( "print \"The copy of entity %d could not be placed (no free entity slot, or it did not survive being spawned again).\n\"", num ) );
					return;
				}
				copy = settled;
			}
		}

		if ( !copy->inuse )
		{
			if ( level.rp_spawn_refusal[0] )
				trap->SendServerCommand( ent->s.number, va( "print \"The copy of entity %d was refused: %s.\n\"", num, level.rp_spawn_refusal ) );
			else
				trap->SendServerCommand( ent->s.number, va( "print \"The copy of entity %d did not survive being spawned there (a class that removes itself, or a spawn its spawn function refused).\n\"", num ) );
			return;
		}

		level.last_spawned_entity = copy;
		G_LogPrintf( "%s dropped a copy of entity %d as entity %d at %s\n", pers->netname, num, copy->s.number, originText );
		trap->SendServerCommand( ent->s.number, va( "print \"Copy of entity %d dropped: entity %d at (%s)%s.\n\"", num, copy->s.number, originText,
			copy->isLogical ? ", logical" : "" ) );
		return;
	}
	else
	{
		static char *backup[ZYK_MAX_SPAWN_STRING_SLOTS];
		int count = level.zyk_spawn_strings_values_count[num];
		gentity_t *rebuilt;

		if ( count > ZYK_MAX_SPAWN_STRING_SLOTS )
			count = ZYK_MAX_SPAWN_STRING_SLOTS;
		memcpy( backup, level.zyk_spawn_strings[num], sizeof( backup[0] ) * count );

		if ( !RP_GrabSetKey( held, "origin", RP_EntGrabOriginKey( held, origin ) ) || ( rotated && !RP_GrabWriteAngles( held, anglesKey, angles ) ) )
		{
			// zyk: put back whatever part of it went in, and keep holding
			memcpy( level.zyk_spawn_strings[num], backup, sizeof( backup[0] ) * count );
			level.zyk_spawn_strings_values_count[num] = count;
			trap->SendServerCommand( ent->s.number, "print \"Cannot drop it: its record has no room for its origin and angles.\n\"" );
			return;
		}

		// GalaxyRP: [Entity System] moved, a map entity (one marked E) is not the map's any more -- see
		// RP_MapEntityExempt()
		held->rpMapEntity = qfalse;

		RP_GrabClear( ent );
		RP_EntGrabRespawnInPlace( held );

		// zyk: turned, a model's own box is not the one it was placed by -- set that against the
		// surface and spawn it once more (see RP_EntGrabSettle; a cut entity keeps its slot)
		if ( held->inuse && !held->isLogical && VectorCompare( playerMins, vec3_origin ) && VectorCompare( playerMaxs, vec3_origin ) )
		{
			vec3_t newMins, newMaxs, settled;

			RP_EntGrabPlaceBox( NULL, held, newMins, newMaxs );
			if ( !VectorCompare( newMins, vec3_origin ) || !VectorCompare( newMaxs, vec3_origin ) )
			{
				RP_EntGrabPlace( point, normal, newMins, newMaxs, settled );
				if ( !VectorCompare( settled, held->s.origin ) &&
					!( ( held->r.contents & ( CONTENTS_SOLID | CONTENTS_BODY ) ) && RP_GrabOccupied( settled, newMins, newMaxs ) ) &&
					RP_GrabSetKey( held, "origin", RP_EntGrabOriginKey( held, settled ) ) )
				{
					RP_EntGrabRespawnInPlace( held );
					Com_sprintf( originText, sizeof( originText ), "%i %i %i", (int)settled[0], (int)settled[1], (int)settled[2] );
				}
			}
		}

		if ( held->inuse )
		{
			G_LogPrintf( "%s moved entity %d to %s\n", pers->netname, num, originText );
			trap->SendServerCommand( ent->s.number, va( "print \"Entity %d moved to (%s).\n\"", num, originText ) );
			return;
		}

		rebuilt = RP_GrabRebuild( backup, count );
		G_LogPrintf( "%s moved entity %d to %s, where it did not survive being spawned; %s\n", pers->netname, num, originText,
			rebuilt ? va( "rebuilt where it was as entity %d", rebuilt->s.number ) : "it could not be rebuilt" );
		if ( rebuilt )
			trap->SendServerCommand( ent->s.number, va( "print \"Entity %d did not survive being spawned at (%s), so it was put back where it was, as entity %d.\n\"",
				num, originText, rebuilt->s.number ) );
		else
			trap->SendServerCommand( ent->s.number, va( "print \"Entity %d did not survive being spawned at (%s), and could not be put back, so it is gone.\n\"",
				num, originText ) );
	}
}

// zyk: /entcopy and /entcut
static void RP_GrabCommand( gentity_t *ent, int mode )
{
	clientPersistant_t *pers = &ent->client->pers;
	const char *cmd = ( mode == RP_HOLD_COPY ) ? "entcopy" : "entcut";
	gentity_t *target;
	const char *why;
	int i;

	if ( !check_admin_command( ent, ADM_ENTITYSYSTEM, qtrue ) )
		return;

	if ( RP_GrabFollowing( ent ) )
	{
		trap->SendServerCommand( ent->s.number, "print \"You are following another player. Stop following first.\n\"" );
		return;
	}

	if ( pers->entHoldNum )
	{
		gentity_t *held = RP_GrabHeld( ent );

		if ( !held )
		{
			int num = pers->entHoldNum;

			RP_GrabClear( ent );
			trap->SendServerCommand( ent->s.number, va( "print \"The entity you were holding (%d) is gone.\n\"", num ) );
			return;
		}

		// zyk: only the command that picked it up puts it down
		if ( pers->entHoldMode != mode )
		{
			trap->SendServerCommand( ent->s.number, va( "print \"You are holding %s entity %d: use ^3/%s^7 to drop it, or ^3/entcancel^7.\n\"",
				pers->entHoldMode == RP_HOLD_COPY ? "a copy of" : "the cut", pers->entHoldNum,
				pers->entHoldMode == RP_HOLD_COPY ? "entcopy" : "entcut" ) );
			return;
		}

		if ( trap->Argc() >= 2 )
		{
			trap->SendServerCommand( ent->s.number, va( "print \"You are already holding entity %d: drop it with ^3/%s^7 first, or ^3/entcancel^7.\n\"",
				pers->entHoldNum, cmd ) );
			return;
		}

		RP_GrabDrop( ent, held );
		return;
	}

	target = RP_GrabPickTarget( ent, cmd );
	if ( !target )
		return;

	why = RP_GrabRefusal( ent, target, mode == RP_HOLD_CUT );
	if ( why )
	{
		trap->SendServerCommand( ent->s.number, va( "print \"Entity %d %s.\n\"", (int)( target - g_entities ), why ) );
		return;
	}

	if ( !RP_GrabRecordHasRoom( target ) )
	{
		trap->SendServerCommand( ent->s.number, va( "print \"Entity %d has no room left in its record for an origin and angles (%d key/value pairs at most).\n\"",
			target->s.number, ZYK_MAX_SPAWN_STRING_SLOTS / 2 ) );
		return;
	}

	pers->entHoldNum = target->s.number;
	pers->entHoldMode = mode;
	pers->entHoldRotated = qfalse;
	RP_GrabRecordAngles( target, RP_GrabAnglesKey( target ), pers->entHoldAngles );
	for ( i = 0; i < 3; i++ )
		pers->entHoldAngles[i] = RP_GrabNormalizeAngle( pers->entHoldAngles[i] );
	pers->entHoldNextBox = 0;
	pers->entHoldGhost = 0;
	target->rpHeldBy = ent->s.number + 1;

	RP_GrabMakeGhost( ent, target );

	if ( mode == RP_HOLD_CUT )
		RP_GrabHide( target );

	RP_GrabUpdate( ent, target );

	G_LogPrintf( "%s picked up %sentity %d (%s)\n", pers->netname, mode == RP_HOLD_COPY ? "a copy of " : "",
		target->s.number, target->classname ? target->classname : "noclass" );
	trap->SendServerCommand( ent->s.number, va( "print \"%s entity %d (%s). Aim where it should go and use ^3/%s^7 again to drop it, ^3/entrotate^7 to turn it, ^3/entcancel^7 to let go%s.\n\"",
		mode == RP_HOLD_COPY ? "Holding a copy of" : "Holding", target->s.number, target->classname ? target->classname : "noclass", cmd,
		mode == RP_HOLD_CUT ? " (it goes back where it was)" : "" ) );
}

void Cmd_EntCopy_f( gentity_t *ent )
{
	RP_GrabCommand( ent, RP_HOLD_COPY );
}

void Cmd_EntCut_f( gentity_t *ent )
{
	RP_GrabCommand( ent, RP_HOLD_CUT );
}

/*
==================
Cmd_EntRotate_f

/entrotate                      45 degrees more yaw
/entrotate <yaw>                that much more yaw
/entrotate <pitch> <yaw> <roll> exactly those angles

What is held turns in the preview and is dropped with the angles; with nothing held, the entity aimed
at is turned in place -- its record changed and it is spawned again, like /entedit.
==================
*/
void Cmd_EntRotate_f( gentity_t *ent )
{
	clientPersistant_t *pers = &ent->client->pers;
	int argc = trap->Argc();
	float values[3] = { 0, 45.0f, 0 };
	vec3_t angles;
	gentity_t *target;
	const char *why;
	const char *key;
	int i, num;

	if ( !check_admin_command( ent, ADM_ENTITYSYSTEM, qtrue ) )
		return;

	if ( argc != 1 && argc != 2 && argc != 4 )
	{
		trap->SendServerCommand( ent->s.number, "print \"Usage: ^3/entrotate^7 (45 degrees of yaw), ^3/entrotate <yaw>^7 (that much more yaw) or ^3/entrotate <pitch> <yaw> <roll>^7 (exactly those angles).\n\"" );
		return;
	}

	for ( i = 1; i < argc; i++ )
	{
		char arg[MAX_STRING_CHARS];
		float v;

		trap->Argv( i, arg, sizeof( arg ) );
		v = atof( arg );

		if ( !( v > -RP_GRAB_ANGLE_LIMIT && v < RP_GRAB_ANGLE_LIMIT ) )
		{
			trap->SendServerCommand( ent->s.number, va( "print \"%s is not an angle.\n\"", arg ) );
			return;
		}

		if ( argc == 2 )
			values[1] = v;
		else
			values[i - 1] = v;
	}

	if ( RP_GrabFollowing( ent ) )
	{
		trap->SendServerCommand( ent->s.number, "print \"You are following another player. Stop following first.\n\"" );
		return;
	}

	// zyk: what is held turns in the preview
	if ( pers->entHoldNum )
	{
		gentity_t *held = RP_GrabHeld( ent );

		if ( !held )
		{
			num = pers->entHoldNum;
			RP_GrabClear( ent );
			trap->SendServerCommand( ent->s.number, va( "print \"The entity you were holding (%d) is gone.\n\"", num ) );
			return;
		}

		if ( held->r.bmodel )
		{
			trap->SendServerCommand( ent->s.number, va( "print \"Entity %d is a brush entity: its angles are the direction it moves in, so it cannot be rotated.\n\"", held->s.number ) );
			return;
		}

		if ( argc == 4 )
			VectorCopy( values, pers->entHoldAngles );
		else
			pers->entHoldAngles[YAW] += values[1];

		for ( i = 0; i < 3; i++ )
			pers->entHoldAngles[i] = RP_GrabNormalizeAngle( pers->entHoldAngles[i] );

		pers->entHoldRotated = qtrue;
		pers->entHoldNextBox = 0;	// redraw the facing line now

		trap->SendServerCommand( ent->s.number, va( "print \"Entity %d will be dropped at angles (%g %g %g).\n\"", held->s.number,
			pers->entHoldAngles[0], pers->entHoldAngles[1], pers->entHoldAngles[2] ) );
		return;
	}

	// zyk: nothing held: the entity aimed at turns in place
	target = RP_GrabAim( ent );
	if ( !target )
	{
		trap->SendServerCommand( ent->s.number, "print \"You are not aiming at an entity. Aim at one, or pick one up with ^3/entcopy^7 or ^3/entcut^7 and turn that.\n\"" );
		return;
	}

	num = target->s.number;
	why = RP_GrabRefusal( ent, target, qtrue );
	if ( why )
	{
		trap->SendServerCommand( ent->s.number, va( "print \"Entity %d %s.\n\"", num, why ) );
		return;
	}

	key = RP_GrabAnglesKey( target );
	RP_GrabRecordAngles( target, key, angles );

	if ( argc == 4 )
		VectorCopy( values, angles );
	else
		angles[YAW] += values[1];

	for ( i = 0; i < 3; i++ )
		angles[i] = RP_GrabNormalizeAngle( angles[i] );

	{
		static char *backup[ZYK_MAX_SPAWN_STRING_SLOTS];
		int count = level.zyk_spawn_strings_values_count[num];
		gentity_t *rebuilt;

		if ( count > ZYK_MAX_SPAWN_STRING_SLOTS )
			count = ZYK_MAX_SPAWN_STRING_SLOTS;
		memcpy( backup, level.zyk_spawn_strings[num], sizeof( backup[0] ) * count );

		if ( !RP_GrabWriteAngles( target, key, angles ) )
		{
			memcpy( level.zyk_spawn_strings[num], backup, sizeof( backup[0] ) * count );
			level.zyk_spawn_strings_values_count[num] = count;
			trap->SendServerCommand( ent->s.number, va( "print \"Entity %d has no room left in its record for its angles.\n\"", num ) );
			return;
		}

		// GalaxyRP: [Entity System] turned, a map entity (one marked E) is not the map's any more -- see
		// RP_MapEntityExempt()
		target->rpMapEntity = qfalse;

		RP_EntGrabRespawnInPlace( target );

		if ( target->inuse )
		{
			trap->SendServerCommand( ent->s.number, va( "print \"Entity %d rotated to (%g %g %g).\n\"", num, angles[0], angles[1], angles[2] ) );
			return;
		}

		rebuilt = RP_GrabRebuild( backup, count );
		if ( rebuilt )
			trap->SendServerCommand( ent->s.number, va( "print \"Entity %d did not survive being spawned again turned, so it was put back as it was, as entity %d.\n\"",
				num, rebuilt->s.number ) );
		else
			trap->SendServerCommand( ent->s.number, va( "print \"Entity %d did not survive being spawned again turned, and could not be put back, so it is gone.\n\"", num ) );
	}
}

void Cmd_EntCancel_f( gentity_t *ent )
{
	if ( !check_admin_command( ent, ADM_ENTITYSYSTEM, qtrue ) )
		return;

	if ( !ent->client->pers.entHoldNum )
	{
		trap->SendServerCommand( ent->s.number, "print \"You are not holding anything.\n\"" );
		return;
	}

	RP_EntGrabCancel( ent, qtrue );
}
