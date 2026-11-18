#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "system.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"

/* Fallback base address for 80x60 Character Buffer */
#ifndef VIDEO_CHARACTER_BUFFER_0_AVALON_CHAR_BUFFER_SLAVE_BASE
#define VIDEO_CHARACTER_BUFFER_0_AVALON_CHAR_BUFFER_SLAVE_BASE 0x00442000
#endif

/* RGB565 Helper Macro */
#define RGB565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define PACK32(p0, p1)  (((uint32_t)(p1) << 16) | (uint32_t)(p0))

/* Phase 2/3 RGB565 Color Palette */
#define COLOR_TRANSPARENT  0xF81F          /* Magenta chroma-key (not drawn) */
#define COLOR_BLACK        0x0000
#define COLOR_WHITE        0xFFFF

/* Road & Track Colors */
#define COLOR_ASPHALT      RGB565(48, 52, 58)
#define COLOR_ASPHALT_ALT  RGB565(56, 60, 68)
#define COLOR_DASH_YELLOW  RGB565(245, 200, 40)
#define COLOR_CURB_RED     RGB565(220, 30, 30)
#define COLOR_CURB_WHITE   RGB565(245, 245, 245)

/* Off-Track Scenery Colors */
#define COLOR_GRASS_BASE   RGB565(34, 130, 48)
#define COLOR_GRASS_LIGHT  RGB565(52, 160, 64)
#define COLOR_TREE_DARK    RGB565(20, 88, 32)
#define COLOR_TREE_TRUNK   RGB565(110, 72, 36)

/* Vehicle Palette */
#define C_TR  COLOR_TRANSPARENT
#define C_BK  RGB565(18, 18, 22)      /* Tires / Shadows */
#define C_GY  RGB565(130, 138, 148)   /* Metallic Wing / Trim */
#define C_WH  RGB565(248, 248, 252)   /* Racing White */
#define C_RD  RGB565(225, 25, 25)     /* F1 Ferrari Red */
#define C_DR  RGB565(155, 15, 15)     /* F1 Shading Red */
#define C_BL  RGB565(30, 110, 235)    /* Rival Car Blue */
#define C_DB  RGB565(18, 65, 160)     /* Rival Car Dark Blue */
#define C_YL  RGB565(245, 195, 25)    /* Rival Car Yellow */
#define C_DY  RGB565(185, 135, 10)    /* Rival Car Dark Yellow */
#define C_GL  RGB565(140, 220, 255)   /* Windshield Tint */

/* Track Bitmask & Grid Constants */
#define EMPTY_ROAD_ROW 0x000000FFF8001FFFULL
#define CAR_ROW        25
#define CAR_MIN_COL    2
#define CAR_MAX_COL    36
#define STD_ROAD_WIDTH 14

uint64_t screen_rows[30];
int      curb_left_hist[30];
int      curb_right_hist[30];


// 80x60 Character Buffer Score & HUD Functions (0x0044_2000)

static inline void char_put_at(uint32_t char_base, int cx, int cy, char c) {
    if (cx < 0 || cx >= 80 || cy < 0 || cy >= 60) return;
    uint32_t offset = ((uint32_t)cy << 7) + (uint32_t)cx;
    IOWR_8DIRECT(char_base, offset, (uint8_t)c);
}

void char_put_str(uint32_t char_base, int cx, int cy, const char *str) {
    while (*str && cx < 80) {
        char_put_at(char_base, cx++, cy, *str++);
    }
}

void char_clear_screen(uint32_t char_base) {
    for (int cy = 0; cy < 60; cy++) {
        uint32_t row_offset = (uint32_t)cy << 7;
        for (int cx = 0; cx < 80; cx++) {
            IOWR_8DIRECT(char_base, row_offset + cx, ' ');
        }
    }
}

