// TI CC35xx shim: ESPHome's mDNS on lwIP's mDNS responder, which TI's Wi-Fi SDK builds into lwip.a
// (LWIP_MDNS_RESPONDER). Home Assistant finds ESPHome devices by their _esphomelib._tcp service; this announces it
// on the station interface. The lwIP calls are in C (ti/frl_wifi_append.c). Replaces mdns_zephyr.cpp, a stub.
#include "esphome/core/defines.h"
#if defined(USE_CC35XX) && defined(USE_MDNS)

#include <cstdio>
#include <cstring>
#include "esphome/components/mdns/mdns_component.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

extern "C" {
int FRL_mdns_add(int slot, const char *host, const char *service, int udp, int port, const char *const *txt, int ntxt);
void FRL_mdns_announce(void);
void FRL_mdns_remove(void);
}

namespace esphome::mdns {

static const char *const TAG = "mdns.cc35xx";

static void register_cc35xx(MDNSComponent *, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services) {
  int slot = 0, added = 0;
  for (auto &service : services) {
    static char items[16][64];
    const char *ptrs[16];
    int n = 0;
    for (const auto &record : service.txt_records) {
      if (n >= 16)
        break;
      snprintf(items[n], sizeof(items[n]), "%s=%s", MDNS_STR_ARG(record.key), MDNS_STR_ARG(record.value));
      ptrs[n] = items[n];
      n++;
    }
    const char *proto = MDNS_STR_ARG(service.proto);
    int rc = FRL_mdns_add(slot++, App.get_name().c_str(), MDNS_STR_ARG(service.service_type), std::strcmp(proto, "_udp") == 0,
                          service.port.value(), ptrs, n);
    if (rc >= 0)
      added++;
  }
  ESP_LOGI(TAG, "mDNS responder on the station interface: %s.local, %d of %d services", App.get_name().c_str(), added,
           (int) services.size());
}

void MDNSComponent::setup() { this->setup_buffers_and_register_(register_cc35xx); }
void MDNSComponent::on_shutdown() { FRL_mdns_remove(); }

}  // namespace esphome::mdns

// Called by the Wi-Fi backend when the station gets its address, so the first announcement goes out at once.
extern "C" void frl_mdns_announce(void) { FRL_mdns_announce(); }
#endif
