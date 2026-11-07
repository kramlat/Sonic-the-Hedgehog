#include "test.h"

#include <string.h>

#include "Video.h"
#include "Viewport.h"
#include "Backend/VDP.h"

// The VDP on the GPU (engine/Backend/VDP.c's VDP_CaptureFrame and the Qt backend's shaders/vdp.comp): its picture of a frame is the CPU's, pixel for pixel. The frames are made of noise (every
// tile, colour, name table entry, line scroll and sprite at random) so that each rule is met somewhere: the flips, the priorities, the window, shadow/highlight, the split screens and their
// water, and a horizontal interrupt that changes the colours and the tiles half way down. Without a GPU to ask (no device, or one without compute shaders) the tests say so and pass.

static uint32_t noise = 1;

static uint32_t Noise(void) {
    noise = noise * 1103515245u + 12345u;
    return noise >> 8;
}

static sprite_t gpu_sprites[2][VIEWPORT_SPRITES + 1];

static void FillPlane(plane_t *plane) {
    for (unsigned i = 0; i < 64u * 32u; i++)
        Plane_Put(plane, i * 2, (uint16_t)Noise());
}

static void FillSprites(viewport_t *vp, sprite_t *table, int count) {
    for (int i = 0; i < count; i++) {
        sprite_t *s = &table[i];
        s->y = (uint16_t)(128 - 32 + Noise() % (SCREEN_HEIGHT + 64));
        s->x = (uint16_t)(128 - 32 + Noise() % (SCREEN_WIDTH + 64));
        s->size_link = (uint16_t)((Noise() & 0x0F00) | (i + 1 < count ? i + 1 : 0));
        s->pattern = (uint16_t)(Noise() % 0x900); // (some past the tile space's end: blank)
        const uint16_t word = (uint16_t)Noise();
        s->attr = (uint16_t)(((word & 0x8000) ? TILE_ATTR_PRIORITY : 0) | ((word & 0x0800) ? TILE_ATTR_X_FLIP : 0) | ((word & 0x1000) ? TILE_ATTR_Y_FLIP : 0) |
                             (((word >> 13) & 3) << TILE_ATTR_PALETTE_SHIFT));
        s->generation = 0;
    }
    vp->sprites = count > 0 ? table : NULL;
}

static void Prepare(uint32_t seed) {
    noise = seed;
    VDP_SetReal(0);
    VDP_SetSplitScreen(VDP_SPLIT_NONE);
    VDP_SetSplitWater(NULL, NULL, 0, 0);
    VDP_SetShadowHighlight(false);
    VDP_SetInterrupts(NULL, NULL);
    for (size_t i = 0; i < VRAM_SIZE; i++)
        VDP_TileSpace()[i] = (uint8_t)Noise();
    uint16_t cram[VDP_PALETTES * 16];
    for (size_t i = 0; i < sizeof(cram) / sizeof(cram[0]); i++)
        cram[i] = (uint16_t)(Noise() & 0x0EEE);
    VDP_SeekCRAM(0);
    VDP_WriteCRAM(cram, sizeof(cram) / sizeof(cram[0]));
    VDP_SetBackgroundColour((uint8_t)(Noise() % 64));
    Viewport_SetSize(SCREEN_WIDTH, SCREEN_HEIGHT, 64, 32);
    viewport_t *both[2] = { &screen1p, &screen2p };
    for (int v = 0; v < 2; v++) {
        viewport_t *vp = both[v];
        Viewport_UseOwnPlanes(vp);
        FillPlane(&vp->plane_a);
        FillPlane(&vp->plane_b);
        FillPlane(&vp->window);
        Viewport_ClearWindow(vp);
        for (size_t i = 0; i < vp->hscroll_bytes / sizeof(int16_t); i++)
            vp->hscroll[i] = (int16_t)Noise();
        vp->vsram = (vsram_t){ (int16_t)Noise(), (int16_t)Noise() };
        vp->palette = NULL;
        FillSprites(vp, gpu_sprites[v], 20 + (int)(Noise() % 60));
    }
    screen1p.hint_enable = false;
}

// The GPU's picture against the CPU's: how many pixels differ (-1: no GPU)
static int Different(void) {
    VDP_GpuFrame frame;
    const bool captured = VDP_CaptureFrame(&frame, true);
    CHECK(captured);
    if (!captured)
        return 0;
    int pitch;
    const uint32_t *cpu = VDP_GetFrame(&pitch, NULL);
    return VDP_GpuCompareFrame(&frame, cpu, pitch);
}

static int gpu_missing_said;

static void Expect(int different) {
    if (different < 0) {
        if (!gpu_missing_said++)
            printf("(no GPU to ask: not checked) ");
        return;
    }
    CHECK_EQ(different, 0);
}

