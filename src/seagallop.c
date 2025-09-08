#include "global.h"
#include "gflib.h"
#include "scanline_effect.h"
#include "trainer_pokemon_sprites.h"
#include "text_window.h"
#include "task.h"
#include "help_system.h"
#include "overworld.h"
#include "event_data.h"
#include "field_fadetransition.h"
#include "field_weather.h"
#include "constants/songs.h"
#include "constants/maps.h"
#include "constants/seagallop.h"

#define TILESTAG_FERRY 3000
#define TILESTAG_WAKE  4000

#define PALTAG_FERRY_WAKE 3000

enum {
    DIRN_WESTBOUND,
    DIRN_EASTBOUND,
};

#define BG_WATER 3

#define gSpecialVar_SeagallopOrigin       gSpecialVar_0x8004
#define gSpecialVar_SeagallopDestination  gSpecialVar_0x8006

static EWRAM_DATA void *sBg3TilemapBuffer = NULL;

static void CB2_SetUpSeagallopScene(void);
static void VBlankCB_Seagallop(void);
static void MainCB2_Seagallop(void);
static void Task_Seagallop_Start(u8 taskId);
static void Task_Seagallop_Travel(u8 taskId);
static void Task_Seagallop_WaitFadeOut(u8 taskId);
static void Task_Seagallop_Warp(void);
static void ResetGPU(void);
static void ResetAllAssets(void);
static void SetDispcnt(void);
static void ResetBGPos(void);
static void LoadFerrySpriteResources(void);
static void FreeFerrySpriteResources(void);
static void CreateFerrySprite(void);
static void SpriteCB_Ferry(struct Sprite *sprite);
static void CreateWakeSprite(s16 x);
static void SpriteCB_Wake(struct Sprite *sprite);
static bool8 GetDirectionOfTravel(void);

static const u16 sWaterTiles[] = INCBIN_U16("graphics/seagallop/water.4bpp");
static const u16 sWaterPal[] = INCBIN_U16("graphics/seagallop/water.gbapal");
static const u16 sWaterTilemap_Westbound[] = INCBIN_U16("graphics/seagallop/water_westbound.bin");
static const u16 sWaterTilemap_Eastbound[] = INCBIN_U16("graphics/seagallop/water_eastbound.bin");
static const u16 sFerrySpriteTiles[] = INCBIN_U16("graphics/seagallop/ferry_sprite.4bpp");
static const u16 sFerryAndWakePal[] = INCBIN_U16("graphics/seagallop/ferry_and_wake.gbapal");
static const u16 sWakeSpriteTiles[] = INCBIN_U16("graphics/seagallop/wake.4bpp");

static const struct BgTemplate sBGTemplates[] = {
    {
        .bg = BG_WATER,
        .charBaseIndex = 3,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 3,
        .baseTile = 0x000
    }
};

struct SeagallopWarp {
    u8 mapGroup;
    u8 mapNum;
    s8 x;
    s8 y;
};

