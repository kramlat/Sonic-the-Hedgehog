#ifndef _OBJECT_HUD_H
#define _OBJECT_HUD_H

#include "Sst.h"


extern bool hud_lives_lower_left; // the lives counter of a wide picture goes in the lower left (a split screen)

extern bool hud_hold_back; // the title card has the picture covered: HUD_Draw draws nothing
extern bool hud_player_two; // the picture is the second player's (online): the lives counter has Miles' icon and name

// A split screen's second view has numbers of its own (Miles' score, rings and time): a tile bank that holds a copy of the HUD's art with his digits, which the second sprite of the HUD draws from (the first sprite is then not
// drawn in that view). `hud_second_rings_zero` and `hud_second_minutes_nine` are what make its RINGS and TIME words flash. NULL: one HUD for both views.
extern const tilebank_t *hud_second_bank;
extern bool hud_second_rings_zero, hud_second_minutes_nine;

extern bool hud_visible; // a level or the ending is on: HUD_Draw draws the HUD
void HUD_Draw(void);     // queues the HUD's sprites (it is not an object: no slot, no id)

#endif //_OBJECT_HUD_H
