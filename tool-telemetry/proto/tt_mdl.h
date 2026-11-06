/* Models installed over the air: the package a hub writes to a tag, and the tree walk that uses it.
 * A package is one decision tree: kind (0 uses current, 1 speed only), version, node count, CRC, then 5 bytes a node
 * (feature, threshold, left, right; feature < 0 is a leaf whose threshold is the class). The hub sends it as GATT writes:
 *   BEGIN  01 kind ver(2) n crc(2)          the slot for that kind stops being used until COMMIT
 *   DATA   02 offset(2) bytes...            up to 17 bytes a write (20-byte writes, the default ATT MTU)
 *   COMMIT 03 mac(16)                       length, CRC and every child index checked, then the MAC (tt_mdl_finish)
 * The MAC is HMAC-SHA256 over kind, version, count and the nodes, cut to 16 bytes, with a fleet key the tag is built
 * with and the home base signs with. A package without the right MAC never runs, and one in flash is checked again
 * at every boot. The CRC catches radio damage; the MAC catches anyone else writing to the tag.
 * No heap. The same file builds for the tag and for the host test (proto/test_mdl.py). */
#ifndef TT_MDL_H
#define TT_MDL_H

#include <stddef.h>
#include <stdint.h>
#include "tt_proto.h"

#define TT_MDL_MAX_NODES  40
#define TT_MDL_NODE_BYTES 5
#define TT_MDL_CHUNK      17

typedef struct {
    uint16_t ver;
    uint8_t  kind, n;
    uint16_t crc;
    uint8_t  mac[16];
    uint8_t  node[TT_MDL_MAX_NODES * TT_MDL_NODE_BYTES];
} tt_mdl_t;

enum { TT_MDL_OK = 0, TT_MDL_ERR_OP = 1, TT_MDL_ERR_LEN = 2, TT_MDL_ERR_STATE = 3, TT_MDL_ERR_CRC = 4, TT_MDL_ERR_TREE = 5,
       TT_MDL_ERR_AUTH = 6, TT_MDL_PENDING = 7 };
#define TT_MDL_KEY_LEN 16

typedef struct {
    tt_mdl_t slot[2];          /* [0] uses current, [1] speed only */
    uint8_t  live[2];          /* 1 once committed (or loaded from flash and checked) */
    uint8_t  loading;          /* kind + 1 while a BEGIN is open, else 0 */
    uint8_t  result;           /* TT_MDL_* of the last write */
    uint16_t got;              /* bytes received since BEGIN */
    uint16_t gen[2];           /* bumped by every BEGIN: a MAC check counts only if the slot's gen did not move */
} tt_mdl_store_t;

uint16_t tt_mdl_crc16(const uint8_t *p, size_t n, uint16_t crc);
/* CRC over kind, version and node count, then the nodes. */
uint16_t tt_mdl_crc(const tt_mdl_t *m);
/* 1 when the slot's tree is whole: every child index in range and every path ends in a leaf. */
int tt_mdl_valid(const tt_mdl_t *m);
/* One GATT write. Returns TT_MDL_OK, 1 when a COMMIT passed every check but the MAC (call tt_mdl_finish from a task
   with stack to spare: SHA-256 is not run in the GATT callback), or an error code + 10. */
int tt_mdl_write(tt_mdl_store_t *s, const uint8_t *w, uint16_t len);
/* Checks slot k's MAC; makes it live and returns TT_MDL_OK, or leaves it off and returns TT_MDL_ERR_AUTH. */
int tt_mdl_finish(tt_mdl_store_t *s, uint8_t k, const uint8_t key[TT_MDL_KEY_LEN]);
/* 1 when a slot read back from flash is whole and signed with this key. */
int tt_mdl_trusted(const tt_mdl_t *m, uint8_t k, const uint8_t key[TT_MDL_KEY_LEN]);
/* HMAC-SHA256 of the package, first 16 bytes. */
void tt_mdl_mac(const tt_mdl_t *m, const uint8_t key[TT_MDL_KEY_LEN], uint8_t out[16]);
/* HMAC-SHA256 of a then b under the fleet key, first 16 bytes (signed commands use it with their own prefix). */
void tt_hmac16(const uint8_t key[TT_MDL_KEY_LEN], const uint8_t *a, size_t na, const uint8_t *b, size_t nb, uint8_t out[16]);

/* Signed commands to the tool (stage two), 18 bytes written to characteristic 0xFAB3:
 *   cmd(1: 1 stop, 2 lock, 3 unlock, 4 run, 5 lease) arg(1: run duty, percent 0 to 40; lease minutes 0 to 255,
 *   0 ends the lease rule) counter(4, must grow) mac(12)
 * A lease keeps the tool usable for that many minutes; with no newer lease by then the tag locks the tool itself.
 * mac = HMAC-SHA256(key, C0 || this tag's address (6, as advertised, least significant byte first) || cmd duty counter)
 * cut to 12 bytes: bound to one tag, never valid twice. */
#define TT_CMD_LEN 18
enum { TT_CMD_OK = 0, TT_CMD_ERR_MAC = 1, TT_CMD_ERR_OLD = 2, TT_CMD_ERR_BAD = 3, TT_CMD_PENDING = 7 };
/* Checks one command; returns TT_CMD_OK and fills cmd, duty and counter, or an error. */
int tt_cmd_check(const uint8_t key[TT_MDL_KEY_LEN], const uint8_t addr[6], const uint8_t w[TT_CMD_LEN], uint32_t last_counter,
                 uint8_t *cmd, uint8_t *duty, uint32_t *counter);

/* The class of a pull from the live tree for its inputs, or -1 when that slot is not live (use the built-in model). */
int tt_mdl_classify(const tt_mdl_store_t *s, const tt_pull_t *p);
/* Version the tag reports: the current-using slot's when live, else 0 (the built-in model). */
uint16_t tt_mdl_version(const tt_mdl_store_t *s);
/* 8-byte status a client reads: ver[0](2) ver[1](2) live bits, result, got(2). */
void tt_mdl_status(const tt_mdl_store_t *s, uint8_t out[8]);

#endif
