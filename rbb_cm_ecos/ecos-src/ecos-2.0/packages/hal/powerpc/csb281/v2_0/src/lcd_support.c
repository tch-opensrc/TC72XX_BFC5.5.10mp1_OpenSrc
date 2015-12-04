//==========================================================================
//
//        Lcd_support.c
//
//        Cogent CSB281 - LCD/CRT support routines
//
//==========================================================================
//####ECOSGPLCOPYRIGHTBEGIN####
// -------------------------------------------
// This file is part of eCos, the Embedded Configurable Operating System.
// Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
// Copyright (C) 2003 Gary Thomas
//
// eCos is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 or (at your option) any later version.
//
// eCos is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License along
// with eCos; if not, write to the Free Software Foundation, Inc.,
// 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
//
// As a special exception, if other files instantiate templates or use macros
// or inline functions from this file, or you compile this file and link it
// with other works to produce a work based on this file, this file does not
// by itself cause the resulting work to be covered by the GNU General Public
// License. However the source code for this file must still be made available
// in accordance with section (3) of the GNU General Public License.
//
// This exception does not invalidate any other reasons why a work based on
// this file might be covered by the GNU General Public License.
//
// Alternative licenses for eCos may be arranged by contacting Red Hat, Inc.
// at http://sources.redhat.com/ecos/ecos-license/
// -------------------------------------------
//####ECOSGPLCOPYRIGHTEND####
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):     gthomas
// Contributors:  gthomas
// Date:          2001-11-03
// Description:   Simple LCD support
//####DESCRIPTIONEND####

#include <pkgconf/hal.h>

#include <cyg/infra/diag.h>
#include <cyg/hal/hal_io.h>       // IO macros
#include <cyg/hal/hal_if.h>       // Virtual vector support
#include <cyg/hal/hal_arch.h>     // Register state info
#include <cyg/hal/hal_intr.h>     // HAL interrupt macros

#include <cyg/hal/lcd_support.h>
#include <cyg/hal/hal_cache.h>

#include <string.h>

#ifdef CYGPKG_ISOINFRA
# include <pkgconf/isoinfra.h>
# ifdef CYGINT_ISO_STDIO_FORMATTED_IO
#  include <stdio.h>  // sscanf
# endif
#endif

#ifndef FALSE
#define FALSE 0
#define TRUE  1
#endif

// Logical layout
#define LCD_WIDTH  640
#define LCD_HEIGHT 480
#define LCD_DEPTH   16

#define RGB_RED(x)   (((x)&0x1F)<<11)
#define RGB_GREEN(x) (((x)&0x3F)<<5)
#define RGB_BLUE(x)  (((x)&0x1F)<<0)

// Black on light blue
static int bg = RGB_RED(0x1F) | RGB_GREEN(0x3F) | RGB_BLUE(0x1F);
static int fg = RGB_RED(0) | RGB_GREEN(0) | RGB_BLUE(0);

static struct lcd_info lcd;

int sed135x_init(int depth, struct lcd_info *info);

// Compute the location for a pixel within the framebuffer
static cyg_uint16 *
lcd_fb(int row, int col)
{
    cyg_uint16 *res = (cyg_uint16 *)((char *)lcd.fb+(row*lcd.rlen)+(col*lcd.stride));
    return res;
}

void
lcd_on(bool enable)
{
    static bool on = false;

    if (enable) {
        if (!on) {
            (*lcd.on)();
        }
        on = true;
    } else {
//        (*lcd.off)();
    }
}

// Initialize LCD hardware

void
lcd_init(int depth)
{
    sed135x_init(depth, &lcd);
}

// Get information about the frame buffer
int
lcd_getinfo(struct lcd_info *info)
{
    if (lcd.bpp == 0) {
        return 0;  // LCD not initialized
    }
    *info = lcd;
    return 1; // Information valid
}

// Clear screen
void
lcd_clear(void)
{
#if 0
    cyg_uint32 *fb_row0, *fb_rown;
    cyg_uint32 _bg = (bg<<16)|bg;

    fb_row0 = lcd_fb(0, 0);
    fb_rown = lcd_fb(lcd_height, 0);
    while (fb_row0 != fb_rown) {
        *fb_row0++ = _bg;
    }
#endif
}

#ifdef CYGSEM_CSB281_LCD_COMM

//
// Additional support for LCD/Keyboard as 'console' device
//

#ifdef CYGOPT_CSB281_LCD_COMM_LOGO
#include "banner.xpm"
#endif
#include "font.h"

// Virtual screen info
static int curX = 0;  // Last used position
static int curY = 0;
//static int width = LCD_WIDTH / (FONT_WIDTH*NIBBLES_PER_PIXEL);
//static int height = LCD_HEIGHT / (FONT_HEIGHT*SCREEN_SCALE);