static const struct SeagallopWarp sSeagallopWarps[] = {
    [SEAGALLOP_VERMILION_CITY] = {
        .mapGroup = MAP_GROUP(MAP_VERMILION_CITY),
        .mapNum = MAP_NUM(MAP_VERMILION_CITY),
        .x = 23,
        .y = 32,
    },
    [SEAGALLOP_ONE_ISLAND] = {
        .mapGroup = MAP_GROUP(MAP_ONE_ISLAND_HARBOR),
        .mapNum = MAP_NUM(MAP_ONE_ISLAND_HARBOR),
        .x = 8,
        .y = 5,
    },
    [SEAGALLOP_TWO_ISLAND] = {
        .mapGroup = MAP_GROUP(MAP_TWO_ISLAND_HARBOR),
        .mapNum = MAP_NUM(MAP_TWO_ISLAND_HARBOR),
        .x = 8,
        .y = 5,
    },
    [SEAGALLOP_THREE_ISLAND] = {
        .mapGroup = MAP_GROUP(MAP_THREE_ISLAND_HARBOR),
        .mapNum = MAP_NUM(MAP_THREE_ISLAND_HARBOR),
        .x = 8,
        .y = 5,
    },
    [SEAGALLOP_FOUR_ISLAND] = {
        .mapGroup = MAP_GROUP(MAP_FOUR_ISLAND_HARBOR),
        .mapNum = MAP_NUM(MAP_FOUR_ISLAND_HARBOR),
        .x = 8,
        .y = 5,
    },
    [SEAGALLOP_FIVE_ISLAND] = {
        .mapGroup = MAP_GROUP(MAP_FIVE_ISLAND_HARBOR),
        .mapNum = MAP_NUM(MAP_FIVE_ISLAND_HARBOR),
        .x = 8,
        .y = 5,
    },
    [SEAGALLOP_SIX_ISLAND] = {
        .mapGroup = MAP_GROUP(MAP_SIX_ISLAND_HARBOR),
        .mapNum = MAP_NUM(MAP_SIX_ISLAND_HARBOR),
        .x = 8,
        .y = 5,
    },
    [SEAGALLOP_SEVEN_ISLAND] = {
        .mapGroup = MAP_GROUP(MAP_SEVEN_ISLAND_HARBOR),
        .mapNum = MAP_NUM(MAP_SEVEN_ISLAND_HARBOR),
        .x = 8,
        .y = 5,
    },
    [SEAGALLOP_CINNABAR_ISLAND] = {
        .mapGroup = MAP_GROUP(MAP_CINNABAR_ISLAND),
        .mapNum = MAP_NUM(MAP_CINNABAR_ISLAND),
        .x = 21,
        .y = 7,
    },
    [SEAGALLOP_NAVEL_ROCK] = {
        .mapGroup = MAP_GROUP(MAP_NAVEL_ROCK_HARBOR),
        .mapNum = MAP_NUM(MAP_NAVEL_ROCK_HARBOR),
        .x = 8,
        .y = 5,
    },
    [SEAGALLOP_BIRTH_ISLAND] = {
        .mapGroup = MAP_GROUP(MAP_BIRTH_ISLAND_HARBOR),
        .mapNum = MAP_NUM(MAP_BIRTH_ISLAND_HARBOR),
        .x = 8,
        .y = 5,
    },
};

// For each origin location, lists the destination locations that are East of it.
static const u16 sTravelDirectionMatrix[] = {
    [SEAGALLOP_VERMILION_CITY]  = (1 << SEAGALLOP_ONE_ISLAND)
                                | (1 << SEAGALLOP_TWO_ISLAND)
                                | (1 << SEAGALLOP_THREE_ISLAND)
                                | (1 << SEAGALLOP_FOUR_ISLAND)
                                | (1 << SEAGALLOP_FIVE_ISLAND)
                                | (1 << SEAGALLOP_SIX_ISLAND)
                                | (1 << SEAGALLOP_SEVEN_ISLAND)
                                | (1 << SEAGALLOP_NAVEL_ROCK)
                                | (1 << SEAGALLOP_BIRTH_ISLAND),

    [SEAGALLOP_ONE_ISLAND]      = (1 << SEAGALLOP_TWO_ISLAND)
                                | (1 << SEAGALLOP_THREE_ISLAND)
                                | (1 << SEAGALLOP_FOUR_ISLAND)
                                | (1 << SEAGALLOP_FIVE_ISLAND)
                                | (1 << SEAGALLOP_SIX_ISLAND)
                                | (1 << SEAGALLOP_SEVEN_ISLAND)
                                | (1 << SEAGALLOP_NAVEL_ROCK)
                                | (1 << SEAGALLOP_BIRTH_ISLAND),

    [SEAGALLOP_TWO_ISLAND]      = (1 << SEAGALLOP_THREE_ISLAND)
                                | (1 << SEAGALLOP_FOUR_ISLAND)
                                | (1 << SEAGALLOP_FIVE_ISLAND)
                                | (1 << SEAGALLOP_SIX_ISLAND)
                                | (1 << SEAGALLOP_SEVEN_ISLAND)
                                | (1 << SEAGALLOP_NAVEL_ROCK)
                                | (1 << SEAGALLOP_BIRTH_ISLAND),

    [SEAGALLOP_THREE_ISLAND]    = (1 << SEAGALLOP_FOUR_ISLAND)
                                | (1 << SEAGALLOP_FIVE_ISLAND)
                                | (1 << SEAGALLOP_SIX_ISLAND)
                                | (1 << SEAGALLOP_SEVEN_ISLAND)
                                | (1 << SEAGALLOP_NAVEL_ROCK)
                                | (1 << SEAGALLOP_BIRTH_ISLAND),

    [SEAGALLOP_FOUR_ISLAND]     = (1 << SEAGALLOP_FIVE_ISLAND)
                                | (1 << SEAGALLOP_SIX_ISLAND)
                                | (1 << SEAGALLOP_SEVEN_ISLAND)
                                | (1 << SEAGALLOP_NAVEL_ROCK)
                                | (1 << SEAGALLOP_BIRTH_ISLAND),

    [SEAGALLOP_FIVE_ISLAND]     = (1 << SEAGALLOP_SIX_ISLAND)
                                | (1 << SEAGALLOP_SEVEN_ISLAND)
                                | (1 << SEAGALLOP_BIRTH_ISLAND),

    [SEAGALLOP_SIX_ISLAND]      = (1 << SEAGALLOP_BIRTH_ISLAND),

    [SEAGALLOP_SEVEN_ISLAND]    = (1 << SEAGALLOP_SIX_ISLAND)
                                | (1 << SEAGALLOP_BIRTH_ISLAND),

    [SEAGALLOP_CINNABAR_ISLAND] = (1 << SEAGALLOP_VERMILION_CITY)
                                | (1 << SEAGALLOP_ONE_ISLAND)
                                | (1 << SEAGALLOP_TWO_ISLAND)
                                | (1 << SEAGALLOP_THREE_ISLAND)
                                | (1 << SEAGALLOP_FOUR_ISLAND)
                                | (1 << SEAGALLOP_FIVE_ISLAND)
                                | (1 << SEAGALLOP_SIX_ISLAND)
                                | (1 << SEAGALLOP_SEVEN_ISLAND)
                                | (1 << SEAGALLOP_CINNABAR_ISLAND) // Pointless
                                | (1 << SEAGALLOP_NAVEL_ROCK)
                                | (1 << SEAGALLOP_BIRTH_ISLAND),

    [SEAGALLOP_NAVEL_ROCK]      = (1 << SEAGALLOP_FIVE_ISLAND)
                                | (1 << SEAGALLOP_SIX_ISLAND)
                                | (1 << SEAGALLOP_SEVEN_ISLAND)
                                | (1 << SEAGALLOP_NAVEL_ROCK)
                                | (1 << SEAGALLOP_BIRTH_ISLAND),

    [SEAGALLOP_BIRTH_ISLAND]    = 0
};

