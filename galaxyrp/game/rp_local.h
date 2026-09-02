/*
=========================== GalaxyRP Mod ============================
Project based on OpenJK and Zyk Mod. Work copyrighted (C) 2020 - 2022
=====================================================================
[Description]: Local definitions for game module
=====================================================================
*/

#ifndef __RP_LOCAL_H__
#define __RP_LOCAL_H__

#include "../../galaxyrp/game/rp_version.h" // Version header

/*
=====================================================================
Global definitions
=====================================================================
*/

#define DB_PATH							"GalaxyRP/database/accounts.db"

// GalaxyRP fix: [Database] Centralised sqlite3_open() wrapper -- see definition in g_main.c for
// why this exists (map-change "database is locked" errors). Every place in the game module that
// used to call sqlite3_open(DB_PATH, &db) directly should call RP_DB_Open(&db) instead.
// Forward-declared (rather than including sqlite3.h here) since this header is pulled in by
// g_local.h for every game module file, and not all of them already include sqlite3.h themselves
// before that happens; sqlite3 is just an opaque "typedef struct sqlite3 sqlite3;" in sqlite3.h,
// so redeclaring it here is safe.
typedef struct sqlite3 sqlite3;
#define RP_DB_BUSY_TIMEOUT_MS			5000
int RP_DB_Open(sqlite3 **db);

#define NUM_OF_GUARDIANS				10 // zyk: number of Light Quest guardians to be defeated 
#define NUM_OF_OBJECTIVES				10 // zyk: number of Dark Quest objectives
// GalaxyRP fix: [Quests] removed NUM_OF_ETERNITY_QUEST_OBJ here — unused anywhere in the codebase
#define NUM_OF_UNIVERSE_QUEST_OBJ		22 // zyk: number of Universe Quest objectives
#define NUM_OF_SKILLS					60 // zyk: number of RPG Mode skills

#define MAX_SHADER_REMAPS				128
#define MAX_RACERS						16 // zyk: Max racers in the map
#define MAX_DUEL_MATCHES				496 // zyk: max matches a tournament may have
#define MAX_CUSTOM_QUESTS				64 // zyk: max amount of custom quests
#define MAX_CUSTOM_QUEST_MISSIONS		512 // zyk: max missions a custom quest can have
#define MAX_MISSION_FIELD_LINES			8 // zyk: max lines of custom quest mission fields to send to client
#define MAX_CUSTOM_QUEST_FIELDS			512 // zyk: max fields a custom quest mission can have
#define MAX_BOUNTY_HUNTER_SENTRIES		5 // zyk: max sentries a Bounty Hunter can have if he has the Upgrade
#define MAX_RPG_CHARS					60 // zyk: max RPG chars an account can have
#define MAX_ACC_NAME_SIZE				30 // zyk: max characters an account or rpg char can have
#define MAX_JETPACK_FUEL				10000 // zyk: max jetpack fuel the player can have

#define JETPACK_SCALE					100 // zyk: used to scale the MAX_JETPACK_FUEL to set the jetpackFuel attribute. Dividing MAX_JETPACK_FUEL per JETPACK_SCALE must result in 100
// GalaxyRP fix: [Shop] NUMBER_OF_SELLER_ITEMS (56, one flat id space shared by every product the
// jawa seller ever sold, most of it long dead) replaced by two smaller counts for the surviving
// /buy item <n> and /buy upgrade <n> subcommands, each freshly numbered from 1.
#define NUMBER_OF_SHOP_ITEMS			12 // zyk: quantity of items sold via /buy item <n> and /stuff item <n>
#define NUMBER_OF_SHOP_UPGRADES			3 // zyk: quantity of upgrades sold via /buy upgrade <n> and /stuff upgrade <n>
#define DUEL_TOURNAMENT_ARENA_SIZE		64 // zyk: default size of the globe model used as the Duel Tournament arena
#define DUEL_TOURNAMENT_PROTECT_TIME	2000 // zyk: duration of the duelists protection in Duel Tournament

/*
=====================================================================
Player / world information
=====================================================================
*/

