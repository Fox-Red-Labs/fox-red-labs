/* The configurations: hz, frame_us, kbps, application kind (0 voice/VoIP, 1 audio, 2 restricted low delay), label.
 * Each runs at complexity 0, 5 and 10. */
static const int op_cfg[][4] = {
    {16000, 20000, 32, 0},     /* SILK, 16 kHz voice, 20 ms */
    {48000, 10000, 96, 1},     /* CELT, 48 kHz music, 10 ms */
    {48000, 20000, 96, 1},     /* CELT, 48 kHz music, 20 ms */
    {16000, 10000, 32, 2},     /* CELT, 16 kHz, 10 ms, to set beside LC3 at 16 kHz */
};
static const int op_cplx[] = { 0, 5, 10 };
#define OP_NCFG ((int)(sizeof op_cfg / sizeof op_cfg[0]))
#define OP_NCPLX 3
#define OP_N (OP_NCFG * OP_NCPLX)
