#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "ayemu.h"

unsigned short tone_table[0x60] = {
	0x0ef8, 0x0e10, 0x0d60, 0x0c80, 0x0bd8, 0x0b28, 0x0a88, 0x09f0,
	0x0960, 0x08e0, 0x0858, 0x07e0, 0x077c, 0x0708, 0x06b0, 0x0640,
	0x05ec, 0x0594, 0x0544, 0x04f8, 0x04b0, 0x0470, 0x042c, 0x03f0,
	0x03be, 0x0384, 0x0358, 0x0320, 0x02f6, 0x02ca, 0x02a2, 0x027c,
	0x0258, 0x0238, 0x0216, 0x01f8, 0x01df, 0x01c2, 0x01ac, 0x0190,
	0x017b, 0x0165, 0x0151, 0x013e, 0x012c, 0x011c, 0x010b, 0x00fc,
	0x00ef, 0x00e1, 0x00d6, 0x00c8, 0x00bd, 0x00b2, 0x00a8, 0x009f,
	0x0096, 0x008e, 0x0085, 0x007e, 0x0077, 0x0070, 0x006b, 0x0064,
	0x005e, 0x0059, 0x0054, 0x004f, 0x004b, 0x0047, 0x0042, 0x003f,
	0x003b, 0x0038, 0x0035, 0x0032, 0x002f, 0x002c, 0x002a, 0x0027,
	0x0025, 0x0023, 0x0021, 0x001f, 0x001d, 0x001c, 0x001a, 0x0019,
	0x0017, 0x0016, 0x0015, 0x0013, 0x0012, 0x0011, 0x0010, 0x000f
};

/* channel a's events table initially points here so that we immediately fetch a new position */
static unsigned char song_start_marker = 0xff;

/* static functions */
static int get_sample_pos(ayemu_stc_channel_t *channel);
static unsigned short get_pitch(unsigned short sample_pitch, int sample_pos, ayemu_stc_channel_t *channel, int height);

/** Open stc file for playback
 *
 *  Return value: true if success, else false
 */
 
int ayemu_stc_open( ayemu_stc_t *stc, const char *filename )
{
	unsigned char *posn_table;
	unsigned char *pattern;
	unsigned char *ornament;
	unsigned char *sample;
	unsigned char *stc_data_end;

	int fd, i;
	struct stat file_info;
	
	fd = open( filename, O_RDONLY );
	if (fd == -1) {
		perror( "Unable to open file" );
		return 0;
	}

	if (fstat(fd, &file_info)) {
		perror( "Unable to stat file" );
		close( fd );
		return 0;
	}
	
	stc->data = (unsigned char *)malloc( file_info.st_size );

	if (read( fd, stc->data, file_info.st_size ) != file_info.st_size) {
		perror( "Error reading file");
		free( stc->data );
		close( fd );
		return -1;
	}

	close( fd );
	stc_data_end = stc->data + file_info.st_size;
	
	stc->delay = stc->data[0];
	posn_table = stc->data + (stc->data[1] | stc->data[2] << 8);
	stc->song_length = *posn_table;
	stc->positions = posn_table + 1;
	
	ornament = stc->data + (stc->data[3] | stc->data[4] << 8);
	for (i = 0; i < 16; i++) {
		stc->ornaments[i] = NULL;
	}
	for (i = 0; i < 16 && *ornament < 16 && ornament < stc_data_end; i++) {
		if (stc->ornaments[*ornament] == NULL) {
			stc->ornaments[*ornament] = ornament + 1;
		}
		ornament += 0x0021;
	}
	
	pattern = stc->data + (stc->data[5] | stc->data[6] << 8);
	for (i = 0; i < 32; i++) {
		stc->patterns[i] = NULL;
	}
	for (i = 0; i < 32 && *pattern < 32 && pattern < stc_data_end; i++) {
		stc->patterns[*pattern] = pattern + 1;
		pattern += 0x0007;
	}

	sample = stc->data + 0x001b;
	for (i = 0; i < 16; i++) {
		stc->samples[i] = NULL;
	}
	for (i = 0; i < 16 && *sample < 16 && sample < stc_data_end; i++) {
		if (stc->samples[*sample] == NULL) {
			stc->samples[*sample] = sample + 1;
		}
		sample += 0x0063;
	}
	
	ayemu_stc_reset(stc);
	return 1;
}

