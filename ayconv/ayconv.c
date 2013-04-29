#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sndfile.h>
#include <ayemu.h>

#define AUDIO_FREQ 44100
#define AUDIO_CHANS 2
#define AUDIO_BITS 16
#define BUFFER_SIZE 1024 /* number of frames */

int main()
{
	SF_INFO wav_info;
	SNDFILE *wav;
	ayemu_player_t player;
	ayemu_player_sndfmt_t format;

	short buf[BUFFER_SIZE * 2];
	size_t filled_size;
	
	format.freq = wav_info.samplerate = AUDIO_FREQ;
	format.channels = wav_info.channels = AUDIO_CHANS;
	format.bpc = AUDIO_BITS;
	wav_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	ayemu_player_open_psg( "dancing_queen.psg", &format, &player );
	
	wav = sf_open( "test.wav", SFM_WRITE, &wav_info );
	
	while ( (filled_size = ayemu_player_fill_buffer( &player, buf, BUFFER_SIZE ) ) > 0) {
		sf_writef_short( wav, buf, filled_size );
	}
	
	sf_close( wav );
	ayemu_player_close( &player );
	return 0;
}
