/* High-level AY playback handlers */

#include <stdlib.h>
#include "ayemu.h"

#define AUDIO_FREQ 44100
#define AUDIO_CHANS 2
#define AUDIO_BITS 16
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


int ayemu_player_open_vtx( const char *filename, ayemu_player_t *player ) {
	VTX_PLAYER_STATE *vtx_state;
	
	ayemu_init( &player->ay );
	ayemu_set_sound_format( &player->ay, AUDIO_FREQ, AUDIO_CHANS, AUDIO_BITS );
	ayemu_set_chip_type( &player->ay, AYEMU_AY, NULL );
	ayemu_set_chip_freq( &player->ay, AY_FREQ );
	ayemu_set_stereo( &player->ay, AYEMU_ACB, NULL );
	
	vtx_state = malloc( sizeof(VTX_PLAYER_STATE) );
	vtx_state->frame = 0;
	ayemu_vtx_open( &vtx_state->vtx, filename );
	ayemu_vtx_load_data( &vtx_state->vtx );
	
	player->ticks_per_interrupt = (AUDIO_FREQ << TICK_MULTIPLIER) / INTERRUPT_FREQ;
	player->ticks_to_next_interrupt = 0;
	player->userdata = vtx_state;
	player->play_frame = &vtx_play_frame;
	player->close = &vtx_close;
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
