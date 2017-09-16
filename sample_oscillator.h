#pragma once

#include "audiofile_data.h"
#include "oscillator.h"

typedef struct sample_oscillator sampleosc;

typedef struct sample_oscillator
{
    oscillator osc;
    audiofile_data *afd;
    double m_read_idx;

} sample_oscillator;

sample_oscillator *sampleosc_new(void);

void sampleosc_set_soundgenerator_interface(sampleosc *self);

void sampleosc_start_oscillator(oscillator *self);
void sampleosc_stop_oscillator(oscillator *self);
void sampleosc_reset_oscillator(oscillator *self);

double sampleosc_do_oscillate(oscillator *self, double *quad_phase_output);

double sampleosc_read_sample_buffer(sampleosc *sosc);
