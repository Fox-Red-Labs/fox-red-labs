/*
 * Spinel library settings for the border-router host on the CC1354P10 (OPENTHREAD_PROJECT_LIB_CONFIG_FILE).
 *
 * The POSIX host keeps an 8 KB receive frame buffer; an MCU with 256 KB of RAM does not need to. A spinel frame
 * carries at most one 802.15.4 frame (127 bytes) plus a property header, and the longest replies seen from TI's
 * RCP (version string, capability list) are well under 300 bytes. 2 KB leaves room for several queued frames.
 */

#ifndef OPENTHREAD_HOST_LIB_CONFIG_H_
#define OPENTHREAD_HOST_LIB_CONFIG_H_

#define OPENTHREAD_LIB_SPINEL_RX_FRAME_BUFFER_SIZE 2048

/* No automatic RCP recovery yet: a dead link stops the host with a logged failure, which is what M1 wants to see. */
#define OPENTHREAD_SPINEL_CONFIG_RCP_RESTORATION_MAX_COUNT 0

/* Spinel log lines are routed through OpenThread logging; keep them short on the MCU. */
#define OPENTHREAD_LIB_SPINEL_LOG_MAX_SIZE 256

#endif // OPENTHREAD_HOST_LIB_CONFIG_H_