static const union AnimCmd sSpriteAnims_Ferry_Westbound[] = {
    ANIMCMD_FRAME(0, 10),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnims_Ferry_Eastbound[] = {
    ANIMCMD_FRAME(0, 10, .hFlip = TRUE),
    ANIMCMD_END
};

static const union AnimCmd *const sSpriteAnimTable_Ferry[] = {
    [DIRN_WESTBOUND] = sSpriteAnims_Ferry_Westbound,
    [DIRN_EASTBOUND] = sSpriteAnims_Ferry_Eastbound
};

static const struct OamData sOamData_Ferry = {
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .size = SPRITE_SIZE(64x64),
};

static const struct SpriteTemplate sFerrySpriteTemplate = {
    .tileTag = TILESTAG_FERRY,
    .paletteTag = PALTAG_FERRY_WAKE,
    .oam = &sOamData_Ferry,
    .anims = sSpriteAnimTable_Ferry,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_Ferry,
};

static const struct SpriteSheet sFerryAndWakeSpriteSheets[] = {
    {.data = (const void *)sWakeSpriteTiles,  .size = sizeof(sWakeSpriteTiles),  .tag = TILESTAG_WAKE},
    {.data = (const void *)sFerrySpriteTiles, .size = sizeof(sFerrySpriteTiles), .tag = TILESTAG_FERRY},
    {}
};

static const struct SpritePalette sFerryAndWakeSpritePalettes[] = {
    {sFerryAndWakePal, PALTAG_FERRY_WAKE},
    {}
};

// The wake animation has 4 frames going from largest to smallest. The last, smallest frame is not used.
#define WAKE_FRAME_NUM_TILES (32 * 32 / 64)
static const union AnimCmd sSpriteAnims_Wake_Westbound[] = {
    ANIMCMD_FRAME(0 * WAKE_FRAME_NUM_TILES, 20),
    ANIMCMD_FRAME(1 * WAKE_FRAME_NUM_TILES, 20),
    ANIMCMD_FRAME(2 * WAKE_FRAME_NUM_TILES, 15),
    ANIMCMD_END,
};

static const union AnimCmd sSpriteAnims_Wake_Eastbound[] = {
    ANIMCMD_FRAME(0 * WAKE_FRAME_NUM_TILES, 20, .hFlip = TRUE),
    ANIMCMD_FRAME(1 * WAKE_FRAME_NUM_TILES, 20, .hFlip = TRUE),
    ANIMCMD_FRAME(2 * WAKE_FRAME_NUM_TILES, 15, .hFlip = TRUE),
    ANIMCMD_END,
};

static const union AnimCmd *const sSpriteAnimTable_Wake[] = {
    [DIRN_WESTBOUND] = sSpriteAnims_Wake_Westbound,
    [DIRN_EASTBOUND] = sSpriteAnims_Wake_Eastbound
};

static const struct OamData sOamData_Wake = {
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
};

static const struct SpriteTemplate sWakeSpriteTemplate = {
    .tileTag = TILESTAG_WAKE,
    .paletteTag = PALTAG_FERRY_WAKE,
    .oam = &sOamData_Wake,
    .anims = sSpriteAnimTable_Wake,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_Wake,
};

void DoSeagallopFerryScene(void)
{
    SetVBlankCallback(NULL);
    HelpSystem_Disable();
    SetMainCallback2(CB2_SetUpSeagallopScene);
}

static void CB2_SetUpSeagallopScene(void)
{
    void ** ptr;
    switch (gMain.state)
    {
    case 0:
        SetVBlankCallback(NULL); // redundant since the setup routine already did this
        ResetGPU();
        gMain.state++;
        break;
    case 1:
        ResetAllAssets();
        gMain.state++;
        break;
    case 2:
        ptr = &sBg3TilemapBuffer;
        *ptr = AllocZeroed(BG_SCREEN_SIZE);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sBGTemplates, ARRAY_COUNT(sBGTemplates));
        SetBgTilemapBuffer(BG_WATER, *ptr);
        ResetBGPos();
        gMain.state++;
        break;
    case 3:
        LoadBgTiles(BG_WATER, sWaterTiles, sizeof(sWaterTiles), 0);
        if (GetDirectionOfTravel() == DIRN_EASTBOUND)
            CopyToBgTilemapBufferRect(BG_WATER, sWaterTilemap_Eastbound, 0, 0, 32, 32);
        else
            CopyToBgTilemapBufferRect(BG_WATER, sWaterTilemap_Westbound, 0, 0, 32, 32);
        LoadPalette(sWaterPal, BG_PLTT_ID(4), sizeof(sWaterPal));
        LoadPalette(GetTextWindowPalette(2), BG_PLTT_ID(15), PLTT_SIZE_4BPP);
        gMain.state++;
        break;
    case 4:
        if (IsDma3ManagerBusyWithBgCopy() != DIRN_EASTBOUND)
        {
            ShowBg(0);
            ShowBg(BG_WATER);
            CopyBgTilemapBufferToVram(BG_WATER);
            gMain.state++;
        }
        break;
    case 5:
        LoadFerrySpriteResources();
        BlendPalettes(PALETTES_ALL, 16, RGB_BLACK);
        gMain.state++;
        break;
    case 6:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        gMain.state++;
        break;
    case 7:
        SetDispcnt();
        SetVBlankCallback(VBlankCB_Seagallop);
        PlaySE(SE_SHIP);
        CreateFerrySprite();
        SetGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON);
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_ALL);
        SetGpuReg(REG_OFFSET_WINOUT, 0);
        SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(0, DISPLAY_WIDTH));
        SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(24, DISPLAY_HEIGHT - 24));
        CreateTask(Task_Seagallop_Start, 8);
        SetMainCallback2(MainCB2_Seagallop);
        gMain.state = 0;
        break;
    }
}

