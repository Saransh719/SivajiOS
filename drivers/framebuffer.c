
#include "framebuffer.h"
#include "../multiboot.h"
#include "../io/io.h"
#include "../mm/paging.h"

#define CELL_WIDTH  12
#define CELL_HEIGHT 24

#define FONT_SCALE 2
#define FONT_WIDTH 5
#define FONT_HEIGHT 7

#define FB_BACKEND_NONE 0
#define FB_BACKEND_VGA  1
#define FB_BACKEND_RGB  2

#define FB_COMMAND_PORT         0x3D4
#define FB_DATA_PORT            0x3D5

#define FB_HIGH_BYTE_COMMAND    14
#define FB_LOW_BYTE_COMMAND     15

static unsigned int fb_backend = FB_BACKEND_NONE;
static unsigned int cursor_row = 0;
static unsigned int cursor_col = 0;

static volatile unsigned short *const vga =
    (volatile unsigned short *)0x000B8000;


static volatile uint32_t *framebuffer;

static uint32_t fb_pitch;
static uint32_t fb_width;
static uint32_t fb_height;

static uint8_t red_position;
static uint8_t green_position;
static uint8_t blue_position;

/* VGA-compatible 16-color palette, represented as RGB. */
static const uint8_t colors[16][3] = {
    {  0,   0,   0},   // Black
    {  0,   0, 170},   // Blue
    {  0, 170,   0},   // Green
    {  0, 170, 170},   // Cyan
    {170,   0,   0},   // Red
    {170,   0, 170},   // Magenta
    {170,  85,   0},   // Brown
    {170, 170, 170},   // Light grey
    { 85,  85,  85},   // Dark grey
    { 85,  85, 255},
    { 85, 255,  85},
    { 85, 255, 255},
    {255,  85,  85},
    {255,  85, 255},
    {255, 255,  85},
    {255, 255, 255}    // White
};

/*
 * Returns one column of a 5x7 character.
 * Each bit represents one vertical pixel.
 */
static uint8_t font_column(char c, unsigned int column)
{
    static const uint8_t letters[26][5] = {
        {0x7E,0x11,0x11,0x11,0x7E}, // A
        {0x7F,0x49,0x49,0x49,0x36}, // B
        {0x3E,0x41,0x41,0x41,0x22}, // C
        {0x7F,0x41,0x41,0x22,0x1C}, // D
        {0x7F,0x49,0x49,0x49,0x41}, // E
        {0x7F,0x09,0x09,0x09,0x01}, // F
        {0x3E,0x41,0x49,0x49,0x7A}, // G
        {0x7F,0x08,0x08,0x08,0x7F}, // H
        {0x00,0x41,0x7F,0x41,0x00}, // I
        {0x20,0x40,0x41,0x3F,0x01}, // J
        {0x7F,0x08,0x14,0x22,0x41}, // K
        {0x7F,0x40,0x40,0x40,0x40}, // L
        {0x7F,0x02,0x0C,0x02,0x7F}, // M
        {0x7F,0x04,0x08,0x10,0x7F}, // N
        {0x3E,0x41,0x41,0x41,0x3E}, // O
        {0x7F,0x09,0x09,0x09,0x06}, // P
        {0x3E,0x41,0x51,0x21,0x5E}, // Q
        {0x7F,0x09,0x19,0x29,0x46}, // R
        {0x46,0x49,0x49,0x49,0x31}, // S
        {0x01,0x01,0x7F,0x01,0x01}, // T
        {0x3F,0x40,0x40,0x40,0x3F}, // U
        {0x1F,0x20,0x40,0x20,0x1F}, // V
        {0x3F,0x40,0x38,0x40,0x3F}, // W
        {0x63,0x14,0x08,0x14,0x63}, // X
        {0x07,0x08,0x70,0x08,0x07}, // Y
        {0x61,0x51,0x49,0x45,0x43}  // Z
    };

    static const uint8_t digits[10][5] = {
        {0x3E,0x51,0x49,0x45,0x3E}, // 0
        {0x00,0x42,0x7F,0x40,0x00}, // 1
        {0x62,0x51,0x49,0x49,0x46}, // 2
        {0x22,0x49,0x49,0x49,0x36}, // 3
        {0x18,0x14,0x12,0x7F,0x10}, // 4
        {0x2F,0x49,0x49,0x49,0x31}, // 5
        {0x3E,0x49,0x49,0x49,0x32}, // 6
        {0x01,0x71,0x09,0x05,0x03}, // 7
        {0x36,0x49,0x49,0x49,0x36}, // 8
        {0x26,0x49,0x49,0x49,0x3E}  // 9
    };

    if (column >= 5)
        return 0;

    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';

    if (c >= 'A' && c <= 'Z')
        return letters[c - 'A'][column];

    if (c >= '0' && c <= '9')
        return digits[c - '0'][column];

    switch (c) {
        case '!': {
            static const uint8_t g[5] = {0,0,0x5F,0,0};
            return g[column];
        }
        case '.': {
            static const uint8_t g[5] = {0,0x60,0x60,0,0};
            return g[column];
        }
        case ',': {
            static const uint8_t g[5] = {0,0x40,0x20,0,0};
            return g[column];
        }
        case ':': {
            static const uint8_t g[5] = {0,0x36,0x36,0,0};
            return g[column];
        }
        case '-': {
            static const uint8_t g[5] = {0x08,0x08,0x08,0x08,0x08};
            return g[column];
        }
        case '_': {
            static const uint8_t g[5] = {0x40,0x40,0x40,0x40,0x40};
            return g[column];
        }
        case '/': {
            static const uint8_t g[5] = {0x60,0x18,0x06,0x01,0};
            return g[column];
        }
        case '?': {
            static const uint8_t g[5] = {0x02,0x01,0x51,0x09,0x06};
            return g[column];
        }
        case '(': {
            static const uint8_t g[5] = {0,0x1C,0x22,0x41,0};
            return g[column];
        }
        case ')': {
            static const uint8_t g[5] = {0,0x41,0x22,0x1C,0};
            return g[column];
        }
        default:
            return 0;
    }
}