// zyk: admin bit values
typedef enum {

	ADM_NPC,
	ADM_NOCLIP,
	ADM_GIVEADM,
	ADM_TELE,
	ADM_ADMPROTECT,
	ADM_ENTITYSYSTEM,
	ADM_SILENCE,
	ADM_CLIENTPRINT,
	ADM_SHAKESCREEN,
	ADM_KICK,
	ADM_PARALYZE,
	ADM_GIVE,
	ADM_SCALE,
	ADM_PLAYERS,
	ADM_DUELARENA,
	// GalaxyRP fix: [Admin] this slot used to be ADM_CUSTOMQUEST, a leftover bit for a command
	// that was removed from the mod. Repurposed for the /admmap map-change command (bit value
	// stays 32768 -- 1 << 15 -- so existing accounts/configs that already granted this bit keep
	// working, and the default admin account's full-permission bitmask already covers it).
	ADM_CHANGEMAP,
	ADM_CREATEITEM,
	ADM_GOD,
	ADM_LEVELUP,
	ADM_SKILL,
	ADM_CREATECREDITS,
	ADM_IGNORECHATDISTANCE,
	ADM_XP,
	ADM_UPDATENEWS,
	ADM_REMOVENEWS,
	ADM_MUSIC,
	ADM_GETUP,
	ADM_NUM_CMDS

} zyk_admin_t;

// zyk: magic powers values
typedef enum {
	MAGIC_MAGIC_SENSE,
	MAGIC_HEALING_WATER,
	MAGIC_WATER_SPLASH,
	MAGIC_WATER_ATTACK,
	MAGIC_EARTHQUAKE,
	MAGIC_ROCKFALL,
	MAGIC_SHIFTING_SAND,
	MAGIC_SLEEPING_FLOWERS,
	MAGIC_POISON_MUSHROOMS,
	MAGIC_TREE_OF_LIFE,
	MAGIC_MAGIC_SHIELD,
	MAGIC_DOME_OF_DAMAGE,
	MAGIC_MAGIC_DISABLE,
	MAGIC_ULTRA_SPEED,
	MAGIC_SLOW_MOTION,
	MAGIC_FAST_AND_SLOW,
	MAGIC_FLAME_BURST,
	MAGIC_ULTRA_FLAME,
	MAGIC_FLAMING_AREA,
	MAGIC_BLOWING_WIND,
	MAGIC_HURRICANE,
	MAGIC_REVERSE_WIND,
	MAGIC_ULTRA_RESISTANCE,
	MAGIC_ULTRA_STRENGTH,
	MAGIC_ENEMY_WEAKENING,
	MAGIC_ICE_STALAGMITE,
	MAGIC_ICE_BOULDER,
	MAGIC_ICE_BLOCK,
	MAGIC_HEALING_AREA,
	MAGIC_MAGIC_EXPLOSION,
	MAGIC_LIGHTNING_DOME,
	MAX_MAGIC_POWERS

} zyk_magic_t;

// zyk: shader remap struct
typedef struct shaderRemap_s {

	char oldShader[MAX_QPATH];
	char newShader[MAX_QPATH];
	float timeOffset;

} shaderRemap_t;

#ifdef __linux__
extern shaderRemap_t remappedShaders[MAX_SHADER_REMAPS];
#else
shaderRemap_t remappedShaders[MAX_SHADER_REMAPS];
#endif

typedef struct saber_db_info_s {
	char	saber1Model[50];
	char	saber2Model[50];
} saber_db_info_t;

typedef struct chat_modifiers_s {
	const char* chat_modifier;
	const char* chat_format;
	int			distance;
} chat_modifiers_t;

typedef struct skill_s {
	int			max_level;
	const char* skill_name;
	const char* skill_description;
	const char* category;
	const char* alignment;

	//GalaxyRP (Alex): [Skills] Value to be used for internal code stuff. (such as force power enums, weapon enums etc..)
	int value_internal;
} skill_t;

/*
=====================================================================
Re-routed functions
=====================================================================
*/

/*
=====================================================================
Cvar registration
=====================================================================
*/

/*
=====================================================================
Common / new functions
=====================================================================
*/

qboolean	zyk_is_ally(gentity_t *ent, gentity_t *other);
int			zyk_number_of_allies(gentity_t *ent, qboolean in_rpg_mode);
void		send_rpg_events(int send_event_timer);
int			zyk_get_remap_count();
void		zyk_text_message(gentity_t *ent, char *filename, qboolean show_in_chat, qboolean broadcast_message, ...);
qboolean	zyk_can_deflect_shots(gentity_t *ent);

#endif // __RP_LOCAL_H__
