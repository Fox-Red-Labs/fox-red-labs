/* lse_patch.h: hot-patching for lse_trap.c (LSE_PATCH=1). Included once, after emulate().
 * At the first trap of a patchable LSE instruction, write a stub that does the same operation with
 * LDAXR/STLXR, then turn the instruction into a branch to the stub. Stub layout (scratch a, b, c, d picked
 * from x9 to x15, none of them Rs, Rt or Rn):
 *   stp a,b,[sp,#-32]! ; stp c,d,[sp,#16] ; [add d,sp,#32 when Rn is SP] ; [mrs b,nzcv for CAS]
 *   retry: ldaxr a,[A] ; op b,a,Rs | cmp ; stlxr wc,b,[A] ; cbnz wc,retry ; [fail: clrex ; msr nzcv,b]
 *   mov Rt|Rs,a ; ldp c,d,[sp,#16] ; ldp a,b,[sp],#32 ; b site+4
 * AArch64 Linux has no red zone below sp, so the pushes cannot clobber live data. */

#define STUB_MAX 24
struct stubrec { uintptr_t start, end, site; uint32_t insn; unsigned char nsave, flagreg, r[6]; };
#define NREC 32768
static struct stubrec recs[NREC];
static int nrec;
struct pool { uintptr_t base, used, size; };
#define NPOOL 64
static struct pool pools[NPOOL];
static int npool;
static int plock;
struct xmap { uintptr_t lo, hi; };
#define NXMAP 4096
static struct xmap xmaps[NXMAP];
static int nxmap;

static uint32_t e_stp_pre(int a, int b, int n, int off) { return 0xA9800000u | ((unsigned)(off / 8) & 0x7f) << 15 | b << 10 | n << 5 | a; }
static uint32_t e_stp(int a, int b, int n, int off) { return 0xA9000000u | ((unsigned)(off / 8) & 0x7f) << 15 | b << 10 | n << 5 | a; }
static uint32_t e_ldp(int a, int b, int n, int off) { return 0xA9400000u | ((unsigned)(off / 8) & 0x7f) << 15 | b << 10 | n << 5 | a; }
static uint32_t e_ldp_post(int a, int b, int n, int off) { return 0xA8C00000u | ((unsigned)(off / 8) & 0x7f) << 15 | b << 10 | n << 5 | a; }
static uint32_t e_addi(int d, int n, int imm) { return 0x91000000u | (unsigned)imm << 10 | n << 5 | d; }
static uint32_t e_ldaxr(int sz, int t, int n) { return 0x085FFC00u | (unsigned)sz << 30 | n << 5 | t; }
static uint32_t e_stlxr(int sz, int s, int t, int n) { return 0x0800FC00u | (unsigned)sz << 30 | s << 16 | n << 5 | t; }
static uint32_t e_reg(uint32_t op, int sf, int d, int n, int m) { return op | (unsigned)sf << 31 | m << 16 | n << 5 | d; }
static uint32_t e_mov(int sf, int d, int m) { return 0x2A0003E0u | (unsigned)sf << 31 | m << 16 | d; }
static uint32_t e_cmp(int sf, int n, int m) { return 0x6B00001Fu | (unsigned)sf << 31 | m << 16 | n << 5; }
static uint32_t e_cmp_ext(int n, int m, int opt) { return 0x6B20001Fu | m << 16 | opt << 13 | n << 5; }
static uint32_t e_cbnz(int t, int off) { return 0x35000000u | ((unsigned)off & 0x7ffff) << 5 | t; }
static uint32_t e_bne(int off) { return 0x54000001u | ((unsigned)off & 0x7ffff) << 5; }
static uint32_t e_b(long off) { return 0x14000000u | ((unsigned long)off & 0x3ffffff); }
#define E_CLREX 0xD503305Fu
static uint32_t e_ldaxp(int x, int t, int t2, int n) { return (x ? 0xC87F8000u : 0x887F8000u) | t2 << 10 | n << 5 | t; }
static uint32_t e_stlxp(int x, int s, int t, int t2, int n) { return (x ? 0xC8208000u : 0x88208000u) | s << 16 | t2 << 10 | n << 5 | t; }
static uint32_t e_ccmp_eq(int sf, int n, int m) { return 0x7A400000u | (unsigned)sf << 31 | m << 16 | n << 5; }
static uint32_t e_mrs_nzcv(int t) { return 0xD53B4200u | t; }
static uint32_t e_msr_nzcv(int t) { return 0xD51B4200u | t; }