#define SCREEN_PAN            20
#define SCREEN_WIDTH          80
#define SCREEN_HEIGHT         (LCD_HEIGHT/FONT_HEIGHT)
#define VISIBLE_SCREEN_WIDTH  (LCD_WIDTH/FONT_WIDTH)
#define VISIBLE_SCREEN_HEIGHT (LCD_HEIGHT/FONT_HEIGHT)
static char screen[SCREEN_HEIGHT][SCREEN_WIDTH];
static int screen_height = SCREEN_HEIGHT;
static int screen_width = SCREEN_WIDTH;
static int screen_pan = 0;

// Usable area on screen [logical pixel rows]
static int screen_start = 0;                       
static int screen_end = LCD_HEIGHT/FONT_HEIGHT;

static bool cursor_enable = true;

// Functions
static void lcd_drawc(cyg_int8 c, int x, int y);

// Note: val is a 16 bit, RGB555 value which must be mapped
// onto a 12 bit value.
#define RED(v)   ((v>>12) & 0x0F)
#define GREEN(v) ((v>>7) & 0x0F)
#define BLUE(v)  ((v>>1) & 0x0F)

static void
set_pixel(int row, int col, unsigned short val)
{
    unsigned short *pix = (unsigned short *)lcd_fb(row, col);
    *pix = _le16(val);
}

static int
_hexdigit(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return c - '0';
    } else {
        if ((c >= 'A') && (c <= 'F')) {
            return (c - 'A') + 0x0A;
        } else {
            if ((c >= 'a') && (c <= 'f')) {
                return (c - 'a') + 0x0a;
            }
        }
    }

    return 0;
}

static int
_hex(char *cp)
{
    return (_hexdigit(*cp)<<4) | _hexdigit(*(cp+1));
}

static unsigned short
parse_color(char *cp)
{
    int red, green, blue;

    while (*cp && (*cp != 'c')) cp++;
    if (cp) {
        cp += 2;
        if (*cp == '#') {
            red = _hex(cp+1);
            green = _hex(cp+3);
            blue = _hex(cp+5);
#define USE_RGB565
#ifdef USE_RGB565
            return RGB_RED(red>>3) | RGB_GREEN(green>>2) | RGB_BLUE(blue>>3);
#else
            return RGB_RED(red>>3) | RGB_GREEN(green>>3) | RGB_BLUE(blue>>3);
#endif
        } else {
            // Should be "None"
            return 0xFFFF;
        }
    } else {
        return 0xFFFF;
    }
}

#ifndef CYGINT_ISO_STDIO_FORMATTED_IO
static int
get_int(unsigned char **_cp)
{
    char *cp = *_cp;
    char c;
    int val = 0;
    
    while ((c = *cp++) && (c != ' ')) {
        if ((c >= '0') && (c <= '9')) {
            val = val * 10 + (c - '0');
        } else {
            return -1;
        }
    }
    *_cp = cp;
    return val;
}
#endif

