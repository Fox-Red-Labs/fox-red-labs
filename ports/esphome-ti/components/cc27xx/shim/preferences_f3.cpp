// SPDX-License-Identifier: GPL-3.0-only
// TI F3 shim: ESPHome saved settings (esphome::global_preferences) on TI's NVS driver.
//
// Layout of the NVS region (two halves, so a power cut while compacting never loses the settings):
//   half header (16 B): "ESPH" magic, sequence number, then records, each 16-byte aligned.
//   record header (16 B): u8 0xA5, u8 checksum, u16 payload length, u32 key, 8 bytes of 0xFF, then the payload padded
//   to 16 B. The checksum is the XOR of every payload, length and key byte with 0x5A.
// A save appends a record (the newest record of a key wins). When the half is full, the live records are written into
// the other half, its header is written last, and the old half is erased. The half header with the higher sequence
// number is the active one.
// The region is declared in SysConfig as CONFIG_FRL_NVS (periph.py), at the top of the application flash.
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include <esphome/components/zephyr/preferences.h>

#include <cinttypes>
#include <cstring>
#include <vector>

extern "C" {
#include <ti/drivers/NVS.h>
#include "ti_drivers_config.h"
}

namespace esphome::zephyr {

static const char *const TAG = "preferences";

static constexpr uint32_t ALIGN = 16;
static constexpr uint32_t HALF_MAGIC = 0x48505345;  // "ESPH"
static constexpr uint8_t REC_MAGIC = 0xA5;

static NVS_Handle nvs = nullptr;
static uint32_t half_size = 0;
static uint32_t active_half = 0;  // 0 or 1
static uint32_t active_seq = 0;
static uint32_t write_off = 0;  // next free offset inside the region
static bool ready = false;

struct RecHeader {
  uint8_t magic;
  uint8_t chk;
  uint16_t len;
  uint32_t key;
  uint8_t pad[8];
};
static_assert(sizeof(RecHeader) == 16, "record header must be 16 bytes");

static uint32_t up(uint32_t n) { return (n + ALIGN - 1) & ~(ALIGN - 1); }

static uint8_t checksum(uint16_t len, uint32_t key, const uint8_t *p) {
  uint8_t c = 0x5A;
  const uint8_t *l = reinterpret_cast<const uint8_t *>(&len);
  const uint8_t *k = reinterpret_cast<const uint8_t *>(&key);
  for (int i = 0; i < 2; i++) c ^= l[i];
  for (int i = 0; i < 4; i++) c ^= k[i];
  for (uint16_t i = 0; i < len; i++) c ^= p[i];
  return c;
}

static bool nvs_read(uint32_t off, void *buf, uint32_t n) { return NVS_read(nvs, off, buf, n) == NVS_STATUS_SUCCESS; }
static bool nvs_write(uint32_t off, const void *buf, uint32_t n) {
  return NVS_write(nvs, off, const_cast<void *>(buf), n, 0) == NVS_STATUS_SUCCESS;
}

static bool write_record(uint32_t off, uint32_t key, const uint8_t *payload, uint16_t len) {
  RecHeader h;
  memset(&h, 0xFF, sizeof(h));
  h.magic = REC_MAGIC;
  h.len = len;
  h.key = key;
  h.chk = checksum(len, key, payload);
  std::vector<uint8_t> padded(up(len ? len : 1), 0xFF);
  if (len) memcpy(padded.data(), payload, len);
  // payload first, header last: a record without a complete header does not exist
  return nvs_write(off + sizeof(h), padded.data(), padded.size()) && nvs_write(off, &h, sizeof(h));
}

struct Live {
  uint32_t key;
  std::vector<uint8_t> data;
};

// Walk one half; fill `live` with the newest record of each key; return the offset of the first free byte.
static uint32_t scan_half(uint32_t half, std::vector<Live> *live, bool *clean) {
  uint32_t base = half * half_size;
  uint32_t off = base + ALIGN;
  *clean = true;
  while (off + sizeof(RecHeader) <= base + half_size) {
    RecHeader h;
    if (!nvs_read(off, &h, sizeof(h))) break;
    if (h.magic == 0xFF) break;  // erased: end of log
    uint32_t total = sizeof(RecHeader) + up(h.len ? h.len : 1);
    if (h.magic != REC_MAGIC || off + total > base + half_size) {
      *clean = false;
      break;
    }
    std::vector<uint8_t> d(h.len);
    if (h.len && !nvs_read(off + sizeof(RecHeader), d.data(), h.len)) break;
    if (checksum(h.len, h.key, d.data()) != h.chk) {
      *clean = false;
      break;
    }
    bool found = false;
    for (auto &l : *live)
      if (l.key == h.key) {
        l.data = std::move(d);
        found = true;
        break;
      }
    if (!found) live->push_back({h.key, std::move(d)});
    off += total;
  }
  return off;
}

// Write `live` into the other half and make it the active one.
static bool compact(const std::vector<Live> &live) {
  uint32_t other = active_half ^ 1;
  uint32_t base = other * half_size;
  if (NVS_erase(nvs, base, half_size) != NVS_STATUS_SUCCESS) return false;
  uint32_t off = base + ALIGN;
  for (const auto &l : live) {
    if (l.data.empty()) continue;  // deleted
    if (off + sizeof(RecHeader) + up(l.data.size()) > base + half_size) return false;
    if (!write_record(off, l.key, l.data.data(), l.data.size())) return false;
    off += sizeof(RecHeader) + up(l.data.size());
  }
  uint8_t hdr[ALIGN];
  memset(hdr, 0xFF, sizeof(hdr));
  uint32_t magic = HALF_MAGIC, seq = active_seq + 1;
  memcpy(hdr, &magic, 4);
  memcpy(hdr + 4, &seq, 4);
  if (!nvs_write(base, hdr, sizeof(hdr))) return false;
  NVS_erase(nvs, active_half * half_size, half_size);
  active_half = other;
  active_seq = seq;
  write_off = off;
  return true;
}

static bool open_store(std::vector<Live> *live) {
  static bool tried = false;
  if (tried) return ready;
  tried = true;
  NVS_init();
  NVS_Params p;
  NVS_Params_init(&p);
  nvs = NVS_open(CONFIG_FRL_NVS, &p);
  if (nvs == nullptr) return false;
  NVS_Attrs attrs;
  NVS_getAttrs(nvs, &attrs);
  half_size = attrs.regionSize / 2;
  uint32_t seq[2] = {0, 0};
  bool valid[2] = {false, false};
  for (uint32_t h = 0; h < 2; h++) {
    uint8_t hdr[8];
    if (nvs_read(h * half_size, hdr, 8)) {
      uint32_t m;
      memcpy(&m, hdr, 4);
      if (m == HALF_MAGIC) {
        memcpy(&seq[h], hdr + 4, 4);
        valid[h] = true;
      }
    }
  }
  if (!valid[0] && !valid[1]) {
    // a blank region: write the first half header (compact() writes into the half that is not active)
    active_half = 1;
    active_seq = 0;
    live->clear();
    ready = compact(*live);
    return ready;
  }
  active_half = (valid[0] && (!valid[1] || seq[0] >= seq[1])) ? 0 : 1;
  active_seq = seq[active_half];
  bool clean = true;
  write_off = scan_half(active_half, live, &clean);
  ready = true;
  if (!clean) compact(*live);  // a damaged tail: rewrite what is good
  return ready;
}

static ZephyrPreferences s_preferences;

ZephyrPreferences *get_preferences() { return &s_preferences; }

bool ZephyrPreferenceBackend::save(const uint8_t *src, size_t len) {
  if (this->data.size() == len && memcmp(this->data.data(), src, len) == 0) return true;  // unchanged
  this->data.assign(src, src + len);
  this->dirty = true;
  return true;
}

bool ZephyrPreferenceBackend::load(uint8_t *dest, size_t len) {
  if (len != this->data.size()) {
    if (!this->data.empty())
      ESP_LOGE(TAG, "size of setting %" PRIx32 " changed, from: %zu, to: %zu", this->type_, this->data.size(), len);
    return false;
  }
  memcpy(dest, this->data.data(), len);
  return true;
}

void ZephyrPreferences::open() {
  std::vector<Live> live;
  if (!open_store(&live)) {
    ESP_LOGE(TAG, "NVS region not available, settings will not survive a reset");
    return;
  }
  for (auto &l : live)
    if (!l.data.empty()) this->backends_.push_back(new ZephyrPreferenceBackend(l.key, std::move(l.data)));  // NOLINT
  ESP_LOGD(TAG, "Loaded %zu settings.", this->backends_.size());
}

ZephyrPreferenceBackend *ZephyrPreferences::find_backend_(uint32_t type) {
  for (auto *b : this->backends_)
    if (b->get_type() == type) return b;
  return nullptr;
}

ESPPreferenceObject ZephyrPreferences::make_preference(size_t length, uint32_t type) {
  (void) length;
  if (auto *b = this->find_backend_(type)) return ESPPreferenceObject(b);
  auto *pref = new ZephyrPreferenceBackend(type);  // NOLINT
  this->backends_.push_back(pref);
  return ESPPreferenceObject(pref);
}

bool ZephyrPreferences::load_from_key(uint32_t type, uint8_t *data, size_t len) {
  auto *b = this->find_backend_(type);
  return b != nullptr && b->load(data, len);
}

bool ZephyrPreferences::sync() {
  if (!ready) return false;
  bool ok = true, any = false;
  for (auto *b : this->backends_) {
    if (!b->dirty) continue;
    any = true;
    uint32_t need = sizeof(RecHeader) + up(b->data.size() ? b->data.size() : 1);
    if (write_off + need > (active_half + 1) * half_size) {
      // full: write every backend's data into the other half
      std::vector<Live> merged;
      for (auto *p : this->backends_) merged.push_back({p->get_type(), p->data});
      ok = compact(merged);
      for (auto *p : this->backends_) p->dirty = false;
      ESP_LOGD(TAG, "Compacted settings: %s", ok ? "ok" : "failed");
      return ok;
    }
    if (!write_record(write_off, b->get_type(), b->data.data(), b->data.size())) {
      ok = false;
      continue;
    }
    write_off += need;
    b->dirty = false;
  }
  if (any) {
    ESP_LOGD(TAG, "Saved settings: %s", ok ? "ok" : "failed");
  }
  return ok;
}

bool ZephyrPreferences::reset() {
  for (auto *b : this->backends_) {
    b->data.clear();
    b->dirty = false;
  }
  if (!ready) return false;
  std::vector<Live> none;
  return compact(none);
}

void setup_preferences() {
  global_preferences = &s_preferences;
  s_preferences.open();
}

}  // namespace esphome::zephyr

namespace esphome {
ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace esphome