void hud_init(uint32_t char_base) {
    char_clear_screen(char_base);
    char_put_str(char_base, 2, 4,  "SCORE");
    char_put_str(char_base, 2, 5,  "-----");
    char_put_str(char_base, 2, 10, "BEST");
    char_put_str(char_base, 2, 11, "----");
}

void hud_update_score(uint32_t char_base, uint32_t score, uint32_t best_score) {
    char buf[16];

    snprintf(buf, sizeof(buf), "%07lu", (unsigned long)score);
    char_put_str(char_base, 2, 7, buf);

    snprintf(buf, sizeof(buf), "%07lu", (unsigned long)best_score);
    char_put_str(char_base, 2, 13, buf);
}


// 16x16 Player F1 Car Sprite (Facing Up / North)

static const uint16_t sprite_f1_player[16][16] = {
    {C_TR,C_TR,C_TR,C_WH,C_WH,C_RD,C_RD,C_WH,C_WH,C_RD,C_RD,C_WH,C_WH,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_RD,C_RD,C_RD,C_WH,C_RD,C_RD,C_WH,C_RD,C_RD,C_RD,C_TR,C_TR,C_TR},
    {C_TR,C_BK,C_BK,C_BK,C_TR,C_TR,C_RD,C_WH,C_WH,C_RD,C_TR,C_TR,C_BK,C_BK,C_BK,C_TR},
    {C_TR,C_BK,C_GY,C_BK,C_TR,C_RD,C_RD,C_WH,C_WH,C_RD,C_RD,C_TR,C_BK,C_GY,C_BK,C_TR},
    {C_TR,C_BK,C_BK,C_BK,C_RD,C_RD,C_DR,C_RD,C_RD,C_DR,C_RD,C_RD,C_BK,C_BK,C_BK,C_TR},
    {C_TR,C_TR,C_TR,C_RD,C_RD,C_WH,C_BK,C_BK,C_BK,C_BK,C_WH,C_RD,C_RD,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_RD,C_RD,C_WH,C_BK,C_GY,C_BK,C_BK,C_GY,C_BK,C_WH,C_RD,C_RD,C_TR,C_TR},
    {C_TR,C_TR,C_RD,C_DR,C_RD,C_BK,C_BK,C_WH,C_WH,C_BK,C_BK,C_RD,C_DR,C_RD,C_TR,C_TR},
    {C_TR,C_TR,C_RD,C_DR,C_RD,C_RD,C_RD,C_WH,C_WH,C_RD,C_RD,C_RD,C_DR,C_RD,C_TR,C_TR},
    {C_TR,C_TR,C_RD,C_RD,C_RD,C_DR,C_RD,C_WH,C_WH,C_RD,C_DR,C_RD,C_RD,C_RD,C_TR,C_TR},
    {C_TR,C_BK,C_BK,C_BK,C_RD,C_RD,C_DR,C_RD,C_RD,C_DR,C_RD,C_RD,C_BK,C_BK,C_BK,C_TR},
    {C_TR,C_BK,C_GY,C_BK,C_TR,C_RD,C_DR,C_RD,C_RD,C_DR,C_RD,C_TR,C_BK,C_GY,C_BK,C_TR},
    {C_TR,C_BK,C_BK,C_BK,C_TR,C_TR,C_DR,C_BK,C_BK,C_DR,C_TR,C_TR,C_BK,C_BK,C_BK,C_TR},
    {C_TR,C_TR,C_WH,C_RD,C_RD,C_RD,C_WH,C_WH,C_WH,C_WH,C_RD,C_RD,C_RD,C_WH,C_TR,C_TR},
    {C_TR,C_TR,C_WH,C_WH,C_WH,C_WH,C_WH,C_WH,C_WH,C_WH,C_WH,C_WH,C_WH,C_WH,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_BK,C_RD,C_RD,C_BK,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR}
};


// 16x16 Obstacle Traffic Cars

