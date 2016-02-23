#ifndef OSCIL_H
#define OSCIL_H

#include <stdio.h>

#include "oscil.h"
#include "sound_generator.h"
#include "table.h"

typedef struct t_oscil OSCIL;

typedef void (*volly) (OSCIL* osc, double vol);
typedef void (*freqy) (OSCIL* osc, double freq);
typedef void (*freqy) (OSCIL* osc, double freq);
typedef void (*incry) (OSCIL* osc, double freq);

typedef struct t_oscil
{

  SOUNDGEN sound_generator;

  double freq;
  double curphase;
  double incr;
  double vol;

  const GTABLE* gtable;
  double dtablen;

  volly voladj;
  freqy freqadj;
  incry incradj;

} OSCIL;

OSCIL* new_oscil(double freq, GTABLE *gt);

double oscil_gennext(void *self);
void volfunc(OSCIL* p_osc, double vol);
void freqfunc(OSCIL* p_osc, double freq);
void incrfunc(OSCIL* p_osc, double v);

void oscil_status(void *self, char *status_string);

#endif // OSCIL_H
