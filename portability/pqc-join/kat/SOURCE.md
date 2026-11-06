# NIST known-answer test vectors for ML-KEM-768 and ML-DSA-44

Terms: [docs/vocab.md](../../../docs/vocab.md). Where each vector in the post-quantum join tests comes from.

`nist_mlkem768_kat.json` holds three ML-KEM-768 vectors taken from NIST's ACVP-Server repository (public NIST data), picked by `make_kat.py`, which also writes `../core/pqc_kat_data.h`.

| Vector | Where in the NIST file | Why |
|---|---|---|
| Key generation, tcId 26 | `gen-val/json-files/ML-KEM-keyGen-FIPS203/internalProjection.json`, group ML-KEM-768 | The same vector TI's `pqc_mlkem_768_keygen` example carries, so TI's library and this code are checked against one answer |
| Encapsulation, tcId 26 (first of the group) | `.../ML-KEM-encapDecap-FIPS203/internalProjection.json`, group ML-KEM-768 encapsulation | Checks ciphertext and shared secret from a fixed message |
| Decapsulation, tcId 86 (first of the group) | same file, group ML-KEM-768 decapsulation | Checks the shared secret from a fixed key and ciphertext |

Fetched 2026-10-08 from `https://raw.githubusercontent.com/usnistgov/ACVP-Server/master/`. SHA-256 of the two source files is in `_source` inside the JSON. mlkem-native's own `test/test_vectors` are not NIST vectors (they come from a deterministic test random number generator), so they are not used here.

## ML-DSA-44

`nist_mldsa44_kat.json` holds five ML-DSA-44 vectors from the same repository, picked by `make_kat_mldsa.py`, which also writes `../core/pqc_mldsa_kat_data.h`. All use the external interface with pure signing (FIPS 204 Algorithms 2 and 3, with a context string), which is what an application calls.

| Vector | Where in the NIST file | Why |
|---|---|---|
| Key generation, tcId 1 | `gen-val/json-files/ML-DSA-keyGen-FIPS204/internalProjection.json`, group ML-DSA-44 | Checks the public and secret key from a fixed seed |
| Signing, hedged, tcId 194 | `.../ML-DSA-sigGen-FIPS204/internalProjection.json`, group ML-DSA-44, not deterministic, external, pure (shortest message) | Checks the signature from a fixed key, message, context and random value |
| Signing, deterministic, tcId 7 | same file, deterministic group (shortest message) | Same, with the all-zero random value FIPS 204 defines for deterministic signing |
| Verification, valid, tcId 11 | `.../ML-DSA-sigVer-FIPS204/internalProjection.json`, group ML-DSA-44, external, pure (shortest valid) | Must accept |
| Verification, invalid, tcId 9 | same group (shortest invalid; NIST's reason: "modified signature - hint") | Must reject |

Fetched 2026-10-08 from the same address. SHA-256 of the three source files is in `_source` inside the JSON.
