#pragma once

#include <stdbool.h>
#include <wchar.h>

#include <defjams.h>
#include <midimaaan.h>

enum
{
    FOLD_FWD,
    FOLD_BAK,
};

class sequenceengine
{
  public:
    sequenceengine() = default;
    ~sequenceengine() = default;

  public:
    int tick; // current 16th note tick from mixer
    midi_event pattern[PPBAR] = {};

    // these used to traverse the pattern in fun ways
    // float count_by;
    float cur_step{0};
    bool started{false};
};

void sequence_engine_reset(sequenceengine *engine);

////////////////////////////////////////////////////////////

bool sequence_engine_list_presets(unsigned int type);
void sequence_engine_clear_events_from(sequenceengine *self, int start_idx);
