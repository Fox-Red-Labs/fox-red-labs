/*
 * lse_trap: run ARMv8.1+ atomics on an ARMv8.0 core (Cortex-A53) by trapping them.
 *
 * Valve's native arm64 Steam client is built with LSE atomics (LDADDAL, SWPA, CAS...). The A53 has no LSE,
 * so the first one raises SIGILL. This LD_PRELOAD library installs a SIGILL handler that decodes the
 * instruction, does the same operation with load-exclusive/store-exclusive (what an ARMv8.0 compiler emits),
 * writes the result registers and steps past the instruction. LDAPR (ARMv8.3 RCpc load) is emulated as a
 * load followed by a full barrier. Anything it does not decode goes to the program's own SIGILL handler.
 *
 * LSE2 (ARMv8.4) lets atomics and LDAR/STLR touch unaligned addresses inside 16 bytes; the client relies on
 * it. Unaligned trapped atomics run under one global lock, and a SIGBUS handler does unaligned LDAR/STLR.
 *
 * Every trapped atomic costs a signal round trip (microseconds). LSE_PATCH=1 removes that cost after the first
 * trap at each site: the handler writes a small load-exclusive/store-exclusive stub for that exact instruction
 * (its registers, size and operation) into a code page within branch range, and replaces the instruction with a
 * branch to the stub, so later runs of that site never trap. Patched: LD<op> ADD, CLR, EOR, SET; SWP; CAS
 * (byte to doubleword); CASP (W and X pairs), in file-backed executable mappings only (never JIT code); LDAPR
 * becomes LDAR in place. MIN/MAX and sites first met on an unaligned address stay on the trap path. A stub that meets an unaligned address (SIGBUS on the
 * exclusive load) is undone in the SIGBUS handler and emulated like a trap.
 * Build: gcc -O2 -march=armv8-a -mno-outline-atomics -fPIC -shared -o lse_trap.so lse_trap.c -ldl
 * Use:   LD_PRELOAD=/path/lse_trap.so program   (LSE_TRAP_STATS=1 prints counts at exit;
 *        LSE_TRAP_DIR=dir keeps live counters in dir/<pid>, a shared page a reader can sample; LSE_PATCH=1)
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

static struct sigaction chained;            /* the program's own SIGILL handler, if it set one */
static int (*real_sigaction)(int, const struct sigaction *, struct sigaction *);
/* counters: in a private struct, or in a shared page (LSE_TRAP_DIR) that survives fork and can be read live */
struct ctr { unsigned long magic, pid; char comm[16]; unsigned long ldop, swp, cas, casp, ldapr, unknown, unaligned, bus,
             patched, patch_fail, stub_bus, inplace; };
static struct ctr ctr0, *C = &ctr0;
#define INC(f) __atomic_fetch_add(&C->f, 1, __ATOMIC_RELAXED)
#define n_ldop C->ldop
#define n_swp C->swp
#define n_cas C->cas
#define n_casp C->casp
#define n_ldapr C->ldapr
#define n_unknown C->unknown
#define n_unaligned C->unaligned
#define n_bus C->bus
static int do_patch;
static struct sigaction chained_bus;
static int ulock;
static void lock(void) { while (__atomic_exchange_n(&ulock, 1, __ATOMIC_ACQUIRE)) ; }
static void unlock(void) { __atomic_store_n(&ulock, 0, __ATOMIC_RELEASE); }
static uint64_t uload(uintptr_t a, unsigned sz) { uint64_t v = 0; memcpy(&v, (void *)a, 1u << sz); return v; }
static void ustore(uintptr_t a, unsigned sz, uint64_t v) { memcpy((void *)a, &v, 1u << sz); }

static inline uint64_t rd(mcontext_t *m, unsigned r) { return r == 31 ? 0 : m->regs[r]; }
static inline void wr(mcontext_t *m, unsigned r, uint64_t v) { if (r != 31) m->regs[r] = v; }
static inline uint64_t base(mcontext_t *m, unsigned rn) { return rn == 31 ? m->sp : m->regs[rn]; }

static uint64_t mask_sz(uint64_t v, unsigned sz) { return sz == 3 ? v : v & ((1ULL << (8u << sz)) - 1); }
static int64_t sext(uint64_t v, unsigned sz) {
    if (sz == 3) return (int64_t)v;
    unsigned b = 8u << sz; return (int64_t)(v << (64 - b)) >> (64 - b);
}

