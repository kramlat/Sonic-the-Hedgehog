#include "test.h"

#include <string.h>

#include "EditorLive.h"
#include "EditorTerrain.h"
#include "LevelData.h"

// The terrain editor's list operations (engine/EditorTerrain.c): copy and paste, insert, delete and move a tile, block or chunk, with what names them rewritten. Each test builds a small level in which every
// tile, block and chunk is told apart by its content, runs an operation and checks that everything that pointed at a thing still points at the same thing.

#define TILES 8
#define BLOCKS 6
#define CHUNKS 5

static uint8_t art[0x8000];

// Tile t is 32 bytes of t; block b has the tiles (b, b+1, b, b+2) mod TILES with its own flip bits; chunk c's cell k names block (c + k) mod BLOCKS (0 on the first cell, the blank one) with solidity bits;
// the layout is the chunks in turn
static void Build(void) {
    memset(art, 0, sizeof(art));
    for (int t = 0; t < TILES; t++)
        memset(art + t * 32, t, 32);
    memset(level_map16, 0, 8 * BLOCKS);
    for (int b = 1; b < BLOCKS; b++)
        for (int k = 0; k < 4; k++) {
            const int tile = b == 0 ? 0 : 1 + (b + (k == 3 ? 2 : k & 2 ? 0 : k)) % (TILES - 1);
            const uint16_t w = (uint16_t)(tile | (b & 1 ? 0x0800 : 0) | ((b & 3) << 13));
            level_map16[b * 8 + k * 2] = (uint8_t)(w >> 8);
            level_map16[b * 8 + k * 2 + 1] = (uint8_t)w;
        }
    memset(level_map128, 0, 128 * CHUNKS);
    for (int c = 1; c < CHUNKS; c++)
        for (int k = 0; k < 64; k++) {
            const int block = k == 0 ? 0 : 1 + (c + k) % (BLOCKS - 1);
            const uint16_t w = (uint16_t)(block | (k & 1 ? 0x0400 : 0) | 0x1000 | 0x8000);
            level_map128[c * 128 + k * 2] = (uint8_t)(w >> 8);
            level_map128[c * 128 + k * 2 + 1] = (uint8_t)w;
        }
    for (int b = 0; b < 0x400; b++) {
        coll_index[0][b] = (uint8_t)(b < BLOCKS ? 10 + b : 0);
        coll_index[1][b] = (uint8_t)(b < BLOCKS ? 20 + b : 0);
    }
    Editor_NoteArt(art, TILES * 32);
    Editor_NoteArtCapacity(sizeof(art));
    Editor_NoteBlocks(8 * BLOCKS);
    Editor_NoteChunks(128 * CHUNKS);
    Editor_NoteSolids(0, BLOCKS);
    Editor_NoteSolids(1, BLOCKS);
    size_t bytes;
    uint8_t *layout = Editor_Layout(&bytes);
    memset(layout, 0, bytes);
    for (int i = 0; i < 40; i++)
        layout[i] = (uint8_t)(i % CHUNKS); // (some cells are chunk 0, the empty one)
}

static uint16_t Word(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

// What chunk `c` is, told by what it points at all the way down: the tile content (the byte its 32 bytes hold) of each of the cells' blocks' four tiles, with the flips and the curves, as a number
static uint32_t Fingerprint(int chunk) {
    size_t bytes;
    const uint8_t *chunks = Editor_Chunks(&bytes), *blocks = Editor_Blocks(&bytes), *tiles = Editor_Art(&bytes);
    uint32_t h = 2166136261u;
    for (int k = 0; k < 64; k++) {
        const uint16_t cw = Word(chunks + chunk * 128 + k * 2);
        const int block = cw & 0x3FF;
        h = (h ^ (cw & 0xFC00)) * 16777619u;
        h = (h ^ coll_index[0][block]) * 16777619u;
        h = (h ^ coll_index[1][block]) * 16777619u;
        for (int q = 0; q < 4; q++) {
            const uint16_t bw = Word(blocks + block * 8 + q * 2);
            h = (h ^ (bw & 0xF800)) * 16777619u;
            h = (h ^ tiles[(bw & 0x7FF) * 32]) * 16777619u;
        }
    }
    return h;
}

// The layout's picture of the level: for each cell, the fingerprint of its chunk (0 for the empty one)
static uint32_t LayoutHash(void) {
    size_t bytes;
    const uint8_t *layout = Editor_Layout(&bytes);
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < bytes; i++)
        h = (h ^ (layout[i] ? Fingerprint(layout[i]) : 0)) * 16777619u;
    return h;
}

