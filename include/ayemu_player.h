#ifndef _AYEMU_player_h
#define _AYEMU_player_h

#include <stdio.h>
#include "ayemu_8912.h"

BEGIN_C_DECLS

/**
 * \defgroup player High-level AY playback routines
 */
/*@{*/

/** Output sound format */
typedef struct
{
	int freq;			/**< sound freq */
	int channels;			/**< channels (1-mono, 2-stereo) */
	int bpc;			/**< bits (8 or 16) */
}
ayemu_player_sndfmt_t;

/** structure for AY playback handler
 * \internal
 * Stores current state of AY playback
 */
typedef struct
{
	ayemu_ay_t ay;		/**< AY chip data */
	int (*play_frame)(void *, ayemu_ay_t *);	/**< Function to write the required data to the AY chip
		to play one interrupt frame of audio */
	int (*close)(void *);	/**< Function to free any resources allocated by this player */
	void *userdata;
	int ticks_per_interrupt;
	int ticks_to_next_interrupt;
} ayemu_player_t;

/** Open vtx file and initialise player
    \arg \c filename - filename of vtx file
    \arg \c format - pointer to ayemu_player_sndfmt_t structure specifying required audio output format
    \arg \c player - pointer to ayemu_player_t structure to be initialised
    \return Return true if success, else false
*/
EXTERN int ayemu_player_open_vtx( const char *filename, ayemu_player_sndfmt_t *format, ayemu_player_t *player );

/** Open psg file and initialise player
    \arg \c filename - filename of psg file
    \arg \c format - pointer to ayemu_player_sndfmt_t structure specifying required audio output format
    \arg \c player - pointer to ayemu_player_t structure to be initialised
    \return Return true if success, else false
*/
EXTERN int ayemu_player_open_psg( const char *filename, ayemu_player_sndfmt_t *format, ayemu_player_t *player );
  
/** Fill a buffer with the required number of samples of AY output
    \arg \c player - pointer to ayemu_player_t structure
    \arg \c filename - filename of vtx file
    \arg \c frames - number of sample frames to output
    \return Return number of sample frames written; 0 indicates no more audio data
*/
EXTERN size_t ayemu_player_fill_buffer( ayemu_player_t *player, void *ptr, size_t frames);

/** Free all allocated resources for this player
    \arg \c player - pointer to ayemu_player_t structure
 */
EXTERN void ayemu_player_close( ayemu_player_t *player );

/*@}*/

END_C_DECLS

#endif