/* load-exclusive / store-exclusive loop for one operation of 1, 2, 4 or 8 bytes */
#define LLSC(T, OPEXPR) do {                                                    \
    T *p = (T *)addr, o, n;                                                     \
    o = __atomic_load_n(p, __ATOMIC_RELAXED);                                   \
    do { uint64_t old = o; uint64_t nv = (OPEXPR); n = (T)nv; }                 \
    while (!__atomic_compare_exchange_n(p, &o, n, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)); \
    oldv = o; } while (0)

#define APPLY_U(old) (swp ? s : opc == 0 ? (old) + s : opc == 1 ? ((old) & ~s) : opc == 2 ? ((old) ^ s) :  \
    opc == 3 ? ((old) | s) : opc == 4 ? (sext(old, sz) > sext(s, sz) ? (old) : s) :                     \
    opc == 5 ? (sext(old, sz) < sext(s, sz) ? (old) : s) : opc == 6 ? (mask_sz(old, sz) > mask_sz(s, sz) ? (old) : s) : \
    (mask_sz(old, sz) < mask_sz(s, sz) ? (old) : s))
static uint64_t do_op(unsigned opc, int swp, unsigned sz, uintptr_t addr, uint64_t s) {
    uint64_t oldv = 0;
    if (addr & ((1u << sz) - 1)) {                       /* LSE2-style unaligned atomic: lock and copy */
        INC(unaligned); lock(); __atomic_thread_fence(__ATOMIC_SEQ_CST);
        uint64_t old = uload(addr, sz); oldv = old;
        uint64_t nv = APPLY_U(old); ustore(addr, sz, nv);
        __atomic_thread_fence(__ATOMIC_SEQ_CST); unlock(); return oldv;
    }
#define APPLY(old) (swp ? s : opc == 0 ? (old) + s : opc == 1 ? ((old) & ~s) : opc == 2 ? ((old) ^ s) :  \
    opc == 3 ? ((old) | s) : opc == 4 ? (sext(old, sz) > sext(s, sz) ? (old) : s) :                     \
    opc == 5 ? (sext(old, sz) < sext(s, sz) ? (old) : s) : opc == 6 ? (mask_sz(old, sz) > mask_sz(s, sz) ? (old) : s) : \
    (mask_sz(old, sz) < mask_sz(s, sz) ? (old) : s))
    switch (sz) {
    case 0: LLSC(uint8_t, APPLY(old)); break;
    case 1: LLSC(uint16_t, APPLY(old)); break;
    case 2: LLSC(uint32_t, APPLY(old)); break;
    default: LLSC(uint64_t, APPLY(old)); break;
    }
    return oldv;
}