static const uint16_t sprite_obs_blue[16][16] = {
    {C_TR,C_TR,C_TR,C_TR,C_DB,C_BL,C_BL,C_BL,C_BL,C_BL,C_BL,C_DB,C_TR,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_BL,C_BL,C_WH,C_BL,C_BL,C_BL,C_BL,C_WH,C_BL,C_BL,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_BK,C_BL,C_BL,C_WH,C_BL,C_BL,C_BL,C_BL,C_WH,C_BL,C_BL,C_BK,C_TR,C_TR},
    {C_TR,C_TR,C_BK,C_BL,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_BL,C_BK,C_TR,C_TR},
    {C_TR,C_TR,C_BK,C_BL,C_GL,C_WH,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_BL,C_BK,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_BL,C_DB,C_DB,C_DB,C_DB,C_DB,C_DB,C_DB,C_DB,C_BL,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_BL,C_DB,C_BL,C_WH,C_BL,C_BL,C_WH,C_BL,C_DB,C_BL,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_BL,C_DB,C_BL,C_WH,C_BL,C_BL,C_WH,C_BL,C_DB,C_BL,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_BL,C_DB,C_DB,C_DB,C_DB,C_DB,C_DB,C_DB,C_DB,C_BL,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_BL,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_BL,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_BK,C_BL,C_BL,C_WH,C_BL,C_BL,C_BL,C_BL,C_WH,C_BL,C_BL,C_BK,C_TR,C_TR},
    {C_TR,C_TR,C_BK,C_BL,C_BL,C_WH,C_BL,C_BL,C_BL,C_BL,C_WH,C_BL,C_BL,C_BK,C_TR,C_TR},
    {C_TR,C_TR,C_BK,C_DB,C_BL,C_BL,C_BL,C_BL,C_BL,C_BL,C_BL,C_BL,C_DB,C_BK,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_DB,C_RD,C_RD,C_DB,C_DB,C_DB,C_DB,C_RD,C_RD,C_DB,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_TR,C_GY,C_GY,C_GY,C_GY,C_GY,C_GY,C_GY,C_GY,C_TR,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR}
};

static const uint16_t sprite_obs_yellow[16][16] = {
    {C_TR,C_TR,C_TR,C_TR,C_DY,C_YL,C_YL,C_YL,C_YL,C_YL,C_YL,C_DY,C_TR,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_YL,C_YL,C_BK,C_YL,C_YL,C_YL,C_YL,C_BK,C_YL,C_YL,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_BK,C_YL,C_YL,C_BK,C_YL,C_YL,C_YL,C_YL,C_BK,C_YL,C_YL,C_BK,C_TR,C_TR},
    {C_TR,C_TR,C_BK,C_YL,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_YL,C_BK,C_TR,C_TR},
    {C_TR,C_TR,C_BK,C_YL,C_GL,C_WH,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_YL,C_BK,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_YL,C_DY,C_DY,C_DY,C_DY,C_DY,C_DY,C_DY,C_DY,C_YL,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_YL,C_DY,C_YL,C_BK,C_YL,C_YL,C_BK,C_YL,C_DY,C_YL,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_YL,C_DY,C_YL,C_BK,C_YL,C_YL,C_BK,C_YL,C_DY,C_YL,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_YL,C_DY,C_DY,C_DY,C_DY,C_DY,C_DY,C_DY,C_DY,C_YL,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_YL,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_GL,C_YL,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_BK,C_YL,C_YL,C_BK,C_YL,C_YL,C_YL,C_YL,C_BK,C_YL,C_YL,C_BK,C_TR,C_TR},
    {C_TR,C_TR,C_BK,C_YL,C_YL,C_BK,C_YL,C_YL,C_YL,C_YL,C_BK,C_YL,C_YL,C_BK,C_TR,C_TR},
    {C_TR,C_TR,C_BK,C_DY,C_YL,C_YL,C_YL,C_YL,C_YL,C_YL,C_YL,C_YL,C_DY,C_BK,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_DY,C_RD,C_RD,C_DY,C_DY,C_DY,C_DY,C_RD,C_RD,C_DY,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_TR,C_GY,C_GY,C_GY,C_GY,C_GY,C_GY,C_GY,C_GY,C_TR,C_TR,C_TR,C_TR},
    {C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR,C_TR}
};


