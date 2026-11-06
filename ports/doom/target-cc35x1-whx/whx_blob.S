/* Includes the converted game data (WHX, made by m33-whx/convert.sh from the reader's own DOOM1.WAD) into the flash image.
 * WHX_PATH is passed by build.sh and points into the git-ignored ports/doom/wad-cache/. The file is never committed. */
	.section .fox_whx,"a"
	.balign 4096
	.global fox_whx
fox_whx:
	.incbin WHX_PATH
