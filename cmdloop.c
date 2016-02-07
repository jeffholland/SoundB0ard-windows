#include <pthread.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "algoriddim.h"
#include "audioutils.h"
#include "bpmrrr.h"
#include "envelope.h"
#include "fm.h"
#include "cmdloop.h"
#include "defjams.h"
#include "mixer.h"
#include "table.h"
#include "utils.h"

extern mixer *mixr;
extern bpmrrr *b;
extern ENVSTREAM *ampstream;

// TODO: make into a single array of lookup tables
extern GTABLE *sine_table;
extern GTABLE *tri_table;
extern GTABLE *square_table;
extern GTABLE *saw_down_table;
extern GTABLE *saw_up_table;

void loopy(void)
{
  char *line;
  while ((line = readline(ANSI_COLOR_MAGENTA "SB#> " ANSI_COLOR_RESET))!= NULL) {
    if (line[0] != 0) {
      add_history(line);
      interpret(line);
    }
  }
}

void ps() 
{
  mixer_ps(mixr);
  bpm_info(b);
  ps_envelope_stream(ampstream);
  //table_info(gtable);
}

void gen() 
{
  gen_next(mixr);
}

int exxit()
{
    printf("\nBeat it, ya val jerk...\n");
    pa_teardown();
    exit(0);
    //return 0;
}

void interpret(char *line)
{
  // easy string comparisons
  if (strcmp(line, "ps") == 0) {
    ps();
    return;
  } else if (strcmp(line, "song") == 0) {
    pthread_t songrun_th;
    if ( pthread_create (&songrun_th, NULL, algo_run, NULL)) {
      fprintf(stderr, "Errr running song\n");
      return;
    }
    pthread_detach(songrun_th);
  } else if (strcmp(line, "exit") == 0) {
    exxit();
  }

  // TODO: move regex outside function to compile once
  // SINE|SAW|TRI (FREQ)
  regmatch_t pmatch[3];
  regex_t cmdtype_rx;
  regcomp(&cmdtype_rx, "(bpm|stop|sine|sawd|sawu|tri|square) ([[:digit:]]+)", REG_EXTENDED|REG_ICASE);

  if (regexec(&cmdtype_rx, line, 3, pmatch, 0) == 0) {

    int val = 0;
    char cmd[20];
    sbmsg *msg = calloc(1, sizeof(sbmsg));

    sscanf(line, "%s %d", cmd, &val);
    msg->freq = val;

    if (strcmp(cmd, "bpm") == 0) {
      bpm_change(b, val);
      update_envelope_stream_bpm(ampstream);
    } else if (strcmp(cmd, "stop") == 0) {
      strncpy(msg->cmd, "faderrr", 19);
      thrunner(msg);
    } else {
      strncpy(msg->cmd, "timed_sig_start", 19);
      strncpy(msg->params, cmd, 10);
      thrunner(msg);


      //if ( pthread_create (&timed_start_th, NULL, timed_sig_start, params)) {
      //  printf("Ooft!\n");
      //}
    }
  }

  regmatch_t tpmatch[4];
  regex_t tsigtype_rx;
  regcomp(&tsigtype_rx, "(vol|freq|fm) ([[:digit:]]+) ([[:digit:]]+)", REG_EXTENDED|REG_ICASE);
  if (regexec(&tsigtype_rx, line, 3, tpmatch, 0) == 0) {
    int sig = 0;
    double val = 0;
    char cmd_type[10];
    sscanf(line, "%s %d %lf", cmd_type, &sig, &val);
    if (strcmp(cmd_type, "vol") == 0) {
      vol_change(mixr, sig, val);
      printf("VOL! %s %d %lf\n", cmd_type, sig, val);
    }
    if (strcmp(cmd_type, "freq") == 0) {
      freq_change(mixr, sig, val);
      printf("FREQ! %s %d %lf\n", cmd_type, sig, val);
    }
    if (strcmp(cmd_type, "fm") == 0) {
      add_fm(mixr, sig, val);
      printf("FML!\n");
    }
  }
}