#ifdef CYGOPT_CSB281_LCD_COMM_LOGO
static int
show_xpm(char *xpm[], int screen_pos)
{
    int i, row, col, offset;
    unsigned char *cp;
    int nrows, ncols, nclrs, cwid;
    unsigned short colors[256];  // Mapped by character index
    short color_value, color_index;
//#define HORRIBLY_SLOW_COLOR_MAPPING
#ifdef HORRIBLY_SLOW_COLOR_MAPPING
    bool color_match;
    short cmap[256][2];
    int j;
#else
    // Caution - this is very GIMP specific
    short cmap[256][3];  // Enough room for 256 colors
    short cmap_col, cmap_row;
    unsigned char std_cmap[] = " .+";
#endif

    cp = xpm[0];
#ifdef CYGINT_ISO_STDIO_FORMATTED_IO
    if (sscanf(cp, "%d %d %d %d", &ncols, &nrows, &nclrs, &cwid) != 4) {
#else
    if (((ncols = get_int(&cp)) < 0) ||
        ((nrows = get_int(&cp)) < 0) ||
        ((nclrs = get_int(&cp)) < 0) ||
        ((cwid = get_int(&cp)) < 0)) {

#endif
        diag_printf("Can't parse XPM data, sorry\n");
        return 0;
    }
//    diag_printf("%d rows, %d cols, %d colors %d chars/pixel\n", nrows, ncols, nclrs, cwid);
    if (cwid > 2) {
        diag_printf("Color map is too wide - %d\n", cwid);
        return 0;
    }

    for (i = 0;  i < 256;  i++) {
        colors[i] = 0x0000;
#ifdef HORRIBLY_SLOW_COLOR_MAPPING
        cmap[i][0] = 0;  cmap[i][1] = 0;
#else
        cmap[i][0] = 0;  cmap[i][1] = 0;  cmap[i][2] = 0;
#endif
    }
    color_index = 0;
    for (i = 0;  i < nclrs;  i++) {
        cp = xpm[i+1];
#ifdef HORRIBLY_SLOW_COLOR_MAPPING
        for (j = 0;  j < cwid;  j++) {
            cmap[color_index][j] = cp[j];
        }
#else
        if (cwid == 1) {
            cmap_col = 0;
        } else {
            for (cmap_col = 0;  cmap_col < 3;  cmap_col++) {
                if (cp[1] == std_cmap[cmap_col]) {
                    break;
                }
            }
        }
        cmap_row = *cp;
        cmap[cmap_row][cmap_col] = color_index;
#endif
        color_value = parse_color(cp+cwid);
        colors[color_index++] = color_value;
//        diag_printf("Color[%d] = %04x\n", color_index-1, colors[color_index-1]);
    }

#ifdef CYGOPT_CSB281_LCD_COMM_LOGO_TOP
    offset = screen_pos;
#else
    offset = screen_pos-nrows;
#endif
    for (row = 0;  row < nrows;  row++) {            
        cp = xpm[nclrs+1+row];        
        for (col = 0;  col < ncols;  col++) {
#ifdef HORRIBLY_SLOW_COLOR_MAPPING
            // Horrible, but portable, way to map colors
            for (color_index = 0;  color_index < nclrs;  color_index++) {
                color_match = true;
                for (j = 0;  j < cwid;  j++) {
                    if (cmap[color_index][j] != cp[j]) {
                        color_match = false;
                    }
                }
                if (color_match) {
                    break;
                }
            }
#else
            if (cwid == 1) {
                cmap_col = 0;
            } else {
                for (cmap_col = 0;  cmap_col < 3;  cmap_col++) {
                    if (cp[1] == std_cmap[cmap_col]) {
                        break;
                    }
                }
            }
            cmap_row = *cp;
            color_index = cmap[cmap_row][cmap_col];
#endif
            set_pixel(row+offset, col, colors[color_index]);
            cp += cwid;
        }
    }
#ifdef CYGOPT_CSB281_LCD_COMM_LOGO_TOP
    screen_start = (nrows + (FONT_HEIGHT-1))/FONT_HEIGHT;
    screen_end = LCD_HEIGHT/FONT_HEIGHT;
    return offset+nrows;
#else    
    screen_start = 0;
    screen_height = offset / FONT_HEIGHT;
    screen_end = screen_height;
    return offset;
#endif
}
#endif

void
lcd_screen_clear(void)
{
    int row, col, pos;

    for (row = 0;  row < screen_height;  row++) {
        for (col = 0;  col < screen_width;  col++) {
            screen[row][col] = ' ';
            lcd_drawc(' ', col, row);
        }
    }
#ifdef CYGOPT_CSB281_LCD_COMM_LOGO
    // Note: Row 0 seems to wrap incorrectly
#ifdef CYGOPT_CSB281_LCD_COMM_LOGO_TOP
    pos = 0;
#else
    pos = (LCD_HEIGHT-1);
#endif
    show_xpm(banner_xpm, pos);
#endif // CYGOPT_CSB281_LCD_COMM_LOGO
    curX = 0;  curY = screen_start;
    if (cursor_enable) {
        lcd_drawc(CURSOR_ON, curX-screen_pan, curY);
    }
}

// Position cursor
void
lcd_moveto(int X, int Y)
{
    if (cursor_enable) {
        lcd_drawc(screen[curY][curX], curX-screen_pan, curY);
    }
    if (X < 0) X = 0;
    if (X >= screen_width) X = screen_width-1;
    curX = X;
    if (Y < screen_start) Y = screen_start;
    if (Y >= screen_height) Y = screen_height-1;
    curY = Y;
    if (cursor_enable) {
        lcd_drawc(CURSOR_ON, curX-screen_pan, curY);
    }
}

// Render a character at position (X,Y) with current background/foreground
static void
lcd_drawc(cyg_int8 c, int x, int y)
{
    cyg_uint8 bits;
    int l, p;
    int xoff, yoff;

    if ((x < 0) || (x >= VISIBLE_SCREEN_WIDTH) || 
        (y < 0) || (y >= screen_height)) return;  
    for (l = 0;  l < FONT_HEIGHT;  l++) {
        bits = font_table[c-FIRST_CHAR][l]; 
        yoff = y*FONT_HEIGHT + l;
        xoff = x*FONT_HEIGHT;
        for (p = 0;  p < FONT_WIDTH;  p++) {
            set_pixel(yoff, xoff + p, (bits & 0x01) ? fg : bg);
            bits >>= 1;
        }
    }
}

static void
lcd_refresh(void)
{
    int row, col;

    for (row = screen_start;  row < screen_height;  row++) {
        for (col = 0;  col < VISIBLE_SCREEN_WIDTH;  col++) {
            if ((col+screen_pan) < screen_width) {
                lcd_drawc(screen[row][col+screen_pan], col, row);
            } else {
                lcd_drawc(' ', col, row);
            }
        }
    }
    if (cursor_enable) {
        lcd_drawc(CURSOR_ON, curX-screen_pan, curY);
    }
}

static void
lcd_scroll(void)
{
    int col;
    cyg_uint8 *c1;
    cyg_uint32 *lc0, *lc1, *lcn;
    cyg_uint16 *fb_row0, *fb_row1, *fb_rown;

    // First scroll up the virtual screen
#if ((SCREEN_WIDTH%4) != 0)
#error Scroll code optimized for screen with multiple of 4 columns
#endif
    lc0 = (cyg_uint32 *)&screen[0][0];
    lc1 = (cyg_uint32 *)&screen[1][0];
    lcn = (cyg_uint32 *)&screen[screen_height][0];
    while (lc1 != lcn) {
        *lc0++ = *lc1++;
    }
    c1 = &screen[screen_height-1][0];
    for (col = 0;  col < screen_width;  col++) {
        *c1++ = 0x20;
    }
    fb_row0 = lcd_fb(screen_start*FONT_HEIGHT, 0);
    fb_row1 = lcd_fb((screen_start+1)*FONT_HEIGHT, 0);
    fb_rown = lcd_fb(screen_end*FONT_HEIGHT, 0);
    while (fb_row1 != fb_rown) {
        *fb_row0 = *fb_row1;
        fb_row0 = (cyg_uint16 *)((char *)fb_row0 + lcd.stride);
        fb_row1 = (cyg_uint16 *)((char *)fb_row1 + lcd.stride);
    }
    // Erase bottom line
    for (col = 0;  col < screen_width;  col++) {
        lcd_drawc(' ', col, screen_end-1);
    }
}

// Draw one character at the current position
void
lcd_putc(cyg_int8 c)
{
// TEMP
    int cache_on;
    HAL_DCACHE_IS_ENABLED(cache_on);
    if (cache_on) {
        HAL_DCACHE_SYNC();
        HAL_DCACHE_DISABLE();
    }
// TEMP
    if (cursor_enable) {
        lcd_drawc(screen[curY][curX], curX-screen_pan, curY);
    }
    switch (c) {
    case '\r':
        curX = 0;
        break;
    case '\n':
        curY++;
        break;
    case '\b':
        curX--;
        if (curX < 0) {
            curY--;
            if (curY < 0) curY = 0;
            curX = screen_width-1;
        }
        break;
    default:
        if (((cyg_uint8)c < FIRST_CHAR) || ((cyg_uint8)c > LAST_CHAR)) c = '.';
        screen[curY][curX] = c;
        lcd_drawc(c, curX-screen_pan, curY);
        curX++;
        if (curX == screen_width) {
            curY++;
            curX = 0;
        }
    } 
    if (curY >= screen_height) {
        lcd_scroll();
        curY = (screen_height-1);
    }
    if (cursor_enable) {
        lcd_drawc(CURSOR_ON, curX-screen_pan, curY);
    }
// TEMP
    if (cache_on) {
        HAL_DCACHE_ENABLE();
    }
// TEMP
}

// Basic LCD 'printf()' support

#include <stdarg.h>

#define is_digit(c) ((c >= '0') && (c <= '9'))

static int
_cvt(unsigned long val, char *buf, long radix, char *digits)
{
    char temp[80];
    char *cp = temp;
    int length = 0;

    if (val == 0) {
        /* Special case */
        *cp++ = '0';
    } else {
        while (val) {
            *cp++ = digits[val % radix];
            val /= radix;
        }
    }
    while (cp != temp) {
        *buf++ = *--cp;
        length++;
    }
    *buf = '\0';
    return (length);
}

static int
lcd_vprintf(void (*putc)(cyg_int8), const char *fmt0, va_list ap)
{
    char c, sign, *cp;
    int left_prec, right_prec, zero_fill, length, pad, pad_on_right;
    char buf[32];
    long val;

    while ((c = *fmt0++)) {
        cp = buf;
        length = 0;
        if (c == '%') {
            c = *fmt0++;
            left_prec = right_prec = pad_on_right = 0;
            if (c == '-') {
                c = *fmt0++;
                pad_on_right++;
            }
            if (c == '0') {
                zero_fill = TRUE;
                c = *fmt0++;
            } else {
                zero_fill = FALSE;
            }
            while (is_digit(c)) {
                left_prec = (left_prec * 10) + (c - '0');
                c = *fmt0++;
            }
            if (c == '.') {
                c = *fmt0++;
                zero_fill++;
                while (is_digit(c)) {
                    right_prec = (right_prec * 10) + (c - '0');
                    c = *fmt0++;
                }
            } else {
                right_prec = left_prec;
            }
            sign = '\0';
            switch (c) {
            case 'd':
            case 'x':
            case 'X':
                val = va_arg(ap, long);
                switch (c) {
                case 'd':
                    if (val < 0) {
                        sign = '-';
                        val = -val;
                    }
                    length = _cvt(val, buf, 10, "0123456789");
                    break;
                case 'x':
                    length = _cvt(val, buf, 16, "0123456789abcdef");
                    break;
                case 'X':
                    length = _cvt(val, buf, 16, "0123456789ABCDEF");
                    break;
                }
                break;
            case 's':
                cp = va_arg(ap, char *);
                length = strlen(cp);
                break;
            case 'c':
                c = va_arg(ap, long /*char*/);
                (*putc)(c);
                continue;
            default:
                (*putc)('?');
            }
            pad = left_prec - length;
            if (sign != '\0') {
                pad--;
            }
            if (zero_fill) {
                c = '0';
                if (sign != '\0') {
                    (*putc)(sign);
                    sign = '\0';
                }
            } else {
                c = ' ';
            }
            if (!pad_on_right) {
                while (pad-- > 0) {
                    (*putc)(c);
                }
            }
            if (sign != '\0') {
                (*putc)(sign);
            }
            while (length-- > 0) {
                (*putc)(c = *cp++);
                if (c == '\n') {
                    (*putc)('\r');
                }
            }
            if (pad_on_right) {
                while (pad-- > 0) {
                    (*putc)(' ');
                }
            }
        } else {
            (*putc)(c);
            if (c == '\n') {
                (*putc)('\r');
            }
        }
    }

    // FIXME
    return 0;
}

int
_lcd_printf(char const *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = lcd_vprintf(lcd_putc, fmt, ap);
    va_end(ap);
    return (ret);
}

void
lcd_setbg(int red, int green, int blue)
{
    bg = RGB_RED(red) | RGB_GREEN(green) | RGB_BLUE(blue);
}

void
lcd_setfg(int red, int green, int blue)
{
    fg = RGB_RED(red) | RGB_GREEN(green) | RGB_BLUE(blue);
}

//
// Support LCD/keyboard (PS2) as a virtual I/O channel
//   Adapted from i386/pcmb_screen.c
//


//-----------------------------------------------------------------------------
// Keyboard definitions

#define KBDATAPORT      0x79000003    // data I/O port
#define KBCMDPORT       0x79000007    // command port (write)
#define KBSTATPORT      0x79000007    // status port  (read)
#define KBINRDY         0x01
#define KBOUTRDY        0x02
#define KBTXTO          0x40          // Transmit timeout - nothing there
#define KBTEST          0xAB

// Scan codes

#define LSHIFT    0x2a
#define RSHIFT    0x36
#define CTRL      0x1d
#define ALT       0x38
#define CAPS      0x3a
#define NUMS      0x45

#define BREAK     0x80

// Bits for KBFlags

#define KBNormal  0x0000
#define KBShift   0x0001
#define KBCtrl    0x0002
#define KBAlt     0x0004
#define KBIndex   0x0007  // mask for the above

#define KBExtend   0x0010
#define KBAck      0x0020
#define KBResend   0x0040
#define KBShiftL   (0x0080 | KBShift)
#define KBShiftR   (0x0100 | KBShift)
#define KBCtrlL    (0x0200 | KBCtrl)
#define KBCtrlR    (0x0400 | KBCtrl)
#define KBAltL     (0x0800 | KBAlt)
#define KBAltR     (0x1000 | KBAlt)
#define KBCapsLock 0x2000
#define KBNumLock  0x4000

#define KBArrowUp       0x48
#define KBArrowRight    0x4D
#define KBArrowLeft     0x4B
#define KBArrowDown     0x50

//-----------------------------------------------------------------------------
// Keyboard Variables

static  int  KBFlags = 0;

static  CYG_BYTE  KBPending = 0xFF;

static  CYG_BYTE  KBScanTable[128][4] = {
//  Normal    Shift    Control    Alt
// 0x00
    {  0xFF,    0xFF,    0xFF,    0xFF,   },
    {  0x1b,    0x1b,    0x1b,    0xFF,  },
    {  '1',    '!',    0xFF,    0xFF,  },
    {  '2',    '"',    0xFF,    0xFF,  },
    {  '3',    '#',    0xFF,    0xFF,  },
    {  '4',    '$',    0xFF,    0xFF,  },
    {  '5',    '%',    0xFF,    0xFF,  },
    {  '6',    '^',    0xFF,    0xFF,  },
    {  '7',    '&',    0xFF,    0xFF,  },
    {  '8',    '*',    0xFF,    0xFF,  },
    {  '9',    '(',    0xFF,    0xFF,  },
    {  '0',    ')',    0xFF,    0xFF,  },
    {  '-',    '_',    0xFF,    0xFF,  },
    {  '=',    '+',    0xFF,    0xFF,  },
    {  '\b',    '\b',    0xFF,    0xFF,  },
    {  '\t',    '\t',    0xFF,    0xFF,  },
// 0x10
    {  'q',    'Q',    0x11,    0xFF,  },
    {  'w',    'W',    0x17,    0xFF,  },
    {  'e',    'E',    0x05,    0xFF,  },
    {  'r',    'R',    0x12,    0xFF,  },
    {  't',    'T',    0x14,    0xFF,  },
    {  'y',    'Y',    0x19,    0xFF,  },
    {  'u',    'U',    0x15,    0xFF,  },
    {  'i',    'I',    0x09,    0xFF,  },
    {  'o',    'O',    0x0F,    0xFF,  },
    {  'p',    'P',    0x10,    0xFF,  },
    {  '[',    '{',    0x1b,    0xFF,  },
    {  ']',    '}',    0x1d,    0xFF,  },
    {  '\r',    '\r',    '\n',    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  'a',    'A',    0x01,    0xFF,  },
    {  's',    'S',    0x13,    0xFF,  },
// 0x20
    {  'd',    'D',    0x04,    0xFF,  },
    {  'f',    'F',    0x06,    0xFF,  },
    {  'g',    'G',    0x07,    0xFF,  },
    {  'h',    'H',    0x08,    0xFF,  },
    {  'j',    'J',    0x0a,    0xFF,  },
    {  'k',    'K',    0x0b,    0xFF,  },
    {  'l',    'L',    0x0c,    0xFF,  },
    {  ';',    ':',    0xFF,    0xFF,  },
    {  0x27,    '@',    0xFF,    0xFF,  },
    {  '#',    '~',    0xFF,    0xFF,  },
    {  '`',    '~',    0xFF,    0xFF,  },
    {  '\\',    '|',    0x1C,    0xFF,  },
    {  'z',    'Z',    0x1A,    0xFF,  },
    {  'x',    'X',    0x18,    0xFF,  },
    {  'c',    'C',    0x03,    0xFF,  },
    {  'v',    'V',    0x16,    0xFF,  },
// 0x30
    {  'b',    'B',    0x02,    0xFF,  },
    {  'n',    'N',    0x0E,    0xFF,  },
    {  'm',    'M',    0x0D,    0xFF,  },
    {  ',',    '<',    0xFF,    0xFF,  },
    {  '.',    '>',    0xFF,    0xFF,  },
    {  '/',    '?',    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  ' ',    ' ',    ' ',    ' ',  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xF1,    0xE1,    0xFF,    0xFF,  },
    {  0xF2,    0xE2,    0xFF,    0xFF,  },
    {  0xF3,    0xE3,    0xFF,    0xFF,  },
    {  0xF4,    0xE4,    0xFF,    0xFF,  },
    {  0xF5,    0xE5,    0xFF,    0xFF,  },
// 0x40
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },

    {  0x15,    0x15,    0x15,    0x15,  },
    {  0x10,    0x10,    0x10,    0x10,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
// 0x50
    {  0x04,    0x04,    0x04,    0x04,  },
    {  0x0e,    0x0e,    0x0e,    0x0e,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
// 0x60
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
// 0x70
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
    {  0xFF,    0xFF,    0xFF,    0xFF,  },
};

static int KBIndexTab[8] = { 0, 1, 2, 2, 3, 3, 3, 3 };

//-----------------------------------------------------------------------------

static __inline__ cyg_uint8
inb(cyg_uint32 port)
{
    cyg_uint8 val;
    HAL_READ_UINT8(port, val);
    return val;
}

static __inline__ void
outb(cyg_uint32 port, cyg_uint8 val)
{
    HAL_WRITE_UINT8(port, val);
}

static cyg_bool
KeyboardInit(void)
{
    unsigned char c, s;

    /* flush input queue */
    while ((inb(KBSTATPORT) & KBINRDY)) {
        (void)inb(KBDATAPORT);
    }

    /* Send self-test - controller local */
    while (inb(KBSTATPORT) & KBOUTRDY) ;
    outb(KBCMDPORT,0xAA);
    while ((inb(KBSTATPORT) & KBINRDY) == 0) ; /* wait input ready */
    if ((c = inb(KBDATAPORT)) != 0x55) {
#ifdef DEBUG_KBD_INIT
        diag_printf("Keyboard self test failed - result: %x\n", c);
#endif
        return false;
    }

    /* Enable interrupts and keyboard controller */
    while (inb(KBSTATPORT) & KBOUTRDY) ;
    outb(KBCMDPORT,0x60);     
    while (inb(KBSTATPORT) & KBOUTRDY) ;
    outb(KBCMDPORT,0x45);
    CYGACC_CALL_IF_DELAY_US(10000);  // 10ms
        
    while (inb(KBSTATPORT) & KBOUTRDY) ;
    outb(KBCMDPORT,0xAE);  // Enable keyboard

    /* See if a keyboard is connected */
    while (inb(KBSTATPORT) & KBOUTRDY) ;
    outb(KBDATAPORT,0xFF);     
    while (((s = inb(KBSTATPORT)) & (KBINRDY|KBTXTO)) == 0) ; /* wait input ready */
    if ((s & KBTXTO) || ((c = inb(KBDATAPORT)) != 0xFA)) {
#ifdef DEBUG_KBD_INIT
        diag_printf("Keyboard reset failed - no ACK: %x, stat: %x\n", c, s);
#endif
        return false;
    }
    while (((s = inb(KBSTATPORT)) & KBINRDY) == 0) ; /* wait input ready */
    if ((s & KBTXTO) || ((c = inb(KBDATAPORT)) != 0xAA)) {
#ifdef DEBUG_KBD_INIT
        diag_printf("Keyboard reset failed - bad code: %x, stat: %x\n", c, s);
#endif
        return false;
    }

    // Set scan mode
    while (inb(KBSTATPORT) & KBOUTRDY) ;
    outb(KBCMDPORT,0x20);
    while ((inb(KBSTATPORT) & KBINRDY) == 0) ; /* wait input ready */
    if (! (inb(KBDATAPORT) & 0x40)) {
        /*
         * Quote from PS/2 System Reference Manual:
         *
         * "Address hex 0060 and address hex 0064 should be
         * written only when the input-buffer-full bit and
         * output-buffer-full bit in the Controller Status
         * register are set 0." (KBINRDY and KBOUTRDY)
         */
                
        while (inb(KBSTATPORT) & (KBINRDY | KBOUTRDY)) ;
        outb(KBDATAPORT,0xF0);
        while (inb(KBSTATPORT) & (KBINRDY | KBOUTRDY)) ;
        outb(KBDATAPORT,0x01);
    }
        
    KBFlags = 0;
    return true;
} /* KeyboardInit */

//-----------------------------------------------------------------------------

static CYG_BYTE 
KeyboardAscii(CYG_BYTE scancode)
{
    CYG_BYTE ascii = 0xFF;

    // Start by handling all shift/ctl keys:

    switch( scancode ) {
    case 0xe0:
        KBFlags |= KBExtend;
        return 0xFF;

    case 0xfa:
        KBFlags |= KBAck;
        return 0xFF;

    case 0xfe:
        KBFlags |= KBResend;
        return 0xFF;

    case LSHIFT:
        KBFlags |= KBShiftL;
        return 0xFF;

    case LSHIFT | BREAK:
        KBFlags &= ~KBShiftL;
        return 0xFF;

    case RSHIFT:
        KBFlags |= KBShiftR;
        return 0xFF;

    case RSHIFT | BREAK:
        KBFlags &= ~KBShiftR;
        return 0xFF;

    case CTRL:
        if( KBFlags & KBExtend )
        {
            KBFlags |= KBCtrlR;
            KBFlags &= ~KBExtend;
        }
        else  KBFlags |= KBCtrlL;
        return 0xFF;

    case CTRL | BREAK:
        if( KBFlags & KBExtend )
        {
            KBFlags &= ~KBCtrlR;
            KBFlags &= ~KBExtend;
        }
        else  KBFlags &= ~KBCtrlL;
        return 0xFF;


    case ALT:
        if( KBFlags & KBExtend )
        {
            KBFlags |= KBAltR;
            KBFlags &= ~KBExtend;
        }
        else  KBFlags |= KBAltL;
        return 0xFF;

    case ALT | BREAK:
        if( KBFlags & KBExtend )
        {
            KBFlags &= ~KBAltR;
            KBFlags &= ~KBExtend;
        }
        else  KBFlags &= ~KBAltL;
        return 0xFF;

    case CAPS:
        KBFlags ^= KBCapsLock;
    case CAPS | BREAK:
        return 0xFF;

    case NUMS:
        KBFlags ^= KBNumLock;
    case NUMS | BREAK:
        return 0xFF;

    case KBArrowUp:
    case KBArrowDown:
        screen_pan = 0;
        lcd_refresh();
        break;
    case KBArrowLeft:
        screen_pan -= SCREEN_PAN;
        if (screen_pan < 0) screen_pan = 0;
        lcd_refresh();
        break;
    case KBArrowRight:
        screen_pan += SCREEN_PAN;
        if (screen_pan > (SCREEN_WIDTH-SCREEN_PAN)) 
            screen_pan = SCREEN_WIDTH-SCREEN_PAN;
        lcd_refresh();
        break;

    }

    // Clear Extend flag if set
    KBFlags &= ~KBExtend;

    // Ignore all other BREAK codes
    if( scancode & 0x80 ) return 0xFF;

    // Here the scancode is for something we can turn
    // into an ASCII value

    ascii = KBScanTable[scancode & 0x7F][KBIndexTab[KBFlags & KBIndex]];

    return ascii;

} /* KeyboardAscii */

//-----------------------------------------------------------------------------

static int 
KeyboardTest(void)
{
    // If there is a pending character, return True
    if( KBPending != 0xFF ) return true;

    // If there is something waiting at the port, get it
    for(;;) {
        CYG_BYTE stat, code;
        CYG_BYTE c;
        
        HAL_READ_UINT8( KBSTATPORT, stat );

        if( (stat & KBINRDY) == 0 )
            break;

        HAL_READ_UINT8( KBDATAPORT, code );

        // Translate to ASCII
        c = KeyboardAscii(code);
    
        // if it is a real ASCII char, save it and
        // return True.
        if( c != 0xFF ) {
            KBPending = c;
            return true;
        }
    }

    // Otherwise return False
    return false;
  
} /* KeyboardTest */

char 
KeyboardChar(void)
{
    char c = KBPending;
    KBPending = 0xFF;
    return c;
}

static int  _timeout = 500;

static cyg_bool
lcd_comm_getc_nonblock(void* __ch_data, cyg_uint8* ch)
{
    if( !KeyboardTest() )
        return false;
    *ch = KBPending;
    KBPending = 0xFF;
    return true;
}

static cyg_uint8
lcd_comm_getc(void* __ch_data)
{
    cyg_uint8 ch;

    while (!lcd_comm_getc_nonblock(__ch_data, &ch)) ;
    return ch;
}

static void
lcd_comm_putc(void* __ch_data, cyg_uint8 c)
{
    lcd_putc(c);
}

static void
lcd_comm_write(void* __ch_data, const cyg_uint8* __buf, cyg_uint32 __len)
{
#if 0
    CYGARC_HAL_SAVE_GP();

    while(__len-- > 0)
        lcd_comm_putc(__ch_data, *__buf++);

    CYGARC_HAL_RESTORE_GP();
#endif
}

static void
lcd_comm_read(void* __ch_data, cyg_uint8* __buf, cyg_uint32 __len)
{
#if 0
    CYGARC_HAL_SAVE_GP();

    while(__len-- > 0)
        *__buf++ = lcd_comm_getc(__ch_data);

    CYGARC_HAL_RESTORE_GP();
#endif
}

static cyg_bool
lcd_comm_getc_timeout(void* __ch_data, cyg_uint8* ch)
{
    int delay_count;
    cyg_bool res;

    delay_count = _timeout * 2; // delay in .5 ms steps
    for(;;) {
        res = lcd_comm_getc_nonblock(__ch_data, ch);
        if (res || 0 == delay_count--)
            break;
        CYGACC_CALL_IF_DELAY_US(500);
    }
    return res;
}

static int
lcd_comm_control(void *__ch_data, __comm_control_cmd_t __func, ...)
{
    static int vector = 0;
    int ret = -1;
    static int irq_state = 0;

    CYGARC_HAL_SAVE_GP();

    switch (__func) {
    case __COMMCTL_IRQ_ENABLE:
        ret = irq_state;
        irq_state = 1;
        break;
    case __COMMCTL_IRQ_DISABLE:
        ret = irq_state;
        irq_state = 0;
        break;
    case __COMMCTL_DBG_ISR_VECTOR:
        ret = vector;
        break;
    case __COMMCTL_SET_TIMEOUT:
    {
        va_list ap;

        va_start(ap, __func);

        ret = _timeout;
        _timeout = va_arg(ap, cyg_uint32);

        va_end(ap);
        break;
    }
    case __COMMCTL_FLUSH_OUTPUT:
        ret = 0;
        break;
    default:
        break;
    }
    CYGARC_HAL_RESTORE_GP();
    return ret;
}

static int
lcd_comm_isr(void *__ch_data, int* __ctrlc, 
           CYG_ADDRWORD __vector, CYG_ADDRWORD __data)
{
#if 0
    char ch;

    cyg_drv_interrupt_acknowledge(__vector);
    *__ctrlc = 0;
    if (lcd_comm_getc_nonblock(__ch_data, &ch)) {
        if (ch == 0x03) {
            *__ctrlc = 1;
        }
    }
    return CYG_ISR_HANDLED;
#else
    return 0;
#endif
}

void
lcd_comm_init(void)
{
    static int init = 0;

    if (!init) {
        hal_virtual_comm_table_t* comm;
        int cur = CYGACC_CALL_IF_SET_CONSOLE_COMM(CYGNUM_CALL_IF_SET_COMM_ID_QUERY_CURRENT);

        init = 1;
        if (!KeyboardInit()) {
            // No keyboard - no LCD/CRT display
            return;
        }

        // Initialize screen
        cursor_enable = true;
        lcd_init(16);
        lcd_on(true);
        lcd_screen_clear();

        // Setup procs in the vector table
        CYGACC_CALL_IF_SET_CONSOLE_COMM(2);  // FIXME - should be controlled by CDL
        comm = CYGACC_CALL_IF_CONSOLE_PROCS();
        //CYGACC_COMM_IF_CH_DATA_SET(*comm, chan);
        CYGACC_COMM_IF_WRITE_SET(*comm, lcd_comm_write);
        CYGACC_COMM_IF_READ_SET(*comm, lcd_comm_read);
        CYGACC_COMM_IF_PUTC_SET(*comm, lcd_comm_putc);
        CYGACC_COMM_IF_GETC_SET(*comm, lcd_comm_getc);
        CYGACC_COMM_IF_CONTROL_SET(*comm, lcd_comm_control);
        CYGACC_COMM_IF_DBG_ISR_SET(*comm, lcd_comm_isr);
        CYGACC_COMM_IF_GETC_TIMEOUT_SET(*comm, lcd_comm_getc_timeout);

        // Restore original console
        CYGACC_CALL_IF_SET_CONSOLE_COMM(cur);
    }
}

#ifdef CYGPKG_REDBOOT
#include <redboot.h>

// Get here when RedBoot is idle.  If it's been long enough, then
// dim the LCD.  The problem is - how to determine other activities
// so at this doesn't get in the way.  In the default case, this will
// be called from RedBoot every 10ms (CYGNUM_REDBOOT_CLI_IDLE_TIMEOUT)

#define MAX_IDLE_TIME (30*100)

static void
idle(bool is_idle)
{
    static int idle_time = 0;
    static bool was_idled = false;

    if (is_idle) {
        if (!was_idled) {
            if (++idle_time == MAX_IDLE_TIME) {
                was_idled = true;
                lcd_on(false);
            }
        }
    } else {        
        idle_time = 0;
        if (was_idled) {
            was_idled = false;
                lcd_on(true);
        }
    }
}

RedBoot_idle(idle, RedBoot_AFTER_NETIO);
#endif
#endif
