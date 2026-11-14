#include "Object/HUD.h"
#include "Resource/Mappings/HUD.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
#include "EditorLive.h"
#include "Level.h"

// The HUD is not an object: it needs no slot and no place in the id table, the level's frame just draws it (HUD_Draw, called by GM_Level and GM_Ending). Its two sprites are
// plain Sst structures the draw queue points at.
// Where the lives counter goes in a widescreen picture: a sprite of its own in the top right corner. (The original has it in
// the bottom left, as part of the HUD's own sprite; the widescreen frames of that sprite leave it out.)
#define HUD_WIDE_FRAMES 4 // the HUD frames without the lives counter come after the four with it

// A split screen (Sonic 2's) puts the lives counter in the lower left of each view, where the original's picture has it, not in the corner of a wide picture
bool hud_lives_lower_left;
bool hud_player_two;
bool hud_hold_back;

// Whether the screen has one (a level, the ending): GM_Level and GM_Ending say so
bool hud_visible;

static Sst hud_sprite, hud_lives_sprite, hud_lives_tails_icon, hud_lives_tails_text;

// (Sonic 2's second view: the lives counter has Tails' icon, at this tile, and his name MILES)
#define HUD_TAILS_ICON_TILE 0x7E0
#define HUD_LIVES_ICON_OFFSET 0x10A // (the icon's tile in the HUD's own art)
#define HUD_MILES_TEXT_TILE 0x7E4   // (the name MILES, after the icon's four tiles)
#define HUD_LIVES_TEXT_OFFSET 0x10E // (the name's tile in the HUD's own art)

static bool HUD_IsWide(void) { return SCREEN_WIDTH > 320; }

void HUD_Draw(void) {
    if (!hud_visible || hud_hold_back || (editor_present && !editor_playtest))
        return; // (the level editor draws its own)

    Sst *self = &hud_sprite;
    self->x_pos.f.u = 0x90;
    self->y_pos.f.u = 0x108 + SCREEN_TALLADD2; // (the picture may have changed size)
    self->mappings = Mappings_HUD;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_HUD);
    self->render_flags = 0;
    self->priority = 0 * 0x80;

    // Determine frame. TIME flashes at 9:00 whether or not you have rings (the original only did it with no rings, which
    // the later games fixed), so this is applied in every build.
    uint8_t frame = 0;
    if (!(frame_count & 8)) {
        if (!rings)
            frame += 1; // Flash RINGS
        if (level_time.min == 9)
            frame += 2; // Flash TIME
    }
    const bool lives_apart = HUD_IsWide() || hud_lives_lower_left || hud_player_two; // (a split screen draws its lives counters as sprites of their own)
    self->mapping_frame = frame + (lives_apart ? HUD_WIDE_FRAMES : 0);
    Sst_Draw(self);

    if (!lives_apart)
        return;
    Sst *lives = &hud_lives_sprite;
    lives->mappings = Mappings_HUD;
    lives->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_HUD);
    lives->render_flags = 0;
    lives->priority = 0 * 0x80;
    lives->mapping_frame = 2 * HUD_WIDE_FRAMES; // just the lives
    lives->hidden_in_view = 0;
    if (hud_lives_lower_left) {
        lives->x_pos.f.u = 0x90;
        lives->y_pos.f.u = 0x148 + SCREEN_TALLADD2; // (the lives-only frame has its pieces at 0, the full frames at $40)
        lives->hidden_in_view = 2; // (the second view has Miles')
    } else if (hud_player_two && !HUD_IsWide()) {
        lives->x_pos.f.u = 0x90;
        lives->y_pos.f.u = 0x148 + SCREEN_TALLADD2;
    } else {
        lives->x_pos.f.u = (int16_t)(0x80 + SCREEN_WIDTH - 0x40);
        lives->y_pos.f.u = 0x88;
    }
    if (hud_lives_lower_left || hud_player_two) {
        // Miles' counter (the second view of a split screen, or the picture of the second player online): his icon and his name
        Sst *const tails[2] = { &hud_lives_tails_icon, &hud_lives_tails_text };
        for (int i = 0; i < 2; i++) {
            tails[i]->mappings = Mappings_HUD;
            tails[i]->render_flags = 0;
            tails[i]->priority = 0 * 0x80;
            tails[i]->x_pos.f.u = lives->x_pos.f.u;
            tails[i]->y_pos.f.u = lives->y_pos.f.u;
            tails[i]->hidden_in_view = hud_lives_lower_left ? 1 : 0;
            tails[i]->art_tile = TILE_MAP(0, 0, 0, 0, i == 0 ? HUD_TAILS_ICON_TILE - HUD_LIVES_ICON_OFFSET : HUD_MILES_TEXT_TILE - HUD_LIVES_TEXT_OFFSET);
            tails[i]->mapping_frame = (uint8_t)(2 * HUD_WIDE_FRAMES + 1 + i);
            Sst_Draw(tails[i]);
        }
        if (!hud_lives_lower_left)
            return; // (online: Miles' counter is the only one)
    }
    Sst_Draw(lives);
}
