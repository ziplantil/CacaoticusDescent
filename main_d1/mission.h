/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#pragma once

#include "misc/types.h"

#define MAX_MISSIONS 						100
#define MAX_LEVELS_PER_MISSION			30
#define MAX_SECRET_LEVELS_PER_MISSION	5
#define MISSION_NAME_LEN 					21

//values for built-in mission

#define BIM_LAST_LEVEL			27
#define BIM_LAST_SECRET_LEVEL	-3
#define BIM_BRIEFING_FILE		"briefing.tex"
#define BIM_ENDING_FILE			"endreg.tex"

 //mission list entry
typedef struct mle {
	char	filename[9];							//filename without extension
	char	mission_name[MISSION_NAME_LEN + 1];
	uint8_t	anarchy_only_flag;					//if true, mission is anarchy only
} mle;

extern mle Mission_list[MAX_MISSIONS];

extern int Current_mission_num;
extern char* Current_mission_filename, * Current_mission_longname;

//arrays of name of the level files
extern char Level_names[MAX_LEVELS_PER_MISSION][13];
extern char Secret_level_names[MAX_SECRET_LEVELS_PER_MISSION][13];

//fills in the global list of missions.  Returns the number of missions
//in the list.  If anarchy_mode set, don't include non-anarchy levels.
//if there is only one mission, this function will call load_mission on it.
int build_mission_list(int anarchy_mode);

//loads the specfied mission from the mission list.  build_mission_list()
//must have been called.  If build_mission_list() returns 0, this function
//does not need to be called.  Returns true if mission loaded ok, else false.
int load_mission(int mission_num);

//loads the named mission if exists.
//Returns true if mission loaded ok, else false.
int load_mission_by_name(char* mission_name);
