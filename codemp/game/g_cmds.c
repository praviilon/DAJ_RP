/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
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
#include "bg_saga.h"

#include "ui/menudef.h"			// for the voice chats

#include "sqlite/sqlite3.h"

#define MAX_EMOTE_WORDS 11;
#define MAX_CHAT_MODIFIERS 24;

//rww - for getting bot commands...
int AcceptBotCommand(char *cmd, gentity_t *pl);
//end rww

void WP_SetSaber( int entNum, saberInfo_t *sabers, int saberNum, const char *saberName );

void Cmd_NPC_f( gentity_t *ent );
void SetTeamQuick(gentity_t *ent, int team, qboolean doBegin);

extern int check_xp(int currentLevel);
extern void Cmd_GalaxyRpUi_f(gentity_t* ent);
extern void Cmd_ZykChars_f(gentity_t* ent);
// GalaxyRP: [Char fix] forward-declared so Cmd_Char_f (below) can flush the currently active
// character before switching/creating/removing one -- same pattern already used above for
// Cmd_GalaxyRpUi_f/Cmd_ZykChars_f, whose own definitions also sit later in this file.
extern void save_account(gentity_t *ent, qboolean save_char_file);
// GalaxyRP fix: [Account] forward-declared, same pattern as save_account() just above, so
// select_player_character() and Cmd_Login_F() (both further up this file than initialize_rpg_skills()'s
// own definition) can call it directly and synchronously instead of relying solely on the G_Kill()
// respawn cycle -- matches the extern declaration g_client.c already carries for the same function.
extern void initialize_rpg_skills(gentity_t *ent);

// GalaxyRP (Alex): [Skills] This is used to display everything about a skill in various places throughout the mod.
const skill_t skills[] = {
	{5, "Jump",					"makes you use the force to jump higher. Level 5 has no height limit, you can continue jumping up until you run out of force, and it also lets you jump out of water",																																			"force",	"neutral",	FP_LEVITATION},
	{5, "Push",					"pushes the opponent forward",																																																																					"force",	"neutral",	FP_PUSH},
	{5, "Pull",					"pulls the opponent towards you",																																																																				"force",	"neutral",	FP_PULL},
	{5, "Speed",				"increases your speed. Level 1 is 1.5 times normal speed. Level 2 is 2.0, level 3 is 2.5 times and level 4 is 3.0 times",																																														"force",	"neutral",	FP_SPEED},
	{3, "Sense",				"allows you to see people through walls, invisible people or cloaked people and you can dodge disruptor shots. Represents your mind strength to resist Mind Control if your sense level is equal or higher than the enemy's mind trick level",																	"force",	"neutral",	FP_SEE},
	{5, "Saber Attack",			"gives you the saber. If you are using Single Saber, gives you the saber styles. If using duals or staff, increases saber damage, which is increased by 20 per cent for each level.",																															"force",	"neutral",	FP_SABER_OFFENSE},
	{5, "Saber Defense",		"increases your ability to block, parry enemy saber attacks or enemy shots",																																																									"force",	"neutral",	FP_SABER_DEFENSE},
	{5, "Saber Throw",			"throws your saber at enemy and gets it back. Each level increases max distance and saber throw speed.",																																																		"force",	"neutral",	FP_SABERTHROW},
	{5, "Absorb",				"allows you to absorb force power attacks done to you",																																																															"force",	"light",	FP_ABSORB},
	{5, "Heal",					"recover some Health. Level 1 restores 5 hp, level 2 restores 10 hp and level 3 restores 25 hp",																																																				"force",	"light",	FP_HEAL},
	{5, "Protect",				"decreases damage done to you by non-force power attacks. At level 4 decreases force consumption when receiving damage",																																														"force",	"light",	FP_PROTECT},
	{5, "Mind Trick",			"makes yourself invisible to the players affected by this force power. Force User class can mind control a player or npc. Level 1 has a duration of 20 seconds, level 2 is 25 seconds and level 3 is 30 seconds",																								"force",	"neutral",	FP_TELEPATHY},
	{5, "Team Heal",			"restores some health to players near you",																																																																		"force",	"light",	FP_TEAM_HEAL},
	{5, "Lightning",			"attacks with a powerful electric attack at players near you. At level 4, does more damage and pushes the enemy back",																																															"force",	"dark",		FP_LIGHTNING},
	{5, "Grip",					"attacks a player by holding and damaging him",																																																																	"force",	"dark",		FP_GRIP},
	{5, "Drain",				"drains force power from a player to restore your health",																																																														"force",	"dark",		FP_DRAIN},
	{5, "Rage",					"makes you 1.3 times faster, increases your saber attack speed and damage and makes you get less damage. Force Guardian class, with Improvements skill at least on level 1, can regen some force when taking damage on health while Rage is active",															"force",	"dark",		FP_RAGE},
	{5, "Team Energize",		"restores some force power to players near you. If Improvements skill is at least at level 1, regens blaster pack and power cell ammo of the target players",																																					"force",	"dark",		FP_TEAM_FORCE},
	{4, "Stun Baton",			"attacks someone with a small electric charge. Has %d damage multiplied by the stun baton level. With Stun Baton Upgrade, can destroy or move some other objects, and also decloaks enemies and decrease their moving speed for some seconds",																	"weapons",	"merc",		WP_STUN_BATON},
	{2, "Blaster Pistol",		"the popular Star Wars pistol used by Han Solo in the movies. Normal fire is a single blaster shot, alternate fire allows you to fire a powerful charged shot. The charged shot causes a lot more damage depending on how much it was charged",																	"weapons",	"merc",		WP_BRYAR_PISTOL},
	{2, "E11 Blaster Rifle",	"the rifle used by the Storm Troopers. Normal fire is a single shot, while the alternate fire is the rapid fire. Level 2 unlocks the alternate fire mode.",																																										"weapons",	"merc",		WP_BLASTER},
	{2, "Disruptor",			"the sniper, used by the rodians ingame. Normal fire is a shot that causes %d damage, alternate fire allows zoom and a charged shot that when fully charged. Level 2 unlocks the alternate fire mode.",																																					"weapons",	"merc",		WP_DISRUPTOR},
	{2, "Bowcaster",			"the famous weapon used by Chewbacca. Normal fire can be charged to fire up to 5 shots at once. Level 2 unlocks the alternate fire mode.",																																																				"weapons",	"merc",		WP_BOWCASTER},
	{2, "Repeater",				"a powerful weapon with a rapid fire and a plasma bomb. Normal fire shoots the rapid fire, and does %d damage. Alt fire fires the plasma bomb. Level 2 unlocks the alternate fire mode.",																																									"weapons",	"merc",		WP_REPEATER},
	{2, "DEMP2",				"a very powerful weapon against machine npc and some vehicles, causing more damage to them and stunning them. Normal fire does %d damage and alt fire can be charged. Level 2 unlocks the alternate fire mode.",																																			"weapons",	"merc",		WP_DEMP2},
	{2, "Flechette",			"this weapon is similar to a shotgun. Normal fire causes %d damage. Alt fire shoots 2 bombs. Level 2 unlocks the alternate fire mode.",																																																					"weapons",	"merc",		WP_FLECHETTE},
	{2, "Rocket Launcher",		"a powerful explosive weapon. Normal fire shoots a rocket causing %d damage. Alt fire shoots a homing missile. Level 2 unlocks the alternate fire mode.",																																																	"weapons",	"merc",		WP_ROCKET_LAUNCHER},
	{2, "Concussion Rifle",		"it shoots a powerful shot that has a big damage area. Alt fire shoots a ray similar to disruptor shots, but it can go through force fields and can throw the enemy on the ground. Level 2 unlocks the alternate fire mode.",																															"weapons",	"merc",		WP_CONCUSSION},
	{2, "Bryar Pistol",			"very similar to the blaster pistol, but this one has a better fire rate with normal shot. Level 2 unlocks the alternate fire mode.",																																																					"weapons",	"merc",		WP_BRYAR_OLD},
	{3, "Melee",				"allows you to attack with your fists and legs. You can punch, kick or do a special melee attack by holding both Attack and Alt Attack buttons (usually the mouse buttons).",																																	"weapons",	"merc",		0},
	{5, "Max Shield",			"The max shield (armor) the player can have. Each level increases 20 per cent of max shield the player can have",																																																"other",	"merc",		0},
	{4, "Shield Strength",		"Each level increases your shield resistance by 7 per cent",																																																													"other",	"merc",		0},
	{4, "Health Strength",		"Each level increases your health resistance by 7 per cent",																																																													"other",	"merc",		0},
	{1, "Drain Shield",			"When using Drain force power, and your health is full, restores some shield. It also makes Drain suck hp/shield from the enemy to restore your hp/shield",																																						"other",	"merc",		0},
	{3, "Jetpack",				"the jetpack, used by Boba Fett. Allows you to fly. To use it, jump and press the Use key (usually R) while in the middle of the jump. Each level uses less fuel, allowing you to fly for a longer time",																										"items",	"merc",		HI_JETPACK},
	{3, "Sense Health",			"allows you to see info about someone, including npcs. Level 1 shows current health. Level 2 shows name, health and shield. Level 3 shows name, health and max health, shield and max shield, force and max force, mp and max mp. To use it, when you are near a player or npc, use ^3Sense ^7force power",		"force",	"light",	0},
	{3, "Shield Heal",			"recovers 4 shield at level 1, 8 shield at level 2 and 12 shield at level 3. To use it, use Heal force power when you have full HP.",																																											"other",	"merc",		0},
	{3, "Team Shield Heal",		"recovers 3 shield at level 1, 6 shield at level 2 and 9 shield at level 3 to players near you. To use it, when near players, use Team Heal force power. It will heal their shield after they have full HP",																									"other",	"merc",		0},
	// GalaxyRP fix: [Skills] indices 38-42 below (Unique Skill/Blaster Pack/Powercell/Metal Bolts/Rockets)
	// are removed from use -- do_upgrade_skill()/do_downgrade_skill() reject them outright and they're no
	// longer shown in ingame_galaxyrp.menu's Skills section. 39-42 never had any gameplay effect coded
	// (unlike the sibling ammo skills at 43-45, which do grant their weapon in initialize_rpg_skills()).
	// 38 did have an effect (a self-heal on the Engage Duel key, in g_active.c) but that was the last
	// remnant of the /unique command's Unique Abilities -- a fully-removed 10-class system -- so its
	// gameplay hook has been removed outright rather than left disabled. All five are left as
	// reserved/unused entries here, rather than deleted outright, so every other skill's numeric index
	// (43+) doesn't shift.
	{1, "Unique Skill",			"placeholder, does nothing",																																																																					"other",	"merc",		0},
	{3, "Blaster Pack",			"used as ammo for Blaster Pistol, Bryar Pistol and E11 Blaster Rifle.",																																																											"ammo",		"merc",		0},
	{3, "Powercell",			"used as ammo for Disruptor, Bowcaster and DEMP2.",																																																																"ammo",		"merc",		0},
	{3, "Metal Bolts",			"used as ammo for Repeater, Flechette and Concussion Rifle.",																																																													"ammo",		"merc",		0},
	{3, "Rockets",				"used as ammo for Rocket Launcher.",																																																																			"ammo",		"merc",		0},
	{3, "Thermals",				"the famous detonator used by Leia in Ep 6 at the Jabba Palace. Normal fire throws it, which explodes after some seconds. Alt fire throws it and it explodes as soon as it touches something.",																													"ammo",		"merc",		WP_THERMAL},
	{3, "Trip Mines",			"a mine that can be planted somewhere. Normal fire plants a mine with a laser that when touched makes the mine explode. Alt fire plants proximity mines",																																						"ammo",		"merc",		WP_TRIP_MINE},
	{3, "Detpacks",				"a very powerful explosive, which you can detonate remotely with the alt fire button.",																																																							"ammo",		"merc",		WP_DET_PACK},
	{1, "Binoculars",			"this item allows you to see distant things better with its zoom.",																																																												"items",	"merc",		HI_BINOCULARS},
	{1, "Bacta Canister",		"allows you to recover 25 HP",																																																																					"items",	"merc",		HI_MEDPAC},
	{1, "Sentry Gun",			"after placed on the ground, shoots at any nearby enemy",																																																														"items",	"merc",		HI_SENTRY_GUN},
	{1, "Seeker Drone",			"a flying ball that flies around you, shooting anyone in its range",																																																											"items",	"merc",		HI_SEEKER},
	{1, "E-Web",				"allows you to shoot at people with it, it has a good fire rate",																																																												"items",	"merc",		HI_EWEB},
	{1, "Big Bacta",			"allows you to recover 50 HP",																																																																					"items",	"merc",		HI_MEDPAC_BIG},
	// GalaxyRP fix: [Skills] value_internal was 0 here -- the same collision Heal (skill_id 9, see the
	// FP_HEAL comment in apply_skill_change_in_game() below) hit, except unguarded: apply_skill_change_
	// in_game()'s "items" branch is gated on value_internal != 0, so with this at 0 it silently skipped
	// Force Field on every live /skillup or /skilldown. initialize_rpg_skills() (the respawn-time full
	// reload) grants HI_SHIELD off pers.skill_levels[52] directly and was never affected -- which is
	// exactly why the item only ever showed up after a /kill or respawn, never immediately. Corrected
	// to HI_SHIELD, matching every other items-category skill in this table.
	{1, "Force Field",			"a powerful shield that protects you from enemy attacks, it can resist a lot against any weapon",																																																				"items",	"merc",		HI_SHIELD},
	{1, "Cloak Item",			"makes you almost invisible to players and invisible to npcs.",																																																													"items",	"merc",		HI_CLOAK},
	{5, "Force Power",			"increases the max force power you have. Necessary to allow you to use force powers and force-based skills",																																																	"force",	"neutral",	0},
	{3, "Improvements",			"placeholder, does nothing",																																																																					"items",	"merc",		0},
	{5, "Armor",				"Each level increases your damage resistance by 10 percent, but also decreases your movement speed by 10 percent.",																																																"items",	"merc",		0},
	{2, "Flame Thrower",		"Allows you to use a flamethrower. Used by alt-firing with a stun baton.",																																																										"items",	"merc",		0},
	{5, "Shield Regeneration",	"Increases the rate at which shield is regenerated by one per second for each point. Meditating will make it faster.",																																															"other",	"merc",		0},
	{5, "Health Regeneration",	"Increases the rate at which health is regenerated by one per second for each point. Meditating will make it faster.",																																															"other",	"merc",		0},
};

#define MAX_WORDED_EMOTES 132
//alex: type for storing worde animations wo use with the emote system
typedef struct worded_animation_s {
	const char* animation_name;
	int			animation_code;
	const char* animation_category;
} worded_animation_t;

/*
alex: list of all worded emotes, ids, and categories, these are to be stored alphabetically
the animation code is to be exactly the same as the correcponding id in anims.h
*/
const worded_animation_t animations[MAX_WORDED_EMOTES] = {
	{"aim",				BOTH_STAND4TOATTACK2,		"Blaster"	},
	{"aim2",			BOTH_STAND5TOAIM,			"Blaster"	},
	{"aim3",			BOTH_ATTACK2,				"Blaster"	},
	{"aim4",			TORSO_WEAPONIDLE4,			"Blaster"	},
	{"aim5",			TORSO_WEAPONIDLE3,			"Blaster"	},
	{"anikata",			BOTH_ANAKINKATA,			"Saber"		},
	{"anikata2",		BOTH_ANAKINKATA2,			"Saber"		},
	{"anikata3",		BOTH_ANAKINKATA3,			"Saber"		},
	{"ataru",			BOTH_ATARU,					"Saber"		},
	{"beg",				BOTH_KNEES1,				"Body"		},
	{"beg2",			BOTH_DEATH14_SITUP,			"Body"		},
	{"beg3",			BOTH_CHOKE2,				"Body"		},
	{"bow",				BOTH_BOW,					"Body"		},
	{"bump",			BOTH_FISTBUMP,				"Body"		},
	{"carry",			BOTH_CARRY,					"Movement"	},
	{"choked",			BOTH_CHOKE3,				"Body"		},
	{"commlinkdown",	BOTH_TALKCOMM1STOP,			"Body"		},
	{"commlinkup",		BOTH_TALKCOMM1START,		"Body"		},
	{"cover",			BOTH_DODGE_HOLD_FL,			"Movement"	},
	{"cross",			BOTH_ARMSCROSSED,			"Body"		},
	{"cuffed",			BOTH_STAND4,				"Body"		},
	{"cufffront",		BOTH_CUFFEDFRONT,			"Body"		},
	{"cuffknees",		BOTH_CUFFEDKNEES,			"Body"		},
	{"cup",				BOTH_COFFEE_IDLE,			"Body"		},
	{"cupsip",			BOTH_COFFEE_SIP,			"Body"		},
	{"datapad",			BOTH_DATAPAD,				"Body"		},
	{"datapad2",		BOTH_DATAPAD2,				"Body"		},
	{"die",				BOTH_DEATH14_UNGRIP,		"Body"		},
	{"djemso",			BOTH_DJEMSO,				"Saber"		},
	{"djemso2",			BOTH_DJEMSO2,				"Saber"		},
	{"drainloop",		BOTH_FORCE_DRAIN_GRAB_HOLD,	"Force"		},
	{"drink",			BOTH_COFFEE_SIP,			"Body"		},
	{"facepalm",		BOTH_FACEPALM,				"Body"		},
	{"facepalm2",		BOTH_FACEPALM2,				"Body"		},
	{"fear",			BOTH_SONICPAIN_HOLD,		"Body"		},
	{"flourish",		BOTH_SHOWOFF_FAST,			"Saber"		},
	{"flourish2",		BOTH_SHOWOFF_MEDIUM,		"Saber"		},
	{"flourish3",		BOTH_SHOWOFF_STRONG,		"Saber"		},
	{"flourish4",		BOTH_SHOWOFF_DUAL,			"Saber"		},
	{"flourish5",		BOTH_SHOWOFF_STAFF,			"Saber"		},
	{"force",			BOTH_USEFORCE,				"Force"		},
	{"forcecasual",		BOTH_FORCECASUAL,			"Force"		},
	{"forcechoke",		BOTH_FORCEGRIP3,			"Force"		},
	{"forcelightning",	BOTH_FORCELIGHTNING_HOLD,	"Force"		},
	{"guard",			BOTH_GUARD,					"Saber"		},
	{"gunspin1",		BOTH_GUNSPINB,				"Blaster"	},
	{"gunspin2",		BOTH_GUNSPINF,				"Blaster"	},
	{"gunspin3",		BOTH_GUNSPINS,				"Blaster"	},
	{"handsback",		BOTH_HANDSBACK,				"Body"		},
	{"handsfront",		BOTH_HANDSFRONT,			"Body"		},
	{"handstand",		BOTH_HANDSTAND,				"Body"		},
	{"handstand2",		BOTH_HANDSTAND2,			"Body"		},
	{"headhold",		BOTH_HANDSHEAD,				"Body"		},
	{"helpedup",		BOTH_HELPEDUP,				"Body"		},
	{"helpup",			BOTH_HELPUP,				"Body"		},
	{"heroic",			BOTH_STAND5TOSTAND8,		"Body"		},
	{"hips",			BOTH_HANDSHIPS,				"Body"		},
	{"hips2",			BOTH_HANDSHIPS2,			"Body"		},
	{"holddetonator",	TORSO_WEAPONREADY10,		"Body"		},
	{"holdobject",		BOTH_OBJECT,				"Body"		},
	{"hug",				BOTH_HUGGER1,				"Body"		},
	{"hurt",			BOTH_HURT,					"Body"		},
	{"hurt2",			BOTH_HURT2,					"Body"		},
	{"idle",			BOTH_SABERIDLE,				"Saber"		},
	{"jarkai",			BOTH_JARKAI,				"Saber"		},
	{"jarkai2",			BOTH_JARKAIREVERSE,			"Saber"		},
	{"juyo",			BOTH_JUYO,					"Saber"		},
	{"kneel",			BOTH_CROUCH3,				"Body"		},
	{"lean",			BOTH_STAND10,				"Body"		},
	{"leanback",		BOTH_LEANWALL,				"Body"		},
	{"leanfront",		BOTH_LEANFRONT,				"Body"		},
	{"leantable",		BOTH_STAND7TOSTAND8,		"Body"		},
	{"makashi",			BOTH_MAKASHI,				"Saber"		},
	{"meditate",		BOTH_STAND5TOSIT2,			"Body"		},
	{"meditate2",		BOTH_MEDITATION2,			"Force"		},
	{"meditate3",		BOTH_MEDITATEFORCE,			"Force"		},
	{"mindtrick",		BOTH_MINDTRICK1,			"Force"		},
	{"niman",			BOTH_NIMAN,					"Saber"		},
	{"pistol",			BOTH_PISTOLREADY,			"Blaster"	},
	{"point",			BOTH_STAND5TOAIM,			"Body"		},
	{"ponder",			BOTH_PONDER,				"Body"		},
	{"ponder2",			BOTH_PONDER2,				"Body"		},
	{"pressbutton",		BOTH_BUTTON_HOLD,			"Body"		},
	{"pushup",			BOTH_PUSHUP,				"Body"		},
	{"quickdraw",		BOTH_QUICKDRAW,				"Blaster"	},
	{"quickdraw2",		BOTH_QUICKDRAW2,			"Blaster"	},
	{"read",			BOTH_READ,					"Body"		},
	{"relax",			BOTH_LAYDOWN,				"Body"		},
	{"saberdraw1",		BOTH_SABERDRAW1,			"Saber"		},
	{"saberdraw2",		BOTH_SABERDRAW2,			"Saber"		},
	{"saberdraw3",		BOTH_SABERDRAW3,			"Saber"		},
	{"saberdraw4",		BOTH_SABERDRAW4,			"Saber"		},
	{"saberdraw5",		BOTH_SABERDRAW5,			"Saber"		},
	{"saberpoint",		BOTH_SABERPOINT,			"Saber"		},
	{"saberpoint2",		BOTH_SABERPOINT2,			"Saber"		},
	{"saberthrow",		BOTH_SABERTHROW1START,		"Saber"		},
	{"salute",			BOTH_SALUTE,				"Body"		},
	{"scratch",			BOTH_HEADSCRATCH,			"Body"		},
	{"shien",			BOTH_SHIEN,					"Saber"		},
	{"shien2",			BOTH_SHIEN2,				"Saber"		},
	{"shien3",			BOTH_SHIEN3,				"Saber"		},
	{"shiicho",			BOTH_SHIICHO,				"Saber"		},
	{"sit",				BOTH_SIT2,					"Body"		},
	{"sit2",			BOTH_SIT3,					"Body"		},
	{"sit3",			BOTH_SIT6,					"Body"		},
	{"sit4",			BOTH_SITARMS,				"Body"		},
	{"sit5",			BOTH_SITCROSS,				"Body"		},
	{"sit6",			BOTH_SITCROSS2,				"Body"		},
	{"sit7",			BOTH_SITLEAN,				"Body"		},
	{"sitfeet",			BOTH_SITFEET,				"Body"		},
	{"sitpalm",			BOTH_SITPALM,				"Body"		},
	{"sitpalm2",		BOTH_SITPALM2,				"Body"		},
	{"sitpilot",		BOTH_GUNSIT1,				"Body"		},
	{"situp",			BOTH_SITUP,					"Body"		},
	{"sleep",			BOTH_SLEEP1,				"Body"		},
	{"sneak",			BOTH_CROUCH1,				"Movement"	},
	{"soresu",			BOTH_SORESU,				"Saber"		},
	{"soresu2",			BOTH_SORESU2,				"Saber"		},
	{"spreadlegs",		BOTH_KNEEL_TO_STAND,		"Body"		},
	{"stance",			BOTH_SABERSTANCE,			"Saber"		},
	{"stance2",			BOTH_SABERSTANCE2,			"Saber"		},
	{"stance3",			BOTH_SABERSTANCE3,			"Saber"		},
	{"surrender",		TORSO_SURRENDER_START,		"Body"		},
	{"tossleft",		BOTH_TOSS1,					"Force"		},
	{"tossright",		BOTH_TOSS2,					"Force"		},
	{"type",			BOTH_CONSOLE1,				"Body"		},
	{"victory",			BOTH_WINGS_CLOSE,			"Saber"		},
	{"victory2",		BOTH_DEATH14_UNGRIP,		"Saber"		},
	{"victory3",		BOTH_DEATH14_SITUP,			"Saber"		},
	{"victory4",		BOTH_VICTORY_STAFF,			"Saber"		},
	{"wave",			BOTH_SILENCEGESTURE1,		"Body"		},
	{"windy",			BOTH_WIND,					"Movement"	},
};

#define MAX_EMOTE_CATEGORIES 5

const char anim_headers[MAX_EMOTE_CATEGORIES][50] = {
	"Body",
	"Movement",
	"Blaster",
	"Saber",
	"Force"
};

#define BROADCAST_DISTANCE 999999999
#define VOICE_DISTANCE 600
#define VOICE_DISTANCE_LONG 2000
#define VOICE_DISTANCE_LOW 65
#define SHOUT_DISTANCE 1500
#define ACTION_DISTANCE 1200
#define ACTION_DISTANCE_LOW 200
#define ACTION_DISTANCE_LONG 2000

const chat_modifiers_t chat_modifiers[] = {
	{"/low",		"chat \"%s^9 lowers their voice:%s\n\"",			VOICE_DISTANCE_LOW	},
	{"/long",		"chat \"%s:%s\n\"",									VOICE_DISTANCE_LONG	},
	{"/all",		"chat \"%s:^2%s\n\"",								BROADCAST_DISTANCE	},
	{"/melow",		"chat \"%s^3%s\n\"",								ACTION_DISTANCE_LOW	},
	{"/meall",		"chat \"%s^3%s\n\"",								BROADCAST_DISTANCE	},
	{"/melong",		"chat \"%s^3%s\n\"",								ACTION_DISTANCE_LONG},
	{"/me",			"chat \"%s^3%s\n\"",								ACTION_DISTANCE		},
	{"/shoutlong",	"chat \"%s ^3shouts:^2%s\n\"",						VOICE_DISTANCE_LONG	},
	{"/shoutall",	"chat \"%s ^3shouts:^2%s\n\"",						BROADCAST_DISTANCE	},
	{"/shout",		"chat \"%s ^3shouts:^2%s\n\"",						SHOUT_DISTANCE		},
	{"/dolow",		"chat \"^3(%s^3)%s\n\"",							ACTION_DISTANCE_LOW	},
	{"/dolong",		"chat \"^3(%s^3)%s\n\"",							ACTION_DISTANCE_LONG},
	{"/doall",		"chat \"^3(%s^3)%s\n\"",							BROADCAST_DISTANCE	},
	{"/do",			"chat \"^3(%s^3)%s\n\"",							ACTION_DISTANCE		},
	{"/forcelow",	"chat \"%s^5 uses the Force to%s\n\"",				ACTION_DISTANCE_LOW	},
	{"/forcelong",	"chat \"%s^5 uses the Force to%s\n\"",				ACTION_DISTANCE_LONG},
	{"/forceall",	"chat \"%s^5 uses the Force to%s\n\"",				BROADCAST_DISTANCE	},
	{"/force",		"chat \"%s^5 uses the Force to%s\n\"",				ACTION_DISTANCE		},
	{"/mylow",		"chat \"%s^3's %s\n\"",								ACTION_DISTANCE_LOW	},
	{"/myall",		"chat \"%s^3's %s\n\"",								BROADCAST_DISTANCE	},
	{"/mylong",		"chat \"%s^3's %s\n\"",								ACTION_DISTANCE_LONG},
	{"/my",			"chat \"%s^3's %s\n\"",								ACTION_DISTANCE		},
	{"/ryl2",		"chat \"%s ^3(Ryl - Lekku only):^2%s\n\"",			VOICE_DISTANCE		},
	{"/ryl",		"chat \"%s ^3(Ryl):^2%s\n\"",						VOICE_DISTANCE		},
	{"/rodian",		"chat \"%s ^3(Rodian):^2%s\n\"",					VOICE_DISTANCE		},
	{"/huttese",	"chat \"%s ^3(Huttese):^2%s\n\"",					VOICE_DISTANCE		},
	{"/catharese",	"chat \"%s ^3(Catharese):^2%s\n\"",					VOICE_DISTANCE		},
	{"/mando",		"chat \"%s ^3(Mando'a):^2%s\n\"",					VOICE_DISTANCE		},
	{"/npc",		"chat \"^3(%s^3) NPC:^4%s\n\"",						VOICE_DISTANCE		},
	{"/npclow",		"chat \"^3(%s^3) NPC Lowers their voice:^4%s\n\"",	VOICE_DISTANCE_LOW	},
	{"/npcall",		"chat \"^3(%s^3) NPC:^4%s\n\"",						BROADCAST_DISTANCE	},
	{"/comm",		"chat \"^6<%s^6>^3 -C-^2%s\n\"",					BROADCAST_DISTANCE	},
	{"/c",			"chat \"^6<%s^6>^3 -C-^2%s\n\"",					BROADCAST_DISTANCE	},
	{"/thought",	"chat \"%s ^7is thinking: %s\n\"",					BROADCAST_DISTANCE	},
};

void play_animation(gentity_t *ent, int animation, int time) {
	ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
	ent->client->ps.forceDodgeAnim = animation;
	// GalaxyRP fix: [logic] the time parameter was ignored in favor of a hardcoded +1000.
	// Every current call site happens to pass 1000 so this was previously harmless, but the
	// function should honor the value it's actually given.
	ent->client->ps.forceHandExtendTime = level.time + time;
}

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage( gentity_t *ent ) {
	char		entry[1024];
	char		string[1400];
	int			stringlength;
	int			i, j;
	gclient_t	*cl;
	int			numSorted, scoreFlags, accuracy, perfect;

	// send the latest information on all clients
	string[0] = 0;
	stringlength = 0;
	scoreFlags = 0;

	numSorted = level.numConnectedClients;

	if (numSorted > MAX_CLIENT_SCORE_SEND)
	{
		numSorted = MAX_CLIENT_SCORE_SEND;
	}

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->pers.connected == CON_CONNECTING ) {
			ping = -1;
		} else {
			ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
		}

		if( cl->accuracy_shots ) {
			accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
		}
		else {
			accuracy = 0;
		}
		perfect = ( cl->ps.persistant[PERS_RANK] == 0 && cl->ps.persistant[PERS_KILLED] == 0 ) ? 1 : 0;

		Com_sprintf (entry, sizeof(entry),
			" %i %i %i %i %i %i %i %i %i %i %i %i %i %i", level.sortedClients[i],
			cl->pers.level, cl->ps.persistant[PERS_KILLED], ping,
			scoreFlags, g_entities[level.sortedClients[i]].s.powerups, accuracy,
			cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
			cl->ps.persistant[PERS_EXCELLENT_COUNT],
			cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
			cl->ps.persistant[PERS_DEFEND_COUNT],
			cl->ps.persistant[PERS_ASSIST_COUNT],
			perfect,
			cl->ps.persistant[PERS_CAPTURES]);
		j = strlen(entry);
		if (stringlength + j > 1022)
			break;
		strcpy (string + stringlength, entry);
		stringlength += j;
	}

	//still want to know the total # of clients
	i = level.numConnectedClients;

	trap->SendServerCommand( ent-g_entities, va("scores %i %i %i%s", i,
		level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
		string ) );
}


/*
==================
Cmd_Score_f

Request current scoreboard information
==================
*/
void Cmd_Score_f( gentity_t *ent ) {
	DeathmatchScoreboardMessage( ent );
}

/*
==================
ConcatArgs
==================
*/
char	*ConcatArgs( int start ) {
	int		i, c, tlen;
	static char	line[MAX_STRING_CHARS];
	int		len;
	char	arg[MAX_STRING_CHARS];

	len = 0;
	c = trap->Argc();
	for ( i = start ; i < c ; i++ ) {
		trap->Argv( i, arg, sizeof( arg ) );
		tlen = strlen( arg );
		if ( len + tlen >= MAX_STRING_CHARS - 1 ) {
			break;
		}
		memcpy( line + len, arg, tlen );
		len += tlen;
		if ( i != c - 1 ) {
			line[len] = ' ';
			len++;
		}
	}

	line[len] = 0;

	return line;
}

/*
==================
StringIsInteger
==================
*/
qboolean StringIsInteger( const char *s ) {
	int			i=0, len=0;
	qboolean	foundDigit=qfalse;

	for ( i=0, len=strlen( s ); i<len; i++ )
	{
		if ( !isdigit( s[i] ) )
			return qfalse;

		foundDigit = qtrue;
	}

	return foundDigit;
}

/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString( gentity_t *to, const char *s, qboolean allowconnecting ) {
	gclient_t	*cl;
	int			idnum;
	char		cleanInput[MAX_NETNAME];

	if ( StringIsInteger( s ) )
	{// numeric values could be slot numbers
		idnum = atoi( s );
		if ( idnum >= 0 && idnum < level.maxclients )
		{
			cl = &level.clients[idnum];
			if ( cl->pers.connected == CON_CONNECTED )
				return idnum;
			else if ( allowconnecting && cl->pers.connected == CON_CONNECTING )
				return idnum;
		}
	}

	Q_strncpyz( cleanInput, s, sizeof(cleanInput) );
	Q_StripColor( cleanInput );

	for ( idnum=0,cl=level.clients; idnum < level.maxclients; idnum++,cl++ )
	{// check for a name match
		if ( cl->pers.connected != CON_CONNECTED )
			if ( !allowconnecting || cl->pers.connected < CON_CONNECTING )
				continue;

		// zyk: changed this so the player name must contain the search string
		if ( Q_stristr( cl->pers.netname_nocolor, cleanInput ) ) // if ( !Q_stricmp( cl->pers.netname_nocolor, cleanInput ) )
			return idnum;
	}

	if (to)
		trap->SendServerCommand( to-g_entities, va( "print \"User %s is not on the server\n\"", s ) );
	else
		trap->Print(va( "User %s is not on the server\n", s ));

	return -1;
}

void print_table_horizontal_line(gentity_t *ent) {
	trap->SendServerCommand(ent - g_entities, "print \" ^9======================================\n\"");
	return;
}

// GalaxyRP fix: [cleanup] const for the same reason as print_row() below, which is this
// function's only caller and needs to pass its own (now-const) text parameter through.
int get_max_spaces_right(const char text[MAX_STRING_CHARS]) {
	return 33 - strlen(text);
}

// GalaxyRP fix: [cleanup] both callers (below) pass a const char* -- anim_headers is itself
// declared const, and animation_name is a const char* struct field -- which MSVC's C4090 flagged
// as a const-qualifier mismatch against this parameter. This function only reads text (strlen via
// strcat, never writes through it), so const is the correct, no-behavior-change fix.
void print_row(gentity_t *ent, const char text[MAX_STRING_CHARS]) {
	char emote_row[MAX_STRING_CHARS] = " ";

	strcat(emote_row, "^9|| ^3");

	strcat(emote_row, text);

	for (int i = 0; i < get_max_spaces_right(text); i++) {
		strcat(emote_row, " ");
	}

	strcat(emote_row, "^9||\n");
	
	trap->SendServerCommand(ent - g_entities, va("print \"%s\"", emote_row));
	
	return;
}

// GalaxyRP fix: [cleanup] const for the same reason as print_row() above -- this only reads
// header_text, and its only caller (print_header() below) needs to pass it through as const.
void print_heading_text_row(gentity_t *ent, const char header_text[MAX_STRING_CHARS]) {
	//34 characters left for space and text
	int length = strlen(header_text);

	char emote_row[MAX_STRING_CHARS] = " ";

	strcat(emote_row, "^9||^3");

	qboolean can_be_centered;

	if (length % 2 == 0) {
		can_be_centered = qtrue;
	}
	else {
		can_be_centered = qfalse;
	}

	int number_of_spaces = 34 - length;
	int left_spaces;
	int right_spaces;

	if (can_be_centered) {
		left_spaces = number_of_spaces / 2;
		right_spaces = left_spaces;
	}
	else {
		//it will be rounded down
		left_spaces = number_of_spaces / 2;
		right_spaces = left_spaces + 1;
	}

	//take care of the space to the left of the writing
	for (int i = 0; i < left_spaces; i++) {
		strcat(emote_row, " ");
	}

	strcat(emote_row, header_text);

	//take care of the space to the right of the writing
	for (int i = 0; i < right_spaces; i++) {
		strcat(emote_row, " ");
	}

	strcat(emote_row, "^9||\n");

	trap->SendServerCommand(ent - g_entities, va("print \"%s\"", emote_row));

	return;
}

// GalaxyRP fix: [cleanup] const for the same reason as print_row() above.
void print_header(gentity_t *ent, const char text[MAX_STRING_CHARS]) {
	print_table_horizontal_line(ent);
	print_heading_text_row(ent, text);
	print_table_horizontal_line(ent);
}

typedef struct admin_command_description_s {
	const char* title;
	int			number;
} admin_command_description_t;

const admin_command_description_t admin_commands[ADM_NUM_CMDS] = {
	{ "NPC",					ADM_NPC					},
	{ "No Clip",				ADM_NOCLIP				},
	{ "Give Admin",				ADM_GIVEADM				},
	{ "Teleport",				ADM_TELE				},
	{ "Admin Protect",			ADM_ADMPROTECT			},
	{ "Entity System",			ADM_ENTITYSYSTEM		},
	{ "Silence",				ADM_SILENCE				},
	{ "Client Print",			ADM_CLIENTPRINT			},
	{ "Shake Screen",			ADM_SHAKESCREEN			},
	// GalaxyRP fix: [Admin] title updated to reflect /killother now sharing this bit -- see the
	// GalaxyRP fix comment in Cmd_KillOther_f.
	{ "Kick / Kill Other",		ADM_KICK				},
	{ "Paralyze",				ADM_PARALYZE			},
	{ "Give",					ADM_GIVE				},
	{ "Scale",					ADM_SCALE				},
	{ "Players",				ADM_PLAYERS				},
	{ "Duel Arena",				ADM_DUELARENA			},
	{ "Change Map",				ADM_CHANGEMAP			},
	{ "Create Item",			ADM_CREATEITEM			},
	{ "God Mode",				ADM_GOD					},
	{ "Level Give",				ADM_LEVELUP				},
	{ "Skill Give",				ADM_SKILL				},
	{ "Create Credits",			ADM_CREATECREDITS		},
	{ "Ignore Char Distance",	ADM_IGNORECHATDISTANCE	},
	{ "Give XP",				ADM_XP					},
	{ "Update News",			ADM_UPDATENEWS			},
	{ "Remove News",			ADM_REMOVENEWS			},
	{ "Play Music",				ADM_MUSIC				},
	{ "Instant Revive",			ADM_GETUP				}
};

qboolean check_admin_command(gentity_t* ent, int admin_command, qboolean with_message) {
	if (!(ent->client->pers.bitvalue & (1 << admin_command)))
	{
		if (with_message) {
			trap->SendServerCommand(ent - g_entities, va("print \"^1You don't have the necessary admin command to execute this.\n^1You need the ^3%s ^1admin command.\n\"", admin_commands[admin_command].title));
		}
		return qfalse;
	}

	return qtrue;
}

// GalaxyRP: [security] adapted from the newer Zyk mod's own zyk_check_user_input() (found while
// comparing that mod's entity-manipulation commands against ours). Rejects any string that is empty,
// too long, or contains anything outside A-Z/a-z/0-9. Use this on any raw player-typed argument that
// is later going to be spliced into a filesystem path (or, worse, a system() call) -- e.g. the
// <filename> argument of /entsave, /entload and /entdeletefile, and a new character's name in
// create_new_character() -- so a value like "../../whatever" or a shell metacharacter can't reach
// fopen()/remove()/system() and escape the folder (or command) it was meant to stay inside.
qboolean zyk_check_user_input(char *user_input, int user_input_size) {
	int i = 0;

	static const char allowed_chars[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789";

	if (user_input_size > MAX_STRING_CHARS)
	{ // zyk: somehow the string is bigger than the max
		return qfalse;
	}

	if (user_input[0] == '\0')
	{ // zyk: empty string
		return qfalse;
	}

	while (user_input[i] != '\0' && i < user_input_size)
	{
		if (strchr(allowed_chars, user_input[i]) == NULL)
		{ // zyk: char is not one of the allowed ones
			return qfalse;
		}

		i++;
	}

	if (user_input[i] != '\0' && i == user_input_size)
	{ // zyk: string did not terminate with NULL
		return qfalse;
	}

	return qtrue;
}

void show_animation_list(gentity_t* ent, int beginning_index, int end_index) {
	for (int i = beginning_index; i < end_index; i++) {
		print_header(ent, anim_headers[i]);
		for (int j = 0; j < MAX_WORDED_EMOTES; j++) {
			//alex: if animation is in that category
			// GalaxyRP fix: [macOS/Clang build failure] stricmp is an MSVC CRT extension, not
			// declared on Linux/macOS -- use the codebase's own portable Q_stricmp (already used
			// elsewhere in this file), matching what every other case-insensitive compare here does.
			if (Q_stricmp(animations[j].animation_category, anim_headers[i]) == 0) {
				print_row(ent, animations[j].animation_name);
			}
		}
	}
	//alex: end the table
	print_table_horizontal_line(ent);

	return;
}

// alex: plays an animation from anims.h by id OR a word (look for animation_t)
void Cmd_Emote_f( gentity_t *ent )
{
	char arg[MAX_TOKEN_CHARS] = {0};
	char anim_id[100] = "";

	if (rp_allow_emotes.integer < 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot use emotes in this server\n\"" );
		return;
	}

	if (level.gametype == GT_SIEGE)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot use emotes in Siege gametype\n\"" );
		return;
	}

	if (rp_allow_emotes.integer != 1 && ent->client->ps.duelInProgress == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use emotes in private duel\n\"");
		return;
	}

	if ( trap->Argc () < 2 ) {
		trap->SendServerCommand( ent-g_entities, va("print \"Usage: emote <anim id between 0 and %d>\n\"",MAX_ANIMATIONS-1) );
		return;
	}

	//alex: string magic
	trap->Argv( 1, arg, sizeof( arg ) );
	// GalaxyRP fix: [security] strcpy could overflow the 100-byte anim_id buffer -- arg can be
	// up to MAX_TOKEN_CHARS-1 (1023) characters long. Use the codebase's own bounded copy
	// instead, matching how player-supplied strings are copied elsewhere in this file.
	Q_strncpyz(anim_id, arg, sizeof(anim_id));

	// GalaxyRP fix: [Guardian] a guardian_mode>0 guard blocking emotes during boss battles used to be
	// here. guardian_mode is permanently 0 now, so it was unreachable.

	if (ent->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot use emotes while knocked down\n\"");
		return;
	}

	for (int i = 0; i < MAX_WORDED_EMOTES; i++)
	{
		//alex: here, the anim_id is the worded animation name
		// GalaxyRP fix: [logic] use Q_stricmp so word-emote matching is case-insensitive,
		// consistent with show_animation_list()'s category matching below.
		if (Q_stricmp(anim_id, animations[i].animation_name) == 0)
		{
			play_animation(ent, animations[i].animation_code, 1000);

			ent->client->pers.player_statuses |= (1 << 1);

			return;
		}
	}

	if (strcmp(anim_id, "list") == 0)
	{
		if (trap->Argc() == 3) {
			trap->Argv(2, arg, sizeof(arg));
			int page = atoi(arg);

			if (page == 2) {
				show_animation_list(ent, 3, MAX_EMOTE_CATEGORIES);
			}
			else {
				trap->SendServerCommand(ent - g_entities, "print \"That is not a valid emote category!\n\"");
			}
		}
		else {
			show_animation_list(ent, 0, 3);
			trap->SendServerCommand(ent - g_entities, "print \"^3Page 1/2. To see the rest of the animations, do /emote list 2\n\"");
		}

		// GalaxyRP fix: [cleanup] both list branches now return here instead of only the
		// "page 1" branch doing so -- previously the invalid-page message fell off the end of
		// the function instead of returning explicitly.
		return;
	}

	// GalaxyRP fix: [logic] numeric-id parsing now uses strtol so it can tell "the player typed
	// 0" apart from "the player typed something that isn't a number" -- atoi() silently returns
	// 0 for both, which used to make anim id 0 (a real animation, BOTH_DEATH1) unplayable, and
	// swallowed all garbage/unrecognized input with no error message at all.
	char* end_ptr = NULL;
	long anim_id_int = strtol(anim_id, &end_ptr, 10);

	if (end_ptr != anim_id && *end_ptr == '\0')
	{
		if (anim_id_int >= 0 && anim_id_int < MAX_ANIMATIONS)
		{
			play_animation(ent, (int)anim_id_int, 1000);

			ent->client->pers.player_statuses |= (1 << 1);

			return;
		}

		trap->SendServerCommand( ent-g_entities, va("print \"Usage: anim ID must be between 0 and %d\n\"",MAX_ANIMATIONS-1) );
		return;
	}

	trap->SendServerCommand( ent-g_entities, va("print \"Unknown emote \\\"%s\\\". Use /emote list to see available emotes.\n\"", anim_id) );
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void zyk_remove_force_powers( gentity_t *ent )
{
	int i = 0;

	for (i = FP_HEAL; i < NUM_FORCE_POWERS; i++)
	{
		ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
		ent->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_0;
	}

	// GalaxyRP fix: [cleanup] this used to re-clear the WP_SABER bit on every loop iteration above
	// (NUM_FORCE_POWERS times); the bit doesn't depend on the loop index, so clearing it once here
	// is equivalent and skips the redundant repeats.
	ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);

	ent->client->ps.weapon = WP_MELEE;

	// zyk: reset the force powers of this player
	WP_InitForcePowers( ent );

	if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] > FORCE_LEVEL_0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);
}

void zyk_adjust_holdable_items(gentity_t *ent);
void zyk_remove_guns( gentity_t *ent )
{
	int i = 0;

	for (i = WP_STUN_BATON; i < WP_NUM_WEAPONS; i++)
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << i);
	}

	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
	ent->client->ps.weapon = WP_MELEE;

	ent->client->ps.ammo[AMMO_BLASTER] = 0;
	ent->client->ps.ammo[AMMO_POWERCELL] = 0;
	ent->client->ps.ammo[AMMO_METAL_BOLTS] = 0;
	ent->client->ps.ammo[AMMO_ROCKETS] = 0;
	ent->client->ps.ammo[AMMO_THERMAL] = 0;
	ent->client->ps.ammo[AMMO_TRIPMINE] = 0;
	ent->client->ps.ammo[AMMO_DETPACK] = 0;
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] = (1 << HI_NONE);

	// GalaxyRP fix: [Items] this cleared the holdable-items ownership bitmask above, but never told
	// the client to deselect its currently-selected holdable item or decloak it -- so /logout left a
	// holdable item (e.g. the Cloak Item) visibly still selected/active, even though it was no longer
	// owned, until something else happened to refresh STAT_HOLDABLE_ITEM. zyk_adjust_holdable_items()
	// already exists for exactly this cleanup (it was only ever called from one item-drop path); call
	// it here too now that ownership has actually changed.
	zyk_adjust_holdable_items(ent);

	if (ent->client->jetPackOn)
	{
		Jetpack_Off(ent);
	}

	// zyk: reset the force powers of this player
	WP_InitForcePowers( ent );

	if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] > FORCE_LEVEL_0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);

	// GalaxyRP fix: [Account] this granted WP_BRYAR_PISTOL without ever giving it any ammo -- the
	// ammo-zeroing block above (correct for the weapons that get fully removed) also zeroed
	// AMMO_BLASTER, and nothing after it restored any, so the Bryar Pistol was equipped but had 0
	// shots and (with cg_autoSwitch on, the client default) couldn't even be selected from the weapon
	// wheel -- invisible in practice. ClientSpawn's own non-siege baseline (g_client.c) always gives a
	// fresh spawn 100 blaster ammo regardless of RPG mode; match that same baseline here, since this
	// function exists specifically to reset a player back to that same logged-out/no-guns baseline.
	ent->client->ps.ammo[AMMO_BLASTER] = 100;
}

void zyk_add_force_powers( gentity_t *ent )
{
	int i = 0;

	for (i = FP_HEAL; i < NUM_FORCE_POWERS; i++)
	{
		ent->client->ps.fd.forcePowersKnown |= (1 << i);
		if (i == FP_SABER_OFFENSE) // zyk: gives Desann and Tavion styles
			ent->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_5;
		else
			ent->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_3;
	}

	// GalaxyRP fix: [cleanup] this used to re-set the WP_SABER bit on every loop iteration above
	// (NUM_FORCE_POWERS times); the bit doesn't depend on the loop index, so setting it once here
	// is equivalent and skips the redundant repeats.
	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);
}

// GalaxyRP fix: [cleanup] ASSUMES zyk_remove_force_powers() was already called on ent (both call
// sites do this immediately beforehand) -- this used to redundantly re-clear forcePowersKnown and
// forcePowerLevel for every force power here too, right after zyk_remove_force_powers() had just
// done the exact same clear. Removed the duplicate loop; the WP_SABER bit clear right below is
// kept since it's a necessary override (zyk_remove_force_powers() can re-set that bit if the
// player's WP_InitForcePowers()-restored saber offense level is non-zero), not a duplicate.
void zyk_add_guns( gentity_t *ent )
{
	int i = 0;

	ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);

	for (i = WP_STUN_BATON; i < WP_NUM_WEAPONS; i++)
	{
		if (i != WP_SABER && i != WP_EMPLACED_GUN && i != WP_TURRET)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << i);
	}

	ent->client->ps.ammo[AMMO_BLASTER] = zyk_max_blaster_pack_ammo.integer;
	ent->client->ps.ammo[AMMO_POWERCELL] = zyk_max_power_cell_ammo.integer;
	ent->client->ps.ammo[AMMO_METAL_BOLTS] = zyk_max_metal_bolt_ammo.integer;
	ent->client->ps.ammo[AMMO_ROCKETS] = zyk_max_rocket_ammo.integer;
	ent->client->ps.ammo[AMMO_THERMAL] = zyk_max_thermal_ammo.integer;
	ent->client->ps.ammo[AMMO_TRIPMINE] = zyk_max_tripmine_ammo.integer;
	ent->client->ps.ammo[AMMO_DETPACK] = zyk_max_detpack_ammo.integer;

	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_BINOCULARS);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SEEKER);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_EWEB);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC_BIG);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SHIELD);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_CLOAK);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);
}

// GalaxyRP fix: [Classes] zyk_skill_allowed_for_class() used to live here. It had zero callers
// anywhere in the codebase (dead regardless of the rpg_class removal), so it is deleted outright.

void Cmd_Give_f( gentity_t *ent )
{
	char arg1[MAX_TOKEN_CHARS] = {0};
	char arg2[MAX_TOKEN_CHARS] = {0};
	int client_id = -1;

	if (!check_admin_command(ent, ADM_GIVE, qtrue))
	{
		return;
	}

	if (level.gametype != GT_FFA && zyk_allow_adm_in_other_gametypes.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Give command not allowed in gametypes other than FFA.\n\"" );
		return;
	}

	if (trap->Argc() != 3)
	{
		trap->SendServerCommand( ent-g_entities, "print \"^1Command Usage: ^3/give ^2<player name or ID> <force|guns>\n"
			"^1Example: ^3/give ^2PlayerName force\n"
			"^7Toggles full force powers or full weapon loadout for the target player (logged-out only).\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	trap->Argv( 2, arg2, sizeof( arg2 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode == 2)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot use on logged-in players.\n\"" );
		return;
	}

	if (g_entities[client_id].client->ps.duelInProgress == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot give stuff to players in private duels\n\"");
		return;
	}

	if (client_id < MAX_CLIENTS && level.sniper_players[client_id] != -1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot give stuff to players in Sniper Battle\n\"");
		return;
	}

	if (ent != &g_entities[client_id] && g_entities[client_id].client->sess.amrpgmode > 0 && g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << 13)))
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
		return;
	}

	if (Q_stricmp(arg2, "force") == 0)
	{
		if (g_entities[client_id].client->pers.player_statuses & (1 << 12))
		{ // zyk: remove force powers
			zyk_remove_force_powers(&g_entities[client_id]);

			g_entities[client_id].client->pers.player_statuses &= ~(1 << 12);
			trap->SendServerCommand( -1, va("print \"Removed force powers from %s^7\n\"", g_entities[client_id].client->pers.netname) );
		}
		else
		{ // zyk: add force powers
			zyk_remove_guns(&g_entities[client_id]);
			zyk_add_force_powers(&g_entities[client_id]);

			g_entities[client_id].client->pers.player_statuses &= ~(1 << 13);
			g_entities[client_id].client->pers.player_statuses |= (1 << 12);
			trap->SendServerCommand( -1, va("print \"Added force powers to %s^7\n\"", g_entities[client_id].client->pers.netname) );
		}
	}
	else if (Q_stricmp(arg2, "guns") == 0)
	{
		if (g_entities[client_id].client->pers.player_statuses & (1 << 13))
		{ // zyk: remove guns
			zyk_remove_guns(&g_entities[client_id]);

			g_entities[client_id].client->pers.player_statuses &= ~(1 << 13);
			trap->SendServerCommand( -1, va("print \"Removed guns from %s^7\n\"", g_entities[client_id].client->pers.netname) );
		}
		else
		{ // zyk: add guns
			zyk_remove_force_powers(&g_entities[client_id]);
			zyk_add_guns(&g_entities[client_id]);

			g_entities[client_id].client->pers.player_statuses &= ~(1 << 12);
			g_entities[client_id].client->pers.player_statuses |= (1 << 13);
			trap->SendServerCommand( -1, va("print \"Added guns to %s^7\n\"", g_entities[client_id].client->pers.netname) );
		}
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid option. Must be ^3force ^7or ^3guns^7.\n\"" );
		return;
	}
}

/*
==================
Cmd_Scale_f

Scales player size
==================
*/
void do_scale(gentity_t *ent, int new_size)
{
	ent->client->ps.iModelScale = new_size;
	ent->client->pers.player_scale = new_size;

	if (new_size == 100) // zyk: default size
		ent->client->pers.player_statuses &= ~(1 << 4);
	else
		ent->client->pers.player_statuses |= (1 << 4);
}

void display_scale_help(gentity_t *ent) {
	int i;
	int scale_table_min = 30;			// start scale
	int scale_table_max = 150;			// final scale
	float scale_table_meters = 0.35;	// start meters
	float scale_table_steps = 0.02;		// increased by

	trap->SendServerCommand(ent->s.number, "print \""
			"^9++============================++\n"
			"^9||^3 Scale ^9||^3 Meters ^9||^3 Feet    ^9||\n"
			"^9++============================++\n"
			"^7\"");

	for (i = scale_table_min; i <= scale_table_max; i++)
	{
		float scale_table_length = (100 * scale_table_meters / 2.54);
		float scale_table_feet = floor(scale_table_length / 12);
		float scale_table_inch = (scale_table_length - 12 * scale_table_feet);

		trap->SendServerCommand(ent->s.number, va("print \"^9||^3 %-5i ^9||^3 %-6.2f ^9||^3 %1.0f' %2.0f'' ^9||\n\"", i, scale_table_meters, scale_table_feet, scale_table_inch));
		if (i == 56 || i == 112) trap->SendServerCommand(ent->s.number, "print \"^9++============================++\n\"");
		scale_table_meters += scale_table_steps;
	}

	trap->SendServerCommand(ent->s.number, "print \"^9++============================++\n\"");
	return;
}

extern void update_current_character_scale(gentity_t* ent, sqlite3* db);
void Cmd_Scale_f( gentity_t *ent ) {
	char arg1[MAX_TOKEN_CHARS] = {0};
	char arg2[MAX_TOKEN_CHARS] = {0};
	int client_id = -1;
	int new_size = 0;

	if (level.gametype != GT_FFA && zyk_allow_adm_in_other_gametypes.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Scale command not allowed in gametypes other than FFA.\n\"" );
		return;
	}

	if (trap->Argc() == 2) {
		trap->Argv(1, arg1, sizeof(arg1));

		if (strcmp(arg1, "help") == 0) {
			display_scale_help(ent);
			return;
		}
		else {
			trap->SendServerCommand(ent - g_entities, "print \"Usage: /scale <playername/help> <size between 20 and 500 (optional, default is 100)>.\n\"");
			return;
		}

	}

	if (trap->Argc() != 3)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Usage: /scale <playername/help> <size between 20 and 500 (optional, default is 100)>.\n\"");
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	trap->Argv( 2, arg2, sizeof( arg2 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	//only ask for admin permissions when scaling someone else
	if (g_entities[client_id].client->pers.netname != ent->client->pers.netname) {

		if (!check_admin_command(ent, ADM_SCALE, qtrue))
		{
			return;
		}
	}

	new_size = atoi(arg2);

	if (new_size < 20 || new_size > 500)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Size must be between 20 and 500.\n\"" );
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode == 2 && g_entities[client_id].client->pers.can_play_quest == 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot scale players in quests.\n\"" );
		return;
	}

	// GalaxyRP fix: [Guardian] a guardian_mode>0 guard blocking scaling players in boss battles used to
	// be here. guardian_mode is permanently 0 now, so it was unreachable.

	if (ent != &g_entities[client_id] && g_entities[client_id].client->sess.amrpgmode > 0 &&
		g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << 13)))
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
		return;
	}

	do_scale(&g_entities[client_id], new_size);

	// GalaxyRP fix: [Database] this used to save with `ent` (the admin issuing the command) instead of
	// the actual target -- so scaling someone else applied the new size live but persisted the admin's
	// own (unchanged) scale to the admin's own row, silently discarding the target's new scale. Their
	// next /login then reloaded ModelScale from the DB and reset them back to whatever was last actually
	// saved (100 by default), even though they still looked correctly resized until then. Save to the
	// real target instead, and only when they're logged in -- a connected-but-not-logged-in target has
	// no character row to save to (CharID 0, same reasoning as the /giveitem login check).
	if (g_entities[client_id].client->sess.loggedin == qtrue)
	{
		sqlite3* db;
		int rc;

		rc = RP_DB_Open(&db);
		if (rc != SQLITE_OK)
		{
			trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			return;
		}

		update_current_character_scale(&g_entities[client_id], db);

		sqlite3_close(db);
	}

	trap->SendServerCommand( -1, va("print \"Scaled player %s ^7to ^3%d^7\n\"", g_entities[client_id].client->pers.netname, new_size) );
}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f( gentity_t *ent ) {
	char *msg = NULL;

	if (!check_admin_command(ent, ADM_GOD, qtrue))
	{
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE)) {
		trap->SendServerCommand(-1, va("chat \"^7%s ^7turned god mode ^1OFF\n\"", ent->client->pers.netname));
		msg = "godmode OFF";
	}
	else {
		trap->SendServerCommand(-1, va("chat \"^7%s ^7turned god mode ^2ON\n\"", ent->client->pers.netname));
		msg = "godmode ON";
	}
		
	trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", msg ) );
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f( gentity_t *ent ) {
	char *msg = NULL;

	ent->flags ^= FL_NOTARGET;
	if ( !(ent->flags & FL_NOTARGET) )
		msg = "^1OFF";
	else
		msg = "^2ON";

	trap->SendServerCommand( ent-g_entities, va( "print \"notarget %s\n\"", msg ) );
	trap->SendServerCommand(-1, va("chat \"^7%s ^7turned notarget %s\n\"", ent->client->pers.netname, msg));
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f( gentity_t *ent ) {
	char *msg = NULL;

	if (!check_admin_command(ent, ADM_NOCLIP, qtrue))
	{
		return;
	}

	if (g_gametype.integer != GT_FFA && zyk_allow_adm_in_other_gametypes.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Noclip command not allowed in gametypes other than FFA.\n\"" );
		return;
	}

	if (ent->client->pers.player_statuses & (1 << 6)) {
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot noClip while downed!\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^1You cannot noClip while downed!\n\"");

		// GalaxyRP fix: [macOS/Clang build failure] Cmd_Noclip_f is declared void -- every other
		// early-out in this function correctly uses a bare "return;"; this one alone returned a
		// value, which Clang rejects as a hard error (-Wreturn-mismatch) on macOS.
		return;
	}

	if (ent->client->ps.eFlags2 & EF2_HELD_BY_MONSTER)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot noclip while being eaten by a rancor.\n\"" );
		return;
	}

	// zyk: deactivating saber
	if ( ent->client->ps.saberHolstered < 2 )
	{
		Cmd_ToggleSaber_f(ent);
	}

	ent->client->noclip = !ent->client->noclip;
	if ( !ent->client->noclip )
		msg = "noclip OFF";
	else
		msg = "noclip ON";

	trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", msg ) );
}


/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f( gentity_t *ent )
{
	if ( !ent->client->pers.localClient )
	{
		trap->SendServerCommand(ent-g_entities, "print \"The levelshot command must be executed by a local client\n\"");
		return;
	}

	// doesn't work in single player
	if ( level.gametype == GT_SINGLE_PLAYER )
	{
		trap->SendServerCommand(ent-g_entities, "print \"Must not be in singleplayer mode for levelshot\n\"" );
		return;
	}

	BeginIntermission();
	trap->SendServerCommand( ent-g_entities, "clientLevelShot" );
}

#if 0
/*
==================
Cmd_TeamTask_f

From TA.
==================
*/
void Cmd_TeamTask_f( gentity_t *ent ) {
	char userinfo[MAX_INFO_STRING];
	char		arg[MAX_TOKEN_CHARS];
	int task;
	int client = ent->client - level.clients;

	if ( trap->Argc() != 2 ) {
		return;
	}
	trap->Argv( 1, arg, sizeof( arg ) );
	task = atoi( arg );

	trap->GetUserinfo(client, userinfo, sizeof(userinfo));
	Info_SetValueForKey(userinfo, "teamtask", va("%d", task));
	trap->SetUserinfo(client, userinfo);
	ClientUserinfoChanged(client);
}
#endif

void G_Kill( gentity_t *ent ) {
	if ((level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL) &&
		level.numPlayingClients > 1 && !level.warmupTime)
	{
		if (!g_allowDuelSuicide.integer)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "ATTEMPTDUELKILL")) );
			return;
		}
	}

	// zyk: target has been paralyzed by an admin
	if (ent && ent->client && !ent->NPC && ent->client->pers.player_statuses & (1 << 6))
		return;

	ent->flags &= ~FL_GODMODE;
	ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;

	if (ent->client->ps.duelInProgress == qtrue)
	{ // zyk: if player is in a private duel, gives kill to the other duelist
		gentity_t *other = &g_entities[ent->client->ps.duelIndex];

		player_die(ent, other, other, 100000, MOD_SUICIDE);
	}
	else if (ent->client->ps.otherKillerTime > level.time && ent->client->ps.otherKiller != ENTITYNUM_NONE)
	{ // zyk: self killing while otherKiller is set gives kill to the otherKiller
		gentity_t *other = &g_entities[ent->client->ps.otherKiller];

		player_die(ent, other, other, 100000, MOD_SUICIDE);
	}
	else
	{
		player_die(ent, ent, ent, 100000, MOD_SUICIDE);
	}
}

void paralyze_player(int client_id) {
	if (client_id == -1)
	{
		return;
	}

	if (!(g_entities[client_id].flags & FL_NOTARGET)) {
		g_entities[client_id].flags ^= FL_NOTARGET;
	}

	//GalaxyRP (Alex): [Death System] Paralyze the target player.
	g_entities[client_id].client->pers.player_statuses |= (1 << 6);

	g_entities[client_id].client->invulnerableTimer = level.time + 3000;
	g_entities[client_id].client->ps.eFlags |= EF_INVULNERABLE;

	g_entities[client_id].client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
	g_entities[client_id].client->ps.forceHandExtendTime = level.time + 500;
	g_entities[client_id].client->ps.velocity[2] += 150;
	g_entities[client_id].client->ps.forceDodgeAnim = 0;
	g_entities[client_id].client->ps.quickerGetup = qtrue;

	//GalaxyRP (Alex): [Death System] Set their HP to 50 so they don't die the old way instantly.
	g_entities[client_id].client->ps.stats[STAT_HEALTH] = 50;
	g_entities[client_id].health = 50;

	g_entities[client_id].client->ps.persistant[PERS_KILLED]++;
}

qboolean can_player_get_up(gentity_t* ent, gentity_t* target) {
	
	if (!(target->client->pers.player_statuses & (1 << 6))) {
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot help them because they're not downed!\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^1You cannot help them because they're not downed!\n\"");

		return qfalse;
	}

	//GalaxyRP (Alex): [Death System] Ent has permission, they can revive anyone.
	if (check_admin_command(ent, ADM_GETUP, qfalse)) {
		trap->SendServerCommand(ent - g_entities, va("cp \"^2You helped %s up.\"", target->client->pers.netname));
		trap->SendServerCommand(ent - g_entities, va("print \"^2You helped %s up.\"", target->client->pers.netname));

		return qtrue;
	}

	//GalaxyRP (Alex): [Death System] Ent and target are the same, player tries to get up by themselves.
	if (ent->client->ps.clientNum == target->client->ps.clientNum) {
		//GalaxyRP (Alex): [Death System] If player's timer is done, allow them to get up.
		if (ent->client->downedTime == 0) {
			trap->SendServerCommand(ent - g_entities, "print \"^2You got up!\n\"");
			trap->SendServerCommand(ent - g_entities, "cp \"^2You got up!\n\"");
			return qtrue;
		}
		else {
			trap->SendServerCommand(ent - g_entities, "print \"^1You cannot get up until the timer is finished!\n\"");
			trap->SendServerCommand(ent - g_entities, "cp \"^1You cannot get up until the timer is finished!\n\"");
			return qfalse;
		}
	}
	//GalaxyRP (Alex): [Death System] Ent and target are different.
	else {
		//GalaxyRP (Alex): [Death System] Can't help someone else get up if you're also down.
		if (ent->client->pers.player_statuses & (1 << 6)) {
			trap->SendServerCommand(ent - g_entities, "print \"^1You cannot help someone else while you're downed!\n\"");
			trap->SendServerCommand(ent - g_entities, "cp \"^1You cannot help someone else while you're downed!\n\"");

			return qfalse;
		}

		// GalaxyRP fix: [Death System] this distance check used to live in unreachable code after
		// this whole if/else -- every branch here already returned before it could ever run, so
		// /helpup had no range requirement at all and could revive someone from anywhere on the
		// map. The admin bypass is already handled by the check_admin_command() return near the top
		// of this function, so only the distance gate needs to apply here.
		if (Distance(ent->client->ps.origin, target->client->ps.origin) > 65) {
			trap->SendServerCommand(ent - g_entities, va("print \"^1You are too far away to help %s up!\n\"", target->client->pers.netname));
			trap->SendServerCommand(ent - g_entities, va("cp \"^1You are too far away to help %s up!\"", target->client->pers.netname));

			return qfalse;
		}

		trap->SendServerCommand(ent - g_entities, va("cp \"^2You helped %s up.\"", target->client->pers.netname));
		trap->SendServerCommand(ent - g_entities, va("print \"^2You helped %s up.\"", target->client->pers.netname));
		trap->SendServerCommand(target->client->ps.clientNum, va("cp \"^2 %s helped you up!.\"", ent->client->pers.netname));
		trap->SendServerCommand(target->client->ps.clientNum, va("print \"^2 %s helped you up!.\"", ent->client->pers.netname));

		return qtrue;
	}
}

void help_up(gentity_t* ent, gentity_t* target) {

	if (can_player_get_up(ent, target)) {
		//GalaxyRP (Alex): [Death System] No longer paralyzed.
		
		target->client->pers.player_statuses &= ~(1 << 6);

		if (target->flags & FL_NOTARGET) {
			target->flags ^= FL_NOTARGET;
		}

		if (ent != target) {
			play_animation(ent, BOTH_HELPUP, 1000);
			play_animation(target, BOTH_HELPEDUP, 1000);
		}
		else {
			int anim_to_play;
			switch (ent->client->pers.skill_levels[0])
			{
			case FORCE_LEVEL_0:
				anim_to_play = BOTH_GETUP1;
				break;
			case FORCE_LEVEL_1:
			case FORCE_LEVEL_2:
			case FORCE_LEVEL_3:
			case FORCE_LEVEL_4:
			case FORCE_LEVEL_5:
				// GalaxyRP fix: [logic] missing break -- without it, every force-sensitive
				// skill level fell through into default and got BOTH_GETUP1 anyway, so the
				// back-flip get-up animation could never actually play.
				anim_to_play = BOTH_BACK_FLIP_UP;
				break;
			default:
				anim_to_play = BOTH_GETUP1;
				break;
			}
			play_animation(target, anim_to_play, 1000);
		}


		if (rp_downed_invulnerability_timer.integer) {
			target->client->invulnerableTimer = level.time + rp_downed_invulnerability_timer.integer * 1000;
			target->client->ps.eFlags |= EF_INVULNERABLE;
		}
	}

	return;
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f( gentity_t *ent ) {
	G_Kill(ent);
}

void Cmd_Helpup_f(gentity_t* ent) {
	char targetIndex[MAX_TOKEN_CHARS];

	if (trap->Argc() < 2) {
		trap->SendServerCommand(ent - g_entities, "print \"Usage: /helpup <player name or ID>\n\"");
		return;
	}

	trap->Argv(1, targetIndex, sizeof(targetIndex));
	int i = ClientNumberFromString(ent, targetIndex, qfalse);
	if (i == -1) {
		return;
	}

	gentity_t* target;

	target = &g_entities[i];

	help_up(ent, target);
	
	return;
}

void Cmd_Getup_f(gentity_t* ent) {
	// GalaxyRP fix: [cleanup] removed an unused 'char otherindex[MAX_TOKEN_CHARS]' local (never
	// read anywhere in this function).
	if (trap->Argc() < 1) {
		trap->SendServerCommand(ent - g_entities, "print \"Usage: /getup\n\"");
		return;
	}

	//GalaxyRP (Alex): [Death System] If player's timer is done or he is an admin, allow them to get up.
	if (ent->client->downedTime == 0 || check_admin_command(ent, ADM_GETUP, qfalse)) {
		help_up(ent, ent);
		return;
	}

	return;
}

/*
=================
ACCOUNT AREA
=================
*/

/*
=================
HELPER METHODS for interacting with the database

The methods in the area are used to split up database actions into multiple helper methods, that can be used in multiple areas without code dupplication.
=================
*/

// GalaxyRP (Alex): [Database] This method forces a model onto a player.
void set_model(gentity_t* ent, char modelName[MAX_STRING_CHARS])
{
	char userinfo[MAX_INFO_STRING];
	int clientNum = ClientNumberFromString(ent, ent->client->pers.netname, qfalse);

	trap->GetUserinfo(clientNum, userinfo, sizeof(userinfo));

	//Alex: this is how u get the current model
	//Q_strncpyz(modelname, Info_ValueForKey(userinfo, "model"), sizeof(modelname));
	Info_SetValueForKey(userinfo, "model", modelName);
	trap->SetUserinfo(clientNum, userinfo);
	ClientUserinfoChanged(clientNum);

	// GalaxyRP fix: [Model] trap->SetUserinfo above only updates the SERVER's cached copy of this
	// client's userinfo -- "model" is a CVAR_USERINFO cvar whose real, authoritative value lives
	// on the CLIENT, so nothing here told the client itself to update its own local cvar. See
	// CG_ModelUpdate_f in cg_servercmds.c for the full explanation (same root cause, and same fix,
	// as CG_SaberUpdate_f). Nothing between trap->SetUserinfo and here validates or rewrites the
	// "model" key, so modelName is still the exact value the client needs pushed.
	trap->SendServerCommand(clientNum, va("supdatemodel \"%s\"\n", modelName));

	return;
}

// GalaxyRP (Alex): [Database] This method forces a netname onto a player.
void set_netname(gentity_t* ent, char netName[MAX_STRING_CHARS])
{
	char userinfo[MAX_INFO_STRING];
	int clientNum = ClientNumberFromString(ent, ent->client->pers.netname, qfalse);

	//Alex: set display name
	Q_strncpyz(ent->client->pers.netname, netName, sizeof(ent->client->pers.netname));
	Q_strncpyz(ent->client->pers.netname_nocolor, netName, sizeof(ent->client->pers.netname_nocolor));
	Q_StripColor(ent->client->pers.netname_nocolor);
	trap->GetUserinfo(clientNum, userinfo, sizeof(userinfo));
	Info_SetValueForKey(userinfo, "name", netName);
	trap->SetUserinfo(clientNum, userinfo);
	ClientUserinfoChanged(clientNum);

	// GalaxyRP fix: [Name] same root cause as the "model" push above -- trap->SetUserinfo only
	// updates the SERVER's cached copy, never the client's own local "name" cvar. See
	// CG_NameUpdate_f in cg_servercmds.c. Pushing ent->client->pers.netname rather than the raw
	// netName parameter, since ClientUserinfoChanged() above runs the name through
	// ClientCleanName() (length/character/color sanitizing) before settling on the final value --
	// pers.netname is that final, authoritative result, same principle as update_saber() pushing
	// pers.saber1/2 rather than the raw requested saber.
	trap->SendServerCommand(clientNum, va("supdatename \"%s\"\n", ent->client->pers.netname));

	return;
}

// GalaxyRP (Alex): [Database] This generic method is strictly for running INSERT, UPDATE, DROP and CREATE statements.
void run_db_query(char* query, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt){
	//trap->Print(query);
	rc = sqlite3_exec(db, query, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return;
	}
}

/*
----ITEMS TABLE----
*/

// GalaxyRP (Alex): [Database] INSERT This method inserts a new item row in the database.
void insert_inv_table_row(gentity_t* ent, char* item_to_add, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	// GalaxyRP fix: [security] this used to go through run_db_query() with the item name spliced
	// straight into the INSERT text via va("...%s..."). Currently unreachable (no command calls this
	// function today), but fixed for consistency/safety with the rest of the DB layer in case it's
	// wired up later.
	rc = sqlite3_prepare(db, "INSERT INTO Items(CharID, ItemName) VALUES(?, ?)", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->pers.CharID);
	sqlite3_bind_text(stmt, 2, item_to_add, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	trap->SendServerCommand(ent->s.number, "print \"Item added to your inventory.\n\"");

	sqlite3_finalize(stmt);

	return;
}


/*
----ACCOUNTS TABLE----
*/

// GalaxyRP (Alex): [Database] SELECT This method selects a row form the accounts table, and assigns the values to the entity.
void select_accounts_table_row(gentity_t* ent, char* username, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	// GalaxyRP fix: [security] this used to build the query text via va("...Username='%s'", username)
	// -- splicing the raw username straight into the SQL string. A username containing a single quote
	// (the SQL string delimiter) could break out of the literal and inject arbitrary SQL, executed
	// with this game server's full database privileges -- reachable via /new (Cmd_Register_F). Bind
	// the value as a parameter instead, so it can never be interpreted as SQL syntax.
	rc = sqlite3_prepare(db, "SELECT AccountID, PlayerSettings, AdminLevel FROM Accounts WHERE Username=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	if (rc == SQLITE_ROW)
	{
		ent->client->sess.accountID = sqlite3_column_int(stmt, 0);
		ent->client->pers.player_settings = sqlite3_column_int(stmt, 1);
		ent->client->pers.bitvalue = sqlite3_column_int(stmt, 2);
		sqlite3_finalize(stmt);
	}

	return;
}

// GalaxyRP (Alex): [Database] SELECT This method selects the id of an account going by the username provided. Usernames should be unique.
int select_account_id_from_username(gentity_t* ent, char* username, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	int accountID;

	// GalaxyRP fix: [security] same va("...%s...")-into-SQL-text issue as select_accounts_table_row()
	// above -- bind username as a parameter instead of splicing it into the query text.
	rc = sqlite3_prepare(db, "SELECT AccountID FROM Accounts WHERE Username=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return -1;
	}
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return -1;
	}
	if (rc == SQLITE_ROW)
	{
		accountID = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}

	return accountID;
}

// GalaxyRP (Alex): [Database] INSERT This method inserts a new row into the accounts table, using the username and password provided, and default values for everything else.
void insert_accounts_table_row(gentity_t* ent, char* username, char* password, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	// GalaxyRP fix: [security] this used to go through run_db_query(), which executes a fully
	// pre-formatted SQL string via sqlite3_exec() with no parameter binding -- username and password
	// (the two values a player directly controls via /new) were spliced straight into the INSERT
	// text via va("...%s...%s..."). A quote in either value could inject arbitrary SQL. Prepare/bind/
	// step directly instead so the values can never be interpreted as SQL syntax.
	// GalaxyRP fix: [Database] PlayerSettings kept as an explicit literal 0 here rather than bound --
	// a first pass at this fix bound ent->client->pers.player_settings instead, on the reasoning that
	// "a brand new account's player_settings is always still 0 at this point". That's wrong: /new
	// (this function's caller) has no guard requiring the connection be logged out first, and
	// /logout doesn't clear pers.player_settings the way it already clears pers.bitvalue -- so a
	// player who is logged into account A, runs /logout, then /new B, still has account A's settings
	// bitmask sitting in pers.player_settings when this INSERT runs, and binding it would seed the
	// brand new account B with a stranger's (well, their own other account's) settings. A new account
	// has no settings history to inherit; 0 is unconditionally correct here regardless of what this
	// connection was doing before. See the UPDATE sites below (update_accounts_table_row_with_current_values()
	// and update_current_character_and_account()) for the actual PlayerSettings-never-saved bug --
	// those bind the real value because they update an EXISTING row for the CURRENTLY logged-in
	// player, where pers.player_settings is guaranteed current.
	rc = sqlite3_prepare(db, "INSERT INTO Accounts(Username, Password, AdminLevel, PlayerSettings, DefaultChar) VALUES(?, ?, ?, '0', ?)", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, password, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, rp_default_account_permissions.integer);
	sqlite3_bind_text(stmt, 4, username, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	return;
}

// GalaxyRP (Alex): [Database] SELECT This method returns the number of accounts with one specific username. Useful for checking if a username is unique.
int select_number_of_accounts_with_username(gentity_t* ent, char* username, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	int numberOfAccounts = 0;

	// GalaxyRP fix: [security] this used to build the query text via va("...Username='%s'", username)
	// -- i.e. splicing the raw username straight into the SQL string. A username containing a single
	// quote (the SQL string delimiter) could break out of the literal and inject arbitrary SQL (e.g.
	// `' OR '1'='1`), executed with this game server's full database privileges. This is the very
	// first query both /login and /new run against attacker-supplied input, so it's reachable
	// unauthenticated. Bind the value as a parameter instead, so it can never be interpreted as SQL
	// syntax regardless of content.
	rc = sqlite3_prepare(db, "SELECT count(Username) FROM Accounts WHERE Username=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 1;
	}
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 1;
	}
	if (rc == SQLITE_ROW)
	{

		numberOfAccounts = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);

		return numberOfAccounts;

	}

	return 1;
}

// GalaxyRP (Alex): [Database] UPDATE This method updates an accounts table row with information contained within the entity with which it's called.
void update_accounts_table_row_with_current_values(gentity_t* ent) {
	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	sqlite3_stmt* stmt = 0;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}
	
	// GalaxyRP fix: [security] this used to go through run_db_query() with the password (and
	// character name) spliced straight into the UPDATE text via va("...%s..."). This is the query
	// /changepassword ultimately writes through, so a password containing a quote could inject
	// arbitrary SQL. Bind every value as a parameter instead.
	// GalaxyRP fix: [Database] PlayerSettings was hardcoded as the literal '0' instead of being
	// bound, on every single write to this table -- since this function is what /settings' toggle
	// commands ultimately save through (via save_account(ent, qfalse)), every settings change was
	// silently reset back to 0 in the DB on the very next save, and came back off on the player's
	// next login. Bind the player's actual in-memory bitmask instead.
	rc = sqlite3_prepare(db, "UPDATE Accounts SET PlayerSettings=?, AdminLevel=?, DefaultChar=?, Password=? WHERE AccountID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->pers.player_settings);
	sqlite3_bind_int(stmt, 2, ent->client->pers.bitvalue);
	sqlite3_bind_text(stmt, 3, ent->client->sess.rpgchar, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, ent->client->pers.password, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 5, ent->client->sess.accountID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	sqlite3_close(db);

	return;
}

// GalaxyRP (Alex): [Database] UPDATE This method updated a accounts table row with information contained within the entity with which it's called. (NEEDS A CHAR NAME AND FOR THE USER TO BE LOGGED IN)
void update_accounts_table_row_with_default_char(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	// GalaxyRP fix: [security] same run_db_query()-with-va()-splice issue as
	// update_accounts_table_row_with_current_values() above -- bind character_name instead. Reachable
	// via /new (Cmd_Register_F -> select_player_character -> here) with the new account's own
	// username used as the character name.
	rc = sqlite3_prepare(db, "UPDATE Accounts SET DefaultChar=? WHERE AccountID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_text(stmt, 1, character_name, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, ent->client->sess.accountID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	return;
}

qboolean is_password_correct(gentity_t* ent, char* username, char* password, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	// GalaxyRP fix: [security] same va("...%s...")-into-SQL-text issue as
	// select_number_of_accounts_with_username() above -- bind username instead. Reachable via /login
	// with the account's own password compared afterward in C via strcmp(), never itself placed into
	// SQL text here.
	rc = sqlite3_prepare(db, "SELECT Password FROM Accounts WHERE Username=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return qfalse;
	}
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return qfalse;
	}
	if (rc == SQLITE_ROW)
	{
		char comparisonPassword[MAX_STRING_CHARS];
		Q_strncpyz(comparisonPassword, (const char*)sqlite3_column_text(stmt, 0), sizeof(comparisonPassword));
		sqlite3_finalize(stmt);

		if (strcmp(password, comparisonPassword) == 0) {
			return qtrue;
		}

		return qfalse;
	}

	return qfalse;
}

/*
----CHARACTERS TABLE----
*/

void update_credits_value(gentity_t* ent) {
	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	sqlite3_stmt* stmt = 0;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	char update_char_query[148] = "UPDATE Characters SET Credits='%i' WHERE CharID='%i'";
	run_db_query(va(update_char_query,
		ent->client->pers.credits,
		ent->client->pers.CharID
	), db, zErrMsg, rc, stmt);

	sqlite3_close(db);

	return;
}

// GalaxyRP (Alex): [Database] INSERT This method inserts a new row in the character table, with default values. ASSUMES PLAYER IS ALREADY LOGGED IN.
void insert_chars_table_row(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	// GalaxyRP fix: [security] this used to go through run_db_query() with the character name
	// spliced straight into the INSERT text via va("...%s..."). Reachable via /new, using the new
	// account's own username as its first character's name. Prepare/bind/step directly instead.
	rc = sqlite3_prepare(db, "INSERT INTO Characters(AccountID, Credits, Level, ModelScale, Name, SkillPoints, Description, NetName, ModelName, xp) VALUES(?, '100', '1', '100', ?, '1', 'Nothing to show.', 'DefaultName', 'kyle', 0)", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->sess.accountID);
	sqlite3_bind_text(stmt, 2, character_name, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	return;
}

// GalaxyRP (Alex): [Database] SELECT This method returns the number of characters that exist with one name. (Useful for preventing duplicates)
int select_number_of_characters_with_name(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {

	// GalaxyRP fix: [security] this used to build the query text via va("...Name='%s'", character_name)
	// -- splicing the raw character name straight into the SQL string. Reachable via /new
	// (Cmd_Register_F, using the new account's own username as its first character's name) and via
	// /char new <name>. Bind the value as a parameter instead.
	rc = sqlite3_prepare(db, "SELECT count(CharID) FROM Characters WHERE AccountID=? AND Name=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 0;
	}
	sqlite3_bind_int(stmt, 1, ent->client->sess.accountID);
	sqlite3_bind_text(stmt, 2, character_name, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 0;
	}
	if (rc == SQLITE_ROW)
	{
		int numberOfChars;

		numberOfChars = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);

		return numberOfChars;

	}

	return 0;
}

// GalaxyRP (Alex): [Database] SELECT This method returns the character ID associated with the character name given, AND which belongs to the account the player is currently logged in with.
int select_char_id_using_char_name(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	int charID = -1;

	// GalaxyRP fix: [security] this used to build the query text via va("...Name='%s'", character_name)
	// -- splicing the raw character name straight into the SQL string. Reachable via /char remove
	// <name>. Bind the value as a parameter instead.
	rc = sqlite3_prepare(db, "SELECT CharID FROM Characters WHERE AccountID=? AND Name=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
	sqlite3_bind_int(stmt, 1, ent->client->sess.accountID);
	sqlite3_bind_text(stmt, 2, character_name, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
	if (rc == SQLITE_ROW)
	{
		charID = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}

	return charID;
}

// GalaxyRP (Alex): [Database] SELECT This method returns the character ID associated with the character name given, AND which belongs to the account the player is currently logged in with.
saber_db_info_t select_saber_info_using_char_id(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	char saber1Model[50] = "";
	char saber2Model[50] = "";
	saber_db_info_t saber_info = {"none", "none"};

	// GalaxyRP fix: [security] this used to also SELECT saberOneColor/saberTwoColor and strcpy() the
	// results into two-byte stack buffers -- safe only for as long as those columns held nothing but
	// the untouched single-digit schema default. They now hold real packed colour values (up to eight
	// digits, see g_local.h), which would have overflowed both buffers on every call. Nothing in this
	// function ever read either value -- it returns saber models only -- so the columns are simply no
	// longer selected; the blade colours are restored by select_player_character() and
	// select_account_and_default_character_data(), which read them into pers.saberRGB[] properly.
	rc = sqlite3_prepare(db, va("SELECT saberOneModel, saberTwoModel FROM Characters WHERE CharID='%i'", ent->client->pers.CharID), -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		// GalaxyRP fix: this is a non-void function (returns saber_db_info_t) -- a bare "return;"
		// here is undefined behaviour (the caller reads whatever garbage was on the stack for the
		// return value) instead of falling back to the safe "none"/"none" default declared above.
		return saber_info;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return saber_info; // GalaxyRP fix: same as above -- return the safe default, not garbage.
	}
	if (rc == SQLITE_ROW)
	{

		// GalaxyRP fix: [security] bound both copies to their destination the same way the two
		// character-load paths already do -- a saber-model name of 50+ characters in the database
		// would otherwise overflow these stack buffers.
		Q_strncpyz(saber1Model, sqlite3_column_text(stmt, 0), sizeof(saber1Model));
		Q_strncpyz(saber2Model, sqlite3_column_text(stmt, 1), sizeof(saber2Model));

		sqlite3_finalize(stmt);
	}

	strcpy(saber_info.saber1Model, saber1Model);
	strcpy(saber_info.saber2Model, saber2Model);

	return saber_info;
}

// GalaxyRP (Alex): [Database] UPDATE This method updated a characters table row with information contained within the entity with which it's called.
void update_chars_table_row_with_current_values(gentity_t* ent) {
	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	sqlite3_stmt* stmt = 0;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}


	// GalaxyRP (Alex): [Database] Grab the model and display name, so they can be saved in the database.
	char userinfo[MAX_INFO_STRING], modelName[MAX_INFO_STRING];
	int clientNum = ClientNumberFromString(ent, ent->client->pers.netname, qfalse);

	trap->GetUserinfo(clientNum, userinfo, sizeof(userinfo));
	Q_strncpyz(modelName, Info_ValueForKey(userinfo, "model"), sizeof(modelName));


	// GalaxyRP fix: [security] this used to go through run_db_query() with the player's description
	// and netname (both freely player-editable text -- e.g. via /desc) spliced straight into the
	// UPDATE text via va("...\"%s\"..."). This function is called on every credits/level/skillpoint
	// change (5 call sites), so it's a frequently-reachable, unauthenticated injection point. Bind
	// every value as a parameter instead.
	rc = sqlite3_prepare(db, "UPDATE Characters SET Credits=?, Level=?, ModelScale=?, Skillpoints=?, Description=?, NetName=?, ModelName=?, xp=? WHERE CharID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->pers.credits);
	sqlite3_bind_int(stmt, 2, ent->client->pers.level);
	sqlite3_bind_int(stmt, 3, ent->client->ps.iModelScale);
	sqlite3_bind_int(stmt, 4, ent->client->pers.skillpoints);
	sqlite3_bind_text(stmt, 5, ent->client->pers.description, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, ent->client->pers.netname, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 7, modelName, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 8, ent->client->pers.xp);
	sqlite3_bind_int(stmt, 9, ent->client->pers.CharID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	sqlite3_close(db);

	return;
}

// GalaxyRP (Alex): [Database] DELETE This method deletes a characters table row which is associated with the ID given.
void delete_chars_table_row_with_id(gentity_t* ent, int id, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {

	char delete_char_query[41] = "DELETE FROM Characters WHERE CharID='%i'";

	run_db_query(va(delete_char_query, id), db, zErrMsg, rc, stmt);

	return;
}

// GalaxyRP (Alex): [Database] DELETE This method deletes a characters table row which is associated with the name given.
void delete_chars_table_row_with_name(gentity_t* ent, char* charName, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {

	// GalaxyRP fix: [security] this used to go through run_db_query() with the character name
	// spliced straight into the DELETE text via va("...%s..."). Currently unreachable (no command
	// calls this function today -- /char remove goes through remove_character() instead, which
	// deletes by CharID), but fixed for consistency/safety with the rest of the DB layer in case
	// it's wired up later.
	rc = sqlite3_prepare(db, "DELETE FROM Characters WHERE Name=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_text(stmt, 1, charName, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	return;
}

/*
----SKILLS TABLE----
*/

// GalaxyRP (Alex): [Database] INSERT This method inserts a new row in the skills table, with default values.
void insert_skills_table_row(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	char insert_new_entry_to_skills_table[919] = "INSERT INTO Skills(Jump, Push, Pull, Speed, Sense, SaberAttack, SaberDefense, SaberThrow, Absorb, Heal, Protect, MindTrick, TeamHeal, Lightning, Grip, Drain, Rage, TeamEnergize, StunBaton, BlasterPistol, BlasterRifle, Disruptor, Bowcaster, Repeater, DEMP2, Flechette, RocketLauncher, ConcussionRifle, BryarPistol, Melee, MaxShield, ShieldStrength, HealthStrength, DrainShield, Jetpack, SenseHealth, ShieldHeal, TeamShieldHeal, UniqueSkill, BlasterPack, PowerCell, MetalBolts, Rockets, Thermals, TripMines, Detpacks, Binoculars, BactaCanister, SentryGun, SeekerDrone, Eweb, BigBacta, ForceField, CloakItem, ForcePower, Improvements) VALUES('0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0')";
	run_db_query(insert_new_entry_to_skills_table, db, zErrMsg, rc, stmt);

	return;
}

// GalaxyRP (Alex): [Database] UPDATE This method updated a skills table row with information contained within the entity with which it's called. Also updates the skillpoint values, since there's no instance where a skill is updated and the skillpoints are not.
void update_skills_table_row_with_current_values(gentity_t* ent) {
	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	sqlite3_stmt* stmt = 0;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}
	
	char update_skills_query[1054] = "UPDATE Skills SET Jump='%i', Push='%i', Pull='%i', Speed='%i', Sense='%i', SaberAttack='%i', SaberDefense='%i', SaberThrow='%i', Absorb='%i', Heal='%i', Protect='%i', MindTrick='%i', TeamHeal='%i', Lightning='%i', Grip='%i', Drain='%i', Rage='%i', TeamEnergize='%i', StunBaton='%i', BlasterPistol='%i', BlasterRifle='%i', Disruptor='%i', Bowcaster='%i', Repeater='%i', DEMP2='%i', Flechette='%i', RocketLauncher='%i', ConcussionRifle='%i', BryarPistol='%i', Melee='%i', MaxShield='%i', ShieldStrength='%i', HealthStrength='%i', DrainShield='%i', Jetpack='%i', SenseHealth='%i', ShieldHeal='%i', TeamShieldHeal='%i', UniqueSkill='%i', BlasterPack='%i', PowerCell='%i', MetalBolts='%i', Rockets='%i', Thermals='%i', TripMines='%i', Detpacks='%i', Binoculars='%i', BactaCanister='%i', SentryGun='%i', SeekerDrone='%i', Eweb='%i', BigBacta='%i', ForceField='%i', CloakItem='%i', ForcePower='%i', Improvements='%i', Armor='%i', Flamethrower='%i', ShieldRegen='%i', HealthRegen='%i' WHERE CharID='%i'; UPDATE Characters SET SkillPoints='%i' WHERE CharID='%i';";
	run_db_query(va(update_skills_query,
		ent->client->pers.skill_levels[0],	//Jump
		ent->client->pers.skill_levels[1],	//Push
		ent->client->pers.skill_levels[2],	//Pull
		ent->client->pers.skill_levels[3],	//Speed
		ent->client->pers.skill_levels[4],	//Sense
		ent->client->pers.skill_levels[5],	//SaberAttack
		ent->client->pers.skill_levels[6],	//SaberDefense
		ent->client->pers.skill_levels[7],	//SaberThrow
		ent->client->pers.skill_levels[8],	//Absorb
		ent->client->pers.skill_levels[9],	//Heal
		ent->client->pers.skill_levels[10],	//Protect
		ent->client->pers.skill_levels[11],	//MindTrick
		ent->client->pers.skill_levels[12],	//TeamHeal
		ent->client->pers.skill_levels[13],	//Lightning
		ent->client->pers.skill_levels[14],	//Grip
		ent->client->pers.skill_levels[15],	//Drain
		ent->client->pers.skill_levels[16],	//Rage
		ent->client->pers.skill_levels[17],	//TeamEnergize
		ent->client->pers.skill_levels[18],	//StunBaton
		ent->client->pers.skill_levels[19],	//BlasterPistol
		ent->client->pers.skill_levels[20],	//BlasterRifle
		ent->client->pers.skill_levels[21],	//Disruptor
		ent->client->pers.skill_levels[22],	//Bowcaster
		ent->client->pers.skill_levels[23],	//Repeater
		ent->client->pers.skill_levels[24],	//DEMP2
		ent->client->pers.skill_levels[25],	//Flechette
		ent->client->pers.skill_levels[26],	//RocketLauncher
		ent->client->pers.skill_levels[27],	//ConcussionRifle
		ent->client->pers.skill_levels[28],	//BryarPistol
		ent->client->pers.skill_levels[29],	//Melee
		ent->client->pers.skill_levels[30],	//MaxShield
		ent->client->pers.skill_levels[31],	//ShieldStrength
		ent->client->pers.skill_levels[32],	//HealthStrength
		ent->client->pers.skill_levels[33],	//DrainShield
		ent->client->pers.skill_levels[34],	//Jetpack
		ent->client->pers.skill_levels[35],	//SenseHealth
		ent->client->pers.skill_levels[36],	//ShieldHeal
		ent->client->pers.skill_levels[37],	//TeamShieldHeal
		ent->client->pers.skill_levels[38],	//UniqueSkill
		ent->client->pers.skill_levels[39],	//BlasterPack
		ent->client->pers.skill_levels[40],	//PowerCell
		ent->client->pers.skill_levels[41],	//MetalBolts
		ent->client->pers.skill_levels[42],	//Rockets
		ent->client->pers.skill_levels[43],	//Thermals
		ent->client->pers.skill_levels[44],	//TripMines
		ent->client->pers.skill_levels[45],	//Detpacks
		ent->client->pers.skill_levels[46],	//Binoculars
		ent->client->pers.skill_levels[47],	//BactaCanister
		ent->client->pers.skill_levels[48],	//SentryGun
		ent->client->pers.skill_levels[49],	//SeekerDrone
		ent->client->pers.skill_levels[50],	//Eweb
		ent->client->pers.skill_levels[51],	//BigBacta
		ent->client->pers.skill_levels[52],	//ForceField
		ent->client->pers.skill_levels[53],	//CloakItem
		ent->client->pers.skill_levels[54],	//ForcePower
		ent->client->pers.skill_levels[55], //Improvements
		ent->client->pers.skill_levels[56], //Armor
		ent->client->pers.skill_levels[57], //Flamethrower
		ent->client->pers.skill_levels[58], //Shield Regen
		ent->client->pers.skill_levels[59], //Health Regen
		ent->client->pers.CharID,
		ent->client->pers.skillpoints,
		ent->client->pers.CharID), db, zErrMsg, rc, stmt);

	sqlite3_close(db);

	return;
}

// GalaxyRP (Alex): [Database] DELETE This method deletes a skills table row which is associated with the ID given.
void delete_skills_table_row_with_id(gentity_t* ent, int id, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {

	char delete_skills_query[41] = "DELETE FROM Skills WHERE CharID='%i'";

	run_db_query(va(delete_skills_query, id), db, zErrMsg, rc, stmt);

	return;
}

/*
----WEAPONS TABLE----
*/

// GalaxyRP (Alex): [Database] INSERT This method inserts a new row in the weapons table, with default values.
void insert_weapons_table_row(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	char insert_new_entry_to_weapons_table[159] = "INSERT INTO Weapons(AmmoBlaster, AmmoPowercell, AmmoMetalBolts, AmmoRockets, AmmoThermal, AmmoTripmine, AmmoDetpack) VALUES('0', '0', '0', '0', '0', '0', '0')";
	run_db_query(insert_new_entry_to_weapons_table, db, zErrMsg, rc, stmt);

	return;
}

// GalaxyRP (Alex): [Database] SELECT This method grabs all the values from a weapons table row (ASSUMES THE PLAYERS IS ALREADY LOGGED IN), and assigns them to the entity.
void select_weapons_table_row_from_entity(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	rc = sqlite3_prepare(db, va("SELECT * FROM Weapons WHERE CharID='%i'", ent->client->pers.CharID), -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	if (rc == SQLITE_ROW)
	{
		// GalaxyRP fix: [gameplay] this used to be a contiguous loop (ps.ammo[i] = column(i - 1))
		// that assumed the ammo_t enum maps 1:1 onto this table's 7 ammo columns. It doesn't --
		// AMMO_EMPLACED sits between AMMO_ROCKETS and AMMO_THERMAL in the enum but the Weapons
		// table has no AmmoEmplaced column at all (the save path already skips it), so every
		// column from AmmoThermal onward was landing one enum slot early, and the final iteration
		// (AMMO_DETPACK) read past the last real column. Read each column explicitly instead,
		// mirroring the explicit list the save path uses.
		ent->client->ps.ammo[AMMO_BLASTER] = sqlite3_column_int(stmt, 1);
		ent->client->ps.ammo[AMMO_POWERCELL] = sqlite3_column_int(stmt, 2);
		ent->client->ps.ammo[AMMO_METAL_BOLTS] = sqlite3_column_int(stmt, 3);
		ent->client->ps.ammo[AMMO_ROCKETS] = sqlite3_column_int(stmt, 4);
		ent->client->ps.ammo[AMMO_THERMAL] = sqlite3_column_int(stmt, 5);
		ent->client->ps.ammo[AMMO_TRIPMINE] = sqlite3_column_int(stmt, 6);
		ent->client->ps.ammo[AMMO_DETPACK] = sqlite3_column_int(stmt, 7);

		sqlite3_finalize(stmt);
		return;
	}

	return;
}

// GalaxyRP (Alex): [Database] UPDATE This method updated a weapons table row with information contained within the entity with which it's called.
void update_weapons_table_row_with_current_values(gentity_t* ent) {

	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	sqlite3_stmt* stmt = 0;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	char update_ammo_query[168] = "UPDATE Weapons SET AmmoBlaster='%i', AmmoPowercell='%i', AmmoMetalBolts='%i', AmmoRockets='%i', AmmoThermal='%i', AmmoTripmine='%i', AmmoDetpack='%i' WHERE CharID='%i'";
	run_db_query(va(update_ammo_query,
		ent->client->ps.ammo[AMMO_BLASTER],
		ent->client->ps.ammo[AMMO_POWERCELL],
		ent->client->ps.ammo[AMMO_METAL_BOLTS],
		ent->client->ps.ammo[AMMO_ROCKETS],
		ent->client->ps.ammo[AMMO_THERMAL],
		ent->client->ps.ammo[AMMO_TRIPMINE],
		ent->client->ps.ammo[AMMO_DETPACK],
		ent->client->pers.CharID
	), db, zErrMsg, rc, stmt);

	sqlite3_close(db);

	return;
}

// GalaxyRP (Alex): [Database] DELETE This method deletes a weapons table row which is associated with the ID given.
void delete_weapons_table_row_with_id(gentity_t* ent, int id, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {

	char delete_weapons_query[41] = "DELETE FROM Weapons WHERE CharID='%i'";

	run_db_query(va(delete_weapons_query, id), db, zErrMsg, rc, stmt);

	return;
}

/*
----NEWS TABLE----
*/

// GalaxyRP (Alex): [Database] INSERT This method inserts a new row in the news table, requires the channel and text be passed into it.
void insert_news_table_row(gentity_t* ent, char* channel, char* news_text) {
	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	sqlite3_stmt* stmt = 0;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	// GalaxyRP fix: [security] this used to go through run_db_query() with the channel name and news
	// text spliced straight into the INSERT text via va("...%s...%s..."). Reachable via /newsadd
	// (admin-gated, but a quote in either value would still let a malicious or compromised admin
	// account run arbitrary SQL against the database, well beyond what the news feature is meant to
	// allow). Prepare/bind/step directly instead.
	rc = sqlite3_prepare(db, "INSERT INTO News(channel, text) VALUES (?, ?)", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	sqlite3_bind_text(stmt, 1, channel, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, news_text, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	sqlite3_close(db);

	return;
}

// GalaxyRP (Alex): [Database] SELECT This method selects all the unique channels from the news table.
void select_news_channels(gentity_t* ent) {
	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	sqlite3_stmt* stmt = 0;
	char channelName[MAX_STRING_CHARS];

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	trap->SendServerCommand(ent - g_entities, "print \"^3Existing channels:\n\"");

	// GalaxyRP (Alex): [Database] Select all info from all character related tables.
	// GalaxyRP fix: [logic] select_news_from_channel() matches channel names case-insensitively
	// (COLLATE NOCASE), so /news treats "general" and "General" as the same channel -- but this DISTINCT
	// had no collation, so /newschannels could list them as two separate channels even though /news
	// merges their entries. Collate the same way here so the channel list matches what /news actually
	// groups together.
	char select_channels_query[300] = "SELECT DISTINCT channel COLLATE NOCASE \
		from News\
		ORDER BY channel COLLATE NOCASE";

	rc = sqlite3_prepare(db, select_channels_query, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	while (rc == SQLITE_ROW)
	{
		// GalaxyRP (Alex): [Database] Grab all the channels line by line.
		strcpy(channelName, sqlite3_column_text(stmt, 0));
		trap->SendServerCommand(ent - g_entities, va("print \"%s\n\"", channelName));
		rc = sqlite3_step(stmt);
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return;
}

void display_news(gentity_t* ent, int newsID, char* newsText, char* date) {

	trap->SendServerCommand(ent - g_entities, va("print \"^3--------------------------|%i|%s|----------------------\n\"",newsID, date));
	trap->SendServerCommand(ent - g_entities, va("print \"^2%s\n\"", newsText));
	trap->SendServerCommand(ent - g_entities, va("print \"^3-------------------------------------------------------------\n\"", date));
	return;
}

void select_news_from_channel(gentity_t* ent, char* channel, int numberOfEntries) {
	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	sqlite3_stmt* stmt = 0;
	char newsText[MAX_STRING_CHARS], date[MAX_STRING_CHARS];
	int newsID = 0;
	int i = 1;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	// GalaxyRP fix: [security] this used to build the query text via va("...channel = '%s'...LIMIT %i",
	// channel, numberOfEntries) -- splicing the raw channel name straight into the SQL string.
	// Reachable via /news <channel> <count>, a public command any connected (even unauthenticated)
	// player can run. Bind both values as parameters instead.
	char select_news_query[300] = "SELECT newsID, text, date\
		from(SELECT newsID, text, date from News WHERE channel = ? COLLATE NOCASE ORDER BY newsID DESC LIMIT ?) \
		ORDER BY newsID ASC";

	rc = sqlite3_prepare(db, select_news_query, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_text(stmt, 1, channel, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, numberOfEntries);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	while (rc == SQLITE_ROW)
	{
		if (i <= numberOfEntries) {
			// GalaxyRP (Alex): [Database] Grab all the news texts line by line.
			newsID = sqlite3_column_int(stmt, 0);
			strcpy(newsText, sqlite3_column_text(stmt, 1));
			strcpy(date, sqlite3_column_text(stmt, 2));

			display_news(ent, newsID, newsText, date);
		}

		rc = sqlite3_step(stmt);
		i++;
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return;
}

// GalaxyRP (Alex): [Database] DELETE This method deletes a news table row which is associated with the ID given.
// GalaxyRP fix: [security/Database] this used to build the query text via va("...newsID='%i'...", newsID)
// and run it through run_db_query() (a bare sqlite3_exec() wrapper). newsID always comes from atoi(), so
// this wasn't actually exploitable, but it was the last news-table query still splicing instead of
// binding, inconsistent with the rest of this file's DB layer -- fixed for consistency and in case this
// function is ever changed to take a raw string. run_db_query() also never reported success/failure or
// rows-affected back to the caller, so Cmd_NewsRemove_f couldn't tell whether anything was actually
// deleted; this now returns qtrue only when a row was actually removed (via sqlite3_changes()).
qboolean delete_news_table_row_with_id(gentity_t* ent, int newsID) {
	sqlite3* db;
	int rc;
	sqlite3_stmt* stmt = 0;
	qboolean deleted = qfalse;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return qfalse;
	}

	rc = sqlite3_prepare(db, "DELETE FROM News WHERE newsID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return qfalse;
	}
	sqlite3_bind_int(stmt, 1, newsID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	else
	{
		deleted = (sqlite3_changes(db) > 0) ? qtrue : qfalse;
	}
	sqlite3_finalize(stmt);

	sqlite3_close(db);

	return deleted;
}

/*
=================
DATABASE ACTIONS for interacting with the database

The methods do broader actions, which are a combination of multiple actions that will almost always be used together.
=================
*/

// GalaxyRP (Alex): [Database] This method saves the player's name, scale and model to the database. All the information is taken from ent. (Characters tables)
void update_current_character_name_and_model(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	char userinfo[MAX_INFO_STRING], modelName[MAX_INFO_STRING];
	int clientNum = ClientNumberFromString(ent, ent->client->pers.netname, qfalse);

	trap->GetUserinfo(clientNum, userinfo, sizeof(userinfo));
	Q_strncpyz(modelName, Info_ValueForKey(userinfo, "model"), sizeof(modelName));

	// GalaxyRP fix: [security] this used to go through run_db_query() with the player's netname,
	// model name, and saber model names (all player-controlled -- netname/model via userinfo, saber
	// names via the saber selection UI/commands) spliced straight into the UPDATE text via
	// va("...\"%s\"...'%s'..."). This runs on ClientUserinfoChanged() -- i.e. every time a player
	// changes their name or model -- making it a frequently-reachable, unauthenticated injection
	// point. Bind every value as a parameter instead.
	// GalaxyRP: [Saber RGB] saberOneColor/saberTwoColor join the saber models here -- this function
	// already runs on every userinfo change, which is exactly when a colour or blade style can have
	// changed (see update_saber_colors() in this file, which routes every change through
	// ClientUserinfoChanged for precisely this reason). Each column now packs two independent things
	// via SABER_STORED_PACK (g_local.h): which of the NUM_SABER_COLORS palette entries is selected
	// (bits 25-28), and the custom RGB payload behind it, if any, tagged with SABERRGB_SET so it can
	// never be mistaken for the legacy schema default (bare 1, no mode/RGB bits set at all) these
	// columns still hold on every pre-existing character row -- that legacy default decodes to mode
	// 0 (SABER_RED) with no RGB payload, a reasonable default for a row this feature predates.
	rc = sqlite3_prepare(db, "UPDATE Characters SET ModelScale=?, NetName=?, ModelName=?, saberOneModel=?, saberTwoModel=?, saberOneColor=?, saberTwoColor=? WHERE CharID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->ps.iModelScale);
	sqlite3_bind_text(stmt, 2, ent->client->pers.netname, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, modelName, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, ent->client->pers.saber1, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, ent->client->pers.saber2, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 6, SABER_STORED_PACK(ent->client->pers.saberColorMode[0], ent->client->pers.saberRGB[0]));
	sqlite3_bind_int(stmt, 7, SABER_STORED_PACK(ent->client->pers.saberColorMode[1], ent->client->pers.saberRGB[1]));
	sqlite3_bind_int(stmt, 8, ent->client->pers.CharID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	return;
}

// GalaxyRP (Alex): [Database] This method saves the player's scale to the database. All the information is taken from ent. (Characters tables)
// GalaxyRP fix: [Database] this used to compute clientNum/userinfo/modelName via GetUserinfo() on every
// call and never use modelName for anything -- the UPDATE below only ever touched ModelScale. Dropped
// the dead lookup. Also converted from run_db_query()/va()-spliced text to a parameterized statement,
// matching the rest of the DB layer -- both values here were always plain ints (iModelScale, CharID) so
// this wasn't exploitable, but it was the last scale-related query still splicing instead of binding.
void update_current_character_scale(gentity_t* ent, sqlite3* db) {
	int rc;
	sqlite3_stmt* stmt = 0;

	rc = sqlite3_prepare(db, "UPDATE Characters SET ModelScale=? WHERE CharID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->ps.iModelScale);
	sqlite3_bind_int(stmt, 2, ent->client->pers.CharID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	return;
}

void update_saber(gentity_t* ent, char* saber1Model, char* saber2Model, int number_of_args);
// GalaxyRP (Alex): [Database] This method changes the character used currently by the player. It reassigns skills, weapons, userinfo, and changes the default character associated with the account.
// GalaxyRP fix: [Char] added announce_switch -- when qfalse, suppresses this function's own "X
// switched to: Y" broadcast at the end. /char new now calls this function immediately after
// creating a character (see Cmd_Char_f) so the player is placed straight onto it instead of having
// to run a separate /char use; without this flag that flow would broadcast both "X created a new
// character and is now using: Y" (sent by Cmd_Char_f) and "X switched to: Y" (this function's own,
// unconditional) back to back for the same action. /new (Cmd_Register_F) and /char use both still
// pass qtrue, unchanged.
void select_player_character(gentity_t* ent, char *character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt, qboolean announce_switch) {
	int numberOfChars = 0;
	
	if (ent->client->sess.loggedin == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^2You must be logged in load a character.\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^2You must be logged in load a character.\n\"");

		return;
	}

	// GalaxyRP (Alex): [Database] Check to see if character exists or not.
	numberOfChars = select_number_of_characters_with_name(ent, character_name, db, zErrMsg, rc, stmt);

	if (numberOfChars != 1) {
		trap->SendServerCommand(ent - g_entities, va("print \"^2Character %s ^2does not exist.\n\"", character_name));
		trap->SendServerCommand(ent - g_entities, va("cp \"^2Character %s ^2does not exist.\n\"", character_name));

		return;
	}

	// GalaxyRP (Alex): [Database] Select all info from all character related tables.
	// GalaxyRP fix: [security] this used to build the query text via
	// va("...Name = '%s'...", character_name, accountID) -- splicing the raw character name straight
	// into the SQL string. Reachable via /new (using the new account's own username as its first
	// character's name) and via /char use <name>. Bind both values as parameters instead.
	char select_character_query[202] = "SELECT *\
		FROM Characters\
		INNER JOIN Skills\
		ON Skills.CharID = Characters.CharID\
		INNER JOIN Weapons\
		ON Weapons.CharID = Characters.CharID\
		WHERE Characters.Name = ? AND Characters.AccountID = ?";

	rc = sqlite3_prepare(db, select_character_query, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_text(stmt, 1, character_name, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, ent->client->sess.accountID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	if (rc == SQLITE_ROW)
	{
		char displayName[MAX_INFO_STRING], modelName[MAX_STRING_CHARS];

		// GalaxyRP fix: [Account] pers.player_statuses (bits like "was given force powers/guns via
		// admin /give while logged out") was never reset here, unlike pers.bitvalue and
		// pers.player_settings in Cmd_LogoutAccount_f -- so a flag set while logged out survived both
		// /new (this function is also the tail end of account creation) and /char use character
		// switches, even though it's only meant to describe a logged-out player's state. Reset it here
		// too, at the same "a character is now loaded" point as the rest of this block.
		ent->client->pers.player_statuses = 0;

		// GalaxyRP (Alex): [Database] Grab info from characters table.
		ent->client->pers.CharID = sqlite3_column_int(stmt, 1);
		ent->client->pers.credits = sqlite3_column_int(stmt, 2);
		ent->client->pers.level = sqlite3_column_int(stmt, 3);
		do_scale(ent, sqlite3_column_int(stmt, 4));
		// GalaxyRP fix: [security] sess.rpgchar is a fixed 32-byte buffer (g_local.h); character_name
		// here is the raw argument /char use <name> was typed with, only bounded by MAX_STRING_CHARS
		// (1024). create_new_character() now rejects any new character name that wouldn't fit this
		// buffer, so no character created from this point on can trigger this -- but a pre-existing
		// row created before that guard existed (or added directly to the database) could still match
		// this query and reach this strcpy() with an oversized name. Q_strncpyz() bounds the copy
		// instead of trusting the match, matching the same defense-in-depth already applied to
		// saber1Model/saber2Model just below.
		Q_strncpyz(ent->client->sess.rpgchar, character_name, sizeof(ent->client->sess.rpgchar));
		ent->client->pers.skillpoints = sqlite3_column_int(stmt, 6);
		strcpy(ent->client->pers.description, sqlite3_column_text(stmt, 7));
		strcpy(displayName, sqlite3_column_text(stmt, 8));
		strcpy(modelName, sqlite3_column_text(stmt, 9));

		// GalaxyRP (Alex): [XP System] Grab XP value from database.
		ent->client->pers.xp = sqlite3_column_int(stmt, 10);

		char saber1Model[30];
		char saber2Model[30];
		int saber1Color;
		int saber2Color;
		// GalaxyRP fix: [security] strcpy() into these fixed 30-byte stack buffers had no length
		// check against the value read from the Characters table -- a saber-model name of 30+
		// characters in the database (whether from a corrupted row or a maliciously edited one)
		// would overflow the buffer. Q_strncpyz() bounds the copy to the destination's size instead.
		Q_strncpyz(saber1Model, sqlite3_column_text(stmt, 11), sizeof(saber1Model));
		saber1Color = sqlite3_column_int(stmt, 12);
		Q_strncpyz(saber2Model, sqlite3_column_text(stmt, 13), sizeof(saber2Model));
		saber2Color = sqlite3_column_int(stmt, 14);

		// GalaxyRP: [Saber RGB] restore the character's custom blade colours and blade styles. These
		// two columns were read into unused locals before this feature existed -- they now actually
		// mean something. Applied here, ahead of update_saber() below, for the same reason the block
		// further down in select_account_and_default_character_data() is: update_saber() can trigger
		// a character save through ClientUserinfoChanged(), and that save writes pers.saberRGB[]/
		// pers.saberColorMode[] straight back to this row -- so both have to already hold this row's
		// values, not the previous character's.
		ent->client->pers.saberRGB[0] = (saber1Color & SABERRGB_SET) ? (saber1Color & SABERRGB_MASK) : 0;
		ent->client->pers.saberRGB[1] = (saber2Color & SABERRGB_SET) ? (saber2Color & SABERRGB_MASK) : 0;
		ent->client->pers.saberColorMode[0] = SABER_STORED_MODE(saber1Color);
		ent->client->pers.saberColorMode[1] = SABER_STORED_MODE(saber2Color);

		// GalaxyRP fix: [gameplay] number_of_sabers was initialized to 1 and only ever explicitly
		// re-set to 1 (the "if" branch never set it to 2), so this always evaluated to 1 regardless
		// of what saber2Model actually held. update_saber() treats number_of_args==2 (i.e.
		// number_of_sabers==1) as "no second saber" and force-overwrites saber2 with "none" --
		// so any saved dual/staff saber configuration was silently discarded and replaced with a
		// single saber on every character load, even though the database still had the real value.
		int number_of_sabers = 1;

		if (strcmp(saber2Model, "none") != 0) {
			number_of_sabers = 2;
		}
		update_saber(ent, saber1Model, saber2Model, number_of_sabers + 1);

		// GalaxyRP (Alex): [Database] Grab info from skills table. (column 15 is CharID, no need to grab that)
		for (int i = 0; i < NUM_OF_SKILLS; i++) {
			ent->client->pers.skill_levels[i] = sqlite3_column_int(stmt, i + 16);
		}

		// GalaxyRP fix: [gameplay] this used to be a contiguous loop (ps.ammo[i] = column(i + 71))
		// that assumed the ammo_t enum maps 1:1 onto the Weapons table's columns, and the "+71"
		// base offset was stale (the comment above still says "column 68 is CharID" -- it's
		// actually column 76 now). Both were wrong: (1) AMMO_EMPLACED sits between AMMO_ROCKETS
		// and AMMO_THERMAL in the enum but has no column in Weapons at all (the save path already
		// skips it), so a contiguous read can never line up past that gap; (2) Armor, Flamethrower,
		// ShieldRegen and HealthRegen were added to Skills after this offset was written, shifting
		// every column after them -- including all of Weapons -- eight places to the right. The net
		// effect was this loop silently reading Skills.Flamethrower, Skills.ShieldRegen,
		// Skills.HealthRegen and Weapons' own CharID column into ps.ammo instead of actual ammo
		// counts. Read each Weapons ammo column explicitly instead, mirroring the explicit list the
		// save path already uses -- verified against the live schema via a standalone harness.
		ent->client->ps.ammo[AMMO_BLASTER] = sqlite3_column_int(stmt, 77);
		ent->client->ps.ammo[AMMO_POWERCELL] = sqlite3_column_int(stmt, 78);
		ent->client->ps.ammo[AMMO_METAL_BOLTS] = sqlite3_column_int(stmt, 79);
		ent->client->ps.ammo[AMMO_ROCKETS] = sqlite3_column_int(stmt, 80);
		ent->client->ps.ammo[AMMO_THERMAL] = sqlite3_column_int(stmt, 81);
		ent->client->ps.ammo[AMMO_TRIPMINE] = sqlite3_column_int(stmt, 82);
		ent->client->ps.ammo[AMMO_DETPACK] = sqlite3_column_int(stmt, 83);

		// GalaxyRP (Alex): [Database] Apply the modelname and net name.
		set_netname(ent, displayName);
		set_model(ent, modelName);
		//ent->client->saber[1].model
		sqlite3_finalize(stmt);

		// GalaxyRP: [Saber RGB] republish the restored blade colours and push them into the player's
		// own cvars, so their console and saber menu show what was actually loaded rather than
		// whatever the client last sent (which the menu would otherwise re-send verbatim on its next
		// Apply, undoing the restore). Inside the row block, next to the set_netname()/set_model()
		// calls it mirrors -- with no row there is no character to have restored a colour from.
		update_saber_colors(ent);
	}

	// GalaxyRP fix: [Account] borrowed from a newer fork of this mod -- call initialize_rpg_skills()
	// directly here, synchronously, instead of relying solely on the G_Kill() respawn below to trigger
	// it (via ClientSpawn). This closes two real gaps in the old kill-only approach: (1) G_Kill() silently
	// no-ops for a paralyzed target (see its own player_statuses bit 6 check) and in GT_DUEL/GT_POWERDUEL
	// when g_allowDuelSuicide is off, so a player who is paralyzed or mid-duel when running /new or
	// /char use would otherwise never get this character's force powers/weapons applied at all; (2) even
	// when G_Kill() does succeed, the respawn it triggers is deferred until the client's next spawn tick,
	// leaving the previous character's loadout visibly equipped for a brief window right after the
	// command returns. initialize_rpg_skills() only touches force powers/weapons/health-shield-force caps
	// derived from pers.skill_levels[] (already loaded above), so calling it here is safe regardless of
	// whether this player is about to be killed. The respawn below, when it does fire, calls
	// initialize_rpg_skills() again via ClientSpawn -- that second call is idempotent (same skill_levels[]
	// in, same result out), so there's no double-apply side effect from calling it twice.
	initialize_rpg_skills(ent);

	// GalaxyRP (Alex): [Database] Kill the tntity to allow everything to take effect.
	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
		trap->SendServerCommand(ent - g_entities, va("print \"%s\n\"", ent->team));

		// GalaxyRP fix: [Model] don't call G_Kill() in the same frame as the set_model() call above --
		// see pending_relog_kill_time's declaration in g_local.h for why (T-pose race with the client's
		// asynchronous model/animation reload). ClientThink_real() in g_active.c fires the actual kill
		// once this buffer elapses.
		ent->client->pers.pending_relog_kill_time = level.time + 300;
	}

	// GalaxyRP (Alex): [Database] Assign the player the info from Accounts table.
	update_accounts_table_row_with_default_char(ent, character_name, db, zErrMsg, rc, stmt);

	// GalaxyRP (Alex): [Database] Display Messages.
	trap->SendServerCommand(ent - g_entities, "print \"^2Character loaded sucessfully!\n\"");
	trap->SendServerCommand(ent - g_entities, "cp \"^2Character loaded sucessfully!\n\"");
	if (announce_switch) {
		trap->SendServerCommand(-1, va("chat \"%s switched to: %s\n\"", ent->client->pers.netname, character_name));
	}

	return;
}

// GalaxyRP (Alex): [Database] This method displays the list of characters to a player.
void select_character_list(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt)
{
	char CharName[MAX_STRING_CHARS];
	int charLevel;

	// GalaxyRP (Alex): [Database] Get list of char names.
	rc = sqlite3_prepare(db, va("SELECT Name, Level FROM Characters WHERE AccountID='%i'", ent->client->sess.accountID), -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	int i = 1;
	while (rc == SQLITE_ROW) {
		strcpy(CharName, sqlite3_column_text(stmt, 0));
		charLevel = sqlite3_column_int(stmt, 1);
		trap->SendServerCommand(ent - g_entities, va("print \"^3%i.^2%s - Level:%i\n\"", i, CharName, charLevel));
		i++;
		rc = sqlite3_step(stmt);
	}

	// GalaxyRP fix: [Char] a bare /char only ever printed the character list above, with no reminder
	// of the subcommands that act on it -- print the same usage tip Cmd_Char_f's own malformed-input
	// fallback already uses right below the list, so a player doesn't have to already know the syntax.
	trap->SendServerCommand(ent - g_entities, "print \"^2Usage: /char <new/use/remove> <character name>\n\"");

	sqlite3_finalize(stmt);
}

// GalaxyRP (Alex): [Database] This method returns a list of character names readable by the UI.
void select_character_list_for_ui(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt, char* CharString)
{
	char CharName[MAX_STRING_CHARS] = "";
	int charLevel = 0;

	// GalaxyRP (Alex): [Database] Get list of char names.
	rc = sqlite3_prepare(db, va("SELECT Name, Level FROM Characters WHERE AccountID='%i'", ent->client->sess.accountID), -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	while (rc == SQLITE_ROW) {
		strcpy(CharName, sqlite3_column_text(stmt, 0));
		charLevel = sqlite3_column_int(stmt, 1);
		strcpy(CharString, va("%s&%s", CharString, CharName));
		rc = sqlite3_step(stmt);
	}

	sqlite3_finalize(stmt);
}

// GalaxyRP (Alex): [Database] This method loads the account information, as well as the information related to the default character, and assigns it to the entity.
// GalaxyRP fix: [security] the username parameter used to be declared "char username[MAX_STRING_CHARS]"
// (1024) -- purely documentation in C (a parameter array decays to a pointer regardless of the size
// written here), but misleading enough that a previous fix bounded an internal copy to match one
// caller's real buffer size (Cmd_Login_F's own local char username[256]) without accounting for the
// other real caller, ClientBegin() (g_client.c), which passes ent->client->sess.filename directly --
// a genuinely fixed 32-byte buffer (g_local.h). GCC's -Wstringop-overflow caught the mismatch once
// that internal copy carried an explicit size to check the declared 1024 against each call site's
// real argument size. Declaring the true minimum safe size (32) here instead -- this value is always
// eventually copied into that same 32-byte sess.filename field regardless of which caller reached
// here -- keeps the signature honest and clears the warning for both callers at once. Keep this in
// sync with the matching extern declaration in g_client.c if it ever changes.
void select_account_and_default_character_data(gentity_t* ent, char username[32], sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	char password[256], name[256], description[MAX_STRING_CHARS], netName[MAX_STRING_CHARS], modelName[MAX_STRING_CHARS];
	int accountID, player_settings, adminLevel, charID, credits, level, modelScale, skillpoints;

	// GalaxyRP fix: [security] this used to build the query text via
	// va("...Username = '%s'...Username = '%s'...", username, username) -- splicing the raw username
	// straight into the SQL string, twice. This is the main query /login runs once the password has
	// already been verified. Bind both placeholders as parameters instead.
	// GalaxyRP fix: [Account] this always looked up the account's DefaultChar, ignoring whichever
	// character was actually active -- harmless the first time this runs (a genuine /login, where no
	// character is active yet), but this same function is also called from ClientBegin() on every map
	// change for any already-logged-in player, which silently reverted an earlier /char use switch to
	// a non-default character back to the default one every time the map changed. That in turn made
	// pers.CharID (and therefore which character's row commands like /createcredits write to) wrong
	// until the player did /char use again. Added a third parameter for the currently-selected
	// character's name (sess.rpgchar): COALESCE/NULLIF picks it over the DefaultChar subquery whenever
	// it's non-empty, and falls back to DefaultChar exactly as before when it's empty (a true first
	// login, or right after /logout -- see the sess.rpgchar reset added to Cmd_LogoutAccount_f).
	char select_account_table_row[480] = "SELECT *\
		FROM Accounts, Characters\
		INNER JOIN Skills\
		ON Skills.CharID = Characters.CharID\
		INNER JOIN Weapons\
		ON Weapons.CharID = Characters.CharID\
		WHERE Accounts.Username = ? AND Characters.CharID = (\
			SELECT CharID\
			FROM Characters\
			Where Characters.Name = COALESCE(NULLIF(?, ''), (\
				SELECT DefaultChar\
				FROM Accounts\
				WHERE Accounts.Username = ?))\
			)";

	rc = sqlite3_prepare(db, select_account_table_row, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, ent->client->sess.rpgchar, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, username, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	if (rc == SQLITE_ROW)
	{
		accountID = sqlite3_column_int(stmt, 0);
		player_settings = sqlite3_column_int(stmt, 1);
		adminLevel = sqlite3_column_int(stmt, 2);
		// GalaxyRP fix: [security] bound both copies instead of trusting the DB values' length. Not
		// reachable through a *new* /new registration any more (Cmd_Register_F now rejects a username/
		// password too long for the fixed buffers these ultimately feed, a few lines below), but an
		// account created before that guard existed could still have an oversized value sitting in the
		// database right now, and this restore runs on every /login for as long as that row exists.
		// GalaxyRP fix note: this function's own signature declares "char username[MAX_STRING_CHARS]",
		// but a parameter array decays to a plain pointer -- sizeof(username) here would silently give
		// the pointer's size (8), not 1024, and the declared 1024 doesn't reflect reality either. This
		// used to bound the copy to 256, matching Cmd_Login_F's own local char username[256] -- but
		// that's not this function's only caller: ClientBegin() (g_client.c) also calls this, passing
		// ent->client->sess.filename directly, a genuinely fixed 32-byte buffer (g_local.h). 256 was
		// safe for the first caller and a guaranteed overflow for the second -- caught by GCC's
		// -Wstringop-overflow once this copy carried an explicit size for it to check against the real
		// argument size at that call site. Bound to sizeof(ent->client->sess.filename) (32) instead:
		// the true minimum safe size across every real caller, since this value is ultimately copied
		// into that same 32-byte field a few lines below regardless of which caller reached here.
		Q_strncpyz(password, sqlite3_column_text(stmt, 3), sizeof(password));
		Q_strncpyz(username, sqlite3_column_text(stmt, 4), sizeof(ent->client->sess.filename));
		charID = sqlite3_column_int(stmt, 7);
		credits = sqlite3_column_int(stmt, 8);
		level = sqlite3_column_int(stmt, 9);
		modelScale = sqlite3_column_int(stmt, 10);
		strcpy(name, sqlite3_column_text(stmt, 11));
		skillpoints = sqlite3_column_int(stmt, 12);
		strcpy(description, sqlite3_column_text(stmt, 13));
		strcpy(netName, sqlite3_column_text(stmt, 14));
		strcpy(modelName, sqlite3_column_text(stmt, 15));

		// GalaxyRP (Alex): [XP System] Grab XP value from database.
		ent->client->pers.xp = sqlite3_column_int(stmt, 16);

		char saber1Model[30];
		char saber2Model[30];
		int saber1Color;
		int saber2Color;
		// GalaxyRP fix: [security] same fixed-size stack-buffer overflow as select_player_character()
		// above -- bound the copy to the destination's size instead of trusting the DB value's length.
		Q_strncpyz(saber1Model, sqlite3_column_text(stmt, 17), sizeof(saber1Model));
		saber1Color = sqlite3_column_int(stmt, 18);
		Q_strncpyz(saber2Model, sqlite3_column_text(stmt, 19), sizeof(saber2Model));
		saber2Color = sqlite3_column_int(stmt, 20);

		// GalaxyRP fix: [Account] apply the loaded account/character fields to ent->client, and mark
		// the session logged in, here -- before update_saber() below -- instead of after it (which
		// is where this block, and the sess.loggedin assignment, used to sit). update_saber() can
		// trigger a database save via ClientUserinfoChanged() (see the "supdatesaber" fix above)
		// whenever the saber it's asked to set differs from what pers.saber1/2 already holds, which
		// is normally true on every login. That save is gated on sess.loggedin being true, and the
		// row it writes to is chosen by ent->client->pers.CharID -- both of which used to only be
		// set much further down, after update_saber() had already run and returned. loggedin being
		// false there meant the save was always silently skipped -- harmless only by coincidence,
		// since the data being "saved" was the same data just read from this very row a few lines
		// above, so there was nothing new to persist. Had that save instead been allowed to fire
		// with the old ordering, it would have written using whatever CharID/model-scale happened
		// to already be sitting in pers/ps from before this login -- a real, silent write to the
		// wrong character's row. Moving CharID/scale (and loggedin) ahead of update_saber() means
		// any save it triggers always targets this row with this row's own data. set_netname() and
		// set_model() below (unchanged, still after this block) each trigger their own save the
		// same way; by the time set_model() runs last, every field involved -- CharID, scale,
		// netname, saber1/2, model -- is already correct, so whatever transient state an earlier
		// save in this sequence wrote is simply overwritten before the function returns.
		ent->client->sess.accountID = accountID;
		ent->client->pers.player_settings = player_settings;
		ent->client->pers.bitvalue = adminLevel;
		// GalaxyRP fix: [security] pers.password and sess.filename are both fixed 32-byte buffers
		// (g_local.h); password/username here come straight from the database (already bounded to
		// 256 above, but still well over 32) and this restore runs on every /login. Bound the copy
		// instead of trusting it, for the same pre-existing-row reason given above.
		Q_strncpyz(ent->client->pers.password, password, sizeof(ent->client->pers.password));
		Q_strncpyz(ent->client->sess.filename, username, sizeof(ent->client->sess.filename));
		ent->client->pers.CharID = charID;
		ent->client->pers.credits = credits;
		ent->client->pers.level = level;
		do_scale(ent, modelScale);
		// GalaxyRP fix: [security] same fixed-32-byte-buffer overflow risk as the matching strcpy() in
		// select_player_character() above (this is /login's own version of the same restore) -- bound
		// the copy instead of trusting a DB-column value that predates create_new_character()'s length
		// guard, or that never went through it (e.g. a first character named after the account's own
		// username at registration time -- see insert_chars_table_row()).
		Q_strncpyz(ent->client->sess.rpgchar, name, sizeof(ent->client->sess.rpgchar));
		ent->client->pers.skillpoints = skillpoints;
		strcpy(ent->client->pers.description, description);

		// GalaxyRP: [Saber RGB] restore this character's custom blade colours and blade styles. Part
		// of this same pre-update_saber() block for exactly the reason spelled out above: the
		// character save that update_saber() can trigger writes pers.saberRGB[]/pers.saberColorMode[]
		// back to whichever row pers.CharID names, so both have to be this character's before it runs.
		ent->client->pers.saberRGB[0] = (saber1Color & SABERRGB_SET) ? (saber1Color & SABERRGB_MASK) : 0;
		ent->client->pers.saberRGB[1] = (saber2Color & SABERRGB_SET) ? (saber2Color & SABERRGB_MASK) : 0;
		ent->client->pers.saberColorMode[0] = SABER_STORED_MODE(saber1Color);
		ent->client->pers.saberColorMode[1] = SABER_STORED_MODE(saber2Color);

		ent->client->sess.loggedin = qtrue;

		// GalaxyRP: [Force Enlightenment] push the login state to the client immediately -- see the
		// matching comment in g_client.c -- so CG_GreyItem stops greying out the "wrong side"
		// Enlightenment pickup for this player right away, not only after the Profile UI is opened.
		trap->SendServerCommand(ent->s.number, va("supdateloggedin %i\n", ent->client->sess.loggedin));

		// GalaxyRP fix: [gameplay] Same bug as select_player_character() above, plus this copy
		// compared against "" instead of "none" -- saber2Model coming from the database is never
		// really an empty string (the column's schema default is 'saber_1', and update_saber()
		// always saves the literal string "none" for an unset second saber), so this "if" almost
		// never matched either way. number_of_sabers was initialized to 1 and never explicitly set
		// to 2, so it always evaluated to 1 regardless of what saber2Model actually held.
		// update_saber() treats number_of_args==2 (number_of_sabers==1) as "no second saber" and
		// force-overwrites saber2 with "none" -- so logging in (or respawning after a map change,
		// which re-runs this same restore) silently discarded any saved dual/staff saber
		// configuration and replaced it with a single saber, even though the database still had
		// the real value. This is also why a fresh character (whose saberTwoModel column defaults
		// to 'saber_1', not "none") never actually showed as dual-wielding on first login.
		int number_of_sabers = 1;

		if (strcmp(saber2Model, "none") != 0) {
			number_of_sabers = 2;
		}
		update_saber(ent, saber1Model, saber2Model, number_of_sabers + 1);

		// GalaxyRP (Alex): [Database] Column 21 is a duplicate of CharID, no need to grab that.
		for (int i = 0; i < NUM_OF_SKILLS; i++) {
			ent->client->pers.skill_levels[i] = sqlite3_column_int(stmt, i + 22);
		}
		// GalaxyRP fix: [gameplay] same bug as select_player_character() above (this function's
		// version of the loop used a "+78" base, with the comment claiming Weapons.CharID sits at
		// column 79 -- it's actually column 82 now that this query also joins in Accounts). Same
		// two causes: AMMO_EMPLACED has no Weapons column at all, and Skills gained four columns
		// (Armor, Flamethrower, ShieldRegen, HealthRegen) after this offset was written. Read each
		// Weapons ammo column explicitly instead, mirroring the explicit list the save path uses --
		// verified against the live schema via a standalone harness.
		ent->client->ps.ammo[AMMO_BLASTER] = sqlite3_column_int(stmt, 83);
		ent->client->ps.ammo[AMMO_POWERCELL] = sqlite3_column_int(stmt, 84);
		ent->client->ps.ammo[AMMO_METAL_BOLTS] = sqlite3_column_int(stmt, 85);
		ent->client->ps.ammo[AMMO_ROCKETS] = sqlite3_column_int(stmt, 86);
		ent->client->ps.ammo[AMMO_THERMAL] = sqlite3_column_int(stmt, 87);
		ent->client->ps.ammo[AMMO_TRIPMINE] = sqlite3_column_int(stmt, 88);
		ent->client->ps.ammo[AMMO_DETPACK] = sqlite3_column_int(stmt, 89);

		sqlite3_finalize(stmt);
	}

	set_netname(ent, netName);
	set_model(ent, modelName);

	// GalaxyRP: [Saber RGB] same as select_player_character() -- republish the restored colours and
	// push them into the client's own cvars so its console and saber menu agree with the server.
	update_saber_colors(ent);

	// GalaxyRP fix: [Account] pers.player_statuses (bits like "was given force powers/guns via admin
	// /give while logged out") was never reset on login, unlike pers.bitvalue and pers.player_settings
	// in Cmd_LogoutAccount_f -- so a flag set while logged out stayed set through this account's login
	// and every subsequent map-change reload, even though it's only meant to describe a logged-out
	// player's state. Reset it here too, for the same reason and at the same "now logged in" point.
	ent->client->pers.player_statuses = 0;

	ent->client->sess.amrpgmode = 2;

	return;
}

// GalaxyRP (Alex): [Database] This method creates a new character associated with the account the player is currently logged in, and adds the default data in all the tables. ASSUMES PLAYER IS LOGGED IN!

// GalaxyRP: [Char fix] added three checks ahead of the pre-existing uniqueness check below, none of
// which previously existed: an empty name (a bare /char new "" would otherwise insert a blank,
// unselectable-looking row that just clutters /char's own list output), a name too long for
// ent->client->sess.rpgchar to hold (a fixed 32-byte buffer -- see the matching Q_strncpyz() fixes
// in select_player_character() and select_account_and_default_character_data() above, both of which
// strcpy() a character name into that buffer once this one is ever loaded), and a name containing
// '&' (CG_ZykChars() on the client, cg_servercmds.c, splits the character list this command's
// caller pushes to the client on exactly that character via strtok(arg, "&"); a name containing '&'
// would be split into multiple bogus entries and desync every character slot after it in that
// player's own character-select UI). Unlike Cmd_Register_F()'s username handling, this deliberately
// does NOT strip color codes or force lowercase -- a character's name is meant to be a player-chosen
// display name (shown with its own colors throughout, e.g. select_character_list()'s listing), not
// a login credential that needs to be predictable/comparable the way an account username does.
qboolean create_new_character(gentity_t* ent, char char_name[MAX_STRING_CHARS], sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {

	if (char_name[0] == '\0') {
		trap->SendServerCommand(ent - g_entities, "print \"^1Character name cannot be empty.\n\"");
		return qfalse;
	}

	if (strlen(char_name) > sizeof(ent->client->sess.rpgchar) - 1) {
		trap->SendServerCommand(ent - g_entities, va("print \"^1Character name can only have a maximum of %i characters.\n\"", (int)sizeof(ent->client->sess.rpgchar) - 1));
		return qfalse;
	}

	if (strchr(char_name, '&') != NULL) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Character name cannot contain the '&' character.\n\"");
		return qfalse;
	}

	// GalaxyRP: [security] sess.rpgchar (this name, once accepted) is later spliced raw into several
	// fopen()/system() calls elsewhere in this file -- zyk_config_filename(), description_add(), and
	// zyk_remove_configs()'s system("rm -f ...")/system("DEL /F ...") calls -- so an unvalidated
	// character name could escape the folder those are meant to stay inside, or, in
	// zyk_remove_configs()'s case, inject extra shell commands. This also subsumes the '&' check
	// above, but that one is left in place for its own, more specific message.
	if (zyk_check_user_input(char_name, strlen(char_name)) == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Character name can only contain letters and numbers.\n\"");
		return qfalse;
	}

	// GalaxyRP (Alex): [Database] Character names should be unique, check for it here.
	if (select_number_of_characters_with_name(ent, char_name, db, zErrMsg, rc, stmt) > 0) {
		trap->SendServerCommand(ent - g_entities, va("print \"^1Character name ^7%s ^1is already in use.\n\"", char_name));
		trap->SendServerCommand(ent - g_entities, va("cp \"^1Character name ^7%s ^1is already in use.\n\"", char_name));
		return qfalse;
	}

	// GalaxyRP fix: [security] this used to go through run_db_query() as one combined
	// Characters+Skills+Weapons INSERT, with the player-chosen character name spliced straight into
	// the Characters portion via va("...'%s'..."). Reachable via /char new <name>, usable by any
	// logged-in player. The Skills and Weapons INSERTs are fully static (every column defaults to
	// '0', no placeholders at all), so only the Characters INSERT needs binding; it's split out and
	// prepared/bound/stepped on its own, while the still-static Skills+Weapons INSERT stays a single
	// run_db_query() call, unchanged, below.
	// GalaxyRP fix: [Char] both of these SQL failure paths used to fall through to "return;" (a bare,
	// unconditional success as far as any caller could tell -- this function returned void). Now that
	// the return value controls whether Cmd_Char_f switches the player onto this character and
	// announces it as created, a failed Characters insert has to report qfalse here too, or the
	// player would be told (and everyone else shown a chat broadcast) that a character was created
	// and is now in use when no row actually exists for it.
	rc = sqlite3_prepare(db, "INSERT INTO Characters(AccountID, Credits, Level, ModelScale, Name, SkillPoints, Description, NetName, ModelName, xp) VALUES(?, '100', '1', '100', ?, '1', 'Nothing to show.', 'DefaultName', 'kyle', 0)", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return qfalse;
	}
	sqlite3_bind_int(stmt, 1, ent->client->sess.accountID);
	sqlite3_bind_text(stmt, 2, char_name, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return qfalse;
	}
	sqlite3_finalize(stmt);

	// GalaxyRP fix: [correctness] this buffer was sized at 900 bytes, but the two INSERT
	// statements it initializes (Skills, then Weapons) actually need 1081 bytes including the
	// terminating NUL -- confirmed by measuring the literal below. 900 predates the Armor,
	// Flamethrower, ShieldRegen and HealthRegen columns that were later added to Skills (the same
	// schema change behind the ammo-restore column-offset bug fixed earlier); the array size was
	// never revisited when the column list grew to match. MSVC flagged this (C4045, "array bounds
	// overflow") and silently truncates the initializer to fit -- with the buffer completely full
	// of literal content and no room left for a NUL terminator. That buffer is then handed straight
	// to run_db_query() -> sqlite3_exec(), which requires a NUL-terminated C string: without one,
	// it reads past the end of this stack array until it happens to find a zero byte elsewhere on
	// the stack, and executes whatever garbage text it finds appended to the truncated, syntactically
	// broken SQL. In practice this means every /char new likely failed to insert that character's
	// Skills and Weapons rows (a syntax error caught and logged by run_db_query(), not crashed on),
	// leaving a Characters row with no matching Skills/Weapons rows -- which the INNER JOINs in
	// select_player_character()/select_account_and_default_character_data() require, so the new
	// character could never actually be loaded. Sized to 1100 (matching the same margin already
	// used by the near-identical-size update_character_query[1100] elsewhere in this file) rather
	// than the exact 1081 needed, so it isn't this fragile again the next time a column is added.
	char create_new_character_query[1100] = "INSERT INTO Skills(Jump, Push, Pull, Speed, Sense, SaberAttack, SaberDefense, SaberThrow, Absorb, Heal, Protect, MindTrick, TeamHeal, Lightning, Grip, Drain, Rage, TeamEnergize, StunBaton, BlasterPistol, BlasterRifle, Disruptor, Bowcaster, Repeater, DEMP2, Flechette, RocketLauncher, ConcussionRifle, BryarPistol, Melee, MaxShield, ShieldStrength, HealthStrength, DrainShield, Jetpack, SenseHealth, ShieldHeal, TeamShieldHeal, UniqueSkill, BlasterPack, PowerCell, MetalBolts, Rockets, Thermals, TripMines, Detpacks, Binoculars, BactaCanister, SentryGun, SeekerDrone, Eweb, BigBacta, ForceField, CloakItem, ForcePower, Improvements) VALUES('0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0');\
		INSERT INTO Weapons(AmmoBlaster, AmmoPowercell, AmmoMetalBolts, AmmoRockets, AmmoThermal, AmmoTripmine, AmmoDetpack) VALUES('0', '0', '0', '0', '0', '0', '0');";

	run_db_query(create_new_character_query, db, zErrMsg, rc, stmt);

	// GalaxyRP fix: [Char] the "X created a char: Y" broadcast that used to be sent from here is now
	// sent by Cmd_Char_f instead, once it also knows the immediately-following switch onto this
	// character (select_player_character()) has happened -- see the comment on that call site.
	return qtrue;
}

// GalaxyRP (Alex): [Database] This method removes a character form the database. It affects all tables that contain character information. ASSUMES THE PLAYER IS LOGGED IN!!!

// GalaxyRP: [Char fix] this used to unconditionally run its DELETEs against whatever CharID
// select_char_id_using_char_name() returned, including -1 (its "not found" sentinel) -- so
// /char remove <a name that doesn't exist> matched zero rows and silently did nothing, with no
// feedback either way, success or failure. It also never checked whether char_name was the
// character the player currently has loaded: deleting that row left ent->client->pers.CharID (and,
// in the common case, the account's own DefaultChar column -- /char use keeps it in sync with
// whatever was last selected, see update_accounts_table_row_with_default_char()) pointing at a
// now-nonexistent character. Any later save would then silently update nothing, and the account's
// next /login would find no matching character to restore at all. Both are checked explicitly now,
// each with its own message; Q_stricmp (case-insensitive) is used for the active-character check
// specifically so it can't be bypassed by retyping the name with different casing.
void remove_character(gentity_t* ent, char char_name[MAX_STRING_CHARS], sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {

	int charID;

	if (Q_stricmp(char_name, ent->client->sess.rpgchar) == 0) {
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot remove your currently active character. Switch to a different character first with /char use <character name>.\n\"");
		return;
	}

	charID = select_char_id_using_char_name(ent, char_name, db, zErrMsg, rc, stmt);

	if (charID == -1) {
		trap->SendServerCommand(ent - g_entities, va("print \"^2Character %s ^2does not exist.\n\"", char_name));
		return;
	}

	char remove_character_query[117] = "DELETE FROM Characters WHERE CharID='%i';DELETE FROM Skills WHERE CharID='%i';DELETE FROM Weapons WHERE CharID='%i';";

	run_db_query(va(remove_character_query, charID, charID, charID), db, zErrMsg, rc, stmt);

	trap->SendServerCommand(ent - g_entities, va("print \"^2Character %s ^2has been removed.\n\"", char_name));
	trap->SendServerCommand(ent - g_entities, va("cp \"^2Character %s ^2has been removed.\n\"", char_name));

	return;
}

// GalaxyRP (Alex): [Database] This method updates the account, Characters, Skills and Weapons tables with current values. ASSUMES THE PLAYER IS LOGGED IN!!!
void update_current_character_and_account(gentity_t* ent) {
	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	sqlite3_stmt* stmt = NULL;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	char userinfo[MAX_INFO_STRING], modelName[MAX_INFO_STRING];
	int clientNum = ClientNumberFromString(ent, ent->client->pers.netname, qfalse);

	trap->GetUserinfo(clientNum, userinfo, sizeof(userinfo));
	Q_strncpyz(modelName, Info_ValueForKey(userinfo, "model"), sizeof(modelName));
	
	// GalaxyRP fix: [security] this used to go through run_db_query() as one combined
	// Characters+Skills+Weapons+Accounts UPDATE, with the player's description, netname, model name
	// (all player-controlled) and DefaultChar (the character name) spliced straight into their
	// respective portions via va("...\"%s\"...'%s'..."). This runs via save_account() -> any
	// logged-in player being periodically/opportunistically saved -- a frequently-reachable,
	// unauthenticated injection point. sqlite3_bind_*() only binds a single prepared statement, so
	// the Characters and Accounts UPDATEs (the two with string values) are split out and each
	// prepared/bound/stepped on their own; the Skills and Weapons UPDATEs are all-integer and
	// unaffected, so they stay combined via run_db_query() below, unchanged.
	rc = sqlite3_prepare(db, "UPDATE Characters SET Credits=?, Level=?, ModelScale=?, Skillpoints=?, Description=?, NetName=?, ModelName=?, xp=? WHERE CharID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->pers.credits);
	sqlite3_bind_int(stmt, 2, ent->client->pers.level);
	sqlite3_bind_int(stmt, 3, ent->client->ps.iModelScale);
	sqlite3_bind_int(stmt, 4, ent->client->pers.skillpoints);
	sqlite3_bind_text(stmt, 5, ent->client->pers.description, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, ent->client->pers.netname, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 7, modelName, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 8, ent->client->pers.xp);
	sqlite3_bind_int(stmt, 9, ent->client->pers.CharID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	// GalaxyRP fix: [Database] same PlayerSettings-hardcoded-to-'0' bug as
	// update_accounts_table_row_with_current_values() above -- this is the other write site
	// (save_account(ent, qtrue), the RPG-char save path) that was silently resetting a logged-in
	// player's /settings toggles back to 0 in the DB. Bind the real value instead.
	rc = sqlite3_prepare(db, "UPDATE Accounts SET PlayerSettings=?, AdminLevel=?, DefaultChar=? WHERE AccountID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->pers.player_settings);
	sqlite3_bind_int(stmt, 2, ent->client->pers.bitvalue);
	sqlite3_bind_text(stmt, 3, ent->client->sess.rpgchar, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 4, ent->client->sess.accountID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	// GalaxyRP fix: [Database] this query used to stop at Improvements, silently omitting the
	// Armor/Flamethrower/ShieldRegen/HealthRegen skill columns that update_skills_query() elsewhere
	// in this file already saves in full -- not an active data-loss bug (those 4 skills can only
	// change via /rpmodeup and /rpmodedown, which always call update_skills_table_row_with_current_values()
	// immediately afterward and save all 60 columns themselves), but this save path should still
	// write a complete, self-consistent row rather than relying on another function to cover the gap.
	char update_character_query[1200] = "UPDATE Skills SET Jump='%i', Push='%i', Pull='%i', Speed='%i', Sense='%i', SaberAttack='%i', SaberDefense='%i', SaberThrow='%i', Absorb='%i', Heal='%i', Protect='%i', MindTrick='%i', TeamHeal='%i', Lightning='%i', Grip='%i', Drain='%i', Rage='%i', TeamEnergize='%i', StunBaton='%i', BlasterPistol='%i', BlasterRifle='%i', Disruptor='%i', Bowcaster='%i', Repeater='%i', DEMP2='%i', Flechette='%i', RocketLauncher='%i', ConcussionRifle='%i', BryarPistol='%i', Melee='%i', MaxShield='%i', ShieldStrength='%i', HealthStrength='%i', DrainShield='%i', Jetpack='%i', SenseHealth='%i', ShieldHeal='%i', TeamShieldHeal='%i', UniqueSkill='%i', BlasterPack='%i', PowerCell='%i', MetalBolts='%i', Rockets='%i', Thermals='%i', TripMines='%i', Detpacks='%i', Binoculars='%i', BactaCanister='%i', SentryGun='%i', SeekerDrone='%i', Eweb='%i', BigBacta='%i', ForceField='%i', CloakItem='%i', ForcePower='%i', Improvements='%i', Armor='%i', Flamethrower='%i', ShieldRegen='%i', HealthRegen='%i' WHERE CharID='%i';\
		UPDATE Weapons SET AmmoBlaster='%i', AmmoPowercell='%i', AmmoMetalBolts='%i', AmmoRockets='%i', AmmoThermal='%i', AmmoTripmine='%i', AmmoDetpack='%i' WHERE CharID='%i'";

	run_db_query(va(update_character_query,
		ent->client->pers.skill_levels[0],	//Jump
		ent->client->pers.skill_levels[1],	//Push
		ent->client->pers.skill_levels[2],	//Pull
		ent->client->pers.skill_levels[3],	//Speed
		ent->client->pers.skill_levels[4],	//Sense
		ent->client->pers.skill_levels[5],	//SaberAttack
		ent->client->pers.skill_levels[6],	//SaberDefense
		ent->client->pers.skill_levels[7],	//SaberThrow
		ent->client->pers.skill_levels[8],	//Absorb
		ent->client->pers.skill_levels[9],	//Heal
		ent->client->pers.skill_levels[10],	//Protect
		ent->client->pers.skill_levels[11],	//MindTrick
		ent->client->pers.skill_levels[12],	//TeamHeal
		ent->client->pers.skill_levels[13],	//Lightning
		ent->client->pers.skill_levels[14],	//Grip
		ent->client->pers.skill_levels[15],	//Drain
		ent->client->pers.skill_levels[16],	//Rage
		ent->client->pers.skill_levels[17],	//TeamEnergize
		ent->client->pers.skill_levels[18],	//StunBaton
		ent->client->pers.skill_levels[19],	//BlasterPistol
		ent->client->pers.skill_levels[20],	//BlasterRifle
		ent->client->pers.skill_levels[21],	//Disruptor
		ent->client->pers.skill_levels[22],	//Bowcaster
		ent->client->pers.skill_levels[23],	//Repeater
		ent->client->pers.skill_levels[24],	//DEMP2
		ent->client->pers.skill_levels[25],	//Flechette
		ent->client->pers.skill_levels[26],	//RocketLauncher
		ent->client->pers.skill_levels[27],	//ConcussionRifle
		ent->client->pers.skill_levels[28],	//BryarPistol
		ent->client->pers.skill_levels[29],	//Melee
		ent->client->pers.skill_levels[30],	//MaxShield
		ent->client->pers.skill_levels[31],	//ShieldStrength
		ent->client->pers.skill_levels[32],	//HealthStrength
		ent->client->pers.skill_levels[33],	//DrainShield
		ent->client->pers.skill_levels[34],	//Jetpack
		ent->client->pers.skill_levels[35],	//SenseHealth
		ent->client->pers.skill_levels[36],	//ShieldHeal
		ent->client->pers.skill_levels[37],	//TeamShieldHeal
		ent->client->pers.skill_levels[38],	//UniqueSkill
		ent->client->pers.skill_levels[39],	//BlasterPack
		ent->client->pers.skill_levels[40],	//PowerCell
		ent->client->pers.skill_levels[41],	//MetalBolts
		ent->client->pers.skill_levels[42],	//Rockets
		ent->client->pers.skill_levels[43],	//Thermals
		ent->client->pers.skill_levels[44],	//TripMines
		ent->client->pers.skill_levels[45],	//Detpacks
		ent->client->pers.skill_levels[46],	//Binoculars
		ent->client->pers.skill_levels[47],	//BactaCanister
		ent->client->pers.skill_levels[48],	//SentryGun
		ent->client->pers.skill_levels[49],	//SeekerDrone
		ent->client->pers.skill_levels[50],	//Eweb
		ent->client->pers.skill_levels[51],	//BigBacta
		ent->client->pers.skill_levels[52],	//ForceField
		ent->client->pers.skill_levels[53],	//CloakItem
		ent->client->pers.skill_levels[54],	//ForcePower
		ent->client->pers.skill_levels[55], //Improvements
		ent->client->pers.skill_levels[56], //Armor
		ent->client->pers.skill_levels[57], //Flamethrower
		ent->client->pers.skill_levels[58], //Shield Regen
		ent->client->pers.skill_levels[59], //Health Regen
		ent->client->pers.CharID,
		ent->client->ps.ammo[AMMO_BLASTER],
		ent->client->ps.ammo[AMMO_POWERCELL],
		ent->client->ps.ammo[AMMO_METAL_BOLTS],
		ent->client->ps.ammo[AMMO_ROCKETS],
		ent->client->ps.ammo[AMMO_THERMAL],
		ent->client->ps.ammo[AMMO_TRIPMINE],
		ent->client->ps.ammo[AMMO_DETPACK],
		ent->client->pers.CharID
	), db, zErrMsg, rc, stmt);

	sqlite3_close(db);

	return;
}

void Cmd_Register_F(gentity_t * ent)
{
	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = NULL;
	char username[256] = { 0 }, password[256] = { 0 };
	int accountID = 0, i = 0;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	if (trap->Argc() != 3)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^2Command Usage: /new <username> <password>\n\"");
		sqlite3_close(db);
		return;
	}

	// GalaxyRP fix: [Account] this had no equivalent of Cmd_Login_F's own "already logged in" guard --
	// an already-logged-in player running /new sailed straight through and got switched onto the
	// brand new account, with whatever progress they'd made on their previous character since its
	// last save silently discarded (this function never calls save_account() the way /char and
	// /logout do). Same check, same message, same order as Cmd_Login_F above: require /logout first.
	if (ent->client->sess.loggedin == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You are already logged in to your account.\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^1You are already logged in to your account.\n\"");
		sqlite3_close(db);
		return;
	}

	trap->Argv(1, username, sizeof(username));
	trap->Argv(2, password, sizeof(password));

	Q_StripColor(username);
	Q_strlwr(username);

	// GalaxyRP fix: [security] neither of these was checked before -- ent->client->sess.filename and
	// ent->client->pers.password are both fixed 32-byte buffers (g_local.h), but username/password
	// here were only bounded by their local 256-byte Argv() buffers. A long-enough /new <username>
	// <password> overflowed sess.filename/pers.password directly, right below, on every single
	// registration; the same oversized values were also persisted to the Accounts table and would go
	// on to overflow those same two fields again on every future /login (see the matching
	// Q_strncpyz() fixes in select_account_and_default_character_data() below). The password limit
	// matches /changepassword's own existing "maximum of 30 characters" check exactly, so a password
	// that's valid from one command is valid from the other.
	if (strlen(username) > sizeof(ent->client->sess.filename) - 1) {
		trap->SendServerCommand(ent - g_entities, va("print \"^1Username can only have a maximum of %i characters.\n\"", (int)sizeof(ent->client->sess.filename) - 1));
		sqlite3_close(db);
		return;
	}

	if (strlen(password) > 30) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Password can only have a maximum of 30 characters.\n\"");
		sqlite3_close(db);
		return;
	}

	// GalaxyRP: [security] sess.filename (this username, once accepted) is later spliced raw into
	// several fopen()/system() calls elsewhere in this file -- zyk_config_filename(),
	// zyk_legacy_config_filename(), and zyk_remove_configs()'s system("rm -f ...")/system("DEL /F
	// ...") calls -- the same gap the character-name check in create_new_character() closes for
	// sess.rpgchar. Reject anything but letters and digits here too, so a crafted username can't
	// escape those folders or, in zyk_remove_configs()'s case, inject extra shell commands. This also
	// catches a username that stripped down to nothing (Q_StripColor() above could reduce an
	// all-color-codes input to an empty string, which was never explicitly checked before).
	if (zyk_check_user_input(username, strlen(username)) == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Username can only contain letters and numbers.\n\"");
		sqlite3_close(db);
		return;
	}

	// GalaxyRP fix: [cosmetic] this used to print comparisonName here instead of username --
	// comparisonName was declared but never assigned anywhere in this function, so both messages
	// always printed an empty name where the rejected username was supposed to appear ("Username
	// ^7^1is already in use."). Print the actual username that was checked, and drop the dead
	// comparisonName local entirely (it served no other purpose).
	if (select_number_of_accounts_with_username(ent, username, db, zErrMsg, rc, stmt) != 0) {
		trap->SendServerCommand(ent - g_entities, va("print \"^1Username ^7%s ^1is already in use.\n\"", username));
		trap->SendServerCommand(ent - g_entities, va("cp \"^1Username ^7%s ^1is already in use.\n\"", username));

		sqlite3_close(db);
		return;
	}

	insert_accounts_table_row(ent, username, password, db, zErrMsg, rc, stmt);

	select_accounts_table_row(ent, username, db, zErrMsg, rc, stmt);

	//always 2, kept for backwards compatibility
	ent->client->sess.amrpgmode = 2;
	ent->client->sess.loggedin = qtrue;

	// GalaxyRP: [Force Enlightenment] see the matching comment in g_client.c -- keeps cgame's
	// CG_GreyItem in sync with login state right away.
	trap->SendServerCommand(ent->s.number, va("supdateloggedin %i\n", ent->client->sess.loggedin));

	// GalaxyRP fix: [security] defense-in-depth -- Q_strncpyz() instead of strcpy(), even though the
	// length checks above already guarantee both fit, matching the same belt-and-suspenders approach
	// already used for saber1Model/saber2Model and the character-name copies elsewhere in this file.
	Q_strncpyz(ent->client->sess.filename, username, sizeof(ent->client->sess.filename));
	Q_strncpyz(ent->client->pers.password, password, sizeof(ent->client->pers.password));

	insert_chars_table_row(ent, username, db, zErrMsg, rc, stmt);
	insert_skills_table_row(ent, db, zErrMsg, rc, stmt);
	insert_weapons_table_row(ent, db, zErrMsg, rc, stmt);
	select_player_character(ent, username, db, zErrMsg, rc, stmt, qtrue);

	trap->SendServerCommand(ent - g_entities, "print \"^2Your account has been successfully created and you are now logged in.\n\"");
	trap->SendServerCommand(ent - g_entities, "cp \"^2Your account has been successfully created and you are now logged in.\n\"");

	sqlite3_close(db);

	return;
}

void Cmd_Login_F(gentity_t * ent)
{
	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = 0;
	char username[256] = { 0 }, password[256] = { 0 }, comparisonUsername[256] = { 0 }, comparisonPassword[256] = { 0 }, defaultChar[256] = { 0 };

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	if (trap->Argc() != 3)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^2Command Usage: /login <username> <password>\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^2Command Usage: /login <username> <password>\n\"");
		sqlite3_close(db);
		return;
	}

	if (ent->client->sess.loggedin == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You are already logged in to your account.\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^1You are already logged in to your account.\n\"");
		sqlite3_close(db);
		return;
	}

	trap->Argv(1, username, sizeof(username));
	trap->Argv(2, password, sizeof(password));

	Q_StripColor(username);
	Q_strlwr(username);

	if (select_number_of_accounts_with_username(ent, username, db, zErrMsg, rc, stmt) == 0)
	{
		//The account does not exist, thus, the error does.
		trap->SendServerCommand(ent - g_entities, va("print \"^1An account with the username %s does not exist.\n\"", username));
		trap->SendServerCommand(ent - g_entities, va("cp \"^1An account with the username %s does not exist.\n\"", username));

		sqlite3_close(db);
		return;
	}
	
	if (is_password_correct(ent, username, password, db, zErrMsg, rc, stmt) == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Incorrect password.\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^1Incorrect password.\n\"");

		sqlite3_close(db);
		return;
	}

	// GalaxyRP fix: [Account] this used to call G_Kill() first and load the account/character data
	// afterward -- so the kill (and whatever respawn eventually followed it) always ran against the
	// *previous* amrpgmode/skill state, relying entirely on the deferred respawn cycle to pick up the
	// newly loaded account once select_account_and_default_character_data() below had finished. Reordered
	// so the account data (and the amrpgmode = 2 it ends with) is loaded first, then applied synchronously
	// via initialize_rpg_skills() below, and only then does the kill run -- same borrowed-from-a-newer-
	// fork pattern as select_player_character() above, for the same reason (G_Kill() silently no-ops
	// while paralyzed or mid-duel with g_allowDuelSuicide off, and even when it doesn't, its respawn is
	// deferred, leaving stale force powers/weapons equipped in the meantime).
	select_account_and_default_character_data(ent, username, db, zErrMsg, rc, stmt);

	initialize_rpg_skills(ent);

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
		// GalaxyRP fix: [Model] don't call G_Kill() in the same frame as the model change performed by
		// select_account_and_default_character_data() above -- see pending_relog_kill_time's declaration
		// in g_local.h for why (T-pose race with the client's asynchronous model/animation reload).
		// ClientThink_real() in g_active.c fires the actual kill once this buffer elapses.
		ent->client->pers.pending_relog_kill_time = level.time + 300;
	}

	trap->SendServerCommand(ent - g_entities, "print \"^2You have sucessfully logged in.\n\"");
	trap->SendServerCommand(ent - g_entities, "cp \"^2You have sucessfully logged in.\n\"");

	sqlite3_close(db);

	Cmd_ZykChars_f(ent);
	Cmd_GalaxyRpUi_f(ent);

	return;
}

// GalaxyRP: [Char fix] full audit/rewrite. Previously this command printed no usage tip at all for
// any malformed invocation -- a bare "/char new" with no name, an unrecognized subcommand, or extra
// arguments were all silently ignored -- unlike every other account command in this file (/login,
// /new, /logout, /changepassword), which all print a "Command Usage: ..." hint the moment their
// arguments don't parse. It also leaked its sqlite3 database handle on those same malformed-
// invocation paths (the argc==1 branch returned without ever calling sqlite3_close(), and anything
// that fell through both "if" blocks skipped the close entirely), and never called
// Cmd_ZykChars_f() after removing a character, so a removed character kept showing in that
// player's own character-select UI until something else happened to refresh it. Most importantly,
// unlike /logout (which calls save_account(ent, qtrue) as its very first statement), it never
// flushed the player's *currently loaded* character to the database before switching away from it,
// creating a new one, or removing one -- so any progress made since the last incidental save point
// (a purchase, a /settings toggle, a race win, etc. -- there is no periodic autosave in this
// codebase) was silently discarded the moment select_player_character() overwrote
// ent->client->pers with a different character's data. save_account(ent, qtrue) is now called up
// front, exactly like /logout does, before any subcommand runs.
void Cmd_Char_f(gentity_t *ent) {
	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = 0;
	int argc = trap->Argc();
	char command[MAX_STRING_CHARS];
	char charName[MAX_STRING_CHARS];

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	// GalaxyRP: [Char fix] flush whatever character is currently active before this command does
	// anything that might change or replace it -- see the function-level comment above.
	save_account(ent, qtrue);

	if (argc == 1)
	{
		select_character_list(ent, db, zErrMsg, rc, stmt);
		sqlite3_close(db);
		return;
	}
	if (argc == 3)
	{
		trap->Argv(1, command, sizeof(command));
		trap->Argv(2, charName, sizeof(charName));

		//Create New Character
		if (Q_stricmp(command, "new") == 0) {
			// GalaxyRP fix: [Char] /char new used to only insert the new character's DB rows and leave
			// the player on whatever character they already had loaded -- they had to separately run
			// /char use <name> to actually start playing it, unlike the reference mod's equivalent
			// command, which switches to the newly created character immediately. Do the same here:
			// on a successful creation, immediately switch onto it the same way /char use does (full
			// data load, synchronous stat/gear apply, and the same kill+respawn), instead of leaving
			// the new character sitting unused in the character list. announce_switch is qfalse here
			// because the combined "created and now using" broadcast below already covers what
			// select_player_character()'s own "switched to" broadcast would otherwise say a second time.
			if (create_new_character(ent, charName, db, zErrMsg, rc, stmt)) {
				trap->SendServerCommand(-1, va("chat \"%s created a new character and is now using: %s\n\"", ent->client->pers.netname, charName));
				select_player_character(ent, charName, db, zErrMsg, rc, stmt, qfalse);
				Cmd_GalaxyRpUi_f(ent);
			}
			sqlite3_close(db);
			Cmd_ZykChars_f(ent);
			return;
		}

		//Switch character
		if (Q_stricmp(command, "use") == 0) {
			// GalaxyRP fix: [Char] mirrors the same "already active" guard remove_character() has
			// (further down) -- without it, a player could re-select the character they already have
			// loaded, which just wastefully re-runs the whole load path (DB re-query, stat/gear
			// reapply, kill+respawn) for no actual change.
			if (Q_stricmp(charName, ent->client->sess.rpgchar) == 0) {
				trap->SendServerCommand(ent - g_entities, va("print \"^1You are already using character %s.\n\"", charName));
				sqlite3_close(db);
				return;
			}

			select_player_character(ent, charName, db, zErrMsg, rc, stmt, qtrue);
			sqlite3_close(db);

			Cmd_ZykChars_f(ent);
			Cmd_GalaxyRpUi_f(ent);

			return;

		}

		//Remove character
		if (Q_stricmp(command, "remove") == 0) {

			remove_character(ent, charName, db, zErrMsg, rc, stmt);
			sqlite3_close(db);
			Cmd_ZykChars_f(ent);
			return;
		}
	}

	// GalaxyRP: [Char fix] anything else -- no arguments beyond "/char" itself is handled above, so
	// reaching here means an unrecognized subcommand, a missing character name, or extra arguments.
	// Print the same usage tip every other malformed invocation in this file gets, instead of
	// silently doing nothing.
	sqlite3_close(db);
	trap->SendServerCommand(ent - g_entities, "print \"^2Command Usage: /char <new/use/remove> <character name>. Run with no arguments to list your characters.\n\"");
}

//INVENTORY

qboolean inventory_does_player_own_item(gentity_t *ent, int itemID, sqlite3 *db, char *zErrMsg, int rc, sqlite3_stmt *stmt)
{
	rc = sqlite3_prepare(db, va("SELECT count(ItemID) FROM Items WHERE ItemID='%i' AND CharID='%i'", itemID, ent->client->pers.CharID), -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return qfalse;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return qfalse;
	}
	if (rc == SQLITE_ROW)
	{
		int numberOfItems;

		numberOfItems = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);

		if (numberOfItems != 1) {
			trap->SendServerCommand(ent - g_entities, "print \"^2You do not own this item.\n\"");
			trap->SendServerCommand(ent - g_entities, "cp \"^2You do not own this item.\n\"");

			return qfalse;
		}
		return qtrue;
	}

	return qfalse;
}

void inventory_display_beginning(gentity_t *ent) {
	trap->SendServerCommand(ent->s.number, "print \"^2Inventory\n\"");
	trap->SendServerCommand(ent->s.number, va("print \"^3[Credits: %i]\n\"", ent->client->pers.credits));
	trap->SendServerCommand(ent->s.number, "print \"^2================================================================================\n\"");
}

void inventory_display_end(gentity_t *ent) {
	trap->SendServerCommand(ent->s.number, "print \"^2================================================================================\n\"");
}

// GalaxyRP fix: [security] this used to build its INSERT via va("...VALUES('%i',\"%s\")"...) with the
// item name spliced straight into the query text -- unlike its siblings update_chars_table_row_with_
// current_values() and insert_inv_table_row(), which were already hardened the same way, this one was
// missed. A name containing a double quote (e.g. /createitem Vader"s Saber) broke the query outright
// with a silently-swallowed SQL error, and a deliberately crafted name could inject arbitrary SQL.
// Bound as a parameter instead. Also now returns qboolean so Cmd_CreateItem_f can tell whether the
// item was actually created before logging it as created.
qboolean inventory_add_item(gentity_t *ent, char item_to_add[MAX_STRING_CHARS], sqlite3 *db, char *zErrMsg, int rc, sqlite3_stmt *stmt) {
	rc = sqlite3_prepare(db, "INSERT INTO Items(CharID, ItemName) VALUES(?, ?)", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return qfalse;
	}
	sqlite3_bind_int(stmt, 1, ent->client->pers.CharID);
	sqlite3_bind_text(stmt, 2, item_to_add, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		return qfalse;
	}
	trap->SendServerCommand(ent->s.number, "print \"Item added to your inventory.\n\"");

	return qtrue;
}

void inventory_remove_item(gentity_t *ent, int id_to_be_removed, sqlite3 *db, char *zErrMsg, int rc, sqlite3_stmt *stmt) {

	if (inventory_does_player_own_item(ent, id_to_be_removed, db, zErrMsg, rc, stmt) == qfalse) {
		return;
	}

	//trap->Print(va("DELETE FROM Items WHERE ItemID='%i'", id_to_be_removed));
	rc = sqlite3_exec(db, va("DELETE FROM Items WHERE ItemID='%i'", id_to_be_removed), 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return;
	}
	trap->SendServerCommand(ent->s.number, "print \"Item was removed.\n\"");

	return;
}

void inventory_transfer_item(gentity_t *ent, gentity_t *otherEnt, int itemID, sqlite3 *db, char *zErrMsg, int rc, sqlite3_stmt *stmt) {
	//trap->Print(va("UPDATE Items SET CharID='%i' WHERE ItemID='%i'", otherEnt->client->pers.CharID, itemID));
	rc = sqlite3_exec(db, va("UPDATE Items SET CharID='%i' WHERE ItemID='%i'", otherEnt->client->pers.CharID, itemID), 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return;
	}

	trap->SendServerCommand(ent->s.number, "print \"Item was transferred.\n\"");
}

void inventory_add_create_item_log(gentity_t *ent, char created_item_name[MAX_STRING_CHARS]) {
	FILE *log_file = NULL;

	log_file = fopen("GalaxyRP/logs/itemlog.txt", "a+");

	// GalaxyRP fix: [Items] fopen() can return NULL (e.g. if GalaxyRP/logs/ doesn't exist yet) --
	// fputs()/fclose() on a NULL FILE* is undefined behavior. Skip logging instead of crashing.
	if (log_file == NULL) {
		trap->Print("Warning: could not open GalaxyRP/logs/itemlog.txt for writing.\n");
		return;
	}

	fputs(va("%s created the item: %s\n", ent->client->pers.netname, created_item_name), log_file);
	fclose(log_file);

	return;
}

void inventory_display_items(gentity_t *ent, sqlite3 *db, char *zErrMsg, int rc, sqlite3_stmt *stmt)
{
	//trap->Print(va("SELECT Count(ItemID) FROM Items WHERE CharID='%i'", ent->client->pers.CharID));
	rc = sqlite3_prepare(db, va("SELECT Count(ItemID) FROM Items WHERE CharID='%i'", ent->client->pers.CharID), -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	if (rc == SQLITE_ROW)
	{
		int itemCount;
		itemCount = sqlite3_column_int(stmt, 0);

		if (itemCount == 0) {
			trap->SendServerCommand(ent->s.number, "print \"Nothing to show.\n\"");
			sqlite3_finalize(stmt);
			return;
		}
	}
	sqlite3_finalize(stmt);

	//trap->Print(va("SELECT ItemID, ItemName FROM Items WHERE CharID='%i'", ent->client->pers.CharID));
	rc = sqlite3_prepare(db, va("SELECT ItemID, ItemName FROM Items WHERE CharID='%i'", ent->client->pers.CharID), -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	while (rc == SQLITE_ROW) {
		int itemID;
		char item[MAX_STRING_CHARS];
		itemID = sqlite3_column_int(stmt, 0);
		strcpy(item, sqlite3_column_text(stmt, 1));

		trap->SendServerCommand(ent - g_entities, va("print \"^3%i. ^2%s\n\"", itemID, item));
		rc = sqlite3_step(stmt);
	}

	sqlite3_finalize(stmt);
}

/*
==================
Cmd_Inventory_f
==================
*/
void Cmd_Inventory_f(gentity_t *ent) {
	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = 0;
	char username[256] = { 0 }, password[256] = { 0 }, comparisonUsername[256] = { 0 }, comparisonPassword[256] = { 0 }, defaultChar[256] = { 0 };

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	inventory_display_beginning(ent);
	inventory_display_items(ent, db, zErrMsg, rc, stmt);
	inventory_display_end(ent);
	sqlite3_close(db);
	return;
}

/*
==================
Cmd_CreateItem_f
==================
*/
void Cmd_CreateItem_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_CREATEITEM, qtrue))
	{
		return;
	}

	if (trap->Argc() != 2) {
		trap->SendServerCommand(ent->s.number, "print \"Usage: /createitem <itemname>\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = 0;
	char username[256] = { 0 }, password[256] = { 0 }, comparisonUsername[256] = { 0 }, comparisonPassword[256] = { 0 }, defaultChar[256] = { 0 };

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	// GalaxyRP fix: [Items] inventory_add_item() now reports whether the insert actually succeeded --
	// only log the creation to itemlog.txt when it did, instead of unconditionally claiming success
	// even when the SQL failed.
	if (inventory_add_item(ent, arg1, db, zErrMsg, rc, stmt) == qtrue) {
		inventory_add_create_item_log(ent, arg1);
	}

	sqlite3_close(db);

	return;
}

/*
==================
Cmd_TrashItem_f
==================
*/
void Cmd_TrashItem_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];

	if (trap->Argc() != 2) {
		trap->SendServerCommand(ent->s.number, "print \"Usage: /trashitem <itemid>\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));
	int id_to_be_removed = atoi(arg1);

	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = 0;
	char username[256] = { 0 }, password[256] = { 0 }, comparisonUsername[256] = { 0 }, comparisonPassword[256] = { 0 }, defaultChar[256] = { 0 };

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	inventory_remove_item(ent, id_to_be_removed, db, zErrMsg, rc, stmt);

	sqlite3_close(db);

	return;
}

/*
==================
Cmd_GiveItem_f
==================
*/
void Cmd_GiveItem_f(gentity_t *ent) {
	char player_name[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];

	if (trap->Argc() != 3) {
		trap->SendServerCommand(ent->s.number, "print \"Usage: /giveitem <itemid> <playerName>\n\"");
		return;
	}
	trap->Argv(1, arg2, sizeof(arg2));
	trap->Argv(2, player_name, sizeof(player_name));

	int item_id = atoi(arg2);

	int player_id = ClientNumberFromString(ent, player_name, qfalse);

	//player not found, no point in going on
	if (player_id == -1) {
		return;
	}

	// GalaxyRP fix: [Items] a connected-but-not-yet-logged-in player has pers.CharID == 0 -- the whole
	// client struct is zeroed on connect (see ClientConnect), and real CharIDs are auto-assigned
	// starting at 1, so 0 is never a real character. Giving an item to someone still at the login
	// screen silently reassigned it to CharID 0, orphaning it permanently -- no /inventory command
	// will ever show it again. Reject the transfer instead of letting it through.
	if (g_entities[player_id].client->sess.loggedin != qtrue) {
		trap->SendServerCommand(ent->s.number, "print \"That player is not logged into an account.\n\"");
		return;
	}

	// GalaxyRP fix: [cleanup] dropped the stray & -- g_entities[player_id].client->ps.origin
	// is already a vec3_t (decays to float*); &... instead produced a pointer-to-array
	// (float(*)[3]), which MSVC flagged (C4047/C4024) as a type mismatch against Distance()'s
	// const float* parameter. Numerically identical address either way, so purely cosmetic.
	if (Distance(ent->client->ps.origin, g_entities[player_id].client->ps.origin) > 1000) {
		trap->SendServerCommand(ent->s.number, "print \"You are too far away from that person.\n\"");
		return;
	}

	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = 0;
	char username[256] = { 0 }, password[256] = { 0 }, comparisonUsername[256] = { 0 }, comparisonPassword[256] = { 0 }, defaultChar[256] = { 0 };

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	// GalaxyRP fix: this used to pass "&stmt" (sqlite3_stmt **) where the function expects a plain
	// sqlite3_stmt * -- a type mismatch that happened to be harmless here (the incoming stmt value
	// is overwritten before it's ever read), but is still wrong and worth cleaning up. Also, on the
	// "player doesn't own this item" path this returned without calling sqlite3_close(db), leaking
	// a database connection on every failed /giveitem attempt; closing it here fixes that leak too.
	if (inventory_does_player_own_item(ent, item_id, db, zErrMsg, rc, stmt) == qfalse) {
		sqlite3_close(db);
		return;
	}

	inventory_transfer_item(ent, &g_entities[player_id], item_id, db, zErrMsg, rc, stmt);

	trap->SendServerCommand(ent->s.number, va("print \"^2You've given an item to %s^2\n\"", &g_entities[player_id].client->pers.netname));

	sqlite3_close(db); // GalaxyRP fix: this success path never closed the connection it opened above.

	return;
}

void Cmd_KillOther_f( gentity_t *ent )
{
	int			i;
	char		otherindex[MAX_TOKEN_CHARS];
	gentity_t	*otherEnt = NULL;

	// GalaxyRP fix: [Admin] /killother used to gate on ADM_GIVEADM ("Give Admin") -- an unrelated,
	// more sensitive flag meant for granting/revoking other players' admin commands -- instead of a
	// permission of its own. It also had no bit in the ADM_NUM_CMDS table at all, so it couldn't be
	// granted or revoked independently and never showed up in /adminlist. Share ADM_KICK with
	// /admkick instead: instantly killing a player is the same severity of action as kicking them,
	// and this way both commands are granted/revoked together as one permission.
	if (!check_admin_command(ent, ADM_KICK, qtrue))
	{
		return;
	}

	if (trap->Argc() < 2) {
		trap->SendServerCommand(ent - g_entities, "print \"Usage: killother <player id>\n\"");
		return;
	}

	trap->Argv(1, otherindex, sizeof(otherindex));
	i = ClientNumberFromString(ent, otherindex, qfalse);
	if (i == -1) {
		return;
	}

	otherEnt = &g_entities[i];
	if (!otherEnt->inuse || !otherEnt->client) {
		return;
	}

	if ((otherEnt->health <= 0 || otherEnt->client->tempSpectate >= level.time || otherEnt->client->sess.sessionTeam == TEAM_SPECTATOR))
	{
		// Intentionally displaying for the command user
		trap->SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEALIVE")));
		return;
	}

	G_Kill(otherEnt);
}

/*
=================
BroadCastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange( gclient_t *client, int oldTeam )
{
	client->ps.fd.forceDoInit = 1; //every time we change teams make sure our force powers are set right

	if (level.gametype == GT_SIEGE)
	{ //don't announce these things in siege
		return;
	}

	if ( client->sess.sessionTeam == TEAM_RED ) {
		trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
			client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEREDTEAM")) );
	} else if ( client->sess.sessionTeam == TEAM_BLUE ) {
		trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
		client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBLUETEAM")));
	} else if ( client->sess.sessionTeam == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
		trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
		client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHESPECTATORS")));
	} else if ( client->sess.sessionTeam == TEAM_FREE ) {
		trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
		client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBATTLE")));
	}

	G_LogPrintf( "ChangeTeam: %i [%s] (%s) \"%s^7\" %s -> %s\n", (int)(client - level.clients), client->sess.IP, client->pers.guid, client->pers.netname, TeamName( oldTeam ), TeamName( client->sess.sessionTeam ) );
}

qboolean G_PowerDuelCheckFail(gentity_t *ent)
{
	int			loners = 0;
	int			doubles = 0;

	if (!ent->client || ent->client->sess.duelTeam == DUELTEAM_FREE)
	{
		return qtrue;
	}

	G_PowerDuelCount(&loners, &doubles, qfalse);

	if (ent->client->sess.duelTeam == DUELTEAM_LONE && loners >= 1)
	{
		return qtrue;
	}

	if (ent->client->sess.duelTeam == DUELTEAM_DOUBLE && doubles >= 2)
	{
		return qtrue;
	}

	return qfalse;
}

/*
=================
SetTeam
=================
*/
qboolean g_dontPenalizeTeam = qfalse;
qboolean g_preventTeamBegin = qfalse;
void SetTeam( gentity_t *ent, char *s ) {
	int					team, oldTeam;
	gclient_t			*client;
	int					clientNum;
	spectatorState_t	specState;
	int					specClient;
	int					teamLeader;

	// fix: this prevents rare creation of invalid players
	if (!ent->inuse)
	{
		return;
	}

	//
	// see what change is requested
	//
	client = ent->client;

	clientNum = client - level.clients;
	specClient = 0;
	specState = SPECTATOR_NOT;
	if ( !Q_stricmp( s, "scoreboard" ) || !Q_stricmp( s, "score" )  ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FREE; // SPECTATOR_SCOREBOARD disabling this for now since it is totally broken on client side
	} else if ( !Q_stricmp( s, "follow1" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -1;
	} else if ( !Q_stricmp( s, "follow2" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -2;
	} else if ( !Q_stricmp( s, "spectator" ) || !Q_stricmp( s, "s" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FREE;
	} else if ( level.gametype >= GT_TEAM ) {
		// if running a team game, assign player to one of the teams
		specState = SPECTATOR_NOT;
		if ( !Q_stricmp( s, "red" ) || !Q_stricmp( s, "r" ) ) {
			team = TEAM_RED;
		} else if ( !Q_stricmp( s, "blue" ) || !Q_stricmp( s, "b" ) ) {
			team = TEAM_BLUE;
		} else {
			// pick the team with the least number of players
			//For now, don't do this. The legalize function will set powers properly now.
			/*
			if (g_forceBasedTeams.integer)
			{
				if (ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
				{
					team = TEAM_BLUE;
				}
				else
				{
					team = TEAM_RED;
				}
			}
			else
			{
			*/
				team = PickTeam( clientNum );
			//}
		}

		if ( g_teamForceBalance.integer && !g_jediVmerc.integer ) {
			int		counts[TEAM_NUM_TEAMS];

			//JAC: Invalid clientNum was being used
			counts[TEAM_BLUE] = TeamCount( ent-g_entities, TEAM_BLUE );
			counts[TEAM_RED] = TeamCount( ent-g_entities, TEAM_RED );

			// We allow a spread of two
			if ( team == TEAM_RED && counts[TEAM_RED] - counts[TEAM_BLUE] > 1 ) {
				//For now, don't do this. The legalize function will set powers properly now.
				/*
				if (g_forceBasedTeams.integer && ent->client->ps.fd.forceSide == FORCE_DARKSIDE)
				{
					trap->SendServerCommand( ent->client->ps.clientNum,
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED_SWITCH")) );
				}
				else
				*/
				{
					//JAC: Invalid clientNum was being used
					trap->SendServerCommand( ent-g_entities,
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED")) );
				}
				return; // ignore the request
			}
			if ( team == TEAM_BLUE && counts[TEAM_BLUE] - counts[TEAM_RED] > 1 ) {
				//For now, don't do this. The legalize function will set powers properly now.
				/*
				if (g_forceBasedTeams.integer && ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
				{
					trap->SendServerCommand( ent->client->ps.clientNum,
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE_SWITCH")) );
				}
				else
				*/
				{
					//JAC: Invalid clientNum was being used
					trap->SendServerCommand( ent-g_entities,
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE")) );
				}
				return; // ignore the request
			}

			// It's ok, the team we are switching to has less or same number of players
		}

		//For now, don't do this. The legalize function will set powers properly now.
		/*
		if (g_forceBasedTeams.integer)
		{
			if (team == TEAM_BLUE && ent->client->ps.fd.forceSide != FORCE_LIGHTSIDE)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBELIGHT")) );
				return;
			}
			if (team == TEAM_RED && ent->client->ps.fd.forceSide != FORCE_DARKSIDE)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEDARK")) );
				return;
			}
		}
		*/

	} else {
		// force them to spectators if there aren't any spots free
		team = TEAM_FREE;
	}

	oldTeam = client->sess.sessionTeam;

	if (level.gametype == GT_SIEGE)
	{
		if (client->tempSpectate >= level.time &&
			team == TEAM_SPECTATOR)
		{ //sorry, can't do that.
			return;
		}

		if ( team == oldTeam && team != TEAM_SPECTATOR )
			return;

		client->sess.siegeDesiredTeam = team;
		//oh well, just let them go.
		/*
		if (team != TEAM_SPECTATOR)
		{ //can't switch to anything in siege unless you want to switch to being a fulltime spectator
			//fill them in on their objectives for this team now
			trap->SendServerCommand(ent-g_entities, va("sb %i", client->sess.siegeDesiredTeam));

			trap->SendServerCommand( ent-g_entities, va("print \"You will be on the selected team the next time the round begins.\n\"") );
			return;
		}
		*/
		if (client->sess.sessionTeam != TEAM_SPECTATOR &&
			team != TEAM_SPECTATOR)
		{ //not a spectator now, and not switching to spec, so you have to wait til you die.
			//trap->SendServerCommand( ent-g_entities, va("print \"You will be on the selected team the next time you respawn.\n\"") );
			qboolean doBegin;
			if (ent->client->tempSpectate >= level.time)
			{
				doBegin = qfalse;
			}
			else
			{
				doBegin = qtrue;
			}

			if (doBegin)
			{
				// Kill them so they automatically respawn in the team they wanted.
				if (ent->health > 0)
				{
					ent->flags &= ~FL_GODMODE;
					ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
					player_die( ent, ent, ent, 100000, MOD_TEAM_CHANGE );
				}
			}

			if (ent->client->sess.sessionTeam != ent->client->sess.siegeDesiredTeam)
			{
				SetTeamQuick(ent, ent->client->sess.siegeDesiredTeam, qfalse);
			}

			return;
		}
	}

	// override decision if limiting the players
	if ( (level.gametype == GT_DUEL)
		&& level.numNonSpectatorClients >= 2 )
	{
		team = TEAM_SPECTATOR;
	}
	else if ( (level.gametype == GT_POWERDUEL)
		&& (level.numPlayingClients >= 3 || G_PowerDuelCheckFail(ent)) )
	{
		team = TEAM_SPECTATOR;
	}
	else if ( g_maxGameClients.integer > 0 &&
		level.numNonSpectatorClients >= g_maxGameClients.integer )
	{
		team = TEAM_SPECTATOR;
	}

	//
	// decide if we will allow the change
	//
	if ( team == oldTeam && team != TEAM_SPECTATOR ) {
		return;
	}

	//
	// execute the team change
	//

	//If it's siege then show the mission briefing for the team you just joined.
//	if (level.gametype == GT_SIEGE && team != TEAM_SPECTATOR)
//	{
//		trap->SendServerCommand(clientNum, va("sb %i", team));
//	}

	// if the player was dead leave the body
	if ( client->ps.stats[STAT_HEALTH] <= 0 && client->sess.sessionTeam != TEAM_SPECTATOR ) {
		MaintainBodyQueue(ent);
	}

	// he starts at 'base'
	client->pers.teamState.state = TEAM_BEGIN;
	if ( oldTeam != TEAM_SPECTATOR ) {
		// Kill him (makes sure he loses flags, etc)
		ent->flags &= ~FL_GODMODE;
		ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
		g_dontPenalizeTeam = qtrue;
		player_die (ent, ent, ent, 100000, MOD_SUICIDE);
		g_dontPenalizeTeam = qfalse;

	}
	// they go to the end of the line for tournaments
	if ( team == TEAM_SPECTATOR && oldTeam != team )
		AddTournamentQueue( client );

	// clear votes if going to spectator (specs can't vote)
	if ( team == TEAM_SPECTATOR )
		G_ClearVote( ent );
	// also clear team votes if switching red/blue or going to spec
	G_ClearTeamVote( ent, oldTeam );

	client->sess.sessionTeam = (team_t)team;
	client->sess.spectatorState = specState;
	client->sess.spectatorClient = specClient;

	client->sess.teamLeader = qfalse;
	if ( team == TEAM_RED || team == TEAM_BLUE ) {
		teamLeader = TeamLeader( team );
		// if there is no team leader or the team leader is a bot and this client is not a bot
		if ( teamLeader == -1 || ( !(g_entities[clientNum].r.svFlags & SVF_BOT) && (g_entities[teamLeader].r.svFlags & SVF_BOT) ) ) {
			//SetLeader( team, clientNum );
		}
	}
	// make sure there is a team leader on the team the player came from
	if ( oldTeam == TEAM_RED || oldTeam == TEAM_BLUE ) {
		CheckTeamLeader( oldTeam );
	}

	BroadcastTeamChange( client, oldTeam );

	//make a disappearing effect where they were before teleporting them to the appropriate spawn point,
	//if we were not on the spec team
	if (oldTeam != TEAM_SPECTATOR)
	{
		gentity_t *tent = G_TempEntity( client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = clientNum;
	}

	// get and distribute relevent paramters
	if ( !ClientUserinfoChanged( clientNum ) )
		return;

	if (!g_preventTeamBegin && level.load_entities_timer == 0)
	{ // zyk: do not call this while entities are being placed in map
		ClientBegin( clientNum, qfalse );
	}
}

/*
=================
StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
extern void G_LeaveVehicle( gentity_t *ent, qboolean ConCheck );
void StopFollowing( gentity_t *ent ) {
	int i=0;
	ent->client->ps.persistant[ PERS_TEAM ] = TEAM_SPECTATOR;
	ent->client->sess.sessionTeam = TEAM_SPECTATOR;
	ent->client->sess.spectatorState = SPECTATOR_FREE;
	ent->client->ps.pm_flags &= ~PMF_FOLLOW;
	ent->r.svFlags &= ~SVF_BOT;
	ent->client->ps.clientNum = ent - g_entities;
	ent->client->ps.weapon = WP_NONE;
	G_LeaveVehicle( ent, qfalse ); // clears m_iVehicleNum as well
	ent->client->ps.emplacedIndex = 0;
	//ent->client->ps.m_iVehicleNum = 0;
	ent->client->ps.viewangles[ROLL] = 0.0f;
	ent->client->ps.forceHandExtend = HANDEXTEND_NONE;
	ent->client->ps.forceHandExtendTime = 0;
	ent->client->ps.zoomMode = 0;
	ent->client->ps.zoomLocked = qfalse;
	ent->client->ps.zoomLockTime = 0;
	ent->client->ps.saberMove = LS_NONE;
	ent->client->ps.legsAnim = 0;
	ent->client->ps.legsTimer = 0;
	ent->client->ps.torsoAnim = 0;
	ent->client->ps.torsoTimer = 0;
	ent->client->ps.isJediMaster = qfalse; // major exploit if you are spectating somebody and they are JM and you reconnect
	ent->client->ps.cloakFuel = 100; // so that fuel goes away after stop following them
	ent->client->ps.jetpackFuel = 100; // so that fuel goes away after stop following them
	ent->health = ent->client->ps.stats[STAT_HEALTH] = 100; // so that you don't keep dead angles if you were spectating a dead person
	ent->client->ps.bobCycle = 0;
	ent->client->ps.pm_type = PM_SPECTATOR;
	ent->client->ps.eFlags &= ~EF_DISINTEGRATION;
	for ( i=0; i<PW_NUM_POWERUPS; i++ )
		ent->client->ps.powerups[i] = 0;
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f( gentity_t *ent ) {
	int			oldTeam;
	char		s[MAX_TOKEN_CHARS];

	oldTeam = ent->client->sess.sessionTeam;

	if ( trap->Argc() != 2 ) {
		switch ( oldTeam ) {
		case TEAM_BLUE:
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTBLUETEAM")) );
			break;
		case TEAM_RED:
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTREDTEAM")) );
			break;
		case TEAM_FREE:
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTFREETEAM")) );
			break;
		case TEAM_SPECTATOR:
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTSPECTEAM")) );
			break;
		}
		return;
	}

	if ( ent->client->switchTeamTime > level.time ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	if (gEscaping)
	{
		return;
	}

	// if they are playing a tournament game, count as a loss
	if ( level.gametype == GT_DUEL
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {//in a tournament game
		//disallow changing teams
		trap->SendServerCommand( ent-g_entities, "print \"Cannot switch teams in Duel\n\"" );
		return;
		//FIXME: why should this be a loss???
		//ent->client->sess.losses++;
	}

	if (level.gametype == GT_POWERDUEL)
	{ //don't let clients change teams manually at all in powerduel, it will be taken care of through automated stuff
		trap->SendServerCommand( ent-g_entities, "print \"Cannot switch teams in Power Duel\n\"" );
		return;
	}

	// Tr!Force: [Plugin] Don't allow
	if (rp_pluginRequired.integer == 2 && !ent->client->pers.clientPlugin)
	{
		ClientBegin(ent->s.number, qfalse);
		return;
	}
	else
	{
		trap->Argv( 1, s, sizeof( s ) );
	}

	SetTeam( ent, s );

	// fix: update team switch time only if team change really happend
	if (oldTeam != ent->client->sess.sessionTeam)
		ent->client->switchTeamTime = level.time + 5000;
}

/*
=================
Cmd_DuelTeam_f
=================
*/
void Cmd_DuelTeam_f(gentity_t *ent)
{
	int			oldTeam;
	char		s[MAX_TOKEN_CHARS];

	if (level.gametype != GT_POWERDUEL)
	{ //don't bother doing anything if this is not power duel
		return;
	}

	/*
	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You cannot change your duel team unless you are a spectator.\n\""));
		return;
	}
	*/

	if ( trap->Argc() != 2 )
	{ //No arg so tell what team we're currently on.
		oldTeam = ent->client->sess.duelTeam;
		switch ( oldTeam )
		{
		case DUELTEAM_FREE:
			trap->SendServerCommand( ent-g_entities, va("print \"None\n\"") );
			break;
		case DUELTEAM_LONE:
			trap->SendServerCommand( ent-g_entities, va("print \"Single\n\"") );
			break;
		case DUELTEAM_DOUBLE:
			trap->SendServerCommand( ent-g_entities, va("print \"Double\n\"") );
			break;
		default:
			break;
		}
		return;
	}

	if ( ent->client->switchDuelTeamTime > level.time )
	{ //debounce for changing
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	trap->Argv( 1, s, sizeof( s ) );

	oldTeam = ent->client->sess.duelTeam;

	if (!Q_stricmp(s, "free"))
	{
		ent->client->sess.duelTeam = DUELTEAM_FREE;
	}
	else if (!Q_stricmp(s, "single"))
	{
		ent->client->sess.duelTeam = DUELTEAM_LONE;
	}
	else if (!Q_stricmp(s, "double"))
	{
		ent->client->sess.duelTeam = DUELTEAM_DOUBLE;
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"'%s' not a valid duel team.\n\"", s) );
	}

	if (oldTeam == ent->client->sess.duelTeam)
	{ //didn't actually change, so don't care.
		return;
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{ //ok..die
		int curTeam = ent->client->sess.duelTeam;
		ent->client->sess.duelTeam = oldTeam;
		G_Damage(ent, ent, ent, NULL, ent->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
		ent->client->sess.duelTeam = curTeam;
	}
	//reset wins and losses
	ent->client->sess.wins = 0;
	ent->client->sess.losses = 0;

	//get and distribute relevent paramters
	if ( ClientUserinfoChanged( ent->s.number ) )
		return;

	ent->client->switchDuelTeamTime = level.time + 5000;
}

int G_TeamForSiegeClass(const char *clName)
{
	int i = 0;
	int team = SIEGETEAM_TEAM1;
	siegeTeam_t *stm = BG_SiegeFindThemeForTeam(team);
	siegeClass_t *scl;

	if (!stm)
	{
		return 0;
	}

	while (team <= SIEGETEAM_TEAM2)
	{
		scl = stm->classes[i];

		if (scl && scl->name[0])
		{
			if (!Q_stricmp(clName, scl->name))
			{
				return team;
			}
		}

		i++;
		if (i >= MAX_SIEGE_CLASSES || i >= stm->numClasses)
		{
			if (team == SIEGETEAM_TEAM2)
			{
				break;
			}
			team = SIEGETEAM_TEAM2;
			stm = BG_SiegeFindThemeForTeam(team);
			i = 0;
		}
	}

	return 0;
}

/*
=================
Cmd_SiegeClass_f
=================
*/
void Cmd_SiegeClass_f( gentity_t *ent )
{
	char className[64];
	int team = 0;
	int preScore;
	qboolean startedAsSpec = qfalse;

	if (level.gametype != GT_SIEGE)
	{ //classes are only valid for this gametype
		return;
	}

	if (!ent->client)
	{
		return;
	}

	if (trap->Argc() < 1)
	{
		return;
	}

	if ( ent->client->switchClassTime > level.time )
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCLASSSWITCH")) );
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		startedAsSpec = qtrue;
	}

	trap->Argv( 1, className, sizeof( className ) );

	team = G_TeamForSiegeClass(className);

	if (!team)
	{ //not a valid class name
		return;
	}

	if (ent->client->sess.sessionTeam != team)
	{ //try changing it then
		g_preventTeamBegin = qtrue;
		if (team == TEAM_RED)
		{
			SetTeam(ent, "red");
		}
		else if (team == TEAM_BLUE)
		{
			SetTeam(ent, "blue");
		}
		g_preventTeamBegin = qfalse;

		if (ent->client->sess.sessionTeam != team)
		{ //failed, oh well
			if (ent->client->sess.sessionTeam != TEAM_SPECTATOR ||
				ent->client->sess.siegeDesiredTeam != team)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCLASSTEAM")) );
				return;
			}
		}
	}

	//preserve 'is score
	preScore = ent->client->ps.persistant[PERS_SCORE];

	//Make sure the class is valid for the team
	BG_SiegeCheckClassLegality(team, className);

	//Set the session data
	strcpy(ent->client->sess.siegeClass, className);

	// get and distribute relevent paramters
	if ( !ClientUserinfoChanged( ent->s.number ) )
		return;

	if (ent->client->tempSpectate < level.time)
	{
		// Kill him (makes sure he loses flags, etc)
		if (ent->health > 0 && !startedAsSpec)
		{
			ent->flags &= ~FL_GODMODE;
			ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
			player_die (ent, ent, ent, 100000, MOD_SUICIDE);
		}

		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR || startedAsSpec)
		{ //respawn them instantly.
			ClientBegin( ent->s.number, qfalse );
		}
	}
	//set it back after we do all the stuff
	ent->client->ps.persistant[PERS_SCORE] = preScore;

	ent->client->switchClassTime = level.time + 5000;
}

/*
=================
Cmd_ForceChanged_f
=================
*/
void Cmd_ForceChanged_f( gentity_t *ent )
{
	char fpChStr[1024];
	const char *buf;
//	Cmd_Kill_f(ent);
	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{ //if it's a spec, just make the changes now
		//trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "FORCEAPPLIED")) );
		//No longer print it, as the UI calls this a lot.
		WP_InitForcePowers( ent );
		goto argCheck;
	}

	buf = G_GetStringEdString("MP_SVGAME", "FORCEPOWERCHANGED");

	strcpy(fpChStr, buf);

	trap->SendServerCommand( ent-g_entities, va("print \"%s%s\n\"", S_COLOR_GREEN, fpChStr) );

	ent->client->ps.fd.forceDoInit = 1;
argCheck:
	if (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
	{ //If this is duel, don't even bother changing team in relation to this.
		return;
	}

	if (trap->Argc() > 1)
	{
		char	arg[MAX_TOKEN_CHARS];

		trap->Argv( 1, arg, sizeof( arg ) );

		if ( arg[0] )
		{ //if there's an arg, assume it's a combo team command from the UI.
			Cmd_Team_f(ent);
		}
	}
}

extern qboolean duel_tournament_is_duelist(gentity_t *ent);

// GalaxyRP: [Force] shared restriction check for Cmd_UpdateForce_f() below -- mirrors
// saber_switch_allowed() further down in this file (see update_saber()/Cmd_UpdateSaber_f()), same
// three checks (private duel, Duel Tournament duelist, boss battle), kept as its own small
// function instead of reusing that one so its name and message stay force-specific. Also gates on
// the player being logged out: a logged-in (RPG mode) player's force powers come from their
// account's database-driven skill levels instead of the "forcepowers" userinfo string WP_InitForcePowers()
// reads (see the "zyk: resetting force powers" WP_InitForcePowers() call in the logout handler
// above, which is what hands a player back to the userinfo-driven system once they log out), so
// re-applying that string here would be meaningless -- and, since amrpgmode is only ever set to 2
// as part of logging in, this also makes the boss-battle check below unreachable in practice, but
// it's kept for the same defense-in-depth reason update_saber()'s checks are unconditional.
static qboolean force_switch_allowed(gentity_t* ent)
{
	if (ent->client->sess.loggedin == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"This command is only available to players who are not logged into an account.\n\"");
		return qfalse;
	}

	if (ent->client->ps.duelInProgress == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use this command in private duels.\n\"");
		return qfalse;
	}

	if (level.duel_tournament_mode == 4 && duel_tournament_is_duelist(ent) == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use this command while duelling in Duel Tournament.\n\"");
		return qfalse;
	}

	// GalaxyRP fix: [Guardian] a guardian_mode>0 guard blocking this command in boss battles used to be
	// here. guardian_mode is permanently 0 now, so it was unreachable.

	return qtrue;
}

/*
==================
Cmd_UpdateForce_f

GalaxyRP: [Force] "/updateforce" -- takes no arguments. Re-applies whatever force power
allocation the player already picked in the in-game force power menu immediately, without needing
a /kill or death to force a respawn. Adapted from JA++'s japp_instantForceSwitch feature (see the
JA++ source at C:\Users\richa\TaystJK\japp-master, specifically Cmd_ForceChanged_f in g_cmds.cpp --
the command this mod's own, pre-existing Cmd_ForceChanged_f above is already modelled on). The
client's force power menu already calls that pre-existing "forcechanged" command automatically on
every change; for a non-spectator it just marks ps.fd.forceDoInit for ClientSpawn() to pick up on
the player's next respawn, which is the "requires /kill" behaviour this command is meant to skip.

Unlike a saber hilt, force powers need no diffing: WP_InitForcePowers() (in w_force.c) re-reads and
re-parses the player's own "forcepowers" userinfo cvar directly, so applying it here is just a
direct call -- clearing any pending forceDoInit afterwards the same way ClientSpawn() does, since
it's already been satisfied. See force_switch_allowed() above for the restriction checks (private
duel, Duel Tournament duelist, boss battle, and -- unlike /updatesaber -- logged-out players only);
no separate enable cvar, per design.
==================
*/
void Cmd_UpdateForce_f( gentity_t *ent ) {
	if (!force_switch_allowed(ent))
		return;

	WP_InitForcePowers(ent);
	ent->client->ps.fd.forceDoInit = 0;
}

extern qboolean WP_SaberStyleValidForSaber( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int saberAnimLevel );
extern qboolean WP_UseFirstValidSaberStyle( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int *saberAnimLevel );
qboolean G_SetSaber(gentity_t *ent, int saberNum, char *saberName, qboolean siegeOverride)
{
	char truncSaberName[MAX_QPATH] = {0};

	if ( !siegeOverride && level.gametype == GT_SIEGE && ent->client->siegeClass != -1 &&
		(bgSiegeClasses[ent->client->siegeClass].saberStance || bgSiegeClasses[ent->client->siegeClass].saber1[0] || bgSiegeClasses[ent->client->siegeClass].saber2[0]) )
	{ //don't let it be changed if the siege class has forced any saber-related things
		return qfalse;
	}

	Q_strncpyz( truncSaberName, saberName, sizeof( truncSaberName ) );

	if ( saberNum == 0 && (!Q_stricmp( "none", truncSaberName ) || !Q_stricmp( "remove", truncSaberName )) )
	{ //can't remove saber 0 like this
		Q_strncpyz( truncSaberName, DEFAULT_SABER, sizeof( truncSaberName ) );
	}

	//Set the saber with the arg given. If the arg is
	//not a valid sabername defaults will be used.
	WP_SetSaber( ent->s.number, ent->client->saber, saberNum, truncSaberName );

	if ( !ent->client->saber[0].model[0] )
	{
		assert(0); //should never happen!
		Q_strncpyz( ent->client->pers.saber1, DEFAULT_SABER, sizeof( ent->client->pers.saber1 ) );
	}
	else
		Q_strncpyz( ent->client->pers.saber1, ent->client->saber[0].name, sizeof( ent->client->pers.saber1 ) );

	if ( !ent->client->saber[1].model[0] )
		Q_strncpyz( ent->client->pers.saber2, "none", sizeof( ent->client->pers.saber2 ) );
	else
		Q_strncpyz( ent->client->pers.saber2, ent->client->saber[1].name, sizeof( ent->client->pers.saber2 ) );

	if ( !WP_SaberStyleValidForSaber( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, ent->client->ps.fd.saberAnimLevel ) )
	{
		WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &ent->client->ps.fd.saberAnimLevel );
		ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
	}

	return qtrue;
}

/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f( gentity_t *ent ) {
	int		i;
	char	arg[MAX_TOKEN_CHARS];

	if ( ent->client->sess.spectatorState == SPECTATOR_NOT && ent->client->switchTeamTime > level.time ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	if ( trap->Argc() != 2 ) {
		if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
			StopFollowing( ent );
		}
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );
	i = ClientNumberFromString( ent, arg, qfalse );
	if ( i == -1 ) {
		return;
	}

	// can't follow self
	if ( &level.clients[ i ] == ent->client ) {
		return;
	}

	// can't follow another spectator
	if ( level.clients[ i ].sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}

	if ( level.clients[ i ].tempSpectate >= level.time ) {
		return;
	}

	// if they are playing a tournament game, count as a loss
	if ( (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {
		//WTF???
		ent->client->sess.losses++;
	}

	// first set them to spectator
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		SetTeam( ent, "spectator" );
		// fix: update team switch time only if team change really happend
		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
			ent->client->switchTeamTime = level.time + 5000;
	}

	ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
	ent->client->sess.spectatorClient = i;
}

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f( gentity_t *ent, int dir ) {
	int		clientnum;
	int		original;
	qboolean	looped = qfalse;

	if ( ent->client->sess.spectatorState == SPECTATOR_NOT && ent->client->switchTeamTime > level.time ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	// Tr!Force: [Plugin] Don't allow
	if (rp_pluginRequired.integer == 2 && !ent->client->pers.clientPlugin) {
		return;
	}

	// if they are playing a tournament game, count as a loss
	if ( (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {\
		//WTF???
		ent->client->sess.losses++;
	}
	// first set them to spectator
	if ( ent->client->sess.spectatorState == SPECTATOR_NOT ) {
		SetTeam( ent, "spectator" );
		// fix: update team switch time only if team change really happend
		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
			ent->client->switchTeamTime = level.time + 5000;
	}

	if ( dir != 1 && dir != -1 ) {
		trap->Error( ERR_DROP, "Cmd_FollowCycle_f: bad dir %i", dir );
	}

	clientnum = ent->client->sess.spectatorClient;
	original = clientnum;

	do {
		clientnum += dir;
		if ( clientnum >= level.maxclients )
		{
			// Avoid /team follow1 crash
			if ( looped )
			{
				clientnum = original;
				break;
			}
			else
			{
				clientnum = 0;
				looped = qtrue;
			}
		}
		if ( clientnum < 0 ) {
			if ( looped )
			{
				clientnum = original;
				break;
			}
			else
			{
				clientnum = level.maxclients - 1;
				looped = qtrue;
			}
		}

		// can only follow connected clients
		if ( level.clients[ clientnum ].pers.connected != CON_CONNECTED ) {
			continue;
		}

		// can't follow another spectator
		if ( level.clients[ clientnum ].sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		// can't follow another spectator
		if ( level.clients[ clientnum ].tempSpectate >= level.time ) {
			return;
		}

		// this is good, we can use it
		ent->client->sess.spectatorClient = clientnum;
		ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
		return;
	} while ( clientnum != original );

	// leave it where it was
}

void Cmd_FollowNext_f( gentity_t *ent ) {
	Cmd_FollowCycle_f( ent, 1 );
}

void Cmd_FollowPrev_f( gentity_t *ent ) {
	Cmd_FollowCycle_f( ent, -1 );
}

extern void save_account(gentity_t *ent, qboolean save_char_file);

/*
==================
G_Say
==================
*/

static void G_SayTo( gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message, char *locMsg )
{
	if (!other) {
		return;
	}
	if (!other->inuse) {
		return;
	}
	if (!other->client) {
		return;
	}
	if ( other->client->pers.connected != CON_CONNECTED ) {
		return;
	}
	if ( mode == SAY_TEAM  && !OnSameTeam(ent, other) ) {
		return;
	}
	if ( mode == SAY_ALLY && ent != other && zyk_is_ally(ent, other) == qfalse) { // zyk: allychat. Send it only to allies and to the player himself
		return;
	}
	/*
	// no chatting to players in tournaments
	if ( (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
		&& other->client->sess.sessionTeam == TEAM_FREE
		&& ent->client->sess.sessionTeam != TEAM_FREE ) {
		//Hmm, maybe some option to do so if allowed?  Or at least in developer mode...
		return;
	}
	*/
	//They've requested I take this out.

	if (level.gametype == GT_SIEGE &&
		ent->client && (ent->client->tempSpectate >= level.time || ent->client->sess.sessionTeam == TEAM_SPECTATOR) &&
		other->client->sess.sessionTeam != TEAM_SPECTATOR &&
		other->client->tempSpectate < level.time)
	{ //siege temp spectators should not communicate to ingame players
		return;
	}

	// zyk: if player is ignored, then he cant say anything to the target player
	if ((ent->s.number < 31 && level.ignored_players[other->s.number][0] & (1 << ent->s.number)) || 
		(ent->s.number >= 31 && level.ignored_players[other->s.number][1] & (1 << (ent->s.number - 31))))
	{
		return;
	}

	if (locMsg)
	{
		trap->SendServerCommand( other-g_entities, va("%s \"%s\" \"%s\" \"%c\" \"%s\" %i",
			mode == SAY_TEAM ? "ltchat" : "lchat",
			name, locMsg, color, message, ent->s.number));
	}
	else
	{
		trap->SendServerCommand( other-g_entities, va("%s \"%s%c%c%s\" %i",
			mode == SAY_TEAM ? "tchat" : "chat",
			name, Q_COLOR_ESCAPE, color, message, ent->s.number));
	}
}

void delete_chat_command(char *original_text, int no_of_chars) {
	char text[MAX_SAY_TEXT] = "";
	for (int i = no_of_chars; i < strlen(original_text); i++) {
		strncat(text, &original_text[i], 1);
	}
	strcpy(original_text, text);
}

void G_Say( gentity_t *ent, gentity_t *target, int mode, const char *chatText ) {
	int			j;
	gentity_t	*other;
	int			color;
	char		name[64];
	// don't let text be too long for malicious reasons. Or let it be VERY long for RP purposes ;)
	char		text[MAX_SAY_TEXT];
	char		location[64];
	char		*locMsg = NULL;
	//distance for distance-based chat
	int distance = 999999999;
	//This is the limit where the chat text will stop appearing at
	int max_voice_distance = 600;
	//variable used for OOC chat (or team chat)
	int ooc_flag = 0;
	char ooc_text[700] = "";
	int broadcast_distance = 999999999;

	if ( level.gametype < GT_TEAM && mode == SAY_TEAM ) {
		ooc_flag = 1;
		mode = SAY_ALL;
	}

	Q_strncpyz( text, chatText, sizeof(text) );

	Q_strstrip( text, "\n\r", "  " );

	switch ( mode ) {
	default:
	case SAY_ALL:
		// zyk: if player is silenced by an admin, he cannot say anything
		if (ent->client->pers.player_statuses & (1 << 0))
			return;

		//ooc chat case
		if (ooc_flag == 1) 
		{
			//add paranthesis for OOC chat (I know it's a workaround and it should be done better but it works)
			char beginning[] = "((";
			char end[] = "^1))";

			strcat(ooc_text, beginning);
			strcat(ooc_text, text);
			strcat(ooc_text, end);

			ooc_text;

			G_LogPrintf("ooc: %s: %s\n", ent->client->pers.netname, text);
			Com_sprintf(name, sizeof(name), "%s%c%c"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
			color = COLOR_RED;

			break;
		}

		char *output = NULL;

		// these two have to be in the same order, one if the distance to the modifiers, so the order has to match
		// in chat_modifiers, shorter strings have to be AFTER the longer string (e.g. /me HAS to be AFTER /melong, otherwise it'll pick /me instead)

		char slash = '/';

		const char *ptr = strchr(text, slash);
		int index_of_slash = -1;
		if (ptr) {
			index_of_slash = ptr - text;
		}

		for (int i = 0; i < ARRAY_LEN(chat_modifiers); i++) {
			output = strstr(text, chat_modifiers[i].chat_modifier);

			
			if (output && index_of_slash == 0) {
				delete_chat_command(text, strlen(chat_modifiers[i].chat_modifier));
				G_LogPrintf(va("%s: %s: %s\n"), chat_modifiers[i].chat_modifier, ent->client->pers.netname, text);

				for (j = 0; j < level.numConnectedClients; j++) {

					other = &g_entities[j];
					if (Distance(ent->client->ps.origin, other->client->ps.origin) <= chat_modifiers[i].distance || other->client->pers.bitvalue & (1 << ADM_IGNORECHATDISTANCE) || other->client->sess.sessionTeam == TEAM_SPECTATOR)
					{
						trap->SendServerCommand(other->client->ps.clientNum, va(chat_modifiers[i].chat_format, ent->client->pers.netname, text));
					}
					else
						continue;
				}

				return;
			}
		}

		G_LogPrintf( "say: %s: %s\n", ent->client->pers.netname, text );
		Com_sprintf (name, sizeof(name), "%s%c%c"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		color = COLOR_GREEN;
		//set the desired distance here
		distance = 700;
		break;
	case SAY_TEAM:
		// zyk: if player is silenced by an admin, he cannot say anything
		if (ent->client->pers.player_statuses & (1 << 0))
			return;

		//This should be visible at all times
		G_LogPrintf( "sayteam: %s: %s\n", ent->client->pers.netname, ooc_text);
		if (Team_GetLocationMsg(ent, location, sizeof(location)))
		{
			Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC")"EC": ",
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
			locMsg = location;
		}
		else
		{
		Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC")"EC": ",
			ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		}
		color = COLOR_CYAN;
		break;
	case SAY_TELL:

		if (target && target->inuse && target->client && level.gametype >= GT_TEAM &&
			target->client->sess.sessionTeam == ent->client->sess.sessionTeam &&
			Team_GetLocationMsg(ent, location, sizeof(location)))
		{
			Com_sprintf (name, sizeof(name), EC"[%s%c%c"EC"]"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
			locMsg = location;
		}
		else
		{
			Com_sprintf (name, sizeof(name), EC"[%s%c%c"EC"]"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		}
		color = COLOR_MAGENTA;
		break;
	case SAY_ALLY: // zyk: say to allies
		// zyk: if player is silenced by an admin, he cannot say anything
		if (ent->client->pers.player_statuses & (1 << 0))
			return;

		G_LogPrintf( "sayally: %s: %s\n", ent->client->pers.netname, text );
		Com_sprintf (name, sizeof(name), EC"{%s%c%c"EC"}"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );

		color = COLOR_WHITE;
		break;
	}

	if ( target ) {
		G_SayTo( ent, target, mode, color, name, text, locMsg );
		return;
	}

	// echo the text to the console
	if ( dedicated.integer ) {
		trap->Print( "%s%s\n", name, text);
	}

	// send it to all the appropriate clients, within the desired distance
	for (j = 0; j < level.maxclients; j++) {
		other = &g_entities[j];
		//I know there can be a switch here, but i'm too lazy
		if (mode == SAY_ALL || mode == SAY_TEAM)
		{
			if (mode == SAY_ALL) {

				if (Distance(ent->client->ps.origin, other->client->ps.origin) <= distance || other->client->pers.bitvalue & (1 << ADM_IGNORECHATDISTANCE))
				{
					if (ooc_flag == 1) {
						G_SayTo(ent, other, mode, color, name, ooc_text, locMsg);
					}else
						G_SayTo(ent, other, mode, color, name, text, locMsg);
				}
				else
					continue;
			}
			else
				G_SayTo(ent, other, mode, color, name, ooc_text, locMsg);
		}
		else
			G_SayTo(ent, other, mode, color, name, text, locMsg);
	}
}


/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f( gentity_t *ent ) {
	char *p = NULL;

	if ( trap->Argc () < 2 )
		return;

	p = ConcatArgs( 1 );

	if ( strlen( p ) >= MAX_SAY_TEXT ) {
		p[MAX_SAY_TEXT-1] = '\0';
		G_SecurityLogPrintf( "Cmd_Say_f from %d (%s) has been truncated: %s\n", ent->s.number, ent->client->pers.netname, p );
	}

	G_Say( ent, NULL, SAY_ALL, p );
}

/*
==================
Cmd_SayTeam_f
==================
*/
static void Cmd_SayTeam_f( gentity_t *ent ) {
	char *p = NULL;

	if ( trap->Argc () < 2 )
		return;

	p = ConcatArgs( 1 );

	if ( strlen( p ) >= MAX_SAY_TEXT ) {
		p[MAX_SAY_TEXT-1] = '\0';
		G_SecurityLogPrintf( "Cmd_SayTeam_f from %d (%s) has been truncated: %s\n", ent->s.number, ent->client->pers.netname, p );
	}

	// zyk: if not in TEAM gametypes and player has allies, use allychat (SAY_ALLY) instead of SAY_ALL
	if (zyk_number_of_allies(ent,qfalse) > 0)
		G_Say( ent, NULL, (level.gametype>=GT_TEAM) ? SAY_TEAM : SAY_ALLY, p );
	else
		G_Say( ent, NULL, (level.gametype>=GT_TEAM) ? SAY_TEAM : SAY_TEAM, p );
}

/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f( gentity_t *ent ) {
	int			targetNum;
	gentity_t	*target;
	char		*p;
	char		arg[MAX_TOKEN_CHARS];

	if ( trap->Argc () < 3 ) {
		trap->SendServerCommand( ent-g_entities, "print \"Usage: tell <player id or name> <message>\n\"" );
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );
	targetNum = ClientNumberFromString( ent, arg, qfalse ); // zyk: changed this. Now it will use new function
	if ( targetNum == -1 ) {
		return;
	}

	target = &g_entities[targetNum];

	p = ConcatArgs( 2 );

	if ( strlen( p ) >= MAX_SAY_TEXT ) {
		p[MAX_SAY_TEXT-1] = '\0';
		G_SecurityLogPrintf( "Cmd_Tell_f from %d (%s) has been truncated: %s\n", ent->s.number, ent->client->pers.netname, p );
	}

	G_LogPrintf( "tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, p );
	G_Say( ent, target, SAY_TELL, p );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !(ent->r.svFlags & SVF_BOT)) {
		G_Say( ent, ent, SAY_TELL, p );
	}
}

//siege voice command
static void Cmd_VoiceCommand_f(gentity_t *ent)
{
	gentity_t *te;
	char arg[MAX_TOKEN_CHARS];
	char *s;
	char content[MAX_TOKEN_CHARS];
	int i = 0;

	/* zyk: now the voice commands will be allowed in all gametypes
	if (level.gametype < GT_TEAM)
	{
		return;
	}
	*/

	strcpy(content,"");

	if (trap->Argc() < 2)
	{
		// zyk: other gamemodes will show info of how to use the voice_cmd
		if (level.gametype < GT_TEAM)
		{
			for (i = 0; i < MAX_CUSTOM_SIEGE_SOUNDS; i++)
			{
				strcpy(content,va("%s%s\n",content,bg_customSiegeSoundNames[i]));
			}
			trap->SendServerCommand( ent-g_entities, va("print \"Usage: /voice_cmd <arg> [f]\nThe f argument is optional, it will make the command use female voice.\nThe arg may be one of the following:\n %s\"",content) );
		}
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR ||
		ent->client->tempSpectate >= level.time)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOICECHATASSPEC")) );
		return;
	}

	trap->Argv(1, arg, sizeof(arg));

	if (arg[0] == '*')
	{ //hmm.. don't expect a * to be prepended already. maybe someone is trying to be sneaky.
		return;
	}

	s = va("*%s", arg);

	//now, make sure it's a valid sound to be playing like this.. so people can't go around
	//screaming out death sounds or whatever.
	while (i < MAX_CUSTOM_SIEGE_SOUNDS)
	{
		if (!bg_customSiegeSoundNames[i])
		{
			break;
		}
		if (!Q_stricmp(bg_customSiegeSoundNames[i], s))
		{ //it matches this one, so it's ok
			break;
		}
		i++;
	}

	if (i == MAX_CUSTOM_SIEGE_SOUNDS || !bg_customSiegeSoundNames[i])
	{ //didn't find it in the list
		return;
	}

	if (level.gametype >= GT_TEAM)
	{
		te = G_TempEntity(vec3_origin, EV_VOICECMD_SOUND);
		te->s.groundEntityNum = ent->s.number;
		te->s.eventParm = G_SoundIndex((char *)bg_customSiegeSoundNames[i]);
		te->r.svFlags |= SVF_BROADCAST;
	}
	else
	{ // zyk: in other gamemodes that are not Team ones, just do a G_Sound call to each allied player
		char arg2[MAX_TOKEN_CHARS];
		char voice_dir[32];

		strcpy(voice_dir,"mp_generic_male");

		if (trap->Argc() == 3)
		{
			trap->Argv(2, arg2, sizeof(arg2));
			if (Q_stricmp(arg2, "f") == 0)
				strcpy(voice_dir,"mp_generic_female");
		}

		G_Sound(ent,CHAN_VOICE,G_SoundIndex(va("sound/chars/%s/misc/%s.mp3",voice_dir,arg)));

		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (zyk_is_ally(ent,&g_entities[i]) == qtrue)
			{
				trap->SendServerCommand(i, va("chat \"%s: ^3%s\"",ent->client->pers.netname,arg));
				G_Sound(&g_entities[i],CHAN_VOICE,G_SoundIndex(va("sound/chars/%s/misc/%s.mp3",voice_dir,arg)));
			}
		}
	}
}


static char	*gc_orders[] = {
	"hold your position",
	"hold this position",
	"come here",
	"cover me",
	"guard location",
	"search and destroy",
	"report"
};
static size_t numgc_orders = ARRAY_LEN( gc_orders );

void Cmd_GameCommand_f( gentity_t *ent ) {
	int				targetNum;
	unsigned int	order;
	gentity_t		*target;
	char			arg[MAX_TOKEN_CHARS] = {0};

	if ( trap->Argc() != 3 ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"Usage: gc <player id> <order 0-%d>\n\"", numgc_orders - 1 ) );
		return;
	}

	trap->Argv( 2, arg, sizeof( arg ) );
	order = atoi( arg );

	if ( order >= numgc_orders ) {
		trap->SendServerCommand( ent-g_entities, va("print \"Bad order: %i\n\"", order));
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );
	targetNum = ClientNumberFromString( ent, arg, qfalse );
	if ( targetNum == -1 )
		return;

	target = &g_entities[targetNum];
	if ( !target->inuse || !target->client )
		return;

	G_LogPrintf( "tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, gc_orders[order] );
	G_Say( ent, target, SAY_TELL, gc_orders[order] );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !(ent->r.svFlags & SVF_BOT) )
		G_Say( ent, ent, SAY_TELL, gc_orders[order] );
}

/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f( gentity_t *ent ) {
	// zyk: changed code, so it will always use ps.origin and the ps.viewangles
	if(ent->client)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"origin: %s angles: %s\n\"", vtos(ent->client->ps.origin), vtos(ent->client->ps.viewangles)));
	}
}

static const char *gameNames[] = {
	"Free For All",
	"Holocron FFA",
	"Jedi Master",
	"Duel",
	"Power Duel",
	"Single Player",
	"Team FFA",
	"Siege",
	"Capture the Flag",
	"Capture the Ysalamiri"
};

/*
==================
Cmd_CallVote_f
==================
*/
extern void SiegeClearSwitchData(void); //g_saga.c

qboolean G_VoteCapturelimit( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int n = Com_Clampi( 0, 0x7FFFFFFF, atoi( arg2 ) );
	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, n );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteClientkick( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int n = atoi ( arg2 );

	if ( n < 0 || n >= level.maxclients ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"invalid client number %d.\n\"", n ) );
		return qfalse;
	}

	if ( g_entities[n].client->pers.connected == CON_DISCONNECTED ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"there is no client with the client number %d.\n\"", n ) );
		return qfalse;
	}

	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s %s", arg1, g_entities[n].client->pers.netname );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteFraglimit( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int n = Com_Clampi( 0, 0x7FFFFFFF, atoi( arg2 ) );
	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, n );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteGametype( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int gt = atoi( arg2 );

	// ffa, ctf, tdm, etc
	if ( arg2[0] && isalpha( arg2[0] ) ) {
		gt = BG_GetGametypeForString( arg2 );
		if ( gt == -1 )
		{
			trap->SendServerCommand( ent-g_entities, va( "print \"Gametype (%s) unrecognised, defaulting to FFA/Deathmatch\n\"", arg2 ) );
			gt = GT_FFA;
		}
	}
	// numeric but out of range
	else if ( gt < 0 || gt >= GT_MAX_GAME_TYPE ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"Gametype (%i) is out of range, defaulting to FFA/Deathmatch\n\"", gt ) );
		gt = GT_FFA;
	}

	// logically invalid gametypes, or gametypes not fully implemented in MP
	if ( gt == GT_SINGLE_PLAYER ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"This gametype is not supported (%s).\n\"", arg2 ) );
		return qfalse;
	}

	level.votingGametype = qtrue;
	level.votingGametypeTo = gt;

	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %d", arg1, gt );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s %s", arg1, gameNames[gt] );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteKick( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int clientid = ClientNumberFromString( ent, arg2, qtrue );
	gentity_t *target = NULL;

	if ( clientid == -1 )
		return qfalse;

	target = &g_entities[clientid];
	if ( !target || !target->inuse || !target->client )
		return qfalse;

	Com_sprintf( level.voteString, sizeof( level.voteString ), "clientkick %d", clientid );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "kick %s", target->client->pers.netname );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

const char *G_GetArenaInfoByMap( const char *map );

void Cmd_MapList_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];

	if ( trap->Argc() < 2 )
	{
		trap->SendServerCommand( ent-g_entities, "print \"Use ^3/maplist <page number> ^7to see map list. Use ^3/maplist bsp ^7to show bsp files, which can be used in /callvote map <bsp file>\n\"" );
		return;
	}

	trap->Argv(1, arg1, sizeof( arg1 ));

	if (Q_stricmp(arg1, "bsp") == 0)
	{
		int i, toggle=0;
		char map[24] = "--", buf[512] = {0};

		Q_strcat( buf, sizeof( buf ), "Map list:" );

		for ( i=0; i<level.arenas.num; i++ ) {
			Q_strncpyz( map, Info_ValueForKey( level.arenas.infos[i], "map" ), sizeof( map ) );
			Q_StripColor( map );

			if ( G_DoesMapSupportGametype( map, level.gametype ) ) {
				char *tmpMsg = va( " ^%c%s", (++toggle&1) ? COLOR_GREEN : COLOR_YELLOW, map );
				if ( strlen( buf ) + strlen( tmpMsg ) >= sizeof( buf ) ) {
					trap->SendServerCommand( ent-g_entities, va( "print \"%s\"", buf ) );
					buf[0] = '\0';
				}
				Q_strcat( buf, sizeof( buf ), tmpMsg );
			}
		}

		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", buf ) );
	}
	else
	{
		int page = 1; // zyk: page the user wants to see
		char file_content[MAX_STRING_CHARS];
		char content[512];
		int i = 0;
		int results_per_page = zyk_list_cmds_results_per_page.integer; // zyk: number of results per page
		FILE *map_list_file;
		strcpy(file_content,"");
		strcpy(content,"");

		page = atoi(arg1);

		// GalaxyRP fix: [validation] atoi() only catches a page argument that parses to exactly 0;
		// a negative page number (e.g. "/maplist -5") passed this check straight through and made
		// both pagination loop bounds below negative, so neither loop below ever ran and the
		// command silently printed a blank page instead of reporting the bad input.
		if (page <= 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid page number\n\"" );
			return;
		}

		map_list_file = fopen("GalaxyRP/maplist.txt","r");
		if (map_list_file != NULL)
		{
			while(i < (results_per_page * (page-1)) && fgets(content, sizeof(content), map_list_file) != NULL)
			{ // zyk: reads the file until it reaches the position corresponding to the page number
				i++;
			}

			while(i < (results_per_page * page) && fgets(content, sizeof(content), map_list_file) != NULL)
			{ // zyk: fgets returns NULL at EOF
				// GalaxyRP fix: [security] this used to be strcpy(file_content, va("%s%s",
				// file_content, content)) -- file_content is a fixed MAX_STRING_CHARS (1024-byte)
				// stack buffer, and that strcpy had no bounds check on the destination at all.
				// Enough map entries on one page (or zyk_list_cmds_results_per_page set too high)
				// overflows it. Q_strcat never writes past the destination's declared size.
				Q_strcat(file_content, sizeof(file_content), content);
				i++;
			}

			fclose(map_list_file);
			trap->SendServerCommand(ent-g_entities, va("print \"\n%s\n\"",file_content));
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, "print \"The maplist file does not exist\n\"" );
			return;
		}
	}
}

qboolean G_VotePoll( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	// zyk: did not put message
	if ( numArgs < 3 ) {
		return qfalse;
	}

	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "poll %s", arg2 );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );

	// zyk: now the vote poll will appear in chat
	trap->SendServerCommand( -1, va("chat \"^3Poll System: ^7%s ^2Yes^3^1/No^7\"",arg2));

	return qtrue;
}

qboolean G_VoteMap( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	char s[MAX_CVAR_VALUE_STRING] = {0}, bspName[MAX_QPATH] = {0}, *mapName = NULL, *mapName2 = NULL;
	fileHandle_t fp = NULL_FILE;
	const char *arenaInfo;

	// didn't specify a map, show available maps
	if ( numArgs < 3 ) {
		Cmd_MapList_f( ent );
		return qfalse;
	}

	if ( strchr( arg2, '\\' ) ) {
		trap->SendServerCommand( ent-g_entities, "print \"Can't have mapnames with a \\\n\"" );
		return qfalse;
	}

	Com_sprintf( bspName, sizeof(bspName), "maps/%s.bsp", arg2 );
	if ( trap->FS_Open( bspName, &fp, FS_READ ) <= 0 ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"Can't find map %s on server\n\"", bspName ) );
		if( fp != NULL_FILE )
			trap->FS_Close( fp );
		return qfalse;
	}
	trap->FS_Close( fp );

	if ( !G_DoesMapSupportGametype( arg2, level.gametype ) ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOVOTE_MAPNOTSUPPORTEDBYGAME" ) ) );
		return qfalse;
	}

	// preserve the map rotation
	trap->Cvar_VariableStringBuffer( "nextmap", s, sizeof( s ) );
	if ( *s )
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s; set nextmap \"%s\"", arg1, arg2, s );
	else
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );

	arenaInfo = G_GetArenaInfoByMap(arg2);
	if ( arenaInfo ) {
		mapName = Info_ValueForKey( arenaInfo, "longname" );
		mapName2 = Info_ValueForKey( arenaInfo, "map" );
	}

	if ( !mapName || !mapName[0] )
		mapName = "ERROR";

	if ( !mapName2 || !mapName2[0] )
		mapName2 = "ERROR";

	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "map %s (%s)", mapName, mapName2 );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteMapRestart( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int n = Com_Clampi( 0, 60, atoi( arg2 ) );
	if ( numArgs < 3 )
		n = 5;
	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, n );
	Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteNextmap( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	char s[MAX_CVAR_VALUE_STRING];

	trap->Cvar_VariableStringBuffer( "nextmap", s, sizeof( s ) );
	if ( !*s ) {
		trap->SendServerCommand( ent-g_entities, "print \"nextmap not set.\n\"" );
		return qfalse;
	}
	SiegeClearSwitchData();
	Com_sprintf( level.voteString, sizeof( level.voteString ), "vstr nextmap");
	Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteTimelimit( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	float tl = Com_Clamp( 0.0f, 35790.0f, atof( arg2 ) );
	if ( Q_isintegral( tl ) )
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, (int)tl );
	else
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %.3f", arg1, tl );
	Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteWarmup( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int n = Com_Clampi( 0, 1, atoi( arg2 ) );
	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, n );
	Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

typedef struct voteString_s {
	const char	*string;
	const char	*aliases;	// space delimited list of aliases, will always show the real vote string
	qboolean	(*func)(gentity_t *ent, int numArgs, const char *arg1, const char *arg2);
	int			numArgs;	// number of REQUIRED arguments, not total/optional arguments
	uint32_t	validGT;	// bit-flag of valid gametypes
	qboolean	voteDelay;	// if true, will delay executing the vote string after it's accepted by g_voteDelay
	const char	*shortHelp;	// NULL if no arguments needed
} voteString_t;

static voteString_t validVoteStrings[] = {
	//	vote string				aliases										# args	valid gametypes							exec delay		short help
	{	"capturelimit",			"caps",				G_VoteCapturelimit,		1,		GTB_CTF|GTB_CTY,						qtrue,			"<num>" },
	{	"clientkick",			NULL,				G_VoteClientkick,		1,		GTB_ALL,								qfalse,			"<clientnum>" },
	{	"fraglimit",			"frags",			G_VoteFraglimit,		1,		GTB_ALL & ~(GTB_SIEGE|GTB_CTF|GTB_CTY),	qtrue,			"<num>" },
	{	"g_doWarmup",			"dowarmup warmup",	G_VoteWarmup,			1,		GTB_ALL,								qtrue,			"<0-1>" },
	{	"g_gametype",			"gametype gt mode",	G_VoteGametype,			1,		GTB_ALL,								qtrue,			"<num or name>" },
	{	"kick",					NULL,				G_VoteKick,				1,		GTB_ALL,								qfalse,			"<client name>" },
	{	"map",					NULL,				G_VoteMap,				0,		GTB_ALL,								qtrue,			"<name>" },
	{	"map_restart",			"restart",			G_VoteMapRestart,		0,		GTB_ALL,								qtrue,			"<optional delay>" },
	{	"nextmap",				NULL,				G_VoteNextmap,			0,		GTB_ALL,								qtrue,			NULL },
	{	"poll",					NULL,				G_VotePoll,				0,		GTB_ALL,								qtrue,			"<message>" },
	{	"timelimit",			"time",				G_VoteTimelimit,		1,		GTB_ALL &~GTB_SIEGE,					qtrue,			"<num>" },
};
static const int validVoteStringsSize = ARRAY_LEN( validVoteStrings );

void Svcmd_ToggleAllowVote_f( void ) {
	if ( trap->Argc() == 1 ) {
		int i = 0;
		for ( i = 0; i<validVoteStringsSize; i++ ) {
			if ( (g_allowVote.integer & (1 << i)) )	trap->Print( "%2d [X] %s\n", i, validVoteStrings[i].string );
			else									trap->Print( "%2d [ ] %s\n", i, validVoteStrings[i].string );
		}
		return;
	}
	else {
		char arg[8] = { 0 };
		int index;

		trap->Argv( 1, arg, sizeof( arg ) );
		index = atoi( arg );

		if ( index < 0 || index >= validVoteStringsSize ) {
			Com_Printf( "ToggleAllowVote: Invalid range: %i [0, %i]\n", index, validVoteStringsSize - 1 );
			return;
		}

		trap->Cvar_Set( "g_allowVote", va( "%i", (1 << index) ^ (g_allowVote.integer & ((1 << validVoteStringsSize) - 1)) ) );
		trap->Cvar_Update( &g_allowVote );

		Com_Printf( "%s %s^7\n", validVoteStrings[index].string, ((g_allowVote.integer & (1 << index)) ? "^2Enabled" : "^1Disabled") );
	}
}

void Cmd_CallVote_f( gentity_t *ent ) {
	int				i=0, numArgs=0;
	char			arg1[MAX_CVAR_VALUE_STRING] = {0};
	char			arg2[MAX_CVAR_VALUE_STRING] = {0};
	voteString_t	*vote = NULL;

	// not allowed to vote at all
	if ( !g_allowVote.integer ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOVOTE" ) ) );
		return;
	}

	// vote in progress
	else if ( level.voteTime ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "VOTEINPROGRESS" ) ) );
		return;
	}

	// can't vote as a spectator, except in (power)duel
	else if ( level.gametype != GT_DUEL && level.gametype != GT_POWERDUEL && ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOSPECVOTE" ) ) );
		return;
	}

	// GalaxyRP fix: [Guardian] a loop blocking /callvote while any player had guardian_mode > 0 used to
	// be here. guardian_mode is permanently 0 now, so it was unreachable.

	// zyk: tests if this player can vote now
	if (zyk_vote_timer.integer > 0 && ent->client->sess.vote_timer > 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You cannot vote now, wait %d seconds and try again.\n\"", ent->client->sess.vote_timer));
		return;
	}

	level.voting_player = ent->s.number;

	// make sure it is a valid command to vote on
	numArgs = trap->Argc();
	trap->Argv( 1, arg1, sizeof( arg1 ) );
	if ( numArgs > 1 )
		Q_strncpyz( arg2, ConcatArgs( 2 ), sizeof( arg2 ) );

	// filter ; \n \r
	if ( Q_strchrs( arg1, ";\r\n" ) || Q_strchrs( arg2, ";\r\n" ) ) {
		trap->SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		return;
	}

	// check for invalid votes
	for ( i=0; i<validVoteStringsSize; i++ ) {
		if ( !(g_allowVote.integer & (1<<i)) )
			continue;

		if ( !Q_stricmp( arg1, validVoteStrings[i].string ) )
			break;

		// see if they're using an alias, and set arg1 to the actual vote string
		if ( validVoteStrings[i].aliases ) {
			char tmp[MAX_TOKEN_CHARS] = {0}, *p = NULL;
			const char *delim = " ";
			Q_strncpyz( tmp, validVoteStrings[i].aliases, sizeof( tmp ) );
			p = strtok( tmp, delim );
			while ( p != NULL ) {
				if ( !Q_stricmp( arg1, p ) ) {
					Q_strncpyz( arg1, validVoteStrings[i].string, sizeof( arg1 ) );
					goto validVote;
				}
				p = strtok( NULL, delim );
			}
		}
	}
	// invalid vote string, abandon ship
	if ( i == validVoteStringsSize ) {
		char buf[1024] = {0};
		int toggle = 0;
		trap->SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		trap->SendServerCommand( ent-g_entities, "print \"Allowed vote strings are: \"" );
		for ( i=0; i<validVoteStringsSize; i++ ) {
			if ( !(g_allowVote.integer & (1<<i)) )
				continue;

			toggle = !toggle;
			if ( validVoteStrings[i].shortHelp ) {
				Q_strcat( buf, sizeof( buf ), va( "^%c%s %s ",
					toggle ? COLOR_GREEN : COLOR_YELLOW,
					validVoteStrings[i].string,
					validVoteStrings[i].shortHelp ) );
			}
			else {
				Q_strcat( buf, sizeof( buf ), va( "^%c%s ",
					toggle ? COLOR_GREEN : COLOR_YELLOW,
					validVoteStrings[i].string ) );
			}
		}

		//FIXME: buffer and send in multiple messages in case of overflow
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", buf ) );
		return;
	}

validVote:
	vote = &validVoteStrings[i];
	if ( !(vote->validGT & (1<<level.gametype)) ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s is not applicable in this gametype.\n\"", arg1 ) );
		return;
	}

	if ( numArgs < vote->numArgs+2 ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s requires more arguments: %s\n\"", arg1, vote->shortHelp ) );
		return;
	}

	level.votingGametype = qfalse;

	level.voteExecuteDelay = vote->voteDelay ? g_voteDelay.integer : 0;

	// there is still a vote to be executed, execute it and store the new vote
	if ( level.voteExecuteTime ) {
		level.voteExecuteTime = 0;
		trap->SendConsoleCommand( EXEC_APPEND, va( "%s\n", level.voteString ) );
	}

	// pass the args onto vote-specific handlers for parsing/filtering
	if ( vote->func ) {
		if ( !vote->func( ent, numArgs, arg1, arg2 ) )
			return;
	}
	// otherwise assume it's a command
	else {
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%s\"", arg1, arg2 );
		Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
		Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	}
	Q_strstrip( level.voteStringClean, "\"\n\r", NULL );

	trap->SendServerCommand( -1, va( "print \"%s^7 %s (%s)\n\"", ent->client->pers.netname, G_GetStringEdString( "MP_SVGAME", "PLCALLEDVOTE" ), level.voteStringClean ) );

	// start the voting, the caller automatically votes yes
	level.voteTime = level.time;
	level.voteYes = 0; // zyk: the caller no longer counts as yes, because it may be a poll or the caller may regret the vote
	level.voteNo = 0;

	for ( i=0; i<level.maxclients; i++ ) {
		level.clients[i].mGameFlags &= ~PSG_VOTED;
		level.clients[i].pers.vote = 0;
	}

	//ent->client->mGameFlags |= PSG_VOTED; // zyk: no longer count the caller
	//ent->client->pers.vote = 1; // zyk: no longer count the caller

	trap->SetConfigstring( CS_VOTE_TIME,	va( "%i", level.voteTime ) );
	trap->SetConfigstring( CS_VOTE_STRING,	level.voteDisplayString );
	trap->SetConfigstring( CS_VOTE_YES,		va( "%i", level.voteYes ) );
	trap->SetConfigstring( CS_VOTE_NO,		va( "%i", level.voteNo ) );
}

/*
==================
Cmd_Vote_f
==================
*/
void Cmd_Vote_f( gentity_t *ent ) {
	char		msg[64] = {0};

	if ( !level.voteTime ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEINPROG")) );
		return;
	}
	if ( ent->client->mGameFlags & PSG_VOTED ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "VOTEALREADY")) );
		return;
	}
	if (level.gametype != GT_DUEL && level.gametype != GT_POWERDUEL)
	{
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")) );
			return;
		}
	}

	trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLVOTECAST")) );

	ent->client->mGameFlags |= PSG_VOTED;

	trap->Argv( 1, msg, sizeof( msg ) );

	if ( tolower( msg[0] ) == 'y' || msg[0] == '1' ) {
		level.voteYes++;
		ent->client->pers.vote = 1;
		trap->SetConfigstring( CS_VOTE_YES, va("%i", level.voteYes ) );
	} else {
		level.voteNo++;
		ent->client->pers.vote = 2;
		trap->SetConfigstring( CS_VOTE_NO, va("%i", level.voteNo ) );
	}

	// a majority will be determined in CheckVote, which will also account
	// for players entering or leaving
}

qboolean G_TeamVoteLeader( gentity_t *ent, int cs_offset, team_t team, int numArgs, const char *arg1, const char *arg2 ) {
	int clientid = numArgs == 2 ? ent->s.number : ClientNumberFromString( ent, arg2, qfalse );
	gentity_t *target = NULL;

	if ( clientid == -1 )
		return qfalse;

	target = &g_entities[clientid];
	if ( !target || !target->inuse || !target->client )
		return qfalse;

	if ( target->client->sess.sessionTeam != team )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"User %s is not on your team\n\"", arg2 ) );
		return qfalse;
	}

	Com_sprintf( level.teamVoteString[cs_offset], sizeof( level.teamVoteString[cs_offset] ), "leader %d", clientid );
	Q_strncpyz( level.teamVoteDisplayString[cs_offset], level.teamVoteString[cs_offset], sizeof( level.teamVoteDisplayString[cs_offset] ) );
	Q_strncpyz( level.teamVoteStringClean[cs_offset], level.teamVoteString[cs_offset], sizeof( level.teamVoteStringClean[cs_offset] ) );
	return qtrue;
}

/*
==================
Cmd_CallTeamVote_f
==================
*/
void Cmd_CallTeamVote_f( gentity_t *ent ) {
	team_t	team = ent->client->sess.sessionTeam;
	int		i=0, cs_offset=0, numArgs=0;
	char	arg1[MAX_CVAR_VALUE_STRING] = {0};
	char	arg2[MAX_CVAR_VALUE_STRING] = {0};

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	// not allowed to vote at all
	if ( !g_allowTeamVote.integer ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE")) );
		return;
	}

	// vote in progress
	else if ( level.teamVoteTime[cs_offset] ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADY")) );
		return;
	}

	// can't vote as a spectator
	else if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSPECVOTE")) );
		return;
	}

	// make sure it is a valid command to vote on
	numArgs = trap->Argc();
	trap->Argv( 1, arg1, sizeof( arg1 ) );
	if ( numArgs > 1 )
		Q_strncpyz( arg2, ConcatArgs( 2 ), sizeof( arg2 ) );

	// filter ; \n \r
	if ( Q_strchrs( arg1, ";\r\n" ) || Q_strchrs( arg2, ";\r\n" ) ) {
		trap->SendServerCommand( ent-g_entities, "print \"Invalid team vote string.\n\"" );
		return;
	}

	// pass the args onto vote-specific handlers for parsing/filtering
	if ( !Q_stricmp( arg1, "leader" ) ) {
		if ( !G_TeamVoteLeader( ent, cs_offset, team, numArgs, arg1, arg2 ) )
			return;
	}
	else {
		trap->SendServerCommand( ent-g_entities, "print \"Invalid team vote string.\n\"" );
		trap->SendServerCommand( ent-g_entities, va("print \"Allowed team vote strings are: ^%c%s %s\n\"", COLOR_GREEN, "leader", "<optional client name or number>" ));
		return;
	}

	Q_strstrip( level.teamVoteStringClean[cs_offset], "\"\n\r", NULL );

	for ( i=0; i<level.maxclients; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED )
			continue;
		if ( level.clients[i].sess.sessionTeam == team )
			trap->SendServerCommand( i, va("print \"%s^7 called a team vote (%s)\n\"", ent->client->pers.netname, level.teamVoteStringClean[cs_offset] ) );
	}

	// start the voting, the caller autoamtically votes yes
	level.teamVoteTime[cs_offset] = level.time;
	level.teamVoteYes[cs_offset] = 1;
	level.teamVoteNo[cs_offset] = 0;

	for ( i=0; i<level.maxclients; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED )
			continue;
		if ( level.clients[i].sess.sessionTeam == team ) {
			level.clients[i].mGameFlags &= ~PSG_TEAMVOTED;
			level.clients[i].pers.teamvote = 0;
		}
	}
	ent->client->mGameFlags |= PSG_TEAMVOTED;
	ent->client->pers.teamvote = 1;

	trap->SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, va("%i", level.teamVoteTime[cs_offset] ) );
	trap->SetConfigstring( CS_TEAMVOTE_STRING + cs_offset, level.teamVoteDisplayString[cs_offset] );
	trap->SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
	trap->SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
}

/*
==================
Cmd_TeamVote_f
==================
*/
void Cmd_TeamVote_f( gentity_t *ent ) {
	team_t		team = ent->client->sess.sessionTeam;
	int			cs_offset=0;
	char		msg[64] = {0};

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !level.teamVoteTime[cs_offset] ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOTEAMVOTEINPROG")) );
		return;
	}
	if ( ent->client->mGameFlags & PSG_TEAMVOTED ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADYCAST")) );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")) );
		return;
	}

	trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLTEAMVOTECAST")) );

	ent->client->mGameFlags |= PSG_TEAMVOTED;

	trap->Argv( 1, msg, sizeof( msg ) );

	if ( tolower( msg[0] ) == 'y' || msg[0] == '1' ) {
		level.teamVoteYes[cs_offset]++;
		ent->client->pers.teamvote = 1;
		trap->SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
	} else {
		level.teamVoteNo[cs_offset]++;
		ent->client->pers.teamvote = 2;
		trap->SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
	}

	// a majority will be determined in TeamCheckVote, which will also account
	// for players entering or leaving
}


/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f( gentity_t *ent ) {
	vec3_t		origin, angles;
	char		buffer[MAX_TOKEN_CHARS];
	int			i;

	if ( trap->Argc() != 5 ) {
		trap->SendServerCommand( ent-g_entities, va("print \"usage: setviewpos x y z yaw\n\""));
		return;
	}

	VectorClear( angles );
	for ( i = 0 ; i < 3 ; i++ ) {
		trap->Argv( i + 1, buffer, sizeof( buffer ) );
		origin[i] = atof( buffer );
	}

	trap->Argv( 4, buffer, sizeof( buffer ) );
	angles[YAW] = atof( buffer );

	TeleportPlayer( ent, origin, angles );
}

void G_LeaveVehicle( gentity_t* ent, qboolean ConCheck ) {

	if (ent->client->ps.m_iVehicleNum)
	{ //tell it I'm getting off
		gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

		if (veh->inuse && veh->client && veh->m_pVehicle)
		{
			if ( ConCheck ) { // check connection
				clientConnected_t pCon = ent->client->pers.connected;
				ent->client->pers.connected = CON_DISCONNECTED;
				veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t *)ent, qtrue);
				ent->client->pers.connected = pCon;
			} else { // or not.
				veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t *)ent, qtrue);
			}
		}
	}

	ent->client->ps.m_iVehicleNum = 0;
}

int G_ItemUsable(playerState_t *ps, int forcedUse)
{
	vec3_t fwd, fwdorg, dest, pos;
	vec3_t yawonly;
	vec3_t mins, maxs;
	vec3_t trtest;
	trace_t tr;

	// fix: dead players shouldn't use items
	if (ps->stats[STAT_HEALTH] <= 0) {
		return 0;
	}

	if (ps->m_iVehicleNum)
	{
		return 0;
	}

	if (ps->pm_flags & PMF_USE_ITEM_HELD)
	{ //force to let go first
		return 0;
	}

	if (!forcedUse)
	{
		forcedUse = bg_itemlist[ps->stats[STAT_HOLDABLE_ITEM]].giTag;
	}

	if (!BG_IsItemSelectable(ps, forcedUse))
	{
		return 0;
	}

	switch (forcedUse)
	{
	case HI_MEDPAC:
	case HI_MEDPAC_BIG:
		if (ps->stats[STAT_HEALTH] >= ps->stats[STAT_MAX_HEALTH])
		{
			return 0;
		}

		if (ps->stats[STAT_HEALTH] <= 0)
		{
			return 0;
		}

		return 1;
	case HI_SEEKER:
		if (ps->eFlags & EF_SEEKERDRONE)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SEEKER_ALREADYDEPLOYED);
			return 0;
		}

		return 1;
	case HI_SENTRY_GUN:
		if (ps->fd.sentryDeployed)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SENTRY_ALREADYPLACED);
			return 0;
		}

		yawonly[ROLL] = 0;
		yawonly[PITCH] = 0;
		yawonly[YAW] = ps->viewangles[YAW];

		VectorSet( mins, -8, -8, 0 );
		VectorSet( maxs, 8, 8, 24 );

		AngleVectors(yawonly, fwd, NULL, NULL);

		fwdorg[0] = ps->origin[0] + fwd[0]*64;
		fwdorg[1] = ps->origin[1] + fwd[1]*64;
		fwdorg[2] = ps->origin[2] + fwd[2]*64;

		trtest[0] = fwdorg[0] + fwd[0]*16;
		trtest[1] = fwdorg[1] + fwd[1]*16;
		trtest[2] = fwdorg[2] + fwd[2]*16;

		trap->Trace(&tr, ps->origin, mins, maxs, trtest, ps->clientNum, MASK_PLAYERSOLID, qfalse, 0, 0);

		if ((tr.fraction != 1 && tr.entityNum != ps->clientNum) || tr.startsolid || tr.allsolid)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SENTRY_NOROOM);
			return 0;
		}

		return 1;
	case HI_SHIELD:
		mins[0] = -8;
		mins[1] = -8;
		mins[2] = 0;

		maxs[0] = 8;
		maxs[1] = 8;
		maxs[2] = 8;

		AngleVectors (ps->viewangles, fwd, NULL, NULL);
		fwd[2] = 0;
		VectorMA(ps->origin, 64, fwd, dest);
		trap->Trace(&tr, ps->origin, mins, maxs, dest, ps->clientNum, MASK_SHOT, qfalse, 0, 0 );
		if (tr.fraction > 0.9 && !tr.startsolid && !tr.allsolid)
		{
			VectorCopy(tr.endpos, pos);
			VectorSet( dest, pos[0], pos[1], pos[2] - 4096 );
			trap->Trace( &tr, pos, mins, maxs, dest, ps->clientNum, MASK_SOLID, qfalse, 0, 0 );
			if ( !tr.startsolid && !tr.allsolid )
			{
				return 1;
			}
		}
		G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SHIELD_NOROOM);
		return 0;
	case HI_JETPACK: //do something?
		return 1;
	case HI_HEALTHDISP:
		return 1;
	case HI_AMMODISP:
		return 1;
	case HI_EWEB:
		return 1;
	case HI_CLOAK:
		return 1;
	default:
		return 1;
	}
}

void saberKnockDown(gentity_t *saberent, gentity_t *saberOwner, gentity_t *other);

void Cmd_ToggleSaber_f(gentity_t *ent)
{
	if (ent->client->ps.fd.forceGripCripple)
	{ //if they are being gripped, don't let them unholster their saber
		if (ent->client->ps.saberHolstered)
		{
			return;
		}
	}

	if (ent->client->ps.saberInFlight)
	{
		if (ent->client->ps.saberEntityNum)
		{ //turn it off in midair
			saberKnockDown(&g_entities[ent->client->ps.saberEntityNum], ent, ent);
		}
		return;
	}

	if (ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return;
	}

	if (ent->client->ps.weapon != WP_SABER)
	{
		return;
	}

//	if (ent->client->ps.duelInProgress && !ent->client->ps.saberHolstered)
//	{
//		return;
//	}

	if (ent->client->ps.duelTime >= level.time)
	{
		return;
	}

	if (ent->client->ps.saberLockTime >= level.time)
	{
		return;
	}

	// zyk: noclip does not allow toggle saber
	if ( ent->client->noclip == qtrue )
	{
		return;
	}

	if (ent->client && ent->client->ps.weaponTime < 1)
	{
		if (ent->client->ps.saberHolstered == 2)
		{
			ent->client->ps.saberHolstered = 0;

			if (ent->client->saber[0].soundOn)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
			}
			if (ent->client->saber[1].soundOn)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
			}
		}
		else
		{
			ent->client->ps.saberHolstered = 2;
			if (ent->client->saber[0].soundOff)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
			}
			if (ent->client->saber[1].soundOff &&
				ent->client->saber[1].model[0])
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
			}
			//prevent anything from being done for 400ms after holster
			ent->client->ps.weaponTime = 400;
		}
	}
}

extern vmCvar_t		d_saberStanceDebug;

extern qboolean WP_SaberCanTurnOffSomeBlades( saberInfo_t *saber );
void Cmd_SaberAttackCycle_f(gentity_t *ent)
{
	int selectLevel = 0;
	qboolean usingSiegeStyle = qfalse;

	if ( !ent || !ent->client )
	{
		return;
	}

	if ( level.intermissionQueued || level.intermissiontime )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"%s (saberAttackCycle)\n\"", G_GetStringEdString( "MP_SVGAME", "CANNOT_TASK_INTERMISSION" ) ) );
		return;
	}

	if ( ent->health <= 0
			|| ent->client->tempSpectate >= level.time
			|| ent->client->sess.sessionTeam == TEAM_SPECTATOR )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "MUSTBEALIVE" ) ) );
		return;
	}


	if ( ent->client->ps.weapon != WP_SABER )
	{
        return;
	}
	/*
	if (ent->client->ps.weaponTime > 0)
	{ //no switching attack level when busy
		return;
	}
	*/

	if (ent->client->saber[0].model[0] && ent->client->saber[1].model[0])
	{ //no cycling for akimbo
		if ( WP_SaberCanTurnOffSomeBlades( &ent->client->saber[1] ) )
		{//can turn second saber off
			if ( ent->client->ps.saberHolstered == 1 )
			{//have one holstered
				//unholster it
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
				ent->client->ps.saberHolstered = 0;
				//g_active should take care of this, but...
				ent->client->ps.fd.saberAnimLevel = SS_DUAL;
			}
			else if ( ent->client->ps.saberHolstered == 0 )
			{//have none holstered
				if ( (ent->client->saber[1].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE) )
				{//can't turn it off manually
				}
				else if ( ent->client->saber[1].bladeStyle2Start > 0
					&& (ent->client->saber[1].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE2) )
				{//can't turn it off manually
				}
				else
				{
					//turn it off
					G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
					ent->client->ps.saberHolstered = 1;
					//g_active should take care of this, but...
					ent->client->ps.fd.saberAnimLevel = SS_FAST;
				}
			}

			if (d_saberStanceDebug.integer)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle dual saber blade.\n\"") );
			}
			return;
		}
	}
	else if (ent->client->saber[0].numBlades > 1
		&& WP_SaberCanTurnOffSomeBlades( &ent->client->saber[0] ) )
	{ //use staff stance then.
		if ( ent->client->ps.saberHolstered == 1 )
		{//second blade off
			if ( ent->client->ps.saberInFlight )
			{//can't turn second blade back on if it's in the air, you naughty boy!
				if (d_saberStanceDebug.integer)
				{
					trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade in air.\n\"") );
				}
				return;
			}
			//turn it on
			G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
			ent->client->ps.saberHolstered = 0;
			//g_active should take care of this, but...
			if ( ent->client->saber[0].stylesForbidden )
			{//have a style we have to use
				WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel );
				if ( ent->client->ps.weaponTime <= 0 )
				{ //not busy, set it now
					ent->client->ps.fd.saberAnimLevel = selectLevel;
				}
				else
				{ //can't set it now or we might cause unexpected chaining, so queue it
					ent->client->saberCycleQueue = selectLevel;
				}
			}
		}
		else if ( ent->client->ps.saberHolstered == 0 )
		{//both blades on
			if ( (ent->client->saber[0].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE) )
			{//can't turn it off manually
			}
			else if ( ent->client->saber[0].bladeStyle2Start > 0
				&& (ent->client->saber[0].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE2) )
			{//can't turn it off manually
			}
			else
			{
				//turn second one off
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
				ent->client->ps.saberHolstered = 1;
				//g_active should take care of this, but...
				if ( ent->client->saber[0].singleBladeStyle != SS_NONE )
				{
					if ( ent->client->ps.weaponTime <= 0 )
					{ //not busy, set it now
						ent->client->ps.fd.saberAnimLevel = ent->client->saber[0].singleBladeStyle;
					}
					else
					{ //can't set it now or we might cause unexpected chaining, so queue it
						ent->client->saberCycleQueue = ent->client->saber[0].singleBladeStyle;
					}
				}
			}
		}
		if (d_saberStanceDebug.integer)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade.\n\"") );
		}
		return;
	}

	if (ent->client->saberCycleQueue)
	{ //resume off of the queue if we haven't gotten a chance to update it yet
		selectLevel = ent->client->saberCycleQueue;
	}
	else
	{
		selectLevel = ent->client->ps.fd.saberAnimLevel;
	}

	if (level.gametype == GT_SIEGE &&
		ent->client->siegeClass != -1 &&
		bgSiegeClasses[ent->client->siegeClass].saberStance)
	{ //we have a flag of useable stances so cycle through it instead
		int i = selectLevel+1;

		usingSiegeStyle = qtrue;

		while (i != selectLevel)
		{ //cycle around upward til we hit the next style or end up back on this one
			if (i >= SS_NUM_SABER_STYLES)
			{ //loop back around to the first valid
				i = SS_FAST;
			}

			if (bgSiegeClasses[ent->client->siegeClass].saberStance & (1 << i))
			{ //we can use this one, select it and break out.
				selectLevel = i;
				break;
			}
			i++;
		}

		if (d_saberStanceDebug.integer)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to cycle given class stance.\n\"") );
		}
	}
	else
	{
		selectLevel++;
		if ( selectLevel > ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] )
		{
			selectLevel = FORCE_LEVEL_1;
		}
		if (d_saberStanceDebug.integer)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to cycle stance normally.\n\"") );
		}
	}
/*
#ifndef FINAL_BUILD
	switch ( selectLevel )
	{
	case FORCE_LEVEL_1:
		trap->SendServerCommand( ent-g_entities, va("print \"Lightsaber Combat Style: %sfast\n\"", S_COLOR_BLUE) );
		break;
	case FORCE_LEVEL_2:
		trap->SendServerCommand( ent-g_entities, va("print \"Lightsaber Combat Style: %smedium\n\"", S_COLOR_YELLOW) );
		break;
	case FORCE_LEVEL_3:
		trap->SendServerCommand( ent-g_entities, va("print \"Lightsaber Combat Style: %sstrong\n\"", S_COLOR_RED) );
		break;
	}
#endif
*/
	if ( !usingSiegeStyle )
	{
		//make sure it's valid, change it if not
		WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel );
	}

	if (ent->client->ps.weaponTime <= 0)
	{ //not busy, set it now
		ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = selectLevel;
	}
	else
	{ //can't set it now or we might cause unexpected chaining, so queue it
		ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = selectLevel;
	}
}

qboolean G_OtherPlayersDueling(void)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent && ent->inuse && ent->client && ent->client->ps.duelInProgress)
		{
			return qtrue;
		}
		i++;
	}

	return qfalse;
}

void Cmd_EngageDuel_f(gentity_t *ent)
{
	trace_t tr;
	vec3_t forward, fwdOrg;

	if (!g_privateDuel.integer)
	{
		return;
	}

	if (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
	{ //rather pointless in this mode..
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NODUEL_GAMETYPE")) );
		return;
	}

	if (ent->client->ps.duelTime >= level.time)
	{
		return;
	}

	if (ent->client->ps.weapon != WP_SABER)
	{
		return;
	}

	// zyk: dont engage if held by a rancor to prevent player being invisible after eaten
	if (ent->client->ps.eFlags2 & EF2_HELD_BY_MONSTER)
	{
		return;
	}

	/*
	if (!ent->client->ps.saberHolstered)
	{ //must have saber holstered at the start of the duel
		return;
	}
	*/
	//NOTE: No longer doing this..

	if (ent->client->ps.saberInFlight)
	{
		return;
	}

	if (ent->client->ps.duelInProgress)
	{
		return;
	}

	if (level.duel_tournament_mode > 1 && level.duel_players[ent->s.number] != -1)
	{ // zyk: during a Duel Tournament, players cannot private duel
		return;
	}

	//New: Don't let a player duel if he just did and hasn't waited 10 seconds yet (note: If someone challenges him, his duel timer will reset so he can accept)
	/*if (ent->client->ps.fd.privateDuelTime > level.time)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "CANTDUEL_JUSTDID")) );
		return;
	}

	if (G_OtherPlayersDueling())
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "CANTDUEL_BUSY")) );
		return;
	}*/

	AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );

	fwdOrg[0] = ent->client->ps.origin[0] + forward[0]*256;
	fwdOrg[1] = ent->client->ps.origin[1] + forward[1]*256;
	fwdOrg[2] = (ent->client->ps.origin[2]+ent->client->ps.viewheight) + forward[2]*256;

	trap->Trace(&tr, ent->client->ps.origin, NULL, NULL, fwdOrg, ent->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);

	if (tr.fraction != 1 && tr.entityNum < MAX_CLIENTS)
	{
		gentity_t *challenged = &g_entities[tr.entityNum];

		if (!challenged || !challenged->client || !challenged->inuse ||
			challenged->health < 1 || challenged->client->ps.stats[STAT_HEALTH] < 1 ||
			challenged->client->ps.weapon != WP_SABER || challenged->client->ps.duelInProgress ||
			challenged->client->ps.saberInFlight ||
			challenged->client->ps.eFlags2 & EF2_HELD_BY_MONSTER) // zyk: added this condition to prevent player being invisible after eaten by a rancor
		{
			return;
		}

		if (challenged->client->ps.duelIndex == ent->s.number && challenged->client->ps.duelTime >= level.time)
		{
			trap->SendServerCommand( /*challenged-g_entities*/-1, va("print \"%s ^7%s %s!\n\"", challenged->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELACCEPT"), ent->client->pers.netname) );

			ent->client->ps.duelInProgress = qtrue;
			challenged->client->ps.duelInProgress = qtrue;

			// zyk: reset hp and shield of both players
			ent->health = 100;
			ent->client->ps.stats[STAT_ARMOR] = 100;

			challenged->health = 100;
			challenged->client->ps.stats[STAT_ARMOR] = 100;

			// zyk: disable jetpack of both players
			Jetpack_Off(ent);
			Jetpack_Off(challenged);

			ent->client->ps.duelTime = level.time + 2000;
			challenged->client->ps.duelTime = level.time + 2000;

			G_AddEvent(ent, EV_PRIVATE_DUEL, 1);
			G_AddEvent(challenged, EV_PRIVATE_DUEL, 1);

			//Holster their sabers now, until the duel starts (then they'll get auto-turned on to look cool)

			if (!ent->client->ps.saberHolstered)
			{
				if (ent->client->saber[0].soundOff)
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
				}
				if (ent->client->saber[1].soundOff &&
					ent->client->saber[1].model[0])
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
				}
				ent->client->ps.weaponTime = 400;
				ent->client->ps.saberHolstered = 2;
			}
			if (!challenged->client->ps.saberHolstered)
			{
				if (challenged->client->saber[0].soundOff)
				{
					G_Sound(challenged, CHAN_AUTO, challenged->client->saber[0].soundOff);
				}
				if (challenged->client->saber[1].soundOff &&
					challenged->client->saber[1].model[0])
				{
					G_Sound(challenged, CHAN_AUTO, challenged->client->saber[1].soundOff);
				}
				challenged->client->ps.weaponTime = 400;
				challenged->client->ps.saberHolstered = 2;
			}
		}
		else
		{
			//Print the message that a player has been challenged in private, only announce the actual duel initiation in private
			trap->SendServerCommand( challenged-g_entities, va("cp \"%s ^7%s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGE")) );
			trap->SendServerCommand( ent-g_entities, va("cp \"%s %s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGED"), challenged->client->pers.netname) );
		}

		challenged->client->ps.fd.privateDuelTime = 0; //reset the timer in case this player just got out of a duel. He should still be able to accept the challenge.

		ent->client->ps.forceHandExtend = HANDEXTEND_DUELCHALLENGE;
		ent->client->ps.forceHandExtendTime = level.time + 1000;

		ent->client->ps.duelIndex = challenged->s.number;
		ent->client->ps.duelTime = level.time + 5000;
	}
}

#ifndef FINAL_BUILD
extern stringID_table_t animTable[MAX_ANIMATIONS+1];

void Cmd_DebugSetSaberMove_f(gentity_t *self)
{
	int argNum = trap->Argc();
	char arg[MAX_STRING_CHARS];

	if (argNum < 2)
	{
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );

	if (!arg[0])
	{
		return;
	}

	self->client->ps.saberMove = atoi(arg);
	self->client->ps.saberBlocked = BLOCKED_BOUNCE_MOVE;

	if (self->client->ps.saberMove >= LS_MOVE_MAX)
	{
		self->client->ps.saberMove = LS_MOVE_MAX-1;
	}

	Com_Printf("Anim for move: %s\n", animTable[saberMoveData[self->client->ps.saberMove].animToUse].name);
}

void Cmd_DebugSetBodyAnim_f(gentity_t *self)
{
	int argNum = trap->Argc();
	char arg[MAX_STRING_CHARS];
	int i = 0;

	if (argNum < 2)
	{
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );

	if (!arg[0])
	{
		return;
	}

	while (i < MAX_ANIMATIONS)
	{
		if (!Q_stricmp(arg, animTable[i].name))
		{
			break;
		}
		i++;
	}

	if (i == MAX_ANIMATIONS)
	{
		Com_Printf("Animation '%s' does not exist\n", arg);
		return;
	}

	G_SetAnim(self, NULL, SETANIM_BOTH, i, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);

	Com_Printf("Set body anim to %s\n", arg);
}
#endif

void StandardSetBodyAnim(gentity_t *self, int anim, int flags)
{
	G_SetAnim(self, NULL, SETANIM_BOTH, anim, flags, 0);
}

void DismembermentTest(gentity_t *self);

void Bot_SetForcedMovement(int bot, int forward, int right, int up);

#ifndef FINAL_BUILD
extern void DismembermentByNum(gentity_t *self, int num);
extern void G_SetVehDamageFlags( gentity_t *veh, int shipSurf, int damageLevel );
#endif

// zyk: displays the yellow bar that shows the cooldown time between magic powers
void display_yellow_bar(gentity_t *ent, int duration)
{
	gentity_t *te = NULL;

	te = G_TempEntity( ent->client->ps.origin, EV_LOCALTIMER );
	te->s.time = level.time;
	te->s.time2 = duration;
	te->s.owner = ent->client->ps.clientNum;
}

// zyk: returns the max amount of Magic Power this player can have
int zyk_max_magic_power(gentity_t *ent)
{
	int max_mp = ent->client->pers.level * 3;

	// GalaxyRP fix: [Classes] the rpg_class==8 (Magic Master) branch granting extra Magic Power used to
	// be here. rpg_class is permanently 0 now that character classes are gone, so this was unreachable.

	return max_mp;
}

// GalaxyRP fix: [Magic] zyk_show_magic_in_chat() and zyk_set_magic_power_cooldown_time() removed
// outright. Both lost their only callers when the Ultimate Power (Ultra Drain/Immunity Power/Chaos
// Power/Time Power) and Magic Power (Ultra Strength/Ultra Resistance/Enemy Weakening) branches were
// removed from TryGrapple() below -- those seven branches were the sole reason either function
// existed (zyk_show_magic_in_chat() printed the "X used power!" chat line for them, and
// zyk_set_magic_power_cooldown_time() set their shared cooldown timer), and both gates that would
// ever let those branches fire (pers.defeated_guardians and pers.universe_quest_progress/
// universe_quest_counter) can never become nonzero -- see the matching fix comment on TryGrapple()'s
// old dispatch logic below for the full explanation. Grepped the whole tree first to confirm neither
// function has any other caller.

extern void poison_mushrooms(gentity_t *ent, int min_distance, int max_distance);
extern void magic_sense(gentity_t *ent, int duration);
extern void healing_water(gentity_t *ent, int heal_amount);
extern void earthquake(gentity_t *ent, int stun_time, int strength, int distance);
extern void blowing_wind(gentity_t *ent, int distance, int duration);
extern void sleeping_flowers(gentity_t *ent, int stun_time, int distance);
extern void time_power(gentity_t *ent, int distance, int duration);
extern void chaos_power(gentity_t *ent, int distance, int duration);
extern void water_splash(gentity_t *ent, int distance, int damage);
extern void ultra_flame(gentity_t *ent, int distance, int damage);
extern void rock_fall(gentity_t *ent, int distance, int damage);
extern void dome_of_damage(gentity_t *ent, int distance, int damage);
extern void ice_stalagmite(gentity_t *ent, int distance, int damage);
extern void ice_boulder(gentity_t *ent, int distance, int damage);
extern void hurricane(gentity_t *ent, int distance, int duration);
extern void slow_motion(gentity_t *ent, int distance, int duration);
extern void ultra_speed(gentity_t *ent, int duration);
extern void ultra_strength(gentity_t *ent, int duration);
extern void ultra_resistance(gentity_t *ent, int duration);
extern void immunity_power(gentity_t *ent, int duration);
extern void ultra_drain(gentity_t *ent, int radius, int damage, int duration);
extern void magic_shield(gentity_t *ent, int duration);
extern void healing_area(gentity_t *ent, int damage, int duration);
extern void lightning_dome(gentity_t *ent, int damage);
extern void magic_explosion(gentity_t *ent, int radius, int damage, int duration);
extern void flame_burst(gentity_t *ent, int duration);
extern void water_attack(gentity_t *ent, int distance, int damage);
extern void shifting_sand(gentity_t *ent, int distance);
extern void tree_of_life(gentity_t *ent);
extern void magic_disable(gentity_t *ent, int distance);
extern void fast_and_slow(gentity_t *ent, int distance, int duration);
extern void flaming_area(gentity_t *ent, int damage);
extern void reverse_wind(gentity_t *ent, int distance, int duration);
extern void enemy_nerf(gentity_t *ent, int distance);
extern void ice_block(gentity_t *ent, int duration);
// GalaxyRP fix: [Magic] removed magic_master_has_this_power() extern here — this function was never
// actually called from TryGrapple() below (or anywhere reachable), and has been removed as dead
// along with the rest of the magic-power selection system it gated (see g_main.c).
qboolean TryGrapple(gentity_t *ent)
{
	if (ent->client->ps.weaponTime > 0)
	{ //weapon busy
		return qfalse;
	}
	if (ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{ //force power or knockdown or something
		return qfalse;
	}
	if (ent->client->grappleState)
	{ //already grappling? but weapontime should be > 0 then..
		return qfalse;
	}

	if (ent->client->ps.weapon != WP_SABER && ent->client->ps.weapon != WP_MELEE)
	{
		return qfalse;
	}

	if (ent->client->ps.weapon == WP_SABER && !ent->client->ps.saberHolstered)
	{
		Cmd_ToggleSaber_f(ent);
		if (!ent->client->ps.saberHolstered)
		{ //must have saber holstered
			return qfalse;
		}
	}

	//G_SetAnim(ent, &ent->client->pers.cmd, SETANIM_BOTH, BOTH_KYLE_PA_1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);
	G_SetAnim(ent, &ent->client->pers.cmd, SETANIM_BOTH, BOTH_KYLE_GRAB, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);
	if (ent->client->ps.torsoAnim == BOTH_KYLE_GRAB)
	{ //providing the anim set succeeded..
		ent->client->ps.torsoTimer += 500; //make the hand stick out a little longer than it normally would
		if (ent->client->ps.legsAnim == ent->client->ps.torsoAnim)
		{
			ent->client->ps.legsTimer = ent->client->ps.torsoTimer;
		}
		ent->client->ps.weaponTime = ent->client->ps.torsoTimer;
		ent->client->dangerTime = level.time;
		
		if (ent->client->sess.amrpgmode == 2)
		{ // zyk: if this is a RPG player, tests if he can use a magic power
			if (ent->client->pers.quest_power_usage_timer < level.time)
			{
				// GalaxyRP fix: [Magic] the rightmove/forwardmove power-selection dispatch (use_this_power),
				// the magic_disabled_powers sanity check, the Magic Improvement mp-cost-factor perk
				// (universe_mp_cost_factor), and the Ultimate Power (Ultra Drain/Immunity Power/Chaos Power/
				// Time Power) and Magic Power (Ultra Strength/Ultra Resistance/Enemy Weakening) branches
				// themselves used to be here. All seven powers they could ever trigger turned out to be
				// permanently unreachable: every one of them is gated on pers.defeated_guardians and/or
				// pers.universe_quest_progress/universe_quest_counter being nonzero, and the only place
				// anywhere in the codebase that ever writes those fields is add_new_char(), which resets them
				// to 0 at character creation -- no quest completion, admin command, or database load ever
				// advances them (confirmed by grepping every assignment to all three fields). Removed the
				// whole dead dispatch outright, including the now-pointless use_this_power/
				// universe_mp_cost_factor locals that existed solely to feed it. zyk_show_magic_in_chat() and
				// zyk_set_magic_power_cooldown_time() lost their only callers here and have been removed too
				// (see their old location above). The effect functions themselves (ultra_drain(), time_power(),
				// etc. in g_main.c) are untouched -- they're still called by the NPC "custom quest npc" random-
				// power block there, so they're left exactly as they were.

				if (ent->client->pers.universe_quest_progress == NUM_OF_UNIVERSE_QUEST_OBJ && ent->client->pers.universe_quest_counter & (1 << 1) && 
					!(ent->client->sess.magic_more_disabled_powers & (1 << 1)))
				{ // zyk: Magic Boost, reward for completing quests in Guardians Sequel. Decreases cooldown time of magic powers
					// GalaxyRP fix: [Classes] the rpg_class==8 (Magic Master) shorter-cooldown branch used
					// to be here. rpg_class is permanently 0 now that character classes are gone, so this
					// was unreachable.
					ent->client->pers.quest_power_usage_timer -= 3000;
				}

				display_yellow_bar(ent,(ent->client->pers.quest_power_usage_timer - level.time));
			}
			else
			{
				trap->SendServerCommand( ent->s.number, va("chat \"^3Magic Power: ^7%d seconds left!\"", ((ent->client->pers.quest_power_usage_timer - level.time)/1000)));
			}

			send_rpg_events(2000);
		}

		return qtrue;
	}

	return qfalse;
}

void Cmd_TargetUse_f( gentity_t *ent )
{
	if ( trap->Argc() > 1 )
	{
		char sArg[MAX_STRING_CHARS] = {0};
		gentity_t *targ;

		trap->Argv( 1, sArg, sizeof( sArg ) );
		targ = G_Find( NULL, FOFS( targetname ), sArg );

		while ( targ )
		{
			if ( targ->use )
				targ->use( targ, ent, ent );
			targ = G_Find( targ, FOFS( targetname ), sArg );
		}
	}
}

void Cmd_TheDestroyer_f( gentity_t *ent ) {
	if ( !ent->client->ps.saberHolstered || ent->client->ps.weapon != WP_SABER )
		return;

	Cmd_ToggleSaber_f( ent );
}

void Cmd_BotMoveForward_f( gentity_t *ent ) {
	int arg = 4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, arg, -1, -1 );
}

void Cmd_BotMoveBack_f( gentity_t *ent ) {
	int arg = -4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, arg, -1, -1 );
}

void Cmd_BotMoveRight_f( gentity_t *ent ) {
	int arg = 4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, -1, arg, -1 );
}

void Cmd_BotMoveLeft_f( gentity_t *ent ) {
	int arg = -4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, -1, arg, -1 );
}

void Cmd_BotMoveUp_f( gentity_t *ent ) {
	int arg = 4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, -1, -1, arg );
}

void Cmd_AddBot_f( gentity_t *ent ) {
	//because addbot isn't a recognized command unless you're the server, but it is in the menus regardless
	trap->SendServerCommand( ent-g_entities, va( "print \"%s.\n\"", G_GetStringEdString( "MP_SVGAME", "ONLY_ADD_BOTS_AS_SERVER" ) ) );
}

// zyk: new functions

// zyk: send the rpg events to the client-side game to all players so players who connect later than one already in the map
//      will receive the events of the one in the map
void send_rpg_events(int send_event_timer)
{
	int i = 0;
	gentity_t *player_ent = NULL;

	for (i = 0; i < level.maxclients; i++)
	{
		player_ent = &g_entities[i];

		if (player_ent && player_ent->client && player_ent->client->pers.connected == CON_CONNECTED && 
			player_ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		{
			player_ent->client->pers.send_event_timer = level.time + send_event_timer;
			player_ent->client->pers.send_event_interval = level.time + 100;
			player_ent->client->pers.player_statuses &= ~(1 << 2);
			player_ent->client->pers.player_statuses &= ~(1 << 3);
		}
	}
}

// zyk: sets the Max HP a player can have in RPG Mode
void set_max_health(gentity_t *ent)
{
	ent->client->pers.max_rpg_health = 100 + (ent->client->pers.level * 2);
	ent->client->ps.stats[STAT_MAX_HEALTH] = ent->client->pers.max_rpg_health;
}

// zyk: sets the Max Shield a player can have in RPG Mode
void set_max_shield(gentity_t *ent)
{
	ent->client->pers.max_rpg_shield = (int)ceil(((ent->client->pers.skill_levels[30] * 1.0)/5) * ent->client->pers.max_rpg_health);
}

// zyk: gives credits to the player
void add_credits(gentity_t *ent, int credits)
{
	ent->client->pers.credits += credits;
	if (ent->client->pers.credits > zyk_max_rpg_credits.integer)
		ent->client->pers.credits = zyk_max_rpg_credits.integer;
}

// zyk: removes credits from the player
void remove_credits(gentity_t *ent, int credits)
{
	ent->client->pers.credits -= credits;
	if (ent->client->pers.credits < 0)
		ent->client->pers.credits = 0;
}

// zyk: loads settings valid both to Admin-Only Mode and to RPG Mode
void zyk_load_common_settings(gentity_t *ent)
{
	// zyk: loading the starting weapon based in player settings
	if (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER) && !(ent->client->pers.player_settings & (1 << 11)))
	{
		ent->client->ps.weapon = WP_SABER;
	}
	else
	{
		ent->client->ps.weapon = WP_MELEE;
	}

	if (!(ent->client->saber[0].model[0] && ent->client->saber[1].model[0]) && !(ent->client->saber[0].saberFlags&SFL_TWO_HANDED))
	{ // zyk: Single Saber
		ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel = ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE];

		// GalaxyRP fix: [Account] amrpgmode == 1 ("account mode", as opposed to == 2 for RPG mode)
		// hasn't been assignable anywhere in the codebase since commit 5d35b28b8 (2022) made login
		// always set amrpgmode = 2 "kept for backwards compatibility" -- so the amrpgmode == 1 half
		// of each of these OR-conditions could never be true and is being dropped as dead code.
		if (ent->client->pers.player_settings & (1 << 26) &&
			ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[5] >= 2)
		{
			// ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel = SS_MEDIUM;
			// ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
			ent->client->ps.fd.saberAnimLevel = SS_MEDIUM;
		}
		else if (ent->client->pers.player_settings & (1 << 27) &&
				ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[5] >= 3)
		{
			ent->client->ps.fd.saberAnimLevel = SS_STRONG;
		}
		else if (ent->client->pers.player_settings & (1 << 28) && ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[5] >= 4)
		{
			ent->client->ps.fd.saberAnimLevel = SS_DESANN;
		}
		else if (ent->client->pers.player_settings & (1 << 29) && ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[5] == 5)
		{
			ent->client->ps.fd.saberAnimLevel = SS_TAVION;
		}
		else if (ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[5] >= 1)
		{
			ent->client->ps.fd.saberAnimLevel = SS_FAST;
		}
	}
}

// zyk: saves info into the player account file. If save_char_file is qtrue, this function must save the char file
void save_account(gentity_t *ent, qboolean save_char_file)
{
	// zyk: used to prevent account save in map change time or before loading account after changing map
	// GalaxyRP fix: [Cvars] zyk_rp_mode and zyk_allow_saving_in_rp_mode were both removed (the server is
	// always considered to be in RP Mode, and saving during RP Mode was always allowed by default), so
	// the old "(zyk_rp_mode.integer != 1 || zyk_allow_saving_in_rp_mode.integer == 1)" check simplifies
	// away entirely -- any logged-in player can now always be saved here.
	// GalaxyRP fix: [Account] the amrpgmode == 1 half of this OR is separately dead -- see the comment
	// above the saber anim level selection earlier in this file for why amrpgmode can no longer be 1.
	if (level.voteExecuteTime < level.time && ent->client->pers.connected == CON_CONNECTED &&
		ent->client->sess.amrpgmode == 2
		)
	{ // zyk: players can only save things if server is not at RP Mode or if it is allowed in config
		if (save_char_file == qtrue)
		{  // zyk: save the RPG char
			update_current_character_and_account(ent);
		}
		else
		{ // zyk: save the main account file
			update_accounts_table_row_with_current_values(ent);
		}
	}
}

// GalaxyRP fix: [macOS/Clang build failure] old K&R-style declaration left max_value with no
// type, defaulting to int under pre-C99 rules ("implicit int"). ISO C99 and later, and Clang by
// default, reject this as a hard error (-Wimplicit-int). Give it its always-intended type.
int roll_dice(int max_value) {
	int result = rand() % (max_value + 1);
	while (result == 0) {
		result = rand() % (max_value + 1);
	}

	return result;
}

/*
==================
Cmd_Roll_f
==================
*/

void Cmd_Roll_f(gentity_t *ent) {

	char arg1[MAX_STRING_CHARS];

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1Command Usage: ^2/roll ^3<max roll>.\n^1Example: ^2/roll ^320\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	if (StringIsInteger(arg1) == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"Argument must be an integer.\n\"");
		return;
	}

	int max_value = atoi(arg1);

	if (max_value < 2) {
		trap->SendServerCommand(ent - g_entities, "print \"Maximum value must be at least two.\n\"");
		return;
	}

	int result = roll_dice(max_value);

	trap->SendServerCommand(-1, va("chat \"^3<Dice Roll> %s^2 rolled a ^3%d^2 out of ^3%d\n\"", ent->client->pers.netname, result, max_value));

	return;
}

/*
==================
Cmd_FlipCoin_f
==================
*/

void Cmd_FlipCoin_f(gentity_t *ent) {

	if (trap->Argc() != 1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1Command Usage: ^2/flipcoin\n\"");
		return;
	}

	int result = roll_dice(2);

	if (result == 1) {
		trap->SendServerCommand(-1, va("chat \"^3%s^2 flipped a coin that landed on ^3HEADS.\n\"", ent->client->pers.netname));
	}
	else {
		trap->SendServerCommand(-1, va("chat \"^3%s^2 flipped a coin that landed on ^3TAILS.\n\"", ent->client->pers.netname));
	}

	return;
}

// GalaxyRP fix: [Classes] validate_rpg_class() used to live here (already a stub returning qtrue,
// tagged "TODO: Remove this method"). Deleted outright along with its sole call site in
// initialize_rpg_skills() below (the `if (validate_rpg_class(ent) == qfalse) return;` guard it was
// gating, which is now unreachable).

// GalaxyRP fix: [Quests] number_of_artifacts, number_of_amulets, and number_of_crystals used to live
// here. All three were only ever called from the general (non-Guardian/Bounty) automated quest system
// (universe_quest_artifacts_checker/universe_crystals_check in g_main.c and got_all_amulets below),
// which is itself fully unreachable now that quest_get_new_player's sole gate is permanently disabled
// (see the GalaxyRP fix comment further down in this file). Deleted outright along with their callers.

// GalaxyRP fix: [Jetpack] shared gate for rp_allow_jetpack_command, used by both the RPG-mode
// auto-grant below (initialize_rpg_skills) and the /jetpack command handler (Cmd_Jetpack_f) so the
// two stay in sync. 0 disables the jetpack command entirely, 1 enables it for everyone, and 2
// enables it only for players currently logged into an account (sess.loggedin -- set qtrue by
// Cmd_Login_f/Cmd_Register_f/Cmd_GalaxyRpUi_f, cleared qfalse by Cmd_Logout_f). Formerly
// zyk_allow_jetpack_command -- renamed to the rp_ prefix used by GalaxyRP's own cvars.
qboolean jetpack_command_allowed(gentity_t *ent)
{
	if (rp_allow_jetpack_command.integer <= 0)
		return qfalse;

	if (rp_allow_jetpack_command.integer == 2 && ent->client->sess.loggedin == qfalse)
		return qfalse;

	return qtrue;
}

// zyk: initialize RPG skills of this player
void initialize_rpg_skills(gentity_t *ent)
{
	if (ent->client->sess.amrpgmode == 2)
	{
		// GalaxyRP fix: [Challenge Mode] a universe_quest_progress==15 block that re-set the Challenge
		// Mode flag (player_settings bit 15) here used to be here. universe_quest_progress can never
		// reach 15 (its only setter resets it to 0), so this was unreachable; it is also moot now that
		// /settings 15 no longer exists.

		// zyk: loading Jump value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_LEVITATION)) && ent->client->pers.skill_levels[0] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_LEVITATION);
		if (ent->client->pers.skill_levels[0] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_LEVITATION);
		ent->client->ps.fd.forcePowerLevel[FP_LEVITATION] = ent->client->pers.skill_levels[0];

		// zyk: loading Push value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_PUSH)) && ent->client->pers.skill_levels[1] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_PUSH);
		if (ent->client->pers.skill_levels[1] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PUSH);
		ent->client->ps.fd.forcePowerLevel[FP_PUSH] = ent->client->pers.skill_levels[1];

		// zyk: loading Pull value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_PULL)) && ent->client->pers.skill_levels[2] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_PULL);
		if (ent->client->pers.skill_levels[2] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PULL);
		ent->client->ps.fd.forcePowerLevel[FP_PULL] = ent->client->pers.skill_levels[2];

		// zyk: loading Speed value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SPEED)) && ent->client->pers.skill_levels[3] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SPEED);
		if (ent->client->pers.skill_levels[3] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SPEED);
		ent->client->ps.fd.forcePowerLevel[FP_SPEED] = ent->client->pers.skill_levels[3];

		// zyk: loading Sense value
		// GalaxyRP fix: [Classes] the rpg_class==2/3/5/8 (force-less classes skip Sense) branch used to
		// be here. rpg_class is permanently 0 now that character classes are gone, so this was
		// unreachable; only the else branch (load Sense normally) is kept, unconditionally.
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SEE)) && ent->client->pers.skill_levels[4] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SEE);
		if (ent->client->pers.skill_levels[4] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SEE);
		ent->client->ps.fd.forcePowerLevel[FP_SEE] = ent->client->pers.skill_levels[4];

		// zyk: loading Saber Offense value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SABER_OFFENSE)) && ent->client->pers.skill_levels[5] > 0)
		{
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SABER_OFFENSE);
		}
		if (ent->client->pers.skill_levels[5] == 0)
		{
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SABER_OFFENSE);
		}
		ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] = ent->client->pers.skill_levels[5];

		// zyk: giving the saber if he has Saber Attack skill level greater than 0
		if (ent->client->pers.skill_levels[5] > 0)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);
		}
		else
		{
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
		}

		// zyk: loading Saber Defense value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SABER_DEFENSE)) && ent->client->pers.skill_levels[6] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SABER_DEFENSE);
		if (ent->client->pers.skill_levels[6] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SABER_DEFENSE);
		ent->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] = ent->client->pers.skill_levels[6];

		// zyk: loading Saber Throw value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SABERTHROW)) && ent->client->pers.skill_levels[7] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SABERTHROW);
		if (ent->client->pers.skill_levels[7] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SABERTHROW);
		ent->client->ps.fd.forcePowerLevel[FP_SABERTHROW] = ent->client->pers.skill_levels[7];

		// zyk: loading Absorb value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_ABSORB)) && ent->client->pers.skill_levels[8] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_ABSORB);
		if (ent->client->pers.skill_levels[8] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_ABSORB);

		if (ent->client->pers.skill_levels[8] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_ABSORB] = ent->client->pers.skill_levels[8];
		else
			ent->client->ps.fd.forcePowerLevel[FP_ABSORB] = FORCE_LEVEL_3;

		// zyk: loading Heal value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_HEAL)) && ent->client->pers.skill_levels[9] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_HEAL);
		if (ent->client->pers.skill_levels[9] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_HEAL);
		ent->client->ps.fd.forcePowerLevel[FP_HEAL] = ent->client->pers.skill_levels[9];

		// zyk: loading Protect value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_PROTECT)) && ent->client->pers.skill_levels[10] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_PROTECT);
		if (ent->client->pers.skill_levels[10] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PROTECT);

		if (ent->client->pers.skill_levels[10] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_PROTECT] = ent->client->pers.skill_levels[10];
		else
			ent->client->ps.fd.forcePowerLevel[FP_PROTECT] = FORCE_LEVEL_3;

		// zyk: loading Mind Trick value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_TELEPATHY)) && ent->client->pers.skill_levels[11] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_TELEPATHY);
		if (ent->client->pers.skill_levels[11] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TELEPATHY);
		ent->client->ps.fd.forcePowerLevel[FP_TELEPATHY] = ent->client->pers.skill_levels[11];

		// zyk: loading Team Heal value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_HEAL)) && ent->client->pers.skill_levels[12] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_TEAM_HEAL);
		if (ent->client->pers.skill_levels[12] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TEAM_HEAL);
		ent->client->ps.fd.forcePowerLevel[FP_TEAM_HEAL] = ent->client->pers.skill_levels[12];

		// zyk: loading Lightning value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_LIGHTNING)) && ent->client->pers.skill_levels[13] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_LIGHTNING);
		if (ent->client->pers.skill_levels[13] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_LIGHTNING);

		if (ent->client->pers.skill_levels[13] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_LIGHTNING] = ent->client->pers.skill_levels[13];
		else
			ent->client->ps.fd.forcePowerLevel[FP_LIGHTNING] = FORCE_LEVEL_3;

		// zyk: loading Grip value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_GRIP)) && ent->client->pers.skill_levels[14] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_GRIP);
		if (ent->client->pers.skill_levels[14] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_GRIP);
		ent->client->ps.fd.forcePowerLevel[FP_GRIP] = ent->client->pers.skill_levels[14];

		// zyk: loading Drain value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_DRAIN)) && ent->client->pers.skill_levels[15] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_DRAIN);
		if (ent->client->pers.skill_levels[15] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_DRAIN);
		ent->client->ps.fd.forcePowerLevel[FP_DRAIN] = ent->client->pers.skill_levels[15];

		// zyk: loading Rage value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_RAGE)) && ent->client->pers.skill_levels[16] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_RAGE);
		if (ent->client->pers.skill_levels[16] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_RAGE);

		if (ent->client->pers.skill_levels[16] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_RAGE] = ent->client->pers.skill_levels[16];
		else
			ent->client->ps.fd.forcePowerLevel[FP_RAGE] = FORCE_LEVEL_3;

		// zyk: loading Team Energize value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_FORCE)) && ent->client->pers.skill_levels[17] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_TEAM_FORCE);
		if (ent->client->pers.skill_levels[17] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TEAM_FORCE);
		ent->client->ps.fd.forcePowerLevel[FP_TEAM_FORCE] = ent->client->pers.skill_levels[17];

		// zyk: loading Melee
		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_MELEE)))
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);

		zyk_load_common_settings(ent);

		ent->client->pers.sense_health_timer = 0;

		// zyk: used to add a cooldown between each flame
		ent->client->cloakDebReduce = 0;

		ent->client->pers.max_force_power = (int)ceil((zyk_max_force_power.value/4.0) * ent->client->pers.skill_levels[54]);
		ent->client->ps.fd.forcePowerMax = ent->client->pers.max_force_power;
		ent->client->ps.fd.forcePower = ent->client->ps.fd.forcePowerMax;

		// zyk: setting rpg control attributes
		ent->client->pers.thermal_vision = qfalse;
		ent->client->pers.thermal_vision_cooldown_time = 0;

		ent->client->pers.quest_power_status = 0;

		ent->client->pers.magic_power = zyk_max_magic_power(ent);

		// GalaxyRP fix: [Skills] cg.magic_power (cgame's mirror of this, drawn by CG_DrawMagicPower)
		// only updates when it receives this event -- normally sent by the periodic per-player sync
		// cascade in g_active.c's ClientThink, which can take several ticks to get back around to
		// resending it, or may not run again at all this life if its window already closed. That let
		// the Magic Power bar keep showing a stale (sometimes near-empty) value from the player's
		// previous life right after login/character-select/respawn, even though the real value was
		// already reset to full above. Send the correct value (always 100% here) immediately instead
		// of waiting on the cascade.
		G_AddEvent(ent, EV_USE_ITEM13, 100);

		ent->client->pers.monk_unique_timer = 0;
		ent->client->pers.unique_skill_duration = 0;

		ent->client->pers.credits_modifier = 0;
		ent->client->pers.score_modifier = 0;

		ent->client->pers.buy_sell_timer = 0;
		ent->client->pers.vertical_dfa_timer = 0;

		// zyk: setting default value of can_play_quest
		ent->client->pers.can_play_quest = 0;

		// GalaxyRP fix: [Guardian] the guardian_mode=0 and guardian_invoked_by_id=-1 resets that used to
		// be here are removed along with the fields themselves (guardian_mode's sole setter, and
		// guardian_invoked_by_id's sole setter spawn_boss(), have zero callers). guardian_timer stays
		// live and is left reset below (used by the quest_mage/ymir/thor ability chain in g_main.c).
		ent->client->pers.guardian_timer = 0;

		ent->client->pers.eternity_quest_timer = 0;

		ent->client->pers.universe_quest_artifact_holder_id = -1;
		ent->client->pers.universe_quest_messages = 0;
		ent->client->pers.universe_quest_timer = 0;

		ent->client->pers.light_quest_timer = 0;
		ent->client->pers.light_quest_messages = 0;

		ent->client->pers.hunter_quest_timer = 0;
		ent->client->pers.hunter_quest_messages = 0;

		// zyk: loading initial RPG weapons
		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_STUN_BATON)) && ent->client->pers.skill_levels[18] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_STUN_BATON);
		if (ent->client->pers.skill_levels[18] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_STUN_BATON);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_PISTOL)) && ent->client->pers.skill_levels[19] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);
		if (ent->client->pers.skill_levels[19] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BRYAR_PISTOL);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BLASTER)) && ent->client->pers.skill_levels[20] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BLASTER);
		if (ent->client->pers.skill_levels[20] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BLASTER);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DISRUPTOR)) && ent->client->pers.skill_levels[21] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DISRUPTOR);
		if (ent->client->pers.skill_levels[21] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_DISRUPTOR);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BOWCASTER)) && ent->client->pers.skill_levels[22] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BOWCASTER);
		if (ent->client->pers.skill_levels[22] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BOWCASTER);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_REPEATER)) && ent->client->pers.skill_levels[23] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_REPEATER);
		if (ent->client->pers.skill_levels[23] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_REPEATER);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DEMP2)) && ent->client->pers.skill_levels[24] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DEMP2);
		if (ent->client->pers.skill_levels[24] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_DEMP2);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_FLECHETTE)) && ent->client->pers.skill_levels[25] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_FLECHETTE);
		if (ent->client->pers.skill_levels[25] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_FLECHETTE);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_ROCKET_LAUNCHER)) && ent->client->pers.skill_levels[26] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_ROCKET_LAUNCHER);
		if (ent->client->pers.skill_levels[26] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_ROCKET_LAUNCHER);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_CONCUSSION)) && ent->client->pers.skill_levels[27] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_CONCUSSION);
		if (ent->client->pers.skill_levels[27] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_CONCUSSION);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_OLD)) && ent->client->pers.skill_levels[28] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_OLD);
		if (ent->client->pers.skill_levels[28] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BRYAR_OLD);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_THERMAL)) && ent->client->pers.skill_levels[43] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_THERMAL);
		if (ent->client->pers.skill_levels[43] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_THERMAL);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_TRIP_MINE)) && ent->client->pers.skill_levels[44] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_TRIP_MINE);
		if (ent->client->pers.skill_levels[44] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_TRIP_MINE);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DET_PACK)) && ent->client->pers.skill_levels[45] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DET_PACK);
		if (ent->client->pers.skill_levels[45] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_DET_PACK);

		// zyk: reseting initial holdable items of the player
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER) & ~(1 << HI_BINOCULARS) & ~(1 << HI_SENTRY_GUN) & ~(1 << HI_EWEB) & ~(1 << HI_CLOAK) & ~(1 << HI_SHIELD) & ~(1 << HI_MEDPAC) & ~(1 << HI_MEDPAC_BIG);

		if (ent->client->pers.skill_levels[46] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_BINOCULARS);

		if (ent->client->pers.skill_levels[47] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC);

		if (ent->client->pers.skill_levels[48] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN);

		if (ent->client->pers.skill_levels[49] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SEEKER);

		if (ent->client->pers.skill_levels[50] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_EWEB);

		if (ent->client->pers.skill_levels[51] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC_BIG);

		if (ent->client->pers.skill_levels[52] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SHIELD);

		if (ent->client->pers.skill_levels[53] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_CLOAK);

		// GalaxyRP fix: [Jetpack] zyk_allow_jetpack_in_siege has been removed -- jetpack is now never
		// allowed in Siege, full stop (this is exactly the cvar's old default, "0"/never-allow, just no
		// longer configurable).
		if (jetpack_command_allowed(ent) &&
			level.gametype != GT_SIEGE && level.gametype != GT_JEDIMASTER &&
			(ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[34] > 0))
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

		// zyk: loading initial health of the player
		set_max_health(ent);
		ent->health = ent->client->pers.max_rpg_health;
		ent->client->ps.stats[STAT_HEALTH] = ent->health;

		// zyk: loading initial shield of the player
		set_max_shield(ent);
		ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.max_rpg_shield;

		// zyk: update the rpg stuff info at the client-side game
		send_rpg_events(10000);
	}
}

/*
==================
Cmd_DateTime_f
==================
*/
void Cmd_DateTime_f( gentity_t *ent ) {
	time_t current_time;

	time(&current_time);
	// zyk: shows current server date and time
	trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", ctime(&current_time)) ); 
}

// TODO: Char class is set here, to be removed.
// zyk: adds a new RPG char with default values
void add_new_char(gentity_t *ent)
{
	int i = 0;

	ent->client->pers.level_up_score = 0;
	ent->client->pers.level = 1;
	ent->client->pers.skillpoints = 1;

	for (i = 0; i < NUM_OF_SKILLS; i++)
	{
		ent->client->pers.skill_levels[i] = 0;
	}

	ent->client->pers.defeated_guardians = 0;
	// GalaxyRP fix: [Quests] hunter_quest_progress/eternity_quest_progress resets removed here — both fields removed as dead (see g_local.h)
	ent->client->pers.secrets_found = 0;
	ent->client->pers.universe_quest_progress = 0;
	ent->client->pers.universe_quest_counter = 0;
	ent->client->pers.credits = 100;
	// GalaxyRP fix: [Classes] rpg_class is permanently 0 now that character classes are gone; the
	// reset-to-0 assignment that used to be here (its sole assignment anywhere) has been removed.
	ent->client->sess.magic_disabled_powers = 0;
	ent->client->sess.magic_more_disabled_powers = 0;
	ent->client->sess.selected_special_power = MAGIC_MAGIC_SENSE;
	ent->client->sess.selected_left_special_power = MAGIC_MAGIC_SENSE;
	ent->client->sess.selected_right_special_power = MAGIC_MAGIC_SENSE;
	ent->client->sess.magic_fist_selection = 0;

	// GalaxyRP fix: [Challenge Mode] the reset that used to clear the Challenge Mode flag
	// (player_settings bit 15) here for a new char has been removed since /settings 15 no longer
	// exists. universe_quest_counter is already fully zeroed just above (it also holds unrelated
	// live bits, e.g. bits 0-3, so that whole-field reset is left in place).
}

// GalaxyRP: Sets up the GalaxyRP directory
void zyk_create_dir(char *file_path)
{
#if defined(__linux__)
	system(va("mkdir -p GalaxyRP%s", file_path));
#else
	system(va("mkdir \"GalaxyRP/%s\"", file_path));
#endif
}

// GalaxyRP fix: [Guardian] clean_guardians() used to live here. Its entire body was permanently a
// no-op, gated on the permanently-false condition pers.guardian_invoked_by_id != -1 (the field's sole
// setter, spawn_boss(), has zero callers and is being removed in g_main.c). Deleted outright, along
// with clean_effect() just below and all guardian_mode-gated command guards throughout this file.
//
// GalaxyRP fix: [Quests] light_quest_defeated_guardians, dark_quest_collected_notes, load_note_model,
// load_crystal_model, load_effect, clean_note_model, and clean_crystal_model used to live here. All
// were exclusively part of the general (non-Guardian/Bounty) automated quest system's map-note/
// map-crystal/map-effect spawning, driven entirely by choose_new_player/quest_get_new_player further
// down in this file -- which is itself fully unreachable (see the GalaxyRP fix comment on
// quest_get_new_player below). Deleted outright, along with the level.quest_note_id/
// level.universe_quest_note_id/level.quest_crystal_id fields they were the sole users of.
//
// GalaxyRP fix: [Guardian] clean_effect() also used to live here. It was kept in that earlier pass
// because it was still called unconditionally from spawn_boss() (the guardian_mode boss-battle
// spawner); now that spawn_boss() itself is being deleted (its sole caller, in g_main.c), clean_effect()
// has zero callers and is deleted too. This orphans level.quest_effect_id (declared in g_local.h,
// initialized in g_main.c) for a later cleanup pass to remove.
extern void zyk_set_entity_field(gentity_t *ent, char *key, char *value);
extern void zyk_spawn_entity(gentity_t *ent);
extern void zyk_main_set_entity_field(gentity_t *ent, char *key, char *value);
extern void zyk_main_spawn_entity(gentity_t *ent);

// GalaxyRP fix: [Quests] got_all_amulets, zyk_number_of_completed_quests, choose_new_player, and
// quest_get_new_player used to live here. quest_get_new_player is the sole setter of
// pers.can_play_quest anywhere in the codebase, and it was disabled with an unconditional early
// return in an earlier change ("Remove dead server cvars and the game code paths that referenced
// them") -- GalaxyRP's current gameplay is admin-driven RP and manual leveling rather than automated
// quests, and this system was already unreachable in practice by default before that (zyk_allow_quests
// defaulted to 0, and zyk_rp_mode defaulted to 1, which independently also blocked it). That single
// permanently-false gate makes this entire general (non-Guardian/Bounty) automated quest system --
// Light/Dark/Hunter/Eternity/Universe Quest map-turn selection alike -- unreachable, so all four
// functions have been deleted outright along with their direct call sites elsewhere in the codebase
// (g_main.c, g_client.c, g_combat.c, g_items.c, g_utils.c). The pers.* quest-progress fields these
// functions read/wrote (defeated_guardians, universe_quest_progress, can_play_quest, etc.) are kept
// -- they are still read by live code elsewhere (the magic-power selection system), so only the dead
// selection logic itself is removed here.
// GalaxyRP fix: [Quests] correction, made in a later pass: this comment previously also listed
// hunter_quest_progress and eternity_quest_progress as "still read by live code elsewhere" and cited
// "/settings Challenge Mode" as a reader -- both were wrong. hunter_quest_progress/
// eternity_quest_progress turned out to have zero readers anywhere (they've since been removed as
// dead, see g_local.h) and Challenge Mode itself was removed as dead in an earlier pass than this
// comment's own claim.

// zyk: tests if the race must be finished
void try_finishing_race()
{
	int j = 0, has_someone_racing = 0;
	gentity_t *this_ent = NULL;

	if (level.race_mode != 0)
	{
		for (j = 0; j < level.maxclients; j++)
		{ 
			this_ent = &g_entities[j];
			if (this_ent && this_ent->client && this_ent->inuse && this_ent->health > 0 && this_ent->client->sess.sessionTeam != TEAM_SPECTATOR && this_ent->client->pers.race_position > 0)
			{ // zyk: searches for the players who are still racing to see if we must finish the race
				has_someone_racing = 1;
			}
		}

		if (has_someone_racing == 0)
		{ // zyk: no one is racing, so finish the race
			level.race_mode = 0;

			for (j = MAX_CLIENTS; j < level.num_entities; j++)
			{
				this_ent = &g_entities[j];
				if (this_ent && Q_stricmp(this_ent->targetname, "zyk_race_line") == 0)
				{ // zyk: removes this start or finish line
					G_FreeEntity(this_ent);
				}
			}

			trap->SendServerCommand( -1, va("chat \"^3Race System: ^7The race is over!\""));
		}
	}
}

/*
==================
Cmd_LogoutAccount_f
==================
*/
void Cmd_LogoutAccount_f( gentity_t *ent ) {

	save_account(ent, qtrue);

	if (ent->client->pers.being_mind_controlled != -1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You cant logout while being mind-controlled.\n\"" );
		return;
	}

	// GalaxyRP fix: [Classes] the rpg_class==1 (Force User) Mind Control logout guard used to be here.
	// rpg_class is permanently 0 now that character classes are gone, so this was unreachable.

	if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot logout while in a Duel Tournament\n\"");
		return;
	}

	if (level.sniper_mode > 0 && level.sniper_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot logout while in a Sniper Battle\n\"");
		return;
	}

	// GalaxyRP fix: [Guardian] the guardian_mode>0 clean_guardians() call used to be here. guardian_mode
	// is permanently 0 now (its sole setter, spawn_boss(), has zero callers and is being removed in
	// g_main.c), so this was unreachable; clean_guardians() itself has also been deleted.

	// zyk: saving the not logged player mode in session
	ent->client->sess.amrpgmode = 0;

	// GalaxyRP fix: [Account] sess.rpgchar (the name of the currently-selected character) was never
	// cleared here, unlike amrpgmode right above it -- so it kept naming whatever character was active
	// when this account logged out. Harmless while amrpgmode stays 0 (nothing reads rpgchar in that
	// state), but a second /login on the same connection (Cmd_Login_F only blocks a re-login while
	// still logged in, not after a /logout) would otherwise carry a stale character name from the
	// previous account straight into select_account_and_default_character_data()'s sess.rpgchar-aware
	// lookup below. Reset it here so a fresh login always falls back to the account's DefaultChar.
	ent->client->sess.rpgchar[0] = '\0';

	// GalaxyRP fix: [Guardian] removed the `if (can_play_quest == 1) { boss_battle_music_reset_timer
	// = ...; }` block here -- can_play_quest can no longer become 1 anywhere (see the GalaxyRP fix
	// comment on quest_get_new_player's old location further up in this file), and
	// boss_battle_music_reset_timer itself has now been removed as dead (see g_local.h).

	ent->client->pers.bitvalue = 0;

	// GalaxyRP fix: [Account] pers.player_settings (the /settings bitmask) was never reset here,
	// unlike pers.bitvalue right above it -- so it kept whatever this account's settings were in
	// memory after logout. Harmless on its own (nothing reads player_settings while amrpgmode != 2),
	// but insert_accounts_table_row() had briefly bound this same field for a newly /new-created
	// account, which would have leaked a leftover value across accounts in the same connection.
	// Reset it here too for the same reason bitvalue is reset, and so pers.player_settings is always
	// trustworthy as "this connection's currently logged-in account's settings, or 0".
	ent->client->pers.player_settings = 0;

	// zyk: initializing mind control attributes used in RPG mode
	ent->client->pers.being_mind_controlled = -1;
	ent->client->pers.mind_controlled1_id = -1;

	// zyk: resetting the forcePowerMax to the cvar value
	ent->client->ps.fd.forcePowerMax = zyk_max_force_power.integer;

	// GalaxyRP fix: [Scale] every other logged-in-only effect here (bitvalue, player_settings, force
	// powers, health/armor caps, RPG weapons via zyk_remove_guns() below) gets reset to its baseline on
	// logout, but /scale's size was never among them -- a scaled player stayed whatever size they were
	// left at while logged in, all the way through logout, instead of visually returning to default like
	// everything else. This only resets the in-memory/visual state (do_scale() never touches the DB) --
	// the character's actually-saved ModelScale is untouched and gets restored correctly on the next
	// /login via select_account_and_default_character_data().
	do_scale(ent, 100);

	// zyk: resetting max hp and shield to 100
	ent->client->ps.stats[STAT_MAX_HEALTH] = 100;

	if (ent->health > 100)
		ent->health = 100;

	if (ent->client->ps.stats[STAT_ARMOR] > 100)
		ent->client->ps.stats[STAT_ARMOR] = 100;

	// GalaxyRP fix: [Account] logging out only ever re-initialized force powers here (WP_InitForcePowers)
	// and then additively OR'd the baseline saber/Bryar Pistol bits into STAT_WEAPONS -- it never cleared
	// STAT_WEAPONS first, so any weapon (and its ammo) unlocked by RPG skills via /give or
	// initialize_rpg_skills() stayed equipped forever after logout. zyk_remove_guns() is the same helper
	// /give's toggle-off path already uses to strip RPG weapons back to the logged-out baseline (melee +
	// conditional saber/Bryar Pistol) and to re-init force powers, so reuse it here instead of duplicating
	// half of it inline.
	zyk_remove_guns(ent);

	// zyk_remove_guns() always grants saber (conditional on force level) and Bryar Pistol unconditionally,
	// with no gametype exclusion -- strip them back out for Jedi Master/Siege, matching this function's
	// pre-existing exclusion for those gametypes.
	if (level.gametype == GT_JEDIMASTER || level.gametype == GT_SIEGE)
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BRYAR_PISTOL);
	}

	ent->client->sess.loggedin = qfalse;

	// GalaxyRP: [Force Enlightenment] see the matching comment in g_client.c -- keeps cgame's
	// CG_GreyItem in sync with login state right away (goes back to greying out the "wrong side"
	// Enlightenment pickup once this player is logged out again).
	trap->SendServerCommand(ent->s.number, va("supdateloggedin %i\n", ent->client->sess.loggedin));

	// zyk: update the rpg stuff info at the client-side game
	send_rpg_events(10000);
			
	trap->SendServerCommand(-1, va("chat \"^3%s ^2logged out\n\"", ent->client->pers.netname));

	trap->SendServerCommand(ent - g_entities, "print \"^2You have sucessfully logged out.\n\"");
	trap->SendServerCommand(ent - g_entities, "cp \"^2You have sucessfully logged out.\n\"");
}

char *zyk_get_settings_values(gentity_t *ent)
{
	int i = 0;
	char content[1024];

	strcpy(content,"");

	for (i = 0; i < 16; i++)
	{ // zyk: settings values
		if (i != 5 && i != 8 && i != 14 && i != 15)
		{
			if (!(ent->client->pers.player_settings & (1 << i)))
			{
				strcpy(content,va("%sON-",content));
			}
			else
			{
				strcpy(content,va("%sOFF-",content));
			}
		}
		else if (i == 5)
		{
			if (!(ent->client->pers.player_settings & (1 << i)))
			{
				strcpy(content, va("%sEnglish-", content));
			}
			else
			{
				strcpy(content, va("%sCustom-", content));
			}
		}
		else if (i == 14)
		{
			if (ent->client->pers.player_settings & (1 << 24))
			{
				strcpy(content,va("%sKorriban Action-",content));
			}
			else if (ent->client->pers.player_settings & (1 << 25))
			{
				strcpy(content,va("%sMP Duel-",content));
			}
			else if (ent->client->pers.player_settings & (1 << 14))
			{
				strcpy(content,va("%sCustom-",content));
			}
			else 
			{
				strcpy(content,va("%sHoth2 Action-",content));
			}

		}
		else if (i == 15)
		{
			if (!(ent->client->pers.player_settings & (1 << i)))
			{
				strcpy(content,va("%sNormal-",content));
			}
			else
			{
				strcpy(content,va("%sChallenge-",content));
			}
		}
		else
		{ // zyk: starting saber style has its own handling code
			if (ent->client->pers.player_settings & (1 << 27))
			{
				strcpy(content,va("%sRed-",content));
			}
			else if (ent->client->pers.player_settings & (1 << 28))
			{
				strcpy(content,va("%sDesann-",content));
			}
			else if (ent->client->pers.player_settings & (1 << 29))
			{
				strcpy(content,va("%sTavion-",content));
			}
			else if (ent->client->pers.player_settings & (1 << 26))
			{
				strcpy(content,va("%sYellow-",content));
			}
			else
			{
				strcpy(content,va("%sBlue-",content));
			}
		}
	}

	// zyk: for compability with older versions, keeping a 0 value here
	strcpy(content, va("%sON-", content));

	return G_NewString(content);
}

// GalaxyRP (Alex): [Skill Display] This method returns a color string based on the ability alignment. Used in displaying the skill to the user.
char *color_ability(skill_t skill) {
	if (strcmp(skill.alignment, "light") == 0) {
		return "^4";
	}
	else if (strcmp(skill.alignment, "dark") == 0) {
		return "^1";
	}
	else if (strcmp(skill.alignment, "neutral") == 0) {
		return "^5";
	}
	else if (strcmp(skill.alignment, "merc") == 0) {
		return "^3";
	}

	// GalaxyRP fix: skills table has entries with alignment "none" (e.g. weapon/item/ammo skills),
	// which none of the branches above handle. Falling off the end of a non-void function is
	// undefined behaviour -- the caller (zyk_list_player_skills, via /skills or similar) would get
	// back a garbage pointer and hand it to va()/strcpy() as a C string, which can crash the
	// server. Return a plain color code for anything that isn't light/dark/neutral/merc instead.
	return "^7";
}

char *add_spacing_for_columns(skill_t skill, char* message, int skill_id) {

	int array_stop = 20 - strlen(skill.skill_name);

	if (skill_id >= 10) {
		array_stop--;
	}

	for (int i = 0; i < array_stop; i++)
	{
		strcpy(message, va("%s ", message));
	}

	strcpy(message, va("%s", message));

	return message;
}

void zyk_list_player_skills(gentity_t *ent, gentity_t *target_ent, char *arg1)
{
	char message[1024];
	int i = 0;
	int display_counter = 0;

	strcpy(message,"");

	for (int i = 0; i < ARRAY_LEN(skills); i++)
	{
		if (strcmp(skills[i].category, arg1) == 0) {
			strcpy(message, va("%s%s%d - %s: %d/%d", message, color_ability(skills[i]), i + 1, skills[i].skill_name, ent->client->pers.skill_levels[i], skills[i].max_level));
			
			if (display_counter % 2 != 0) {
				strcpy(message, va("%s\n", message));
			}
			else {
				strcpy(message, va("%s", add_spacing_for_columns(skills[i], message, i + 1)));
			}
			display_counter++;
		}
	}

	trap->SendServerCommand(target_ent->s.number, va("print \"%s\n\n\"", message));
}

void list_rpg_info(gentity_t *ent, gentity_t *target_ent)
{ // zyk: lists general RPG info of this player
	trap->SendServerCommand(target_ent->s.number, va("print \"\n^2Account: ^7%s\n^2Character: ^7%s\n\n^3Level: ^7%d/%d\n^3XP: ^7%d/%d\n^3Skill Points: ^7%d\n\n^7Use ^2/list help ^7to see console commands\n\n\"", ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->pers.level, zyk_rpg_max_level.integer, ent->client->pers.xp, check_xp(ent->client->pers.level), ent->client->pers.skillpoints));
}

/*
==================
Cmd_ListAccount_f
==================
*/
void Cmd_ListAccount_f( gentity_t *ent ) {
	if (ent->client->sess.amrpgmode == 2)
	{
		if (trap->Argc() == 1)
		{ // zyk: if player didnt pass arguments, lists general info
			list_rpg_info(ent, ent);
		}
		else
		{
			char message[1024];
			char arg1[MAX_STRING_CHARS];
			int i = 0;

			strcpy(message,"");

			trap->Argv(1, arg1, sizeof( arg1 ));

			if (Q_stricmp( arg1, "help" ) == 0)
			{
				trap->SendServerCommand(ent-g_entities, "print \"\n^2/list force: ^7lists force power skills\n^2/list weapons: ^7lists weapon skills\n^2/list other: ^7lists miscellaneous skills\n^2/list ammo: ^7lists ammo skills\n^2/list items: ^7lists holdable items skills\n^2/list [skill number]: ^7lists info about a skill\n^2/list commands: ^7lists the Galaxy Mod console commands\n\n\"");
			}
			else if (Q_stricmp( arg1, "force" ) == 0 || Q_stricmp( arg1, "weapons" ) == 0 || Q_stricmp( arg1, "other" ) == 0 || 
					 Q_stricmp( arg1, "ammo" ) == 0 || Q_stricmp( arg1, "items" ) == 0)
			{
				zyk_list_player_skills(ent, ent, G_NewString(arg1));
			}
			else if (Q_stricmp( arg1, "commands" ) == 0)
			{
				// GalaxyRP fix: [cleanup] dropped a stray backslash before the ^3 colour code below --
				// "\^" isn't a real C escape sequence (MSVC C4129), and every compiler already just
				// discards the backslash and keeps the literal ^3, so this is a no-op for the text
				// actually produced -- just spelled correctly now.
			trap->SendServerCommand(ent - g_entities, "print \"\n^2Commands\n\n^3--------Account--------\n\
^3/new <login> <password>: ^7Creates a new account.\n\
^3/login <login> <password>: ^7Loads the account.\n\
^3/logout: ^7Logs out the account.\n\
^3/changepassword <new password>: ^7Changes the account password.\n\
^3/settings <number (optional)>: ^7Turns on or off account settings. Run with no arguments to list your settings.\n\n\
^3--------Character--------\n\
^3/attributes <description>: ^7Sets your character's description. Can be viewed by others with ^3/ex ^7command.\n\
^3/examine ^7or ^3/ex <player name>: ^7Displays someone's character description.\n\
^3/char <new/use/remove (optional)> <character name (optional)>: ^7Creates/switches to/deletes a character. Run with no arguments to list your characters.\n\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3--------Admin Commands--------\n\
^3/adminlist <command ID (optional)>: ^7Lists admin commands and their availability for current player. If you pass a command ID as argument, shows info about it.\n\
^3/adminlist show <player id or name>: ^7Shows admin commands of another player. ^1(only for Admins with Give Admin)\n\
^3/adminup <player name> <command number>: ^7Gives the player an admin command.\n\
^3/admindown <player name> <command number>: ^7Removes an admin command from the player.\n\
^3/entitysystem: ^7Shows commands to manipulate entities and remap shaders. ^1(only for Admins with Entity System)\n\
^3/playmusic <file path>: ^7Replaces the current map music for all players with the song given.\n\
^3/levelup <player name> <number of levels (optional)>: ^7Levels the player up by one.\n\
^3/leveldown <player name> <number of levels (optional)>: ^7Brings the player's level down by one.\n\"");
				trap->SendServerCommand(ent - g_entities, "print \"^3/givexp <player name>: ^7Gives the player one xp.\n\
^3/removexp <player name>: ^7Removes one xp from the player.\n\
^3/skillup <player name> <skill number> <number of levels (optional)>: ^7upgrades a skill.\n\
^3/skilldown <player name> <skill number> <number of levels (optional)>: ^7downgrades a skill.\n\
^3/god: ^7Makes you invincible.\n\
^3/players <player name(optional)> <force/weapons/other/ammo/items (optional)>: ^7Checks the player's abilities and stats. Use without argument to see info about all players.\n\
^3/telemark: ^7Sets a marker you can teleport to later.\n\
^3/teleport ^7or /^3tele <player name (optional)> <player name (optional)>: ^7Teleports first player to the second player. Using one argument teleports current player to another player. Use with no arguments to teleport to your telemark.\n\"");
				trap->SendServerCommand(ent - g_entities, "print \"^3/silence <player name>: ^7Silences the player.\n\
^3/paralyze <player name>: ^7Paralyzes the player.\n\
^3/admkick <player name>: ^7Kicks player from the server.\n\
^3/killother <player name>: ^7Kills a player.\n\
^3/give <player name> <guns/force>: ^7Gives guns or Force powers to a player who is not logged in.\n\
^3/clientprint <player name> <text>: ^7Prints text on the player's screen. Use ^3-1 ^7argument to print for all players.\n\
^3/shakescreen <distance from player> <intensity> <length>: ^7Shakes players' screen who are a certain distance from you.\n\
^3/duelarena: ^7Sets or unsets the Duel Tournament arena in current map.\n\
^3/duelpause: ^7Pauses/resumes the Duel Tournament.\n\
^3/admmap <gametype number> <map name>: ^7Changes the server to a different map and gametype.\n\
^3/noclip: ^7Makes you able to go through walls.\n\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3--------RP Inventory System--------\n\
^3/inventory ^7or ^3/inv: ^7Displays player's RP inventory.\n\
^3/createitem <itemname>: ^7Creates an item with a given name. Items containing more than one word need double quotes around the argument. ^1(Admin only)\n\
^3/trashitem <itemid>: ^7Deletes an item.\n\
^3/giveitem <itemid> <playername>: ^7Transfers an item to the desired player.\n\n\
^3--------News System--------\n\
^3/news <channel> <number of entries (optional)>: ^7Displays the news of the chosen channel.\n\
^3/newsadd <channel> <text>: ^7Adds the news to a channel. The text has to be enclosed in double quotes for it to register properly. ^1(Admin only)\n\
^3/newsremove <news ID>: ^7Removes the news from a channel. ^1(Admin only)\n\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3--------Credits and Trading--------\n\
^3/createcredits <player name> <amount>: ^7Creates credits and gives them to a player. ^1(Admin only)\n\
^3/spendcredits <amount>: ^7Deletes credits from your inventory and displays a message (For paying NPCs).\n\
^3/givecredits <player name> <amount>: ^7Transfers credits from you to a player.\n\
^3/buy <merchandise ID>: ^7Exchanges credits for merchandise. Stuff bought from upgrades category is permanent.\n\
^3/stuff <ammo/misc/upgrades or merchandise ID>: ^7Shows info about merchandise. Use category name to list available options or ID number to see stuff description.\n\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3--------Ally System--------\n\
^3/allyadd <player name>: ^7Adds a player as an ally.\n\
^3/allychat <text>: ^7Sends message to your allies.\n\
^3/allyremove <player name>: ^7Removes player from allies.\n\
^3/allylist: ^7Lists your allies.\n\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3--------Misc--------\n\
^3/flipcoin: ^7Flips a coin and displays the result in chat.\n\
^3/roll <max value>: ^7Rolls a dice and displays the result in chat.\n\
^3/anim ^7or ^3/emote <id/name/list>: ^7Plays an animation by id or name. ^3List ^7and ^3list 2 ^7are for listing all the available animations.\n\
^3/playsound <channel> <file path>: ^7Plays chosen sound on the map on selected channel.\n\
^3/order <action>: ^7Orders NPC to perform an action.\n\
^3/datetime: ^7Shows current server date and time.\n\
^3/drop: ^7Drops the current weapon of the player. If current weapon is melee, drops the selected Holdable Item from inventory.\n\
^3/ignore <player name>: ^7Ignores chat of a player.\n\
^3/ignorelist: ^7Lists ignored players.\n\
^3/jetpack: ^7Gives or removes jetpack from the player.\n\
^3/updateforce: ^7Applies your force power menu pick instantly, no respawn needed (logged-out players only).\n\"");
				trap->SendServerCommand(ent - g_entities, "print \"^3/maplist: ^7Lists the maps available in the server.\n\
^3/saber <saber1> <saber2>: ^7Changes lightsabers of the player.\n\
^3/sabercolor <1|2> <r g b>/<color name>: ^7Sets the RGB or a preset color of saber 1 or 2. Run with no arguments to see current colors.\n\
^3/saberblade <1|2> <type>: ^7Sets the RGB blade style (classic/flame1/electric1/flame2/electric2) of saber 1 or 2.\n\
^3/updatesaber: ^7Applies your saber menu pick instantly, no respawn needed.\n\
^3/scale <player name (optional)/help> <size>: ^7Scales character model (default is 100). ^3Help ^7lists in-game values compared to real life measurements. ^1(Only admins can scale other players)\n\
^3/getup: ^7Revives current player from downed state.\n\
^3/helpup <player name>: ^7Revives another player from downed state.\n\
^3/training <on|off>: ^7Turns training saber mode (near-zero damage) on or off.\n\
^3/voice_cmd <arg> <f or m>: ^7Activates the voice chat system.\n\
^3/where: ^7Displays your current coordinates.\n\n\"");
			}
			else
			{ // zyk: the player can also list the specific info of a skill passing the skill number as argument
				i = atoi(arg1);
				if (i >= 1 && i <= NUM_OF_SKILLS)
				{
					trap->SendServerCommand(ent - g_entities, va("print \"^3%s: ^7%s\n\"", skills[i-1].skill_name, skills[i-1].skill_description));
				}
				else
				{
					trap->SendServerCommand( ent-g_entities, "print \"Invalid skill number.\n\"" );
				}
			}
		}
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, "print \"\n^1Account System\n^7Create a new account with ^3/new <login> <password>\n^7where login and password are of your choice.\n\n\"" );
	}
}

/*
==================
Cmd_CallSeller_f
==================
*/
extern gentity_t *NPC_SpawnType( gentity_t *ent, char *npc_type, char *targetname, qboolean isVehicle );
void Cmd_CallSeller_f( gentity_t *ent ) {
	gentity_t *npc_ent = NULL;
	int i = 0;
	int seller_id = -1;

	for (i = MAX_CLIENTS; i < level.num_entities; i++)
	{
		npc_ent = &g_entities[i];

		if (npc_ent && npc_ent->client && npc_ent->NPC && Q_stricmp(npc_ent->NPC_type, "jawa_seller") == 0 && 
			npc_ent->health > 0 && npc_ent->client->pers.seller_invoked_by_id == ent->s.number)
		{ // zyk: found the seller of this player
			seller_id = npc_ent->s.number;
			break;
		}
	}

	if (seller_id == -1)
	{
		npc_ent = NPC_SpawnType(ent,"jawa_seller",NULL,qfalse);
		if (npc_ent)
		{
			npc_ent->client->pers.seller_invoked_by_id = ent->s.number;
			npc_ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

			trap->SendServerCommand( ent->s.number, "chat \"^3Jawa Seller: ^7Hello, friend! I have some products to sell.\"");
		}
		else
		{
			trap->SendServerCommand( ent->s.number, va("chat \"%s: ^7The seller couldn't come...\"", ent->client->pers.netname));
		}
	}
	else
	{ // zyk: seller of this player is already in the map, remove him
		npc_ent = &g_entities[seller_id];

		G_FreeEntity(npc_ent);

		trap->SendServerCommand( ent->s.number, "chat \"^3Jawa Seller: ^7See you later, friend!\"");
	}
}

/*
==================
Cmd_Stuff_f
==================
*/
void Cmd_Stuff_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{ // zyk: shows the categories of stuff
		// GalaxyRP fix: [Shop] this used to also mention "/sell <number> to sell" -- there has never
		// been a /sell command (only "buy", see the command table below), so that line described
		// something a player could never actually do. Trimmed to just the real, working /buy usage.
		trap->SendServerCommand( ent-g_entities, "print \"\n^7Use ^2/stuff <category> ^7to buy stuff\nThe Category may be ^3ammo^7, ^3misc ^7or ^3upgrades\n^7Use ^3/stuff <number> ^7to see info about the item\n\n^7Use ^2/buy <number> ^7to buy\nStuff bought from ^3upgrades ^7category are permanent\n\n\"");
		return;
	}
	else
	{
		char arg1[1024];
		int i = 0;

		trap->Argv(1, arg1, sizeof( arg1 ));
		i = atoi(arg1);

		if (Q_stricmp(arg1, "ammo" ) == 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n"
				"^31 - Blaster Pack: ^7Buy: 150\n"
				"^32 - Power Cell: ^7Buy: 200\n"
				"^33 - Metal Bolts: ^7Buy: 250\n"
				"^34 - Rockets: ^7Buy: 500\n"
				"^35 - Thermals: ^7Buy: 50\n"
				"^36 - Trip Mines: ^7Buy: 100\n"
				"^37 - Det Packs: ^7Buy: 200\n"
				"^330 - Flame Thrower Fuel: ^7Buy: 500\n"
				"^348 - Ammo All: ^7Buy: 1450\n\n\"");
		}
		else if (Q_stricmp(arg1, "misc") == 0)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\n"
				"^314 - Ysalamiri: ^7Buy: 2000\n"
				"^331 - Jetpack Fuel: ^7Buy: 500\n"
				"^343 - Force Boon: ^7Buy: 2000\n\n\"");
		}
		else if (Q_stricmp(arg1, "upgrades" ) == 0)
		{
			// GalaxyRP fix: [Upgrades] removed 25/26/27 (Power Cell/Blaster Pack/Metal Bolts Weapons
			// Upgrades) and 28 (Rocket Upgrade) listing lines here — all inert/non-functional
			trap->SendServerCommand( ent-g_entities, "print \"\n"
				"^315 - Impact Reducer: ^7Buy: 40000\n"
				"^333 - Stun Baton Upgrade: ^7Buy: 15000\n"
				"^340 - Holdable Items Upgrade: ^7Buy: 30000\n\"");
		}
		else if (i == 1)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Blaster Pack: ^7recovers 100 ammo of E11 Blaster Rifle, Blaster Pistol and Bryar Pistol weapons\n\n\"");
		}
		else if (i == 2)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Power Cell: ^7recovers 100 ammo of Disruptor, Bowcaster and DEMP2 weapons\n\n\"");
		}
		else if (i == 3)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Metal Bolts: ^7recovers 100 ammo of Repeater, Flechette and Concussion Rifle weapons\n\n\"");
		}
		else if (i == 4)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Rockets: ^7recovers 10 ammo of Rocket Launcher weapon\n\n\"");
		}
		else if (i == 5)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Thermals: ^7recovers 4 ammo of thermals\n\n\"");
		}
		else if (i == 6)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Trip Mines: ^7recovers 3 ammo of trip mines\n\n\"");
		}
		else if (i == 7)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Det Packs: ^7recovers 2 ammo of det packs\n\n\"");
		}
		// GalaxyRP fix: [Upgrades] removed i==8 (Stealth Attacker Upgrade) info text here — inert/non-functional, removed along with all its supporting code
		else if (i == 9)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Shield Booster: ^7recovers 50 shield\n\n\"");
		}
		else if (i == 14)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Ysalamiri: ^7disables the player force powers but also protects the player from enemy force powers\n\n\"");
		}
		else if (i == 15)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Impact Reducer: ^7reduces the knockback of some weapons attacks by 80 per cent\n\n\"");
		}
		else if (i == 17)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3E11 Blaster Rifle: ^7Rifle that is used by the stormtroopers. Uses blaster pack ammo\n\n\"");
		}
		else if (i == 18)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Disruptor: ^7Sniper rifle which can desintegrate the enemy. Uses power cell ammo\n\n\"");
		}
		else if (i == 19)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Repeater: ^7imperial weapon that shoots orbs and a plasma bomb with alt fire. Uses metal bolts ammo\n\n\"");
		}
		else if (i == 20)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Rocket Launcher: ^7weapon that shoots rockets and a homing missile with alternate fire. Uses rockets ammo\n\n\"");
		}
		else if (i == 21)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Bowcaster: ^7weapon that shoots green bolts, normal fire can be charged, and alt fire shoots a bouncing bolt. Uses power cell ammo\n\n\"");
		}
		else if (i == 22)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Blaster Pistol: ^7pistol that can shoot a charged shot with alt fire. Uses blaster pack ammo\n\n\"");
		}
		else if (i == 23)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Flechette: ^7it is the shotgun of the game, and can shoot 2 bombs with alt fire. Uses metal bolts ammo\n\n\"");
		}
		else if (i == 24)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Concussion Rifle: ^7powerful weapon, alt fire can shoot a beam that gets through force fields. Uses metal bolts ammo\n\n\"");
		}
		// GalaxyRP fix: [Upgrades] removed i==25/26/27 (Power Cell/Blaster Pack/Metal Bolts Weapons
		// Upgrades), i==28 (Rocket Upgrade) and i==29 (Bounty Hunter Upgrade) info text here — all
		// inert/non-functional, removed along with all their supporting code
		else if (i == 30)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Flame Thrower Fuel: ^7recovers all fuel of the flame thrower\n\n\"");
		}
		else if (i == 31)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Jetpack Fuel: ^7recovers all fuel of the jetpack\n\n\"");
		}
		else if (i == 32)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Stun Baton: ^7weapon that fires a small electric charge\n\n\"");
		}
		else if (i == 33)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Stun Baton Upgrade: ^7allows stun baton to open any door, including locked ones, move elevators, and move or destroy other objects. Also makes stun baton decloak enemies and decrease their running speed for some seconds\n\n\"");
		}
		else if (i == 34)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Bacta Canister: ^7recovers 25 HP\n\n\"");
		}
		else if (i == 35)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3E-Web: ^7portable emplaced gun\n\n\"");
		}
		else if (i == 36)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3DEMP2: ^7fires an electro magnetic pulse that causes bonus damage against droids. Uses power cell ammo\n\n\"");
		}
		else if (i == 37)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Bryar Pistol: ^7similar to blaster pistol, but has a faster fire rate with normal fire. Uses blaster pack ammo\n\n\"");
		}
		// GalaxyRP fix: [Upgrades] removed i==39 (Armored Soldier Upgrade) info text here — inert/non-functional, removed along with all its supporting code
		else if (i == 40)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Holdable Items Upgrade: ^7Bacta Canister recovers more health, Big Bacta recovers more HP, Force Field resists more and Cloak Item will be able to cloak vehicles\n\n\"");
		}
		else if (i == 42)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Cloak Item: ^7allows the player to cloak himself\n\n\"");
		}
		else if (i == 43)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Force Boon: ^7allows the player to regenerate force faster\n\n\"");
		}
		// GalaxyRP fix: [Upgrades] removed i==45 (Force Gunner Upgrade) and i==47 (Force Guardian
		// Upgrade) info text here — both inert/non-functional, removed along with all their supporting code
		else if (i == 48)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Ammo All: ^7recovers all ammo types, including flame thrower fuel\n\n\"");
		}
		else if (i == 51)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Healing Crystal: ^7regens hp, mp and force. If the player dies, he loses the crystal\n\n\"");
		}
		else if (i == 52)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Energy Crystal: ^7regens shield, blaster pack ammo and power cell ammo. If the player dies, he loses the crystal\n\n\"");
		}
		else if (i == 53)
		{
			// GalaxyRP fix: [Classes] this chain used to dispatch on rpg_class==0..9 (one branch
			// per class, each with its own help text). rpg_class is permanently 0 now that character
			// classes are gone, so the rpg_class==1..9 branches were unreachable and have been
			// removed; the rpg_class==0 condition is likewise always true and has been dropped.
			trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 1: ^7used with /unique command. You can only have one Unique Ability at a time. Free Warrior gets Mimic Damage. If you take damage, does part of the damage back to the enemy. Spends 50 force and 25 mp\n\n\"");
		}
		else if (i == 54)
		{
			// GalaxyRP fix: [Classes] this chain used to dispatch on rpg_class==0..9 (one branch
			// per class, each with its own help text). rpg_class is permanently 0 now that character
			// classes are gone, so the rpg_class==1..9 branches were unreachable and have been
			// removed; the rpg_class==0 condition is likewise always true and has been dropped.
			trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 2: ^7used with /unique command. You can only have one Unique Ability at a time. Free Warrior gets Super Beam, a powerful beam with high damage. Spends 100 force and 25 mp\n\n\"");
		}
		else if (i == 55)
		{
			// GalaxyRP fix: [Classes] this chain used to dispatch on rpg_class==0..9 (one branch
			// per class, each with its own help text). rpg_class is permanently 0 now that character
			// classes are gone, so the rpg_class==1..9 branches were unreachable and have been
			// removed; the rpg_class==0 condition is likewise always true and has been dropped.
			trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 3: ^7used with /unique command. You can only have one Unique Ability at a time. Free Warrior gets Flee to Safety, which sets an area in the map to where the player will be transported to after using /unique again. Spends 50 force and 20 mp\n\n\"");
		}
		else if (i == 56)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\n^3Book of Riddles: ^7a legendary book that shows the answers to the riddles created by the Guardian of Eternity\n\n\"");
		}
	}
}

/*
==================
Cmd_Buy_f
==================
*/
void Cmd_Buy_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int value = 0;
	int found = 0;
	int item_costs[NUMBER_OF_SELLER_ITEMS] = {
		150,		// id:1
		200,		// id:2
		250,		// id:3
		500,		// id:4
		50,			// id:5
		100,		// id:6
		200,		// id:7
		5000,		// id:8
		0,		// id:9
		0,			// id:10
		0,			// id:11
		0,			// id:12
		0,			// id:13
		2000,		// id:14
		40000,		// id:15
		30000,		// id:16
		1,		// id:17
		1,		// id:18
		1,		// id:19
		1,		// id:20
		1,		// id:21
		1,		// id:22
		1,		// id:23
		1,		// id:24
		2000,		// id:25
		1800,		// id:26
		22000,		// id:27
		25000,		// id:28
		5000,		// id:29
		500,		// id:30
		500,		// id:31
		1,		// id:32
		15000,		// id:33
		0,		// id:34
		0,		// id:35
		1,		// id:36
		1,		// id:37
		100,		// id:38
		5000,		// id:39
		30000,		// id:40
		2000,		// id:41
		0,		// id:42
		2000,		// id:43
		50,			// id:44
		5000,		// id:45
		100000,		// id:46
		5000,		// id:47
		1450,		// id:48
		20000,		// id:49
		20000,		// id:50
		2000,		// id:51
		2000,		// id:52
		7000,		// id:53
		7000,		// id:54
		7000,		// id:55
		100000 };	// id:56

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You must specify a product number.\n\"" );
		return;
	}

	trap->Argv(1, arg1, sizeof( arg1 ));
	value = atoi(arg1);

	// zyk: tests the cooldown time to buy or sell
	if (ent->client->pers.buy_sell_timer > level.time)
	{
		trap->SendServerCommand(ent->s.number, "print \"In Buy/Sell cooldown time.\n\"");
		return;
	}

	if (value < 1 || value > NUMBER_OF_SELLER_ITEMS)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid product number.\n\"" );
		return;
	}
	/*else
	{ // zyk: searches for the jawa to see if we are near him to buy or sell to him
		gentity_t *jawa_ent = NULL;
		int j = 0;

		for (j = MAX_CLIENTS; j < level.num_entities; j++)
		{
			jawa_ent = &g_entities[j];

			if (jawa_ent && jawa_ent->client && jawa_ent->NPC && jawa_ent->health > 0 && Q_stricmp( jawa_ent->NPC_type, "jawa_seller" ) == 0 && (int)Distance(ent->client->ps.origin, jawa_ent->client->ps.origin) < 90)
			{
				found = 1;
				break;
			}
		}

		// GalaxyRP fix: [Classes] this condition used to also check
		// !(rpg_class==2 && secrets_found&(1<<1)) (Bounty Hunter Upgrade bypass). rpg_class is
		// permanently 0 now that character classes are gone, so rpg_class==2 is always false, making
		// that whole negated conjunct always true; simplified to just the found==0 check.
		if (found == 0)
		{ // zyk: Bounty Hunter Upgrade allows buying and selling without the need to call the jawa seller
			trap->SendServerCommand(ent->s.number, "print \"You must be near the jawa seller to buy from him.\n\"" );
			return;
		}
	}*/

	// zyk: general validations. Some items require certain conditions to be bought
	// GalaxyRP fix: [Upgrades] removed the "already have" duplicate-purchase checks for value==8
	// (Stealth Attacker), 25/26/27 (Power Cell/Blaster Pack/Metal Bolts Weapons), 28 (Rocket),
	// 29 (Bounty Hunter), 39 (Armored Soldier), 45 (Force Gunner) and 47 (Force Guardian) here —
	// these upgrades and all their supporting code were removed as inert/non-functional.
	if (value == 15 && ent->client->pers.secrets_found & (1 << 9))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Impact Reducer.\n\"" );
		return;
	}
	else if (value == 33 && ent->client->pers.secrets_found & (1 << 15))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Stun Baton Upgrade.\n\"" );
		return;
	}
	else if (value == 40 && ent->client->pers.secrets_found & (1 << 0))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Holdable Items Upgrade.\n\"" );
		return;
	}
	else if (value == 53 && ent->client->pers.secrets_found & (1 << 2))
	{
		trap->SendServerCommand(ent - g_entities, "print \"You already have the Unique Ability 1.\n\"");
		return;
	}
	else if (value == 54 && ent->client->pers.secrets_found & (1 << 3))
	{
		trap->SendServerCommand(ent - g_entities, "print \"You already have the Unique Ability 2.\n\"");
		return;
	}
	else if (value == 55 && ent->client->pers.secrets_found & (1 << 4))
	{
		trap->SendServerCommand(ent - g_entities, "print \"You already have the Unique Ability 3.\n\"");
		return;
	}

	// GalaxyRP fix: [Upgrades] reject these product ids outright before the credit-deduction dispatch
	// below -- 8/25/26/27/28/29/39/45/47 (Stealth Attacker, Power Cell/Blaster Pack/Metal Bolts
	// Weapons, Rocket, Bounty Hunter, Armored Soldier, Force Gunner, Force Guardian Upgrades) had
	// their purchase branches removed as inert/non-functional, but item_costs[] was intentionally
	// left untouched (see the removal comments below) to avoid renumbering every other product id.
	// Without this check the credit-deduction/"Thanks!" code at the end of this function would still
	// fire for these ids and silently charge full price for a purchase that does nothing.
	if (value == 8 || value == 25 || value == 26 || value == 27 || value == 28 || value == 29 ||
		value == 39 || value == 45 || value == 47)
	{
		trap->SendServerCommand( ent-g_entities, "print \"This item is no longer available.\n\"" );
		return;
	}

	// zyk: buying the item if player has enough credits
	if (ent->client->pers.credits >= item_costs[value-1])
	{
		if (value == 1)
		{
			Add_Ammo(ent,AMMO_BLASTER,100);
		}
		else if (value == 2)
		{
			Add_Ammo(ent,AMMO_POWERCELL,100);
		}
		else if (value == 3)
		{
			Add_Ammo(ent,AMMO_METAL_BOLTS,100);
		}
		else if (value == 4)
		{
			Add_Ammo(ent,AMMO_ROCKETS,5);
		}
		else if (value == 5)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_THERMAL);
			Add_Ammo(ent,AMMO_THERMAL,1);
		}
		else if (value == 6)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_TRIP_MINE);
			Add_Ammo(ent,AMMO_TRIPMINE,1);
		}
		else if (value == 7)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DET_PACK);
			Add_Ammo(ent,AMMO_DETPACK,1);
		}
		// GalaxyRP fix: [Upgrades] removed value==8 (Stealth Attacker Upgrade) purchase branch here — inert/non-functional, removed along with all its supporting code
		else if (value == 14)
		{
			if (ent->client->ps.powerups[PW_YSALAMIRI] < level.time)
				ent->client->ps.powerups[PW_YSALAMIRI] = level.time + 60000;
			else
				ent->client->ps.powerups[PW_YSALAMIRI] += 60000;
		}
		else if (value == 15)
		{
			ent->client->pers.secrets_found |= (1 << 9);
		}
		/*else if (value == 17)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BLASTER);
		}
		else if (value == 18)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DISRUPTOR);
		}
		else if (value == 19)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_REPEATER);
		}
		else if (value == 20)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_ROCKET_LAUNCHER);
		}
		else if (value == 21)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BOWCASTER);
		}
		else if (value == 22)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);
		}
		else if (value == 23)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_FLECHETTE);
		}
		else if (value == 24)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_CONCUSSION);
		}*/
		// GalaxyRP fix: [Upgrades] removed value==25/26/27 (Power Cell/Blaster Pack/Metal Bolts Weapons
		// Upgrades), value==28 (Rocket Upgrade) and value==29 (Bounty Hunter Upgrade) purchase branches
		// here — all inert/non-functional, removed along with all their supporting code
		else if (value == 30)
		{
			ent->client->ps.cloakFuel = 100;
		}
		else if (value == 31)
		{
			ent->client->pers.jetpack_fuel = MAX_JETPACK_FUEL;
			ent->client->ps.jetpackFuel = 100;
		}
		/*else if (value == 32)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_STUN_BATON);
		}*/
		else if (value == 33)
		{
			ent->client->pers.secrets_found |= (1 << 15);
		}
		else if (value == 36)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DEMP2);
		}
		/*else if (value == 37)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_OLD);
		}*/
		// GalaxyRP fix: [Upgrades] removed value==39 (Armored Soldier Upgrade) purchase branch here — inert/non-functional, removed along with all its supporting code
		else if (value == 40)
		{
			ent->client->pers.secrets_found |= (1 << 0);
		}
		else if (value == 43)
		{
			if (ent->client->ps.powerups[PW_FORCE_BOON] < level.time)
				ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 60000;
			else
				ent->client->ps.powerups[PW_FORCE_BOON] += 60000;
		}
		else if (value == 44)
		{
			ent->client->pers.magic_power = zyk_max_magic_power(ent);

			send_rpg_events(2000);
		}
		// GalaxyRP fix: [Upgrades] removed value==45 (Force Gunner Upgrade) and value==47 (Force
		// Guardian Upgrade) purchase branches here — both inert/non-functional, removed along with
		// all their supporting code
		else if (value == 48)
		{
			Add_Ammo(ent,AMMO_BLASTER,100);

			Add_Ammo(ent,AMMO_POWERCELL,100);

			Add_Ammo(ent,AMMO_METAL_BOLTS,100);

			Add_Ammo(ent,AMMO_ROCKETS,10);

			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_THERMAL);
			Add_Ammo(ent,AMMO_THERMAL,4);

			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_TRIP_MINE);
			Add_Ammo(ent,AMMO_TRIPMINE,3);

			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DET_PACK);
			Add_Ammo(ent,AMMO_DETPACK,2);

			ent->client->ps.cloakFuel = 100;
		}
		else if (value == 51)
		{
			ent->client->pers.player_statuses |= (1 << 10);
		}
		else if (value == 52)
		{
			ent->client->pers.player_statuses |= (1 << 11);
		}
		else if (value == 53)
		{
			ent->client->pers.secrets_found |= (1 << 2);
			ent->client->pers.secrets_found &= ~(1 << 3);
			ent->client->pers.secrets_found &= ~(1 << 4);
		}
		else if (value == 54)
		{
			ent->client->pers.secrets_found &= ~(1 << 2);
			ent->client->pers.secrets_found |= (1 << 3);
			ent->client->pers.secrets_found &= ~(1 << 4);
		}
		else if (value == 55)
		{
			ent->client->pers.secrets_found &= ~(1 << 2);
			ent->client->pers.secrets_found &= ~(1 << 3);
			ent->client->pers.secrets_found |= (1 << 4);
		}
		else if (value == 56)
		{
			trap->SendServerCommand(ent - g_entities, "chat \"^3Book of Riddles: ^7key clock sword sun fire water time star nature\n\"");
		}

		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));

		ent->client->pers.credits -= item_costs[value-1];
		save_account(ent, qtrue);

		ent->client->pers.buy_sell_timer = level.time + zyk_buying_selling_cooldown.integer;

		trap->SendServerCommand( ent-g_entities, va("chat \"^3Jawa Seller: ^7Thanks %s^7!\n\"",ent->client->pers.netname) );

	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("chat \"^3Jawa Seller: ^7%s^7, my products are not free! Give me the money!\n\"",ent->client->pers.netname) );
		return;
	}
}

// zyk: if an item left the inventory, makes some adjustments on the player
extern void Jedi_Decloak(gentity_t *self);
void zyk_adjust_holdable_items(gentity_t *ent)
{
	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC)) &&
		!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SENTRY_GUN)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SEEKER)) &&
		!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_EWEB)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC_BIG)) &&
		!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SHIELD)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)))
	{ // zyk: if player has no items left, deselect the held item
		ent->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
	}

	// zyk: if player no longer has Cloak Item and is cloaked, decloaks
	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)) && ent->client->ps.powerups[PW_CLOAKED])
		Jedi_Decloak(ent);
}

/*
==================
Cmd_ChangePassword_f
==================
*/
void Cmd_ChangePassword_f( gentity_t *ent ) {
	char arg1[1024];

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand( ent-g_entities, "print \"^1Command Usage: ^3/changepassword ^2<new_password>\n^1Example: ^3/changepassword ^2password123\n\"" );
		return;
	}

	// zyk: gets the new password
	trap->Argv(1, arg1, sizeof( arg1 ));

	if (strlen(arg1) > 30)
	{
		trap->SendServerCommand( ent-g_entities, "print \"The password can only have a maximum of 30 characters.\n\"" );
		return;
	}

	strcpy(ent->client->pers.password,arg1);

	update_accounts_table_row_with_current_values(ent);

	trap->SendServerCommand( ent-g_entities, "print \"^3Your password was changed successfully.\n\"" );
}

void zyk_remove_configs(gentity_t *ent)
{
#if defined(__linux__)
	system(va("rm -f GalaxyRP/configs/%s_%s_freewarrior.txt GalaxyRP/configs/%s_%s_forceuser.txt GalaxyRP/configs/%s_%s_bountyhunter.txt GalaxyRP/configs/%s_%s_armoredsoldier.txt GalaxyRP/configs/%s_%s_monk.txt GalaxyRP/configs/%s_%s_stealthattacker.txt GalaxyRP/configs/%s_%s_duelist.txt GalaxyRP/configs/%s_%s_forcegunner.txt GalaxyRP/configs/%s_%s_magicmaster.txt GalaxyRP/configs/%s_%s_forcetank.txt", ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar));
#else
	system(va("DEL /F \"zykmod\\configs\\%s_%s_freewarrior.txt\" \"zykmod\\configs\\%s_%s_forceuser.txt\" \"zykmod\\configs\\%s_%s_bountyhunter.txt\" \"zykmod\\configs\\%s_%s_armoredsoldier.txt\" \"zykmod\\configs\\%s_%s_monk.txt\" \"zykmod\\configs\\%s_%s_stealthattacker.txt\" \"zykmod\\configs\\%s_%s_duelist.txt\" \"zykmod\\configs\\%s_%s_forcegunner.txt\" \"zykmod\\configs\\%s_%s_magicmaster.txt\" \"zykmod\\configs\\%s_%s_forcetank.txt\"", ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar));
#endif
}

extern void zyk_TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles );

/*
==================
Cmd_Teleport_f
==================
*/
void Cmd_Teleport_f( gentity_t *ent )
{
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	char arg3[MAX_STRING_CHARS];
	char arg4[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_TELE, qtrue))
	{
		return;
	}

	if (g_gametype.integer != GT_FFA && zyk_allow_adm_in_other_gametypes.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Teleport command not allowed in gametypes other than FFA.\n\"" );
		return;
	}

	// GalaxyRP fix: [Guardian] a guardian_mode>0 guard blocking /teleport while in a guardian battle
	// used to be here. guardian_mode is permanently 0 now, so it was unreachable.

	if (trap->Argc() == 1)
	{
		// GalaxyRP fix: [validation] pers.saved_origin/saved_view_angles are zeroed every spawn
		// (see ClientSpawn in g_client.c) and only ever written by Cmd_Telemark_f, so a player who
		// never ran /telemark since their last spawn had this branch teleport them straight to
		// world origin (0,0,0) -- almost always inside geometry or the void. Treat an all-zero
		// saved point as "no telemark set", the same guard TaystJK's /amtele uses for its own
		// telemark case.
		if (ent->client->pers.saved_origin[0] == 0 && ent->client->pers.saved_origin[1] == 0 && ent->client->pers.saved_origin[2] == 0 &&
			ent->client->pers.saved_view_angles[0] == 0 && ent->client->pers.saved_view_angles[1] == 0 && ent->client->pers.saved_view_angles[2] == 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"No telemark set. Use /telemark to mark a spot first.\n\"" );
			return;
		}

		zyk_TeleportPlayer(ent, ent->client->pers.saved_origin, ent->client->pers.saved_view_angles);
	}
	else if (trap->Argc() == 2)
	{
		int client_id = -1;

		trap->Argv( 1,  arg1, sizeof( arg1 ) );

		vec3_t target_origin;

		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if (client_id == -1)
		{
			return;
		}

		// GalaxyRP fix: [Admin] added the "ent != target" self-exemption already used by /give and
		// /scale, so an admin with their own Admin Protect enabled can still target themselves.
		if (ent != &g_entities[client_id] && g_entities[client_id].client->sess.amrpgmode > 0 && g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << 13)))
			{
				trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
				return;
			}

		// GalaxyRP fix: [Guardian] a guardian_mode>0 guard blocking teleport-to-player while the target
		// was in a guardian battle used to be here. guardian_mode is permanently 0 now, so it was
		// unreachable.

		VectorCopy(g_entities[client_id].client->ps.origin,target_origin);
		target_origin[2] = target_origin[2] + 100;

		zyk_TeleportPlayer(ent,target_origin,g_entities[client_id].client->ps.viewangles);
	}
	else if (trap->Argc() == 3)
	{
		// zyk: teleporting a player to another
		int client1_id;
		int client2_id;

		vec3_t target_origin;

		trap->Argv( 1,  arg1, sizeof( arg1 ) );
		trap->Argv( 2,  arg2, sizeof( arg2 ) );

		client1_id = ClientNumberFromString( ent, arg1, qfalse );
		client2_id = ClientNumberFromString( ent, arg2, qfalse );

		if (client1_id == -1)
		{
			return;
		}

		// GalaxyRP fix: [Admin] added the "ent != target" self-exemption already used by /give and
		// /scale, so an admin with their own Admin Protect enabled can still target themselves.
		if (ent != &g_entities[client1_id] && g_entities[client1_id].client->sess.amrpgmode > 0 && g_entities[client1_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client1_id].client->pers.player_settings & (1 << 13)))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}

		// GalaxyRP fix: [Guardian] a guardian_mode>0 guard blocking teleport-a-player-to-another while
		// the first target was in a guardian battle used to be here. guardian_mode is permanently 0
		// now, so it was unreachable.

		if (client2_id == -1)
		{
			return;
		}

		// GalaxyRP fix: [Admin] added the "ent != target" self-exemption already used by /give and
		// /scale, so an admin with their own Admin Protect enabled can still target themselves.
		if (ent != &g_entities[client2_id] && g_entities[client2_id].client->sess.amrpgmode > 0 && g_entities[client2_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client2_id].client->pers.player_settings & (1 << 13)))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}

		// GalaxyRP fix: [Guardian] a guardian_mode>0 guard blocking teleport-a-player-to-another while
		// the second target was in a guardian battle used to be here. guardian_mode is permanently 0
		// now, so it was unreachable.

		VectorCopy(g_entities[client2_id].client->ps.origin,target_origin);
		target_origin[2] = target_origin[2] + 100;

		zyk_TeleportPlayer(&g_entities[client1_id],target_origin,g_entities[client2_id].client->ps.viewangles);
	}
	else if (trap->Argc() == 4)
	{
		// zyk: teleporting to coordinates
		vec3_t target_origin;

		trap->Argv( 1,  arg1, sizeof( arg1 ) );
		trap->Argv( 2,  arg2, sizeof( arg2 ) );
		trap->Argv( 3,  arg3, sizeof( arg3 ) );

		VectorSet(target_origin,atoi(arg1),atoi(arg2),atoi(arg3));

		zyk_TeleportPlayer(ent,target_origin,ent->client->ps.viewangles);
	}
	else if (trap->Argc() == 5)
	{
		// zyk: teleporting a player to coordinates
		vec3_t target_origin;
		int client_id;

		trap->Argv( 1,  arg1, sizeof( arg1 ) );

		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if (client_id == -1)
		{
			return;
		}

		// GalaxyRP fix: [Admin] added the "ent != target" self-exemption already used by /give and
		// /scale, so an admin with their own Admin Protect enabled can still target themselves.
		if (ent != &g_entities[client_id] && g_entities[client_id].client->sess.amrpgmode > 0 && g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << 13)))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}

		// GalaxyRP fix: [Guardian] a guardian_mode>0 guard blocking teleport-a-player-to-coordinates
		// while the target was in a guardian battle used to be here. guardian_mode is permanently 0
		// now, so it was unreachable.

		trap->Argv( 2,  arg2, sizeof( arg2 ) );
		trap->Argv( 3,  arg3, sizeof( arg3 ) );
		trap->Argv( 4,  arg4, sizeof( arg4 ) );

		VectorSet(target_origin,atoi(arg2),atoi(arg3),atoi(arg4));

		zyk_TeleportPlayer(&g_entities[client_id],target_origin,g_entities[client_id].client->ps.viewangles);
	}
}

/*
==================
Cmd_Telemark_f
==================
*/
void Cmd_Telemark_f(gentity_t* ent)
{
	// GalaxyRP fix: [Admin] /telemark had no permission gate of its own (only CMD_LOGGEDIN), even
	// though the only thing that ever reads what it saves is /teleport, which does require ADM_TELE.
	// Gate it on the same bit so the two commands depend on one permission consistently.
	if (!check_admin_command(ent, ADM_TELE, qtrue))
	{
		return;
	}

	VectorCopy(ent->client->ps.origin, ent->client->pers.saved_origin);
	VectorCopy(ent->client->ps.viewangles, ent->client->pers.saved_view_angles);

	trap->SendServerCommand(ent - g_entities, va("print \"Marked point %s with angles %s\n\"", vtos(ent->client->pers.saved_origin), vtos(ent->client->pers.saved_view_angles)));

	return;
}

/*
==================
Cmd_CreditSpend_f
==================
*/
void Cmd_CreditSpend_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	long int value = 0;

	if (trap->Argc() > 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1Command Usage: ^3/spendcredits ^2<value>\n^1Example: ^3/spendcredits ^2350\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	if (!strtol(arg1, NULL, 10)) {
		trap->SendServerCommand(ent - g_entities, "print \"Value must be an integer\n\"");
		return;
	}

	value = atoi(arg1);

	if (value < 1)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"Can only use positive values.\n\""));
		return;
	}

	if ((ent->client->pers.credits - value) < 0)
	{
		trap->SendServerCommand(ent - g_entities, va(("print \"^1You can\'t spend ^3%i ^1credits.\n^7You only have ^3%i ^7credits.\n\""), value, ent->client->pers.credits));
		return;
	}

	remove_credits(ent, value);

	update_credits_value(ent);

	trap->SendServerCommand(-1, va("chat \"^3Credit System: ^7%s ^7spent ^2%d ^7credits.\n\"", ent->client->pers.netname, value, g_entities));

	trap->SendServerCommand(ent - g_entities, "print \"Done.\n\"");
}

/*
==================
Cmd_CreditCreate_f
==================
*/
void Cmd_CreditCreate_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int client_id = 0, value = 0;

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"You must specify a player.\n\"");
		return;
	}

	// player must have adminup permissions
	if (!check_admin_command(ent, ADM_CREATECREDITS, qtrue))
	{
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));
	trap->Argv(2, arg2, sizeof(arg2));

	client_id = ClientNumberFromString(ent, arg1, qfalse);
	value = atoi(arg2);

	if (client_id == -1)
	{
		return;
	}

	if (value < 1)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"Can only use positive values.\n\""));
		return;
	}


	add_credits(&g_entities[client_id], value);
	update_credits_value(&g_entities[client_id]);

	//broadcast the transaction to the whole server

	trap->SendServerCommand(-1, va("chat \"^3Credit System ^5(Admin)^3: ^7%s ^7created ^2%d ^7credits and transferred them to %s\n\"", ent->client->pers.netname, value, g_entities[client_id].client->pers.netname));

	trap->SendServerCommand(ent - g_entities, "print \"Done.\n\"");
}

/*
==================
Cmd_CreditGive_f
==================
*/
void Cmd_CreditGive_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int client_id = 0, value = 0;

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent - g_entities, "print \"You must specify a player.\n\"" );
		return;
	}

	if (trap->Argc() == 2)
	{
		trap->SendServerCommand( ent - g_entities, "print \"You must specify the amount of credits.\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	trap->Argv( 2, arg2, sizeof( arg2 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );
	value = atoi(arg2);

	if (client_id == -1)
	{
		return;
	}

	if (value < 1)
	{
		trap->SendServerCommand( ent - g_entities, va("print \"Can only use positive values.\n\"" ));
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode < 2)
	{
		trap->SendServerCommand( ent - g_entities, va("print \"The player is not in RPG Mode\n\"" ));
		return;
	}

	if ((ent->client->pers.credits - value) < 0)
	{
		trap->SendServerCommand(ent - g_entities, va(("print \"^1You can\'t give ^3%i ^1credits.\n^7You only have ^3%i ^7credits.\n\""), value, ent->client->pers.credits));
		return;
	}
	
	
	add_credits(&g_entities[client_id], value);
	update_credits_value(&g_entities[client_id]);

	remove_credits(ent, value);
	update_credits_value(ent);

	//broadcast the transaction to the whole server

	trap->SendServerCommand(-1, va("chat \"^3Credit System: ^7%s ^7transferred ^2%d ^7credits to %s\n\"", ent->client->pers.netname, value, g_entities[client_id].client->pers.netname));

	trap->SendServerCommand(ent - g_entities, "print \"Done.\n\"");
}

/*
==================
Cmd_AllyList_f
==================
*/
void Cmd_AllyList_f( gentity_t *ent ) {
	char message[1024];
	int i = 0;

	strcpy(message,"");

	for (i = 0; i < level.maxclients; i++)
	{
		int shown = 0;
		gentity_t *this_ent = &g_entities[i];

		if (zyk_is_ally(ent,this_ent) == qtrue)
		{
			strcpy(message,va("%s^7%s ^3(ally)",message,this_ent->client->pers.netname));
			shown = 1;
		}
		if (this_ent && this_ent->client && this_ent->client->pers.connected == CON_CONNECTED && zyk_is_ally(this_ent,ent) == qtrue)
		{
			if (shown == 1)
				strcpy(message,va("%s ^3(added you)",message));
			else
				strcpy(message,va("%s^7%s ^3(added you)",message,this_ent->client->pers.netname));

			shown = 1;
		}

		if (shown == 1)
		{
			strcpy(message,va("%s\n",message));
		}
	}

	trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", message) );
}

void zyk_add_ally(gentity_t *ent, int client_id)
{
	if (client_id > 15)
	{
		ent->client->sess.ally2 |= (1 << (client_id - 16));
	}
	else
	{
		ent->client->sess.ally1 |= (1 << client_id);
	}
}

void zyk_remove_ally(gentity_t *ent, int client_id)
{
	if (client_id > 15)
	{
		ent->client->sess.ally2 &= ~(1 << (client_id - 16));
	}
	else
	{
		ent->client->sess.ally1 &= ~(1 << client_id);
	}
}

/*
==================
Cmd_AllyAdd_f
==================
*/
void Cmd_AllyAdd_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{
		trap->SendServerCommand(ent->s.number, va("print \"^1Command Usage: ^3/allyadd ^2<player name or id>\n^1Example: ^3/allyadd ^2Alex\n\"") );
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		int client_id = -1;

		// GalaxyRP fix: [Guardian] a guardian_mode>0 guard blocking /allyadd during a boss battle used
		// to be here. guardian_mode is permanently 0 now, so it was unreachable.

		if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
		{ // zyk: cant add allies while in Duel Tournament
			trap->SendServerCommand(ent->s.number, va("print \"You cannot add allies while in a Duel Tournament.\n\""));
			return;
		}

		trap->Argv(1, arg1, sizeof( arg1 ));
		client_id = ClientNumberFromString( ent, arg1, qfalse ); 

		if (client_id == -1)
		{
			return;
		}

		if (client_id == (ent-g_entities))
		{ // zyk: player cant add himself as ally
			trap->SendServerCommand(ent->s.number, va("print \"You cannot add yourself as ally\n\"") );
			return; 
		}

		if (zyk_is_ally(ent,&g_entities[client_id]) == qtrue)
		{
			trap->SendServerCommand(ent->s.number, va("print \"You already have this ally.\n\"") );
			return;
		}

		// zyk: add this player as an ally
		zyk_add_ally(ent, client_id);

		// zyk: sending event to update radar at client-side
		G_AddEvent(ent, EV_USE_ITEM14, client_id);

		trap->SendServerCommand(ent->s.number, va("print \"Added ally %s^7\n\"", g_entities[client_id].client->pers.netname) );
		trap->SendServerCommand( client_id, va("print \"%s^7 added you as ally\n\"", ent->client->pers.netname) );
	}
}

/*
==================
Cmd_AllyChat_f
==================
*/
void Cmd_AllyChat_f( gentity_t *ent ) { // zyk: allows chatting with allies
	char *p = NULL;

	if ( trap->Argc () < 2 )
		return;

	p = ConcatArgs( 1 );

	if ( strlen( p ) >= MAX_SAY_TEXT ) {
		p[MAX_SAY_TEXT-1] = '\0';
		G_SecurityLogPrintf( "Cmd_AllyChat_f from %d (%s) has been truncated: %s\n", ent->s.number, ent->client->pers.netname, p );
	}

	G_Say( ent, NULL, SAY_ALLY, p );
}

/*
==================
Cmd_AllyRemove_f
==================
*/
void Cmd_AllyRemove_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"^1Command Usage: ^3/allyremove <player name or id>\n^1Example: ^3/allyremove ^2Alex\n\"") );
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		int client_id = -1;

		if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
		{ // zyk: cant remove allies while in Duel Tournament
			trap->SendServerCommand(ent->s.number, va("print \"You cannot remove allies while in a Duel Tournament.\n\""));
			return;
		}

		trap->Argv(1, arg1, sizeof( arg1 ));
		client_id = ClientNumberFromString( ent, arg1, qfalse ); 

		if (client_id == -1)
		{
			return;
		}

		if (zyk_is_ally(ent,&g_entities[client_id]) == qfalse)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"You do not have this ally.\n\"") );
			return;
		}

		// zyk: removes this ally
		zyk_remove_ally(ent, client_id);

		// zyk: sending event to update radar at client-side
		G_AddEvent(ent, EV_USE_ITEM14, (client_id + MAX_CLIENTS));

		trap->SendServerCommand( ent-g_entities, va("print \"Removed ally %s^7\n\"", g_entities[client_id].client->pers.netname) );
		trap->SendServerCommand( client_id, va("print \"%s^7 removed you as an ally\n\"", ent->client->pers.netname) );
	}
}
// TODO: JLH start here
/*
==================
Cmd_Settings_f
==================
*/
void Cmd_Settings_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{
		char message[1024];
		int len = 0;
		strcpy(message,"");

		// GalaxyRP fix: [Quests] the status line for setting 0 (RPG quests) used to be printed here.
		// /settings 0 has been removed below (see the range-check comment further down) since the
		// general quest system it toggled is gone, so its status line is removed here to match.

		// GalaxyRP fix: [Quests] the status lines for settings 1-4 (Light/Dark/Eternity/Universe Power)
		// used to be printed here. Those quest-completion-granted powers can no longer be earned now
		// that the general quest system is gone (see the GalaxyRP fix comment on quest_get_new_player's
		// old location in this file), so /settings 1-4 have been removed below and their status lines
		// removed here to match.

		// GalaxyRP fix: [Sprintf Overlap] every sprintf() below used to write into message while also
		// reading message as its own %s argument (e.g. sprintf(message, "%s\n...", message)). Passing
		// the same object as both destination and source to sprintf is undefined behavior per the C
		// standard (it's unspecified whether the read happens before, during, or after the write) --
		// GCC already flagged this with -Wrestrict. Fixed by writing to message+len instead, so each
		// call's destination no longer overlaps anything it reads, and tracking the appended length in
		// len rather than re-reading it back out of message.
		if (ent->client->pers.player_settings & (1 << 5))
		{
			len += sprintf(message + len, "\n^3 1 - Language - ^1Custom");
		}
		else
		{
			len += sprintf(message + len, "\n^3 1 - Language - ^3English");
		}

		if (ent->client->pers.player_settings & (1 << 6))
		{
			len += sprintf(message + len, "\n^3 2 - Allow Force Powers from allies - ^1OFF");
		}
		else
		{
			len += sprintf(message + len, "\n^3 2 - Allow Force Powers from allies - ^2ON");
		}

		// GalaxyRP fix: [Magic] the status line for setting 7 (Show magic cast in chat) used to be
		// printed here. /settings 7 has been removed below (see the range-check comment further down)
		// since the chat line it gated, zyk_show_magic_in_chat(), has been removed as dead -- it lost
		// its only callers when the seven magic/ultimate powers it announced were themselves removed
		// as permanently unreachable (see the fix comment on TryGrapple()'s old dispatch logic), so
		// its status line is removed here to match.

		// zyk: Saber Style flags
		if (ent->client->pers.player_settings & (1 << 26))
			len += sprintf(message + len, "\n^3 3 - Starting Single Saber Style - ^3Yellow");
		else if (ent->client->pers.player_settings & (1 << 27))
			len += sprintf(message + len, "\n^3 3 - Starting Single Saber Style - ^1Red");
		else if (ent->client->pers.player_settings & (1 << 28))
			len += sprintf(message + len, "\n^3 3 - Starting Single Saber Style - ^1Desann");
		else if (ent->client->pers.player_settings & (1 << 29))
			len += sprintf(message + len, "\n^3 3 - Starting Single Saber Style - ^5Tavion");
		else
			len += sprintf(message + len, "\n^3 3 - Starting Single Saber Style - ^5Blue");

		if (ent->client->pers.player_settings & (1 << 9))
		{
			len += sprintf(message + len, "\n^3 4 - Allow Screen Message - ^1OFF");
		}
		else
		{
			len += sprintf(message + len, "\n^3 4 - Allow Screen Message - ^2ON");
		}

		if (ent->client->pers.player_settings & (1 << 10))
		{
			len += sprintf(message + len, "\n^3 5 - Use healing force only at allied players - ^1OFF");
		}
		else
		{
			len += sprintf(message + len, "\n^3 5 - Use healing force only at allied players - ^2ON");
		}

		if (ent->client->pers.player_settings & (1 << 11))
		{
			len += sprintf(message + len, "\n^3 6 - Start With Saber ^1OFF");
		}
		else
		{
			len += sprintf(message + len, "\n^3 6 - Start With Saber ^2ON");
		}

		// GalaxyRP fix: [Settings] the status line for setting 12 (Jetpack) used to be printed here.
		// /settings 12 has been removed below (see the range-check comment further down) -- its bit was
		// only ever read back by this same status line, never by anything gating actual jetpack
		// availability (that's Cmd_Jetpack_f, which checks unrelated fields), so toggling it never did
		// anything.

		if (ent->client->pers.player_settings & (1 << 13))
		{
			len += sprintf(message + len, "\n^3 7 - Admin Protect ^1OFF");
		}
		else
		{
			len += sprintf(message + len, "\n^3 7 - Admin Protect ^2ON");
		}

		// GalaxyRP fix: [Challenge Mode] the status lines for settings 14 (Boss Battle Music) and 15
		// (Difficulty/Challenge Mode) used to be printed here. Both settings have been removed below
		// (see the range-check comment further down) since everything downstream of Challenge Mode
		// activation is dead, so their status lines are removed here to match.

		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\n^7Choose a setting above and use ^3/settings <number> ^7to turn it ^2ON ^7or ^1OFF^7\n\"", message) );
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		char new_status[32];
		int value = 0;

		trap->Argv(1, arg1, sizeof( arg1 ));
		value = atoi(arg1);

		// GalaxyRP fix: [Settings] player-facing /settings numbers renumbered to a clean 1-7 sequence
		// (previously 5,6,8,9,10,11,13 -- gaps left behind by settings 0-4,7,12,14,15, which were
		// removed in earlier passes of this same cleanup; see the status-line comments above for why
		// each one is gone). The underlying player_settings bit positions below are NOT renumbered --
		// doing that would mean migrating every already-saved player_settings value in the database,
		// plus updating every other reader of these bits in g_main.c/g_client.c/w_force.c. Instead this
		// table translates the number the player types into the real bit index those readers still
		// expect; everything below continues to operate on that real bit index exactly as before.
		static const int settings_number_to_bit[] = { 0, 5, 6, 8, 9, 10, 11, 13 }; // index 0 unused (rejected below)

		if (value <= 0 || value >= (int)ARRAY_LEN(settings_number_to_bit))
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid settings value.\n\"" );
			return;
		}

		value = settings_number_to_bit[value];

		if (value != 8)
		{
			if (ent->client->pers.player_settings & (1 << value))
			{
				ent->client->pers.player_settings &= ~(1 << value);

				if (value == 5)
					strcpy(new_status, "^3English^7");
				else
					strcpy(new_status,"^2ON^7");
			}
			else
			{
				ent->client->pers.player_settings |= (1 << value);

				if (value == 5)
					strcpy(new_status, "^1Custom^7");
				else
					strcpy(new_status,"^1OFF^7");
			}
		}
		// GalaxyRP fix: [Challenge Mode] the value==14 (Boss Battle Music cycling) and value==15
		// (Difficulty/Challenge Mode activation gate) branches used to be here. Both are removed since
		// 14 and 15 are now rejected above as invalid settings values, and everything downstream of
		// Challenge Mode activation is dead.
		else
		{ // zyk: starting saber style has its own handling code
			if (ent->client->pers.player_settings & (1 << 26))
			{
				ent->client->pers.player_settings &= ~(1 << 26);
				ent->client->pers.player_settings |= (1 << 27);
				strcpy(new_status,"^1Red^7");
			}
			else if (ent->client->pers.player_settings & (1 << 27))
			{
				ent->client->pers.player_settings &= ~(1 << 27);
				ent->client->pers.player_settings |= (1 << 28);
				strcpy(new_status,"^1Desann^7");
			}
			else if (ent->client->pers.player_settings & (1 << 28))
			{
				ent->client->pers.player_settings &= ~(1 << 28);
				ent->client->pers.player_settings |= (1 << 29);
				strcpy(new_status,"^5Tavion");
			}
			else if (ent->client->pers.player_settings & (1 << 29))
			{
				ent->client->pers.player_settings &= ~(1 << 29);
				strcpy(new_status,"^5Blue^7");
			}
			else
			{
				ent->client->pers.player_settings |= (1 << 26);
				strcpy(new_status,"^3Yellow^7");
			}
		}

		save_account(ent, qfalse);

		// GalaxyRP fix: [Quests] the value==0 (RPG quests) print branch used to be here. Removed since
		// 0 is now rejected above as an invalid settings value.
		if (value == 5)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Language %s\n\"", new_status) );
		}
		else if (value == 6)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Allow Force Powers from allies %s\n\"", new_status) );
		}
		// GalaxyRP fix: [Magic] the value==7 (Show magic cast in chat) print branch used to be here.
		// Removed since 7 is now rejected above as an invalid settings value.
		else if (value == 8)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Starting Single Saber Style %s\n\"", new_status) );
		}
		else if (value == 9)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Allow Screen Message %s\n\"", new_status) );
		}
		else if (value == 10)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Use healing force only at allied players %s\n\"", new_status) );
		}
		else if (value == 11)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Start With Saber %s\n\"", new_status) );
		}
		else if (value == 13)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Admin Protect %s\n\"", new_status) );
		}
		// GalaxyRP fix: [Challenge Mode] the value==14 (Boss Battle Music) and value==15 (Difficulty)
		// print branches used to be here. Removed since 14 and 15 are now rejected above as invalid
		// settings values.

		// GalaxyRP fix: [Quests] the value==0 kill-on-toggle block used to be here, forcing a player
		// out of round when toggling RPG quests. Removed since 0 is now rejected above as an invalid
		// settings value.

	}
}

char *zyk_config_filename(gclient_t *client)
{
	// GalaxyRP fix: [Classes] rpg_class is permanently 0 now that character classes are gone, so this
	// always resolved to the freewarrior filename. The rpg_class==1..9 branches and the trailing
	// else were unreachable and have been removed.
	return va("GalaxyRP/configs/%s_%s_freewarrior.txt", client->sess.filename, client->sess.rpgchar);
}

char *zyk_legacy_config_filename(gclient_t *client)
{
	// GalaxyRP fix: [Classes] rpg_class is permanently 0 now that character classes are gone, so this
	// always resolved to the freewarrior filename. The rpg_class==1..9 branches and the trailing
	// else were unreachable and have been removed.
	return va("GalaxyRP/configs/%s_freewarrior.txt", client->sess.filename);
}

void save_config(gentity_t *ent)
{
	FILE *config_file = NULL;
	gclient_t *client;
	int unique_ability_flag = 0;

	client = ent->client;

	zyk_create_dir("configs");

	config_file = fopen(zyk_config_filename(client),"w");

	if (config_file != NULL)
	{
		// zyk: saving Unique Ability bought for this class
		if (client->pers.secrets_found & (1 << 2))
		{
			unique_ability_flag = 2;
		}
		else if (client->pers.secrets_found & (1 << 3))
		{
			unique_ability_flag = 3;
		}
		else if (client->pers.secrets_found & (1 << 4))
		{
			unique_ability_flag = 4;
		}

		fprintf(config_file,"%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n",
		client->pers.level,client->pers.skill_levels[0],client->pers.skill_levels[1],client->pers.skill_levels[2]
			,client->pers.skill_levels[3],client->pers.skill_levels[4],client->pers.skill_levels[5],client->pers.skill_levels[6],client->pers.skill_levels[7],client->pers.skill_levels[8]
			,client->pers.skill_levels[9],client->pers.skill_levels[10],client->pers.skill_levels[11],client->pers.skill_levels[12],client->pers.skill_levels[13],client->pers.skill_levels[14]
			,client->pers.skill_levels[15],client->pers.skill_levels[16],client->pers.skill_levels[17],client->pers.skill_levels[18],client->pers.skill_levels[19],client->pers.skill_levels[20],client->pers.skill_levels[21],client->pers.skill_levels[22]
			,client->pers.skill_levels[23],client->pers.skill_levels[24],client->pers.skill_levels[25],client->pers.skill_levels[26],client->pers.skill_levels[27],client->pers.skill_levels[28],client->pers.skill_levels[29],client->pers.skill_levels[30],client->pers.skill_levels[31]
			,client->pers.skill_levels[32],client->pers.skill_levels[33],client->pers.skill_levels[34],client->pers.skill_levels[35],client->pers.skill_levels[36],client->pers.skill_levels[37],client->pers.skill_levels[38],client->pers.skill_levels[39],client->pers.skill_levels[40],client->pers.skill_levels[41]
			,client->pers.skill_levels[42],client->pers.skill_levels[43],client->pers.skill_levels[44],client->pers.skill_levels[45],client->pers.skill_levels[46],client->pers.skill_levels[47],client->pers.skill_levels[48],client->pers.skill_levels[49]
			,client->pers.skill_levels[50],client->pers.skill_levels[51],client->pers.skill_levels[52],client->pers.skill_levels[53],client->pers.skill_levels[54],client->pers.skill_levels[55],unique_ability_flag);
		
		fclose(config_file);
	}
}

// GalaxyRP fix: [Quests] Cmd_GuardianQuest_f and Cmd_BountyQuest_f used to live here. Both were
// fully dead -- an unconditional `return;` as their first statement, and Cmd_GuardianQuest_f was not
// even registered in the commands[] dispatch table (Cmd_BountyQuest_f's entry was already commented
// out there). Deleted outright, along with the now-unreferenced zyk_allow_guardian_quest/
// zyk_allow_bounty_quest cvars, per the same convention used for other confirmed-unreachable code in
// this file.

// GalaxyRP fix: [Cvars] Cmd_PlayerMode_f (which read zyk_allow_rpg_mode, now removed) used to live
// here. It was already fully orphaned before this change -- not registered in the commands[] dispatch
// table, not commented out there either, and not prototyped or called from anywhere else in the
// codebase -- so it has been deleted outright rather than short-circuited, per the same convention
// used for other confirmed-unreachable code in this file.

void zyk_spawn_race_line(int x, int y, int z, int yaw)
{
	gentity_t *new_ent_line = G_Spawn();

	// zyk: starting line
	zyk_set_entity_field(new_ent_line, "classname", "fx_runner");
	zyk_set_entity_field(new_ent_line, "targetname", "zyk_race_line");
	new_ent_line->s.modelindex = G_EffectIndex("mp/crystalbeamred");
	zyk_set_entity_field(new_ent_line, "origin", va("%d %d %d", x, y, z));
	zyk_set_entity_field(new_ent_line, "angles", va("0 %d 0", yaw));

	zyk_spawn_entity(new_ent_line);
}

/*
==================
Cmd_RaceMode_f
==================
*/
void Cmd_RaceMode_f( gentity_t *ent ) {
	if (zyk_allow_race_mode.integer != 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^3Race System: ^7this mode is not allowed in this server\n\""));
		return;
	}

	if (ent->client->pers.player_statuses & (1 << 26))
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join race while being in nofight mode\n\"");
		return;
	}

	if (ent->client->pers.race_position == 0)
	{
		int j = 0, swoop_number = -1;
		int occupied_positions[MAX_CLIENTS]; // zyk: sets 1 to each race position already occupied by a player
		gentity_t *this_ent = NULL;
		vec3_t origin, yaw;
		char zyk_info[MAX_INFO_STRING] = {0};
		char zyk_mapname[128] = {0};

		if (level.gametype == GT_CTF)
		{
			trap->SendServerCommand(ent->s.number, "print \"Races are not allowed in CTF.\n\"");
			return;
		}

		if (level.race_mode > 1)
		{
			trap->SendServerCommand(ent->s.number, "print \"Race has already started. Try again at the next race!\n\"");
			return;
		}

		// GalaxyRP fix: [Guardian] a loop blocking race start while any player had guardian_mode > 0
		// used to be here. guardian_mode is permanently 0 now, so it was unreachable.

		// zyk: getting the map name
		trap->GetServerinfo(zyk_info, sizeof(zyk_info));
		Q_strncpyz(zyk_mapname, Info_ValueForKey( zyk_info, "mapname" ), sizeof(zyk_mapname));

		if (Q_stricmp(zyk_mapname, "t2_trip") == 0)
		{
			level.race_map = 1;

			// zyk: initializing array of occupied_positions
			for (j = 0; j < MAX_CLIENTS; j++)
			{
				occupied_positions[j] = 0;
			}

			// zyk: calculates which position the swoop of this player must be spawned
			for (j = 0; j < MAX_CLIENTS; j++)
			{
				this_ent = &g_entities[j];
				if (this_ent && ent != this_ent && this_ent->client && this_ent->inuse && this_ent->health > 0 && this_ent->client->sess.sessionTeam != TEAM_SPECTATOR && this_ent->client->pers.race_position > 0)
					occupied_positions[this_ent->client->pers.race_position - 1] = 1;
			}

			for (j = 0; j < MAX_RACERS; j++)
			{
				if (occupied_positions[j] == 0)
				{ // zyk: an empty race position, use this one
					swoop_number = j;
					break;
				}
			}

			if (swoop_number == -1)
			{ // zyk: exceeded the MAX_RACERS
				trap->SendServerCommand( ent-g_entities, "print \"The race is already full of racers! Try again later!\n\"" );
				return;
			}
			
			origin[0] = -3930;
			origin[1] = (-20683 + (swoop_number * 80));
			origin[2] = 1509;

			yaw[0] = 0.0f;
			yaw[1] = -179.0f;
			yaw[2] = 0.0f;

			if (level.race_mode == 0)
			{ // zyk: if this is the first player entering the race, clean the old race swoops left in the map and place start and finish lines
				int k = 0;

				// zyk: starting line
				zyk_spawn_race_line(-4568, -20820, 1494, 90);
				zyk_spawn_race_line(-4568, -18720, 1494, -90);

				// zyk: finish line
				zyk_spawn_race_line(4750, -9989, 1520, 179);
				zyk_spawn_race_line(3225, -9962, 1520, -1);

				for (k = 0; k < MAX_RACERS; k++)
				{
					if (level.race_mode_vehicle[k] != -1)
					{
						gentity_t *vehicle_ent = &g_entities[level.race_mode_vehicle[k]];
						if (vehicle_ent)
						{
							G_FreeEntity(vehicle_ent);
						}
						
						level.race_mode_vehicle[k] = -1;
					}
				}
			}

			if (swoop_number < MAX_RACERS)
			{
				// zyk: removing a possible swoop that was in the same position by a player who tried to race before in this position
				if (level.race_mode_vehicle[swoop_number] != -1)
				{
					gentity_t *vehicle_ent = &g_entities[level.race_mode_vehicle[swoop_number]];

					if (vehicle_ent && vehicle_ent->NPC && Q_stricmp(vehicle_ent->NPC_type, "swoop") == 0)
					{
						G_FreeEntity(vehicle_ent);
					}
				}

				// zyk: teleporting player to the swoop area
				zyk_TeleportPlayer( ent, origin, yaw);

				ent->client->pers.race_position = swoop_number + 1;

				this_ent = NPC_SpawnType(ent,"swoop",NULL,qtrue);
				if (this_ent)
				{ // zyk: setting the vehicle hover height and hover strength
					this_ent->m_pVehicle->m_pVehicleInfo->hoverHeight = 40.0;
					this_ent->m_pVehicle->m_pVehicleInfo->hoverStrength = 40.0;

					level.race_mode_vehicle[swoop_number] = this_ent->s.number;
				}

				level.race_start_timer = level.time + zyk_start_race_timer.integer; // zyk: race will start some seconds after the last player who joined the race
				level.race_mode = 1;

				trap->SendServerCommand( -1, va("chat \"^3Race System: ^7%s ^7joined the race!\n\"",ent->client->pers.netname) );
			}
		}
		else if (Q_stricmp(zyk_mapname, "t3_stamp") == 0)
		{
			int i = 0;

			level.race_map = 2;

			// zyk: initializing array of occupied_positions
			for (j = 0; j < MAX_CLIENTS; j++)
			{
				occupied_positions[j] = 0;
			}

			// zyk: calculates which position the swoop of this player must be spawned
			for (j = 0; j < MAX_CLIENTS; j++)
			{
				this_ent = &g_entities[j];
				if (this_ent && ent != this_ent && this_ent->client && this_ent->inuse && this_ent->health > 0 && this_ent->client->sess.sessionTeam != TEAM_SPECTATOR && this_ent->client->pers.race_position > 0)
					occupied_positions[this_ent->client->pers.race_position - 1] = 1;
			}

			for (j = 0; j < MAX_RACERS; j++)
			{
				if (occupied_positions[j] == 0)
				{ // zyk: an empty race position, use this one
					swoop_number = j;
					break;
				}
			}

			if (swoop_number == -1)
			{ // zyk: exceeded the MAX_RACERS
				trap->SendServerCommand(ent - g_entities, "print \"The race is already full of racers! Try again later!\n\"");
				return;
			}

			origin[0] = (1020 - ((swoop_number % 4) * 90));
			origin[1] = (1370 + ((swoop_number/4) * 90));
			origin[2] = 97;

			yaw[0] = 0.0f;
			yaw[1] = -90.0f;
			yaw[2] = 0.0f;

			if (level.race_mode == 0)
			{ // zyk: if this is the first player entering the race, clean the old race swoops left in the map
				int k = 0;

				for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
				{ // zyk: removing all entities except the spawnpoints
					gentity_t *removed_ent = &g_entities[i];

					if (removed_ent && Q_stricmp(removed_ent->classname, "func_breakable") == 0 && removed_ent->s.number >= 471 && removed_ent->s.number <= 472)
					{
						GlobalUse(removed_ent, removed_ent, removed_ent);
					}
					else if (removed_ent && Q_stricmp(removed_ent->classname, "info_player_deathmatch") != 0)
					{
						G_FreeEntity(removed_ent);
					}
				}

				for (k = 0; k < MAX_RACERS; k++)
				{
					if (level.race_mode_vehicle[k] != -1)
					{
						gentity_t *vehicle_ent = &g_entities[level.race_mode_vehicle[k]];
						if (vehicle_ent)
						{
							G_FreeEntity(vehicle_ent);
						}

						level.race_mode_vehicle[k] = -1;
					}
				}

				// zyk: starting line
				zyk_spawn_race_line(660, 1198, 88, 1);
				zyk_spawn_race_line(1070, 1198, 88, 179);

				// zyk: finish line
				zyk_spawn_race_line(-6425, -168, -263, -1);
				zyk_spawn_race_line(-5700, -180, -263, 179);
			}

			if (swoop_number < MAX_RACERS)
			{
				// zyk: removing a possible swoop that was in the same position by a player who tried to race before in this position
				if (level.race_mode_vehicle[swoop_number] != -1)
				{
					gentity_t *vehicle_ent = &g_entities[level.race_mode_vehicle[swoop_number]];

					if (vehicle_ent && vehicle_ent->NPC && Q_stricmp(vehicle_ent->NPC_type, "tauntaun") == 0)
					{
						G_FreeEntity(vehicle_ent);
					}
				}

				// zyk: teleporting player to the swoop area
				zyk_TeleportPlayer(ent, origin, yaw);

				ent->client->pers.race_position = swoop_number + 1;

				this_ent = NPC_SpawnType(ent, "tauntaun", NULL, qtrue);
				if (this_ent)
				{ // zyk: setting the vehicle id and increasing tauntaun hp
					this_ent->health *= 5;
					level.race_mode_vehicle[swoop_number] = this_ent->s.number;
				}

				level.race_start_timer = level.time + zyk_start_race_timer.integer; // zyk: race will start some seconds after the last player who joined the race
				level.race_mode = 1;

				trap->SendServerCommand(-1, va("chat \"^3Race System: ^7%s ^7joined the race!\n\"", ent->client->pers.netname));
			}
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, "print \"Races can only be done in ^3t2_trip ^7and ^3t3_stamp ^7maps.\n\"" );
		}
	}
	else
	{
		trap->SendServerCommand( -1, va("chat \"^3Race System: ^7%s ^7abandoned the race!\n\"",ent->client->pers.netname) );

		ent->client->pers.race_position = 0;
		try_finishing_race();
	}
}

/*
==================
Cmd_Drop_f
==================
*/
extern qboolean saberKnockOutOfHand(gentity_t *saberent, gentity_t *saberOwner, vec3_t velocity);
void Cmd_Drop_f( gentity_t *ent ) {
	vec3_t vel;
	gitem_t *item = NULL;
	gentity_t *launched;
	int weapon = ent->client->ps.weapon;
	vec3_t uorg, vecnorm, thispush_org;
	int current_ammo = 0;
	int ammo_count = 0;

	if (weapon == WP_NONE || weapon == WP_EMPLACED_GUN || weapon == WP_TURRET)
	{ //can't have this
		return;
	}

	VectorCopy(ent->client->ps.origin, thispush_org);

	VectorCopy(ent->client->ps.origin, uorg);
	uorg[2] += 64;

	VectorSubtract(uorg, thispush_org, vecnorm);
	VectorNormalize(vecnorm);

	if (weapon == WP_SABER)
	{
		vel[0] = vecnorm[0]*100;
		vel[1] = vecnorm[1]*100;
		vel[2] = vecnorm[2]*100;
		saberKnockOutOfHand(&g_entities[ent->client->ps.saberEntityNum],ent,vel);
		return;
	}

	// zyk: velocity with which the item will be tossed
	vel[0] = vecnorm[0] * 500;
	vel[1] = vecnorm[1] * 500;
	vel[2] = vecnorm[2] * 500;

	// zyk: when using melee, drop items
	if (weapon == WP_MELEE && ent->client->ps.stats[STAT_HOLDABLE_ITEM] > 0 && 
		bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giType == IT_HOLDABLE)
	{
		item = BG_FindItemForHoldable(bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giTag);

		if (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << item->giTag))
		{ // zyk: if player has the item in inventory, drop it
			launched = LaunchItem(item, ent->client->ps.origin, vel);

			// zyk: this player cannot get this item for 1 second
			launched->genericValue10 = level.time + 1000;
			launched->genericValue11 = ent->s.number;

			// zyk: remove item from inventory
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << item->giTag);

			// GalaxyRP fix: [Classes] a rpg_class==2 (Bounty Hunter) sentry-gun-counter decrement used to
			// be here. rpg_class is permanently 0 now that character classes are gone, so this was
			// unreachable.

			zyk_adjust_holdable_items(ent);
		}
	}
	else if (weapon != WP_MELEE)
	{
		// find the item type for this weapon
		item = BG_FindItemForWeapon((weapon_t)weapon);

		launched = LaunchItem(item, ent->client->ps.origin, vel);

		launched->s.generic1 = ent->s.number;
		launched->s.powerups = level.time + 1500;

		launched->count = bg_itemlist[BG_GetItemIndexByTag(weapon, IT_WEAPON)].quantity;

		// zyk: setting amount of ammo in this dropped weapon
		current_ammo = ent->client->ps.ammo[weaponData[weapon].ammoIndex];
		ammo_count = (int)ceil(bg_itemlist[BG_GetItemIndexByTag(weapon, IT_WEAPON)].quantity * zyk_add_ammo_scale.value);

		if (current_ammo < ammo_count)
		{ // zyk: player does not have the default ammo to set in the weapon, so set the current_ammo of the player in the weapon
			ent->client->ps.ammo[weaponData[weapon].ammoIndex] -= current_ammo;
			if (zyk_add_ammo_scale.value > 0 && current_ammo > 0)
				launched->count = (current_ammo / zyk_add_ammo_scale.value);
			else
				launched->count = -1; // zyk: in this case, player has no ammo, so weapon should add no ammo to the player who picks up this weapon
		}
		else
		{
			ent->client->ps.ammo[weaponData[weapon].ammoIndex] -= ammo_count;
			if (zyk_add_ammo_scale.value > 0 && current_ammo > 0)
				launched->count = (ammo_count / zyk_add_ammo_scale.value);
			else
				launched->count = -1; // zyk: in this case, player has no ammo, so weapon should add no ammo to the player who picks up this weapon
		}

		if ((ent->client->ps.ammo[weaponData[weapon].ammoIndex] < 1 && weapon != WP_DET_PACK) ||
			(weapon != WP_THERMAL && weapon != WP_DET_PACK && weapon != WP_TRIP_MINE))
		{
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << weapon);

			ent->s.weapon = WP_MELEE;
			ent->client->ps.weapon = WP_MELEE;
		}
	}
}

/*
==================
Cmd_Jetpack_f
==================
*/
void Cmd_Jetpack_f( gentity_t *ent ) {
	if (level.melee_mode > 1 && level.melee_players[ent->s.number] != -1)//<-
	{ // zyk: cannot get jetpack in Melee Battle
		return;
	}

	// GalaxyRP fix: [Jetpack] zyk_allow_jetpack_in_siege has been removed -- jetpack is now never
	// allowed in Siege, full stop (this is exactly the cvar's old default, "0"/never-allow, just no
	// longer configurable).
	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK)) && jetpack_command_allowed(ent) &&
		(ent->client->sess.amrpgmode < 2 || ent->client->pers.skill_levels[34] > 0) &&
		level.gametype != GT_SIEGE && level.gametype != GT_JEDIMASTER &&
		!(ent->client->pers.player_statuses & (1 << 12)))
	{ // zyk: gets jetpack if player does not have it. RPG players need jetpack skill to get it
		// zyk: Jedi Master gametype will not allow jetpack
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);
	}
	else
	{
		if (ent->client->jetPackOn)
			Jetpack_Off(ent);
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_JETPACK);
	}
}

/*
==================
Cmd_Remap_f
==================
*/
void Cmd_Remap_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	float f = level.time * 0.001;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 3)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify the old shader and new shader. Ex: ^3/remap models/weapons2/heavy_repeater/heavy_repeater_w.glm models/items/bacta^7\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	trap->Argv( 2, arg2, sizeof( arg2 ) );

	AddRemap(G_NewString(arg1), G_NewString(arg2), f);
	trap->SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());

	trap->SendServerCommand( ent-g_entities, "print \"Shader remapped\n\"" );
}

/*
==================
Cmd_RemapList_f
==================
*/
void Cmd_RemapList_f(gentity_t *ent) {
	int page = 1; // zyk: page the user wants to see
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char content[MAX_STRING_CHARS];
	int i = 0;
	int results_per_page = 8; // zyk: number of results per page

	strcpy(content, "");

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if (number_of_args < 2)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You must specify the page. Example: ^3/remaplist 1^7\n\""));
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));
	
	page = atoi(arg1);

	if (page == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid page number\n\"");
		return;
	}

	// zyk: makes i start from the first result of the correct page
	i = results_per_page * (page - 1);

	while (i < (results_per_page * page) && i < zyk_get_remap_count())
	{
		strcpy(content, va("%s%s - %s\n", content, remappedShaders[i].oldShader, remappedShaders[i].newShader));
		i++;
	}

	trap->SendServerCommand(ent->s.number, va("print \"\n^3Old Shader   -   New Shader\n\n^7%s\n\"", content));
}

/*
==================
Cmd_RemapDeleteFile_f
==================
*/
void Cmd_RemapDeleteFile_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	FILE *this_file = NULL;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a file name.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("remaps/%s", zyk_mapname));

	this_file = fopen(va("GalaxyRP/remaps/%s/%s.txt",zyk_mapname,arg1),"r");
	if (this_file)
	{
		fclose(this_file);

		remove(va("GalaxyRP/remaps/%s/%s.txt",zyk_mapname,arg1));

		trap->SendServerCommand( ent-g_entities, va("print \"File %s deleted from server\n\"", arg1) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"File %s does not exist\n\"", arg1) );
	}
}

/*
==================
Cmd_RemapSave_f
==================
*/
void Cmd_RemapSave_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	int i = 0;
	FILE *remap_file = NULL;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify the file name\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("remaps/%s", zyk_mapname));

	// zyk: saving remaps in the file
	remap_file = fopen(va("GalaxyRP/remaps/%s/%s.txt",zyk_mapname,arg1),"w");
	for (i = 0; i < zyk_get_remap_count(); i++)
	{
		fprintf(remap_file,"%s\n%s\n%f\n",remappedShaders[i].oldShader,remappedShaders[i].newShader,remappedShaders[i].timeOffset);
	}
	fclose(remap_file);

	trap->SendServerCommand( ent-g_entities, va("print \"Remaps saved in %s file\n\"", arg1) );
}

/*
==================
Cmd_RemapLoad_f
==================
*/
void Cmd_RemapLoad_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	char old_shader[128];
	char new_shader[128];
	char time_offset[128];
	FILE *remap_file = NULL;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify the file name\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	strcpy(old_shader,"");
	strcpy(new_shader,"");
	strcpy(time_offset,"");

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("remaps/%s", zyk_mapname));

	// zyk: loading remaps from the file
	remap_file = fopen(va("GalaxyRP/remaps/%s/%s.txt",zyk_mapname,arg1),"r");
	if (remap_file)
	{
		while(fscanf(remap_file,"%s",old_shader) != EOF)
		{
			fscanf(remap_file,"%s",new_shader);
			fscanf(remap_file,"%s",time_offset);

			AddRemap(G_NewString(old_shader), G_NewString(new_shader), atof(time_offset));
		}
		
		fclose(remap_file);

		trap->SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());

		trap->SendServerCommand( ent-g_entities, va("print \"Remaps loaded from %s file\n\"", arg1) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Remaps not loaded. File %s not found\n\"", arg1) );
	}
}

/*
==================
Cmd_EntUndo_f
==================
*/
void Cmd_EntUndo_f(gentity_t *ent) {
	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if (level.last_spawned_entity)
	{ // zyk: removes the last entity spawned by /entadd command
		trap->SendServerCommand(ent->s.number, va("print \"Entity %d cleaned\n\"", level.last_spawned_entity->s.number));

		G_FreeEntity(level.last_spawned_entity);

		level.last_spawned_entity = NULL;
	}
}

/*
==================
Cmd_EntOrigin_f
==================
*/
void Cmd_EntOrigin_f(gentity_t *ent) {
	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if (level.ent_origin_set == qfalse)
	{
		VectorCopy(ent->client->ps.origin, level.ent_origin);
		VectorCopy(ent->client->ps.viewangles, level.ent_angles);
		level.ent_origin_set = qtrue;

		trap->SendServerCommand(ent->s.number, va("print \"Entity origin: (%f %f %f) angles: (%f %f %f)\n\"", level.ent_origin[0], level.ent_origin[1], level.ent_origin[2], level.ent_angles[0], level.ent_angles[1], level.ent_angles[2]));
	}
	else
	{
		level.ent_origin_set = qfalse;
		trap->SendServerCommand(ent->s.number, "print \"Entity origin unset\n\"");
	}
}

/*
==================
Cmd_EntAdd_f
==================
*/
void Cmd_EntAdd_f( gentity_t *ent ) {
	gentity_t *new_ent = NULL;
	int number_of_args = trap->Argc();
	int i = 0;
	char key[64];
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	qboolean has_origin_set = qfalse; // zyk: if player do not pass an origin key, use the one set with /entorigin
	qboolean has_angles_set = qfalse; // zyk: if player do not pass an angles key, use the one set with /entorigin

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Usage: ^3/entadd <classname> <key> <value> <key> <value>^7. You must specify at least the entity class.\n\
			^7Example: ^3/entadd info_player_deathmatch^7, which spawns a spawn point in the map\n\"") );
		return;
	}

	if ( number_of_args % 2 != 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify an even number of arguments after the classname, because they are key/value pairs\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	// zyk: spawns the new entity
	new_ent = G_Spawn();

	if (new_ent)
	{
		strcpy(key,"");

		// zyk: setting the entity classname
		zyk_main_set_entity_field(new_ent, "classname", G_NewString(arg1));

		for(i = 2; i < number_of_args; i++)
		{
			if (i % 2 == 0)
			{ // zyk: key
				trap->Argv( i, arg2, sizeof( arg2 ) );
				strcpy(key, G_NewString(arg2));

				if (Q_stricmp(key, "origin") == 0)
				{ // zyk: if origin was passed
					has_origin_set = qtrue;
				}

				if (Q_stricmp(key, "angles") == 0)
				{ // zyk: if angles was passed
					has_angles_set = qtrue;
				}
			}
			else
			{ // zyk: value
				trap->Argv( i, arg2, sizeof( arg2 ) );

				zyk_main_set_entity_field(new_ent, G_NewString(key), G_NewString(arg2));
			}
		}

		if (level.ent_origin_set == qtrue && (has_origin_set == qfalse || has_angles_set == qfalse))
		{ // zyk: if origin or angles were not passed, use the origin or angles set with /entorigin
			if (has_origin_set == qfalse)
			{
				zyk_main_set_entity_field(new_ent, "origin", G_NewString(va("%f %f %f", level.ent_origin[0], level.ent_origin[1], level.ent_origin[2])));
			}

			if (has_angles_set == qfalse)
			{
				zyk_main_set_entity_field(new_ent, "angles", G_NewString(va("%f %f %f", level.ent_angles[0], level.ent_angles[1], level.ent_angles[2])));
			}
		}
		else if (has_origin_set == qfalse)
		{ // zyk: origin field was not passed, so spawn entity where player is aiming at
			trace_t		tr;
			vec3_t		tfrom, tto, fwd;
			vec3_t		shot_mins, shot_maxs;
			int radius = 32768;

			VectorSet(tfrom, ent->client->ps.origin[0], ent->client->ps.origin[1], ent->client->ps.origin[2] + 35);

			AngleVectors(ent->client->ps.viewangles, fwd, NULL, NULL);
			tto[0] = tfrom[0] + fwd[0] * radius;
			tto[1] = tfrom[1] + fwd[1] * radius;
			tto[2] = tfrom[2] + fwd[2] * radius;

			VectorSet(shot_mins, -5, -5, -5);
			VectorSet(shot_maxs, 5, 5, 5);

			trap->Trace(&tr, tfrom, shot_mins, shot_maxs, tto, ent->s.number, CONTENTS_SOLID, qfalse, 0, 0);

			if (tr.fraction != 1.0)
			{ // zyk: hit something
				zyk_main_set_entity_field(new_ent, "origin", G_NewString(va("%f %f %f", tr.endpos[0], tr.endpos[1], tr.endpos[2])));
			}
		}

		zyk_main_spawn_entity(new_ent);

		if (new_ent->s.number != 0)
		{
			level.last_spawned_entity = new_ent;
		}

		trap->SendServerCommand( ent-g_entities, va("print \"Entity %d spawned\n\"", new_ent->s.number) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Error in entity spawn\n\"") );
		return;
	}
}

/*
==================
Cmd_EntEdit_f
==================
*/
void Cmd_EntEdit_f( gentity_t *ent ) {
	gentity_t *this_ent = NULL;
	int number_of_args = trap->Argc();
	int entity_id = -1;
	int i = 0;
	char key[64];
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify at least the entity ID.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	entity_id = atoi(arg1);

	if (entity_id < 0 || entity_id >= level.num_entities)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Invalid Entity ID.\n\"") );
		return;
	}

	this_ent = &g_entities[entity_id];

	if (number_of_args == 2)
	{
		// zyk: players have their origin and yaw set in ps struct
		if (entity_id < (MAX_CLIENTS + BODY_QUEUE_SIZE))
			trap->SendServerCommand( ent-g_entities, va("print \"\n^3classname: ^7%s\n^3origin: ^7%f %f %f\n\n\"", this_ent->classname, this_ent->r.currentOrigin[0], this_ent->r.currentOrigin[1], this_ent->r.currentOrigin[2]) );
		else
		{
			char content[1024];

			strcpy(content, "");

			if (this_ent->inuse)
			{ 
				while (i < level.zyk_spawn_strings_values_count[entity_id])
				{
					strcpy(content, va("%s^3%s: ^7%s\n", content, level.zyk_spawn_strings[this_ent->s.number][i], level.zyk_spawn_strings[this_ent->s.number][i + 1]));

					i += 2;
				}

				trap->SendServerCommand(ent - g_entities, va("print \"\n%s\n\"", content));
			}
			else
			{
				trap->SendServerCommand(ent - g_entities, va("print \"Entity %d is not in use\n\"", entity_id));
			}
		}
	}
	else
	{
		if ( number_of_args % 2 != 0)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"You must specify an even number of arguments, because they are key/value pairs.\n\"") );
			return;
		}

		strcpy(key,"");

		for(i = 2; i < number_of_args; i++)
		{
			if (i % 2 == 0)
			{ // zyk: key
				trap->Argv(i, arg2, sizeof(arg2));
				strcpy(key, G_NewString(arg2));
			}
			else
			{ // zyk: value
				trap->Argv(i, arg2, sizeof(arg2));

				zyk_main_set_entity_field(this_ent, G_NewString(key), G_NewString(arg2));
			}
		}

		zyk_main_spawn_entity(this_ent);

		trap->SendServerCommand(ent-g_entities, va("print \"Entity %d edited\n\"", this_ent->s.number) );
	}
}

// GalaxyRP (Alex): Builds an npc spawner string based on an npc entity.
char *create_npc_spawner_for_npc(gentity_t *ent) {
	return va("classname;npc_spawner;npc_type;%s;origin;%f %f %f;angles;%f %f %f;\n", ent->NPC_type, ent->client->ps.origin[0], ent->client->ps.origin[1], ent->client->ps.origin[2], ent->client->ps.viewangles[0], ent->client->ps.viewangles[1], ent->client->ps.viewangles[2]);
}

/*
==================
Cmd_EntSave_f
==================
*/
void Cmd_EntSave_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	int i = 0;
	int j = 0;
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	FILE *this_file = NULL;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent->s.number, va("print \"You must specify a file name.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	// GalaxyRP: [security] arg1 is spliced straight into the GalaxyRP/entities/<map>/<arg1>.txt path
	// below -- reject anything but letters and digits so a crafted value (e.g. containing "../")
	// can't write outside the intended folder.
	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand( ent->s.number, "print \"Invalid file name. Only letters and numbers allowed.\n\"" );
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("entities/%s", zyk_mapname));

	// zyk: saving the entities into the file
	this_file = fopen(va("GalaxyRP/entities/%s/%s.txt",zyk_mapname,arg1),"w");

	for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
	{
		gentity_t *this_ent = &g_entities[i];

		if (this_ent && this_ent->inuse)
		{ // zyk: freed entities will not be saved
			j = 0;

			while (j < level.zyk_spawn_strings_values_count[this_ent->s.number])
			{
				fprintf(this_file, "%s;%s;", level.zyk_spawn_strings[this_ent->s.number][j], level.zyk_spawn_strings[this_ent->s.number][j + 1]);

				j += 2;
			}

			if (j > 0)
			{ // zyk: break line only if the entity had keys and values to save
				fprintf(this_file, "\n");
			}
		}
		// GalaxyRP (Alex): NPCs should be saved as NPC spawners instead. So do the conversion.
		if (strcmp(this_ent->classname, "NPC") == 0) {
			fprintf(this_file, create_npc_spawner_for_npc(this_ent));
		}
	}

	fclose(this_file);

	trap->SendServerCommand( ent->s.number, va("print \"Entities saved in %s file\n\"", arg1) );
}

/*
==================
Cmd_EntLoad_f
==================
*/
void Cmd_EntLoad_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	int i = 0;
	FILE *this_file = NULL;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a file name.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	// GalaxyRP: [security] arg1 is spliced straight into the GalaxyRP/entities/<map>/<arg1>.txt path
	// below -- reject anything but letters and digits so a crafted value (e.g. containing "../")
	// can't read from outside the intended folder.
	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid file name. Only letters and numbers allowed.\n\"" );
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("entities/%s", zyk_mapname));

	strcpy(level.load_entities_file, va("GalaxyRP/entities/%s/%s.txt",zyk_mapname,arg1));

	this_file = fopen(level.load_entities_file,"r");
	if (this_file)
	{ // zyk: loads entities from the file if it exists
		fclose(this_file);

		// zyk: cleaning entities. Only the ones from the file will be in the map
		for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
		{
			gentity_t *target_ent = &g_entities[i];

			if (target_ent)
				G_FreeEntity( target_ent );
		}

		level.load_entities_timer = level.time + 1050;

		trap->SendServerCommand( ent-g_entities, va("print \"Loading entities from %s file\n\"", arg1) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"File %s does not exist\n\"", arg1) );
	}
}

/*
==================
Cmd_EntDeleteFile_f
==================
*/
void Cmd_EntDeleteFile_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	FILE *this_file = NULL;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a file name.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	// GalaxyRP: [security] arg1 is spliced straight into the GalaxyRP/entities/<map>/<arg1>.txt path
	// below -- reject anything but letters and digits so a crafted value (e.g. containing "../")
	// can't read or delete files outside the intended folder.
	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid file name. Only letters and numbers allowed.\n\"" );
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("entities/%s", zyk_mapname));

	this_file = fopen(va("GalaxyRP/entities/%s/%s.txt",zyk_mapname,arg1),"r");
	if (this_file)
	{
		fclose(this_file);

		remove(va("GalaxyRP/entities/%s/%s.txt",zyk_mapname,arg1));

		trap->SendServerCommand( ent-g_entities, va("print \"File %s deleted from server\n\"", arg1) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"File %s does not exist\n\"", arg1) );
	}
}

/*
==================
Cmd_EntNear_f
==================
*/
void Cmd_EntNear_f( gentity_t *ent ) {
	int i = 0;
	int distance = 200;
	int numListedEntities = 0;
	int entityList[MAX_GENTITIES];
	vec3_t mins, maxs, center;
	char message[MAX_STRING_CHARS * 2];
	char arg1[MAX_STRING_CHARS];
	gentity_t *this_ent = NULL;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	strcpy(message,"");

	if (trap->Argc() > 1)
	{
		trap->Argv(1, arg1, sizeof(arg1));

		distance = atoi(arg1);
	}

	VectorCopy(ent->client->ps.origin, center);

	for (i = 0; i < 3; i++)
	{
		mins[i] = center[i] - distance;
		maxs[i] = center[i] + distance;
	}

	numListedEntities = trap->EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

	for (i = 0; i < numListedEntities; i++)
	{
		this_ent = &g_entities[entityList[i]];

		if (this_ent && ent != this_ent && this_ent->s.number >= (MAX_CLIENTS + BODY_QUEUE_SIZE) && this_ent->inuse == qtrue)
		{
			strcpy(message,va("%s\n%d - %s",message, this_ent->s.number,this_ent->classname));
		}

		if (strlen(message) > (MAX_STRING_CHARS - 11))
		{
			trap->SendServerCommand(ent->s.number, "print \"Too much info. Decrease the distance argument\n\"");
			return;
		}
	}

	// zyk: if there are still enough room to list, use old method to get some entities not listed with EntitiesInBox
	for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
	{
		int j = 0;
		qboolean already_found = qfalse;

		this_ent = &g_entities[i];

		for (j = 0; j < numListedEntities; j++)
		{
			if (entityList[j] == i)
			{ // zyk: this entity was already listed
				already_found = qtrue;
				break;
			}
		}

		if (this_ent && ent != this_ent && already_found == qfalse && this_ent->inuse == qtrue && (int)Distance(ent->client->ps.origin, this_ent->r.currentOrigin) < distance && this_ent->s.eType != ET_MOVER)
		{ // zyk: do not list mover entities in this old method, they are listed with EntitiesInBox
			strcpy(message, va("%s\n%d - %s", message, this_ent->s.number, this_ent->classname));
		}

		if (strlen(message) > (MAX_STRING_CHARS - 11))
		{
			trap->SendServerCommand(ent->s.number, "print \"Too much info. Decrease the distance argument\n\"");
			return;
		}
	}

	if (Q_stricmp(message, "") == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"No entities found\n\"");
		return;
	}

	trap->SendServerCommand( ent->s.number, va("print \"%s\n\"", message) );
}

/*
==================
Cmd_EntList_f
==================
*/
void Cmd_EntList_f( gentity_t *ent ) {
	int i = 0;
	int page_number = 0;
	gentity_t *target_ent;
	char arg1[MAX_STRING_CHARS];
	char message[1024];
	int len = 0;

	strcpy(message,"");

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( trap->Argc() < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a page number greater than 0. Example: ^3/entlist 5^7. You can also search for classname, targetname and target that matches at least part of it. Example: ^3/entlist info_player_deathmatch^7\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	page_number = atoi(arg1);

	if (page_number > 0)
	{
		for (i = 0; i < level.num_entities; i++)
		{
			if (i >= ((page_number - 1) * 10) && i < (page_number * 10))
			{ // zyk: this command lists 10 entities per page
				target_ent = &g_entities[i];
				// GalaxyRP fix: [Sprintf Overlap] was sprintf(message, "%s\n...", message, ...) --
				// message was both destination and a %s source argument, which is undefined behavior
				// (see the matching fix in Cmd_Settings_f above for the full explanation). Fixed the
				// same way: write to message+len and track the appended length in len.
				len += sprintf(message + len, "\n%d - %s - %s - %s", i, target_ent->classname, target_ent->targetname, target_ent->target);
			}
		}
	}
	else
	{ // zyk: search by classname, targetname or target
		int found_entities = 0;

		for (i = 0; i < level.num_entities; i++)
		{
			target_ent = &g_entities[i];

			if (target_ent && 
				((target_ent->classname && strstr(target_ent->classname, G_NewString(arg1))) ||
				 (target_ent->targetname && strstr(target_ent->targetname, G_NewString(arg1))) ||
				 (target_ent->target && strstr(target_ent->target, G_NewString(arg1)))))
			{
				// GalaxyRP fix: [Sprintf Overlap] same message-overlaps-itself issue as above, same fix.
				len += sprintf(message + len, "\n%d - %s - %s - %s", i, target_ent->classname, target_ent->targetname, target_ent->target);
				found_entities++;
			}

			// zyk: max entities to list
			if (found_entities == 14)
				break;
		}
	}

	trap->SendServerCommand( ent-g_entities, va("print \"^3\nID - classname - targetname - target\n^7%s\n\n\"",message) );
}

/*
==================
Cmd_EntRemove_f
==================
*/
void Cmd_EntRemove_f( gentity_t *ent ) {
	int i = 0;
	int entity_id = -1;
	int entity_id2 = -1;
	gentity_t *target_ent;
	char   arg1[MAX_STRING_CHARS];
	char   arg2[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( trap->Argc() < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify an entity id.\n\"") );
		return;
	}

	if (trap->Argc() == 2)
	{
		trap->Argv( 1, arg1, sizeof( arg1 ) );
		entity_id = atoi(arg1);

		if (entity_id >= 0 && entity_id < MAX_CLIENTS)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity ID %d is a player slot and cannot be removed.\n\"",entity_id) );
			return;
		}

		for (i = 0; i < level.num_entities; i++)
		{
			target_ent = &g_entities[i];
			if ((target_ent-g_entities) == entity_id)
			{
				G_FreeEntity( target_ent );
				trap->SendServerCommand( ent-g_entities, va("print \"Entity %d removed.\n\"",i) );
				return;
			}
		}

		trap->SendServerCommand( ent-g_entities, va("print \"Entity %d not found.\n\"",entity_id) );
	}
	else
	{
		trap->Argv( 1, arg1, sizeof( arg1 ) );
		entity_id = atoi(arg1);

		if (entity_id >= 0 && entity_id < MAX_CLIENTS)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity 1 ID %d is a player slot and cannot be removed.\n\"",entity_id) );
			return;
		}

		trap->Argv( 2, arg2, sizeof( arg2 ) );
		entity_id2 = atoi(arg2);

		if (entity_id2 >= 0 && entity_id2 < MAX_CLIENTS)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity 2 ID %d is a player slot and cannot be removed.\n\"",entity_id) );
			return;
		}

		for (i = 0; i < level.num_entities; i++)
		{
			target_ent = &g_entities[i];
			if ((target_ent-g_entities) >= entity_id && (target_ent-g_entities) <= entity_id2)
			{
				G_FreeEntity( target_ent );
			}
		}

		trap->SendServerCommand( ent-g_entities, "print \"Entities removed.\n\"" );
		return;
	}
}

void Cmd_SpawnPlatform_f(gentity_t* ent) 
{
	gentity_t* new_ent = NULL;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	new_ent = G_Spawn();

	if (new_ent)
	{
		zyk_main_set_entity_field(new_ent, "classname", "func_plat");
		zyk_main_set_entity_field(new_ent, "origin", G_NewString(va("%f %f %f", ent->client->ps.origin[0], ent->client->ps.origin[1], ent->client->ps.origin[2])));
		zyk_main_set_entity_field(new_ent, "angles", "0 0 0");
		zyk_main_set_entity_field(new_ent, "spawnflags", "1024");
		zyk_main_set_entity_field(new_ent, "mins", "-64 -64 -8");
		zyk_main_set_entity_field(new_ent, "maxs", "64 64 8");
		zyk_main_set_entity_field(new_ent, "model", "models/map_objects/factory/catw2_b.md3");

		zyk_main_spawn_entity(new_ent);

		if (new_ent->s.number != 0)
		{
			level.last_spawned_entity = new_ent;
		}
	}

}

void Cmd_SpawnDummy_f(gentity_t* ent)
{
	gentity_t* new_ent = NULL;

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand(ent - g_entities, "print \"You don't have this admin command.\n\"");
		return;
	}

	new_ent = G_Spawn();

	if (new_ent)
	{
		zyk_main_set_entity_field(new_ent, "classname", "zyk_training_pole");
		zyk_main_set_entity_field(new_ent, "origin", G_NewString(va("%f %f %f", ent->client->ps.origin[0], ent->client->ps.origin[1], ent->client->ps.origin[2])));
		zyk_main_set_entity_field(new_ent, "angles", "0 0 0");
		zyk_main_set_entity_field(new_ent, "spawnflags", "1");

		zyk_main_spawn_entity(new_ent);

		if (new_ent->s.number != 0)
		{
			level.last_spawned_entity = new_ent;
		}
	}

}

qboolean is_entity_a_pickup(gentity_t* ent) {
	char* output = NULL;

	// GalaxyRP (Alex): [Entity System] Remove all weapon pickups
	output = strstr(ent->classname, "weapon_");

	if (output) {
		return qtrue;
	}

	// GalaxyRP (Alex): [Entity System] Remove all ammo pickups
	output = strstr(ent->classname, "ammo_");

	if (output) {
		return qtrue;
	}

	// GalaxyRP (Alex): [Entity System] Remove all item pickups
	output = strstr(ent->classname, "item_");

	if (output) {
		return qtrue;
	}

	return qfalse;
}

void Cmd_RemovePickups_f(gentity_t* ent) {

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	gentity_t* target_ent;

	for (int i = 0; i < level.num_entities; i++)
	{
		target_ent = &g_entities[i];

		if (is_entity_a_pickup(target_ent) == qtrue) {
			G_FreeEntity(target_ent);
		}
	}
	trap->SendServerCommand(ent - g_entities, va("print \"All pickups have been removed.\n\""));
	
	return;
}

/*
==================
Cmd_ClientPrint_f
==================
*/
void Cmd_ClientPrint_f( gentity_t *ent ) {
	int client_id = -1;
	char   arg1[MAX_STRING_CHARS];
	char   arg2[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_CLIENTPRINT, qtrue))
	{
		return;
	}

	if ( trap->Argc() < 3)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Usage: /clientprint <player name or ID, or -1 to show to all players> <message>\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (atoi(arg1) != -1)
	{ // zyk: -1 means all players will get the message
		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if (client_id == -1)
		{
			return;
		}
	}

	trap->Argv( 2, arg2, sizeof( arg2 ) );

	trap->SendServerCommand( client_id, va("cp \"%s\"", arg2) );
	trap->SendServerCommand( client_id, va("print \"^3* %s ^3*\n\"", arg2) );
}

/*
==================
Cmd_Silence_f
==================
*/
void Cmd_Silence_f( gentity_t *ent ) {
	int client_id = -1;
	char   arg[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_SILENCE, qtrue))
	{
		return;
	}

	if ( trap->Argc() < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a player name or ID.\n\"") );
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );
	client_id = ClientNumberFromString( ent, arg, qfalse );

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->pers.player_statuses & (1 << 0))
	{
		g_entities[client_id].client->pers.player_statuses &= ~(1 << 0);
		trap->SendServerCommand( -1, va("chat \"^3Admin System: ^7player %s^7 is no longer silenced!\n\"", g_entities[client_id].client->pers.netname) );
	}
	else
	{
		g_entities[client_id].client->pers.player_statuses |= (1 << 0);
		trap->SendServerCommand( -1, va("chat \"^3Admin System: ^7player %s^7 is silenced!\n\"", g_entities[client_id].client->pers.netname) );
	}
}

// zyk: shows admin commands of this player. Shows info to the target_ent player
// GalaxyRP fix: [cleanup] renamed params to match their actual roles -- whose_commands is whose
// bitvalue is read to build the list, shown_to is who the resulting messages are sent to. The old
// names ("ent" / "target_ent") read backwards at the self-listing call site (where both happen to
// be the same entity) and made the "show" call site error-prone to reason about.
void zyk_show_admin_commands(gentity_t *whose_commands, gentity_t *shown_to)
{
	char message[1024];
	// GalaxyRP fix: [cleanup] removed an unused 'char message_content[ADM_NUM_CMDS + 1][80]' local
	// (never read anywhere in this function).
	strcpy(message,"");

	// GalaxyRP fix: [cleanup] removed a dead outer 'int i = 0;' that was immediately shadowed by
	// this loop's own 'int i' -- the outer one was never read.
	for (int i = 0; i < ADM_NUM_CMDS; i++) {
		if ((whose_commands->client->pers.bitvalue & (1 << admin_commands[i].number)))
		{
			trap->SendServerCommand(shown_to - g_entities, va("print \"^3%d ^7- %s: ^2yes\n\"", admin_commands[i].number, admin_commands[i].title));
		}
		else
		{
			trap->SendServerCommand(shown_to - g_entities, va("print \"^3%d ^7- %s: ^1no\n\"", admin_commands[i].number, admin_commands[i].title));
		}
	}

	return;
}

/*
==================
Cmd_AdminList_f
==================
*/
void Cmd_AdminList_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];

	if (trap->Argc() == 1)
	{
		zyk_show_admin_commands(ent, ent);
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	// GalaxyRP fix: [Admin] check for the "show" keyword before treating arg1 as a numeric command
	// ID -- previously /adminlist show (with no player name, 2 total args) fell into the numeric-
	// help branch below, atoi()'d "show" into 0, and that collided with the real ADM_NPC (0)
	// command number: the missing-argument mistake silently printed the NPC command's help text
	// instead of any error.
	if (Q_stricmp(arg1, "show") == 0)
	{
		gentity_t *player_ent = NULL;
		char arg2[MAX_STRING_CHARS];
		int client_id = -1;

		// GalaxyRP fix: [cleanup] route through the shared check_admin_command() helper instead of
		// a hand-rolled "pers.bitvalue & (1 << ADM_GIVEADM)" check, matching every other admin
		// command.
		if (!check_admin_command(ent, ADM_GIVEADM, qtrue))
		{
			return;
		}

		if (trap->Argc() < 3)
		{
			trap->SendServerCommand( ent-g_entities, "print \"You must specify a player name or ID. Usage: /adminlist show <player name or ID>\n\"" );
			return;
		}

		trap->Argv( 2, arg2, sizeof( arg2 ) );

		client_id = ClientNumberFromString( ent, arg2, qfalse );
		if (client_id == -1)
		{
			return;
		}

		player_ent = &g_entities[client_id];

		if (player_ent->client->sess.amrpgmode == 0)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Player %s ^7is not logged in.\n\"", player_ent->client->pers.netname) );
			return;
		}

		// zyk: player is logged in. Show his admin commands
		zyk_show_admin_commands(player_ent, ent);
		return;
	}

	// zyk: display help info for an admin command
	// GalaxyRP fix: [Admin] atoi() silently returns 0 for a non-numeric string, which collided with
	// the real ADM_NPC (0) command number -- require the argument to actually be an integer first,
	// same fix already applied to /admmap's gametype argument.
	if (!StringIsInteger(arg1))
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid admin command number. Use ^3/adminlist ^7to see your admin commands and their numbers.\n\"" );
		return;
	}

	{
		int command_number = atoi(arg1);

		if (command_number == ADM_NPC)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/npc spawn <name> ^7to spawn a npc. Use ^3/npc spawn vehicle <name> ^7to spawn a vehicle. Use ^3/npc kill all ^7to kill all npcs\n\n\"" );
		}
		else if (command_number == ADM_NOCLIP)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/noclip ^7to toggle noclip\n\n\"" );
		}
		else if (command_number == ADM_GIVEADM)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nThis flag allows admins to give or remove admin commands from a player with ^3/adminup <name> <command number> ^7and ^3/admindown <name> <command number>^7. Use ^3/adminlist show <player name or ID> ^7to see admin commands of a player\n\n\"" );
		}
		else if (command_number == ADM_TELE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nThis command can be ^3/teleport^7 or ^3/tele^7. Use ^3/telemark ^7to mark a spot in map, then use ^3/teleport ^7to go there. Use ^3/teleport <player name or ID> ^7to teleport to a player. \nUse ^3/teleport <player name or ID> <player name or ID> ^7to teleport a player to another. Use ^3/teleport <x> <y> <z> ^7to teleport to coordinates. Use ^3/teleport <player name or ID> <x> <y> <z> ^7to teleport a player to coordinates\n\n\"" );
		}
		else if (command_number == ADM_ADMPROTECT)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nWith this flag, a player can use Admin Protect option in ^3/settings ^7to protect himself from admin commands\n\n\"" );
		}
		else if (command_number == ADM_ENTITYSYSTEM)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/entitysystem ^7to see the Entity System commands enabled by this flag\n\n\"" );
		}
		else if (command_number == ADM_SILENCE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/silence <player name or ID> ^7to silence that player\n\n\"" );
		}
		else if (command_number == ADM_CLIENTPRINT)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/clientprint <player name or ID, or -1 for all players> <message> ^7to print a message in the screen\n\n\"" );
		}
		else if (command_number == ADM_SHAKESCREEN)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/shakescreen ^7to shake player's screen\n\n\"" );
		}
		else if (command_number == ADM_KICK)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/admkick <player name or ID> ^7to kick a player from the server, or ^3/killother <player name or ID> ^7to instantly kill a player. Both share this admin command\n\n\"" );
		}
		else if (command_number == ADM_PARALYZE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/paralyze <player name or ID> ^7to paralyze a player. Use it again so the target player will no longer be paralyzed\n\n\"" );
		}
		else if (command_number == ADM_GIVE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/give <player name or ID> <option> ^7to give guns or Force powers to a player who is not logged in. Option may be ^3guns ^7or ^3force ^7\n\n\"" );
		}
		else if (command_number == ADM_SCALE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/scale <player name or ID> <size between 20 and 500> ^7to change the player model size\n\n\"" );
		}
		else if (command_number == ADM_PLAYERS)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/players ^7to see info about the players. Use ^3/players <player name or ID> ^7to see RPG info of a player. Use ^3/players <player name or ID> ^7and a third argument (^3force,weapons,other,ammo,items^7) to see skill levels of the player\n\n\"" );
		}
		else if (command_number == ADM_DUELARENA)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/duelarena ^7to set or unset the Duel Tournament arena in this map. The arena is saved automatically. Also, use ^3/duelpause ^7to pause/resume the tournament\n\n\"");
		}
		else if (command_number == ADM_CHANGEMAP)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/admmap <gametype number> <map name> ^7to change the server to a different map and gametype\n\n\"");
		}
		else if (command_number == ADM_CREATEITEM)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/createitem <itemname> ^7to create an item with a given name. Items containing more than one word need double quotes around the argument\n\n\"");
		}
		else if (command_number == ADM_GOD)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/God ^7to make you invinsible\n\n\"");
		}
		else if (command_number == ADM_LEVELUP)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/levelup ^7or ^3/leveldown <player name> <number of levels (optional)> ^7to increase or decrease the level of a player respectively\n\n\"");
		}
		else if (command_number == ADM_SKILL)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/skillup ^7or ^3/skilldown <player name> <skill number> <number of levels (optional)> ^7to increase or decrease the skill levels of a player respectively\n\n\"");
		}
		else if (command_number == ADM_CREATECREDITS)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/createcredits <player name> <amount> ^7to create credits and place them to a player's account\n\n\"");
		}
		else if (command_number == ADM_IGNORECHATDISTANCE)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nWith this flag a player can see all chats on the server no matter the distance\n\n\"");
		}
		else if (command_number == ADM_XP)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/givexp ^7or ^3/removexp <player name> ^7to give or remove xp points respectively\n\n\"");
		}
		else if (command_number == ADM_UPDATENEWS)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/newsadd <channel> <text> ^7to add the news to a channel. The text has to be enclosed in double quotes for it to register properly\n\n\"");
		}
		else if (command_number == ADM_REMOVENEWS)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/newsremove <news ID> ^7to remove the news from a channel\n\n\"");
		}
		else if (command_number == ADM_MUSIC)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/playmusic <path> ^7to replace the current map music for all players with the song given\n\n\"");
		}
		else if (command_number == ADM_GETUP)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/getup ^7and ^3/helpup <player name> ^7to revive yourself and other players from downed state\n\n\"");
		}
		else
		{
			// GalaxyRP fix: [Admin] the numeric-help chain above had no fallback for an
			// out-of-range command number -- /adminlist 999 previously produced total silence.
			trap->SendServerCommand( ent-g_entities, "print \"Invalid admin command number. Use ^3/adminlist ^7to see your admin commands and their numbers.\n\"" );
		}
	}
}

/*
==================
Cmd_AdminUp_f
==================
*/
void Cmd_AdminUp_f( gentity_t *ent ) {
	char	arg1[MAX_STRING_CHARS];
	char	arg2[MAX_STRING_CHARS];
	int client_id = -1;
	int i = 0;
	int bitvaluecommand = 0;

	// GalaxyRP fix: [cleanup] route through the shared check_admin_command() helper instead of a
	// hand-rolled "pers.bitvalue & (1 << ADM_GIVEADM)" check, matching every other admin command.
	if (!check_admin_command(ent, ADM_GIVEADM, qtrue))
	{
		return;
	}

	if ( trap->Argc() != 3 )
	{
		trap->SendServerCommand( ent-g_entities, "print \"You must write the player name and the admin command number.\n\"" );
		return;
	}
	trap->Argv( 1,  arg1, sizeof( arg1 ) );
	trap->Argv( 2,  arg2, sizeof( arg2 ) );
	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode == 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Player is not logged in\n\"") );
		return;
	}
	if (Q_stricmp (arg2, "all") == 0)
	{ // zyk: if player wrote all, give all commands to the target player
		for (i = 0; i < ADM_NUM_CMDS; i++)
			g_entities[client_id].client->pers.bitvalue |= (1 << i);

		// GalaxyRP fix: [Admin] audit trail -- neither /adminup nor /admindown used to log who
		// granted/revoked what to whom, even though these two commands can hand out or take away
		// full server control. Log every grant/revoke to the server log.
		G_LogPrintf( "AdminUp: %s^7 granted all admin commands to %s^7\n", ent->client->pers.netname, g_entities[client_id].client->pers.netname );
		// GalaxyRP fix: [Admin] unique success message instead of the generic "Admin commands
		// upgraded successfully." that both /adminup and /admindown used to share verbatim.
		trap->SendServerCommand( ent-g_entities, va("print \"You granted all admin commands to %s^7.\n\"", g_entities[client_id].client->pers.netname) );
	}
	else
	{
		// GalaxyRP fix: [Admin] atoi() silently returns 0 for a non-numeric string (e.g. a typo
		// like "abc" would parse as command 0 / ADM_NPC instead of being rejected), so require the
		// argument to actually be an integer first -- same fix already applied to /admmap's
		// gametype argument.
		if (!StringIsInteger(arg2))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Invalid admin command\n\"") );
			return;
		}

		bitvaluecommand = atoi(arg2);
		if (bitvaluecommand < 0 || bitvaluecommand >= ADM_NUM_CMDS)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Invalid admin command\n\"") );
			return;
		}
		g_entities[client_id].client->pers.bitvalue |= (1 << bitvaluecommand);

		G_LogPrintf( "AdminUp: %s^7 granted %s ^7to %s^7\n", ent->client->pers.netname, admin_commands[bitvaluecommand].title, g_entities[client_id].client->pers.netname );
		trap->SendServerCommand( ent-g_entities, va("print \"You granted %s ^7to %s^7.\n\"", admin_commands[bitvaluecommand].title, g_entities[client_id].client->pers.netname) );
	}

	save_account(&g_entities[client_id], qfalse);
}

/*
==================
Cmd_AdminDown_f
==================
*/
void Cmd_AdminDown_f( gentity_t *ent ) {
	char	arg1[MAX_STRING_CHARS];
	char	arg2[MAX_STRING_CHARS];
	int client_id = -1;
	int bitvaluecommand = 0;

	// GalaxyRP fix: [cleanup] route through the shared check_admin_command() helper instead of a
	// hand-rolled "pers.bitvalue & (1 << ADM_GIVEADM)" check, matching every other admin command.
	if (!check_admin_command(ent, ADM_GIVEADM, qtrue))
	{
		return;
	}

	if ( trap->Argc() != 3 )
	{
		trap->SendServerCommand( ent-g_entities, "print \"You must write a player name and the admin command number.\n\"" );
		return;
	}
	trap->Argv( 1,  arg1, sizeof( arg1 ) );
	trap->Argv( 2,  arg2, sizeof( arg2 ) );
	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode == 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Player is not logged in\n\"") );
		return;
	}

	if (Q_stricmp (arg2, "all") == 0)
	{ // zyk: if player wrote all, take away all admin commands from target player
		g_entities[client_id].client->pers.bitvalue = 0;

		// GalaxyRP fix: [Admin] audit trail -- neither /adminup nor /admindown used to log who
		// granted/revoked what to whom, even though these two commands can hand out or take away
		// full server control. Log every grant/revoke to the server log.
		G_LogPrintf( "AdminDown: %s^7 revoked all admin commands from %s^7\n", ent->client->pers.netname, g_entities[client_id].client->pers.netname );
		// GalaxyRP fix: [Admin] unique success message instead of the generic (and, for this
		// command, actively wrong -- it said "upgraded" for a downgrade) message that both
		// /adminup and /admindown used to share verbatim.
		trap->SendServerCommand( ent-g_entities, va("print \"You revoked all admin commands from %s^7.\n\"", g_entities[client_id].client->pers.netname) );
	}
	else
	{
		// GalaxyRP fix: [Admin] atoi() silently returns 0 for a non-numeric string (e.g. a typo
		// like "abc" would parse as command 0 / ADM_NPC instead of being rejected), so require the
		// argument to actually be an integer first -- same fix already applied to /admmap's
		// gametype argument.
		if (!StringIsInteger(arg2))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Invalid admin command\n\"") );
			return;
		}

		bitvaluecommand = atoi(arg2);
		if (bitvaluecommand < 0 || bitvaluecommand >= ADM_NUM_CMDS)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Invalid admin command\n\"") );
			return;
		}
		g_entities[client_id].client->pers.bitvalue &= ~(1 << bitvaluecommand);

		G_LogPrintf( "AdminDown: %s^7 revoked %s ^7from %s^7\n", ent->client->pers.netname, admin_commands[bitvaluecommand].title, g_entities[client_id].client->pers.netname );
		trap->SendServerCommand( ent-g_entities, va("print \"You revoked %s ^7from %s^7.\n\"", admin_commands[bitvaluecommand].title, g_entities[client_id].client->pers.netname) );
	}

	save_account(&g_entities[client_id], qfalse);
}

void show_skill_change_message(gentity_t* ent, gentity_t* ent2, qboolean downgrade, qboolean success, int skill_id, int number_of_changes) {
	char success_message[256];

	if (downgrade == qtrue) {
		if (success == qtrue) {

			if (ent->client->ps.clientNum != ent2->client->ps.clientNum) {

				strcpy(success_message, "print \"^2You downgraded the target's ^3%s ^2skill by ^3%d ^2points. Current value: ^3%d^2.\n\"");
			}
			else {
				strcpy(success_message, "print \"^2You downgraded the ^3%s ^2skill by ^3%d ^2points. Current value: ^3%d^2.\n\"");
			}
		}
		else {
			if (ent->client->ps.clientNum != ent2->client->ps.clientNum) {

				strcpy(success_message, "print \"^1Target already reached the minimum level of ^3%s ^1skill. You can only downgrade by ^3%d ^1points. Nothing was updated. Current value: ^3%d^2.\n\"");
			}
			else {
				strcpy(success_message, "print \"^1You reached the minimum level of ^3%s ^1skill. You can only downgrade by ^3%d ^1points. Nothing was updated. Current value: ^3%d^2.\n\"");
			}
		}
	}
	else {
		if (success == qtrue) {

			if (ent->client->ps.clientNum != ent2->client->ps.clientNum) {

				strcpy(success_message, "print \"^2You upgraded the target's ^3%s ^2skill by ^3%d ^2points. Current value: ^3%d^2.\n\"");
			}
			else {
				strcpy(success_message, "print \"^2You upgraded the ^3%s ^2skill by ^3%d ^2points. Current value: ^3%d^2.\n\"");
			}


		}
		else {
			if (ent->client->ps.clientNum != ent2->client->ps.clientNum) {

				strcpy(success_message, "print \"^1Target already reached the maximum level of ^3%s ^1skill. You can only upgrade by ^3%d ^1points. Nothing was updated. Current value: ^3%d^2.\n\"");
			}
			else {
				strcpy(success_message, "print \"^1You already reached the maximum level of ^3%s ^1skill. You can only upgrade by ^3%d ^1points. Nothing was updated. Current value: ^3%d^2.\n\"");
			}
		}
	}

	trap->SendServerCommand(ent - g_entities, va(success_message, skills[skill_id].skill_name, number_of_changes, ent2->client->pers.skill_levels[skill_id]));
}

// GalaxyRP fix: [Skills] downgrading a weapon-granting skill (Saber Attack, or any of the mercenary
// weapon/ammo skills below) to 0 correctly cleared the weapon's STAT_WEAPONS bit, but if the player
// was actively holding that exact weapon at the time, nothing told them to put it away -- they kept
// using a weapon that no longer showed as owned until they happened to switch weapons themselves.
// Mirrors the same weapon-removed-while-held handling TossWeapon() (g_combat.c) already does: pick
// another owned weapon (or none) and switch to it immediately, then fire EV_NOAMMO so the client's
// HUD and weapon model update right away instead of silently keeping the old one equipped.
void zyk_deselect_weapon_if_active(gentity_t* ent, int weapon) {
	int new_weapon = WP_NONE;
	int i;

	if (ent->client->ps.weapon != weapon) {
		return;
	}

	for (i = 0; i < WP_NUM_WEAPONS; i++) {
		if ((ent->client->ps.stats[STAT_WEAPONS] & (1 << i)) && i != WP_NONE) {
			new_weapon = i;
			break;
		}
	}

	ent->s.weapon = new_weapon;
	ent->client->ps.weapon = new_weapon;

	G_AddEvent(ent, EV_NOAMMO, weapon);
}

void apply_skill_change_in_game(gentity_t* ent, int skill_id, qboolean upgrade) {
	switch (skill_id) {
	case 30:
		//GalaxyRP (Alex): [Skill] Reset max shield immediately.
		set_max_shield(ent);
		// GalaxyRP fix: [Skills] set_max_shield() only recalculates pers.max_rpg_shield -- it never
		// touches the player's current shield. Downgrading Max Shield could leave ps.stats[STAT_ARMOR]
		// sitting above the new, lower cap, where it would stay until spent in combat (every shield-gain
		// path already clamps against max_rpg_shield before adding, but nothing clamped an already-too-
		// high value down). Clamp it here too so a downgrade takes full effect immediately, not just for
		// future gains.
		if (ent->client->ps.stats[STAT_ARMOR] > ent->client->pers.max_rpg_shield) {
			ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.max_rpg_shield;
		}
		break;
	case 54:
		//GalaxyRP (Alex): [Skill] Reset max force power immediately.
		ent->client->pers.max_force_power = (int)ceil((zyk_max_force_power.value / 4.0) * ent->client->pers.skill_levels[skill_id]);
		ent->client->ps.fd.forcePowerMax = ent->client->pers.max_force_power;
		// GalaxyRP fix: [Skills] same gap as Max Shield above -- downgrading Force Power lowers
		// forcePowerMax immediately but never clamped the player's current forcePower down to match,
		// so they could keep a force pool above their new cap until it was spent. Clamped here for the
		// same reason.
		if (ent->client->ps.fd.forcePower > ent->client->ps.fd.forcePowerMax) {
			ent->client->ps.fd.forcePower = ent->client->ps.fd.forcePowerMax;
		}
		break;
	case 34:
		// GalaxyRP fix: [Skills] the client-side blue/yellow jetpack flame effect (cg_players.c's
		// jetpack FX code) reads a cached per-player flag that's normally set by a one-time-per-life
		// event push in g_active.c's ClientThink sync cascade (gated by player_statuses bit 3) -- so
		// upgrading or downgrading Jetpack to/from level 3 while already alive didn't take effect
		// until the next respawn re-ran that cascade. Send the corrected event immediately here too,
		// mirroring that cascade's own condition exactly, so the effect updates live instead of only
		// on next respawn. This doesn't touch player_statuses bit 3, so the lazy cascade is left free
		// to also (harmlessly) resend the same thing later.
		if (ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[34] == 3)
			G_AddEvent(ent, EV_ITEMUSEFAIL, 7);
		else
			G_AddEvent(ent, EV_ITEMUSEFAIL, 8);
		break;
	default:
		//GalaxyRP (Alex): [Skill] Do nothing for standard skills.
		break;
	}

	//GalaxyRP (Alex): [Skill] Give them the Force Ability.
	// GalaxyRP fix: [Skills] this used to gate on "value_internal != 0" alone, which silently skipped
	// Heal (skill_id 9): its value_internal is FP_HEAL, and FP_HEAL == 0 -- the very same 0 this array
	// also uses as a sentinel for "no linked force power" on entries like Sense Health (skill_id 33,
	// deliberately not a selectable/togglable force power). That collision meant a live /skillup or
	// /skilldown on Heal never touched forcePowerLevel[FP_HEAL] or forcePowersKnown at all -- the
	// change only became visible on the player's next respawn, when initialize_rpg_skills() (which has
	// no such guard) reloaded every force power from pers.skill_levels[] from scratch. Explicitly
	// admitting skill_id == 9 here fixes Heal in place, the same way skill_id == 5 is already
	// special-cased below for the saber weapon bit.
	if (strcmp(skills[skill_id].category,"force") == 0 && (skills[skill_id].value_internal != 0 || skill_id == 9)) {
		// GalaxyRP fix: Absorb, Protect and Lightning are capped at ps.fd.forcePowerLevel ==
		// FORCE_LEVEL_3 in the DB-load path below (see the "loading Absorb/Protect/Lightning
		// value" blocks a bit further down in this file) -- their levels 4 and 5 are meant to be
		// applied purely as bonus effects keyed off pers.skill_levels[] directly (see e.g. the
		// "Lightning level 4/5" damage bonus in ForceLightningDamage()), never by actually raising
		// forcePowerLevel past 3. That's because the client renders Force Lightning's FX based on
		// ps.activeForcePass, which is set to forcePowerLevel[FP_LIGHTNING] every time Lightning
		// is activated (see WP_ForcePowerStart) -- and cg_players.c treats any activeForcePass
		// above FORCE_LEVEL_3 as a *Drain* effect, not Lightning (this is the same encoding
		// vanilla JKA uses for NPC dark side attacks). This code path (the immediate, in-place
		// effect of a /skillup or /skilldown) didn't apply that cap, so upgrading Lightning to
		// level 4 or 5 set forcePowerLevel[FP_LIGHTNING] to 4/5 unclamped, and the very next use
		// of Force Lightning rendered as Force Drain until the next respawn re-ran the (correctly
		// clamped) DB-load path and put it back to 3. Applying the same cap here keeps this path
		// consistent with the DB-load path so the bug can't resurface after a skill change.
		if ((skills[skill_id].value_internal == FP_ABSORB || skills[skill_id].value_internal == FP_PROTECT || skills[skill_id].value_internal == FP_LIGHTNING)
			&& ent->client->pers.skill_levels[skill_id] >= 4) {
			ent->client->ps.fd.forcePowerLevel[skills[skill_id].value_internal] = FORCE_LEVEL_3;
		}
		else {
			ent->client->ps.fd.forcePowerLevel[skills[skill_id].value_internal] = ent->client->pers.skill_levels[skill_id];
		}

		if (upgrade) {
			if (!(ent->client->ps.fd.forcePowersKnown & (1 << skills[skill_id].value_internal))) {
				ent->client->ps.fd.forcePowersKnown |= (1 << skills[skill_id].value_internal);
			}
		}
		else {
			if (ent->client->ps.fd.forcePowerLevel[skills[skill_id].value_internal] == 0)
			{
				ent->client->ps.fd.forcePowersKnown &= ~(1 << skills[skill_id].value_internal);
			}
		}
	}

	//GalaxyRP (Alex): [Skill] Give them the weapon.
	if ((strcmp(skills[skill_id].category, "weapons") == 0 || strcmp(skills[skill_id].category, "ammo") == 0) && skills[skill_id].value_internal != 0) {
		if (upgrade) {
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << skills[skill_id].value_internal);
		}
		else {
			if (ent->client->pers.skill_levels[skill_id] == 0) {
				ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << skills[skill_id].value_internal);
				// GalaxyRP fix: [Skills] see zyk_deselect_weapon_if_active() above -- put the weapon
				// away immediately if the player was actively holding it when its skill hit 0.
				zyk_deselect_weapon_if_active(ent, skills[skill_id].value_internal);
			}
		}
	}
	
	//GalaxyRP (Alex): [Skill] Give or take away a lightsaber if they got the skill for it.
	if (skill_id == 5) {
		if (upgrade) {
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);
		}
		else {
			if (ent->client->pers.skill_levels[skill_id] == 0) {
				ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
				// GalaxyRP fix: [Skills] see zyk_deselect_weapon_if_active() above -- downgrading Saber
				// Attack to 0 correctly removed the saber from STAT_WEAPONS, but a player who was
				// actively holding it kept swinging it (it no longer showed as owned, but nothing put
				// it away) until they manually switched weapons themselves. Put it away immediately.
				zyk_deselect_weapon_if_active(ent, WP_SABER);
			}
		}
	}

	//GalaxyRP (Alex): [Skill] Give them the item.
	if (strcmp(skills[skill_id].category, "items") == 0 && skills[skill_id].value_internal != 0) {
		if (upgrade) {
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << skills[skill_id].value_internal);
		}
		else {
			if (ent->client->pers.skill_levels[skill_id] == 0) {
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << skills[skill_id].value_internal);
			}
		}
	}
}

qboolean do_upgrade_skill(gentity_t* upgrader, gentity_t* upgradee, int skill_id, qboolean dont_show_message, int number_of_upgrades)
{
	if (skill_id < 0 || skill_id >= NUM_OF_SKILLS)
	{
		trap->SendServerCommand(upgrader - g_entities, "print \"Invalid skill number.\n\"");
		return qfalse;
	}

	// GalaxyRP fix: [Skills] skill_id 38-42 (Unique Skill/Blaster Pack/Power Cell/Metal Bolts/Rockets)
	// are kept as reserved, unused entries in the skills[] table below so every skill index at 43+
	// doesn't shift. 39-42 never had any gameplay effect coded anywhere (unlike the sibling ammo skills
	// at 43-45, which do grant their weapon). 38's own ability (g_active.c's GENCMD_ENGAGE_DUEL heal)
	// has been removed outright, the last remnant of the /unique command's now fully-removed Unique
	// Abilities. All five are blocked here instead of being left as a leveling slot that does nothing.
	// skill_id 55 (Improvements) joins them here for the same reason: its own gameplay hooks (a global
	// damage bonus in g_combat.c, a Magic Sense duration bonus and the unlock gates on 3 Magic powers
	// in g_main.c, and a Team Energize ammo-regen bonus in w_force.c) have all been removed outright as
	// loose ends from the same removed-features cleanup, rather than left half-working.
	if ((skill_id >= 38 && skill_id <= 42) || skill_id == 55)
	{
		trap->SendServerCommand(upgrader - g_entities, "print \"This skill is no longer in use.\n\"");
		return qfalse;
	}

	int number_of_possible_upgrades = skills[skill_id].max_level - upgradee->client->pers.skill_levels[skill_id];
	if (number_of_possible_upgrades < number_of_upgrades) {
		number_of_upgrades = number_of_possible_upgrades;
	}

	if (number_of_possible_upgrades == 0) {
		show_skill_change_message(upgrader, upgradee, qfalse, qfalse, skill_id, number_of_upgrades);

		return qfalse;
	}

	if (upgradee->client->pers.skillpoints < number_of_upgrades)
	{
		if (dont_show_message == qfalse) {
			trap->SendServerCommand(upgradee - g_entities, "print \"^1You don't have enough skillpoints.\n\"");
			if (upgrader->client->ps.clientNum != upgradee->client->ps.clientNum) {
				trap->SendServerCommand(upgrader - g_entities, "print \"^1Target player doesn't have enough skillpoints.\n\"");
			}
		}
		return qfalse;
	}

	for (int i = 0; i < number_of_upgrades; i++) {
		upgradee->client->pers.skill_levels[skill_id]++;
		upgradee->client->pers.skillpoints--;
	}

	apply_skill_change_in_game(upgradee, skill_id, qtrue);
	show_skill_change_message(upgrader, upgradee, qfalse, qtrue, skill_id, number_of_upgrades);

	return qtrue;
}

qboolean do_downgrade_skill(gentity_t* downgrader, gentity_t* downgradee, int skill_id, int number_of_downgrades)
{
	qboolean dont_show_message = qfalse;

	// zyk: validation on the upgrade level, which must be in the range of valid skills.
	if (skill_id < 0 || skill_id >= NUM_OF_SKILLS)
	{
		trap->SendServerCommand(downgrader - g_entities, "print \"Invalid skill number.\n\"");
		return qfalse;
	}

	// GalaxyRP fix: [Skills] see the matching fix comment in do_upgrade_skill() above -- skill_id 38-42
	// and 55 are reserved/unused and blocked from being touched by either command.
	if ((skill_id >= 38 && skill_id <= 42) || skill_id == 55)
	{
		trap->SendServerCommand(downgrader - g_entities, "print \"This skill is no longer in use.\n\"");
		return qfalse;
	}

	int number_of_possible_downgrades = downgradee->client->pers.skill_levels[skill_id];

	if (number_of_possible_downgrades < number_of_downgrades) {
		number_of_downgrades = number_of_possible_downgrades;
	}

	if (number_of_possible_downgrades == 0) {
		show_skill_change_message(downgrader, downgradee, qtrue, qfalse, skill_id, number_of_downgrades);

		return qfalse;
	}

	for (int i = 0; i < number_of_downgrades; i++) {
		downgradee->client->pers.skill_levels[skill_id]--;
		downgradee->client->pers.skillpoints++;
	}

	apply_skill_change_in_game(downgradee, skill_id, qfalse);
	show_skill_change_message(downgrader, downgradee, qtrue, qtrue, skill_id, number_of_downgrades);

	return qtrue;
}

/*
==================
Cmd_RpModeUp_f
==================
*/
void Cmd_RpModeUp_f( gentity_t *ent ) {
	char	arg1[MAX_STRING_CHARS];
	char	arg2[MAX_STRING_CHARS];
	char	arg3[MAX_STRING_CHARS];
	int client_id = -1;
	int number_of_upgrades = 1;

	if (!check_admin_command(ent, ADM_SKILL, qtrue))
	{
		return;
	}

	if ( trap->Argc() < 3 || trap->Argc() > 4)
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"You must write a player name or ID and the skill number.\n\"" ); 
		return; 
	}

	trap->Argv( 1,  arg1, sizeof( arg1 ) );
	trap->Argv( 2,  arg2, sizeof( arg2 ) );
	trap->Argv( 3,  arg3, sizeof( arg3 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (trap->Argc() == 4) {
		number_of_upgrades = atoi(arg3);

		// GalaxyRP fix: [Skills] this optional count argument was never validated -- a zero or negative
		// value still made it all the way to do_upgrade_skill(), whose internal for-loop just silently
		// never executed (no skill_levels/skillpoints change), yet the function still returned qtrue and
		// printed a "You upgraded the X skill by [0 or negative] points" success message despite nothing
		// having actually changed. Rejected outright here instead.
		if (number_of_upgrades <= 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid number of upgrades. Must be a positive number.\n\"" );
			return;
		}
	}

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode != 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Player is not in RPG Mode\n\"") );
		return;
	}

	qboolean is_upgraded = qfalse;

	// zyk: the upgrade is done if it doesnt go above the maximum level of the skill
	is_upgraded = do_upgrade_skill(ent, &g_entities[client_id], atoi(arg2) - 1, qfalse, number_of_upgrades);

	if (is_upgraded == qtrue) {
		// GalaxyRP (Alex): [Database] Only update the skills table. Also update the characters table to save the skill point
		update_skills_table_row_with_current_values(&g_entities[client_id]);
	}
}

/*
==================
Cmd_RpModeDown_f
==================
*/
void Cmd_RpModeDown_f( gentity_t *ent ) {
	char	arg1[MAX_STRING_CHARS];
	char	arg2[MAX_STRING_CHARS];
	char	arg3[MAX_STRING_CHARS];
	int client_id = -1;
	int number_of_downgrades = 1;

	if (!check_admin_command(ent, ADM_SKILL, qtrue))
	{
		return;
	}

	if (trap->Argc() < 3 || trap->Argc() > 4)
	{
		trap->SendServerCommand(ent - g_entities, "print \"You must write a player name or ID and the skill number.\n\"");
		return;
	}

	trap->Argv( 1,  arg1, sizeof( arg1 ) );
	trap->Argv( 2,  arg2, sizeof( arg2 ) );
	trap->Argv( 3,  arg3, sizeof( arg3 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (trap->Argc() == 4) {
		number_of_downgrades = atoi(arg3);

		// GalaxyRP fix: [Skills] see the matching fix comment in Cmd_RpModeUp_f above -- this optional
		// count argument was never validated, letting a zero or negative value through to
		// do_downgrade_skill() with the same silent-no-op-but-still-"success" outcome. Rejected here.
		if (number_of_downgrades <= 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid number of downgrades. Must be a positive number.\n\"" );
			return;
		}
	}

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode != 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Player is not in RPG Mode\n\"") );
		return;
	}

	qboolean is_upgraded = qfalse;

	// zyk: the upgrade is done if it doesnt go above the maximum level of the skill
	is_upgraded = do_downgrade_skill(ent, &g_entities[client_id], atoi(arg2) - 1, number_of_downgrades);

	if (is_upgraded == qtrue) {
		// GalaxyRP (Alex): [Database] Only update the skills table.
		update_skills_table_row_with_current_values(&g_entities[client_id]);
	}
}

int calculate_skillpoints_for_level(int level) {
	int skillpoints = 0;

	if (level % 10 == 0) // zyk: every level divisible by 10 the player will get bonus skillpoints
		skillpoints += (level / 10) + 1;
	else
		skillpoints++;

	return skillpoints;
}

// zyk: gives rpg score to the player
void increase_level(gentity_t* ent, qboolean admin_rp_mode, int number_of_levels)
{
	int send_message = 0; // zyk: if its 1, sends the message in player console
	char message[128];

	strcpy(message, "");

	// GalaxyRP fix: [Cvars] this used to also check "admin_rp_mode == qfalse && zyk_rp_mode.integer == 1"
	// here, but every call site in the codebase always passes admin_rp_mode = qtrue, so that check could
	// never actually trigger even before zyk_rp_mode was removed -- it was already dead code.

	for (int i = 1; i <= number_of_levels; i++) {
		if (ent->client->pers.level < zyk_rpg_max_level.integer)
		{
			ent->client->pers.level++;

			ent->client->pers.skillpoints += calculate_skillpoints_for_level(ent->client->pers.level);

			strcpy(message, va("^3New Level: ^7%d^3, Skillpoints: ^7%d\n", ent->client->pers.level, ent->client->pers.skillpoints));

			// zyk: got a new level, so change the max health and max shield
			set_max_health(ent);
			set_max_shield(ent);

			send_message = 1;

		}
	}

	if (ent->client->pers.level == zyk_rpg_max_level.integer) {
		trap->SendServerCommand(ent - g_entities, va("chat \"^3You have reached maximum level!\n\""));
	}

	trap->SendServerCommand(ent - g_entities, va("chat \"^3New Level: ^7%d^3, Skillpoints: ^7%d\n\"", ent->client->pers.level, ent->client->pers.skillpoints));
}

/*
==================
Cmd_LevelGive_f
==================
*/
void Cmd_LevelGive_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int client_id = -1;
	int number_of_levels = 1;

	if (!check_admin_command(ent, ADM_LEVELUP, qtrue))
	{
		return;
	}
   
	if ( trap->Argc() < 2 || trap->Argc() > 3)
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"^1Usage: ^2/levelup <player> <no of levels>(optional)\n\"" ); 
		return;
	}
	else {
		if (trap->Argc() == 3) {
			trap->Argv(2, arg2, sizeof(arg2));

			number_of_levels = atoi(arg2);

			// GalaxyRP fix: [Levelling] see the matching fix in Cmd_RpModeUp_f above -- this optional
			// count argument was never validated. A zero or negative value let increase_level()'s
			// internal loop silently do nothing while the command still printed a "leveled up" success
			// message (and, before the fix below, still wrote the wrong player's row to the database).
			// Rejected outright here instead.
			if (number_of_levels <= 0)
			{
				trap->SendServerCommand( ent-g_entities, "print \"Invalid number of levels. Must be a positive number.\n\"" );
				return;
			}
		}
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	// GalaxyRP fix: [Cvars] this used to also require zyk_rp_mode.integer == 1 here ("The server is not
	// at RP Mode"), but the server was always meant to be considered in RP Mode (that cvar's default),
	// and it has been removed -- so leveling up is unconditionally allowed to admins with ADM_LEVELUP now.

	if (g_entities[client_id].client->pers.level + number_of_levels > zyk_rpg_max_level.integer) {
		int max_possible_value = zyk_rpg_max_level.integer - g_entities[client_id].client->pers.level;
		trap->SendServerCommand(ent - g_entities, va("print \"^1Too many levels selected, operation not done. Maximum allowed: %d\n\"", max_possible_value));
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode == 2)
	{
		g_entities[client_id].client->pers.score_modifier = g_entities[client_id].client->pers.level;
		g_entities[client_id].client->pers.credits_modifier = -10;
		increase_level(&g_entities[client_id], qtrue, number_of_levels);

		trap->SendServerCommand(ent - g_entities, va("print \"^2Target player leveled up. Their current level is: ^3%i^2. Their skillpoint count is: ^3%i^2.\n\"", g_entities[client_id].client->pers.level, g_entities[client_id].client->pers.skillpoints));

		// GalaxyRP fix: [Database] this used to pass ent (the admin issuing the command) instead of the
		// target -- update_chars_table_row_with_current_values() writes whichever entity it's given, so
		// the target's new level/skillpoints were applied live in memory but never actually persisted;
		// only the admin's own (unchanged) row got redundantly rewritten. If the target disconnected
		// before some unrelated save happened to touch their row, the level-up was silently lost. Fixed
		// to save the actual target, matching Cmd_GiveXp_f/Cmd_RemoveXp_f below.
		update_chars_table_row_with_current_values(&g_entities[client_id]);

		return;
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"^1The player must be logged in!\n\"") );
	}
}

// GalaxyRP (Alex): [Levelling] Check to see if there's enough free skillpoints to level down. (prevents skillpoints going negative).
// GalaxyRP fix: [cleanup] target used to be passed by value (a whole gentity_t copied onto the stack
// on every /leveldown call, just to read a couple of fields through its client pointer). Passed by
// pointer instead -- same data, no unnecessary copy.
qboolean check_if_player_can_level_down(gentity_t* ent, gentity_t* target, int number_of_levels) {
	int skillpoints_needed = 0;
	int current_level = target->client->pers.level;

	int level_at_end = current_level - number_of_levels;

	for(int i = current_level; i > level_at_end; i--) {
		if (current_level > 1)
		{
			skillpoints_needed += calculate_skillpoints_for_level(current_level);
			current_level--;
		}
	}

	if (target->client->pers.skillpoints < skillpoints_needed) {
		trap->SendServerCommand(ent - g_entities, va("print \"^1Operation could not be done. Player needs %d skillpoints, but only has %d available.\n\"", skillpoints_needed, target->client->pers.skillpoints));

		return qfalse;
	}

	return qtrue;
}

void decrease_level(gentity_t* ent, qboolean admin_rp_mode, int number_of_levels)
{
	int send_message = 0; // zyk: if its 1, sends the message in player console
	char message[128];

	strcpy(message, "");

	// GalaxyRP fix: [Cvars] this used to also check "admin_rp_mode == qfalse && zyk_rp_mode.integer == 1"
	// here, but every call site in the codebase always passes admin_rp_mode = qtrue, so that check could
	// never actually trigger even before zyk_rp_mode was removed -- it was already dead code.

	int final_level = ent->client->pers.level - number_of_levels;

	for (int i = ent->client->pers.level; i > final_level; i--) {
		if (ent->client->pers.level > 1)
		{
			if (ent->client->pers.level % 10 == 0) // zyk: every level divisible by 10 the player will get bonus skillpoints
				ent->client->pers.skillpoints -= (ent->client->pers.level / 10) + 1;
			else
				ent->client->pers.skillpoints--;

			ent->client->pers.level--;

			strcpy(message, va("^3New Level: ^7%d^3, Skillpoints: ^7%d\n", ent->client->pers.level, ent->client->pers.skillpoints));

			// zyk: got a new level, so change the max health and max shield
			set_max_health(ent);
			set_max_shield(ent);

			send_message = 1;

		}
	}

	if (ent->client->pers.level == zyk_rpg_max_level.integer) {
		trap->SendServerCommand(ent - g_entities, va("chat \"^3You have reached maximum level!\n\""));
	}

	if (ent->client->pers.level == 1) {
		trap->SendServerCommand(ent - g_entities, va("chat \"^3You have reached minimum level!\n\""));
	}

	trap->SendServerCommand(ent - g_entities, va("chat \"^3New Level: ^7%d^3, Skillpoints: ^7%d\n\"", ent->client->pers.level, ent->client->pers.skillpoints));
}

/*
==================
Cmd_LevelTake_f
==================
*/
void Cmd_LevelTake_f(gentity_t* ent) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int client_id = -1;
	int number_of_levels = 1;

	if (!check_admin_command(ent, ADM_LEVELUP, qtrue))
	{
		return;
	}

	if (trap->Argc() < 2 || trap->Argc() > 3)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1Usage: ^2/leveldown <player> <no of levels>(optional)\n\"");
		return;
	}
	else {
		if (trap->Argc() == 3) {
			trap->Argv(2, arg2, sizeof(arg2));

			number_of_levels = atoi(arg2);

			// GalaxyRP fix: [Levelling] see the matching fix in Cmd_LevelGive_f above -- this optional
			// count argument was never validated. Rejected outright here instead.
			if (number_of_levels <= 0)
			{
				trap->SendServerCommand( ent-g_entities, "print \"Invalid number of levels. Must be a positive number.\n\"" );
				return;
			}
		}
	}

	trap->Argv(1, arg1, sizeof(arg1));

	client_id = ClientNumberFromString(ent, arg1, qfalse);

	if (client_id == -1)
	{
		return;
	}

	// GalaxyRP fix: [Cvars] this used to also require zyk_rp_mode.integer == 1 here ("The server is not
	// at RP Mode"), but the server was always meant to be considered in RP Mode (that cvar's default),
	// and it has been removed -- so leveling down is unconditionally allowed to admins with ADM_LEVELUP now.

	if (g_entities[client_id].client->pers.level - number_of_levels < 1) {
		int min_possible_value = g_entities[client_id].client->pers.level - 1;
		trap->SendServerCommand(ent - g_entities, va("print \"^1Too many levels selected, operation not done. Cannot lower someone's level below 1. Maximum allowed: %d\n\"", min_possible_value));
		return;
	}
	if (check_if_player_can_level_down(ent, &g_entities[client_id], number_of_levels) == qfalse) {
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode == 2)
	{
		g_entities[client_id].client->pers.score_modifier = g_entities[client_id].client->pers.level;
		g_entities[client_id].client->pers.credits_modifier = -10;
		decrease_level(&g_entities[client_id], qtrue, number_of_levels);

		trap->SendServerCommand(ent - g_entities, va("print \"^2Target player leveled down. Their current level is: ^3%i^2. Their skillpoint count is: ^3%i^2.\n\"", g_entities[client_id].client->pers.level, g_entities[client_id].client->pers.skillpoints));

		// GalaxyRP fix: [Database] see the matching fix in Cmd_LevelGive_f above -- this used to pass
		// ent (the admin) instead of the target, so the level-down was applied live but never actually
		// persisted. Fixed to save the actual target.
		update_chars_table_row_with_current_values(&g_entities[client_id]);

		return;
	}
	else
	{
		trap->SendServerCommand(ent - g_entities, va("print \"^1The player must be logged in!\n\""));
	}
}

// GalaxyRP (Alex): [XP System] This method returns the amount of XP needed to get to the next level, based on a given level.
int check_xp(int currentLevel) {
	int expNeeded = (currentLevel / 10) + 2;
	return expNeeded;
}

/*
==================
Cmd_GiveXp_f
==================
*/
void Cmd_GiveXp_f(gentity_t* ent) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;

	if (!check_admin_command(ent, ADM_XP, qtrue))
	{
		return;
	}

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"You must specify the player name or ID.\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	client_id = ClientNumberFromString(ent, arg1, qfalse);

	if (client_id == -1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Player not found on server.\n\"");
		return;
	}

	g_entities[client_id].client->pers.xp++;

	trap->SendServerCommand(&g_entities[client_id] - g_entities, va("chat \"^2You were given XP! Your current XP is: ^3%i^2/^3%i^2\n\"", g_entities[client_id].client->pers.xp, check_xp(g_entities[client_id].client->pers.level)));
	trap->SendServerCommand(ent - g_entities, va("print \"^2Target player was given XP. Their current XP is: ^3%i^2/^3%i^2\n\"", g_entities[client_id].client->pers.xp, check_xp(g_entities[client_id].client->pers.level)));

	// GalaxyRP (Alex): [XP System] If player has reached max xp for this level, level them up.
	if (g_entities[client_id].client->pers.xp == check_xp(g_entities[client_id].client->pers.level)) {

		increase_level(&g_entities[client_id], qtrue, 1);
		trap->SendServerCommand(ent - g_entities, va("print \"^2Target player leveled up. Their current level is: ^3%i^2. Their skillpoint count is: ^3%i^2.\n\"", g_entities[client_id].client->pers.level, g_entities[client_id].client->pers.skillpoints));
		// GalaxyRP (Alex): [XP System] Player levelled up, so their xp is now 0.
		g_entities[client_id].client->pers.xp = 0;
	}

	update_chars_table_row_with_current_values(&g_entities[client_id]);

	return;
}

/*
==================
Cmd_RemoveXp_f
==================
*/
void Cmd_RemoveXp_f(gentity_t* ent) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;

	if (!check_admin_command(ent, ADM_XP, qtrue))
	{
		return;
	}

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"You must specify the player name or ID.\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	client_id = ClientNumberFromString(ent, arg1, qfalse);

	if (client_id == -1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Player not found on server.\n\"");
		return;
	}

	if(g_entities[client_id].client->pers.xp == 0) {
		trap->SendServerCommand(ent - g_entities, va("print \"^2Target player's XP is already at 0. Nothing was done.\n\""));
		return;
	}

	g_entities[client_id].client->pers.xp--;

	trap->SendServerCommand(ent - g_entities, va("print \"^2Target player's XP was reduced with one point. Their current XP is: ^3%i^2/^3%i^2\n\"", g_entities[client_id].client->pers.xp, check_xp(g_entities[client_id].client->pers.level)));

	update_chars_table_row_with_current_values(&g_entities[client_id]);

	return;
}

/*
==================
Cmd_EntitySystem_f
==================
*/
void Cmd_EntitySystem_f( gentity_t *ent ) {
	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	trap->SendServerCommand( ent-g_entities, "print \"\n^3--------Entity System--------\n\
^3/entadd <classname> <key> <value> <key> <value>...: ^7Adds a new entity to the map.\n\
^3/entedit <entity id> <key> <value> <key> <value>...: ^7Edits entity fields or shows entity info if no key/value arguments were specified.\n\
^3/entnear <distance>: ^7Lists entities in less than 200 map units or distance passed as argument.\n\
^3/entlist <page number>: ^7Lists all entities present on the map.\n\
^3/entorigin: ^7Sets your position as origin for new entities. Use again to unset.\n\
^3/entundo: ^7Removes last added entity. Only works once.\n\
^3/entsave <filename>: ^7Saves current entities into a preset file. Use ^3default ^7name to make it load with the map.\n\
^3/entload <filename>: ^7Loads entities from a preset file.\n\
^3/entremove <entity id>: ^7Removes the entity from the map.\n\"");
	trap->SendServerCommand( ent-g_entities, "print \"^3/entdeletefile <filename>: ^7Deletes entity preset file.\n\
^3/remap <shader> <new shader>: ^7Remaps shader in the map.\n\
^3/remaplist: ^7Lists already remapped shaders in the map.\n\
^3/remapsave <file name>: ^7Saves current remaps in a preset file. Use ^3default ^7name to make it load with the map.\n\
^3/remapload <file name>: ^7Loads remaps from preset file.\n\
^3/remapdeletefile <file name>: ^7Deletes remap preset file.\n\
^3/removepickups: ^7Removes all pickups from the current map (ammo, health, shield, and weapons).\n\
^3/shaderlist: ^7Lists all map shaders.\n\
^3/spawnplatform: ^7Spawns a platform where the player is.\n\
^3/spawndummy: ^7Spawns a dummy where the player is.\n\n\" " );
}

/*
==================
Cmd_AdmKick_f
==================
*/
void Cmd_AdmKick_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;

	if (!check_admin_command(ent, ADM_KICK, qtrue))
	{
		return;
	}
   
	if ( trap->Argc() != 2) 
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"You must specify the player name or ID.\n\"" ); 
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	client_id = ClientNumberFromString( ent, arg1, qtrue );

	if (client_id == -1)
	{
		return;
	}

	trap->SendConsoleCommand( EXEC_APPEND, va( "kick %d\n", client_id) );
}

/*
==================
Cmd_AdmMap_f

GalaxyRP fix: [Admin] new admin command, adapted from JAPro's "ammap" command (bundled with the
TaystJK engine this mod now targets -- see https://github.com/taysta/TaystJK). Renamed "ammap" to
"admmap" to match this mod's existing "adm*" admin command naming (admkick, adminup, etc). JAPro's
version gates the command on its own two-rank, server-config-driven admin system
(G_AdminAllowed(..., JAPRO_ACCOUNTFLAG_A_CHANGEMAP, ...)); GalaxyRP has no such ranks, so that
check is replaced with this mod's per-account admin bit system (check_admin_command(ent,
ADM_CHANGEMAP, ...)), using the previously-unused "Placeholder" bit (32768) freed up by a removed
command. The gametype bound was also switched from JAPro's hardcoded '0'-'8' char check to this
mod's own GT_MAX_GAME_TYPE, since the two projects' gametype lists don't necessarily match, and a
message is now sent back to the admin when the requested map isn't found instead of silently doing
nothing.
==================
*/
void Cmd_AdmMap_f( gentity_t *ent ) {
	char	gametype[8];
	int		gtype = 0;
	char	mapname[MAX_MAPNAMELENGTH];

	if (!check_admin_command(ent, ADM_CHANGEMAP, qtrue))
	{
		return;
	}

	if ( trap->Argc() != 3 )
	{
		trap->SendServerCommand( ent-g_entities, "print \"Usage: /admmap <gametype number> <map name>.\n\"" );
		return;
	}

	trap->Argv( 1, gametype, sizeof( gametype ) );
	trap->Argv( 2, mapname, sizeof( mapname ) );

	if (strchr(mapname, ';') || strchr(mapname, '\r') || strchr(mapname, '\n'))
	{ // zyk: block console command injection through the map name argument
		trap->SendServerCommand( ent-g_entities, "print \"Invalid map name.\n\"" );
		return;
	}

	// GalaxyRP fix: [Admin] atoi() silently returns 0 for a non-numeric string (e.g. "dfd"
	// would parse as gametype 0 / FFA instead of being rejected), so require the argument to
	// actually be an integer first using this file's existing StringIsInteger() helper.
	//
	// GalaxyRP fix: [Admin] the old check only rejected values outside 0..GT_MAX_GAME_TYPE-1,
	// which let an admin "successfully" switch the server to a gametype Jedi Academy doesn't
	// actually support (GT_HOLOCRON, GT_JEDIMASTER, GT_SINGLE_PLAYER, GT_CTY -- 1, 2, 5, 9),
	// leaving the server in a broken/inert state. Only the six gametypes this game actually
	// implements are accepted now.
	if (!StringIsInteger(gametype))
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid gametype. Choose: 0 - Free For All, 3 - Duel, 4 - Power Duel, 6 - Team Free For All, 7 - Siege, 8 - Capture the Flag.\n\"" );
		return;
	}

	gtype = atoi(gametype);

	switch (gtype)
	{
		case GT_FFA:
		case GT_DUEL:
		case GT_POWERDUEL:
		case GT_TEAM:
		case GT_SIEGE:
		case GT_CTF:
			break;
		default:
			trap->SendServerCommand( ent-g_entities, "print \"Invalid gametype. Choose: 0 - Free For All, 3 - Duel, 4 - Power Duel, 6 - Team Free For All, 7 - Siege, 8 - Capture the Flag.\n\"" );
			return;
	}

	{ // zyk: make sure the requested map actually exists before changing to it
		char				unsortedMaps[4096];
		char*				possibleMapName;
		int					numMaps;
		const unsigned int	MAX_MAPS = 512;
		qboolean			found = qfalse;

		numMaps = trap->FS_GetFileList( "maps", ".bsp", unsortedMaps, sizeof( unsortedMaps ) );
		if (numMaps) {
			int len, i;
			if (numMaps > MAX_MAPS)
				numMaps = MAX_MAPS;
			possibleMapName = unsortedMaps;
			for (i = 0; i < numMaps; i++) {
				len = strlen(possibleMapName);
				if (!Q_stricmp(possibleMapName + len - 4, ".bsp"))
					possibleMapName[len-4] = '\0';
				if (!Q_stricmp(mapname, possibleMapName)) {
					found = qtrue;
					break;
				}
				possibleMapName += len + 1;
			}
		}

		if (!found)
		{
			// GalaxyRP fix: [Admin] this engine's console tokenizer (Cmd_TokenizeString2 in
			// cmd.cpp) does not support backslash-escaped quotes inside a quoted string -- a
			// literal \" here isn't an escaped quote, it's a backslash followed by the quote
			// that closes the string early, cutting the printed message down to "Map \" and
			// dropping everything after it. Use single quotes around the map name instead
			// (same convention already used elsewhere in this file, e.g. the duel team
			// validation message above) so the double-quoted print string stays intact.
			trap->SendServerCommand( ent-g_entities, va("print \"Map '%s' not found.\n\"", mapname) );
			return;
		}
	}

	trap->SendServerCommand( -1, va("print \"^3Map change triggered by ^7%s\n\"", ent->client->pers.netname) );
	G_LogPrintf( "Map change triggered by ^7%s\n", ent->client->pers.netname );

	trap->SendConsoleCommand( EXEC_APPEND, va("g_gametype %i\n", gtype) );
	trap->SendConsoleCommand( EXEC_APPEND, va("map %s\n", mapname) );
}

/*
==================
Cmd_Order_f
==================
*/
void Cmd_Order_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int i = 0;
   
	if ( trap->Argc() == 1) 
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"^3/order follow: ^7npc will follow the leader\n^3/order guard: ^7npc will stand still shooting at everyone except the leader and his allies\n^3/order cover: ^7npc will follow the leader shooting at everyone except the leader and his allies\n\"" );
	}
	else
	{
		trap->Argv( 1, arg1, sizeof( arg1 ) );

		if (Q_stricmp(arg1, "follow") == 0)
		{
			for (i = MAX_CLIENTS; i < level.num_entities; i++)
			{
				gentity_t *this_ent = &g_entities[i];

				if (this_ent && this_ent->client && this_ent->NPC && this_ent->client->NPC_class != CLASS_VEHICLE && 
					this_ent->client->leader == ent)
				{
					this_ent->client->pers.player_statuses &= ~(1 << 18);
					this_ent->client->pers.player_statuses &= ~(1 << 19);
					this_ent->NPC->tempBehavior = BS_FOLLOW_LEADER;
				}
			}
			trap->SendServerCommand( ent-g_entities, "print \"Order given.\n\"" );
		}
		else if (Q_stricmp(arg1, "guard") == 0)
		{
			for (i = MAX_CLIENTS; i < level.num_entities; i++)
			{
				gentity_t *this_ent = &g_entities[i];

				if (this_ent && this_ent->client && this_ent->NPC && this_ent->client->NPC_class != CLASS_VEHICLE && 
					this_ent->client->leader == ent)
				{
					this_ent->NPC->tempBehavior = BS_STAND_GUARD;
					this_ent->client->pers.player_statuses &= ~(1 << 19);
					this_ent->client->pers.player_statuses |= (1 << 18);
				}
			}
			trap->SendServerCommand( ent-g_entities, "print \"Order given.\n\"" );
		}
		else if (Q_stricmp(arg1, "cover") == 0)
		{
			for (i = MAX_CLIENTS; i < level.num_entities; i++)
			{
				gentity_t *this_ent = &g_entities[i];

				if (this_ent && this_ent->client && this_ent->NPC && this_ent->client->NPC_class != CLASS_VEHICLE && 
					this_ent->client->leader == ent)
				{
					this_ent->NPC->tempBehavior = BS_FOLLOW_LEADER;
					this_ent->client->pers.player_statuses &= ~(1 << 18);
					this_ent->client->pers.player_statuses |= (1 << 19);
				}
			}
			trap->SendServerCommand( ent-g_entities, "print \"Order given.\n\"" );
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid npc order.\n\"" );
		}
	}
}

/*
==================
Cmd_Paralyze_f
==================
*/
void Cmd_Paralyze_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;

	if (!check_admin_command(ent, ADM_PARALYZE, qtrue))
	{
		return;
	}
   
	if ( trap->Argc() != 2) 
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"You must specify the player name or ID.\n\"" ); 
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->pers.player_statuses & (1 << 6))
	{ // zyk: if paralyzed, remove it from the target player
		g_entities[client_id].client->pers.player_statuses &= ~(1 << 6);

		// zyk: kill the target player to prevent exploits with RPG Mode commands
		//G_Kill(&g_entities[client_id]);

		trap->SendServerCommand( ent-g_entities, va("print \"Target player %s ^7is no longer paralyzed\n\"", g_entities[client_id].client->pers.netname) );
		trap->SendServerCommand( client_id, va("print \"You are no longer paralyzed\n\"") );
	}
	else
	{ // zyk: paralyze the target player
		g_entities[client_id].client->pers.player_statuses |= (1 << 6);

		g_entities[client_id].client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
		g_entities[client_id].client->ps.forceHandExtendTime = level.time + 500;
		g_entities[client_id].client->ps.velocity[2] += 150;
		g_entities[client_id].client->ps.forceDodgeAnim = 0;
		g_entities[client_id].client->ps.quickerGetup = qtrue;

		trap->SendServerCommand( ent-g_entities, va("print \"Paralyzed the target player %s^7\n\"", g_entities[client_id].client->pers.netname) );
		trap->SendServerCommand( client_id, va("print \"You were paralyzed by an admin\n\"") );
	}
}

/*
==================
Cmd_Players_f
==================
*/
void Cmd_Players_f( gentity_t *ent ) {
	char content[MAX_STRING_CHARS];
	int i = 0;
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;
	int number_of_args = trap->Argc();

	strcpy(content,"ID - Name - IP - Type\n");

	if (!check_admin_command(ent, ADM_PLAYERS, qtrue))
	{
		return;
	}

	if (number_of_args == 1)
	{
		for (i = 0; i < level.maxclients; i++)
		{
			gentity_t *player = &g_entities[i];

			if (player && player->client && player->client->pers.connected != CON_DISCONNECTED)
			{
				strcpy(content, va("%s%d - %s ^7- %s - ",content,player->s.number,player->client->pers.netname,player->client->sess.IP));

				if (player->client->sess.amrpgmode > 0)
				{
					if (player->client->pers.bitvalue != 0)
						strcpy(content, va("%s^3(admin)",content));
					else
						strcpy(content, va("%s^3(logged)",content));
				}

				if (player->client->sess.amrpgmode == 2)
				{
					strcpy(content, va("%s ^3(rpg)",content));
				}

				strcpy(content, va("%s^7\n",content));
			}
		}

		trap->SendServerCommand( ent-g_entities, va("print \"%s\"", content) );
	}
	else
	{
		gentity_t *player_ent = NULL;

		trap->Argv( 1, arg1, sizeof( arg1 ) );

		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if (client_id == -1)
		{
			return;
		}

		player_ent = &g_entities[client_id];

		if (player_ent->client->sess.amrpgmode != 2)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Player %s ^7is not in RPG Mode.\n\"", player_ent->client->pers.netname) );
			return;
		}

		if (number_of_args == 2)
		{
			list_rpg_info(player_ent, ent);
		}
		else
		{
			char arg2[MAX_STRING_CHARS];

			trap->Argv( 2, arg2, sizeof( arg2 ) );

			if (Q_stricmp(arg2, "force") == 0 || Q_stricmp(arg2, "weapons") == 0 || Q_stricmp(arg2, "other") == 0 || 
				Q_stricmp(arg2, "ammo") == 0 || Q_stricmp(arg2, "items") == 0)
			{ // zyk: show skills of the player
				zyk_list_player_skills(player_ent, ent, G_NewString(arg2));
			}
			else
			{
				trap->SendServerCommand( ent-g_entities, "print \"Invalid option.\n\"" );
			}
		}
	}
}

/*
==================
Cmd_Ignore_f
==================
*/
void Cmd_Ignore_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;
	gentity_t *player = NULL;

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You must pass a player name or ID as argument.\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	player = &g_entities[client_id];

	if (client_id < 31 && !(level.ignored_players[ent->s.number][0] & (1 << client_id)))
	{
		level.ignored_players[ent->s.number][0] |= (1 << client_id);
		trap->SendServerCommand( ent-g_entities, va("print \"Ignored player %s^7\n\"", player->client->pers.netname) );
	}
	else if (client_id >= 31 && !(level.ignored_players[ent->s.number][1] & (1 << (client_id - 31))))
	{
		level.ignored_players[ent->s.number][1] |= (1 << (client_id - 31));
		trap->SendServerCommand( ent-g_entities, va("print \"Ignored player %s^7\n\"", player->client->pers.netname) );
	}
	else if (client_id < 31 && level.ignored_players[ent->s.number][0] & (1 << client_id))
	{
		level.ignored_players[ent->s.number][0] &= ~(1 << client_id);
		trap->SendServerCommand( ent-g_entities, va("print \"No longer ignore player %s^7\n\"", player->client->pers.netname) );
	}
	else if (client_id >= 31 && level.ignored_players[ent->s.number][1] & (1 << (client_id - 31)))
	{
		level.ignored_players[ent->s.number][1] &= ~(1 << (client_id - 31));
		trap->SendServerCommand( ent-g_entities, va("print \"No longer ignore player %s^7\n\"", player->client->pers.netname) );
	}
}

/*
==================
Cmd_IgnoreList_f
==================
*/
void Cmd_IgnoreList_f(gentity_t *ent) {
	int i = 0;
	char ignored_players[MAX_STRING_CHARS];
	gentity_t *player = NULL;

	strcpy(ignored_players, "");

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (i < 31 && level.ignored_players[ent->s.number][0] & (1 << i))
		{
			player = &g_entities[i];
			strcpy(ignored_players, va("%s%s^7\n", ignored_players, player->client->pers.netname));
		}
		else if (i >= 31 && level.ignored_players[ent->s.number][1] & (1 << (i - 31)))
		{
			player = &g_entities[i];
			strcpy(ignored_players, va("%s%s^7\n", ignored_players, player->client->pers.netname));
		}
	}

	trap->SendServerCommand(ent->s.number, va("print \"%s^7\n\"", ignored_players));
}

// GalaxyRP fix: these were only forward-declared further down in this file (right before
// Cmd_Saber_f), *after* update_saber() below already calls both of them. That left the compiler
// seeing an implicit (and wrong, int-returning) declaration at first use, which is a hard error
// under any strict/C++ compilation (and silently wrong even where it's only a warning) -- moving
// the declarations up here, before their first use, fixes it without touching the later ones.
extern qboolean duel_tournament_is_duelist(gentity_t *ent);
extern qboolean G_SaberModelSetup(gentity_t *ent);

// GalaxyRP: [Saber] shared by update_saber() (the "/saber <a> <b>" path) and Cmd_UpdateSaber_f()
// (the argument-less "/updatesaber" path below) -- both are just server-side saber changes, so
// both are subject to the same three restriction checks. These used to only be active at
// zyk_allow_saber_command level 2; that cvar is gone (see the instant-saber-switching change
// above this one) and this mod only ever wanted level-2 behaviour, so the checks now run
// unconditionally for any command that can change a player's saber. Prints the matching refusal
// message and returns qfalse if the change is currently blocked.
static qboolean saber_switch_allowed(gentity_t* ent)
{
	if (ent->client->ps.duelInProgress == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use this command in private duels.\n\"");
		return qfalse;
	}

	if (level.duel_tournament_mode == 4 && duel_tournament_is_duelist(ent) == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use this command while duelling in Duel Tournament.\n\"");
		return qfalse;
	}

	// GalaxyRP fix: [Guardian] a guardian_mode>0 guard blocking this command in boss battles used to be
	// here. guardian_mode is permanently 0 now, so it was unreachable.

	return qtrue;
}

// GalaxyRP: [Saber] shared tail of update_saber() -- re-reads this client's own userinfo, diffs
// its saber1/saber2 keys against pers.saber1/2, and applies any difference server-side
// (G_SetSaber/ClientUserinfoChanged/G_SaberModelSetup/saber-style reset), then syncs the client's
// own display. update_saber() calls this right after pushing saber1Model/saber2Model into
// userinfo itself (the "/saber <a> <b>" path, which names the sabers explicitly). Cmd_UpdateSaber_f
// below calls it directly with no such push, picking up whatever the in-game saber menu's Apply
// button already wrote into this client's own "saber1"/"saber2" userinfo cvars (see
// UI_UpdateSaberCvars() in ui_main.c) -- adapted from JA++'s japp_allowSaberSwitch idea (see
// C:\Users\richa\TaystJK\japp-master\game\g_client.cpp's ClientSpawn userinfo-diff block, which is
// what actually performs this apply there, only on respawn), but exposed as its own always-on
// command instead of folding it into "/saber" with no arguments (which here still just prints the
// player's current saber names, unchanged) and instead of JA++'s separate enable cvar.
static void apply_saber_from_userinfo(gentity_t* ent)
{
	qboolean changedSaber = qfalse;
	char userinfo[MAX_INFO_STRING] = { 0 }, * saber = NULL, * key = NULL, * value = NULL;

	//first we want the userinfo so we can see if we should update this client's saber -rww
	trap->GetUserinfo(ent->s.number, userinfo, sizeof(userinfo));

	for (int i = 0; i < MAX_SABERS; i++)
	{
		saber = (i & 1) ? ent->client->pers.saber2 : ent->client->pers.saber1;
		value = Info_ValueForKey(userinfo, va("saber%i", i + 1));
		if (saber && value &&
			(Q_stricmp(value, saber) || !saber[0] || !ent->client->saber[0].model[0]))
		{ //doesn't match up (or our saber is BS), we want to try setting it
			if (G_SetSaber(ent, i, value, qfalse))
				changedSaber = qtrue;

			//Well, we still want to say they changed then (it means this is siege and we have some overrides)
			else if (!saber[0] || !ent->client->saber[0].model[0])
				changedSaber = qtrue;
		}
	}

	if (changedSaber)
	{ //make sure our new info is sent out to all the other clients, and give us a valid stance
		if (!ClientUserinfoChanged(ent->s.number))
			return;

		//make sure the saber models are updated
		G_SaberModelSetup(ent);

		for (int i = 0; i < MAX_SABERS; i++)
		{
			saber = (i & 1) ? ent->client->pers.saber2 : ent->client->pers.saber1;
			key = va("saber%d", i + 1);
			value = Info_ValueForKey(userinfo, key);
			if (Q_stricmp(value, saber))
			{// they don't match up, force the user info
				Info_SetValueForKey(userinfo, key, saber);
				trap->SetUserinfo(ent->s.number, userinfo);
			}
		}

		if (ent->client->saber[0].model[0] && ent->client->saber[1].model[0])
		{ //dual
			ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = SS_DUAL;
		}
		else if ((ent->client->saber[0].saberFlags & SFL_TWO_HANDED))
		{ //staff
			ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = SS_STAFF;
		}
		else
		{
			ent->client->sess.saberLevel = Com_Clampi(SS_FAST, SS_STRONG, ent->client->sess.saberLevel);
			ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel;

			// limit our saber style to our force points allocated to saber offense
			if (level.gametype != GT_SIEGE && ent->client->ps.fd.saberAnimLevel > ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE])
				ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel = ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE];
		}
		if (level.gametype != GT_SIEGE)
		{// let's just make sure the styles we chose are cool
			if (!WP_SaberStyleValidForSaber(&ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, ent->client->ps.fd.saberAnimLevel))
			{
				WP_UseFirstValidSaberStyle(&ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &ent->client->ps.fd.saberAnimLevel);
				ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
			}
		}
	}

	// GalaxyRP fix: [Saber] push the final, authoritative saber1/saber2 down to the client's own
	// local cvars -- see CG_SaberUpdate_f in cg_servercmds.c for why this is needed (in short:
	// trap->SetUserinfo above only updates the SERVER's cached copy of this client's userinfo, it
	// never tells the client itself, so its own console/UI kept showing whatever it last sent,
	// including after a DB-driven login restore or a rejected/corrected invalid saber combo).
	// Sent unconditionally (not just when changedSaber) so a no-op /saber call (already matching)
	// or a login restore that leaves saber1/saber2 unchanged still keeps the client in sync.
	trap->SendServerCommand(ent - g_entities, va("supdatesaber \"%s\" \"%s\"\n", ent->client->pers.saber1, ent->client->pers.saber2));
}

void update_saber(gentity_t* ent, char* saber1Model, char* saber2Model, int number_of_args) {
	char userinfo[MAX_INFO_STRING] = { 0 }, * value = NULL;

	if (!saber_switch_allowed(ent))
		return;

	if (number_of_args == 1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Usage: /saber <saber1> <saber2>. Examples: /saber single_1, /saber single_1 single_1, /saber dual_1\n\"");
		return;
	}

	//first we want the userinfo so we can see if we should update this client's saber -rww
	trap->GetUserinfo(ent->s.number, userinfo, sizeof(userinfo));

	value = G_NewString(saber1Model);

	Info_SetValueForKey(userinfo, "saber1", value);

	if (number_of_args == 2)
	{
		value = "none";
	}
	else
	{
		value = G_NewString(saber2Model);
	}

	Info_SetValueForKey(userinfo, "saber2", value);

	trap->SetUserinfo(ent->s.number, userinfo);

	apply_saber_from_userinfo(ent);
}

/*
==================
Cmd_Saber_f
==================
*/
extern qboolean duel_tournament_is_duelist(gentity_t *ent);
extern qboolean G_SaberModelSetup(gentity_t *ent);
void Cmd_Saber_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int number_of_args = trap->Argc();

	trap->Argv(1, arg1, sizeof(arg1));
	trap->Argv(2, arg2, sizeof(arg2));

	update_saber(ent, arg1, arg2, number_of_args);
}

/*
==================
Cmd_UpdateSaber_f

GalaxyRP: [Saber] "/updatesaber" -- takes no arguments. Re-applies whatever saber hilt/type the
player already picked in the in-game saber menu and pressed Apply on (which only ever wrote to
this client's own "saber1"/"saber2" userinfo cvars, see UI_UpdateSaberCvars() in ui_main.c)
immediately, without needing a /kill or death to force a respawn -- ClientSpawn() in g_client.c
does this same userinfo diff-and-apply, but only ever runs it when the player is placed fresh in
the world. Adapted from JA++'s japp_allowSaberSwitch feature (see
C:\Users\richa\TaystJK\japp-master\game\g_client.cpp's ClientSpawn), kept as its own command
instead of folding it into "/saber" with no arguments (which stays a pure status query here) and
subject to the same restriction checks as any other saber change instead of a separate enable
cvar -- see saber_switch_allowed() above.
==================
*/
void Cmd_UpdateSaber_f( gentity_t *ent ) {
	if (!saber_switch_allowed(ent))
		return;

	apply_saber_from_userinfo(ent);
}

// GalaxyRP: [Saber RGB] defined in bg_saberLoad.c (shared across game/cgame/ui) -- Cmd_SaberColor_f
// below needs the palette-name string for a saber's currently-selected mode.
const char *SaberColorToString( saber_colors_t color );

/*
==================
GalaxyRP: [Saber RGB] custom blade colours

pers.saberRGB[] is the authoritative copy of a player's custom blade colours (see g_local.h). Three
different things can change it -- the /sabercolor command below, an RGB-capable client's own saber
menu writing its cp_sbRGB1/cp_sbRGB2 userinfo cvars, and a database restore on login -- and after
any of them the same three things have to happen, which is what update_saber_colors() below does:

  1. the "c3"/"c4" clientinfo configstring keys have to be rebuilt so everyone else's cgame sees
     the new colour, and the character's database row has to be updated. Both fall out of a single
     ClientUserinfoChanged() call: it republishes the configstring from pers.saberRGB[], and it
     already saves the character on every userinfo change (see update_current_character_name_and_model).
  2. the player's own cp_sbRGB1/cp_sbRGB2 and color1/color2 cvars have to be corrected, because
     those live on the CLIENT and the server cannot write them directly. Exactly the same problem
     -- and the same fix -- as saber models and the "supdatesaber" command: without this, a colour
     the server chose (restored from the database, or set by /sabercolor) would be invisible to the
     client's own console and saber menu, and the menu would happily re-send its stale value on the
     next Apply and undo the change.
==================
*/
void update_saber_colors( gentity_t *ent ) {
	char userinfo[MAX_INFO_STRING] = { 0 };

	if ( !ent || !ent->client )
		return;

	// GalaxyRP: [Saber RGB] patch the server's own CACHED copy of this client's userinfo before
	// calling ClientUserinfoChanged() below -- mirroring the same fix already applied to saber
	// models (see update_saber() above). ClientUserinfoChanged() reads cp_sbRGB1/cp_sbRGB2/color1/
	// color2 back out of this cache, not out of the values just decided by our three callers, so
	// without this a stale cached value (left over from any earlier RGB use this session) could
	// immediately stomp a freshly-set or freshly-cleared value within this same call.
	trap->GetUserinfo( ent->s.number, userinfo, sizeof( userinfo ) );
	Info_SetValueForKey( userinfo, "cp_sbRGB1", va( "%i", ent->client->pers.saberRGB[0] ) );
	Info_SetValueForKey( userinfo, "cp_sbRGB2", va( "%i", ent->client->pers.saberRGB[1] ) );
	Info_SetValueForKey( userinfo, "color1", va( "%i", ent->client->pers.saberColorMode[0] ) );
	Info_SetValueForKey( userinfo, "color2", va( "%i", ent->client->pers.saberColorMode[1] ) );
	trap->SetUserinfo( ent->s.number, userinfo );

	// Rebuild the clientinfo configstring (publishing c1..c4) and save the character.
	ClientUserinfoChanged( ent->s.number );

	// Push the authoritative values into the client's own cvars. Sent unconditionally, the same way
	// supdatesaber is, so a login restore that happens to leave the colour unchanged still leaves
	// the client's menu holding the right value rather than whatever it last sent. Four ints now
	// (mode1 rgb1 mode2 rgb2) instead of two, so the receiving end can restore color1/color2 and
	// the palette-name cvars too, not just the RGB payload -- see CG_SaberColorUpdate_f.
	trap->SendServerCommand( ent - g_entities, va( "supdatesabercolor %i %i %i %i\n",
		ent->client->pers.saberColorMode[0], ent->client->pers.saberRGB[0],
		ent->client->pers.saberColorMode[1], ent->client->pers.saberRGB[1] ) );
}

// Reads one 0-255 colour channel from a /sabercolor argument.
static int SaberColorArg( int argNum ) {
	char arg[MAX_TOKEN_CHARS];

	trap->Argv( argNum, arg, sizeof( arg ) );

	return Com_Clampi( 0, 255, atoi( arg ) );
}

// GalaxyRP: [Saber RGB] the ordinary (non-RGB, non-blade-type) palette entries /sabercolor accepts
// by name -- black included, since it has its own fixed shader rather than needing a custom colour.
static const struct { const char *name; saber_colors_t color; } saberOrdinaryColors[] = {
	{ "red",	SABER_RED },
	{ "orange",	SABER_ORANGE },
	{ "yellow",	SABER_YELLOW },
	{ "green",	SABER_GREEN },
	{ "blue",	SABER_BLUE },
	{ "purple",	SABER_PURPLE },
	{ "black",	SABER_BLACK },
};

static void SaberColorUsage( gentity_t *ent ) {
	trap->SendServerCommand( ent - g_entities,
		"print \"Usage: ^5/sabercolor <1|2> <red> <green> <blue>^7 (0-255 each)\n"
		"       ^5/sabercolor <1|2> <red|orange|yellow|green|blue|purple|black>^7\n\"" );
}

/*
==================
Cmd_SaberColor_f

  /sabercolor                          -- show both blades' current colours and the usage
  /sabercolor <1|2> <r> <g> <b>        -- set that blade's custom RGB colour (0-255 each channel)
  /sabercolor <1|2> <colour name>      -- set that blade to an ordinary palette colour, switching
                                           it out of RGB mode
==================
*/
void Cmd_SaberColor_f( gentity_t *ent ) {
	int argc = trap->Argc();
	int saberNum;
	char arg1[MAX_TOKEN_CHARS], arg2[MAX_TOKEN_CHARS];

	if ( argc == 1 ) {
		int i;

		SaberColorUsage( ent );

		for ( i = 0; i < 2; i++ ) {
			int mode = ent->client->pers.saberColorMode[i];

			if ( mode >= SABER_RGB && mode <= SABER_ELEC2 ) {
				trap->SendServerCommand( ent - g_entities, va( "print \"Saber %i is currently RGB %i %i %i.\n\"",
					i + 1,
					SABERRGB_R( ent->client->pers.saberRGB[i] ),
					SABERRGB_G( ent->client->pers.saberRGB[i] ),
					SABERRGB_B( ent->client->pers.saberRGB[i] ) ) );
			} else {
				trap->SendServerCommand( ent - g_entities, va( "print \"Saber %i is currently %s.\n\"",
					i + 1, SaberColorToString( (saber_colors_t)mode ) ) );
			}
		}
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	saberNum = atoi( arg1 );
	if ( saberNum != 1 && saberNum != 2 ) {
		SaberColorUsage( ent );
		return;
	}

	if ( argc == 5 ) {
		// /sabercolor <n> <r> <g> <b>
		int rgb = SABERRGB_PACK( SaberColorArg( 2 ), SaberColorArg( 3 ), SaberColorArg( 4 ) );

		ent->client->pers.saberRGB[saberNum - 1] = rgb;
		// GalaxyRP: [Saber RGB] entering an RGB triplet defaults the blade style to "classic" (6)
		// only if it wasn't already one of the five RGB-family values -- so re-picking the colour
		// on an already-blade-typed saber (via /saberblade) doesn't reset the style back to classic.
		if ( ent->client->pers.saberColorMode[saberNum - 1] < SABER_RGB ||
			ent->client->pers.saberColorMode[saberNum - 1] > SABER_ELEC2 )
			ent->client->pers.saberColorMode[saberNum - 1] = SABER_RGB;

		update_saber_colors( ent );

		trap->SendServerCommand( ent - g_entities, va( "print \"Saber %i colour set to %i %i %i.\n\"",
			saberNum, SABERRGB_R( rgb ), SABERRGB_G( rgb ), SABERRGB_B( rgb ) ) );
		return;
	}

	if ( argc == 3 ) {
		// /sabercolor <n> <name>
		int i;

		trap->Argv( 2, arg2, sizeof( arg2 ) );

		for ( i = 0; i < (int)ARRAY_LEN( saberOrdinaryColors ); i++ ) {
			if ( !Q_stricmp( arg2, saberOrdinaryColors[i].name ) ) {
				ent->client->pers.saberColorMode[saberNum - 1] = saberOrdinaryColors[i].color;
				// An ordinary named colour carries no custom RGB payload -- clear it so a later
				// /saberblade (which requires the mode to already be RGB-family) can't inherit a
				// stale colour left over from a previous RGB session on this saber.
				ent->client->pers.saberRGB[saberNum - 1] = 0;

				update_saber_colors( ent );

				trap->SendServerCommand( ent - g_entities, va( "print \"Saber %i colour set to %s.\n\"",
					saberNum, saberOrdinaryColors[i].name ) );
				return;
			}
		}

		trap->SendServerCommand( ent - g_entities, va(
			"print \"Unknown saber colour '%s'. Valid names: red, orange, yellow, green, blue, purple, black.\n\"",
			arg2 ) );
		return;
	}

	SaberColorUsage( ent );
}

/*
==================
Cmd_SaberBlade_f

  /saberblade <1|2> <bladeType>  -- pick a blade style among an already-RGB saber. Blade types:
                                     classic, flame1, electric1, flame2, electric2.
                                     The target saber must already be in RGB mode (set via
                                     /sabercolor <n> <r> <g> <b>) -- this only changes which of the
                                     five RGB-family shaders is used, not the custom colour itself.
==================
*/
static const struct { const char *name; saber_colors_t color; } saberBladeTypes[] = {
	{ "classic",	SABER_RGB },
	{ "flame1",		SABER_FLAME1 },
	{ "electric1",	SABER_ELEC1 },
	{ "flame2",		SABER_FLAME2 },
	{ "electric2",	SABER_ELEC2 },
};

void Cmd_SaberBlade_f( gentity_t *ent ) {
	int argc = trap->Argc();
	int saberNum, mode;
	char arg1[MAX_TOKEN_CHARS], arg2[MAX_TOKEN_CHARS];
	int i;

	if ( argc != 3 ) {
		trap->SendServerCommand( ent - g_entities,
			"print \"Usage: ^5/saberblade <1|2> <classic|flame1|electric1|flame2|electric2>^7\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	saberNum = atoi( arg1 );
	if ( saberNum != 1 && saberNum != 2 ) {
		trap->SendServerCommand( ent - g_entities,
			"print \"Usage: ^5/saberblade <1|2> ...^7 -- saber number must be 1 or 2.\n\"" );
		return;
	}

	trap->Argv( 2, arg2, sizeof( arg2 ) );
	mode = -1;
	for ( i = 0; i < (int)ARRAY_LEN( saberBladeTypes ); i++ ) {
		if ( !Q_stricmp( arg2, saberBladeTypes[i].name ) ) {
			mode = saberBladeTypes[i].color;
			break;
		}
	}
	if ( mode == -1 ) {
		trap->SendServerCommand( ent - g_entities,
			"print \"Unknown blade type. Valid types: classic, flame1, electric1, flame2, electric2.\n\"" );
		return;
	}

	if ( ent->client->pers.saberColorMode[saberNum - 1] < SABER_RGB ||
		ent->client->pers.saberColorMode[saberNum - 1] > SABER_ELEC2 ) {
		trap->SendServerCommand( ent - g_entities, va(
			"print \"Saber %i isn't set to a custom RGB colour yet -- use ^5/sabercolor %i <r> <g> <b>^7 first.\n\"",
			saberNum, saberNum ) );
		return;
	}

	ent->client->pers.saberColorMode[saberNum - 1] = mode;
	update_saber_colors( ent );

	trap->SendServerCommand( ent - g_entities, va( "print \"Saber %i blade style set to %s.\n\"",
		saberNum, arg2 ) );
}

// GalaxyRP (Alex): [Armor Skill] minimum time between successful Armor-skill blaster-deflect procs.
// Matches the existing Saber Defense "one block per ~200ms" throttle in g_missile.c, so a burst of
// blaster fire can't be deflected shot-for-shot once a player has high-level armor.
#define ARMOR_DEFLECT_COOLDOWN_MS 200

// GalaxyRP (Alex): [Armor Skill] revived zyk_can_deflect_shots() (previously a dead stub removed in
// an earlier cleanup pass -- see g_missile.c for where its call site was simplified away) to give
// level 1-5 Armor skill a percent chance to deflect an incoming blaster-type shot back at its
// shooter: 10% at level 1, 25% at level 2, 40% at level 3, 55% at level 4, 70% at level 5. Only one
// proc can succeed per ARMOR_DEFLECT_COOLDOWN_MS (see armor_deflect_timer in g_local.h), and the
// caller in g_missile.c only reaches this check for weapons that are already eligible for the
// existing FL_SHIELDED deflection path (Disruptor rifle shots are hitscan and never call this).
qboolean zyk_can_deflect_shots(gentity_t *ent)
{
	int armor_level = 0;
	int deflect_chance = 0;

	if (ent == NULL || ent->client == NULL || ent->client->sess.amrpgmode != 2)
	{
		return qfalse;
	}

	if (ent->health < 1)
	{ // zyk: a dead player cannot deflect anything
		return qfalse;
	}

	armor_level = ent->client->pers.skill_levels[56];

	if (armor_level < 1)
	{
		return qfalse;
	}

	if (ent->client->pers.armor_deflect_timer >= level.time)
	{ // zyk: still on cooldown from a previous deflect
		return qfalse;
	}

	deflect_chance = 10 + ((armor_level - 1) * 15); // zyk: 10/25/40/55/70% at levels 1-5

	if (Q_irand(0, 100) < deflect_chance)
	{
		ent->client->pers.armor_deflect_timer = level.time + ARMOR_DEFLECT_COOLDOWN_MS;
		return qtrue;
	}

	return qfalse;
}

// GalaxyRP fix: [Skills] removed zyk_can_use_unique() and Cmd_Unique_f() here -- the /unique command
// was already disabled (its console command table entry was commented out, further down this file),
// and its three Unique Abilities were the last remnant of a fully-removed 10-class system. Neither is
// coming back, so the dead command and its helper are gone rather than left disabled. The command
// table's own commented-out "unique" entry is removed below where the other command registrations are.


/*
==================
Cmd_DuelMode_f
==================
*/
extern void duel_tournament_end();
void Cmd_DuelMode_f(gentity_t *ent) {
	if (zyk_allow_duel_tournament.integer != 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^3Duel Tournament: ^7this mode is not allowed in this server\n\""));
		return;
	}

	if (level.duel_arena_loaded == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"There is no duel arena in this map\n\"");
		return;
	}

	if (level.sniper_mode > 0 && level.sniper_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Sniper Battle\n\"");
		return;
	}

	if (level.melee_mode > 0 && level.melee_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Melee Battle\n\"");
		return;
	}

	if (ent->client->pers.player_statuses & (1 << 26))
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join tournament while being in nofight mode\n\"");
		return;
	}

	if (level.duel_players[ent->s.number] == -1 && level.duel_tournament_mode > 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join the duel tournament now\n\"");
		return;
	}
	else if (level.duel_players[ent->s.number] == -1)
	{ // zyk: join the tournament
		if (level.duelists_quantity == MAX_CLIENTS)
		{
			trap->SendServerCommand(ent->s.number, va("print \"There are already %d duelists in tournament\n\"", MAX_CLIENTS));
			return;
		}
		else
		{
			if (level.duelists_quantity == 0)
			{ // zyk: first duelist joined. Put the globe model in the duel arena and set its origin point
				gentity_t *new_ent = G_Spawn();

				zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
				zyk_set_entity_field(new_ent, "spawnflags", "0");
				zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)level.duel_tournament_origin[0], (int)level.duel_tournament_origin[1], (int)level.duel_tournament_origin[2]));
				zyk_set_entity_field(new_ent, "model", "models/map_objects/vjun/globe.md3");
				zyk_set_entity_field(new_ent, "targetname", "zyk_duel_globe");
				zyk_set_entity_field(new_ent, "zykmodelscale", G_NewString(zyk_duel_tournament_arena_scale.string));

				zyk_spawn_entity(new_ent);

				level.duel_tournament_model_id = new_ent->s.number;
			}

			level.duel_tournament_mode = 1;
			level.duel_tournament_timer = level.time + zyk_duel_tournament_time_to_start.integer;
			level.duel_players[ent->s.number] = 0;
			level.duel_players_hp[ent->s.number] = 0;
			
			level.duelists_quantity++;

			// zyk: removing allies from this player
			ent->client->sess.ally1 = 0;
			ent->client->sess.ally2 = 0;

			trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7%s ^7joined the tournament!\n\"", ent->client->pers.netname));
		}
	}
	else if (level.duel_tournament_mode == 1)
	{ // zyk: leave the tournament
		level.duel_players[ent->s.number] = -1;
		level.duelists_quantity--;

		if (level.duelists_quantity == 0)
		{ // zyk: everyone left the tournament. End it
			duel_tournament_end();
		}

		trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7%s ^7left the tournament!\n\"", ent->client->pers.netname));
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot leave the duel tournament after it started\n\"");
	}
}

/*
==================
Cmd_DuelTable_f
==================
*/
void duel_show_table(gentity_t *ent)
{
	int i = 0;
	int j = 0;
	int chosen_player_id = -1;
	int array_length = 0;
	char content[1024];
	int sorted_players[MAX_CLIENTS]; // zyk: used to show score of players by ordering from the highest score to lowest
	int show_table_id = -1;

	if (ent)
	{
		show_table_id = ent->s.number;
	}

	if (level.duel_tournament_mode == 0)
	{
		trap->SendServerCommand(show_table_id, "print \"There is no duel tournament now\n\"");
		return;
	}

	// zyk: put the number of matches
	strcpy(content, va("\n^7Total: %d\nPlayed: %d\n\n", level.duel_matches_quantity, level.duel_matches_done));

	for (i = 0; i < MAX_CLIENTS; i++)
	{ // zyk: adding players to sorted_players and calculating the array length
		if (level.duel_players[i] != -1)
		{
			if (level.duel_allies[i] == -1 || i < level.duel_allies[i] || level.duel_allies[level.duel_allies[i]] != i)
			{ // zyk: do not sort the allies. Use the lower id to sort the score of a team if both players add themselves as a team
				sorted_players[array_length] = i;
				array_length++;
			}
		}
	}

	for (i = 0; i < array_length; i++)
	{ // zyk: sorting sorted_players array
		for (j = 1; j < array_length; j++)
		{
			if ((level.duel_players[sorted_players[j]] > level.duel_players[sorted_players[j - 1]]) ||
				(level.duel_players[sorted_players[j]] == level.duel_players[sorted_players[j - 1]] &&
					level.duel_players_hp[sorted_players[j]] > level.duel_players_hp[sorted_players[j - 1]]) ||
					(level.duel_players[sorted_players[j]] == level.duel_players[sorted_players[j - 1]] &&
						level.duel_players_hp[sorted_players[j]] == level.duel_players_hp[sorted_players[j - 1]] &&
						sorted_players[j] < sorted_players[j - 1]))
			{ // zyk: score of j is higher than j - 1, or remaining hp and shield of j higher than j - 1, or player id of j lower than j - 1
				chosen_player_id = sorted_players[j - 1];
				sorted_players[j - 1] = sorted_players[j];
				sorted_players[j] = chosen_player_id;
			}
		}
	}

	for (i = 0; i < array_length; i++)
	{
		gentity_t *player_ent = &g_entities[sorted_players[i]];
		char ally_name[36];

		if (level.duel_allies[sorted_players[i]] != -1 && level.duel_allies[level.duel_allies[sorted_players[i]]] == sorted_players[i])
		{ // zyk: show ally if they both added each other as a team
			strcpy(ally_name, va(" / %s", g_entities[level.duel_allies[sorted_players[i]]].client->pers.netname));
		}
		else
		{
			strcpy(ally_name, "");
		}

		strcpy(content, va("%s^7%s^7%s^7: ^3%d  ^1%d\n", content, player_ent->client->pers.netname, ally_name, level.duel_players[player_ent->s.number], level.duel_players_hp[player_ent->s.number]));
	}

	trap->SendServerCommand(show_table_id, va("print \"%s\n\"", content));
}

void Cmd_DuelTable_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	int page = 0;
	int i = 0;
	int results_per_page = 8;
	char content[MAX_STRING_CHARS];

	strcpy(content, "");

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You must pass a page number. Example: ^3/dueltable 1^7\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	page = atoi(arg1);

	if (page < 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid page number\n\"");
		return;
	}

	if (page == 1)
	{
		duel_show_table(ent);
	}
	else
	{
		page--;

		// zyk: makes i start from the first result of the correct page
		i = results_per_page * (page - 1);

		while (i < (results_per_page * page) && i < level.duel_matches_quantity)
		{
			gentity_t *first_duelist = &g_entities[level.duel_matches[i][0]];
			gentity_t *second_duelist = &g_entities[level.duel_matches[i][1]];
			char first_ally_name[36];
			char second_ally_name[36];
			char first_name[96];
			char second_name[96];

			strcpy(first_ally_name, "");
			strcpy(second_ally_name, "");
			strcpy(first_name, "^3Left Tournament");
			strcpy(second_name, "^3Left Tournament");

			if (first_duelist && level.duel_allies[first_duelist->s.number] != -1)
			{
				strcpy(first_ally_name, va("^7 / %s", g_entities[level.duel_allies[first_duelist->s.number]].client->pers.netname));
			}

			if (second_duelist && level.duel_allies[second_duelist->s.number] != -1)
			{
				strcpy(second_ally_name, va("^7 / %s", g_entities[level.duel_allies[second_duelist->s.number]].client->pers.netname));
			}

			if (first_duelist && first_duelist->client && 
				first_duelist->client->pers.connected == CON_CONNECTED &&
				first_duelist->client->sess.sessionTeam != TEAM_SPECTATOR &&
				level.duel_players[first_duelist->s.number] != -1)
			{ // zyk: first duelist still in Tournament
				strcpy(first_name, va("^7%s%s", first_duelist->client->pers.netname, first_ally_name));
			}

			if (second_duelist && second_duelist->client &&
				second_duelist->client->pers.connected == CON_CONNECTED &&
				second_duelist->client->sess.sessionTeam != TEAM_SPECTATOR &&
				level.duel_players[second_duelist->s.number] != -1)
			{ // zyk: second duelist still in Tournament
				strcpy(second_name, va("^7%s%s", second_duelist->client->pers.netname, second_ally_name));
			}

			if (i < level.duel_matches_done)
			{ // zyk: this match was already played in this tournament cycle
				strcpy(content, va("%s%s ^3%d x %d %s\n", content, first_name, level.duel_matches[i][2], level.duel_matches[i][3], second_name));
			}
			else if (i == level.duel_matches_done)
			{ // zyk: current match
				char duel_time_remaining[32];

				strcpy(duel_time_remaining, "");

				if (level.duel_tournament_mode == 4)
				{ // zyk: this duel is the current one, show the time remaining in seconds
					strcpy(duel_time_remaining, va("   ^3Time: ^7%d", (level.duel_tournament_timer - level.time) / 1000));
				}

				strcpy(content, va("%s%s ^1%d x %d %s%s\n", content, first_name, level.duel_matches[i][2], level.duel_matches[i][3], second_name, duel_time_remaining));
			}
			else
			{ // zyk: match not played yet in this tournament cycle
				strcpy(content, va("%s%s ^7%d x %d %s\n", content, first_name, level.duel_matches[i][2], level.duel_matches[i][3], second_name));
			}

			i++;
		}

		trap->SendServerCommand(ent->s.number, va("print \"\n^7%s\n\"", content));
	}
}

/*
==================
Cmd_DuelArena_f
==================
*/
void Cmd_DuelArena_f(gentity_t *ent) {
	FILE *duel_arena_file;
	char content[1024];
	char zyk_info[MAX_INFO_STRING] = { 0 };
	char zyk_mapname[128] = { 0 };

	// zyk: getting the map name
	trap->GetServerinfo(zyk_info, sizeof(zyk_info));
	Q_strncpyz(zyk_mapname, Info_ValueForKey(zyk_info, "mapname"), sizeof(zyk_mapname));

	strcpy(content, "");

	if (!check_admin_command(ent, ADM_DUELARENA, qtrue))
	{
		return;
	}

	zyk_create_dir("duelarena");

	duel_arena_file = fopen(va("GalaxyRP/duelarena/%s/origin.txt", zyk_mapname), "r");
	if (duel_arena_file == NULL)
	{ // zyk: arena file does not exist yet, create one
		VectorCopy(ent->client->ps.origin, level.duel_tournament_origin);

		zyk_create_dir(va("duelarena/%s", zyk_mapname));

		duel_arena_file = fopen(va("GalaxyRP/duelarena/%s/origin.txt", zyk_mapname), "w");
		fprintf(duel_arena_file, "%d\n%d\n%d\n", (int)level.duel_tournament_origin[0], (int)level.duel_tournament_origin[1], (int)level.duel_tournament_origin[2]);
		fclose(duel_arena_file);

		level.duel_arena_loaded = qtrue;

		trap->SendServerCommand(ent - g_entities, va("print \"Added duel arena at %d %d %d\n\"", (int)level.duel_tournament_origin[0], (int)level.duel_tournament_origin[1], (int)level.duel_tournament_origin[2]));
	}
	else
	{ // zyk: arena file already exists, remove it
		fclose(duel_arena_file);

		remove(va("GalaxyRP/duelarena/%s/origin.txt", zyk_mapname));

		level.duel_arena_loaded = qfalse;

		trap->SendServerCommand(ent - g_entities, va("print \"Removed duel arena of this map\n\""));
	}
}

/*
==================
Cmd_DuelPause_f
==================
*/
void Cmd_DuelPause_f(gentity_t *ent) {
	if (!check_admin_command(ent, ADM_DUELARENA, qtrue))
	{
		return;
	}

	if (level.duel_tournament_mode == 0)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^3Duel Tournament: ^7There is no duel tournament now\n\"");
		return;
	}

	if (level.duel_tournament_paused == qfalse)
	{ // zyk: pauses the tournament
		level.duel_tournament_paused = qtrue;
		trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7pausing the tournament\"");
	}
	else
	{ // zyk: resumes the tournament
		level.duel_tournament_paused = qfalse;
		trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7resuming the tournament\"");
	}
}

/*
==================
Cmd_SniperMode_f
==================
*/
void Cmd_SniperMode_f(gentity_t *ent) {
	if (zyk_allow_sniper_battle.integer != 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^3Sniper Battle: ^7this mode is not allowed in this server\n\""));
		return;
	}

	/*if (ent->client->sess.amrpgmode == 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"You cannot be in RPG Mode to play the Sniper Battle.\n\"");
		return;
	}*/

	if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Duel Tournament\n\"");
		return;
	}

	if (level.melee_mode > 0 && level.melee_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Melee Battle\n\"");
		return;
	}

	if (ent->client->pers.player_statuses & (1 << 26))
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join sniper battle while being in nofight mode\n\"");
		return;
	}

	if (level.sniper_players[ent->s.number] == -1 && level.sniper_mode > 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join the Sniper Battle now\n\"");
		return;
	}
	else if (level.sniper_players[ent->s.number] == -1)
	{ // zyk: join the sniper battle
		level.sniper_players[ent->s.number] = 0;
		level.sniper_mode = 1;
		level.sniper_mode_timer = level.time + zyk_sniper_battle_time_to_start.integer;
		level.sniper_mode_quantity++;

		trap->SendServerCommand(-1, va("chat \"^3Sniper Battle: ^7%s ^7joined the battle!\n\"", ent->client->pers.netname));
	}
	else
	{
		level.sniper_players[ent->s.number] = -1;
		level.sniper_mode_quantity--;
		trap->SendServerCommand(-1, va("chat \"^3Sniper Battle: ^7%s ^7left the battle!\n\"", ent->client->pers.netname));
	}
}

/*
==================
Cmd_SniperTable_f
==================
*/
void Cmd_SniperTable_f(gentity_t *ent) {
	int i = 0;
	char content[1024];

	strcpy(content, "\nSniper Battle Players\n\n");

	if (level.sniper_mode == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"There is no Sniper Battle now\n\"");
		return;
	}

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.sniper_players[i] != -1)
		{ // zyk: a player in Sniper Battle
			gentity_t *player_ent = &g_entities[i];

			strcpy(content, va("%s^7%s   ^3%d\n", content, player_ent->client->pers.netname, level.sniper_players[i]));
		}
	}

	trap->SendServerCommand(ent->s.number, va("print \"%s\n\"", content));
}

/*
==================
Cmd_MeleeMode_f
==================
*/
extern void melee_battle_end();
void Cmd_MeleeMode_f(gentity_t *ent) {
	if (zyk_allow_melee_battle.integer != 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^3Melee Battle: ^7this mode is not allowed in this server\n\""));
		return;
	}

	/*if (ent->client->sess.amrpgmode == 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"You cannot be in RPG Mode to play the Melee Battle.\n\"");
		return;
	}*/

	if (level.melee_arena_loaded == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"There is no melee arena in this map\n\"");
		return;
	}

	if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Duel Tournament\n\"");
		return;
	}

	if (level.sniper_mode > 0 && level.sniper_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Sniper Battle\n\"");
		return;
	}

	if (ent->client->pers.player_statuses & (1 << 26))
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join melee battle while being in nofight mode\n\"");
		return;
	}

	if (level.melee_players[ent->s.number] == -1 && level.melee_mode > 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join the Melee Battle now\n\"");
		return;
	}
	else if (level.melee_players[ent->s.number] == -1)
	{ // zyk: join the melee battle
		if (level.melee_mode_quantity == 0)
		{ // zyk: first player joined. Put the model in the melee arena and set its origin point
			gentity_t *new_ent = G_Spawn();

			zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
			zyk_set_entity_field(new_ent, "spawnflags", "65537");
			zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)level.melee_mode_origin[0], (int)level.melee_mode_origin[1], (int)level.melee_mode_origin[2]));
			zyk_set_entity_field(new_ent, "model", "models/map_objects/factory/catw2_b.md3");
			zyk_set_entity_field(new_ent, "targetname", "zyk_melee_catwalk");
			zyk_set_entity_field(new_ent, "zykmodelscale", "400");
			zyk_set_entity_field(new_ent, "mins", "-256 -256 -32");
			zyk_set_entity_field(new_ent, "maxs", "256 256 32");

			zyk_spawn_entity(new_ent);

			level.melee_model_id = new_ent->s.number;
		}

		level.melee_players[ent->s.number] = 0;
		level.melee_mode = 1;
		level.melee_mode_timer = level.time + 12000;
		level.melee_mode_quantity++;

		trap->SendServerCommand(-1, va("chat \"^3Melee Battle: ^7%s ^7joined the battle!\n\"", ent->client->pers.netname));
	}
	else
	{
		level.melee_players[ent->s.number] = -1;
		level.melee_mode_quantity--;

		if (level.melee_mode_quantity == 0)
		{ // zyk: everyone left the battle. Remove the catwalk
			melee_battle_end();
		}

		trap->SendServerCommand(-1, va("chat \"^3Melee Battle: ^7%s ^7left the battle!\n\"", ent->client->pers.netname));
	}
}

/*
==================
Cmd_MeleeArena_f
==================
*/
void Cmd_MeleeArena_f(gentity_t *ent) {
	FILE *duel_arena_file;
	char content[1024];
	char zyk_info[MAX_INFO_STRING] = { 0 };
	char zyk_mapname[128] = { 0 };

	// zyk: getting the map name
	trap->GetServerinfo(zyk_info, sizeof(zyk_info));
	Q_strncpyz(zyk_mapname, Info_ValueForKey(zyk_info, "mapname"), sizeof(zyk_mapname));

	strcpy(content, "");

	if (!check_admin_command(ent, ADM_DUELARENA, qtrue))
	{
		return;
	}

	zyk_create_dir("meleearena");

	duel_arena_file = fopen(va("GalaxyRP/meleearena/%s/origin.txt", zyk_mapname), "r");
	if (duel_arena_file == NULL)
	{ // zyk: arena file does not exist yet, create one
		VectorCopy(ent->client->ps.origin, level.melee_mode_origin);

		zyk_create_dir(va("meleearena/%s", zyk_mapname));

		duel_arena_file = fopen(va("GalaxyRP/meleearena/%s/origin.txt", zyk_mapname), "w");
		fprintf(duel_arena_file, "%d\n%d\n%d\n", (int)level.melee_mode_origin[0], (int)level.melee_mode_origin[1], (int)level.melee_mode_origin[2]);
		fclose(duel_arena_file);

		level.melee_arena_loaded = qtrue;

		trap->SendServerCommand(ent - g_entities, va("print \"Added melee arena at %d %d %d\n\"", (int)level.melee_mode_origin[0], (int)level.melee_mode_origin[1], (int)level.melee_mode_origin[2]));
	}
	else
	{ // zyk: arena file already exists, remove it
		fclose(duel_arena_file);

		remove(va("GalaxyRP/meleearena/%s/origin.txt", zyk_mapname));

		level.melee_arena_loaded = qfalse;

		trap->SendServerCommand(ent - g_entities, va("print \"Removed melee arena of this map\n\""));
	}
}

/*
==================
Cmd_RpgLmsMode_f
==================
*/
void Cmd_RpgLmsMode_f(gentity_t *ent) {
	if (zyk_allow_rpg_lms.integer != 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^3RPG LMS: ^7this mode is not allowed in this server\n\""));
		return;
	}

	// GalaxyRP fix: [Guardian] a guardian_mode>0 guard blocking /rpglms during boss battles used to be
	// here. guardian_mode is permanently 0 now, so it was unreachable.

	if (ent->client->pers.player_statuses & (1 << 26))
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join RPG LMS while being in nofight mode\n\"");
		return;
	}

	if (level.rpg_lms_players[ent->s.number] == -1 && level.rpg_lms_mode > 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join the RPG LMS now\n\"");
		return;
	}
	else if (level.rpg_lms_players[ent->s.number] == -1)
	{ // zyk: join the rpg lms battle
		level.rpg_lms_players[ent->s.number] = 0;
		level.rpg_lms_mode = 1;
		level.rpg_lms_timer = level.time + 15000;
		level.rpg_lms_quantity++;

		trap->SendServerCommand(-1, va("chat \"^3RPG LMS: ^7%s ^7joined the battle!\n\"", ent->client->pers.netname));
	}
	else
	{
		level.rpg_lms_players[ent->s.number] = -1;
		level.rpg_lms_quantity--;
		trap->SendServerCommand(-1, va("chat \"^3RPG LMS: ^7%s ^7left the battle!\n\"", ent->client->pers.netname));
	}
}

/*
==================
Cmd_RpgLmsTable_f
==================
*/
void Cmd_RpgLmsTable_f(gentity_t *ent) {
	int i = 0;
	char content[1024];

	strcpy(content, "\nRPG LMS Players\n\n");

	if (level.rpg_lms_mode == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"There is no RPG LMS now\n\"");
		return;
	}

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.rpg_lms_players[i] != -1)
		{ // zyk: a player in RPG LMS Battle
			gentity_t *player_ent = &g_entities[i];

			strcpy(content, va("%s^7%s   ^3%d\n", content, player_ent->client->pers.netname, level.rpg_lms_players[i]));
		}
	}

	trap->SendServerCommand(ent->s.number, va("print \"%s\n\"", content));
}

/*
==================
Cmd_News_f
==================
*/
void Cmd_News_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS], arg2[MAX_STRING_CHARS];
	int numberOfEntries = 10;

	if (trap->Argc() > 3 || trap->Argc() < 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: /news <channel name> <number of entries>\n\"");
		return;
	}

	if (trap->Argc() == 3)
	{
		trap->Argv(2, arg2, sizeof(arg2));
		numberOfEntries = atoi(arg2);
		// GalaxyRP fix: [validation] this used to print the error and fall through anyway, so
		// /news <channel> <count> would still run with the out-of-range count -- e.g. a count over 10
		// bypassed the cap entirely (the display loop's only bound is numberOfEntries), and a negative
		// count got bound straight into the SQL LIMIT, which SQLite treats as "no limit" and fetches
		// every row in the channel for nothing.
		if (numberOfEntries < 1 || numberOfEntries > 10) {
			trap->SendServerCommand(ent->s.number, "print \"Error: Can only display between one and ten entries at a time.\n\"");
			return;
		}
	}

	trap->Argv(1, arg1, sizeof(arg1));
	trap->SendServerCommand(ent->s.number, va("print \"^3Viewing entries in channel ^2%s^3:\n\"", arg1));
	select_news_from_channel(ent, arg1, numberOfEntries);
}

void Cmd_NewsChannels_f(gentity_t* ent) {
	select_news_channels(ent);
	return;
}

void Cmd_NewsRemove_f(gentity_t* ent) {
	char arg1[MAX_STRING_CHARS];
	int newsID;

	if (!check_admin_command(ent, ADM_REMOVENEWS, qtrue))
	{
		return;
	}

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: /newsremove <news id>\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));
	newsID = atoi(arg1);
	// GalaxyRP fix: [validation] newsID is the News table's INTEGER PRIMARY KEY, which (like CharID)
	// auto-starts at 1 -- 0 is never a real row, so this used to let "ID must be positive" through for
	// an ID of exactly 0.
	if (newsID <= 0) {
		trap->SendServerCommand(ent->s.number, "print \"ID must be positive.\n\"");
		return;
	}

	// GalaxyRP fix: [Database] delete_news_table_row_with_id used to go through run_db_query(), which
	// is a bare sqlite3_exec() wrapper that reports neither rows-affected nor failure back to the
	// caller -- so this command claimed "Removed news entry with ID %i!" unconditionally, even for an
	// ID that matched nothing. It now reports whether a row was actually deleted.
	if (delete_news_table_row_with_id(ent, newsID))
	{
		trap->SendServerCommand(ent->s.number, va("print \"Removed news entry with ID %i!\n\"", newsID));
	}
	else
	{
		trap->SendServerCommand(ent->s.number, va("print \"No news entry found with ID %i.\n\"", newsID));
	}

	return;
}

/*
==================
Cmd_UpdateNews_f
==================
*/
void Cmd_UpdateNews_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	FILE *news_file = NULL;

	if (!check_admin_command(ent, ADM_UPDATENEWS, qtrue))
	{
		return;
	}

	if (trap->Argc() != 3)
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: /newsadd <channel> <news text>\n\"");
		return;
	}
	trap->Argv(1, arg1, sizeof(arg1));
	trap->Argv(2, arg2, sizeof(arg2));

	// GalaxyRP fix: [validation] nothing stopped an admin from posting a blank news entry (e.g.
	// /newsadd general "" or a channel name that trims to nothing) -- reject empty channel/text instead
	// of silently inserting an empty row.
	if (arg1[0] == '\0' || arg2[0] == '\0')
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: /newsadd <channel> <news text>\n\"");
		return;
	}

	insert_news_table_row(ent, arg1, arg2);

	trap->SendServerCommand(ent->s.number, va("print \"Added news to channel %s\n\"", arg1));

}

void description_display_beginning(gentity_t *ent, char netname[MAX_STRING_CHARS]) {
	trap->SendServerCommand(ent->s.number, va("print \"^2Looking over %s^2 you notice:\n\"", netname));
	trap->SendServerCommand(ent->s.number, "print \"^2================================================================================\n\"");
}

void description_display_end(gentity_t *ent) {
	trap->SendServerCommand(ent->s.number, "print \"^2================================================================================\n\"");
}

void description_add(gentity_t *ent, char description_to_add[MAX_STRING_CHARS]) {
	FILE *description_file = NULL;

	description_file = fopen(va("GalaxyRP/descriptions/%s.txt", ent->client->sess.rpgchar), "w+");

	if (description_file != NULL) {
		fputs(va("%s\n", description_to_add), description_file);
		fclose(description_file);
		trap->SendServerCommand(ent->s.number, "print \"Description set sucessfully.\n\"");
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"File not found.\n\"");
	}

	return;
}

/*
==================
Cmd_Examine_f
==================
*/
void Cmd_Examine_f(gentity_t *ent) {
	char player_name[MAX_STRING_CHARS];

	if (trap->Argc() != 2) {
		trap->SendServerCommand(ent->s.number, "print \"Usage: /examine <playername>\n\"");
		return;
	}

	trap->Argv(1, player_name, sizeof(player_name));
	int player_id = ClientNumberFromString(ent, player_name, qfalse);

	//player not found, no point in going on
	if (player_id == -1) {
		return;
	}

	// GalaxyRP fix: [cleanup] see the identical fix/comment where this same line appears above.
	if (Distance(ent->client->ps.origin, g_entities[player_id].client->ps.origin) > 1000) {
		trap->SendServerCommand(ent->s.number, "print \"You are too far away from that person.\n\"");
		return;
	}

	if (trap->Argc())
	{

		// GalaxyRP fix: [cleanup] same "extra &" pattern as the Distance() calls above --
		// pers.netname is already a char[]; MSVC flagged the pointer-to-array this produced
		// (C4047/C4024) against description_display_beginning()'s char* parameter.
		description_display_beginning(ent, g_entities[player_id].client->pers.netname);
		trap->SendServerCommand(ent->s.number, va("print \"%s\n\"", &g_entities[player_id].client->pers.description));
		description_display_end(ent);

		return;
	}
	return;
}

/*
==================
Cmd_Attributes_f
==================
*/
void Cmd_Attributes_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];

	if (trap->Argc() != 2) {
		trap->SendServerCommand(ent->s.number, "print \"Usage: /attributes <text>\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	strcpy(ent->client->pers.description, arg1);

	update_chars_table_row_with_current_values(ent);

	return;
}

/*
==================
Cmd_ShakeScreen_f
==================
*/
void Cmd_ShakeScreen_f(gentity_t* ent)
{
	int i, distanceFromPlayer, intensity, duration;
	char arg1[MAX_STRING_CHARS], arg2[MAX_STRING_CHARS], arg3[MAX_STRING_CHARS];
	gentity_t *other;

	if (!check_admin_command(ent, ADM_SHAKESCREEN, qtrue))
	{
		return;
	}

	if (trap->Argc() != 4)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^2Command Usage: /shakeScreen <distance from player> <intensity> <length>\nExample: /shakeScreen 1 5 7\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));
	distanceFromPlayer = atoi(arg1);
	trap->Argv(2, arg2, sizeof(arg2));
	intensity = atoi(arg2);
	trap->Argv(3, arg3, sizeof(arg3));
	duration = atoi(arg3) * 1000;

	for (i = 0; i < level.maxclients; i++)
	{
		other = &g_entities[i];

		if (g_entities[i].inuse && g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED && Distance(ent->client->ps.origin, other->client->ps.origin) <= distanceFromPlayer)
		{
			G_ScreenShake(g_entities[i].s.origin, &g_entities[i], intensity, duration, qtrue);
			trap->SendServerCommand(i, "print \"^3An admin shook your screen.\n\"");
		}
	}
	trap->SendServerCommand(ent - g_entities, "print \"^2You shook people's screen.\n\"");
	return;
}

/*
==================
Cmd_NoFight_f
==================
*/
void Cmd_NoFight_f(gentity_t *ent) {
	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		if (ent->client->pers.player_statuses & (1 << 26))
		{
			ent->client->pers.player_statuses &= ~(1 << 26);
			trap->SendServerCommand(ent->s.number, "print \"Deactivated\n\"");
		}
		else
		{
			ent->client->pers.player_statuses |= (1 << 26);
			trap->SendServerCommand(ent->s.number, "print \"Activated\n\"");
		}
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"This command must be used as spectator\n\"");
	}
}
/*
==================
Cmd_ModVersion_f
==================
*/

void Cmd_ModVersion_f(gentity_t *ent) {
	trap->SendServerCommand(ent->s.number, va("print \"\n%s\n\n\"", GAMEVERSION));
}

typedef struct sound_channels_s {
	const char* channel_name;
	int			channel_code;
} sound_channels_t;

// GalaxyRP fix: [cleanup] declared with an explicit size ([14]) that had to be kept in sync by hand with
// the two hardcoded loop bounds in Cmd_ZykSound_f below -- let the compiler derive it from the
// initializer instead, and use ARRAY_LEN() at each call site, so adding/removing an entry here can't
// silently desync from those loops.
const sound_channels_t sound_channels[] = {
	{"auto",				CHAN_AUTO			},
	{"local",				CHAN_LOCAL			},
	{"weapon",				CHAN_WEAPON			},
	{"voice",				CHAN_VOICE			},
	{"voice_attenuate",		CHAN_VOICE_ATTEN	},
	{"item",				CHAN_ITEM			},
	{"body",				CHAN_BODY			},
	{"ambient",				CHAN_AMBIENT		},
	{"local_sound",			CHAN_LOCAL_SOUND	},
	{"announcer",			CHAN_ANNOUNCER		},
	{"less_attenuate",		CHAN_LESS_ATTEN		},
	{"menu1",				CHAN_MENU1			},
	{"voice_global",		CHAN_VOICE_GLOBAL	},
	{"music",				CHAN_MUSIC			},
};

/*
==================
Cmd_ZykSound_f
==================
*/
void Cmd_ZykSound_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int soundIndex;

	if (rp_allow_playsound_command.integer < 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"This command is not allowed in this server\n\"");
		return;
	}

	if (trap->Argc() < 2 || trap->Argc() > 3)
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/playsound <channel> <sound file path>\n ^2Sound channels available: \n\"");
		// GalaxyRP fix: [cleanup] this hardcoded the array length (14) instead of deriving it, so
		// adding/removing a channel here without also updating this literal (and the matching one
		// below) would silently desync the printed list from the array, or read past its end.
		for (int i = 0; i < ARRAY_LEN(sound_channels); i++) {
			trap->SendServerCommand(ent->s.number, va("print \"^3%s\n\"", sound_channels[i].channel_name));
		}
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	if (trap->Argc() == 3)
	{
		trap->Argv(2, arg2, sizeof(arg2));

		// GalaxyRP fix: [validation] nothing rejected an empty sound path (e.g. /playsound auto "") --
		// that fed straight into G_SoundIndex()/G_Sound(), both of which assert() a non-empty/non-zero
		// value, so this could trip an assertion in a debug build for no reason.
		if (arg2[0] == '\0')
		{
			trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/playsound <channel> <sound file path>\n\"");
			return;
		}

		for (int i = 0; i < ARRAY_LEN(sound_channels); i++) {
			if (strcmp(arg1, sound_channels[i].channel_name) == 0) {
				// GalaxyRP fix: [security] G_SoundIndex() crashes the whole server (ERR_DROP) once its
				// 256-slot sound table fills up with never-before-seen names -- and this path is a raw
				// player-typed string with no cap on distinct values, so any connected player could
				// crash the server with a couple hundred /playsound calls using unique nonsense paths.
				// G_SoundIndexSafe() returns 0 instead of crashing when the table is full; refuse the
				// request with a message in that case instead of handing G_Sound() an invalid index.
				soundIndex = G_SoundIndexSafe(G_NewString(arg2));
				if (soundIndex == 0)
				{
					trap->SendServerCommand(ent->s.number, "print \"Cannot play this sound right now (server's sound table is full).\n\"");
					return;
				}

				G_Sound(ent, sound_channels[i].channel_code, soundIndex);

				return;
			}
		}

		trap->SendServerCommand(ent->s.number, "print \"Channel not found! \n\"");
		return;
	}

	if (trap->Argc() == 2)
	{
		if (arg1[0] == '\0')
		{
			trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/playsound <channel> <sound file path>\n\"");
			return;
		}

		soundIndex = G_SoundIndexSafe(G_NewString(arg1));
		if (soundIndex == 0)
		{
			trap->SendServerCommand(ent->s.number, "print \"Cannot play this sound right now (server's sound table is full).\n\"");
			return;
		}

		G_Sound(ent, CHAN_AUTO, soundIndex);
		return;
	}

	return;
}

/*
==================
Cmd_Music_f
==================
*/
void Cmd_Music_f(gentity_t* ent) {
	char audioPath[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_MUSIC, qtrue)) {
		return;
	}

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/playmusic <sound file path>\n\"");
		return;
	}

	trap->Argv(1, audioPath, sizeof(audioPath));

	// GalaxyRP fix: [validation] nothing rejected an empty path (e.g. /playmusic ""), which would
	// silently clear the level's music track instead of doing anything resembling "play a file".
	if (audioPath[0] == '\0')
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/playmusic <sound file path>\n\"");
		return;
	}

	trap->SendServerCommand(ent - g_entities, va("print \"^2You started playing the music file: ^7%s\n\"", audioPath));
	trap->SetConfigstring(CS_MUSIC, audioPath);

	return;
}

qboolean Is_Char_Name_Valid(char charName[MAX_STRING_CHARS]) {

	char forbiddenCharacters[MAX_STRING_CHARS] = " ?!�$%^&*()-+=][{}#~';:/>.<,|";

	for (int i = 0; i < strlen(charName); i++) {
		for (int j = 0; j < strlen(forbiddenCharacters); j++) {
			if (charName[i] == forbiddenCharacters[j]) {
				return qfalse;
			}
		}
	}

	return qtrue;
}

/*
==================
Cmd_CustomQuest_f
==================
*/
void save_quest_file(int quest_number)
{
	FILE *quest_file = NULL;
	int i = 0;

	zyk_create_dir("customquests");

	quest_file = fopen(va("GalaxyRP/customquests/%d.txt", quest_number), "w");
	fprintf(quest_file, "%s;%s;%s;\n", level.zyk_custom_quest_main_fields[quest_number][0], level.zyk_custom_quest_main_fields[quest_number][1], level.zyk_custom_quest_main_fields[quest_number][2]);

	for (i = 0; i < level.zyk_custom_quest_mission_count[quest_number]; i++)
	{
		int j = 0;

		for (j = 0; j < level.zyk_custom_quest_mission_values_count[quest_number][i]; j += 2)
		{
			fprintf(quest_file, "%s;%s;", level.zyk_custom_quest_missions[quest_number][i][j], level.zyk_custom_quest_missions[quest_number][i][j + 1]);
		}

		if (j > 0)
		{ // zyk: break line if the mission had at least one key/value pair to save
			fprintf(quest_file, "\n");
		}
	}

	fclose(quest_file);
}

// GalaxyRP fix: [validation] the old inline check was content[strlen(content) - 1] == '\n' with no
// guard for an empty content -- if a leaderboard record was ever short a line (a truncated/malformed
// leaderboard.txt, or an fgets() call that used to go unchecked -- see the fgets() NULL checks added
// in Cmd_DuelBoard_f below), content could be empty ("") and strlen(content) - 1 underflows to
// (size_t)-1, indexing out of bounds. Guard the empty case here once instead of at every call site.
static void RP_StripTrailingNewline(char *s)
{
	size_t len = strlen(s);
	if (len > 0 && s[len - 1] == '\n')
	{
		s[len - 1] = '\0';
	}
}

/*
==================
Cmd_DuelBoard_f
==================
*/
void Cmd_DuelBoard_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	int page = 1; // zyk: page the user wants to see
	char file_content[MAX_STRING_CHARS];
	char content[512]; // GalaxyRP fix: [cleanup] was 64 -- too small for a player name longer than
						// 63 characters, which would get split across two fgets() calls and desync
						// that record's fields. Match Cmd_MapList_f's buffer size.
	int i = 0;
	int results_per_page = zyk_list_cmds_results_per_page.integer; // zyk: number of results per page
	FILE *leaderboard_file;

	if (trap->Argc() < 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"Use ^3/duelboard <page number> ^7to see the Duel Tournament Leaderboard, which shows the winners and their number of tournaments won\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	if (level.duel_leaderboard_step > 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"Leaderboard is being generated. Please wait some seconds\n\"");
		return;
	}

	strcpy(file_content, "");
	strcpy(content, "");

	page = atoi(arg1);

	// GalaxyRP fix: [validation] atoi() only catches a page argument that parses to exactly 0; a
	// negative page number (e.g. "/duelboard -5") passed this check straight through and made both
	// pagination loop bounds below negative, so neither loop below ever ran and the command
	// silently printed a blank page instead of reporting the bad input.
	if (page <= 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid page number\n\"");
		return;
	}

	leaderboard_file = fopen("GalaxyRP/leaderboard.txt", "r");
	if (leaderboard_file != NULL)
	{
		// GalaxyRP fix: [security] each leaderboard record is 3 lines (header, name, wins); this
		// used to check fgets()'s return value only on the first of the 3 reads per iteration and
		// ignore it on the other two, so a truncated/malformed leaderboard.txt (a record not a clean
		// multiple of 3 lines) could read past a real record boundary or operate on stale content.
		// Bail out of the skip-loop the moment any read fails instead. The 3 discarded lines here
		// are never used for output, so there's no need to newline-strip them.
		while (i < (results_per_page * (page - 1)))
		{ // zyk: reads the file until it reaches the position corresponding to the page number
			if (fgets(content, sizeof(content), leaderboard_file) == NULL) break;
			if (fgets(content, sizeof(content), leaderboard_file) == NULL) break;
			if (fgets(content, sizeof(content), leaderboard_file) == NULL) break;
			i++;
		}

		while (i < (results_per_page * page))
		{
			// zyk: unused header/separator line for this record
			if (fgets(content, sizeof(content), leaderboard_file) == NULL) break;

			// zyk: player name
			if (fgets(content, sizeof(content), leaderboard_file) == NULL) break;
			RP_StripTrailingNewline(content);
			// GalaxyRP fix: [security] this used to be strcpy(file_content, va("%s%s     ",
			// file_content, content)) -- file_content is a fixed MAX_STRING_CHARS (1024-byte) stack
			// buffer, and that strcpy had no bounds check on the destination at all. Enough
			// entries on one page (or zyk_list_cmds_results_per_page set too high) overflows it.
			// Q_strcat never writes past the destination's declared size.
			Q_strcat(file_content, sizeof(file_content), content);
			Q_strcat(file_content, sizeof(file_content), "     ");

			// zyk: number of tournaments won
			if (fgets(content, sizeof(content), leaderboard_file) == NULL) break;
			RP_StripTrailingNewline(content);
			Q_strcat(file_content, sizeof(file_content), "^3");
			Q_strcat(file_content, sizeof(file_content), content);
			Q_strcat(file_content, sizeof(file_content), "^7\n");

			i++;
		}

		fclose(leaderboard_file);
		trap->SendServerCommand(ent->s.number, va("print \"\n%s\n\"", file_content));
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"No leaderboard yet\n\"");
		return;
	}
}

// GalaxyRP: [Training Saber fix] pers.training_mode is only ever written by this command and by
// ClientBegin's reset -- it is never told about a saber reselection (/saber, the saber-select UI,
// or a DB-driven saber reload on spawn), which fully re-parses the new saber's parms and resets
// damageScale/damageScale2 back to that saber's own defaults. That leaves a window where the flag
// still claims training mode is ACTIVE even though the player's current saber(s) are already back
// to full damage. Rather than trust the flag blindly, treat training mode as active only when the
// flag says so AND every saber slot's live damage scale is still actually zeroed, and self-heal the
// flag the moment that stops being true. This is what "true status" means below, and using it for
// the on/off guards too (not just the status print) keeps every code path agreeing with what's
// actually applied to the player right now.
static qboolean Training_IsLiveActive(gentity_t* ent)
{
	int i;

	if (!ent->client->pers.training_mode)
		return qfalse;

	for (i = 0; i < MAX_SABERS; i++)
	{
		if (ent->client->saber[i].damageScale != 0 || ent->client->saber[i].damageScale2 != 0)
		{
			// Stale flag: a saber slot's live damage scale no longer matches what training mode
			// applied, almost certainly because the player picked a new saber since. Self-heal so
			// nothing downstream (status display, on/off guards) is fooled by the old flag value.
			ent->client->pers.training_mode = qfalse;
			return qfalse;
		}
	}

	return qtrue;
}

//GalaxyRP (Alex): [Training Saber] This method activates and deactivates the training saber, making it so that when active, you do very little damage.

// GalaxyRP: [Training Saber fix] rewritten from a single no-argument toggle to explicit
// "/training on" / "/training off" verbs. The old toggle couldn't tell "turn on" from "turn off"
// apart from its own stored state, so if that state and the live saber values ever drifted --
// e.g. from picking a new saber while training was active, which resets damageScale/damageScale2
// back to that saber's own defaults without training_mode being told -- the next toggle would
// silently do the wrong thing (store the already-zeroed value as "original", or leave full damage
// live while claiming to be active). Explicit on/off can't cross those states the same way: "on"
// while already on and "off" while already off are both harmless no-ops that don't touch anything
// live, and turning it on is the point where the player is warned about the exact conditions that
// deactivate it, rather than needing to be told after the fact.
// "/training" with no argument now reports the true current status (see Training_IsLiveActive)
// alongside the usage tip, so a player who switched saber, /kill'd, or rejoined always sees what's
// actually applied rather than a possibly-stale flag.
void Cmd_TrainingMode_f(gentity_t* ent) {
	char arg[MAX_TOKEN_CHARS];
	int i;
	qboolean isActive;

	// Resolving this up front self-heals pers.training_mode if it had gone stale, so the status
	// print and the on/off guards below all agree with reality.
	isActive = Training_IsLiveActive(ent);

	if (trap->Argc() == 1)
	{
		if (isActive)
			trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/training <on|off>^7. Training saber is currently ^2ACTIVE\n\"");
		else
			trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/training <on|off>^7. Training saber is currently ^1INACTIVE\n\"");
		return;
	}

	trap->Argv(1, arg, sizeof(arg));

	if (!Q_stricmp(arg, "on"))
	{
		if (isActive)
		{
			trap->SendServerCommand(ent->s.number, "print \"Training saber is already ^2ACTIVE\n\"");
			return;
		}

		// GalaxyRP: [Training Saber fix] cover both saber slots (saber1 and saber2), not just the
		// primary one -- previously only ent->client->saber[0] (equivalent to the old
		// ent->client->saber-> shorthand) was touched, so a player's second, independently-wielded
		// saber kept dealing full damage the whole time training mode was "active".
		for (i = 0; i < MAX_SABERS; i++)
		{
			ent->client->pers.training_stored_damageScale[i] = ent->client->saber[i].damageScale;
			ent->client->pers.training_stored_damageScale2[i] = ent->client->saber[i].damageScale2;
			ent->client->saber[i].damageScale = 0;
			ent->client->saber[i].damageScale2 = 0;
		}

		ent->client->pers.training_mode = qtrue;

		trap->SendServerCommand(ent->s.number, "print \"Training saber is ^2ACTIVE^7. Warning: changing your saber or rejoining the game (reconnecting, or a map change) will turn this off.\n\"");
	}
	else if (!Q_stricmp(arg, "off"))
	{
		if (!isActive)
		{
			trap->SendServerCommand(ent->s.number, "print \"Training saber is already ^1INACTIVE\n\"");
			return;
		}

		for (i = 0; i < MAX_SABERS; i++)
		{
			ent->client->saber[i].damageScale = ent->client->pers.training_stored_damageScale[i];
			ent->client->saber[i].damageScale2 = ent->client->pers.training_stored_damageScale2[i];
		}

		ent->client->pers.training_mode = qfalse;

		trap->SendServerCommand(ent->s.number, "print \"Training saber is ^1INACTIVE\n\"");
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/training <on|off>\n\"");
	}

	return;
}

// GalaxyRP fix: [Settings] plain-text status words for the ui_zyk_setting_N_value cvars used by the
// Settings panel in ingame_galaxyrp.menu. Those cvars (declared via XCVAR_DEF in ui_xcvar.h) were
// never written by any code anywhere in the repo -- they only ever displayed their static "0"
// default, which is meaningless to a player looking at the panel. This piggybacks on the existing
// zykmod server->cgame sync (see the content string built in Cmd_GalaxyRpUi_f just below, and
// CG_ZykMod/ui_cvars_in_order in cg_servercmds.c) to push the exact same words Cmd_Settings_f already
// prints to console when run with no arguments, so the panel and the console command can never say
// different things. setting_number matches the player-facing /settings <N> numbering (see
// settings_number_to_bit in Cmd_Settings_f above), not the underlying player_settings bit position.
// Deliberately NOT sharing code with Cmd_Settings_f's own status-line block -- that block is already
// correct and tested, and duplicating a handful of comparisons here is lower risk than refactoring it.
void zyk_setting_status_text(gentity_t *ent, int setting_number, char *out, int out_size) {
	switch (setting_number) {
		case 2: // Allow Force Powers from allies
			Q_strncpyz(out, (ent->client->pers.player_settings & (1 << 6)) ? "OFF" : "ON", out_size);
			break;
		case 3: // Starting Single Saber Style (multi-bit, spans bits 26-29)
			if (ent->client->pers.player_settings & (1 << 26))
				Q_strncpyz(out, "Yellow", out_size);
			else if (ent->client->pers.player_settings & (1 << 27))
				Q_strncpyz(out, "Red", out_size);
			else if (ent->client->pers.player_settings & (1 << 28))
				Q_strncpyz(out, "Desann", out_size);
			else if (ent->client->pers.player_settings & (1 << 29))
				Q_strncpyz(out, "Tavion", out_size);
			else
				Q_strncpyz(out, "Blue", out_size);
			break;
		case 4: // Allow Screen Message
			Q_strncpyz(out, (ent->client->pers.player_settings & (1 << 9)) ? "OFF" : "ON", out_size);
			break;
		case 5: // Use healing force only at allied players
			Q_strncpyz(out, (ent->client->pers.player_settings & (1 << 10)) ? "OFF" : "ON", out_size);
			break;
		case 6: // Start With Saber
			Q_strncpyz(out, (ent->client->pers.player_settings & (1 << 11)) ? "OFF" : "ON", out_size);
			break;
		case 7: // Admin Protect
			Q_strncpyz(out, (ent->client->pers.player_settings & (1 << 13)) ? "OFF" : "ON", out_size);
			break;
		default:
			Q_strncpyz(out, "", out_size);
			break;
	}
}

void Cmd_GalaxyRpUi_f(gentity_t* ent) {
	// zyk: sends info to the client-side menu if player has the client-side plugin
	char userinfo[MAX_INFO_STRING];
	char modelname[MAX_STRING_CHARS];
	char saber1Model[MAX_STRING_CHARS];
	char saber2Model[MAX_STRING_CHARS];
	int clientNum = ClientNumberFromString(ent, ent->client->pers.netname, qfalse);

	trap->GetUserinfo(clientNum, userinfo, sizeof(userinfo));

	//GalaxyRP (Alex): [User Info] We grab values from the current user info, so that we can set ui cvars based on that. Otherwise, when you use the ui, there's strange behavior.
	Q_strncpyz(modelname, Info_ValueForKey(userinfo, "model"), sizeof(modelname));
	Q_strncpyz(saber1Model, Info_ValueForKey(userinfo, "saber1"), sizeof(saber1Model));
	Q_strncpyz(saber2Model, Info_ValueForKey(userinfo, "saber2"), sizeof(saber2Model));

	// GalaxyRP: [Profile UI] tell the client whether it is currently logged into an account, so the
	// Profile menu can gate the Force icon and the Character Information/Skills/Characters/Shop/Games
	// sections on login state (see CG_LoggedInUpdate_f). Sent unconditionally and before the NOGUID
	// return below, so a logged-out player (who by definition has no GUID-backed account session)
	// still gets a fresh "0" every time the menu is opened rather than being skipped entirely.
	trap->SendServerCommand(ent->s.number, va("supdateloggedin %i\n", ent->client->sess.loggedin));

	if (Q_stricmp(ent->client->pers.guid, "NOGUID") == 0)
	{
		return;
	}

	if (ent->client->sess.loggedin == qtrue)
	{
		char content[1024];
		int level = ent->client->pers.level;
		int xp = ent->client->pers.xp;
		int xpToLevel = check_xp(level);
		int skillpoints = ent->client->pers.skillpoints;
		int credits = ent->client->pers.credits;

		strcpy(content, "");

		strcpy(content, va("%s%s~%s~%s~%s~%d~%d/%d~%d~%d~%s~", content, ent->client->pers.netname, modelname, saber1Model, saber2Model, level, xp, xpToLevel, skillpoints, credits, ent->client->sess.rpgchar));

		for (int i = 0; i < ARRAY_LEN(skills); i++) {
			strcpy(content, va("%s%d/%d~", content, ent->client->pers.skill_levels[i], skills[i].max_level));
		}

		// GalaxyRP fix: [Settings] appended after the skills loop and before the packet is sent, in the
		// same fixed order as the 6 new entries added to ui_cvars_in_order[] in cg_servercmds.c
		// (ui_zyk_setting_6/8/9/10/11/13_value). See zyk_setting_status_text() above for what each word
		// means and why this piggybacks on the zykmod sync instead of its own command.
		{
			char setting_text[16];
			int setting_number;
			int settings_to_sync[] = { 2, 3, 4, 5, 6, 7 };

			for (int i = 0; i < ARRAY_LEN(settings_to_sync); i++) {
				setting_number = settings_to_sync[i];
				zyk_setting_status_text(ent, setting_number, setting_text, sizeof(setting_text));
				strcpy(content, va("%s%s~", content, setting_text));
			}
		}

		trap->SendServerCommand(ent->s.number, va("zykmod \"%s\"", content));
	}
	else
	{
		return;
	}
}

/*
==================
Cmd_ZykChars_f
==================
*/
void Cmd_ZykChars_f(gentity_t* ent) {
	// zyk: sends info to the client-side menu if player has the client-side plugin
	if (Q_stricmp(ent->client->pers.guid, "NOGUID") == 0)
	{
		return;
	}

	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	sqlite3_stmt* stmt = 0;
	char char_string[MAX_STRING_CHARS] = "";

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	select_character_list_for_ui(ent, db, zErrMsg, rc, stmt, char_string);

	trap->SendServerCommand(ent->s.number, va("zykchars \"%s\"", char_string));
	sqlite3_close(db);
	return;
}

/*
=================
ClientCommand
=================
*/

#define CMD_NOINTERMISSION		(1<<0)
#define CMD_CHEAT				(1<<1)
#define CMD_ALIVE				(1<<2)
#define CMD_LOGGEDIN			(1<<3) // zyk: player must be logged in his account
#define CMD_RPG					(1<<4) // zyk: player must be in RPG Mode

typedef struct command_s {
	const char	*name;
	void		(*func)(gentity_t *ent);
	int			flags;
} command_t;

int cmdcmp( const void *a, const void *b ) {
	return Q_stricmp( (const char *)a, ((command_t*)b)->name );
}


/* This array MUST be sorted correctly by alphabetical name field */
command_t commands[] = {
	{ "addbot",				Cmd_AddBot_f,				0 },
	{ "admkick",			Cmd_AdmKick_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "callteamvote",		Cmd_CallTeamVote_f,			CMD_NOINTERMISSION },
	{ "callvote",			Cmd_CallVote_f,				CMD_NOINTERMISSION },
	{ "datetime",			Cmd_DateTime_f,				CMD_NOINTERMISSION },
	{ "debugBMove_Back",	Cmd_BotMoveBack_f,			CMD_CHEAT|CMD_ALIVE },
	{ "debugBMove_Forward",	Cmd_BotMoveForward_f,		CMD_CHEAT|CMD_ALIVE },
	{ "debugBMove_Left",	Cmd_BotMoveLeft_f,			CMD_CHEAT|CMD_ALIVE },
	{ "debugBMove_Right",	Cmd_BotMoveRight_f,			CMD_CHEAT|CMD_ALIVE },
	{ "debugBMove_Up",		Cmd_BotMoveUp_f,			CMD_CHEAT|CMD_ALIVE },
	{ "drop",				Cmd_Drop_f,					CMD_ALIVE|CMD_NOINTERMISSION },
	{ "follow",				Cmd_Follow_f,				CMD_NOINTERMISSION },
	{ "follownext",			Cmd_FollowNext_f,			CMD_NOINTERMISSION },
	{ "followprev",			Cmd_FollowPrev_f,			CMD_NOINTERMISSION },
	{ "forcechanged",		Cmd_ForceChanged_f,			0 },
	{ "updateforce",		Cmd_UpdateForce_f,			CMD_NOINTERMISSION },	// GalaxyRP: [Force] instant-apply from the in-game force power menu, logged-out players only
	{ "gc",					Cmd_GameCommand_f,			CMD_NOINTERMISSION },
	{ "give",				Cmd_Give_f,					CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "jetpack",			Cmd_Jetpack_f,				CMD_ALIVE|CMD_NOINTERMISSION },
	{ "kill",				Cmd_Kill_f,					CMD_ALIVE|CMD_NOINTERMISSION },
	{ "killother",			Cmd_KillOther_f,			CMD_NOINTERMISSION },
	{ "levelshot",			Cmd_LevelShot_f,			CMD_CHEAT|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "maplist",			Cmd_MapList_f,				CMD_NOINTERMISSION },
	{ "saber",				Cmd_Saber_f,				CMD_NOINTERMISSION },
	{ "saberblade",			Cmd_SaberBlade_f,			CMD_NOINTERMISSION },	// GalaxyRP: [Saber RGB]
	{ "sabercolor",			Cmd_SaberColor_f,			CMD_NOINTERMISSION },	// GalaxyRP: [Saber RGB]
	{ "updatesaber",		Cmd_UpdateSaber_f,			CMD_NOINTERMISSION },	// GalaxyRP: [Saber] instant-apply from the in-game saber menu, see saber/saberblade/sabercolor above
	{ "say",				Cmd_Say_f,					0 },
	{ "say_team",			Cmd_SayTeam_f,				0 },
	{ "score",				Cmd_Score_f,				0 },
	{ "settings",			Cmd_Settings_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "setviewpos",			Cmd_SetViewpos_f,			CMD_CHEAT|CMD_NOINTERMISSION },
	{ "shakescreen",		Cmd_ShakeScreen_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "siegeclass",			Cmd_SiegeClass_f,			CMD_NOINTERMISSION },
	{ "team",				Cmd_Team_f,					CMD_NOINTERMISSION },
	{ "teamvote",			Cmd_TeamVote_f,				CMD_NOINTERMISSION },
	{ "tell",				Cmd_Tell_f,					0 },
	{ "t_use",				Cmd_TargetUse_f,			CMD_CHEAT|CMD_ALIVE },
	{ "voice_cmd",			Cmd_VoiceCommand_f,			CMD_NOINTERMISSION },
	{ "vote",				Cmd_Vote_f,					CMD_NOINTERMISSION },
	/*
	=============
	GalaxyRP (Alex): [Commands] This is where GalaxyRP commands begin
	=============
	*/
	{ "admindown",			Cmd_AdminDown_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "adminlist",			Cmd_AdminList_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "adminup",			Cmd_AdminUp_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "admmap",				Cmd_AdmMap_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "anim",				Cmd_Emote_f,				CMD_ALIVE | CMD_NOINTERMISSION },
	{ "allyadd",			Cmd_AllyAdd_f,				CMD_NOINTERMISSION },
	{ "allychat",			Cmd_AllyChat_f,				CMD_NOINTERMISSION },
	{ "allylist",			Cmd_AllyList_f,				CMD_NOINTERMISSION },
	{ "allyremove",			Cmd_AllyRemove_f,			CMD_NOINTERMISSION },
	{ "attributes",			Cmd_Attributes_f,			CMD_LOGGEDIN },
	{ "buy",				Cmd_Buy_f,					CMD_RPG | CMD_ALIVE | CMD_NOINTERMISSION },
	//{ "callseller",			Cmd_CallSeller_f,			CMD_RPG | CMD_ALIVE | CMD_NOINTERMISSION },
	{ "changepassword",		Cmd_ChangePassword_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "char",				Cmd_Char_f,					CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "clientprint",		Cmd_ClientPrint_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "createcredits",		Cmd_CreditCreate_f,			CMD_RPG | CMD_NOINTERMISSION },
	{ "createitem",			Cmd_CreateItem_f,			CMD_LOGGEDIN},
	{ "duelarena",			Cmd_DuelArena_f,			CMD_LOGGEDIN | CMD_ALIVE | CMD_NOINTERMISSION },
	{ "duelboard",			Cmd_DuelBoard_f,			CMD_NOINTERMISSION },
	{ "duelmode",			Cmd_DuelMode_f,				CMD_ALIVE | CMD_NOINTERMISSION },
	{ "duelpause",			Cmd_DuelPause_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "dueltable",			Cmd_DuelTable_f,			CMD_NOINTERMISSION },
	{ "duelteam",			Cmd_DuelTeam_f,				CMD_NOINTERMISSION },
	{ "emote",				Cmd_Emote_f,				CMD_ALIVE | CMD_NOINTERMISSION },
	{ "entadd",				Cmd_EntAdd_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entdeletefile",		Cmd_EntDeleteFile_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entedit",			Cmd_EntEdit_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entitysystem",		Cmd_EntitySystem_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entlist",			Cmd_EntList_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entload",			Cmd_EntLoad_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entnear",			Cmd_EntNear_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entorigin",			Cmd_EntOrigin_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entremove",			Cmd_EntRemove_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entsave",			Cmd_EntSave_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entundo",			Cmd_EntUndo_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "ex",					Cmd_Examine_f,				CMD_LOGGEDIN },
	{ "examine",			Cmd_Examine_f,				CMD_LOGGEDIN },
	{ "flipcoin",			Cmd_FlipCoin_f,				0 },
	{ "givecredits",		Cmd_CreditGive_f,			CMD_RPG | CMD_NOINTERMISSION },
	{ "giveitem",			Cmd_GiveItem_f,				CMD_LOGGEDIN},
	{ "givexp",				Cmd_GiveXp_f,				CMD_LOGGEDIN},
	{ "god",				Cmd_God_f,					CMD_ALIVE | CMD_NOINTERMISSION },
	{ "helpup",				Cmd_Helpup_f,				CMD_ALIVE | CMD_NOINTERMISSION},
	{ "getup",				Cmd_Getup_f,				CMD_ALIVE | CMD_NOINTERMISSION},
	{ "ignore",				Cmd_Ignore_f,				CMD_NOINTERMISSION },
	{ "ignorelist",			Cmd_IgnoreList_f,			CMD_NOINTERMISSION },
	{ "inv",				Cmd_Inventory_f,			CMD_LOGGEDIN},
	{ "inventory",			Cmd_Inventory_f,			CMD_LOGGEDIN},
	{ "levelup",			Cmd_LevelGive_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "leveldown",			Cmd_LevelTake_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "list",				Cmd_ListAccount_f,			CMD_NOINTERMISSION },
	{ "listaccount",		Cmd_ListAccount_f,			CMD_NOINTERMISSION },
	{ "login",				Cmd_Login_F,				0 },
	{ "logout",				Cmd_LogoutAccount_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "meleemode",			Cmd_MeleeMode_f,			CMD_ALIVE | CMD_NOINTERMISSION },
	{ "modversion",			Cmd_ModVersion_f,			CMD_NOINTERMISSION },
	{ "new",				Cmd_Register_F,				CMD_NOINTERMISSION },
	{ "news",				Cmd_News_f,					0 },
	{ "newsadd",			Cmd_UpdateNews_f,					CMD_LOGGEDIN },
	{ "newschannels",		Cmd_NewsChannels_f,					0 },
	{ "newsremove",			Cmd_NewsRemove_f,					CMD_LOGGEDIN },
	{ "noclip",				Cmd_Noclip_f,				CMD_LOGGEDIN | CMD_ALIVE | CMD_NOINTERMISSION },
	{ "nofight",			Cmd_NoFight_f,				CMD_NOINTERMISSION },
	{ "notarget",			Cmd_Notarget_f,				CMD_ALIVE | CMD_NOINTERMISSION },
	{ "npc",				Cmd_NPC_f,					CMD_LOGGEDIN },
	{ "order",				Cmd_Order_f,				CMD_ALIVE | CMD_NOINTERMISSION },
	{ "paralyze",			Cmd_Paralyze_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "spawnplatform",		Cmd_SpawnPlatform_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "spawndummy",			Cmd_SpawnDummy_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	// GalaxyRP fix: [validation] this used to have no CMD_LOGGEDIN flag at all -- a connected but not
	// logged-in player could run /playsound. Adding it here reuses the same central "You must be logged
	// in" check (and message) every other logged-in-only command already goes through, rather than
	// duplicating that check inside Cmd_ZykSound_f itself.
	{ "playsound",			Cmd_ZykSound_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "playmusic",			Cmd_Music_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "players",			Cmd_Players_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "racemode",			Cmd_RaceMode_f,				CMD_ALIVE | CMD_NOINTERMISSION },
	{ "remap",				Cmd_Remap_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "remapdeletefile",	Cmd_RemapDeleteFile_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "remaplist",			Cmd_RemapList_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "remapload",			Cmd_RemapLoad_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "remapsave",			Cmd_RemapSave_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "removexp",			Cmd_RemoveXp_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "removepickups",		Cmd_RemovePickups_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "roll",				Cmd_Roll_f,					0 },
	{ "rpglmsmode",			Cmd_RpgLmsMode_f,			CMD_RPG | CMD_ALIVE | CMD_NOINTERMISSION },
	{ "rpglmstable",		Cmd_RpgLmsTable_f,			CMD_NOINTERMISSION },
	{ "scale",				Cmd_Scale_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "silence",			Cmd_Silence_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "skilldown",			Cmd_RpModeDown_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "skillup",			Cmd_RpModeUp_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "snipermode",			Cmd_SniperMode_f,			CMD_ALIVE | CMD_NOINTERMISSION },
	{ "snipertable",		Cmd_SniperTable_f,			CMD_NOINTERMISSION },
	{ "spendcredits",		Cmd_CreditSpend_f,			CMD_RPG | CMD_NOINTERMISSION },
	{ "stuff",				Cmd_Stuff_f,				CMD_RPG | CMD_NOINTERMISSION },
	{ "tele",				Cmd_Teleport_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "telemark",			Cmd_Telemark_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "teleport",			Cmd_Teleport_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "training",			Cmd_TrainingMode_f,			CMD_ALIVE | CMD_NOINTERMISSION },
	{ "trashitem",			Cmd_TrashItem_f,			CMD_LOGGEDIN },
	{ "where",				Cmd_Where_f,				CMD_NOINTERMISSION },
	// GalaxyRP fix: no CMD_ALIVE -- Cmd_GalaxyRpUi_f's supdateloggedin push is meant to reach the
	// client unconditionally (see its own comment above the send), including while spectating, so a
	// player who quit logged in and reconnected logged out/spectating gets corrected as soon as the
	// menu's "exec zykmod" fires on open, instead of being stuck showing the logged-in menu state
	// until they're alive again.
	{ "zykmod",				Cmd_GalaxyRpUi_f,			CMD_NOINTERMISSION },
	{ "zykchars",			Cmd_ZykChars_f,			CMD_ALIVE | CMD_NOINTERMISSION }
//	{ "meleearena",			Cmd_MeleeArena_f,			CMD_ALIVE|CMD_NOINTERMISSION },
//	{ "thedestroyer",		Cmd_TheDestroyer_f,			CMD_CHEAT|CMD_ALIVE|CMD_NOINTERMISSION },
//	{ "teamtask",			Cmd_TeamTask_f,				CMD_NOINTERMISSION },
//	{ "kylesmash",			TryGrapple,					0 },

};
static const size_t numCommands = ARRAY_LEN( commands );

void ClientCommand( int clientNum ) {
	gentity_t	*ent = NULL;
	char		cmd[MAX_TOKEN_CHARS] = {0};
	command_t	*command = NULL;

	ent = g_entities + clientNum;
	if ( !ent->client || ent->client->pers.connected != CON_CONNECTED ) {
		G_SecurityLogPrintf( "ClientCommand(%d) without an active connection\n", clientNum );
		return;		// not fully in game yet
	}

	trap->Argv( 0, cmd, sizeof( cmd ) );

	//rww - redirect bot commands
	if ( strstr( cmd, "bot_" ) && AcceptBotCommand( cmd, ent ) )
		return;
	//end rww

	command = (command_t *)Q_LinearSearch( cmd, commands, numCommands, sizeof( commands[0] ), cmdcmp );
	if ( !command )
	{
		trap->SendServerCommand( clientNum, va( "print \"Unknown command %s\n\"", cmd ) );
		return;
	}

	else if ( (command->flags & CMD_NOINTERMISSION)
		&& ( level.intermissionQueued || level.intermissiontime ) )
	{
		trap->SendServerCommand( clientNum, va( "print \"%s (%s)\n\"", G_GetStringEdString( "MP_SVGAME", "CANNOT_TASK_INTERMISSION" ), cmd ) );
		return;
	}

	else if ( (command->flags & CMD_CHEAT)
		&& !sv_cheats.integer )
	{
		trap->SendServerCommand( clientNum, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOCHEATS" ) ) );
		return;
	}

	else if ( (command->flags & CMD_ALIVE)
		&& (ent->health <= 0
			|| ent->client->tempSpectate >= level.time
			|| ent->client->sess.sessionTeam == TEAM_SPECTATOR) )
	{
		trap->SendServerCommand( clientNum, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "MUSTBEALIVE" ) ) );
		return;
	}

	else if ( (command->flags & CMD_LOGGEDIN)
		&& ent->client->sess.amrpgmode == 0 )
	{ // zyk: new condition
		trap->SendServerCommand( clientNum, "print \"You must be logged in\n\"" );
		return;
	}

	else if ( (command->flags & CMD_RPG)
		&& ent->client->sess.amrpgmode < 2 )
	{ // zyk: new condition
		trap->SendServerCommand( clientNum, "print \"You must be in RPG Mode\n\"" );
		return;
	}

	else if (Q_stricmp(cmd, "roll") == 0) {
		Cmd_Roll_f(ent);
	}

	else
		command->func( ent );
}