static void VDPGpu_OneViewIsTheCPUs(void) {
    for (uint32_t seed = 1; seed <= 4; seed++) {
        Prepare(seed);
        Expect(Different());
    }
}

static void VDPGpu_TheWindowIsTheCPUs(void) {
    static const window_region_t regions[] = {
        { true, false, 96, 0, false, false }, { true, false, 200, 0, true, false }, { false, true, 0, 64, false, false }, { false, true, 0, 120, false, true },
    };
    for (size_t i = 0; i < sizeof(regions) / sizeof(regions[0]); i++) {
        Prepare(10 + (uint32_t)i);
        Viewport_SetWindow(&screen1p, regions[i]);
        Expect(Different());
    }
}

static void VDPGpu_ShadowAndHighlightAreTheCPUs(void) {
    for (uint32_t seed = 20; seed <= 22; seed++) {
        Prepare(seed);
        VDP_SetShadowHighlight(true);
        Expect(Different());
    }
    VDP_SetShadowHighlight(false);
}

static uint16_t water_dry[64], water_wet[64], second_palette[64];

static void VDPGpu_SideBySideIsTheCPUs(void) {
    Prepare(30);
    VDP_SetSplitScreen(VDP_SPLIT_SIDE);
    Expect(Different());
    for (int i = 0; i < 64; i++)
        second_palette[i] = (uint16_t)(Noise() & 0x0EEE);
    screen2p.palette = second_palette; // (the second view's own palette)
    Expect(Different());
    for (int i = 0; i < 64; i++) {
        water_dry[i] = (uint16_t)(Noise() & 0x0EEE);
        water_wet[i] = (uint16_t)(Noise() & 0x0EEE);
    }
    VDP_SetSplitWater(water_dry, water_wet, 60, 150);
    Expect(Different());
    VDP_SetSplitWater(NULL, NULL, 0, 0);
    VDP_SetSplitScreen(VDP_SPLIT_NONE);
    screen2p.palette = NULL;
}

static void VDPGpu_StackedIsTheCPUs(void) {
    Prepare(40);
    VDP_SetSplitScreen(VDP_SPLIT_STACKED);
    Expect(Different());
    for (int i = 0; i < 64; i++) {
        water_dry[i] = (uint16_t)(Noise() & 0x0EEE);
        water_wet[i] = (uint16_t)(Noise() & 0x0EEE);
    }
    VDP_SetSplitWater(water_dry, water_wet, -1, 100);
    Expect(Different());
    VDP_SetSplitWater(NULL, NULL, 0, 0);
    VDP_SetSplitScreen(VDP_SPLIT_NONE);
}

// A horizontal interrupt that changes a colour line, some tiles and the backdrop: the lines after it read what it left
static int interrupts;

static void ChangingInterrupt(void) {
    interrupts++;
    uint16_t line[16];
    for (int i = 0; i < 16; i++)
        line[i] = (uint16_t)(Noise() & 0x0EEE);
    VDP_SeekCRAM(16 * (size_t)(interrupts & 3));
    VDP_WriteCRAM(line, 16);
    if (interrupts % 3 == 0)
        for (size_t i = 0; i < 0x800; i++)
            VDP_TileSpace()[(Noise() % (VRAM_SIZE / 32)) * 32 + (i & 31)] = (uint8_t)Noise();
    VDP_SetBackgroundColour((uint8_t)(Noise() % 64));
}

static void VDPGpu_HorizontalInterruptsAreTheCPUs(void) {
    Prepare(50);
    interrupts = 0;
    VDP_SetInterrupts(NULL, ChangingInterrupt);
    screen1p.hint_counter = 37;
    screen1p.hint_enable = true;
    Expect(Different());
    CHECK(interrupts > 3);
    screen1p.hint_enable = false;
    VDP_SetInterrupts(NULL, NULL);
}

static void VDPGpu_TheRealReadingsAreLeftToTheCPU(void) {
    Prepare(60);
    VDP_SetReal(VDP_REAL_PLANES);
    VDP_GpuFrame frame;
    CHECK(!VDP_CaptureFrame(&frame, true)); // (drawn by the CPU)
    VDP_SetReal(0);
}

void RegisterVDPGpuTests(void) {
    RUN_TEST(VDPGpu_OneViewIsTheCPUs);
    RUN_TEST(VDPGpu_TheWindowIsTheCPUs);
    RUN_TEST(VDPGpu_ShadowAndHighlightAreTheCPUs);
    RUN_TEST(VDPGpu_SideBySideIsTheCPUs);
    RUN_TEST(VDPGpu_StackedIsTheCPUs);
    RUN_TEST(VDPGpu_HorizontalInterruptsAreTheCPUs);
    RUN_TEST(VDPGpu_TheRealReadingsAreLeftToTheCPU);
}
