/* avshim: the three FFmpeg calls Valve's arm64 libvideo.so expects that stock FFmpeg 8 does not have.
 * Valve's FFmpeg build adds tracked allocation (call sites pass size, source file, line) and a hook to
 * register its own allocator. Here the tracked calls go to plain av_malloc/av_mallocz and the hook is a no-op.
 * Exported under the LIBAVUTIL_60 version node so libvideo.so's versioned references bind to them.
 * Build: gcc -O2 -fPIC -shared -Wl,--version-script=avshim.map -o avshim.so avshim.c -lavutil */
#include <stddef.h>
void *av_malloc(size_t size);
void *av_mallocz(size_t size);
void *av_malloc_tracked(size_t size, const char *file, int line) { (void)file; (void)line; return av_malloc(size); }
void *av_mallocz_tracked(size_t size, const char *file, int line) { (void)file; (void)line; return av_mallocz(size); }
void av_register_malloc(void *a, void *b, void *c) { (void)a; (void)b; (void)c; }
