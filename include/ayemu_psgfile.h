#ifndef _AYEMU_psgfile_h
#define _AYEMU_psgfile_h

#include <stdio.h>
#include "ayemu_8912.h"

BEGIN_C_DECLS

/** internal state for PSG file playback
 * \internal
 */
typedef struct
{
	FILE *fp;			/**< open .psg file pointer */
	unsigned char frames_to_skip;	/**< number of interrupt frames to wait before processing the next batch of registers */
} ayemu_psg_t;

/**
 * \defgroup psgfile Functions for playing back psg files
 */
/*@{*/


/** Open psg file for playback
		\arg \c psg - pointer to PSG_PLAYER_STATE structure
		\arg \c filename - filename of psg file file
		\return Return true if success, else false
*/
EXTERN int ayemu_psg_open(ayemu_psg_t *psg, const char *filename);
	
/** Send one interrupt frame of data to the AY.
 * \return 1 if frame sent, 0 if data is finished
 */
EXTERN int ayemu_psg_play_frame( ayemu_psg_t *psg, ayemu_ay_t *ay);
	
/** Free all of allocated resources for this file.
 * Call this function after finishing with the psg file
 */
EXTERN void ayemu_psg_close( ayemu_psg_t *psg );

/*@}*/

END_C_DECLS

#endif
