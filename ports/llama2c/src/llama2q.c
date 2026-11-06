/* See llama2q.h. Derived from runq.c (llama2.c, MIT, Copyright (c) 2023 Andrej Karpathy). */
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "llama2q.h"

#define KV_DIM ((LL_DIM * LL_KV_HEADS) / LL_HEADS)
#define HEAD_SIZE (LL_DIM / LL_HEADS)

static int GS;   /* group size of the checkpoint */

typedef struct { const int8_t *q; const float *s; } QT;

static struct {
    const float *rms_att, *rms_ffn, *rms_final;
    QT tokens, wq[LL_LAYERS], wk[LL_LAYERS], wv[LL_LAYERS], wo[LL_LAYERS], w1[LL_LAYERS], w2[LL_LAYERS], w3[LL_LAYERS];
    const QT *wcls;
} W;

/* Run state, all static. */
static float x[LL_DIM], xb[LL_DIM], xb2[LL_DIM], hb[LL_HIDDEN], hb2[LL_HIDDEN], q[LL_DIM], k[KV_DIM], v[KV_DIM];
static float att[LL_HEADS * LL_STEPS], logits[LL_VOCAB];
static int8_t xq_q[LL_DIM], hq_q[LL_HIDDEN];
static float xq_s[LL_DIM], hq_s[LL_HIDDEN];   /* sized for group size 1 so any group size fits */
static float key_cache[LL_LAYERS * LL_STEPS * KV_DIM], value_cache[LL_LAYERS * LL_STEPS * KV_DIM];

int ll_ids[LL_STEPS];
uint32_t ll_fwd_cycles[LL_STEPS];
int ll_prompt_tokens;
#ifdef LL_DEBUG_POS
float ll_dbg_logits[LL_VOCAB];   /* raw logits at position LL_DEBUG_POS, for comparing builds */
#endif

static void map_tensors(QT *out, int n, const uint8_t **p, int size_each)
{
    for (int i = 0; i < n; i++) {
        out[i].q = (const int8_t *)*p; *p += size_each;
        out[i].s = (const float *)*p;  *p += (size_each / GS) * sizeof(float);
    }
}

/* ---- tokenizer: strings are copied into one static pool so they end in NUL ---- */
typedef struct { char *str; int id; } TokenIndex;
static char *vocab[LL_VOCAB];
static float vocab_scores[LL_VOCAB];
static char vocab_pool[8192];
static TokenIndex sorted_vocab[LL_VOCAB];
static unsigned int max_token_length;
static unsigned char byte_pieces[512];

static int compare_tokens(const void *a, const void *b) { return strcmp(((const TokenIndex *)a)->str, ((const TokenIndex *)b)->str); }

int ll_init(const uint8_t *m, const uint8_t *t)
{
    uint32_t magic; int version; int cfg[7];
    memcpy(&magic, m, 4); memcpy(&version, m + 4, 4); memcpy(cfg, m + 8, 28);
    if (magic != 0x616b3432u || version != 2) return -1;
    if (cfg[0] != LL_DIM || cfg[1] != LL_HIDDEN || cfg[2] != LL_LAYERS || cfg[3] != LL_HEADS || cfg[4] != LL_KV_HEADS || cfg[5] != LL_VOCAB) return -2;
    uint8_t shared = m[36]; memcpy(&GS, m + 37, 4);
    if (GS < 1 || LL_DIM % GS || LL_HIDDEN % GS || !shared) return -3;
    if ((uintptr_t)m & 3) return -4;
    const uint8_t *p = m + 256;
    W.rms_att = (const float *)p; p += LL_LAYERS * LL_DIM * 4;
    W.rms_ffn = (const float *)p; p += LL_LAYERS * LL_DIM * 4;
    W.rms_final = (const float *)p; p += LL_DIM * 4;
    map_tensors(&W.tokens, 1, &p, LL_VOCAB * LL_DIM);
    map_tensors(W.wq, LL_LAYERS, &p, LL_DIM * LL_DIM);
    map_tensors(W.wk, LL_LAYERS, &p, LL_DIM * KV_DIM);
    map_tensors(W.wv, LL_LAYERS, &p, LL_DIM * KV_DIM);
    map_tensors(W.wo, LL_LAYERS, &p, LL_DIM * LL_DIM);
    map_tensors(W.w1, LL_LAYERS, &p, LL_DIM * LL_HIDDEN);
    map_tensors(W.w2, LL_LAYERS, &p, LL_HIDDEN * LL_DIM);
    map_tensors(W.w3, LL_LAYERS, &p, LL_DIM * LL_HIDDEN);
    W.wcls = &W.tokens;
    /* tokenizer file: int max_token_length, then per token: float score, int len, len bytes */
    memcpy(&max_token_length, t, 4); t += 4;
    size_t used = 0;
    for (int i = 0; i < LL_VOCAB; i++) {
        int len; memcpy(&vocab_scores[i], t, 4); memcpy(&len, t + 4, 4); t += 8;
        if (used + len + 1 > sizeof vocab_pool || len < 0) return -5;
        vocab[i] = vocab_pool + used; memcpy(vocab[i], t, len); vocab[i][len] = '\0'; used += len + 1; t += len;
    }
    for (int i = 0; i < 256; i++) { byte_pieces[i * 2] = (unsigned char)i; byte_pieces[i * 2 + 1] = '\0'; }
    for (int i = 0; i < LL_VOCAB; i++) { sorted_vocab[i].str = vocab[i]; sorted_vocab[i].id = i; }
    qsort(sorted_vocab, LL_VOCAB, sizeof(TokenIndex), compare_tokens);
    return 0;
}

