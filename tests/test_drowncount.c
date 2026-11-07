#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/DrownCount.h"
#include "Video.h"
#include "SstLoad.h"
#include "Game.h"
#include "Object/Sonic.h"

// LZ's drowning countdown: once the air is down to 12 seconds, number bubbles (5, 4, 3, 2, 1, 0) rise from Sonic's mouth, then stick to
// the screen and flash.

static void DrownCount_NumbersAreShownAndStayShown(void) {
    level_id = LEVEL_ID(ZoneId_LZ, 0);
    Sst_Reset();
    memset(player, 0, sizeof(Sst));
    player->code = sst_object_codes[ObjId_Sonic];
    player->routine = 2;
    player->status |= SST_PSTATUS_UNDERWATER;
    player->x_pos.f.u = 0x1200;
    player->y_pos.f.u = 0x400;
    scrpos_x.f.u = 0x1200 - 160;
    scrpos_y.f.u = 0x400 - 112;
    wtr_pos1 = 0x300;
    air = 12;

    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {(int16_t)scrpos_x.f.u, (int16_t)scrpos_y.f.u, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);
    DrownCount_StartMaster();

    int shown_frames = 0, number_objects_seen = 0, max_air_seen = air;
    for (int frame = 0; frame < 60 * 14 && air > 0 && air != 0xFFFF; frame++) {
        ExecuteObjects();
        BuildSprites(NULL);
        for (int i = 0; i < SST_COUNT; i++) {
            Sst *o = &sst[i];
            if (o->code == NULL || o->code != sst_object_codes[ObjId_DrownCount] || (o->subtype & DROWNCOUNT_MASTER_BIT))
                continue;
            if (o->routine == 0xC || o->routine == 0xE) { // a number, stuck to the screen
                number_objects_seen++;
                if (o->render_flags & SST_RENDER_ON_SCREEN)
                    shown_frames++;
            }
        }
    }
    CHECK(number_objects_seen > 0);
    CHECK(shown_frames > 0);
}

// The debug cheat does not save Sonic from dying; B in the hurt, dead and drowning states enters object placement mode, and its exit resets them (the final game's way)
static void DebugCheat_DiesButBEntersPlacementModeAndResetsOnExit(void) {
    level_id = LEVEL_ID(ZoneId_LZ, 0);
    Sst_Reset();
    memset(player, 0, sizeof(Sst));
    player->code = sst_object_codes[ObjId_Sonic];
    player->routine = 2;
    player->x_pos.f.u = 0x1200;
    player->y_pos.f.u = 0x400;
    debug_cheat = true;
    debug_use = 0;
    rings = 0;
    shield = 0;
    CHECK_EQ(HurtSonicFrom(player, 0x1300, false), -1);
    CHECK_EQ(player->routine, 6);
    jpad1_press1 = JPAD_B;
    Obj_Sonic(player);
    CHECK(debug_use != 0);
    jpad1_press1 = 0;
    Obj_Sonic(player);
    jpad1_press1 = JPAD_B;
    Obj_Sonic(player);
    CHECK_EQ(debug_use, 0);
    CHECK_EQ(player->routine, 2);

    // drowning: the exit gives him his breath back
    jpad1_press1 = 0;
    player->status |= SST_PSTATUS_UNDERWATER;
    scrpos_x.f.u = 0x1200 - 160;
    scrpos_y.f.u = 0x400 - 112;
    wtr_pos1 = 0x300;
    air = 0;
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {(int16_t)scrpos_x.f.u, (int16_t)scrpos_y.f.u, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);
    DrownCount_StartMaster();
    for (int f = 0; f < 70 && lock_multi != 0x81; f++)
        ExecuteObjects();
    CHECK_EQ(lock_multi, 0x81);
    CHECK_EQ(player->anim, SonAnimId_Drown);
    jpad1_press1 = JPAD_B;
    Obj_Sonic(player);
    CHECK(debug_use != 0);
    jpad1_press1 = 0;
    Obj_Sonic(player);
    jpad1_press1 = JPAD_B;
    Obj_Sonic(player);
    CHECK_EQ(debug_use, 0);
    CHECK_EQ(lock_multi, 0);
    CHECK_EQ(air, 30);
    CHECK_EQ(player->routine, 2);
    for (int f = 0; f < 0x100; f++)
        ExecuteObjects();
    CHECK(player->routine != 6); // (the drowning is over: no death)
    jpad1_press1 = 0;
    debug_cheat = false;
}

void RegisterDrownCountTests(void) {
    RUN_TEST(DebugCheat_DiesButBEntersPlacementModeAndResetsOnExit);
    RUN_TEST(DrownCount_NumbersAreShownAndStayShown);
}
