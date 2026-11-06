# micro-wake-word-models

- Upstream: https://github.com/esphome/micro-wake-word-models, commit `40ff33f57f8fc6ad71a75ef085abbf742495b225` (main when copied, 2026-10-30). The published wake word models of ESPHome's `micro_wake_word` component (author Kevin Ahrendt, Open Home Foundation).
- Copied unmodified: `v2/okay_nabu.*`, `v2/hey_jarvis.*`, `v2/vad.*` (quantised streaming TFLite models and their manifests), `LICENSE`, `NOTICE`. Hashes are in `SHA256SUMS`. Left out: the other v2 models (alexa, hey_mycroft), the v1 models (115 kB each, an older and larger network), and `experiments/`.
- Licence: Apache License 2.0 for the whole repository, model weights included. The upstream README says: "There are no separate or additional terms for the model binaries." The wake word names (Okay Nabu, Hey Jarvis) belong to third parties and imply no endorsement (see `NOTICE`).
- Used by: `ports/microwakeword/`.
