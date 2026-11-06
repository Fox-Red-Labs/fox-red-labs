#include <new>
#include <string.h>
#include "plaitsrun.h"
#include "plaits/dsp/voice.h"
#include "stmlib/utils/buffer_allocator.h"

pl_res_t pl_res[PL_ENGINES];
uint32_t pl_crc_blk[PL_ENGINES][PL_BLOCKS];
uint32_t pl_cyc_blk[PL_ENGINES][PL_BLOCKS];
pl_info_t pl_info;
int16_t pl_keep[PL_BLOCKS * PL_BLOCK * 2];
int pl_keep_engine = -1;
const char* const pl_names[PL_ENGINES] = {
    "va_vcf", "phase_dist", "fm6_a", "fm6_b", "fm6_c", "wave_terrain", "string_machine", "chiptune",
    "virtual_analog", "waveshaping", "fm2op", "grain", "additive", "wavetable", "chord", "speech",
    "swarm", "noise", "particle", "string", "modal", "bass_drum", "snare_drum", "hi_hat" };

static char shared_buffer[16384];                 /* the same 16 kB plaits.cc gives the voice */
static plaits::Voice voice_store;                 /* one voice, re-initialised per engine */

uint32_t pl_crc32(uint32_t crc, const void* p, unsigned n) {
    const uint8_t* b = (const uint8_t*)p; crc = ~crc;
    while (n--) { crc ^= *b++; for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u))); }
    return ~crc;
}

void pl_run_engine(int e, uint32_t (*cyc)(void)) {
    using namespace plaits;
    stmlib::BufferAllocator alloc(shared_buffer, sizeof shared_buffer);
    memset(shared_buffer, 0, sizeof shared_buffer);
    Voice* v = new (&voice_store) Voice;
    v->Init(&alloc);
    pl_info.voice_bytes = sizeof(Voice); pl_info.shared_bytes = sizeof shared_buffer; pl_info.shared_free_after_init = (uint32_t)alloc.free();

    static const float notes[PL_NOTES] = { 48.0f, 52.0f, 55.0f, 60.0f };
    static const float harm[PL_NOTES] = { 0.3f, 0.5f, 0.7f, 0.9f };
    static const float timb[PL_NOTES] = { 0.5f, 0.6f, 0.4f, 0.7f };
    static const float morp[PL_NOTES] = { 0.5f, 0.3f, 0.6f, 0.5f };
    Patch patch; Modulations mod; memset(&patch, 0, sizeof patch); memset(&mod, 0, sizeof mod);
    patch.engine = e;   /* Voice adds this integer base to the engine CV (0 here) */
    patch.frequency_modulation_amount = 0.0f; patch.timbre_modulation_amount = 0.5f; patch.morph_modulation_amount = 0.5f;
    patch.decay = 0.5f; patch.lpg_colour = 0.5f;
    mod.trigger_patched = true;

    pl_res_t* r = &pl_res[e]; memset(r, 0, sizeof *r);
    r->engine = e; r->blocks = PL_BLOCKS; r->cyc_min = 0xFFFFFFFFu;
    uint64_t total = 0; uint32_t crc_all = 0;
    for (int b = 0; b < PL_BLOCKS; b++) {
        int n = b / PL_BLOCKS_PER_NOTE, k = b % PL_BLOCKS_PER_NOTE;
        patch.note = notes[n]; patch.harmonics = harm[n]; patch.timbre = timb[n]; patch.morph = morp[n];
        mod.trigger = (k < PL_GATE_BLOCKS) ? 1.0f : 0.0f;
        Voice::Frame frames[PL_BLOCK];
        uint32_t t0 = cyc();
        v->Render(patch, mod, frames, PL_BLOCK);
        uint32_t dt = cyc() - t0;
        uint32_t c = pl_crc32(0, frames, sizeof frames);
        crc_all = pl_crc32(crc_all, frames, sizeof frames);
        pl_crc_blk[e][b] = c; pl_cyc_blk[e][b] = dt;
        for (int i = 0; i < PL_BLOCK; i++) {
            int a = frames[i].out < 0 ? -frames[i].out : frames[i].out; int a2 = frames[i].aux < 0 ? -frames[i].aux : frames[i].aux;
            if ((uint32_t)a > r->peak) r->peak = a; if ((uint32_t)a2 > r->peak) r->peak = a2;
            if (frames[i].out || frames[i].aux) r->nonzero++;
        }
        if (pl_keep_engine == e) memcpy(&pl_keep[b * PL_BLOCK * 2], frames, sizeof frames);
        total += dt;
        if (b == 0) r->cyc_first = dt;
        else { if (dt < r->cyc_min) r->cyc_min = dt; if (dt > r->cyc_max) { r->cyc_max = dt; r->cyc_worst_idx = b; } }
    }
    r->crc_all = crc_all; r->cyc_lo = (uint32_t)total; r->cyc_hi = (uint32_t)(total >> 32); r->active_engine = v->active_engine();
}
