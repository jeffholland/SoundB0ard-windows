#include <pthread.h>
#include <sndfile.h>
#include <stdlib.h>
#include <string.h>

#include "defjams.h"
#include "drumr.h"
#include "mixer.h"
#include "utils.h"

extern mixer *mixr;

void pattern_char_to_int(char *char_pattern, int *final_pattern)
{
    int sp_count = 0;
    char *sp, *sp_last, *spattern[32];
    char *sep = " ";

    // extract numbers from string into spattern
    for (sp = strtok_r(char_pattern, sep, &sp_last); sp;
         sp = strtok_r(NULL, sep, &sp_last)) {
        spattern[sp_count++] = sp;
    }

    for (int i = 0; i < sp_count; i++) {
        int pat_num = atoi(spattern[i]);
        if (pat_num < DRUM_PATTERN_LEN) {
            printf("PAT_NUM: %d is %d\n", pat_num, (1 << pat_num));
            *final_pattern = (1 << pat_num) | *final_pattern;
            printf("NOW SET %d\n", *final_pattern);
        }
    }
}

int *load_file_to_buffer(char *filename, int *bufsize, SF_INFO *sf_info)
{
    SNDFILE *snd_file;

    sf_info->format = 0;
    snd_file = sf_open(filename, SFM_READ, sf_info);
    if (!snd_file) {
        printf("Err opening %s : %d\n", filename, sf_error(snd_file));
        return NULL;
    }
    printf("Filename:: %s\n", filename);
    printf("SR: %d\n", sf_info->samplerate);
    printf("Channels: %d\n", sf_info->channels);
    printf("Frames: %lld\n", sf_info->frames);

    *bufsize = sf_info->channels * sf_info->frames;
    printf("Making buffer size of %d\n", *bufsize);

    int *buffer = calloc(*bufsize, sizeof(int));
    if (buffer == NULL) {
        printf("Ooft, memory issues, mate!\n");
        return NULL;
    }

    sf_readf_int(snd_file, buffer, *bufsize);
    return buffer;
}

DRUM *new_drumr(char *filename, char *pattern)
{
    DRUM *drumr = calloc(1, sizeof(DRUM));

    pattern_char_to_int(pattern, &drumr->patterns[drumr->num_patterns++]);

    SF_INFO sf_info;
    memset(&sf_info, 0, sizeof(SF_INFO));
    int bufsize;
    int *buffer = load_file_to_buffer(filename, &bufsize, &sf_info);

    int fslen = strlen(filename);
    drumr->filename = calloc(1, fslen + 1);
    strncpy(drumr->filename, filename, fslen);

    drumr->buffer = buffer;
    drumr->bufsize = bufsize;
    drumr->samplerate = sf_info.samplerate;
    drumr->channels = sf_info.channels;
    drumr->vol = 0.7;

    drumr->sound_generator.gennext = &drum_gennext;
    drumr->sound_generator.status = &drum_status;
    drumr->sound_generator.getvol = &drum_getvol;
    drumr->sound_generator.setvol = &drum_setvol;
    drumr->sound_generator.type = DRUM_TYPE;

    // TODO: do i need to free pattern ?
    return drumr;
}

void update_pattern(void *self, int newpattern)
{
    DRUM *drumr = self;
    drumr->pattern = newpattern;

    // TODO: do i need to free old pattern too?
}

