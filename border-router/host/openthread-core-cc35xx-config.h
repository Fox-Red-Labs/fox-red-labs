/* OpenThread project configuration for a border-router host on the TI CC35xx (Cortex-M33), with the radio
 * on a separate TI chip running ot-rcp, reached over spinel/HDLC on a UART. Fox Red Labs, public sources only. */
#ifndef OPENTHREAD_CORE_CC35XX_CONFIG_H_
#define OPENTHREAD_CORE_CC35XX_CONFIG_H_

#define OPENTHREAD_CONFIG_PLATFORM_INFO "CC35XX-HOST"

/* The radio is a co-processor: this build is a host (like ot-br-posix), not a radio. */
#define OPENTHREAD_LIB_SPINEL_RX_FRAME_BUFFER_SIZE 512
#define OPENTHREAD_SPINEL_CONFIG_RCP_RESTORATION_MAX_COUNT 2

/* Border router features (all in OpenThread's core), with the switches they depend on */
#define OPENTHREAD_CONFIG_BORDER_ROUTER_ENABLE 1
#define OPENTHREAD_CONFIG_TMF_NETDATA_SERVICE_ENABLE 1
#define OPENTHREAD_CONFIG_ECDSA_ENABLE 1
#define OPENTHREAD_CONFIG_DETERMINISTIC_ECDSA_ENABLE 1
#define OPENTHREAD_CONFIG_BORDER_ROUTING_ENABLE 1
#define OPENTHREAD_CONFIG_SRP_SERVER_ENABLE 1
#define OPENTHREAD_CONFIG_DNSSD_SERVER_ENABLE 1
#define OPENTHREAD_CONFIG_MULTICAST_DNS_ENABLE 1
#define OPENTHREAD_CONFIG_NAT64_BORDER_ROUTING_ENABLE 1
#define OPENTHREAD_CONFIG_NAT64_TRANSLATOR_ENABLE 1
#define OPENTHREAD_CONFIG_BORDER_AGENT_ENABLE 1
#define OPENTHREAD_CONFIG_NETDATA_PUBLISHER_ENABLE 1
#define OPENTHREAD_CONFIG_IP6_SLAAC_ENABLE 1

/* Memory: start small; tune against the 512 KB application RAM once Wi-Fi and lwIP are in the same image */
#define OPENTHREAD_CONFIG_NUM_MESSAGE_BUFFERS 96
#define OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE 0

#endif
