/* ML-DSA-44 (FIPS 204) signatures beside the post-quantum join. Board-independent: uses the same pqc_hal_t.
 * Library: third-party/mldsa-native (v2.0.0, unmodified), built with MLD_CONFIG_PARAMETER_SET=44 and
 * MLD_CONFIG_NO_RANDOMIZED_API (the board supplies randomness itself). */
#ifndef PQC_SIGN_H
#define PQC_SIGN_H
#include "pqc_join.h"

#define PQC_DSA_PK_BYTES 1312
#define PQC_DSA_SK_BYTES 2560
#define PQC_DSA_SIG_BYTES 2420

/* Known-answer tests: NIST ACVP ML-DSA-44 key generation tcId 1, one hedged and one deterministic signing test,
 * one valid and one invalid signature to verify, plus a sign-then-verify round trip and a one-bit flip.
 * Returns 1 on pass. Prints and records rows. */
int pqc_sign_kat(const pqc_hal_t *h);
/* Time keygen, sign and verify three times each (fixed inputs, so the three agree), painting the stack before each
 * call, then sign 16 different messages to show how much rejection sampling moves the signing time.
 * Rows: mldsa44_keygen|sign|verify (median cycles), mldsa44_sign16 (median of 16, min and max in the note),
 * stack_mldsa44_keygen|sign|verify (net bytes). */
void pqc_sign_timing(const pqc_hal_t *h);
int pqc_dsa_nosign(const pqc_hal_t *h);   /* keygen + verify KATs and timing, no signing (small-stack parts) */
#endif
