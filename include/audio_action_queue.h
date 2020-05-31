#include <string>
#include <vector>

#include <interpreter/object.hpp>
#include <soundgenerator.h>

enum AudioAction
{
    NO_ACTION,
    ADD,
    ADD_FX,
    BPM,
    INFO,
    HELP,
    LOAD_PRESET,
    MIDI_INIT,
    NOTE_ON,
    PREVIEW,
    RAND,
    SAVE_PRESET,
    SPEED,
    STATUS,
    UPDATE,
    MIXER_UPDATE,
};

struct audio_action_queue_item
{
    AudioAction type;
    int mixer_soundgen_idx{0};

    // ADD varz
    std::shared_ptr<SoundGenerator> sg{nullptr};

    // STATUS varz
    bool status_all{false};

    // UPDATE varz
    int fx_id{0};
    std::string param_name{};
    std::string param_val{0};

    // NOTE_ON varz
    std::vector<std::shared_ptr<object::Object>> args;

    // PREVIEW varz
    std::string preview_filename;

    // ADD_FX varz
    // PRESET varz

    // BPM varz
    double new_bpm;
};
