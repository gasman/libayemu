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


int psg_play_frame( void *userdata, ayemu_ay_t *ay)
{
	ayemu_psg_t *psg = (ayemu_psg_t *)userdata;
	return ayemu_psg_play_frame( psg, ay );
}

int psg_close( void *userdata )
{
	ayemu_psg_t *psg = (ayemu_psg_t *)userdata;
	ayemu_psg_close( psg );
	free( psg );
	return 1;
}

int stc_play_frame( void *userdata, ayemu_ay_t *ay)
{
	unsigned char ay_regs[14];
	ayemu_stc_t *stc = (ayemu_stc_t *)userdata;
	
	if (ayemu_stc_get_frame( stc, ay_regs )) {
		ayemu_set_regs( ay, ay_regs );
		return 1;
	} else {
		return 0;
	}
}

int stc_close( void *userdata )
{
	ayemu_stc_t *stc = (ayemu_stc_t *)userdata;
	ayemu_stc_close( stc );
	free( stc );
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
	
	ayemu_psg_t *psg;
	
	psg = malloc( sizeof(ayemu_psg_t) );
	
	if (!ayemu_psg_open( psg, filename )) {
		free( psg );
		return 0;
	}

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

int ayemu_player_open_stc( const char *filename, ayemu_player_sndfmt_t *format, ayemu_player_t *player ) {
	
	ayemu_stc_t *stc;
	
	stc = malloc( sizeof(ayemu_stc_t) );
	
	if (!ayemu_stc_open( stc, filename )) {
		free( stc );
		return 0;
	}

	ayemu_init( &player->ay );
	ayemu_set_sound_format( &player->ay, format->freq, format->channels, format->bpc );
	ayemu_set_chip_type( &player->ay, AYEMU_AY, NULL );
	ayemu_set_chip_freq( &player->ay, AY_FREQ );
	ayemu_set_stereo( &player->ay, AYEMU_ACB, NULL );	
	
	player->ticks_per_interrupt = (format->freq << TICK_MULTIPLIER) / INTERRUPT_FREQ;
	player->ticks_to_next_interrupt = 0;
	player->userdata = stc;
	player->play_frame = &stc_play_frame;
	player->close = &stc_close;
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
