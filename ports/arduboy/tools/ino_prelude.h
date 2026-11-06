/* Force-included before an Arduino sketch so it sees the Arduboy2 layer. */
#include <Arduino.h>
#include <Arduboy2.h>
#ifdef FRL_RENAME_MODE_T
/* A game that defines its own type called mode_t: the system header has already declared one,
 * so give the game's name a different spelling from here on. */
#include <sys/types.h>
#define mode_t frl_game_mode_t
#endif
