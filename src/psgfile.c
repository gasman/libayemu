#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

#include "ayemu.h"

/** Open psg file for playback
 *
 *  Return value: true if success, else false
 */
 
int ayemu_psg_open( ayemu_psg_t *psg, const char *filename )
{
	const char *expected_header = "PSG\x1a";
	char header[4];

	if ((psg->fp = fopen (filename, "rb")) == NULL) {
		fprintf( stderr, "ayemu_player_open_psg: Cannot open file %s: %s\n",
			filename, strerror( errno ) );
		return 0;
	}
	/* read psg header */
	fread( header, 4, 1, psg->fp );
	if (strncmp( header, expected_header, 4 ) != 0) {
		fprintf( stderr, "ayemu_player_open_psg: %s is not a valid PSG file\n",
			filename );
		fclose( psg->fp );
		return 0;
	}
	
	psg->frames_to_skip = 0;
	return 1;
}

/** Send one interrupt frame of data to the AY.
 * \return 1 if frame sent, 0 if data is finished
 */
int ayemu_psg_play_frame( ayemu_psg_t *psg, ayemu_ay_t *ay)
{
	int cmd;

	if (psg->frames_to_skip) {
		psg->frames_to_skip--;
		return 1;
	}

	while (!feof( psg->fp ) && (cmd = fgetc( psg->fp )) < 16) {
		ayemu_set_reg( ay, cmd, fgetc( psg->fp ) );
	}

	if (feof( psg->fp ) || cmd == 0xfd) {
		/* end of music */
		return 0;
	} else if (cmd == 0xfe) {
		/* multiple end of interrupt (pause for set number of frames) */
		psg->frames_to_skip = fgetc( psg->fp );
		return 1;
	} else if (cmd == 0xff) {
		/* end of interrupt */
		return 1;
	} else {
		/* unrecognised command */
		fprintf( stderr, "psg_play_frame: warning: unrecognised command %d\n", cmd );
		return 1;
	}
}

/** Free all of allocated resources for this file.
 *
 */
void ayemu_psg_close(ayemu_psg_t *psg)
{
	if (psg->fp) {
		fclose(psg->fp);
		psg->fp = NULL;
	}
}
