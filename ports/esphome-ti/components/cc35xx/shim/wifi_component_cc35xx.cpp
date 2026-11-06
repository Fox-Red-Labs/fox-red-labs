// ESPHome Wi-Fi backend for TI CC35xx (LP-EM-CC35X1), on TI's Wi-Fi driver (Wlan_*) and TI's lwIP glue from the
// mqtt_client example (wifi_if.c, tcpip_if.c). Same shape as ESPHome's Pico W backend: the driver has no ESPHome-style
// event stream, so wifi_loop_() polls link and IP state. Scanning and the fallback access point are not implemented
// yet; use `fast_connect: true` (connect to the configured network without scanning first).
#include "esphome/components/wifi/wifi_component.h"

#if defined(USE_WIFI) && defined(USE_CC35XX)

#include <cstring>
#include "lwip/netif.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

extern "C" {
int FRL_wifi_init(void);
int FRL_wifi_sta_up(void);
int FRL_wifi_connect(const char *ssid, int ssid_len, const char *pass, int pass_len);
int FRL_wifi_disconnect(void);
int FRL_wifi_link(void);
void *FRL_wifi_netif(void);
const unsigned char *FRL_wifi_bssid(void);
const char *FRL_wifi_ssid(void);
int FRL_wifi_rssi(void);
int FRL_wifi_channel(void);
int FRL_wifi_scan_start(void);
int FRL_wifi_scan_done(void);
int FRL_wifi_scan_count(void);
int FRL_wifi_scan_entry(int i, char ssid[33], unsigned char bssid[6], int *rssi, int *channel, int *secured);
void FRL_log(const char *fmt, ...);
void frl_mdns_announce(void) __attribute__((weak));
}