void ayemu_stc_reset( ayemu_stc_t *stc)
{
	int i;
	stc->channels[0].events = &song_start_marker;
	stc->delay_counter = 1;
	stc->position = 0;
	memset( stc->last_registers, 0, 14 * sizeof(unsigned char) );
	stc->has_looped = 0;
	for (i = 0; i < 3; i++) {
		stc->channels[i].sample_repeat_counter = -1; /* not playing */
		stc->channels[i].current_ornament = stc->ornaments[0];
		stc->channels[i].current_sample = stc->samples[1]; /* left undefined in original routine */
		/* sample_position and note_value will be set on first note. Other sample parameters
		(e.g. repeat/replen) will come into play after that, so we don't need to worry
		about them here */
		stc->channels[i].envelope_state = ENVELOPE_OFF;
		stc->channels[i].row_skip = 0;
		stc->channels[i].row_counter = 0;
	}
}

/** Fetch the next interrupt frame of data as an array of AY registers.
 * \return 1 if frame sent, 0 if data is finished
 */
int ayemu_stc_get_frame(ayemu_stc_t *stc, unsigned char *regs) {
	int i;
	ayemu_stc_channel_t *channel;
	unsigned char event, mask, noise_mask, tone_mask, noise_value, volume;
	int position, sample_pos;
	unsigned char *position_info;
	unsigned char *pattern_info;
	unsigned short pitch, sample_pitch;
	unsigned char *sample_data;

	memcpy( regs, stc->last_registers, 14 * sizeof(unsigned char) );
	stc->delay_counter--;
	if (stc->delay_counter == 0) {
    /* process new row */
		stc->delay_counter = stc->delay; /* reset delay counter */
    for (i = 0; i < 3; i++) {
    	channel = stc->channels + i;
    	
    	channel->row_counter--;
			if (channel->row_counter < 0) { /* are we on a row where we should read new events? */
				channel->row_counter = channel->row_skip; /* reset row-skip counter */
	
				if (i == 0 && *(channel->events) == 0xff) { /* check for pattern end marker */
					/* start next position */
					position = stc->position;
					if (position > stc->song_length) {
						/* restart song */
						stc->has_looped = 1;
						position = 0;
					}
					stc->position = position + 1;
					/* read position info from appropriate offset in positions table */
					position_info = stc->positions + position * 2;
					stc->position_height = position_info[1];
					pattern_info = stc->patterns[position_info[0]];
					stc->channels[0].events = stc->data + (pattern_info[0] | (pattern_info[1] << 8));
					stc->channels[1].events = stc->data + (pattern_info[2] | (pattern_info[3] << 8));
					stc->channels[2].events = stc->data + (pattern_info[4] | (pattern_info[5] << 8));
				}

				/* read event data */
				for (;;) {
					event = *(channel->events);
					channel->events++;
					if (event < 0x60) {
						channel->note_value = event; /* set note value */
						channel->sample_position = 0x00;
						channel->sample_repeat_counter = 0x20;
						break;
					} else if (event < 0x70) {
						/* set sample */
						channel->current_sample = stc->samples[event-0x60];
					} else if (event < 0x80) {
						/* set ornament */
						channel->current_ornament = stc->ornaments[event-0x70];
						channel->envelope_state = 0;
					} else if (event == 0x80) {
						/* rest: turn off sample */
						channel->sample_repeat_counter = -1;
						break;
					} else if (event == 0x81) {
						/* do nothing */
						break;
					} else if (event == 0x82) {
						/* set ornament 0 */
						channel->current_ornament = stc->ornaments[0];
						channel->envelope_state = 0;
					} else if (event < 0x8f) {
						/* set envelope */
						regs[13] = event - 0x80;
						regs[12] = 0x00; /* unset in original */
						regs[11] = *(channel->events);
						channel->events++;
						channel->envelope_state = ENVELOPE_TRIGGERED;
						channel->current_ornament = stc->ornaments[0];
					} else {
						/* set row_skip */
						channel->row_counter = channel->row_skip = event - 0xa1;
					}
				}
			}
		}
	}

	/* Render AY output for the current frame */
	for (i = 0; i < 3; i++) {
   	channel = stc->channels + i;
		sample_pos = get_sample_pos(channel);
		regs[7] = 0; /* mixer */
		if (i == 0 || channel->sample_repeat_counter != -1) {
			/* not clear why channel 0 gets special treatment, except to ensure that mixer_reg gets set with something */

			/* read sample frame */
			sample_data = channel->current_sample + 3*sample_pos;
			mask = sample_data[1];	/* read mask bits */
			noise_mask = (mask & 0x80) ? 0x08 : 0x00;
			tone_mask = (mask & 0x40) ? 0x01 : 0x00;	/* tone mask */
			noise_value = mask & 0x1f; /* noise value */
			sample_pitch = ((sample_data[0] & 0xf0) << 4) | sample_data[2];
			if (mask & 0x20) sample_pitch |= 0x1000; /* sign bit */
			volume = sample_data[0] & 0x0f;
			regs[7] |= (noise_mask | tone_mask) << i;
			if (channel->sample_repeat_counter != -1) {
				if (noise_mask == 0) regs[6] = noise_value;
				pitch = get_pitch(sample_pitch, sample_pos, channel, stc->position_height);
				regs[i << 1] = pitch & 0xff;
				regs[(i << 1) + 1] = pitch >> 8;
				regs[i + 8] = volume;
			} else {
				regs[i + 8] = 0; /* sample ended, so silence */
			}
			if (channel->sample_repeat_counter != 0xff && channel->envelope_state != ENVELOPE_OFF) {
				if (channel->envelope_state != ENVELOPE_ON) {
					channel->envelope_state = ENVELOPE_ON;
					printf("triggering envelope %d\n", regs[13]);
				} else {
					printf("holding envelope %d\n", regs[13]);
					regs[13] = 0xff; /* was 0 in original routine; set to 0xff to indicate no change */
				}
				regs[i + 8] = regs[i + 8] | 0x10;
			}
		}
	}
	printf("---\n");
	memcpy( stc->last_registers, regs, 14 * sizeof(unsigned char) );

	return !stc->has_looped;

}