/* ---- neural net blocks, runq.c ---- */
typedef struct { int8_t *q; float *s; } QV;   /* a quantized activation vector */

static void quantize(QV *qx, const float *xin, int n)
{
    int num_groups = n / GS; float Q_MAX = 127.0f;
    for (int group = 0; group < num_groups; group++) {
        float wmax = 0.0f;
        for (int i = 0; i < GS; i++) { float val = fabsf(xin[group * GS + i]); if (val > wmax) wmax = val; }
        float scale = wmax / Q_MAX;
        qx->s[group] = scale;
        for (int i = 0; i < GS; i++) {
            float quant_value = xin[group * GS + i] / scale;
            qx->q[group * GS + i] = (int8_t)roundf(quant_value);
        }
    }
}

static void rmsnorm(float *o, const float *xin, const float *weight, int size)
{
    float ss = 0.0f;
    for (int j = 0; j < size; j++) ss += xin[j] * xin[j];
    ss /= size; ss += 1e-5f; ss = 1.0f / sqrtf(ss);
    for (int j = 0; j < size; j++) o[j] = weight[j] * (ss * xin[j]);
}

static void softmax(float *xs, int size)
{
    float max_val = xs[0];
    for (int i = 1; i < size; i++) if (xs[i] > max_val) max_val = xs[i];
    float sum = 0.0f;
    for (int i = 0; i < size; i++) { xs[i] = expf(xs[i] - max_val); sum += xs[i]; }
    for (int i = 0; i < size; i++) xs[i] /= sum;
}

static void matmul(float *xout, const int8_t *xqq, const float *xqs, const QT *w, int n, int d)
{
    for (int i = 0; i < d; i++) {
        float val = 0.0f; int32_t ival = 0; int in = i * n;
        for (int j = 0; j <= n - GS; j += GS) {
            for (int kk = 0; kk < GS; kk++) ival += ((int32_t)xqq[j + kk]) * ((int32_t)w->q[in + j + kk]);
            val += ((float)ival) * w->s[(in + j) / GS] * xqs[j / GS];
            ival = 0;
        }
        xout[i] = val;
    }
}

