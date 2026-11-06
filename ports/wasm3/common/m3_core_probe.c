// Compiles wasm3's own m3_core.c inside this file so the runner can read how much of the fixed heap is in use.
// The wasm3 source stays unmodified; this only adds a function after it.
#include "m3_core.c"
#if d_m3FixedHeap
unsigned frl_m3_heap_used(void) { return (unsigned)(fixedHeapPtr - fixedHeap); }
unsigned frl_m3_heap_size(void) { return d_m3FixedHeap; }
// wasm3's fixed heap is a bump allocator: a free only gives back the newest block. After the runtime and environment
// are freed nothing of wasm3 is alive, so the whole heap can start over. Without this each pass of the app leaks.
void frl_m3_heap_reset(void) { fixedHeapPtr = fixedHeap; fixedHeapLast = NULL; }
#else
unsigned frl_m3_heap_used(void) { return 0; }
unsigned frl_m3_heap_size(void) { return 0; }
void frl_m3_heap_reset(void) {}
#endif