static void EditorTerrain_ReportsTheCountsAndLimits(void) {
    Build();
    CHECK_EQ((long long)EditorTerrain_Count(EDITOR_TERRAIN_TILE), TILES);
    CHECK_EQ((long long)EditorTerrain_Count(EDITOR_TERRAIN_BLOCK), BLOCKS);
    CHECK_EQ((long long)EditorTerrain_Count(EDITOR_TERRAIN_CHUNK), CHUNKS);
    CHECK_EQ((long long)EditorTerrain_Capacity(EDITOR_TERRAIN_BLOCK), 0x400);
    CHECK_EQ((long long)EditorTerrain_Capacity(EDITOR_TERRAIN_CHUNK), 0x100);
    CHECK_EQ((long long)EditorTerrain_Capacity(EDITOR_TERRAIN_TILE), 0x400); // (the buffer holds 1024 tiles)
}

static void EditorTerrain_MovingATileKeepsEveryBlocksPicture(void) {
    Build();
    uint32_t before[CHUNKS];
    for (int c = 1; c < CHUNKS; c++)
        before[c] = Fingerprint(c);
    const uint32_t layout = LayoutHash();
    CHECK(EditorTerrain_Move(EDITOR_TERRAIN_TILE, 2, 6) == NULL);
    CHECK_EQ(art[6 * 32], 2); // (tile 2 is number 6 now)
    CHECK_EQ(art[2 * 32], 3); // (the ones between moved down)
    for (int c = 1; c < CHUNKS; c++)
        CHECK(Fingerprint(c) == before[c]);
    CHECK(LayoutHash() == layout);
    CHECK_EQ(EditorTerrain_Mapped(EDITOR_TERRAIN_TILE, 2), 6);
    CHECK_EQ(EditorTerrain_Mapped(EDITOR_TERRAIN_TILE, 4), 3);
    CHECK_EQ(EditorTerrain_Mapped(EDITOR_TERRAIN_TILE, 7), 7);
}

static void EditorTerrain_MovingABlockKeepsTheChunksAndTheCurves(void) {
    Build();
    uint32_t before[CHUNKS];
    for (int c = 1; c < CHUNKS; c++)
        before[c] = Fingerprint(c);
    CHECK(EditorTerrain_Move(EDITOR_TERRAIN_BLOCK, 5, 1) == NULL);
    CHECK_EQ(coll_index[0][1], 15); // (block 5's curve went with it)
    CHECK_EQ(coll_index[1][2], 21);
    for (int c = 1; c < CHUNKS; c++)
        CHECK(Fingerprint(c) == before[c]);
}

static void EditorTerrain_MovingAChunkKeepsTheLayout(void) {
    Build();
    const uint32_t layout = LayoutHash();
    CHECK(EditorTerrain_Move(EDITOR_TERRAIN_CHUNK, 1, 4) == NULL);
    CHECK(LayoutHash() == layout);
    size_t bytes;
    CHECK_EQ(Editor_Layout(&bytes)[1], 4); // (the cell that had chunk 1 has number 4 now)
    CHECK_EQ(Editor_Layout(&bytes)[0], 0); // (the empty chunk stays the empty one)
}

