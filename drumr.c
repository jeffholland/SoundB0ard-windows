#include <libgen.h>
#include <sndfile.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "bpmrrr.h"
#include "defjams.h"
#include "drumr.h"

extern bpmrrr *b;

DRUM* new_drumr(char *filename, char *pattern)
{
  DRUM *drumr = calloc(1, sizeof(DRUM));
  drumr->position = 0;
  drumr->swing = 0;

  // drum pattern stuff
  int sp_count = 0;
  char *sp, *sp_last, *spattern[32];
  char *sep = " ";

  // extract numbers from string into spattern
  for ( sp = strtok_r(pattern, sep, &sp_last);
        sp;
        sp = strtok_r(NULL, sep, &sp_last))
  {
      spattern[sp_count++] = sp;
  }

  // convert those spattern chars into real integers and use
  // as index into DRUM*'s pattern
  for (int i = 0; i < sp_count; i++) {
    int pat_num = atoi(spattern[i]);
    if (pat_num < DRUM_PATTERN_LEN) {
      //drumr->pattern[pat_num] = 1;
      printf("PAT_NUM: %d is %d\n", pat_num, ( 1 << pat_num));
      drumr->pattern = ( 1 << pat_num ) | drumr->pattern;  /// bitmask!
      printf("NOW SET %d\n", drumr->pattern);
    }
  }

  // soundfile part
  SNDFILE *snd_file;
  SF_INFO sf_info;

  sf_info.format = 0;
  snd_file = sf_open(filename, SFM_READ, &sf_info);
  if (!snd_file) {
    printf("Err opening %s : %d\n", filename, sf_error(snd_file));
    return (void*) NULL;
  }
  printf("Filename:: %s\n", filename);
  printf("SR: %d\n", sf_info.samplerate);
  printf("Channels: %d\n", sf_info.channels);
  printf("Format: %d\n", sf_info.format);
  printf("Frames: %lld\n", sf_info.frames);

  int bufsize = sf_info.channels * sf_info.frames;
  printf("Making buffer size of %d\n", bufsize);

  int *buffer = calloc(bufsize, sizeof(int));
  if (buffer == NULL) {
    printf("Ooft, memory issues, mate!\n");
    return (void*) NULL;
  }

  printf("SFREADF_INT\n");
  sf_count_t framecount = sf_readf_int(snd_file, buffer, bufsize);
  printf("FRAMES READ/WRITTEN IS %" PRId64 "\n", framecount);
  printf("POST SFREADF_INT\n");

  int fslen = strlen(filename);
  drumr->filename = calloc(1, fslen + 1);
  strncpy(drumr->filename, filename, fslen);

  drumr->buffer = buffer;
  drumr->bufsize = bufsize;
  drumr->samplerate = sf_info.samplerate;
  drumr->channels = sf_info.channels;
  drumr->vol = 0.0;

  drumr->sound_generator.gennext = &drum_gennext;
  drumr->sound_generator.status = &drum_status;
  drumr->sound_generator.getvol = &drum_getvol;
  drumr->sound_generator.setvol = &drum_setvol;
  drumr->sound_generator.type = DRUM_TYPE;

  // TODO: do i need to free pattern ?
  return drumr;
}

void swingrrr(void *self, int swing_setting)
{
    DRUM *drumr = self;
    drumr->swing = 1 - drumr->swing; // toggle
    drumr->swing_setting = swing_setting;
}

void update_pattern(void *self, int newpattern)
{
    DRUM *drumr = self;
    drumr->pattern = newpattern;

    // TODO: do i need to free old pattern too?
}

double drum_gennext(void *self)
//void drum_gennext(void* self, double* frame_vals, int framesPerBuffer)
{
  DRUM *drumr = self;
  double val = 0;

  if ( (b->cur_tick % (TICKS_PER_BEAT)) == 0 ) {
    if (!drumr->tick_started) {
      drumr->tick++;
      drumr->tick_started = 1;
    }
  } else {
    drumr->tick_started = 0;
  }

  if (!drumr->swing) {
    if (drumr->pattern & (1 << (drumr->tick % DRUM_PATTERN_LEN)) && 
                          (b->cur_tick % (TICKS_PER_BEAT) == 0)) {
      drumr->playing = 1;
      drumr->position = 0;
    }
  } else {
    if (drumr->pattern & ( 1 << (drumr->tick % DRUM_PATTERN_LEN))) {
      if ( b->cur_tick % TICKS_PER_BEAT == 0 ){
        //printf("SCHWING\n");
        drumr->playing = 1;
        drumr->position = 0;
      } else if ( (drumr->tick % 2) && b->cur_tick % TICKS_PER_BEAT == drumr->swing_setting ){
        //printf("SCHWUNG\n");
        drumr->playing = 1;
        drumr->position = 0;
      }
    }
  }

  if (drumr->playing) {
    val = drumr->buffer[drumr->position++] / 2147483648.0 ; // convert 16bit to double btw 0 and 1
  }

  if ((int)drumr->position == drumr->bufsize) { // end of playback - so reset
    drumr->playing = 0;
    drumr->position = 0;
  }

  if (val > 1 || val < -1)
    printf("BURNIE - DRUM OVERLOAD!\n");

  val = effector(&drumr->sound_generator, val);
  val = envelopor(&drumr->sound_generator, val);

  return val * drumr->vol;
}

void drum_status(void *self, char *status_string)
{
  DRUM *drumr = self;
  char spattern[DRUM_PATTERN_LEN + 1] = "";
  for (int i = 0; i < DRUM_PATTERN_LEN; i++) {
    if (drumr->pattern & ( 1 << i) ) {
      strncat(&spattern[i], "1", 1);
    } else {
      strncat(&spattern[i], "0", 1);
    }
  }
  spattern[DRUM_PATTERN_LEN] = '\0';
  snprintf(status_string, 119, ANSI_COLOR_CYAN "[%s]\t[%s] vol: %.2lf" ANSI_COLOR_RESET, basename(drumr->filename), spattern, drumr->vol);
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