double drum_gennext(void *self)
// void drum_gennext(void* self, double* frame_vals, int framesPerBuffer)
{
    DRUM *drumr = self;
    double val = 0;

    int cur_pattern_position =
        1 << (mixr->sixteenth_note_tick % DRUM_PATTERN_LEN); // bitwise pattern
    int sample_idx = conv_bitz(
        cur_pattern_position); // convert to an integer we can use as index below


    if ((drumr->patterns[drumr->cur_pattern_num] & cur_pattern_position) &&
            !drumr->sample_positions[sample_idx].played) {

        if (drumr->swing) {
            if (mixr->sixteenth_note_tick % 2) {
                double swing_offset = PPQN * 2 / 100.0;
                switch (drumr->swing_setting) {
                    case 1:
                        swing_offset = swing_offset * 50 - PPQN;
                        break;
                    case 2:
                        swing_offset = swing_offset * 54 - PPQN;
                        break;
                    case 3:
                        swing_offset = swing_offset * 58 - PPQN;
                        break;
                    case 4:
                        swing_offset = swing_offset * 62 - PPQN;
                        break;
                    case 5:
                        swing_offset = swing_offset * 66 - PPQN;
                        break;
                    case 6:
                        swing_offset = swing_offset * 71 - PPQN;
                        break;
                    default:
                        swing_offset = swing_offset * 50 - PPQN;
                }
                if (mixr->tick % (PPQN/4) == (int)swing_offset/2) {
                    drumr->sample_positions[sample_idx].playing = 1;
                    drumr->sample_positions[sample_idx].played = 1;
                    //printf("SWING SWUNG tick %% PPQN: %d\n", mixr->tick % PPQN);
                }
            } 
            else {
                drumr->sample_positions[sample_idx].playing = 1;
                drumr->sample_positions[sample_idx].played = 1;
                //printf("SWING NORM tick %% PPQN: %d\n", mixr->tick % PPQN);
            }
        }
        else {
          drumr->sample_positions[sample_idx].playing = 1;
          drumr->sample_positions[sample_idx].played = 1;
        }
    }

    for (int i = 0; i < DRUM_PATTERN_LEN; i++) {
        if (drumr->sample_positions[i].playing) {
            val +=
                drumr->buffer[drumr->sample_positions[i].position++] /
                2147483648.0; // convert from 16bit in to double between 0 and 1
            if ((int)drumr->sample_positions[i].position ==
                drumr->bufsize) { // end of playback - so reset
              drumr->sample_positions[i].playing = 0;
              drumr->sample_positions[i].position = 0;
            }
        }
    }

    if (mixr->sixteenth_note_tick != drumr->tick) {
        int prev_note = sample_idx - 1;
        if (prev_note == -1)
            prev_note = 15;

        drumr->sample_positions[prev_note].played = 0;
        drumr->tick = mixr->sixteenth_note_tick;

        if (drumr->tick % 16 == 0) {
            drumr->cur_pattern_num =
                (drumr->cur_pattern_num + 1) % drumr->num_patterns;
        }
    }

    val = effector(&drumr->sound_generator, val);
    val = envelopor(&drumr->sound_generator, val);

    return val * drumr->vol;
}

void drum_status(void *self, char *status_string)
{
    DRUM *drumr = self;
    char spattern[DRUM_PATTERN_LEN + 1] = "";
    for (int i = 0; i < DRUM_PATTERN_LEN; i++) {
        if (drumr->pattern & (1 << i)) {
            strncat(&spattern[i], "1", 1);
        }
        else {
            strncat(&spattern[i], "0", 1);
        }
    }
    spattern[DRUM_PATTERN_LEN] = '\0';
    snprintf(status_string, 119,
             ANSI_COLOR_CYAN "[%s]\t[%s] vol: %.2lf" ANSI_COLOR_RESET,
             drumr->filename, spattern, drumr->vol);
}

double drum_getvol(void *self)
{
    DRUM *drumr = self;
    return drumr->vol;
}

void drum_setvol(void *self, double v)
{
    DRUM *drumr = self;
    if (v < 0.0 || v > 1.0) {
        return;
    }
    drumr->vol = v;
}

void swingrrr(void *self, int swing_setting)
{
    DRUM *drumr = self;
    // printf("SWING CALLED FOR %d\n", swing_setting);
    if (drumr->swing) {
        printf("swing OFF\n");
        drumr->swing = 0;
    }
    else {
        printf("Swing ON to %d\n", swing_setting);
        drumr->swing = 1;
        drumr->swing_setting = swing_setting;
    }
}

void add_pattern(void *self, char *pattern)
{
    DRUM *drumr = self;
    pattern_char_to_int(pattern, &drumr->patterns[drumr->num_patterns++]);
}