__attribute__((noinline)) static float *forward(int token, int pos)
{
    const int kv_mul = LL_HEADS / LL_KV_HEADS;
    QV xq = { xq_q, xq_s }, hq = { hq_q, hq_s };
    /* one row of the embedding table, dequantized: same product as dequantize() in runq.c */
    for (int i = 0; i < LL_DIM; i++) x[i] = W.tokens.q[token * LL_DIM + i] * W.tokens.s[(token * LL_DIM + i) / GS];
    for (int l = 0; l < LL_LAYERS; l++) {
        rmsnorm(xb, x, W.rms_att + l * LL_DIM, LL_DIM);
        quantize(&xq, xb, LL_DIM);
        matmul(q, xq.q, xq.s, &W.wq[l], LL_DIM, LL_DIM);
        matmul(k, xq.q, xq.s, &W.wk[l], LL_DIM, KV_DIM);
        matmul(v, xq.q, xq.s, &W.wv[l], LL_DIM, KV_DIM);
        for (int i = 0; i < LL_DIM; i += 2) {
            int head_dim = i % HEAD_SIZE;
            float freq = 1.0f / powf(10000.0f, head_dim / (float)HEAD_SIZE);
            float val = pos * freq;
            float fcr = cosf(val), fci = sinf(val);
            int rotn = i < KV_DIM ? 2 : 1;
            for (int vv = 0; vv < rotn; vv++) {
                float *vec = vv == 0 ? q : k;
                float v0 = vec[i], v1 = vec[i + 1];
                vec[i] = v0 * fcr - v1 * fci;
                vec[i + 1] = v0 * fci + v1 * fcr;
            }
        }
        int loff = l * LL_STEPS * KV_DIM;
        memcpy(key_cache + loff + pos * KV_DIM, k, KV_DIM * sizeof(float));
        memcpy(value_cache + loff + pos * KV_DIM, v, KV_DIM * sizeof(float));
        for (int h = 0; h < LL_HEADS; h++) {
            float *qh = q + h * HEAD_SIZE;
            float *a = att + h * LL_STEPS;
            for (int t = 0; t <= pos; t++) {
                float *kk = key_cache + loff + t * KV_DIM + (h / kv_mul) * HEAD_SIZE;
                float score = 0.0f;
                for (int i = 0; i < HEAD_SIZE; i++) score += qh[i] * kk[i];
                score /= sqrtf(HEAD_SIZE);
                a[t] = score;
            }
            softmax(a, pos + 1);
            float *xbh = xb + h * HEAD_SIZE;
            memset(xbh, 0, HEAD_SIZE * sizeof(float));
            for (int t = 0; t <= pos; t++) {
                float *vv = value_cache + loff + t * KV_DIM + (h / kv_mul) * HEAD_SIZE;
                float aw = a[t];
                for (int i = 0; i < HEAD_SIZE; i++) xbh[i] += aw * vv[i];
            }
        }
        quantize(&xq, xb, LL_DIM);
        matmul(xb2, xq.q, xq.s, &W.wo[l], LL_DIM, LL_DIM);
        for (int i = 0; i < LL_DIM; i++) x[i] += xb2[i];
        rmsnorm(xb, x, W.rms_ffn + l * LL_DIM, LL_DIM);
        quantize(&xq, xb, LL_DIM);
        matmul(hb, xq.q, xq.s, &W.w1[l], LL_DIM, LL_HIDDEN);
        matmul(hb2, xq.q, xq.s, &W.w3[l], LL_DIM, LL_HIDDEN);
        for (int i = 0; i < LL_HIDDEN; i++) {
            float val = hb[i];
            val *= (1.0f / (1.0f + expf(-val)));
            val *= hb2[i];
            hb[i] = val;
        }
        quantize(&hq, hb, LL_HIDDEN);
        matmul(xb, hq.q, hq.s, &W.w2[l], LL_HIDDEN, LL_DIM);
        for (int i = 0; i < LL_DIM; i++) x[i] += xb[i];
    }
    rmsnorm(x, x, W.rms_final, LL_DIM);
    quantize(&xq, x, LL_DIM);
    matmul(logits, xq.q, xq.s, W.wcls, LL_DIM, LL_VOCAB);
    return logits;
}

/* ---- tokenizer use, runq.c ---- */
static const char *decode(int prev_token, int token)
{
    const char *piece = vocab[token];
    if (prev_token == 1 && piece[0] == ' ') piece++;
    /* raw-byte tokens look like <0x0A>: same result as sscanf("<0x%02hhX>") */
    if (piece[0] == '<' && piece[1] == '0' && piece[2] == 'x' && piece[5] == '>' && piece[6] == '\0') {
        unsigned val = 0; int ok = 1;
        for (int i = 3; i < 5; i++) {
            char c = piece[i]; unsigned d;
            if (c >= '0' && c <= '9') d = c - '0'; else if (c >= 'A' && c <= 'F') d = c - 'A' + 10; else if (c >= 'a' && c <= 'f') d = c - 'a' + 10; else { ok = 0; break; }
            val = val * 16 + d;
        }
        if (ok) piece = (const char *)byte_pieces + val * 2;
    }
    return piece;
}

static int str_lookup(const char *str)
{
    TokenIndex tok = { (char *)str, 0 };
    TokenIndex *res = bsearch(&tok, sorted_vocab, LL_VOCAB, sizeof(TokenIndex), compare_tokens);
    return res != NULL ? res->id : -1;
}

