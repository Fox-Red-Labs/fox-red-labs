/* LVGL v9.6.0 configuration for the LP-MSPM0G3507 (Cortex-M0+, 128 KB flash, 32 KB RAM).
 * Everything LVGL defaults to "on" that the demo does not use is switched off here.
 * Anything not listed takes lv_conf_internal.h's default, which is "off" for all the
 * drivers, file systems, image decoders and OS layers. */
#ifndef LV_CONF_H
#define LV_CONF_H

/* TI's libc has no strnlen declaration; LVGL's string layer calls it and port.c defines it. */
#include <stddef.h>
size_t strnlen(const char *s, size_t max);

/* ---- memory ---- */
#define LV_USE_STDLIB_MALLOC   LV_STDLIB_BUILTIN      /* LVGL's own TLSF heap, a static array in .bss */
#define LV_USE_STDLIB_STRING   LV_STDLIB_CLIB         /* TI libc memcpy/memset are faster than the built-in loops */
#define LV_USE_STDLIB_SPRINTF  LV_STDLIB_BUILTIN      /* small, no float */
#ifndef LV_MEM_SIZE
#define LV_MEM_SIZE            (16 * 1024)
#endif
#define LV_MEM_ADR             0
#define LV_USE_FLOAT           0
#define LV_USE_OS              LV_OS_NONE

/* ---- display and drawing ---- */
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565
#define LV_DEF_REFR_PERIOD     20
#define LV_DPI_DEF             130
#define LV_DRAW_BUF_ALIGN      4
#define LV_DRAW_BUF_STRIDE_ALIGN 1
#define LV_USE_DRAW_SW         1
#define LV_DRAW_SW_DRAW_UNIT_CNT 1
#define LV_DRAW_SW_SUPPORT_RGB565            1
#define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED    0
#define LV_DRAW_SW_SUPPORT_RGB565A8          0
#define LV_DRAW_SW_SUPPORT_RGB888            0
#define LV_DRAW_SW_SUPPORT_XRGB8888          0
#define LV_DRAW_SW_SUPPORT_ARGB8888          0
#define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 0
#define LV_DRAW_SW_SUPPORT_L8                0
#define LV_DRAW_SW_SUPPORT_AL88              0
#define LV_DRAW_SW_SUPPORT_A8                0
#define LV_DRAW_SW_SUPPORT_I1                0
#define LV_DRAW_SW_COMPLEX                   1   /* rounded corners, arcs, circles */
#define LV_DRAW_SW_SLIM                      1   /* our size patch (size-patch.diff): no shadow, blur, image, layer, triangle drawing, no image decoder */
#define LV_DRAW_SW_SHADOW_CACHE_SIZE         0
#define LV_DRAW_SW_CIRCLE_CACHE_SIZE         2
#define LV_USE_DRAW_SW_COMPLEX_GRADIENTS     0
#define LV_USE_DRAW_SW_ASM                   LV_DRAW_SW_ASM_NONE
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE        (2 * 1024)
#define LV_CACHE_DEF_SIZE                    0
#define LV_IMAGE_HEADER_CACHE_DEF_CNT        0
#define LV_USE_MATRIX                        0
#define LV_USE_VECTOR_GRAPHIC                0
#define LV_USE_SNAPSHOT                      0

/* ---- features ---- */
#define LV_USE_LOG             0
#define LV_USE_ASSERT_NULL     0
#define LV_USE_ASSERT_MALLOC   0
#define LV_USE_ASSERT_STYLE    0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ      0
#define LV_ASSERT_HANDLER      while (1) { }
#define LV_USE_SYSMON          0
#define LV_USE_PROFILER        0
#define LV_USE_OBJ_ID          0
#define LV_USE_OBJ_NAME        0
#define LV_USE_OBJ_PROPERTY    0
#define LV_USE_OBSERVER        0
#define LV_USE_TRANSLATION     0
#define LV_USE_COLOR_FILTER    0
#define LV_USE_EXT_DATA        0
#define LV_OBJ_STYLE_CACHE     0
#define LV_USE_FLEX            0
#define LV_USE_GRID            0
#define LV_USE_GRIDNAV         0
#define LV_USE_GESTURE_RECOGNITION 0
#define LV_USE_BIDI            0
#define LV_USE_ARABIC_PERSIAN_CHARS 0
#define LV_USE_FONT_COMPRESSED 0
#define LV_USE_FONT_PLACEHOLDER 0
#define LV_USE_FONT_MANAGER    0
#define LV_USE_IMGFONT         0
#define LV_USE_BIN_DECODER_RAM_LOAD 0
#define LV_USE_RLE             0
#define LV_USE_LZ4             0
#define LV_USE_FS_STDIO        0
#define LV_USE_FS_POSIX        0
#define LV_USE_FS_WIN32        0
#define LV_USE_FS_FATFS        0
#define LV_USE_FS_MEMFS        0
#define LV_USE_FS_LITTLEFS     0
#define LV_USE_FS_ARDUINO_ESP_LITTLEFS 0
#define LV_USE_FS_ARDUINO_SD   0
#define LV_USE_FS_UEFI         0

/* ---- themes: default only ---- */
#define LV_USE_THEME_DEFAULT   1
#define LV_THEME_DEFAULT_DARK  1
#define LV_THEME_DEFAULT_GROW  0
#define LV_THEME_DEFAULT_TRANSITION_TIME 0
#define LV_USE_THEME_SIMPLE    0
#define LV_USE_THEME_MONO      0

/* ---- fonts: Montserrat 14 (default) and 20 ---- */
#define LV_FONT_MONTSERRAT_14  1
#ifdef DEMO_FULL
#define LV_FONT_MONTSERRAT_20  1   /* 21.8 KB of flash: the second font is what no longer fits in 128 KB */
#endif
#define LV_FONT_MONTSERRAT_14_ALIGNED 0
#define LV_FONT_DEFAULT        &lv_font_montserrat_14

/* ---- widgets the demo uses: label, button, slider, switch, arc, chart ---- */
#define LV_USE_ANIMIMG         0
#define LV_USE_ARC             1
#define LV_USE_ARCLABEL        0
#define LV_USE_BAR             1   /* slider is built on it */
#define LV_USE_BUTTON          1
#define LV_USE_BUTTONMATRIX    0
#define LV_USE_CALENDAR        0
#define LV_USE_CANVAS          0
#define LV_USE_CHART           1
#define LV_USE_CHECKBOX        0
#define LV_USE_DROPDOWN        0
#define LV_USE_IMAGE           0
#define LV_USE_IMAGEBUTTON     0
#define LV_USE_KEYBOARD        0
#define LV_USE_LABEL           1
#define LV_USE_LED             0
#define LV_USE_LINE            0
#define LV_USE_LIST            0
#define LV_USE_MENU            0
#define LV_USE_MSGBOX          0
#define LV_USE_ROLLER          0
#define LV_USE_SCALE           0
#define LV_USE_SLIDER          1
#define LV_USE_SPAN            0
#define LV_USE_SPINBOX         0
#define LV_USE_SPINNER         0
#define LV_USE_SWITCH          1
#define LV_USE_TABLE           0
#define LV_USE_TABVIEW         0
#define LV_USE_TEXTAREA        0
#define LV_USE_TILEVIEW        0
#define LV_USE_WIN             0
#define LV_LABEL_TEXT_SELECTION 0
#define LV_LABEL_LONG_TXT_HINT 0

#endif /*LV_CONF_H*/
