/* RAM-backed EEPROM with the AVR EEPROM library calls games use. The image starts erased (0xFF).
 * Nothing is saved across power cycles yet (high scores reset). */
#ifndef FRL_EEPROM_H
#define FRL_EEPROM_H
#include <stdint.h>
#include <string.h>
#ifndef FRL_EEPROM_SIZE
#define FRL_EEPROM_SIZE 1024
#endif
extern uint8_t frl_eeprom_image[FRL_EEPROM_SIZE];
struct EEPROMClass {
    uint16_t length() const { return FRL_EEPROM_SIZE; }
    uint8_t read(int a) const { return (a >= 0 && a < FRL_EEPROM_SIZE) ? frl_eeprom_image[a] : 0xFF; }
    void write(int a, uint8_t v) { if (a >= 0 && a < FRL_EEPROM_SIZE) frl_eeprom_image[a] = v; }
    void update(int a, uint8_t v) { write(a, v); }
    template <typename T> T &get(int a, T &t) const {
        uint8_t *p = (uint8_t *)&t; for (unsigned i = 0; i < sizeof(T); i++) p[i] = read(a + (int)i); return t; }
    template <typename T> const T &put(int a, const T &t) {
        const uint8_t *p = (const uint8_t *)&t; for (unsigned i = 0; i < sizeof(T); i++) write(a + (int)i, p[i]); return t; }
};
extern EEPROMClass EEPROM;
#endif
