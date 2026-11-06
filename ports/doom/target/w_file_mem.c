/* WAD access for the M33 build (GPL-2.0, see ../LICENSE).
 * The WAD sits in the 8 MB QSPI NOR. This chip's QSPI is a command and FIFO controller
 * (no memory-mapped window found in SDK 1.04.00.00), so the engine reads lumps through
 * fox_wad_read(), which copies from the flash into RAM. Define FOX_WAD_MAPPED_ADDR to use
 * a WAD that is mapped into the address space instead (a small WAD in internal flash, or a part with XIP):
 * lumps are then used in place and never copied into the zone. */
#include <string.h>
#include "w_file.h"
#include "z_zone.h"

#ifndef FOX_WAD_LEN
#define FOX_WAD_LEN 4196020u   /* DOOM1.WAD 1.9 shareware */
#endif

extern size_t fox_wad_read(unsigned int offset, void *buffer, size_t len);   /* platform: QSPI read, or a memcpy */
extern wad_file_class_t fox_wad_file;

static wad_file_t the_wad;

static wad_file_t *W_Mem_OpenFile(char *path)
{
    if (strcmp(path, "doom1.wad") != 0) return NULL;
    the_wad.file_class = &fox_wad_file;
#ifdef FOX_WAD_MAPPED_ADDR
    the_wad.mapped = (byte *)FOX_WAD_MAPPED_ADDR;
#else
    the_wad.mapped = NULL;
#endif
    the_wad.length = FOX_WAD_LEN;
    return &the_wad;
}

static void W_Mem_CloseFile(wad_file_t *wad) { (void)wad; }

static size_t W_Mem_Read(wad_file_t *wad, unsigned int offset, void *buffer, size_t buffer_len)
{
    (void)wad;
    if (offset >= FOX_WAD_LEN) return 0;
    if (offset + buffer_len > FOX_WAD_LEN) buffer_len = FOX_WAD_LEN - offset;
    return fox_wad_read(offset, buffer, buffer_len);
}

wad_file_class_t fox_wad_file = { W_Mem_OpenFile, W_Mem_CloseFile, W_Mem_Read };