static void encode(const char *text, int *tokens, int *n_tokens)
{
    char str_buffer[64]; size_t str_len = 0;
    *n_tokens = 0;
    tokens[(*n_tokens)++] = 1;   /* BOS */
    if (text[0] != '\0') tokens[(*n_tokens)++] = str_lookup(" ");
    for (const char *c = text; *c != '\0'; c++) {
        if ((*c & 0xC0) != 0x80) str_len = 0;
        str_buffer[str_len++] = *c; str_buffer[str_len] = '\0';
        if ((*(c + 1) & 0xC0) == 0x80 && str_len < 4) continue;
        int id = str_lookup(str_buffer);
        if (id != -1) tokens[(*n_tokens)++] = id;
        else for (size_t i = 0; i < str_len; i++) tokens[(*n_tokens)++] = (unsigned char)str_buffer[i] + 3;
        str_len = 0;
    }
    while (1) {
        float best_score = -1e10f; int best_id = -1, best_idx = -1;
        for (int i = 0; i < (*n_tokens - 1); i++) {
            size_t la = strlen(vocab[tokens[i]]), lb = strlen(vocab[tokens[i + 1]]);
            if (la + lb >= sizeof str_buffer) continue;
            memcpy(str_buffer, vocab[tokens[i]], la); memcpy(str_buffer + la, vocab[tokens[i + 1]], lb + 1);
            int id = str_lookup(str_buffer);
            if (id != -1 && vocab_scores[id] > best_score) { best_score = vocab_scores[id]; best_id = id; best_idx = i; }
        }
        if (best_idx == -1) break;
        tokens[best_idx] = best_id;
        for (int i = best_idx + 1; i < (*n_tokens - 1); i++) tokens[i] = tokens[i + 1];
        (*n_tokens)--;
    }
}

/* ---- sampler, runq.c ---- */
typedef struct { float prob; int index; } ProbIndex;
static ProbIndex probindex[LL_VOCAB];

static int sample_argmax(const float *p, int n)
{
    int max_i = 0; float max_p = p[0];
    for (int i = 1; i < n; i++) if (p[i] > max_p) { max_i = i; max_p = p[i]; }
    return max_i;
}
static int sample_mult(const float *p, int n, float coin)
{
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) { cdf += p[i]; if (coin < cdf) return i; }
    return n - 1;
}
static int compare_prob(const void *a, const void *b)
{
    const ProbIndex *a_ = a, *b_ = b;
    if (a_->prob > b_->prob) return -1;
    if (a_->prob < b_->prob) return 1;
    return 0;
}
static int sample_topp(const float *p, int n, float topp, float coin)
{
    int n0 = 0;
    const float cutoff = (1.0f - topp) / (n - 1);
    for (int i = 0; i < n; i++) if (p[i] >= cutoff) { probindex[n0].index = i; probindex[n0].prob = p[i]; n0++; }
    qsort(probindex, n0, sizeof(ProbIndex), compare_prob);
    float cumulative_prob = 0.0f; int last_idx = n0 - 1;
    for (int i = 0; i < n0; i++) { cumulative_prob += probindex[i].prob; if (cumulative_prob > topp) { last_idx = i; break; } }
    float r = coin * cumulative_prob, cdf = 0.0f;
    for (int i = 0; i <= last_idx; i++) { cdf += probindex[i].prob; if (r < cdf) return probindex[i].index; }
    return probindex[last_idx].index;
}
static unsigned int random_u32(unsigned long long *state)
{
    *state ^= *state >> 12; *state ^= *state << 25; *state ^= *state >> 27;
    return (*state * 0x2545F4914F6CDD1Dull) >> 32;
}
static float random_f32(unsigned long long *state) { return (random_u32(state) >> 8) / 16777216.0f; }

static int sample(float *lg, float temperature, float topp, unsigned long long *rng)
{
    if (temperature == 0.0f) return sample_argmax(lg, LL_VOCAB);
    for (int i = 0; i < LL_VOCAB; i++) lg[i] /= temperature;
    softmax(lg, LL_VOCAB);
    float coin = random_f32(rng);
    if (topp <= 0 || topp >= 1) return sample_mult(lg, LL_VOCAB, coin);
    return sample_topp(lg, LL_VOCAB, topp, coin);
}

int ll_generate(const char *prompt, int steps, float temperature, float topp, unsigned long long seed, void (*emit)(const char *))
{
    int prompt_tokens[LL_MAX_PROMPT]; int num_prompt_tokens = 0;
    if (strlen(prompt) + 3 > LL_MAX_PROMPT) return -1;
    if (steps > LL_STEPS) steps = LL_STEPS;
    unsigned long long rng = seed;
    encode(prompt, prompt_tokens, &num_prompt_tokens);
    ll_prompt_tokens = num_prompt_tokens;
    int next, token = prompt_tokens[0], pos = 0;
    while (pos < steps) {
        ll_ids[pos] = token;
        uint32_t c0 = LL_CYCLES();
        float *lg = forward(token, pos);
        ll_fwd_cycles[pos] = LL_CYCLES() - c0;
#ifdef LL_DEBUG_POS
        if (pos == LL_DEBUG_POS) memcpy(ll_dbg_logits, lg, sizeof ll_dbg_logits);
#endif
        if (pos < num_prompt_tokens - 1) next = prompt_tokens[pos + 1];
        else next = sample(lg, temperature, topp, &rng);
        pos++;
        if (next == 1) break;
        emit(decode(token, next));
        token = next;
    }
    return pos;
}