/* 0 = not patchable; else stub length in words. kind: 1 LD<op>, 2 SWP, 3 CAS, 4 CASP. rec gets the saved
 * registers (in push order) and the register that holds the caller's flags (0xff: none). */
static int gen_stub(uint32_t i, uint32_t *c, uintptr_t at, uintptr_t ret, struct stubrec *rec) {
    unsigned sz = i >> 30, rs = (i >> 16) & 31, rn = (i >> 5) & 31, rt = i & 31, opc = (i >> 12) & 7;
    int kind = 0;
    if ((i & 0x3F200C00u) == 0x38200000u) {
        if (((i >> 15) & 1) == 0 && opc <= 3) kind = 1;
        else if (((i >> 15) & 1) == 1 && opc == 0) kind = 2;
    } else if ((i & 0x3FA07C00u) == 0x08A07C00u) kind = 3;
    else if ((i & 0xBFA07C00u) == 0x08207C00u && !(rs & 1) && !(rt & 1) && rs < 30 && rt < 30) kind = 4;
    if (!kind) return 0;
    int ns = kind == 4 ? 6 : 4, k = 0; unsigned r[6];
    for (unsigned x = 9; x <= 15 && k < ns; x++)
        if (x != rs && x != rt && x != rn && !(kind == 4 && (x == rs + 1 || x == rt + 1))) r[k++] = x;
    for (unsigned x = 0; x <= 8 && k < ns; x++)
        if (x != rs && x != rt && x != rn && !(kind == 4 && (x == rs + 1 || x == rt + 1))) r[k++] = x;
    if (k < ns) return 0;
    unsigned a = r[0], b = r[1], cc = r[2], d = r[3], A = rn, F;
    int sf = sz == 3, n = 0;
    if (kind == 4) { F = 48; sf = (i >> 30) & 1; }
    else F = 32;
    c[n++] = e_stp_pre(r[0], r[1], 31, -(int)F);
    c[n++] = e_stp(r[2], r[3], 31, 16);
    if (ns == 6) c[n++] = e_stp(r[4], r[5], 31, 32);
    if (rn == 31) { c[n++] = e_addi(d, 31, F); A = d; }
    rec->flagreg = 0xff;
    if (kind == 3) { c[n++] = e_mrs_nzcv(b); rec->flagreg = b; }
    if (kind == 4) { c[n++] = e_mrs_nzcv(r[4]); rec->flagreg = r[4]; }
    int retry = n;
    if (kind == 4) c[n++] = e_ldaxp(sf, a, b, A); else c[n++] = e_ldaxr(sz, a, A);
    if (kind == 1) {
        static const uint32_t ops[4] = { 0x0B000000u, 0x0A200000u, 0x4A000000u, 0x2A000000u }; /* add bic eor orr */
        c[n++] = e_reg(ops[opc], sf, b, a, rs);
        c[n] = e_stlxr(sz, cc, b, A); n++;
        c[n] = e_cbnz(cc, retry - n); n++;
        if (rt != 31) c[n++] = e_mov(sf, rt, a);
    } else if (kind == 2) {
        c[n] = e_stlxr(sz, cc, rs, A); n++;
        c[n] = e_cbnz(cc, retry - n); n++;
        if (rt != 31) c[n++] = e_mov(sf, rt, a);
    } else if (kind == 3) {
        c[n++] = sz >= 2 ? e_cmp(sf, a, rs) : e_cmp_ext(a, rs, sz);
        int bne = n++;
        c[n] = e_stlxr(sz, cc, rt, A); n++;
        c[n] = e_cbnz(cc, retry - n); n++;
        c[n] = e_b(2); n++;                                  /* over the clrex */
        c[bne] = e_bne(n - bne);
        c[n++] = E_CLREX;
        c[n++] = e_msr_nzcv(b);
        if (rs != 31) c[n++] = e_mov(sf, rs, a);
    } else {
        /* compare both halves; on a mismatch store the old pair back so the 128-bit read is single-copy atomic */
        c[n++] = e_cmp(sf, a, rs);
        c[n++] = e_ccmp_eq(sf, b, rs + 1);
        int bne = n++;
        c[n] = e_stlxp(sf, cc, rt, rt + 1, A); n++;
        c[n] = e_cbnz(cc, retry - n); n++;
        int bdone = n++;
        c[bne] = e_bne(n - bne);
        c[n] = e_stlxp(sf, cc, a, b, A); n++;
        c[n] = e_cbnz(cc, retry - n); n++;
        c[bdone] = e_b(n - bdone);
        c[n++] = e_msr_nzcv(r[4]);
        c[n++] = e_mov(sf, rs, a);
        c[n++] = e_mov(sf, rs + 1, b);
    }
    if (ns == 6) c[n++] = e_ldp(r[4], r[5], 31, 32);
    c[n++] = e_ldp(r[2], r[3], 31, 16);
    c[n++] = e_ldp_post(r[0], r[1], 31, F);
    c[n] = e_b((long)(ret - (at + 4 * n)) / 4); n++;
    rec->nsave = ns; for (k = 0; k < ns; k++) rec->r[k] = r[k];
    return n;
}