static uint64_t do_cas(unsigned sz, uintptr_t addr, uint64_t expect, uint64_t nv) {
    if (addr & ((1u << sz) - 1)) {
        INC(unaligned); lock(); __atomic_thread_fence(__ATOMIC_SEQ_CST);
        uint64_t old = uload(addr, sz); if (old == expect) ustore(addr, sz, nv);
        __atomic_thread_fence(__ATOMIC_SEQ_CST); unlock(); return old;
    }
    switch (sz) {
    case 0: { uint8_t e = expect; __atomic_compare_exchange_n((uint8_t *)addr, &e, (uint8_t)nv, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return e; }
    case 1: { uint16_t e = expect; __atomic_compare_exchange_n((uint16_t *)addr, &e, (uint16_t)nv, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return e; }
    case 2: { uint32_t e = expect; __atomic_compare_exchange_n((uint32_t *)addr, &e, (uint32_t)nv, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return e; }
    default: { uint64_t e = expect; __atomic_compare_exchange_n((uint64_t *)addr, &e, nv, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return e; }
    }
}

/* Do instruction i on context m (pc not advanced). 1 = done, 0 = not an instruction we know. */
static int emulate(uint32_t i, mcontext_t *m) {
    unsigned sz = i >> 30, rs = (i >> 16) & 31, rn = (i >> 5) & 31, rt = i & 31;
    if ((i & 0x3F200C00u) == 0x38200000u) {                 /* LD<op>, SWP, LDAPR family */
        unsigned o3 = (i >> 15) & 1, opc = (i >> 12) & 7;
        uintptr_t a = base(m, rn);
        if (o3 == 0) { INC(ldop); wr(m, rt, mask_sz(do_op(opc, 0, sz, a, mask_sz(rd(m, rs), sz)), sz)); }
        else if (opc == 0) { INC(swp); wr(m, rt, mask_sz(do_op(0, 1, sz, a, mask_sz(rd(m, rs), sz)), sz)); }
        else if (opc == 4 && rs == 31) {                    /* LDAPR */
            INC(ldapr); uint64_t v;
            switch (sz) { case 0: v = *(volatile uint8_t *)a; break; case 1: v = *(volatile uint16_t *)a; break;
                          case 2: v = *(volatile uint32_t *)a; break; default: v = *(volatile uint64_t *)a; }
            __atomic_thread_fence(__ATOMIC_SEQ_CST); wr(m, rt, v);
        } else return 0;
        return 1;
    }
    if ((i & 0x3FA07C00u) == 0x08A07C00u) {                 /* CAS, CASA, CASL, CASAL (B/H/W/X) */
        INC(cas);
        uint64_t e = mask_sz(rd(m, rs), sz), nv = mask_sz(rd(m, rt), sz);
        wr(m, rs, mask_sz(do_cas(sz, base(m, rn), e, nv), sz));
        return 1;
    }
    if ((i & 0xBFA07C00u) == 0x08207C00u && !(rs & 1) && !(rt & 1)) {  /* CASP (W or X pairs) */
        INC(casp);
        int x = (i >> 30) & 1;
        uintptr_t a = base(m, rn);
        if (a & (x ? 15 : 7)) {
            INC(unaligned); unsigned b = x ? 8 : 4; uint64_t m0 = x ? ~0ULL : 0xFFFFFFFFULL;
            lock(); __atomic_thread_fence(__ATOMIC_SEQ_CST);
            uint64_t o0 = 0, o1 = 0; memcpy(&o0, (void *)a, b); memcpy(&o1, (void *)(a + b), b);
            if (o0 == (rd(m, rs) & m0) && o1 == (rd(m, rs + 1) & m0)) {
                uint64_t n0 = rd(m, rt), n1 = rd(m, rt + 1); memcpy((void *)a, &n0, b); memcpy((void *)(a + b), &n1, b);
            }
            __atomic_thread_fence(__ATOMIC_SEQ_CST); unlock(); wr(m, rs, o0); wr(m, rs + 1, o1);
        } else if (x) {
            /* 128-bit compare-and-swap with LDAXP/STLXP; written out so the library needs no libatomic
             * (GCC calls __atomic_compare_exchange_16 for this, and Steam binds every symbol at load) */
            uint64_t e0 = rd(m, rs), e1 = rd(m, rs + 1), n0 = rd(m, rt), n1 = rd(m, rt + 1), o0, o1; uint32_t st;
            __asm__ volatile("1: ldaxp %0, %1, [%3]\n\t"
                             "cmp %0, %4\n\t ccmp %1, %5, #0, eq\n\t b.ne 2f\n\t"
                             "stlxp %w2, %6, %7, [%3]\n\t cbnz %w2, 1b\n\t b 3f\n"
                             "2: clrex\n3:"
                             : "=&r"(o0), "=&r"(o1), "=&r"(st) : "r"(a), "r"(e0), "r"(e1), "r"(n0), "r"(n1) : "memory", "cc");
            wr(m, rs, o0); wr(m, rs + 1, o1);
        } else {
            uint64_t e = (rd(m, rs + 1) << 32) | (uint32_t)rd(m, rs);
            uint64_t nv = (rd(m, rt + 1) << 32) | (uint32_t)rd(m, rt);
            __atomic_compare_exchange_n((uint64_t *)a, &e, nv, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
            wr(m, rs, (uint32_t)e); wr(m, rs + 1, (uint32_t)(e >> 32));
        }
        return 1;
    }
    return 0;
}

#include "lse_patch.h"

static void handler(int sig, siginfo_t *si, void *uc_) {
    ucontext_t *uc = uc_; mcontext_t *m = &uc->uc_mcontext;
    uint32_t i = *(uint32_t *)m->pc;
    uintptr_t ad = base(m, (i >> 5) & 31);    /* before emulate: the result may overwrite the base register */
    if (emulate(i, m)) {
        if (do_patch) try_patch(m->pc, i, ad);
        m->pc += 4; return;
    }
    /* Another thread patched this site between our fault and our read of it: run the new instruction. */
    if (do_patch && ((i & 0xFC000000u) == 0x14000000u || (i & 0x3FFFFC00u) == 0x08DFFC00u)) return;
    INC(unknown);
    if (getenv("LSE_TRAP_STATS")) {
        char b[96]; int k = snprintf(b, sizeof b, "lse_trap: unknown insn %08x at %llx\n", i, (unsigned long long)m->pc);
        if (write(2, b, k) < 0) {}
    }
    if (chained.sa_flags & SA_SIGINFO) { if (chained.sa_sigaction) { chained.sa_sigaction(sig, si, uc_); return; } }
    else if (chained.sa_handler != SIG_DFL && chained.sa_handler != SIG_IGN && chained.sa_handler) { chained.sa_handler(sig); return; }
    struct sigaction d = { .sa_handler = SIG_DFL }; real_sigaction(SIGILL, &d, NULL);   /* re-fault, default action */
}

/* SIGBUS: an LDAR or STLR on an unaligned address (fine on LSE2 cores, an alignment fault on the A53) */
static void bus_handler(int sig, siginfo_t *si, void *uc_) {
    ucontext_t *uc = uc_; mcontext_t *m = &uc->uc_mcontext;
    if (do_patch && stub_fault(m)) return;
    uint32_t i = *(uint32_t *)m->pc;
    unsigned sz = i >> 30, rn = (i >> 5) & 31, rt = i & 31;
    if ((i & 0x3FFFFC00u) == 0x08DFFC00u) {             /* LDAR */
        INC(bus); uint64_t v = uload(base(m, rn), sz); __atomic_thread_fence(__ATOMIC_SEQ_CST); wr(m, rt, v); m->pc += 4; return;
    }
    if ((i & 0x3FFFFC00u) == 0x089FFC00u) {             /* STLR */
        INC(bus); __atomic_thread_fence(__ATOMIC_SEQ_CST); ustore(base(m, rn), sz, rd(m, rt)); __atomic_thread_fence(__ATOMIC_SEQ_CST); m->pc += 4; return;
    }
    if (getenv("LSE_TRAP_STATS")) {
        char b[96]; int k = snprintf(b, sizeof b, "lse_trap: SIGBUS insn %08x at %llx\n", i, (unsigned long long)m->pc);
        if (write(2, b, k) < 0) {}
    }
    if (chained_bus.sa_flags & SA_SIGINFO) { if (chained_bus.sa_sigaction) { chained_bus.sa_sigaction(sig, si, uc_); return; } }
    else if (chained_bus.sa_handler != SIG_DFL && chained_bus.sa_handler != SIG_IGN && chained_bus.sa_handler) { chained_bus.sa_handler(sig); return; }
    struct sigaction d = { .sa_handler = SIG_DFL }; real_sigaction(SIGBUS, &d, NULL);
}

/* keep our handler in front: a program that sets SIGILL gets chained behind us */
int sigaction(int sig, const struct sigaction *act, struct sigaction *old) {
    if (!real_sigaction) real_sigaction = dlsym(RTLD_NEXT, "sigaction");
    if (sig == SIGILL || sig == SIGBUS) {
        struct sigaction *c = sig == SIGILL ? &chained : &chained_bus;
        if (old) *old = *c;
        if (act) *c = *act;
        return 0;
    }
    return real_sigaction(sig, act, old);
}

static void stats(void) {
    if (!getenv("LSE_TRAP_STATS")) return;
    fprintf(stderr, "lse_trap[%d]: ldop=%lu swp=%lu cas=%lu casp=%lu ldapr=%lu unaligned=%lu bus=%lu unknown=%lu"
            " patched=%lu inplace=%lu patch_fail=%lu stub_bus=%lu\n", getpid(),
            n_ldop, n_swp, n_cas, n_casp, n_ldapr, n_unaligned, n_bus, n_unknown, C->patched, C->inplace, C->patch_fail, C->stub_bus);
}

__attribute__((constructor)) static void init(void) {
    real_sigaction = dlsym(RTLD_NEXT, "sigaction");
    do_patch = getenv("LSE_PATCH") && getenv("LSE_PATCH")[0] == '1';
    const char *d = getenv("LSE_TRAP_DIR");
    if (d) {   /* one shared page per exec'd process; forked children (CEF zygote) add into their parent's page */
        char p[256]; snprintf(p, sizeof p, "%s/%d", d, getpid());
        int fd = open(p, O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0 && ftruncate(fd, 4096) == 0) {
            void *q = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (q != MAP_FAILED) { C = q; C->magic = 0x4C534554; C->pid = getpid();
                int f2 = open("/proc/self/comm", O_RDONLY); if (f2 >= 0) { ssize_t k = read(f2, C->comm, 15); (void)k; close(f2); } }
        }
        if (fd >= 0) close(fd);
    }
    struct sigaction sa; memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = handler; sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER; sigemptyset(&sa.sa_mask);
    real_sigaction(SIGILL, &sa, NULL);
    sa.sa_sigaction = bus_handler; real_sigaction(SIGBUS, &sa, NULL);
    atexit(stats);
}