namespace esphome::wifi {

static bool s_inited = false;        // NOLINT
static bool s_connecting = false;    // NOLINT
static bool s_was_connected = false; // NOLINT
static bool s_had_ip = false;        // NOLINT
static uint32_t s_connect_start = 0; // NOLINT

static struct netif *sta_netif() { return static_cast<struct netif *>(FRL_wifi_netif()); }

static bool sta_has_ip() {
  struct netif *n = sta_netif();
  return n != nullptr && netif_is_up(n) && !ip4_addr_isany_val(*netif_ip4_addr(n));
}

static bool sta_connected() { return FRL_wifi_link() && sta_has_ip(); }

bool WiFiComponent::wifi_mode_(optional<bool> sta, optional<bool> ap) {
  if (!s_inited) {
    int rc = FRL_wifi_init();
    FRL_log("[esphome-cc35xx] Wi-Fi driver start: %d\r\n", rc);
    if (rc != 0)
      return false;
    s_inited = true;
  }
  if (sta.has_value() && *sta) {
    int rc = FRL_wifi_sta_up();
    if (rc < 0) {
      FRL_log("[esphome-cc35xx] station role up failed: %d\r\n", rc);
      return false;
    }
  }
  (void) ap;
  return true;
}

bool WiFiComponent::wifi_apply_power_save_() { return true; }
bool WiFiComponent::wifi_apply_output_power_(float output_power) { return true; }
bool WiFiComponent::wifi_sta_pre_setup_() { return this->wifi_mode_(true, {}); }

bool WiFiComponent::wifi_sta_ip_config_(const optional<ManualIP> &manual_ip) {
  // DHCP only; TI's glue starts the DHCP client on the interface (TCPIP_IF_FLAGS_DHCPC).
  return !manual_ip.has_value();
}

bool WiFiComponent::wifi_apply_hostname_() {
#if LWIP_NETIF_HOSTNAME
  if (struct netif *n = sta_netif())
    netif_set_hostname(n, App.get_name().c_str());
#endif
  return true;
}

bool WiFiComponent::wifi_sta_connect_(const WiFiAP &ap) {
  if (!this->wifi_mode_(true, {}))
    return false;
  auto ssid = ap.get_ssid();
  auto pass = ap.get_password();
  int rc = FRL_wifi_connect(ssid.c_str(), (int) ssid.size(), pass.c_str(), (int) pass.size());
  FRL_log("[esphome-cc35xx] connect to '%s': %d\r\n", ssid.c_str(), rc);
  s_connecting = rc >= 0;
  s_connect_start = millis();
  return rc >= 0;
}

WiFiSTAConnectStatus WiFiComponent::wifi_sta_connect_status_() const {
  if (sta_connected())
    return WiFiSTAConnectStatus::CONNECTED;
  if (s_connecting) {
    // The driver reports a refused join as a disconnect event about a second after the attempt; with no link 8 s after
    // the attempt, call it failed so ESPHome retries (or scans) instead of waiting.
    if (!FRL_wifi_link() && millis() - s_connect_start > 8000)
      return WiFiSTAConnectStatus::ERROR_CONNECT_FAILED;
    return WiFiSTAConnectStatus::CONNECTING;
  }
  return WiFiSTAConnectStatus::IDLE;
}

static uint32_t s_scan_start = 0;  // NOLINT

bool WiFiComponent::wifi_scan_start_(bool passive) {
  if (!this->wifi_mode_(true, {}))
    return false;
  int rc = FRL_wifi_scan_start();
  s_scan_start = millis();
  return rc >= 0;
}

// Called from wifi_loop_() while scanning: copies TI's scan list into ESPHome's, as the LibreTiny backend does.
static void collect_scan(WiFiComponent *c, bool needs_full, wifi_scan_vector_t<WiFiScanResult> &out) {
  (void) c;
  int num = FRL_wifi_scan_count();
  char ssid[33];
  unsigned char b[6];
  int rssi, ch, sec;
  size_t count = 0;
  for (int i = 0; i < num; i++)
    if (FRL_wifi_scan_entry(i, ssid, b, &rssi, &ch, &sec) == 0)
      count++;
  out.clear();
  if (!out.try_init(count))
    return;
  for (int i = 0; i < num; i++) {
    if (FRL_wifi_scan_entry(i, ssid, b, &rssi, &ch, &sec) != 0)
      continue;
    out.emplace_back(bssid_t{b[0], b[1], b[2], b[3], b[4], b[5]}, ssid, strlen(ssid), (uint8_t) ch, (int8_t) rssi, sec != 0,
                     ssid[0] == '\0');
  }
}

#ifdef USE_WIFI_AP
bool WiFiComponent::wifi_ap_ip_config_(const optional<ManualIP> &manual_ip) { return false; }
bool WiFiComponent::wifi_start_ap_(const WiFiAP &ap) { return false; }
network::IPAddress WiFiComponent::wifi_soft_ap_ip() { return {}; }
#endif

bool WiFiComponent::wifi_disconnect_() {
  s_connecting = false;
  return FRL_wifi_disconnect() >= 0;
}

bssid_t WiFiComponent::wifi_bssid() {
  bssid_t b{};
  std::memcpy(b.data(), FRL_wifi_bssid(), 6);
  return b;
}

const char *WiFiComponent::wifi_ssid_to(std::span<char, SSID_BUFFER_SIZE> buffer) {
  std::strncpy(buffer.data(), FRL_wifi_ssid(), buffer.size() - 1);
  buffer[buffer.size() - 1] = '\0';
  return buffer.data();
}

int32_t WiFiComponent::get_wifi_channel() { return FRL_wifi_channel(); }

int8_t WiFiComponent::wifi_rssi() {
  if (!sta_connected())
    return WIFI_RSSI_DISCONNECTED;
  static uint32_t last = 0;
  static int8_t cached = WIFI_RSSI_DISCONNECTED;
  // One firmware round trip at most every 5 s; the wifi_signal sensor and the API both ask.
  if (cached == WIFI_RSSI_DISCONNECTED || millis() - last > 5000) {
    last = millis();
    cached = static_cast<int8_t>(FRL_wifi_rssi());
  }
  return cached;
}

network::IPAddresses WiFiComponent::wifi_sta_ip_addresses() {
  network::IPAddresses addrs{};
  if (struct netif *n = sta_netif())
    addrs[0] = network::IPAddress(&n->ip_addr);
  return addrs;
}

network::IPAddress WiFiComponent::wifi_subnet_mask_() {
  struct netif *n = sta_netif();
  return n ? network::IPAddress(&n->netmask) : network::IPAddress();
}
network::IPAddress WiFiComponent::wifi_gateway_ip_() {
  struct netif *n = sta_netif();
  return n ? network::IPAddress(&n->gw) : network::IPAddress();
}
network::IPAddress WiFiComponent::wifi_dns_ip_(int num) {
  const ip_addr_t *d = dns_getserver(num);
  return network::IPAddress(d);
}

bool WiFiComponent::wifi_loop_() {
  if (this->state_ == WIFI_COMPONENT_STATE_STA_SCANNING && !this->scan_done_ &&
      (FRL_wifi_scan_done() || millis() - s_scan_start > 10000)) {
    {
      ScanResultsLock lock(this);
      collect_scan(this, this->needs_full_scan_results_(), this->scan_result_);
      this->scan_done_ = true;
    }
    FRL_log("[esphome-cc35xx] scan done: %d networks\r\n", FRL_wifi_scan_count());
#ifdef USE_WIFI_SCAN_RESULTS_LISTENERS
    this->notify_scan_results_listeners_();
#endif
  }
  bool connected = sta_connected();
  static uint32_t diag = 0;
  if (!connected && millis() - diag > 5000) {
    diag = millis();
    struct netif *n = sta_netif();
    char ip[16] = "-";
    if (n) ip4addr_ntoa_r(netif_ip4_addr(n), ip, sizeof(ip));
    FRL_log("[esphome-cc35xx] waiting: link %d, netif %p up %d, ip %s\r\n", FRL_wifi_link(), (void *) n, n ? (int) netif_is_up(n) : -1, ip);
  }
  if (connected && !s_was_connected) {
    s_was_connected = true;
    s_connecting = false;
    char ip[16];
    ip4addr_ntoa_r(netif_ip4_addr(sta_netif()), ip, sizeof(ip));
    FRL_log("[esphome-cc35xx] connected, IP %s, channel %d\r\n", ip, FRL_wifi_channel());
    FRL_log("[esphome-cc35xx] RSSI %d\r\n", (int) this->wifi_rssi());
#ifdef USE_WIFI_CONNECT_STATE_LISTENERS
    this->pending_.connect_state = true;
#endif
  } else if (!connected && s_was_connected) {
    s_was_connected = false;
    s_had_ip = false;
    FRL_log("[esphome-cc35xx] disconnected\r\n");
    this->update_connected_state_();
#ifdef USE_WIFI_CONNECT_STATE_LISTENERS
    this->notify_disconnect_state_listeners_();
#endif
  }
  if (connected && !s_had_ip) {
    s_had_ip = true;
    if (frl_mdns_announce)
      frl_mdns_announce();
#ifdef USE_WIFI_IP_STATE_LISTENERS
    this->notify_ip_state_listeners_();
#endif
  }
  return true;
}

void WiFiComponent::wifi_pre_setup_() {}

}  // namespace esphome::wifi
#endif
