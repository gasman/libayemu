/* High-level AY playback handlers */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ayemu.h"

#define AY_FREQ 1773400
#define INTERRUPT_FREQ 50
#define TICK_MULTIPLIER 8

/* VTX structures / callbacks */
typedef struct
{
	ayemu_vtx_t vtx;
	size_t frame;
}
VTX_PLAYER_STATE;

int vtx_play_frame( void *userdata, ayemu_ay_t *ay)
{
	unsigned char ay_regs[14];

	VTX_PLAYER_STATE *vtx_state = (VTX_PLAYER_STATE *)userdata;

	if (vtx_state->frame < vtx_state->vtx.frames) {
		ayemu_vtx_getframe( &vtx_state->vtx, vtx_state->frame, ay_regs );
		vtx_state->frame++;
		ayemu_set_regs( ay, ay_regs );
		return 1;
	} else {
		return 0;
	}
}

int vtx_close( void *userdata )
{
	VTX_PLAYER_STATE *vtx_state = (VTX_PLAYER_STATE *)userdata;

	ayemu_vtx_free( &vtx_state->vtx );
	free( vtx_state );
	return 1;
}

/* PSG structures / callbacks */
typedef struct
{
	FILE *fp;			/**< open .psg file pointer */
	unsigned char frames_to_skip;	/**< number of interrupt frames to wait before processing the next batch of registers */
} PSG_PLAYER_STATE;

int psg_play_frame( void *userdata, ayemu_ay_t *ay)
{
	PSG_PLAYER_STATE *psg = (PSG_PLAYER_STATE *)userdata;
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

int psg_close( void *userdata )
{
	PSG_PLAYER_STATE *psg = (PSG_PLAYER_STATE *)userdata;
	fclose( psg->fp );
	free( psg );
	return 1;
}

int ayemu_player_open_vtx( const char *filename, ayemu_player_sndfmt_t *format, ayemu_player_t *player ) {
	VTX_PLAYER_STATE *vtx_state;
	
	ayemu_init( &player->ay );
	ayemu_set_sound_format( &player->ay, format->freq, format->channels, format->bpc );
	ayemu_set_chip_type( &player->ay, AYEMU_AY, NULL );
	ayemu_set_chip_freq( &player->ay, AY_FREQ );
	ayemu_set_stereo( &player->ay, AYEMU_ACB, NULL );
	
	vtx_state = malloc( sizeof(VTX_PLAYER_STATE) );
	vtx_state->frame = 0;
	ayemu_vtx_open( &vtx_state->vtx, filename );
	ayemu_vtx_load_data( &vtx_state->vtx );
	
	player->ticks_per_interrupt = (format->freq << TICK_MULTIPLIER) / INTERRUPT_FREQ;
	player->ticks_to_next_interrupt = 0;
	player->userdata = vtx_state;
	player->play_frame = &vtx_play_frame;
	player->close = &vtx_close;
	return 1;
}

int ayemu_player_open_psg( const char *filename, ayemu_player_sndfmt_t *format, ayemu_player_t *player ) {
	const char *expected_header = "PSG\x1a";
	char header[4];
	
	PSG_PLAYER_STATE *psg;
	
	psg = malloc( sizeof(PSG_PLAYER_STATE) );
	if ((psg->fp = fopen (filename, "rb")) == NULL) {
		fprintf( stderr, "ayemu_player_open_psg: Cannot open file %s: %s\n",
			filename, strerror( errno ) );
		free( psg );
		return 0;
	}
	/* read psg header */
	fread( header, 4, 1, psg->fp );
	if (strncmp( header, expected_header, 4 ) != 0) {
		fprintf( stderr, "ayemu_player_open_psg: %s is not a valid PSG file\n",
			filename );
		fclose( psg->fp );
		free( psg );
		return 0;
	}
	
	psg->frames_to_skip = 0;

	ayemu_init( &player->ay );
	ayemu_set_sound_format( &player->ay, format->freq, format->channels, format->bpc );
	ayemu_set_chip_type( &player->ay, AYEMU_AY, NULL );
	ayemu_set_chip_freq( &player->ay, AY_FREQ );
	ayemu_set_stereo( &player->ay, AYEMU_ACB, NULL );	
	
	player->ticks_per_interrupt = (format->freq << TICK_MULTIPLIER) / INTERRUPT_FREQ;
	player->ticks_to_next_interrupt = 0;
	player->userdata = psg;
	player->play_frame = &psg_play_frame;
	player->close = &psg_close;
	return 1;
}

size_t ayemu_player_fill_buffer( ayemu_player_t *player, void *ptr, size_t frames)
{
	int chunk_size;

	size_t frames_written = 0;	
	int playing = 1;
	int ticks_to_buffer_end = frames << TICK_MULTIPLIER;
	
	while (playing && player->ticks_to_next_interrupt < ticks_to_buffer_end) {
		/* generate sound up to next interrupt */
		chunk_size = player->ticks_to_next_interrupt >> TICK_MULTIPLIER;
		ptr = ayemu_gen_sound( &player->ay, ptr, chunk_size );
		frames_written += chunk_size;
		player->ticks_to_next_interrupt -= chunk_size << TICK_MULTIPLIER;
		ticks_to_buffer_end -= chunk_size << TICK_MULTIPLIER;

		playing = player->play_frame( (void *)player->userdata, &player->ay );
		if (playing) {
			player->ticks_to_next_interrupt += player->ticks_per_interrupt;
		}
	}
	if (playing) {
		/* generate sound up to buffer end */
		chunk_size = ticks_to_buffer_end >> TICK_MULTIPLIER;
		ptr = ayemu_gen_sound( &player->ay, ptr, chunk_size );
		frames_written += chunk_size;
		player->ticks_to_next_interrupt -= chunk_size << TICK_MULTIPLIER;
	}
	return frames_written;
}

void ayemu_player_close( ayemu_player_t *player )
{
	player->close( player->userdata );
}