// Pre-Packed 32-Bit 8x8 Environment Tiles (4 x uint32_t per row = zero inner-loop overhead)

#define G_BB PACK32(COLOR_GRASS_BASE,  COLOR_GRASS_BASE)
#define G_BL PACK32(COLOR_GRASS_BASE,  COLOR_GRASS_LIGHT)
#define G_LB PACK32(COLOR_GRASS_LIGHT, COLOR_GRASS_BASE)
#define G_LL PACK32(COLOR_GRASS_LIGHT, COLOR_GRASS_LIGHT)
#define G_BT PACK32(COLOR_GRASS_BASE,  COLOR_TREE_DARK)
#define G_TB PACK32(COLOR_TREE_DARK,   COLOR_GRASS_BASE)
#define G_TT PACK32(COLOR_TREE_DARK,   COLOR_TREE_DARK)
#define G_TL PACK32(COLOR_TREE_DARK,   COLOR_GRASS_LIGHT)
#define G_LT PACK32(COLOR_GRASS_LIGHT, COLOR_TREE_DARK)
#define G_KK PACK32(COLOR_TREE_TRUNK,  COLOR_TREE_TRUNK)

static const uint32_t tile_grass_plain_packed[8][4] = {
    {G_BB, G_BB, G_BB, G_BB},
    {G_BL, G_BB, G_BB, G_BB},
    {G_BB, G_BB, G_LB, G_BB},
    {G_BB, G_BB, G_BB, G_BB},
    {G_BB, G_LB, G_BB, G_BB},
    {G_BB, G_BB, G_BB, G_LB},
    {G_BB, G_BB, G_BB, G_BB},
    {G_BB, G_BB, G_BB, G_BB}
};

static const uint32_t tile_grass_bush_packed[8][4] = {
    {G_BB, G_TT, G_TT, G_BB},
    {G_BT, G_LL, G_TT, G_TB},
    {G_TL, G_LT, G_TL, G_TT},
    {G_TT, G_TT, G_TT, G_TT},
    {G_BT, G_TT, G_TT, G_TB},
    {G_BB, G_BT, G_KK, G_BB},
    {G_BB, G_BT, G_KK, G_BB},
    {G_BB, G_BB, G_BB, G_BB}
};


// Constant-Speed Unrolled 32-Bit Tile & Sprite Drawing Primitives

static inline void draw_packed_tile_8x8(uint32_t buffer_base, int grid_x, int grid_y, const uint32_t tile[8][4]) {
    int start_x = grid_x << 3;
    int start_y = grid_y << 3;

    for (int dy = 0; dy < 8; dy++) {
        uint32_t row_offset = ((start_y + dy) << 10) + (start_x << 1);
        IOWR_32DIRECT(buffer_base, row_offset + 0,  tile[dy][0]);
        IOWR_32DIRECT(buffer_base, row_offset + 4,  tile[dy][1]);
        IOWR_32DIRECT(buffer_base, row_offset + 8,  tile[dy][2]);
        IOWR_32DIRECT(buffer_base, row_offset + 12, tile[dy][3]);
    }
}

static inline void draw_solid_8x8(uint32_t buffer_base, int grid_x, int grid_y, uint16_t color) {
    int start_x = grid_x << 3;
    int start_y = grid_y << 3;
    uint32_t packed = PACK32(color, color);

    for (int dy = 0; dy < 8; dy++) {
        uint32_t row_offset = ((start_y + dy) << 10) + (start_x << 1);
        IOWR_32DIRECT(buffer_base, row_offset + 0,  packed);
        IOWR_32DIRECT(buffer_base, row_offset + 4,  packed);
        IOWR_32DIRECT(buffer_base, row_offset + 8,  packed);
        IOWR_32DIRECT(buffer_base, row_offset + 12, packed);
    }
}