static void VBlankCB_Seagallop(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void MainCB2_Seagallop(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

// Data for Task_Seagallop_*
#define tTimer data[1]

static void Task_Seagallop_Start(u8 taskId)
{
    gTasks[taskId].func = Task_Seagallop_Travel;
}

#define SCROLL_SPEED 6

static void ScrollBG(void)
{
    if (GetDirectionOfTravel() == DIRN_EASTBOUND)
    {
        ChangeBgX(BG_WATER, SCROLL_SPEED << 8, BG_COORD_ADD);
    }
    else
    {
        ChangeBgX(BG_WATER, SCROLL_SPEED << 8, BG_COORD_SUB);
    }
}

// Scrolls the water background while waiting for the ferry/wake sprites to travel across the screen.
static void Task_Seagallop_Travel(u8 taskId)
{
    struct Task *task = &gTasks[taskId];

    ScrollBG();
    if (++task->tTimer == 140)
    {
        // Start fade out
        Overworld_FadeOutMapMusic();
        WarpFadeOutScreen();
        task->func = Task_Seagallop_WaitFadeOut;
    }
}

static void Task_Seagallop_WaitFadeOut(u8 taskId)
{
    ScrollBG();
    if (BGMusicStopped() && !gPaletteFade.active)
    {
        Task_Seagallop_Warp();
        HelpSystem_Enable();
        DestroyTask(taskId);
    }
}

static void Task_Seagallop_Warp(void)
{
    const struct SeagallopWarp * warpInfo;

    if (gSpecialVar_SeagallopDestination >= ARRAY_COUNT(sSeagallopWarps))
        gSpecialVar_SeagallopDestination = 0;

    warpInfo = &sSeagallopWarps[gSpecialVar_SeagallopDestination];
    SetWarpDestination(warpInfo->mapGroup, warpInfo->mapNum, WARP_ID_NONE, warpInfo->x, warpInfo->y);
    PlayRainStoppingSoundEffect();
    PlaySE(SE_EXIT);
    gFieldCallback = FieldCB_DefaultWarpExit;
    WarpIntoMap();
    SetMainCallback2(CB2_LoadMap);
    ResetInitialPlayerAvatarState();
    FreeFerrySpriteResources();
    Free(sBg3TilemapBuffer);
    FreeAllWindowBuffers();
}

static void ResetGPU(void)
{
    void *dest = (void *) VRAM;
    DmaClearLarge16(3, dest, VRAM_SIZE, 0x1000);

    DmaClear32(3, (void *)OAM, OAM_SIZE);
    DmaClear16(3, (void *)PLTT, PLTT_SIZE);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    SetGpuReg(REG_OFFSET_BG0CNT, 0);
    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    SetGpuReg(REG_OFFSET_BG1CNT, 0);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);
    SetGpuReg(REG_OFFSET_BG2CNT, 0);
    SetGpuReg(REG_OFFSET_BG2HOFS, 0);
    SetGpuReg(REG_OFFSET_BG2VOFS, 0);
    SetGpuReg(REG_OFFSET_BG3CNT, 0);
    SetGpuReg(REG_OFFSET_BG3HOFS, 0);
    SetGpuReg(REG_OFFSET_BG3VOFS, 0);
    SetGpuReg(REG_OFFSET_WIN0H, 0);
    SetGpuReg(REG_OFFSET_WIN0V, 0);
    SetGpuReg(REG_OFFSET_WININ, 0);
    SetGpuReg(REG_OFFSET_WINOUT, 0);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_BLDALPHA, 0);
    SetGpuReg(REG_OFFSET_BLDY, 0);
}

