// TI CC35xx shim: an ESPHome native-API client that runs on the board itself and talks to the board's own API server
// over lwIP's loopback interface (127.0.0.1:6053). Built only with `api_self_test: true` in the cc35xx: block.
// It proves the API server, the sockets and the entity plumbing without any other device on the network; it does not
// exercise the Wi-Fi hop, which only a client on the same network (Home Assistant) can. Messages and field numbers are
// ESPHome's api.proto as generated in api_pb2.h (2026.9.1): Hello 1/2, DeviceInfo 9/10, ListEntities 11..19,
// SubscribeStates 20, SwitchCommand 33, SwitchState 26. Frames are ESPHome's plaintext framing: 0x00, varint length,
// varint type, payload.
#include "esphome/core/defines.h"
#if defined(USE_CC35XX) && defined(FRL_API_SELF_TEST) && defined(USE_API)

#include <cstdint>
#include <cstring>
#include <algorithm>
extern "C" {
#include <FreeRTOS.h>
#include <task.h>
#include <ti/drivers/GPIO.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_types.h)
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include DeviceFamily_constructPath(inc/hw_iomux.h)
void FRL_log(const char *fmt, ...);
}
#include "lwip/sockets.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"

extern "C" void *FRL_wifi_netif(void);

namespace {

int put_varint(uint8_t *p, uint32_t v) {
  int n = 0;
  do {
    uint8_t b = v & 0x7F;
    v >>= 7;
    p[n++] = b | (v ? 0x80 : 0);
  } while (v);
  return n;
}

// One write per frame: a header and body in two writes meet Nagle's algorithm and the peer's delayed ACK (about 250 ms).
bool send_frame(int fd, uint32_t type, const uint8_t *payload, uint32_t len) {
  uint8_t f[96];
  int n = 0;
  f[n++] = 0x00;
  n += put_varint(f + n, len);
  n += put_varint(f + n, type);
  if (len > sizeof(f) - n)
    return false;
  if (len)
    std::memcpy(f + n, payload, len);
  n += len;
  return lwip_send(fd, f, n, 0) == n;
}

// The LED's output latch (OUT bit of its I/O control register; TI's GPIO_write sets it). GPIO_read returns the pad value,
// which reads 0 on a pin driven as an output here. Address as in TI's GPIOWFF3.c (IOMUX_CTL_ADDR).
#define IOMUX_IO_REGION_SIZE (4096U)  // as in TI's GPIOWFF3.c
bool led_latch(uint8_t pin) {
  const uint32_t addr = IOMUX_BASE + IOMUX_O_SCLKICFG + (pin * IOMUX_IO_REGION_SIZE) + (IOMUX_O_GPIO2CTL - IOMUX_O_GPIO2CFG);
  return (HWREG(addr) & IOMUX_GPIO2CTL_OUT_M) != 0;
}

bool read_exact(int fd, uint8_t *buf, uint32_t len) {
  uint32_t got = 0;
  while (got < len) {
    int r = lwip_recv(fd, buf + got, len - got, 0);
    if (r <= 0)
      return false;
    got += r;
  }
  return true;
}

bool read_varint(int fd, uint32_t *out) {
  uint32_t v = 0;
  for (int shift = 0; shift < 35; shift += 7) {
    uint8_t b;
    if (!read_exact(fd, &b, 1))
      return false;
    v |= (uint32_t) (b & 0x7F) << shift;
    if (!(b & 0x80)) {
      *out = v;
      return true;
    }
  }
  return false;
}

// Returns the message type, fills payload (truncated to cap) and its full length; -1 on error or timeout.
int read_frame(int fd, uint8_t *payload, uint32_t cap, uint32_t *len_out) {
  uint8_t zero;
  uint32_t len, type;
  if (!read_exact(fd, &zero, 1) || zero != 0x00 || !read_varint(fd, &len) || !read_varint(fd, &type))
    return -1;
  uint32_t keep = std::min(len, cap);
  if (keep && !read_exact(fd, payload, keep))
    return -1;
  for (uint32_t skip = keep; skip < len; skip++) {  // drain the rest of an oversize message
    uint8_t b;
    if (!read_exact(fd, &b, 1))
      return -1;
  }
  *len_out = len;
  return (int) type;
}

// Finds a fixed32 field (wire type 5) or a varint field (wire type 0) by number in a flat message.
bool find_field(const uint8_t *p, uint32_t len, uint32_t field, uint32_t *val) {
  uint32_t i = 0;
  while (i < len) {
    uint32_t tag = 0;
    for (int s = 0; i < len; s += 7) {
      uint8_t b = p[i++];
      tag |= (uint32_t) (b & 0x7F) << s;
      if (!(b & 0x80))
        break;
    }
    uint32_t f = tag >> 3, wt = tag & 7;
    if (wt == 0) {
      uint32_t v = 0;
      for (int s = 0; i < len; s += 7) {
        uint8_t b = p[i++];
        v |= (uint32_t) (b & 0x7F) << s;
        if (!(b & 0x80))
          break;
      }
      if (f == field) {
        *val = v;
        return true;
      }
    } else if (wt == 5) {
      if (f == field && i + 4 <= len) {
        *val = p[i] | (p[i + 1] << 8) | (p[i + 2] << 16) | ((uint32_t) p[i + 3] << 24);
        return true;
      }
      i += 4;
    } else if (wt == 1) {
      i += 8;
    } else if (wt == 2) {
      uint32_t l = 0;
      for (int s = 0; i < len; s += 7) {
        uint8_t b = p[i++];
        l |= (uint32_t) (b & 0x7F) << s;
        if (!(b & 0x80))
          break;
      }
      i += l;
    } else {
      return false;
    }
  }
  return false;
}

uint32_t now_ms() { return (uint32_t) xTaskGetTickCount() * portTICK_PERIOD_MS; }

void run_once(int round);

void selftest_task(void *) {
  for (int round = 1;; round++) {
    run_once(round);
    vTaskDelay(pdMS_TO_TICKS(300000));  // again in 5 minutes: a long-run soak
  }
}

void run_once(int round) {
  // Wait for an address (the API server only needs lwIP up, but this keeps the order of a real client) and 10 s more.
  for (;;) {
    struct netif *n = static_cast<struct netif *>(FRL_wifi_netif());
    if (n && netif_is_up(n) && !ip4_addr_isany_val(*netif_ip4_addr(n)))
      break;
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  if (round == 1)
    vTaskDelay(pdMS_TO_TICKS(10000));
  FRL_log("[selftest] round %d start: ESPHome API client on 127.0.0.1:6053\r\n", round);
  int fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in a;
  std::memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_port = lwip_htons(6053);
  a.sin_addr.s_addr = lwip_htonl(0x7F000001);
  struct timeval tv = {3, 0};
  lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  int one = 1;
  lwip_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  uint32_t t0 = now_ms();
  if (fd < 0 || lwip_connect(fd, reinterpret_cast<struct sockaddr *>(&a), sizeof(a)) != 0) {
    FRL_log("[selftest] FAIL connect (fd %d)\r\n", fd);
    if (fd >= 0)
      lwip_close(fd);
    return;
  }
  static uint8_t buf[512];
  uint32_t len = 0;
  // HelloRequest: client_info (1) "frl-selftest", api_version_major (2) 1, api_version_minor (3) 10
  const char ci[] = "frl-selftest";
  uint8_t hello[32];
  int h = 0;
  hello[h++] = 0x0A;
  hello[h++] = sizeof(ci) - 1;
  std::memcpy(hello + h, ci, sizeof(ci) - 1);
  h += sizeof(ci) - 1;
  hello[h++] = 0x10;
  hello[h++] = 1;
  hello[h++] = 0x18;
  hello[h++] = 10;
  send_frame(fd, 1, hello, h);
  int t = read_frame(fd, buf, sizeof(buf), &len);
  FRL_log("[selftest] hello: got type %d (want 2) after %u ms\r\n", t, (unsigned) (now_ms() - t0));
  if (t != 2) {
    lwip_close(fd);
    return;
  }
  send_frame(fd, 9, nullptr, 0);
  t = read_frame(fd, buf, sizeof(buf), &len);
  FRL_log("[selftest] device info: type %d (want 10), %u bytes\r\n", t, (unsigned) len);
  // ListEntities: collect the switch key, count entities until ListEntitiesDone (19)
  send_frame(fd, 11, nullptr, 0);
  uint32_t switch_key = 0;
  int entities = 0;
  for (;;) {
    t = read_frame(fd, buf, sizeof(buf), &len);
    if (t < 0 || t == 19)
      break;
    entities++;
    if (t == 17)
      find_field(buf, std::min<uint32_t>(len, sizeof(buf)), 2, &switch_key);
  }
  FRL_log("[selftest] list entities: %d entities, done %s, switch key 0x%08x\r\n", entities, t == 19 ? "yes" : "no",
          (unsigned) switch_key);
  send_frame(fd, 20, nullptr, 0);
  // Drain the initial states for a second.
  struct timeval quick = {1, 0};
  lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &quick, sizeof(quick));
  int initial = 0;
  while (read_frame(fd, buf, sizeof(buf), &len) >= 0)
    initial++;
  lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  FRL_log("[selftest] subscribe states: %d initial state messages\r\n", initial);
  // 20 switch commands, alternating; each timed from send to the matching SwitchStateResponse, LED pin read back.
  uint32_t lat[20];
  int ok = 0, pin_ok = 0;
  for (int i = 0; i < 20 && switch_key; i++) {
    bool want = (i % 2) == 0;
    uint8_t cmd[8] = {0x0D, (uint8_t) switch_key, (uint8_t) (switch_key >> 8), (uint8_t) (switch_key >> 16),
                      (uint8_t) (switch_key >> 24), 0x10, (uint8_t) want};
    uint32_t s = now_ms();
    send_frame(fd, 33, cmd, 7);
    bool got = false;
    while (!got) {
      t = read_frame(fd, buf, sizeof(buf), &len);
      if (t < 0)
        break;
      uint32_t key = 0, state = 0;
      if (t == 26 && find_field(buf, len, 1, &key) && key == switch_key) {
        find_field(buf, len, 2, &state);
        got = (state != 0) == want;
      }
    }
    uint32_t e = now_ms() - s;
    if (got) {
      lat[ok++] = e;
      if (led_latch(34) == want)
        pin_ok++;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  std::sort(lat, lat + ok);
  FRL_log("[selftest] round %d switch: %d of 20 confirmed by SwitchStateResponse, LED latch matched %d, latency ms min %u median %u max %u, free heap %u min %u\r\n",
          round, ok, pin_ok, ok ? (unsigned) lat[0] : 0, ok ? (unsigned) lat[ok / 2] : 0, ok ? (unsigned) lat[ok - 1] : 0, (unsigned) xPortGetFreeHeapSize(), (unsigned) xPortGetMinimumEverFreeHeapSize());
  send_frame(fd, 5, nullptr, 0);  // DisconnectRequest
  read_frame(fd, buf, sizeof(buf), &len);
  lwip_close(fd);
  FRL_log("[selftest] round %d done in %u ms\r\n", round, (unsigned) (now_ms() - t0));
}

}  // namespace

extern "C" void frl_api_selftest_start(void) { xTaskCreate(selftest_task, "apitest", 4096 / sizeof(StackType_t), nullptr, 2, nullptr); }
#endif
