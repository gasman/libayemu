#ifndef _AYEMU_stcfile_h
#define _AYEMU_stcfile_h

#include <stdio.h>
#include "ayemu_8912.h"

BEGIN_C_DECLS

/** internal state for STC file playback
 * \internal
 */
typedef enum _envelope_state {
	ENVELOPE_OFF, ENVELOPE_TRIGGERED, ENVELOPE_ON
} ayemu_stc_envelope_state_t;

typedef struct _channel_data {
	int sample_repeat_counter;
	unsigned char *current_ornament;
	unsigned char *current_sample;
	int sample_position;
	int note_value;
	ayemu_stc_envelope_state_t envelope_state;
	unsigned char *events;
	int row_skip;
	int row_counter;
} ayemu_stc_channel_t;

typedef struct
{
	unsigned char *data;
	int delay;
	int delay_counter;
	int song_length;
	unsigned char *positions;
	int position;
	int position_height;
	unsigned char *ornaments[16];
	unsigned char *patterns[32];
	unsigned char *samples[16];
	ayemu_stc_channel_t channels[3];
	unsigned char last_registers[14];
	int has_looped;
} ayemu_stc_t;

/**
 * \defgroup stcfile Functions for playing back stc files
 */
/*@{*/


/** Open stc file for playback
		\arg \c stc - pointer to ayemu_stc_t structure
		\arg \c filename - filename of stc file
		\return Return true if success, else false
*/
EXTERN int ayemu_stc_open(ayemu_stc_t *stc, const char *filename);
	
/** Return playback to the start of the stc file.
 */
EXTERN void ayemu_stc_reset( ayemu_stc_t *stc );

/** Send one interrupt frame of data to the AY.
 * \return 1 if frame sent, 0 if data is finished
 */
/* EXTERN int ayemu_stc_play_frame( ayemu_stc_t *stc, ayemu_ay_t *ay); */
	
/** Fetch the next interrupt frame of data as an array of AY registers.
 * \return 1 if frame sent, 0 if data is finished
 */
EXTERN int ayemu_stc_get_frame(ayemu_stc_t *stc, unsigned char *regs);

/** Free all of allocated resources for this file.
 * Call this function after finishing with the stc file
 */
EXTERN void ayemu_stc_close( ayemu_stc_t *stc );

/*@}*/

END_C_DECLS

#endif