static void ResetAllAssets(void)
{
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    ResetAllPicSprites();
    ResetPaletteFade();
    FreeAllSpritePalettes();
}

static void SetDispcnt(void)
{
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_BG0_ON | DISPCNT_BG3_ON | DISPCNT_OBJ_ON);
}

static void ResetBGPos(void)
{
    ChangeBgX(0, 0, BG_COORD_SET);
    ChangeBgY(0, 0, BG_COORD_SET);
    ChangeBgX(1, 0, BG_COORD_SET);
    ChangeBgY(1, 0, BG_COORD_SET);
    ChangeBgX(2, 0, BG_COORD_SET);
    ChangeBgY(2, 0, BG_COORD_SET);
    ChangeBgX(3, 0, BG_COORD_SET);
    ChangeBgY(3, 0, BG_COORD_SET);
}

static void LoadFerrySpriteResources(void)
{
    LoadSpriteSheets(sFerryAndWakeSpriteSheets);
    LoadSpritePalettes(sFerryAndWakeSpritePalettes);
}

static void FreeFerrySpriteResources(void)
{
    FreeSpriteTilesByTag(TILESTAG_FERRY);
    FreeSpriteTilesByTag(TILESTAG_WAKE);
    FreeSpritePaletteByTag(PALTAG_FERRY_WAKE);
}

// Data for SpriteCB_Ferry
#define sVelocity data[0]
#define sDistance data[1]
#define sTimer    data[2]

#define FERRY_Y_POS 92

static void CreateFerrySprite(void)
{
    u8 spriteId = CreateSprite(&sFerrySpriteTemplate, 0, FERRY_Y_POS, 0);
    gSprites[spriteId].sVelocity = 3 << 4;
    if (GetDirectionOfTravel() == DIRN_EASTBOUND)
    {
        StartSpriteAnim(&gSprites[spriteId], DIRN_EASTBOUND);
    }
    else
    {
        gSprites[spriteId].x = DISPLAY_WIDTH;
        gSprites[spriteId].sVelocity *= -1;
    }
}