static void put_pixel(
    uint32_t x,
    uint32_t y,
    uint8_t red,
    uint8_t green,
    uint8_t blue
)
{
    if (!framebuffer || x >= fb_width || y >= fb_height)
        return;

    uint32_t color =
        ((uint32_t)red << red_position) |
        ((uint32_t)green << green_position) |
        ((uint32_t)blue << blue_position);

    uint32_t offset = (y * fb_pitch + x * 4) / 4;

    framebuffer[offset] = color;
}

/* Called once before using the framebuffer functions. */
int fb_init(const struct multiboot_info *mbi)
{
    framebuffer = 0;
    cursor_row = 0;
    cursor_col = 0;

    /* Check whether GRUB supplied a graphical framebuffer. */
    if (!(mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER))
    {
        fb_backend = FB_BACKEND_VGA;
        return 1;
    }

    if (mbi->framebuffer_type != 1 ||
        mbi->framebuffer_bpp != 32 ||
        mbi->framebuffer_addr > 0xFFFFFFFFULL)
    {
        fb_backend = FB_BACKEND_VGA;
        return 1;
    }

    fb_width = mbi->framebuffer_width;
    fb_height = mbi->framebuffer_height;
    fb_pitch = mbi->framebuffer_pitch;

    if (fb_width < FB_WIDTH * CELL_WIDTH ||
        fb_height < FB_HEIGHT * CELL_HEIGHT ||
        fb_pitch < fb_width * 4)
    {
        fb_backend = FB_BACKEND_VGA;
        return 1;
    }

    /* QEMU UEFI XRGB8888 layout, verified in our test. */
    red_position = 16;
    green_position = 8;
    blue_position = 0;

    framebuffer =
        (volatile uint32_t *)paging_physical_to_framebuffer_virtual(
            (uint32_t)mbi->framebuffer_addr);

    fb_backend = FB_BACKEND_RGB;

    return 1;
}

void fb_move_cursor(unsigned short pos)
{
    outb(FB_COMMAND_PORT, FB_HIGH_BYTE_COMMAND);
    outb(FB_DATA_PORT, (pos >> 8) & 0x00FF);

    outb(FB_COMMAND_PORT, FB_LOW_BYTE_COMMAND);
    outb(FB_DATA_PORT, pos & 0x00FF);
}

static void fb_scroll(void)
{
    if (fb_backend == FB_BACKEND_VGA)
    {
        for (unsigned int i = 0;
             i < FB_WIDTH * (FB_HEIGHT - 1);
             i++)
        {
            vga[i] = vga[i + FB_WIDTH];
        }
    }
    else if (fb_backend == FB_BACKEND_RGB)
    {
        for (unsigned int y = 0;
             y < (FB_HEIGHT - 1) * CELL_HEIGHT;
             y++)
        {
            for (unsigned int x = 0; x < FB_WIDTH * CELL_WIDTH; x++)
            {
                unsigned int dst = (y * fb_pitch + x * 4) / 4;
                unsigned int src =
                    ((y + CELL_HEIGHT) * fb_pitch + x * 4) / 4;

                framebuffer[dst] = framebuffer[src];
            }
        }
    }

    /* Clear the last row. */
    for (unsigned int col = 0; col < FB_WIDTH; col++)
    {
        fb_write_cell(
            (FB_HEIGHT - 1) * FB_WIDTH + col,
            ' ',
            FB_WHITE,
            FB_BLACK
        );
    }

    cursor_row = FB_HEIGHT - 1;
    cursor_col = 0;
}

