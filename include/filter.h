#pragma once

#include "defjams.h"
#include "modmatrix.h"
#include "synthfunctions.h"

// 46.88.. = semitones between frequencies (80, 18000.0) / 2
// taken from Will Pirkle book 'designing software synths..'
#define FILTER_FC_MOD_RANGE 46.881879936465680
#define FILTER_FC_MIN 80        // 80Hz
#define FILTER_FC_MAX 18000     // 18 kHz
#define FILTER_FC_DEFAULT 10000 // 10kHz
#define FILTER_Q_DEFAULT 0.707  // butterworth (noidea!)
#define FILTER_Q_MOD_RANGE 10   // dunno if this will work!
#define FILTER_TYPE_DEFAULT LPF4

typedef enum
{
    LPF1,
    HPF1,
    LPF2,
    HPF2,
    BPF2,
    BSF2,
    LPF4,
    HPF4,
    BPF4,
    NUM_FILTER_TYPES
} filter_type;

struct Filter
{
    Filter();
    virtual ~Filter() = default;

    ModulationMatrix *modmatrix{nullptr};
    GlobalFilterParams *global_filter_params{nullptr};

    // sources
    unsigned m_mod_source_fc;
    unsigned m_mod_source_fc_control;

    // GUI controls
    double m_fc_control;  // filter cut-off
    double m_q_control;   // 'qualvity factor' 1-10
    double m_aux_control; // a spare control, used in SEM and ladder filters

    unsigned m_nlp;           // Non Linear Processing on/off switch
    double m_saturation{100}; // used in NLP

    unsigned m_filter_type;

    double m_fc;     // current filter cut-off val
    double m_q;      // current q value
    double m_fc_mod; // frequency cutoff modulation input

    virtual void SetFcMod(double d);
    virtual void SetQControl(double d);
    virtual void SetFcControl(double val);
    virtual void Update();
    virtual void Reset();
    virtual double DoFilter(double xn) = 0;

    void SetType(unsigned int type);
    void InitGlobalParameters(GlobalFilterParams *params);
};