/* Single-pass 32-bit composited sprite quadrant over road_color (no second pass or 16-bit writes) */
static inline void draw_sprite_on_road_8x8(uint32_t buffer_base, int grid_x, int grid_y,
                                           uint16_t bg_color, const uint16_t sprite[16][16],
                                           int quad_x, int quad_y) {
    int start_x = grid_x << 3;
    int start_y = grid_y << 3;
    int src_x0  = quad_x << 3;
    int src_y0  = quad_y << 3;

    for (int dy = 0; dy < 8; dy++) {
        uint32_t row_offset = ((start_y + dy) << 10) + (start_x << 1);
        const uint16_t *srow = &sprite[src_y0 + dy][src_x0];

        uint16_t p0 = (srow[0] != COLOR_TRANSPARENT) ? srow[0] : bg_color;
        uint16_t p1 = (srow[1] != COLOR_TRANSPARENT) ? srow[1] : bg_color;
        uint16_t p2 = (srow[2] != COLOR_TRANSPARENT) ? srow[2] : bg_color;
        uint16_t p3 = (srow[3] != COLOR_TRANSPARENT) ? srow[3] : bg_color;
        uint16_t p4 = (srow[4] != COLOR_TRANSPARENT) ? srow[4] : bg_color;
        uint16_t p5 = (srow[5] != COLOR_TRANSPARENT) ? srow[5] : bg_color;
        uint16_t p6 = (srow[6] != COLOR_TRANSPARENT) ? srow[6] : bg_color;
        uint16_t p7 = (srow[7] != COLOR_TRANSPARENT) ? srow[7] : bg_color;

        IOWR_32DIRECT(buffer_base, row_offset + 0,  PACK32(p0, p1));
        IOWR_32DIRECT(buffer_base, row_offset + 4,  PACK32(p2, p3));
        IOWR_32DIRECT(buffer_base, row_offset + 8,  PACK32(p4, p5));
        IOWR_32DIRECT(buffer_base, row_offset + 12, PACK32(p6, p7));
    }
}


// ROM Fetch & Track Scrolling

uint64_t fetch_next_rom_row(uint8_t *row_cnt) {
    IOWR_ALTERA_AVALON_PIO_DATA(ROW_CTRL_BASE, (*row_cnt & 0x0F));
    usleep(5);

    uint32_t low  = IORD_ALTERA_AVALON_PIO_DATA(ROM_DATA_LOW_BASE);
    uint32_t high = IORD_ALTERA_AVALON_PIO_DATA(ROM_DATA_HIGH_BASE);
    uint64_t row_bits = ((uint64_t)high << 32) | (uint64_t)low;

    (*row_cnt)++;
    if (*row_cnt >= 16) {
        *row_cnt = 0;
        IOWR_ALTERA_AVALON_PIO_DATA(ROW_CTRL_BASE, 0x10);
        usleep(5);
        IOWR_ALTERA_AVALON_PIO_DATA(ROW_CTRL_BASE, 0x00);
    }
    return row_bits;
}

/*
 * Computes the new top row's left and right curb columns while guaranteeing:
 * 1. An obstacle car touching the wall (shrinking the 0-span below 14 cols) never moves the curb inward.
 * 2. Even if the ROM bitmask jumps by 2 columns, the curb steps by at most 1 block per row
 *    (directly adjacent to the previous row's track edge).
 */
static inline void get_road_curbs(uint64_t row_bits, int *out_curb_left, int *out_curb_right) {
    int left = 0;
    while (left < 40 && ((row_bits >> (39 - left)) & 1ULL)) {
        left++;
    }

    int right = 39;
    while (right >= 0 && ((row_bits >> (39 - right)) & 1ULL)) {
        right--;
    }

    if (left > right) {
        *out_curb_left  = 12;
        *out_curb_right = 27;
    } else {
        *out_curb_left  = left - 1;
        *out_curb_right = right + 1;
    }
}

