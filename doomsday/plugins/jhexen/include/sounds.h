/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */
// Sfx and music identifiers
// Generated with DED Manager 1.13

#ifndef __AUDIO_CONSTANTS_H__
#define __AUDIO_CONSTANTS_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

// Sounds.
typedef enum {
	SFX_NONE,					   // 000
	SFX_PLAYER_FIGHTER_NORMAL_DEATH,	// 001
	SFX_PLAYER_FIGHTER_CRAZY_DEATH,	// 002
	SFX_PLAYER_FIGHTER_EXTREME1_DEATH,	// 003
	SFX_PLAYER_FIGHTER_EXTREME2_DEATH,	// 004
	SFX_PLAYER_FIGHTER_EXTREME3_DEATH,	// 005
	SFX_PLAYER_FIGHTER_BURN_DEATH, // 006
	SFX_PLAYER_CLERIC_NORMAL_DEATH,	// 007
	SFX_PLAYER_CLERIC_CRAZY_DEATH, // 008
	SFX_PLAYER_CLERIC_EXTREME1_DEATH,	// 009
	SFX_PLAYER_CLERIC_EXTREME2_DEATH,	// 010
	SFX_PLAYER_CLERIC_EXTREME3_DEATH,	// 011
	SFX_PLAYER_CLERIC_BURN_DEATH,  // 012
	SFX_PLAYER_MAGE_NORMAL_DEATH,  // 013
	SFX_PLAYER_MAGE_CRAZY_DEATH,   // 014
	SFX_PLAYER_MAGE_EXTREME1_DEATH,	// 015
	SFX_PLAYER_MAGE_EXTREME2_DEATH,	// 016
	SFX_PLAYER_MAGE_EXTREME3_DEATH,	// 017
	SFX_PLAYER_MAGE_BURN_DEATH,	   // 018
	SFX_PLAYER_FIGHTER_PAIN,	   // 019
	SFX_PLAYER_CLERIC_PAIN,		   // 020
	SFX_PLAYER_MAGE_PAIN,		   // 021
	SFX_PLAYER_FIGHTER_GRUNT,	   // 022
	SFX_PLAYER_CLERIC_GRUNT,	   // 023
	SFX_PLAYER_MAGE_GRUNT,		   // 024
	SFX_PLAYER_LAND,			   // 025
	SFX_PLAYER_POISONCOUGH,		   // 026
	SFX_PLAYER_FIGHTER_FALLING_SCREAM,	// 027
	SFX_PLAYER_CLERIC_FALLING_SCREAM,	// 028
	SFX_PLAYER_MAGE_FALLING_SCREAM,	// 029
	SFX_PLAYER_FALLING_SPLAT,	   // 030
	SFX_PLAYER_FIGHTER_FAILED_USE, // 031
	SFX_PLAYER_CLERIC_FAILED_USE,  // 032
	SFX_PLAYER_MAGE_FAILED_USE,	   // 033
	SFX_PLATFORM_START,			   // 034
	SFX_PLATFORM_STARTMETAL,	   // 035
	SFX_PLATFORM_STOP,			   // 036
	SFX_STONE_MOVE,				   // 037
	SFX_METAL_MOVE,				   // 038
	SFX_DOOR_OPEN,				   // 039
	SFX_DOOR_LOCKED,			   // 040
	SFX_DOOR_METAL_OPEN,		   // 041
	SFX_DOOR_METAL_CLOSE,		   // 042
	SFX_DOOR_LIGHT_CLOSE,		   // 043
	SFX_DOOR_HEAVY_CLOSE,		   // 044
	SFX_DOOR_CREAK,				   // 045
	SFX_PICKUP_WEAPON,			   // 046
	SFX_PICKUP_ARTIFACT,		   // 047
	SFX_PICKUP_KEY,				   // 048
	SFX_PICKUP_ITEM,			   // 049
	SFX_PICKUP_PIECE,			   // 050
	SFX_WEAPON_BUILD,			   // 051
	SFX_ARTIFACT_USE,			   // 052
	SFX_ARTIFACT_BLAST,			   // 053
	SFX_TELEPORT,				   // 054
	SFX_THUNDER_CRASH,			   // 055
	SFX_FIGHTER_PUNCH_MISS,		   // 056
	SFX_FIGHTER_PUNCH_HITTHING,	   // 057
	SFX_FIGHTER_PUNCH_HITWALL,	   // 058
	SFX_FIGHTER_GRUNT,			   // 059
	SFX_FIGHTER_AXE_HITTHING,	   // 060
	SFX_FIGHTER_HAMMER_MISS,	   // 061
	SFX_FIGHTER_HAMMER_HITTHING,   // 062
	SFX_FIGHTER_HAMMER_HITWALL,	   // 063
	SFX_FIGHTER_HAMMER_CONTINUOUS, // 064
	SFX_FIGHTER_HAMMER_EXPLODE,	   // 065
	SFX_FIGHTER_SWORD_FIRE,		   // 066
	SFX_FIGHTER_SWORD_EXPLODE,	   // 067
	SFX_CLERIC_CSTAFF_FIRE,		   // 068
	SFX_CLERIC_CSTAFF_EXPLODE,	   // 069
	SFX_CLERIC_CSTAFF_HITTHING,	   // 070
	SFX_CLERIC_FLAME_FIRE,		   // 071
	SFX_CLERIC_FLAME_EXPLODE,	   // 072
	SFX_CLERIC_FLAME_CIRCLE,	   // 073
	SFX_MAGE_WAND_FIRE,			   // 074
	SFX_MAGE_LIGHTNING_FIRE,	   // 075
	SFX_MAGE_LIGHTNING_ZAP,		   // 076
	SFX_MAGE_LIGHTNING_CONTINUOUS, // 077
	SFX_MAGE_LIGHTNING_READY,	   // 078
	SFX_MAGE_SHARDS_FIRE,		   // 079
	SFX_MAGE_SHARDS_EXPLODE,	   // 080
	SFX_MAGE_STAFF_FIRE,		   // 081
	SFX_MAGE_STAFF_EXPLODE,		   // 082
	SFX_SWITCH1,				   // 083
	SFX_SWITCH2,				   // 084
	SFX_SERPENT_SIGHT,			   // 085
	SFX_SERPENT_ACTIVE,			   // 086
	SFX_SERPENT_PAIN,			   // 087
	SFX_SERPENT_ATTACK,			   // 088
	SFX_SERPENT_MELEEHIT,		   // 089
	SFX_SERPENT_DEATH,			   // 090
	SFX_SERPENT_BIRTH,			   // 091
	SFX_SERPENTFX_CONTINUOUS,	   // 092
	SFX_SERPENTFX_HIT,			   // 093
	SFX_POTTERY_EXPLODE,		   // 094
	SFX_DRIP,					   // 095
	SFX_CENTAUR_SIGHT,			   // 096
	SFX_CENTAUR_ACTIVE,			   // 097
	SFX_CENTAUR_PAIN,			   // 098
	SFX_CENTAUR_ATTACK,			   // 099
	SFX_CENTAUR_DEATH,			   // 100
	SFX_CENTAURLEADER_ATTACK,	   // 101
	SFX_CENTAUR_MISSILE_EXPLODE,   // 102
	SFX_WIND,					   // 103
	SFX_BISHOP_SIGHT,			   // 104
	SFX_BISHOP_ACTIVE,			   // 105
	SFX_BISHOP_PAIN,			   // 106
	SFX_BISHOP_ATTACK,			   // 107
	SFX_BISHOP_DEATH,			   // 108
	SFX_BISHOP_MISSILE_EXPLODE,	   // 109
	SFX_BISHOP_BLUR,			   // 110
	SFX_DEMON_SIGHT,			   // 111
	SFX_DEMON_ACTIVE,			   // 112
	SFX_DEMON_PAIN,				   // 113
	SFX_DEMON_ATTACK,			   // 114
	SFX_DEMON_MISSILE_FIRE,		   // 115
	SFX_DEMON_MISSILE_EXPLODE,	   // 116
	SFX_DEMON_DEATH,			   // 117
	SFX_WRAITH_SIGHT,			   // 118
	SFX_WRAITH_ACTIVE,			   // 119
	SFX_WRAITH_PAIN,			   // 120
	SFX_WRAITH_ATTACK,			   // 121
	SFX_WRAITH_MISSILE_FIRE,	   // 122
	SFX_WRAITH_MISSILE_EXPLODE,	   // 123
	SFX_WRAITH_DEATH,			   // 124
	SFX_PIG_ACTIVE1,			   // 125
	SFX_PIG_ACTIVE2,			   // 126
	SFX_PIG_PAIN,				   // 127
	SFX_PIG_ATTACK,				   // 128
	SFX_PIG_DEATH,				   // 129
	SFX_MAULATOR_SIGHT,			   // 130
	SFX_MAULATOR_ACTIVE,		   // 131
	SFX_MAULATOR_PAIN,			   // 132
	SFX_MAULATOR_HAMMER_SWING,	   // 133
	SFX_MAULATOR_HAMMER_HIT,	   // 134
	SFX_MAULATOR_MISSILE_HIT,	   // 135
	SFX_MAULATOR_DEATH,			   // 136
	SFX_FREEZE_DEATH,			   // 137
	SFX_FREEZE_SHATTER,			   // 138
	SFX_ETTIN_SIGHT,			   // 139
	SFX_ETTIN_ACTIVE,			   // 140
	SFX_ETTIN_PAIN,				   // 141
	SFX_ETTIN_ATTACK,			   // 142
	SFX_ETTIN_DEATH,			   // 143
	SFX_FIRED_SPAWN,			   // 144
	SFX_FIRED_ACTIVE,			   // 145
	SFX_FIRED_PAIN,				   // 146
	SFX_FIRED_ATTACK,			   // 147
	SFX_FIRED_MISSILE_HIT,		   // 148
	SFX_FIRED_DEATH,			   // 149
	SFX_ICEGUY_SIGHT,			   // 150
	SFX_ICEGUY_ACTIVE,			   // 151
	SFX_ICEGUY_ATTACK,			   // 152
	SFX_ICEGUY_FX_EXPLODE,		   // 153
	SFX_SORCERER_SIGHT,			   // 154
	SFX_SORCERER_ACTIVE,		   // 155
	SFX_SORCERER_PAIN,			   // 156
	SFX_SORCERER_SPELLCAST,		   // 157
	SFX_SORCERER_BALLWOOSH,		   // 158
	SFX_SORCERER_DEATHSCREAM,	   // 159
	SFX_SORCERER_BISHOPSPAWN,	   // 160
	SFX_SORCERER_BALLPOP,		   // 161
	SFX_SORCERER_BALLBOUNCE,	   // 162
	SFX_SORCERER_BALLEXPLODE,	   // 163
	SFX_SORCERER_BIGBALLEXPLODE,   // 164
	SFX_SORCERER_HEADSCREAM,	   // 165
	SFX_DRAGON_SIGHT,			   // 166
	SFX_DRAGON_ACTIVE,			   // 167
	SFX_DRAGON_WINGFLAP,		   // 168
	SFX_DRAGON_ATTACK,			   // 169
	SFX_DRAGON_PAIN,			   // 170
	SFX_DRAGON_DEATH,			   // 171
	SFX_DRAGON_FIREBALL_EXPLODE,   // 172
	SFX_KORAX_SIGHT,			   // 173
	SFX_KORAX_ACTIVE,			   // 174
	SFX_KORAX_PAIN,				   // 175
	SFX_KORAX_ATTACK,			   // 176
	SFX_KORAX_COMMAND,			   // 177
	SFX_KORAX_DEATH,			   // 178
	SFX_KORAX_STEP,				   // 179
	SFX_THRUSTSPIKE_RAISE,		   // 180
	SFX_THRUSTSPIKE_LOWER,		   // 181
	SFX_STAINEDGLASS_SHATTER,	   // 182
	SFX_FLECHETTE_BOUNCE,		   // 183
	SFX_FLECHETTE_EXPLODE,		   // 184
	SFX_LAVA_MOVE,				   // 185
	SFX_WATER_MOVE,				   // 186
	SFX_ICE_STARTMOVE,			   // 187
	SFX_EARTH_STARTMOVE,		   // 188
	SFX_WATER_SPLASH,			   // 189
	SFX_LAVA_SIZZLE,			   // 190
	SFX_SLUDGE_GLOOP,			   // 191
	SFX_CHOLY_FIRE,				   // 192
	SFX_SPIRIT_ACTIVE,			   // 193
	SFX_SPIRIT_ATTACK,			   // 194
	SFX_SPIRIT_DIE,				   // 195
	SFX_VALVE_TURN,				   // 196
	SFX_ROPE_PULL,				   // 197
	SFX_FLY_BUZZ,				   // 198
	SFX_IGNITE,					   // 199
	SFX_PUZZLE_SUCCESS,			   // 200
	SFX_PUZZLE_FAIL_FIGHTER,	   // 201
	SFX_PUZZLE_FAIL_CLERIC,		   // 202
	SFX_PUZZLE_FAIL_MAGE,		   // 203
	SFX_EARTHQUAKE,				   // 204
	SFX_BELLRING,				   // 205
	SFX_TREE_BREAK,				   // 206
	SFX_TREE_EXPLODE,			   // 207
	SFX_SUITOFARMOR_BREAK,		   // 208
	SFX_POISONSHROOM_PAIN,		   // 209
	SFX_POISONSHROOM_DEATH,		   // 210
	SFX_AMBIENT1,				   // 211
	SFX_AMBIENT2,				   // 212
	SFX_AMBIENT3,				   // 213
	SFX_AMBIENT4,				   // 214
	SFX_AMBIENT5,				   // 215
	SFX_AMBIENT6,				   // 216
	SFX_AMBIENT7,				   // 217
	SFX_AMBIENT8,				   // 218
	SFX_AMBIENT9,				   // 219
	SFX_AMBIENT10,				   // 220
	SFX_AMBIENT11,				   // 221
	SFX_AMBIENT12,				   // 222
	SFX_AMBIENT13,				   // 223
	SFX_AMBIENT14,				   // 224
	SFX_AMBIENT15,				   // 225
	SFX_STARTUP_TICK,			   // 226
	SFX_SWITCH_OTHERLEVEL,		   // 227
	SFX_RESPAWN,				   // 228
	SFX_KORAX_VOICE_1,			   // 229
	SFX_KORAX_VOICE_2,			   // 230
	SFX_KORAX_VOICE_3,			   // 231
	SFX_KORAX_VOICE_4,			   // 232
	SFX_KORAX_VOICE_5,			   // 233
	SFX_KORAX_VOICE_6,			   // 234
	SFX_KORAX_VOICE_7,			   // 235
	SFX_KORAX_VOICE_8,			   // 236
	SFX_KORAX_VOICE_9,			   // 237
	SFX_BAT_SCREAM,				   // 238
	SFX_CHAT,					   // 239
	SFX_MENU_MOVE,				   // 240
	SFX_CLOCK_TICK,				   // 241
	SFX_FIREBALL,				   // 242
	SFX_PUPPYBEAT,				   // 243
	SFX_MYSTICINCANT,			   // 244
	NUMSFX						   // 245
} sfxenum_t;

// Music.
typedef enum {
	NUMMUSIC					   // 000
} musicenum_t;

#endif