static void EditorTerrain_InsertingShiftsWhatComesAfter(void) {
    Build();
    uint32_t before[CHUNKS];
    for (int c = 1; c < CHUNKS; c++)
        before[c] = Fingerprint(c);
    const uint32_t layout = LayoutHash();
    CHECK(EditorTerrain_Insert(EDITOR_TERRAIN_TILE, 3) == NULL);
    CHECK_EQ((long long)EditorTerrain_Count(EDITOR_TERRAIN_TILE), TILES + 1);
    CHECK_EQ(art[3 * 32], 0); // (blank)
    CHECK_EQ(art[4 * 32], 3);
    CHECK(EditorTerrain_Insert(EDITOR_TERRAIN_BLOCK, 2) == NULL);
    CHECK_EQ(coll_index[0][2], 0);
    CHECK_EQ(coll_index[0][3], 12);
    CHECK(EditorTerrain_Insert(EDITOR_TERRAIN_CHUNK, 1) == NULL);
    CHECK_EQ((long long)EditorTerrain_Count(EDITOR_TERRAIN_CHUNK), CHUNKS + 1);
    for (int c = 1; c < CHUNKS; c++)
        CHECK(Fingerprint(c + 1) == before[c]);
    CHECK(LayoutHash() == layout);
}

static void EditorTerrain_InsertStopsAtTheLimit(void) {
    Build();
    while (EditorTerrain_Count(EDITOR_TERRAIN_CHUNK) < EditorTerrain_Capacity(EDITOR_TERRAIN_CHUNK))
        CHECK(EditorTerrain_Insert(EDITOR_TERRAIN_CHUNK, 1) == NULL);
    CHECK_EQ((long long)EditorTerrain_Count(EDITOR_TERRAIN_CHUNK), 0x100);
    CHECK(EditorTerrain_Insert(EDITOR_TERRAIN_CHUNK, 1) != NULL);
    CHECK_EQ((long long)EditorTerrain_Count(EDITOR_TERRAIN_CHUNK), 0x100);
}

static void EditorTerrain_DeletingPointsWhatNamedItAtTheBlankOne(void) {
    Build();
    // block 3 goes: the cells of the chunks that had it have block 0 (nothing); the blocks after it are one lower
    uint32_t keep = Fingerprint(1);
    (void)keep;
    CHECK(EditorTerrain_Delete(EDITOR_TERRAIN_BLOCK, 3) == NULL);
    CHECK_EQ((long long)EditorTerrain_Count(EDITOR_TERRAIN_BLOCK), BLOCKS - 1);
    CHECK_EQ(coll_index[0][3], 14); // (block 4's curve, one lower)
    size_t bytes;
    const uint8_t *chunks = Editor_Chunks(&bytes);
    int zeros = 0;
    for (int k = 0; k < 64; k++)
        if ((Word(chunks + 1 * 128 + k * 2) & 0x3FF) == 0)
            zeros++;
    CHECK(zeros >= 2); // (the first cell was the blank one already; the one that had block 3 is now)
    // chunk 2 goes: the layout's cells that had it are empty; chunk 3's are 2 now
    Build();
    CHECK(EditorTerrain_Delete(EDITOR_TERRAIN_CHUNK, 2) == NULL);
    const uint8_t *layout = Editor_Layout(&bytes);
    CHECK_EQ(layout[2], 0);
    CHECK_EQ(layout[3], 2);
}

static void EditorTerrain_DeleteNeedsTwoAndKeepsTheBlankOne(void) {
    Build();
    CHECK(EditorTerrain_Delete(EDITOR_TERRAIN_BLOCK, 0) != NULL);
    CHECK(EditorTerrain_Delete(EDITOR_TERRAIN_BLOCK, 99) != NULL);
    CHECK(EditorTerrain_Move(EDITOR_TERRAIN_BLOCK, 0, 2) != NULL);
    CHECK(EditorTerrain_Move(EDITOR_TERRAIN_BLOCK, 2, 0) != NULL);
    CHECK(EditorTerrain_Insert(EDITOR_TERRAIN_BLOCK, 0) != NULL);
    while (EditorTerrain_Count(EDITOR_TERRAIN_CHUNK) > 2)
        CHECK(EditorTerrain_Delete(EDITOR_TERRAIN_CHUNK, 1) == NULL);
    CHECK(EditorTerrain_Delete(EDITOR_TERRAIN_CHUNK, 1) == NULL); // (two: one can go)
    CHECK_EQ((long long)EditorTerrain_Count(EDITOR_TERRAIN_CHUNK), 1);
    CHECK(EditorTerrain_Delete(EDITOR_TERRAIN_CHUNK, 1) != NULL); // (one is left: it is the blank one)
}

