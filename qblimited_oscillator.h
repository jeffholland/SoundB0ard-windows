#pragma once

#include "oscillator.h"

typedef struct qblimited_oscillator qb_osc;

struct qblimited_oscillator {
    oscillator *osc;
};

qb_osc *qb_osc_new(void);

double qb_do_oscillate(oscillator *self, double *quad_phase_output);

void qb_start_oscillator(oscillator *self);
void qb_stop_oscillator(oscillator *self);

void qb_reset_oscillator(oscillator *self);

double qb_do_sawtooth(oscillator *self, double modulo, double dInc);
double qb_do_square(oscillator *self, double modulo, double dInc);
double qb_do_triangle(double modulo, double dInc, double dFo,
                      double dSquareModulator, double *pZ_register);
