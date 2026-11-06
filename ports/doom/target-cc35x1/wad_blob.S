/* Includes the shareware DOOM1.WAD into the flash image, byte for byte (GPL-2.0 file, the WAD itself is not in this repository).
 * WAD_PATH is passed by build.sh and points into the git-ignored ports/doom/wad-cache/. */
	.section .rodata.fox_wad,"a"
	.balign 4
	.global fox_wad
fox_wad:
	.incbin WAD_PATH
	.global fox_wad_len
	.balign 4
fox_wad_len:
	.word 4196020