void scroll_track_down(uint64_t new_top_row) {
    for (int y = 29; y > 0; y--) {
        screen_rows[y]     = screen_rows[y - 1];
        curb_left_hist[y]  = curb_left_hist[y - 1];
        curb_right_hist[y] = curb_right_hist[y - 1];
    }

    int next_c_left, next_c_right;
    get_road_curbs(new_top_row, &next_c_left, &next_c_right);

    screen_rows[0]     = new_top_row;
    curb_left_hist[0]  = next_c_left;
    curb_right_hist[0] = next_c_right;
}

void render_frame(uint32_t buffer_base, int car_col, uint32_t frame_tick) {
    uint32_t phase = frame_tick & 0x3;

    for (int y = 0; y < 30; y++) {
        uint64_t row_bits   = screen_rows[y];
        uint32_t scroll_y   = (uint32_t)y - frame_tick;
        int      curb_left  = curb_left_hist[y];
        int      curb_right = curb_right_hist[y];
        int      road_left  = curb_left + 1;
        int      road_right = curb_right - 1;
        int      center_col = (curb_left + curb_right + 1) >> 1;

        for (int col = 0; col < 40; col++) {
            uint16_t road_color = ((col + (scroll_y & 0x1)) & 0x1) ? COLOR_ASPHALT : COLOR_ASPHALT_ALT;

            /* 1. Player F1 Car */
            if ((y == CAR_ROW || y == CAR_ROW + 1) && (col == car_col || col == car_col + 1)) {
                int quad_x = col - car_col;
                int quad_y = y - CAR_ROW;
                draw_sprite_on_road_8x8(buffer_base, col, y, road_color, sprite_f1_player, quad_x, quad_y);
                continue;
            }

            /* Determine continuous curb span on the grass side of the road boundary */
            int prev_c_left  = (y > 0) ? curb_left_hist[y - 1]  : curb_left;
            int prev_c_right = (y > 0) ? curb_right_hist[y - 1] : curb_right;

            int left_curb_start  = (prev_c_left < curb_left)   ? (prev_c_left + 1)  : curb_left;
            int right_curb_end   = (prev_c_right > curb_right) ? (prev_c_right - 1) : curb_right;

            /* 2. Red/White Track Curbs (strictly at or outside curb_left / curb_right) */
            if ((col >= left_curb_start && col <= curb_left) ||
                (col >= curb_right      && col <= right_curb_end)) {
                uint16_t curb_color = ((scroll_y >> 1) & 0x1) ? COLOR_CURB_RED : COLOR_CURB_WHITE;
                draw_solid_8x8(buffer_base, col, y, curb_color);
                continue;
            }

            /* 3. Off-Track Grass & Scenery (anything outside the curb bridge) */
            if (col < left_curb_start || col > right_curb_end) {
                if ((col == 2 || col == 6 || col == 33 || col == 37) && ((scroll_y & 0x7) == 0)) {
                    draw_packed_tile_8x8(buffer_base, col, y, tile_grass_bush_packed);
                } else {
                    draw_packed_tile_8x8(buffer_base, col, y, tile_grass_plain_packed);
                }
                continue;
            }
            /* 4. Inside Roadway: Draw 2x2 Obstacle Car ONLY for isolated 2-wide blocks inside road */
            int bit_pos = 39 - col;
            uint8_t is_solid = (row_bits >> bit_pos) & 0x1;
            if (is_solid && col > road_left && col < road_right) {
                uint8_t left_solid  = ((row_bits >> (bit_pos + 1)) & 0x1);
                uint8_t right_solid = ((row_bits >> (bit_pos - 1)) & 0x1);

                /* Must be paired horizontally (2 columns wide) to be a valid 2x2 traffic car */
                if (left_solid || right_solid) {
                    uint8_t top_solid = (y > 0)
                        ? ((screen_rows[y - 1] >> bit_pos) & 0x1)
                        : (((screen_rows[1] >> bit_pos) & 0x1) ? 0 : 1);

                    int quad_x = left_solid ? 1 : 0;
                    int quad_y = top_solid  ? 1 : 0;

                    int obs_origin_col = col - quad_x;
                    const uint16_t (*obs_sprite)[16] = (obs_origin_col & 0x2) ? sprite_obs_blue : sprite_obs_yellow;

                    draw_sprite_on_road_8x8(buffer_base, col, y, road_color, obs_sprite, quad_x, quad_y);
                    continue;
                }
            }

            /* 5. Empty Drivable Asphalt + Dynamic Center Dashed Line */
            draw_solid_8x8(buffer_base, col, y, road_color);

            if (col == center_col && (((y - phase) & 0x3) != 0)) {
                int start_x = (col << 3) + 3;
                int start_y = y << 3;
                uint32_t dash_packed = PACK32(COLOR_DASH_YELLOW, COLOR_DASH_YELLOW);
                for (int dy = 0; dy < 8; dy++) {
                    uint32_t offset = ((start_y + dy) << 10) + (start_x << 1);
                    IOWR_32DIRECT(buffer_base, offset, dash_packed);
                }
            }
        }
    }
}


