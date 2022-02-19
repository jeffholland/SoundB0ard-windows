#pragma once

#include <audioutils.h>
#include <soundgenerator.h>

#include <interpreter/object.hpp>
#include <string>
#include <vector>

enum AudioAction {
  ADD,
  ADD_FX,
  BPM,
  INFO,
  HELP,
  LIST_PRESETS,
  LOAD_PRESET,
  MIDI_EVENT_ADD,
  MIDI_EVENT_ADD_DELAYED,
  MIDI_CHORD_EVENT_ADD,
  MIDI_CHORD_EVENT_ADD_DELAYED,
  MIDI_EVENT_CLEAR,
  MIDI_INIT,
  MIXER_UPDATE,
  MONITOR,
  NO_ACTION,
  PREVIEW,
  RAND,
  SAVE_PRESET,
  STATUS,
  SOLO,
  STOP,
  UNSOLO,
  UPDATE,
};

struct audio_action_queue_item {
  AudioAction type;
  int mixer_soundgen_idx{-1};
  std::shared_ptr<SBAudio::SoundGenerator> sg;

  std::vector<double> buffer;
  AudioBufferDetails audio_buffer_details;

  int delayed_by{0};  // in midi ticks
  int start_at{0};    // in midi ticks

  // ADD varz
  unsigned int soundgenerator_type;
  std::string filepath;  // used for sample and digisynth
  bool loop_mode;        // for looper

  // STATUS varz
  bool status_all{false};

  // UPDATE varz
  int fx_id{0};
  std::string param_name{};
  std::string param_val{0};

  // NOTE_ON varz
  std::vector<std::shared_ptr<object::Object>> args;
  int soundgen_num;
  std::vector<int> notes;
  int velocity;
  int duration;
  int note_start_time;

  // PREVIEW varz
  std::string preview_filename;

  // ADD_FX varz
  // PRESET varz

  // BPM varz
  double new_bpm;
};