/** Free all of allocated resources for this file.
 *
 */
void ayemu_stc_close(ayemu_stc_t *stc)
{
	if (stc->data != NULL) {
		free(stc->data);
		stc->data = NULL;
	}
}


static int get_sample_pos(ayemu_stc_channel_t *channel) {
	int pos = 0; /* actually left uninitialized in original routine */
	unsigned char *repeat_info;
	
	if (channel->sample_repeat_counter != -1) { /* if sample not finished */
		channel->sample_repeat_counter--;

		pos = channel->sample_position;
		channel->sample_position = (pos + 1) & 0x1f;

		if (channel->sample_repeat_counter == 0) { /* if on last frame of sample */
			repeat_info = channel->current_sample + 0x0060;
			/* get repeat value (repeat, not replen) */
			if (repeat_info[0] == 0) { /* original routine tests sign flag after decrementing,
				so strictly speaking a repeat value from 0x81 to 0xff would have the same effect */
				channel->sample_repeat_counter = -1; /* if repeat was 0, set sample position to -1 (not playing) */
			} else {
				pos = repeat_info[0] - 1; /* c = new offset into sample */
				channel->sample_position = (pos + 1) & 0x1f; /* and into ornament */
				channel->sample_repeat_counter = repeat_info[1];
			}
		}
	}
	return pos;
}

static unsigned short get_pitch(unsigned short sample_pitch, int sample_pos, ayemu_stc_channel_t *channel, int height) {
	unsigned short pitch;
	pitch = tone_table[
		channel->note_value + channel->current_ornament[sample_pos] + height
	];
	if (sample_pitch & 0x1000) { /* here de = sample pitch */
		pitch += (sample_pitch & 0xefff); /* add sample_pitch with sign bit (bit 4) stripped */
	} else {
		pitch -= sample_pitch;
	}
	return pitch;
}