// Helper to Reset Track & Curb History Arrays

void reset_track_state(void) {
    for (int y = 0; y < 30; y++) {
        screen_rows[y]     = EMPTY_ROAD_ROW;
        curb_left_hist[y]  = 12;
        curb_right_hist[y] = 27;
    }
}


// Main Game Loop with Constant Speed & Survival Score

int main(void) {
    uint32_t pixel_buf_base = SRAM_0_BASE;
    uint32_t char_buf_base  = VIDEO_CHARACTER_BUFFER_0_AVALON_CHAR_BUFFER_SLAVE_BASE;

    IOWR_32DIRECT(VIDEO_PIXEL_BUFFER_DMA_0_BASE, 0, pixel_buf_base);
    IOWR_32DIRECT(VIDEO_PIXEL_BUFFER_DMA_0_BASE, 4, pixel_buf_base);

    reset_track_state();

    uint8_t  row_cnt    = 0;
    int      car_col    = 19;
    uint32_t frame_tick = 0;
    uint32_t score      = 0;
    uint32_t best_score = 0;

    hud_init(char_buf_base);
    hud_update_score(char_buf_base, score, best_score);

    while (1) {
        uint32_t keys = IORD_ALTERA_AVALON_PIO_DATA(KEYS_BASE);
        if ((keys & 0x2) == 0 && car_col > CAR_MIN_COL) {
            car_col--;
        }
        if ((keys & 0x1) == 0 && car_col < CAR_MAX_COL) {
            car_col++;
        }

        uint64_t next_row = fetch_next_rom_row(&row_cnt);
        scroll_track_down(next_row);

        uint64_t car_mask = (0x3ULL << (38 - car_col));
        if ((screen_rows[CAR_ROW] & car_mask) != 0) {
            if (score > best_score) {
                best_score = score;
            }

            render_frame(pixel_buf_base, car_col, frame_tick);
            hud_update_score(char_buf_base, score, best_score);
            char_put_str(char_buf_base, 2, 17, "CRASHED!");

            usleep(1000000); /* 1.0 second pause on crash */

            reset_track_state();
            car_col = 19;
            row_cnt = 0;
            score   = 0;

            hud_init(char_buf_base);
            hud_update_score(char_buf_base, score, best_score);
            continue;
        }

        /* Increment score every survived tick (+10 pts per row = 200 pts/sec) */
        score += 10;

        render_frame(pixel_buf_base, car_col, frame_tick);
        hud_update_score(char_buf_base, score, best_score);

        frame_tick++;
        usleep(50000);
    }

    return 0;
}
