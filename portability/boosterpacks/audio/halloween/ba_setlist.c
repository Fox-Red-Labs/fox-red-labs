/* The Halloween set list, in play order. Songs: the .tune files in songs/, converted with tools/tune2h.py. */
#include "ba_show.h"
#include "mountain_king.h"
#include "toccata.h"
#include "danse_macabre.h"
#include "pumpkin_patrol.h"

#define N(a) ((uint16_t)(sizeof(a) / sizeof((a)[0])))

const ba_show_song_t ba_setlist[] = {
    { "In the Hall of the Mountain King (Grieg)", mountain_king_rows, N(mountain_king_rows), MOUNTAIN_KING_TEMPO,
      mountain_king_marks, MOUNTAIN_KING_NMARKS, MOUNTAIN_KING_VOL },
    { "Pumpkin Patrol (original)", pumpkin_patrol_rows, N(pumpkin_patrol_rows), PUMPKIN_PATROL_TEMPO, 0, 0, PUMPKIN_PATROL_VOL },
    { "Toccata and Fugue in D minor, opening (Bach)", toccata_rows, N(toccata_rows), TOCCATA_TEMPO,
      toccata_marks, TOCCATA_NMARKS, TOCCATA_VOL },
    { "Danse macabre (Saint-Saens)", danse_macabre_rows, N(danse_macabre_rows), DANSE_MACABRE_TEMPO, 0, 0, DANSE_MACABRE_VOL },
};
const uint8_t ba_setlist_n = (uint8_t)(sizeof ba_setlist / sizeof ba_setlist[0]);
