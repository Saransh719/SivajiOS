#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#define FB_WIDTH  80
#define FB_HEIGHT 25

#define FB_BLACK       0
#define FB_BLUE        1
#define FB_GREEN       2
#define FB_CYAN        3
#define FB_RED         4
#define FB_MAGENTA     5
#define FB_BROWN       6
#define FB_LIGHT_GREY  7
#define FB_DARK_GREY   8
#define FB_WHITE       15

void fb_clear(unsigned char fg, unsigned char bg);

void fb_move_cursor(unsigned short pos);

void fb_move_cursor(unsigned short pos);

int fb_write(char *buf, unsigned int len);

void fb_write_cell(
    unsigned int i,
    char c,
    unsigned char fg,
    unsigned char bg
);

void fb_write_string(
    const char *str,
    unsigned int row,
    unsigned int col,
    unsigned char fg,
    unsigned char bg
);

#endif