/* file-backed executable mappings, from /proc/self/maps (read again when a site is not in the cache) */
static int read_xmaps(void) {
    static char buf[1 << 19];
    int fd = open("/proc/self/maps", O_RDONLY); if (fd < 0) return 0;
    size_t len = 0; ssize_t k;
    while (len < sizeof buf - 1 && (k = read(fd, buf + len, sizeof buf - 1 - len)) > 0) len += k;
    close(fd); buf[len] = 0; nxmap = 0;
    for (char *p = buf; *p && nxmap < NXMAP; ) {
        char *e = strchr(p, '\n'); if (e) *e = 0;
        unsigned long lo, hi, off, ino; char perm[5]; unsigned maj, mn;
        if (sscanf(p, "%lx-%lx %4s %lx %x:%x %lu", &lo, &hi, perm, &off, &maj, &mn, &ino) == 7 && perm[2] == 'x' && ino != 0)
            { xmaps[nxmap].lo = lo; xmaps[nxmap].hi = hi; nxmap++; }
        if (!e) break;
        p = e + 1;
    }
    return 1;
}
static int in_xmap(uintptr_t pc) {
    for (int t = 0; t < 2; t++) {
        for (int j = 0; j < nxmap; j++) if (pc >= xmaps[j].lo && pc < xmaps[j].hi) return 1;
        if (t == 0 && !read_xmaps()) return 0;
    }
    return 0;
}

static uintptr_t pool_alloc(uintptr_t site, size_t bytes) {
    for (int j = 0; j < npool; j++) {
        struct pool *p = &pools[j];
        uintptr_t at = p->base + p->used;
        long dist = (long)(at - site);
        if (p->used + bytes <= p->size && dist < (100L << 20) && dist > -(100L << 20)) { p->used += bytes; return at; }
    }
    if (npool == NPOOL) return 0;
    size_t sz = 1 << 20;
    for (long step = 8; step <= 96; step += 8) {                 /* MB below, then above, the site */
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            uintptr_t h = (site + sgn * (step << 20)) & ~((uintptr_t)0xFFFF);
            void *q = mmap((void *)h, sz, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | 0x100000 /* MAP_FIXED_NOREPLACE */, -1, 0);
            if (q == MAP_FAILED) continue;
            if ((uintptr_t)q != h) { munmap(q, sz); continue; }
            pools[npool].base = h; pools[npool].size = sz; pools[npool].used = bytes; npool++;
            return h;
        }
    }
    return 0;
}