void fb_backspace(void)
{
    // Nothing to erase at the beginning of the screen.
    if (cursor_row == 0 && cursor_col == 0)
        return;

    // Move to the previous character position.
    if (cursor_col == 0) {
        cursor_row--;
        cursor_col = FB_WIDTH - 1;
    } else {
        cursor_col--;
    }

    // Erase the character.
    fb_write_cell(
        cursor_row * FB_WIDTH + cursor_col,
        ' ',
        FB_WHITE,
        FB_BLACK
    );

    // Update VGA hardware cursor.
    if (fb_backend == FB_BACKEND_VGA) {
        fb_move_cursor(cursor_row * FB_WIDTH + cursor_col);
    }
}

int fb_write(char *buf, unsigned int len)
{
    if (!buf)
        return -1;

    for (unsigned int i = 0; i < len; i++)
    {
        char c = buf[i];

        if (c == '\n')
        {
            cursor_col = 0;
            cursor_row++;
        }
        else
        {
            fb_write_cell(
                cursor_row * FB_WIDTH + cursor_col,
                c,
                FB_WHITE,
                FB_BLACK
            );

            cursor_col++;

            if (cursor_col >= FB_WIDTH)
            {
                cursor_col = 0;
                cursor_row++;
            }
        }

        if (cursor_row >= FB_HEIGHT)
            fb_scroll();
    }

    /* Update the hardware cursor in VGA text mode. */
    if (fb_backend == FB_BACKEND_VGA)
    {
        fb_move_cursor(
            cursor_row * FB_WIDTH + cursor_col
        );
    }

    return (int)len;
}

void fb_write_cell(
    unsigned int i,
    char c,
    unsigned char fg,
    unsigned char bg
)
{
    if (i >= FB_WIDTH * FB_HEIGHT)
        return;

    if (fb_backend == FB_BACKEND_VGA)
    {
        unsigned char attribute =
            ((bg & 0x0F) << 4) | (fg & 0x0F);
    
        vga[i] = (unsigned short)c |
                 ((unsigned short)attribute << 8);
    
        return;
    }
    
    if (fb_backend != FB_BACKEND_RGB)
        return;

    if (i >= FB_WIDTH * FB_HEIGHT)
        return;

    unsigned int row = i / FB_WIDTH;
    unsigned int col = i % FB_WIDTH;

    unsigned int start_x = col * CELL_WIDTH;
    unsigned int start_y = row * CELL_HEIGHT;

    if (c == '\0')
        c = ' ';

    for (unsigned int y = 0; y < CELL_HEIGHT; y++) {
        for (unsigned int x = 0; x < CELL_WIDTH; x++) {

            int pixel_on = 0;

            if (x >= 1 && x < 11 &&
                y >= 5 && y < 19) {

                unsigned int font_x = (x - 1) / FONT_SCALE;
                unsigned int font_y = (y - 5) / FONT_SCALE;

                if (font_x < FONT_WIDTH && font_y < FONT_HEIGHT) {
                    uint8_t column = font_column(c, font_x);
                    pixel_on = (column >> font_y) & 1;
                }
            }

            unsigned char *unused = 0;
            (void)unused;

            uint8_t red = colors[pixel_on ? fg & 0x0F : bg & 0x0F][0];
            uint8_t green = colors[pixel_on ? fg & 0x0F : bg & 0x0F][1];
            uint8_t blue = colors[pixel_on ? fg & 0x0F : bg & 0x0F][2];

            put_pixel(
                start_x + x,
                start_y + y,
                red,
                green,
                blue
            );
        }
    }
}

void fb_clear(unsigned char fg, unsigned char bg)
{
    for (unsigned int i = 0; i < FB_WIDTH * FB_HEIGHT; i++)
        fb_write_cell(i, ' ', fg, bg);
    cursor_row = 0;
    cursor_col = 0;
    if (fb_backend == FB_BACKEND_VGA) fb_move_cursor(0);
}

void fb_write_string(
    const char *str,
    unsigned int row,
    unsigned int col,
    unsigned char fg,
    unsigned char bg
)
{
    if (!str || row >= FB_HEIGHT || col >= FB_WIDTH)
        return;

    while (*str && row < FB_HEIGHT) {

        if (*str == '\n') {
            row++;
            col = 0;
            str++;
            continue;
        }

        if (col >= FB_WIDTH) {
            row++;
            col = 0;

            if (row >= FB_HEIGHT)
                break;
        }

        fb_write_cell(
            row * FB_WIDTH + col,
            *str,
            fg,
            bg
        );

        col++;
        str++;
    }
}