static void EditorTerrain_CopyPastesAThingWithItsCurves(void) {
    Build();
    CHECK(EditorTerrain_Paste(EDITOR_TERRAIN_BLOCK, 2) != NULL); // (nothing copied yet)
    CHECK(EditorTerrain_Copy(EDITOR_TERRAIN_BLOCK, 4) == NULL);
    CHECK(EditorTerrain_Paste(EDITOR_TERRAIN_BLOCK, 2) == NULL);
    CHECK(memcmp(level_map16 + 2 * 8, level_map16 + 4 * 8, 8) == 0);
    CHECK_EQ(coll_index[0][2], 14);
    CHECK_EQ(coll_index[1][2], 24);
    CHECK(EditorTerrain_Copy(EDITOR_TERRAIN_TILE, 5) == NULL);
    CHECK(EditorTerrain_Paste(EDITOR_TERRAIN_TILE, 1) == NULL);
    CHECK_EQ(art[1 * 32], 5);
    CHECK(EditorTerrain_Copy(EDITOR_TERRAIN_CHUNK, 3) == NULL);
    CHECK(EditorTerrain_Paste(EDITOR_TERRAIN_CHUNK, 1) == NULL);
    CHECK(memcmp(level_map128 + 128, level_map128 + 3 * 128, 128) == 0);
    CHECK(EditorTerrain_Paste(EDITOR_TERRAIN_CHUNK, 0) != NULL); // (the blank one is left alone)
}

static void EditorTerrain_ABlankLevelHasAFloorAndEnoughToEdit(void) {
    Build();
    CHECK(EditorTerrain_MakeBlank());
    CHECK_EQ((long long)EditorTerrain_Count(EDITOR_TERRAIN_TILE), 3);
    CHECK_EQ((long long)EditorTerrain_Count(EDITOR_TERRAIN_BLOCK), 3);
    CHECK_EQ((long long)EditorTerrain_Count(EDITOR_TERRAIN_CHUNK), 2);
    size_t bytes;
    const uint8_t *layout = Editor_Layout(&bytes);
    CHECK_EQ(layout[3 * 0x100], 1);
    CHECK_EQ(layout[0], 0);
    CHECK(coll_index[0][1] != 0); // (the earth block is solid)
    CHECK_EQ(coll_index[0][0], 0);
    // and it can be edited: a tile inserted, a chunk deleted
    CHECK(EditorTerrain_Insert(EDITOR_TERRAIN_TILE, 3) == NULL);
    CHECK(EditorTerrain_Delete(EDITOR_TERRAIN_CHUNK, 1) == NULL);
    Editor_SetTarget(0x11, 0, "WNT");
    Editor_Open(0, 0, NULL, NULL);
    CHECK(!Editor_HasTarget());
    Editor_Close();
}

void RegisterEditorTerrainTests(void) {
    RUN_TEST(EditorTerrain_ABlankLevelHasAFloorAndEnoughToEdit);
    RUN_TEST(EditorTerrain_ReportsTheCountsAndLimits);
    RUN_TEST(EditorTerrain_MovingATileKeepsEveryBlocksPicture);
    RUN_TEST(EditorTerrain_MovingABlockKeepsTheChunksAndTheCurves);
    RUN_TEST(EditorTerrain_MovingAChunkKeepsTheLayout);
    RUN_TEST(EditorTerrain_InsertingShiftsWhatComesAfter);
    RUN_TEST(EditorTerrain_InsertStopsAtTheLimit);
    RUN_TEST(EditorTerrain_DeletingPointsWhatNamedItAtTheBlankOne);
    RUN_TEST(EditorTerrain_DeleteNeedsTwoAndKeepsTheBlankOne);
    RUN_TEST(EditorTerrain_CopyPastesAThingWithItsCurves);
}