static void try_patch(uintptr_t pc, uint32_t i, uintptr_t ad) {
    unsigned sz = i >> 30;
    if (__atomic_exchange_n(&plock, 1, __ATOMIC_ACQUIRE)) return;  /* another thread is patching: trap this time */
    if (*(volatile uint32_t *)pc != i) goto out;                    /* already patched by another thread */
    if (!in_xmap(pc)) { INC(patch_fail); goto out; }                 /* JIT or anonymous code: leave it alone */
    uintptr_t pg = pc & ~(uintptr_t)4095;
    uint32_t newi;
    if ((i & 0x3FFFFC00u) == 0x38BFC000u) {                          /* LDAPR -> LDAR, same registers */
        newi = 0x08DFFC00u | (i & 0xC00003FFu);
    } else {
        int casp = (i & 0xBFA07C00u) == 0x08207C00u;
        if ((!casp && (ad & ((1u << sz) - 1))) || nrec == NREC) { INC(patch_fail); goto out; }
        if (casp && (ad & (((i >> 30) & 1) ? 15 : 7))) { INC(patch_fail); goto out; }   /* unaligned pair */
        uint32_t code[STUB_MAX]; struct stubrec *r = &recs[nrec];
        uintptr_t at = pool_alloc(pc, STUB_MAX * 4);
        if (!at) { INC(patch_fail); goto out; }
        int n = gen_stub(i, code, at, pc + 4, r);
        if (!n) { INC(patch_fail); goto out; }                       /* MIN/MAX: trap path */
        memcpy((void *)at, code, n * 4);
        __builtin___clear_cache((char *)at, (char *)at + n * 4);
        r->start = at; r->end = at + n * 4; r->site = pc; r->insn = i;
        __atomic_store_n(&nrec, nrec + 1, __ATOMIC_RELEASE);
        newi = e_b((long)(at - pc) / 4);
    }
    if (mprotect((void *)pg, 4096, PROT_READ | PROT_WRITE | PROT_EXEC)) { INC(patch_fail); goto out; }
    __atomic_store_n((uint32_t *)pc, newi, __ATOMIC_RELEASE);
    __builtin___clear_cache((char *)pc, (char *)pc + 4);
    mprotect((void *)pg, 4096, PROT_READ | PROT_EXEC);
    if ((i & 0x3FFFFC00u) == 0x38BFC000u) INC(inplace); else INC(patched);
out:
    __atomic_store_n(&plock, 0, __ATOMIC_RELEASE);
}

/* SIGBUS inside a stub: an exclusive load met an unaligned address. Undo the stub's pushes, emulate the
 * original instruction on the restored registers, continue after the site. */
static int stub_fault(mcontext_t *m) {
    uintptr_t pc = m->pc; int n = __atomic_load_n(&nrec, __ATOMIC_ACQUIRE);
    for (int j = 0; j < n; j++) {
        struct stubrec *r = &recs[j];
        if (pc < r->start || pc >= r->end) continue;
        uint64_t *sp = (uint64_t *)m->sp;
        if (r->flagreg != 0xff)                                       /* CAS, CASP: the caller's flags */
            m->pstate = (m->pstate & ~0xF0000000ULL) | (m->regs[r->flagreg] & 0xF0000000ULL);
        for (int k = 0; k < r->nsave; k++) m->regs[r->r[k]] = sp[k];
        m->sp += r->nsave == 6 ? 48 : 32;
        if (!emulate(r->insn, m)) return 0;
        INC(stub_bus);
        m->pc = r->site + 4;
        return 1;
    }
    return 0;
}