static void SpriteCB_Ferry(struct Sprite *sprite)
{
    sprite->sDistance += sprite->sVelocity;
    sprite->x2 = sprite->sDistance >> 4;
    if (sprite->sTimer % 5 == 0)
    {
        // Every 5th frame we create a new wake sprite.
        // They animate from a large wake to a small wake, then disappear.
        // Because the animation takes 55 frames, there are at max 11 wake
        // sprites at once (except during 1 frame before the 12th sprite gets deleted).
        CreateWakeSprite(sprite->x + sprite->x2);
    }
    sprite->sTimer++;

    // This waits a little longer than necessary; the travel distance to get the
    // ferry sprite from one side of the screen to off the other is 272 pixels
    // (240 for the screen, 32 for half the sprite's width).
    // There's no visible effect of this extra time, just extra unnecessary work offscreen.
    if (sprite->x2 < -300 || sprite->x2 > 300)
    {
        DestroySprite(sprite);
    }
}

#undef sVelocity
#undef sDistance
#undef sTimer

static void CreateWakeSprite(s16 x)
{
    u8 spriteId = CreateSprite(&sWakeSpriteTemplate, x, FERRY_Y_POS, 8);
    if (spriteId != MAX_SPRITES)
    {
        if (GetDirectionOfTravel() == DIRN_EASTBOUND)
        {
            StartSpriteAnim(&gSprites[spriteId], DIRN_EASTBOUND);
        }
    }
}

static void SpriteCB_Wake(struct Sprite *sprite)
{
    if (sprite->animEnded)
    {
        DestroySprite(sprite);
    }
}

static bool8 GetDirectionOfTravel(void)
{
    if (gSpecialVar_SeagallopOrigin >= ARRAY_COUNT(sTravelDirectionMatrix))
    {
        return DIRN_EASTBOUND;
    }
    return (sTravelDirectionMatrix[gSpecialVar_SeagallopOrigin] >> gSpecialVar_SeagallopDestination) & 1;
}

// For "All aboard SEAGALLOP HI-SPEED ##" text
u8 GetSeagallopNumber(void)
{
    u16 originId, destId;

    originId = gSpecialVar_SeagallopOrigin;
    destId = gSpecialVar_SeagallopDestination;

    if (originId == SEAGALLOP_CINNABAR_ISLAND || destId == SEAGALLOP_CINNABAR_ISLAND)
        return 1;

    if (originId == SEAGALLOP_VERMILION_CITY || destId == SEAGALLOP_VERMILION_CITY)
        return 7;

    if (originId == SEAGALLOP_NAVEL_ROCK || destId == SEAGALLOP_NAVEL_ROCK)
        return 10;

    if (originId == SEAGALLOP_BIRTH_ISLAND || destId == SEAGALLOP_BIRTH_ISLAND)
        return 12;

    if ((originId == SEAGALLOP_ONE_ISLAND 
      || originId == SEAGALLOP_TWO_ISLAND 
      || originId == SEAGALLOP_THREE_ISLAND) 
      && (destId == SEAGALLOP_ONE_ISLAND 
       || destId == SEAGALLOP_TWO_ISLAND 
       || destId == SEAGALLOP_THREE_ISLAND))
        return 2;

    if ((originId == SEAGALLOP_FOUR_ISLAND 
      || originId == SEAGALLOP_FIVE_ISLAND) 
      && (destId == SEAGALLOP_FOUR_ISLAND 
       || destId == SEAGALLOP_FIVE_ISLAND))
        return 3;

    if ((originId == SEAGALLOP_SIX_ISLAND 
      || originId == SEAGALLOP_SEVEN_ISLAND) 
      && (destId == SEAGALLOP_SIX_ISLAND 
       || destId == SEAGALLOP_SEVEN_ISLAND))
        return 5;

    return 6;
}

bool8 IsPlayerLeftOfVermilionSailor(void)
{
    if (gSaveBlock1Ptr->location.mapGroup == MAP_GROUP(MAP_VERMILION_CITY) 
       && gSaveBlock1Ptr->location.mapNum == MAP_NUM(MAP_VERMILION_CITY) 
       && gSaveBlock1Ptr->pos.x < 24)
        return TRUE;

    return FALSE;
}
