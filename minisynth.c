#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "midi_freq_table.h"
#include "minisynth.h"
#include "mixer.h"
#include "utils.h"

extern mixer *mixr;
extern const wchar_t *sparkchars;

// defined in minisynth_voice.h
const wchar_t *s_mode_names[] = {L"SAW3", L"SQR3", L"SAW2SQR", L"TRI2SAW",
                                 L"TRI2SQR"};

minisynth *new_minisynth(void)
{
    minisynth *ms = (minisynth *)calloc(1, sizeof(minisynth));
    if (ms == NULL)
        return NULL; // barf

    // ms->vol = 0.7;
    ms->sustain = 0;
    ms->num_melodies = 1;

    ms->sound_generator.gennext = &minisynth_gennext;
    ms->sound_generator.status = &minisynth_status;
    ms->sound_generator.setvol = &minisynth_setvol;
    ms->sound_generator.getvol = &minisynth_getvol;
    ms->sound_generator.type = SYNTH_TYPE;

    ms->m_voice_mode = 0;
    ms->m_detune_cents = 0.0;
    ms->m_lfo1_amplitude = 1.0;
    ms->m_lfo1_rate = DEFAULT_LFO_RATE;
    ms->m_fc_control = FILTER_FC_DEFAULT;
    ms->m_q_control = FILTER_Q_DEFAULT;
    ms->m_attack_time_msec = EG_DEFAULT_STATE_TIME;
    ms->m_delay_time_msec = EG_DEFAULT_STATE_TIME;
    ms->m_decay_release_time_msec = EG_DEFAULT_STATE_TIME;
    ms->m_pulse_width_pct = OSC_PULSEWIDTH_DEFAULT;
    ms->m_feedback_pct = 0;
    ms->m_delay_ratio = 0;
    ms->m_wet_mix = 0.0;
    ms->m_octave = 0;
    ms->m_portamento_time_msec = DEFAULT_PORTAMENTO_TIME_MSEC;
    ms->m_lfo1_osc_pitch_intensity = 0.0;
    ms->m_sub_osc_db = -96.000000;
    ms->m_eg1_osc_intensity = 0.0;
    ms->m_eg1_filter_intensity = 0.0;
    ms->m_lfo1_filter_fc_intensity = 0.0;
    // ms->m_sustain_level = 0.510000;
    ms->m_sustain_level = 0.9;
    ms->m_noise_osc_db = -96.000000;
    ms->m_lfo1_amp_intensity = 0.0;
    ms->m_lfo1_pan_intensity = 0.0;
    ms->m_eg1_dca_intensity = 1.0;
    ms->m_lfo1_waveform = 0;
    ms->m_volume_db = 0.7;
    ms->m_legato_mode = DEFAULT_LEGATO_MODE;
    ms->m_pitchbend_range = 1;
    ms->m_reset_to_zero = DEFAULT_RESET_TO_ZERO;
    ms->m_filter_keytrack = DEFAULT_FILTER_KEYTRACK;
    ms->m_filter_keytrack_intensity = DEFAULT_FILTER_KEYTRACK_INTENSITY;
    ms->m_velocity_to_attack_scaling = 0;
    ms->m_note_number_to_decay_scaling = 0;
    ms->m_delay_mode = 0;
    ms->m_eg1_dca_intensity = 1.0;
    ms->m_sustain_override = false;
    ms->m_last_midi_note = 0;
    ms->m_morph_mode = false;

    for (int i = 0; i < MAX_VOICES; i++) {
        ms->m_voices[i] = new_minisynth_voice();
        if (!ms->m_voices[i])
            return NULL; // would be bad

        minisynth_voice_init_global_parameters(ms->m_voices[i],
                                               &ms->m_global_synth_params);
    }

    // clears out momatric sources and resets all oscs, lfos, eg's etc.
    minisynth_prepare_for_play(ms);

    // use first voice to setup global
    minisynth_voice_initialize_modmatrix(ms->m_voices[0], &ms->m_ms_modmatrix);

    for (int i = 0; i < MAX_VOICES; i++) {
        voice_set_modmatrix_core(&ms->m_voices[i]->m_voice,
                                 get_matrix_core(&ms->m_ms_modmatrix));
    }
    for (int i = 0; i < MAX_NUM_MIDI_LOOPS; i++) {
        ms->melody_multiloop_count[i] = 1;
        for (int j = 0; j < PPNS; j++) {
            ms->melodies[i][j] = NULL;
        }
    }

    minisynth_update(ms);
    arpeggiator_init(&ms->m_arp);

    limiter_init(&ms->m_limiter, 10.0, 500.); // attack time, decay

    return ms;
}

////////////////////////////////////

bool minisynth_prepare_for_play(minisynth *ms)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (ms->m_voices[i]) {
            minisynth_voice_prepare_for_play(ms->m_voices[i]);
        }
    }

    stereo_delay_prepare_for_play(&ms->m_delay_fx);

    minisynth_update(ms);
    ms->m_last_note_frequency = -1.0;

    return true;
}

void minisynth_update(minisynth *ms)
{
    ms->m_global_synth_params.voice_params.voice_mode = ms->m_voice_mode;
    ms->m_global_synth_params.voice_params.portamento_time_msec =
        ms->m_portamento_time_msec;

    ms->m_global_synth_params.voice_params.osc_fo_pitchbend_mod_range =
        ms->m_pitchbend_range;

    // --- intensities
    ms->m_global_synth_params.voice_params.filter_keytrack_intensity =
        ms->m_filter_keytrack_intensity;
    ms->m_global_synth_params.voice_params.lfo1_filter1_mod_intensity =
        ms->m_lfo1_filter_fc_intensity;
    ms->m_global_synth_params.voice_params.lfo1_osc_mod_intensity =
        ms->m_lfo1_osc_pitch_intensity;
    ms->m_global_synth_params.voice_params.lfo1_dca_amp_mod_intensity =
        ms->m_lfo1_amp_intensity;
    ms->m_global_synth_params.voice_params.lfo1_dca_pan_mod_intensity =
        ms->m_lfo1_pan_intensity;

    ms->m_global_synth_params.voice_params.eg1_osc_mod_intensity =
        ms->m_eg1_osc_intensity;
    ms->m_global_synth_params.voice_params.eg1_filter1_mod_intensity =
        ms->m_eg1_filter_intensity;
    ms->m_global_synth_params.voice_params.eg1_dca_amp_mod_intensity =
        ms->m_eg1_dca_intensity;

    // --- oscillators:
    double noise_amplitude = ms->m_noise_osc_db == -96.0
                                 ? 0.0
                                 : pow(10.0, ms->m_noise_osc_db / 20.0);
    double sub_amplitude =
        ms->m_sub_osc_db == -96.0 ? 0.0 : pow(10.0, ms->m_sub_osc_db / 20.0);

    // --- osc3 is sub osc
    ms->m_global_synth_params.osc3_params.amplitude = sub_amplitude;

    // --- osc4 is noise osc
    ms->m_global_synth_params.osc4_params.amplitude = noise_amplitude;

    // --- pulse width
    ms->m_global_synth_params.osc1_params.pulse_width_control =
        ms->m_pulse_width_pct;
    ms->m_global_synth_params.osc2_params.pulse_width_control =
        ms->m_pulse_width_pct;
    ms->m_global_synth_params.osc3_params.pulse_width_control =
        ms->m_pulse_width_pct;

    // --- octave
    ms->m_global_synth_params.osc1_params.octave = ms->m_octave;
    ms->m_global_synth_params.osc2_params.octave = ms->m_octave;
    ms->m_global_synth_params.osc3_params.octave =
        ms->m_octave - 1; // sub-oscillator

    // --- detuning for minisynth
    ms->m_global_synth_params.osc1_params.cents = ms->m_detune_cents;
    ms->m_global_synth_params.osc2_params.cents = -ms->m_detune_cents;
    // no detune on 3rd oscillator

    // --- filter:
    ms->m_global_synth_params.filter1_params.fc_control = ms->m_fc_control;
    ms->m_global_synth_params.filter1_params.q_control = ms->m_q_control;

    // --- lfo1:
    ms->m_global_synth_params.lfo1_params.waveform = ms->m_lfo1_waveform;
    ms->m_global_synth_params.lfo1_params.amplitude = ms->m_lfo1_amplitude;
    ms->m_global_synth_params.lfo1_params.osc_fo = ms->m_lfo1_rate;

    // --- eg1:
    ms->m_global_synth_params.eg1_params.attack_time_msec =
        ms->m_attack_time_msec;
    ms->m_global_synth_params.eg1_params.decay_time_msec =
        ms->m_decay_release_time_msec;
    ms->m_global_synth_params.eg1_params.sustain_level = ms->m_sustain_level;
    ms->m_global_synth_params.eg1_params.release_time_msec =
        ms->m_decay_release_time_msec;
    ms->m_global_synth_params.eg1_params.reset_to_zero =
        (bool)ms->m_reset_to_zero;
    ms->m_global_synth_params.eg1_params.legato_mode = (bool)ms->m_legato_mode;

    // --- dca:
    ms->m_global_synth_params.dca_params.amplitude_db = ms->m_volume_db;

    // --- enable/disable mod matrix stuff
    if (ms->m_velocity_to_attack_scaling == 1)
        enable_matrix_row(&ms->m_ms_modmatrix, SOURCE_VELOCITY,
                          DEST_ALL_EG_ATTACK_SCALING, true); // enable
    else
        enable_matrix_row(&ms->m_ms_modmatrix, SOURCE_VELOCITY,
                          DEST_ALL_EG_ATTACK_SCALING, false);

    if (ms->m_note_number_to_decay_scaling == 1)
        enable_matrix_row(&ms->m_ms_modmatrix, SOURCE_MIDI_NOTE_NUM,
                          DEST_ALL_EG_DECAY_SCALING, true); // enable
    else
        enable_matrix_row(&ms->m_ms_modmatrix, SOURCE_MIDI_NOTE_NUM,
                          DEST_ALL_EG_DECAY_SCALING, false);

    if (ms->m_filter_keytrack == 1)
        enable_matrix_row(&ms->m_ms_modmatrix, SOURCE_MIDI_NOTE_NUM,
                          DEST_ALL_FILTER_KEYTRACK, true); // enable
    else
        enable_matrix_row(&ms->m_ms_modmatrix, SOURCE_MIDI_NOTE_NUM,
                          DEST_ALL_FILTER_KEYTRACK, false);

    // // --- update master FX delay
    stereo_delay_set_delay_time_ms(&ms->m_delay_fx, ms->m_delay_time_msec);
    stereo_delay_set_feedback_percent(&ms->m_delay_fx, ms->m_feedback_pct);
    stereo_delay_set_delay_ratio(&ms->m_delay_fx, ms->m_delay_ratio);
    stereo_delay_set_wet_mix(&ms->m_delay_fx, ms->m_wet_mix);
    stereo_delay_set_mode(&ms->m_delay_fx, ms->m_delay_mode);
    stereo_delay_update(&ms->m_delay_fx);
}

bool minisynth_midi_note_on(minisynth *ms, unsigned int midinote,
                            unsigned int velocity)
{
    bool steal_note = true;
    for (int i = 0; i < MAX_VOICES; i++) {
        minisynth_voice *msv = ms->m_voices[i];
        if (!msv)
            return false; // should never happen
        if (!msv->m_voice.m_note_on) {
            minisynth_increment_voice_timestamps(ms);
            voice_note_on(&msv->m_voice, midinote, velocity,
                          get_midi_freq(midinote), ms->m_last_note_frequency);

            ms->m_last_note_frequency = get_midi_freq(midinote);
            steal_note = false;
            break;
        }
    }

    if (steal_note) {
        if (mixr->debug_mode)
            printf("STEAL NOTE\n");
        minisynth_voice *msv = minisynth_get_oldest_voice(ms);
        if (msv) {
            minisynth_increment_voice_timestamps(ms);
            voice_note_on(&msv->m_voice, midinote, velocity,
                          get_midi_freq(midinote), ms->m_last_note_frequency);
        }
        ms->m_last_note_frequency = get_midi_freq(midinote);
    }

    return true;
}

bool minisynth_midi_note_off(minisynth *ms, unsigned int midinote,
                             unsigned int velocity, bool all_notes_off)
{
    (void)velocity;

    if (all_notes_off) {
        for (int i = 0; i < MAX_VOICES; i++) {
            if (ms->m_voices[i])
                voice_note_off(&ms->m_voices[i]->m_voice, midinote);
        }
    }

    for (int i = 0; i < MAX_VOICES; i++) {
        minisynth_voice *msv =
            minisynth_get_oldest_voice_with_note(ms, midinote);
        if (msv) {
            voice_note_off(&msv->m_voice, midinote);
        }
    }
    return true;
}

void minisynth_midi_control(minisynth *ms, unsigned int data1,
                            unsigned int data2)
{
    double scaley_val;
    // switch (mixr->m_midi_controller_mode) {
    // case MIDI_KNOB_MODE_ONE:
    switch (data1) {
    case 9:
        ms->m_lfo1_waveform = (++ms->m_lfo1_waveform) % MAX_LFO_OSC;
        printf("LFO! Mode Toggle: %d MaxLFO: %d\n", ms->m_lfo1_waveform,
               MAX_LFO_OSC);
        break;
    case 10:
        ms->m_voice_mode = (++ms->m_voice_mode) % 5;
        printf("Voice! Mode Toggle - %ls\n", s_mode_names[ms->m_voice_mode]);
        break;
    case 11:
        ms->m_legato_mode = 1 - ms->m_legato_mode;
        printf("Legato! Mode %d\n", ms->m_legato_mode);
        break;
    case 12:
        ms->m_reset_to_zero = 1 - ms->m_reset_to_zero;
        printf("Reset To Zero! Mode\n");
        break;
    case 13:
        ms->m_filter_keytrack = 1 - ms->m_filter_keytrack;
        printf("Filter Keytrack! Mode\n");
        break;
    case 14:
        ms->m_velocity_to_attack_scaling = 1 - ms->m_velocity_to_attack_scaling;
        printf("Velocity To Attack! Mode %d\n",
               ms->m_velocity_to_attack_scaling);
        break;
    case 15:
        ms->m_note_number_to_decay_scaling =
            1 - ms->m_note_number_to_decay_scaling;
        printf("Note To Decay Scaling! Mode %d\n",
               ms->m_note_number_to_decay_scaling);
        break;
    case 16:
        printf("Toggle! MIDI Knob Modee!\n");
        break;
    /// BANK B on MPK Mini MKII
    case 17:
        printf("Delay Mode! Mode\n");
        break;
    case 18:
        printf("Sustain Override! Mode\n");
        break;
    case 19:
        printf("19! \n");
        break;
    case 20:
        printf("20! Mode\n");
        break;
    case 21:
        printf("21! MIDI Mode\n");
        break;
    case 22:
        printf("22! MIDI Mode\n");
        break;
    case 23:
        printf("23! MIDI Mode\n");
        break;
    case 24:
        printf("24! MIDI Mode\n");
        break;
    case 1: // K1 - Envelope Attack Time Msec
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            if (mixr->debug_mode)
                printf("Envelope attack!\n");
            scaley_val = scaleybum(0, 127, EG_MINTIME_MS, EG_MAXTIME_MS, data2);
            ms->m_attack_time_msec = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
            scaley_val = scaleybum(0, 127, 0, 1000, data2);
            ms->m_delay_time_msec = scaley_val;
            if (mixr->debug_mode)
                printf("DELAY MS: %f\n", ms->m_delay_time_msec);
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
            if (mixr->debug_mode)
                printf("EG1->DCA intensity!\n");
            scaley_val = scaleybum(0, 127, 0, 10, data2);
            ms->m_eg1_dca_intensity = scaley_val;
        }
        break;
    case 2: // K2 - Envelope Decay Time Msec
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            if (mixr->debug_mode)
                printf("Envelope decay!\n");
            scaley_val = scaleybum(0, 127, EG_MINTIME_MS, EG_MAXTIME_MS, data2);
            ms->m_decay_release_time_msec = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
            scaley_val = scaleybum(0, 127, 0, 10, data2);
            // ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
            // scaley_val = scaleybum(0, 127, X, X, data2);
            // ms->X = scaley_val;
        }
        break;
    case 3: // K3 - Envelope Sustain Level
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            if (mixr->debug_mode)
                printf("Env Sustain level\n");
            scaley_val = scaleybum(0, 127, 0, 1, data2);
            ms->m_sustain_level = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
            // scaley_val = scaleybum(0, 127, X, X, data2);
            // ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
            // scaley_val = scaleybum(0, 127, X, X, data2);
            // ms->X = scaley_val;
        }
        break;
    case 4: // K4 - Synth Volume
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            printf("Synth volume\n");
            scaley_val = scaleybum(0, 127, 0, 1, data2);
            ms->m_volume_db = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
            //    scaley_val = scaleybum(0, 127, X, X, data2);
            //    ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
            //    scaley_val = scaleybum(0, 127, X, X, data2);
            //    ms->X = scaley_val;
        }
        break;
    case 5: // K6 - LFO amplitude
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            if (mixr->debug_mode)
                printf("LFO Amplitude\n");
            scaley_val = scaleybum(0, 128, 0.0, 1.0, data2);
            ms->m_lfo1_amplitude = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
            //    scaley_val = scaleybum(0, 127, X, X, data2);
            //    ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
            //    scaley_val = scaleybum(0, 127, X, X, data2);
            //    ms->X = scaley_val;
        }
        break;
    case 6: // K5 - LFO rate
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            if (mixr->debug_mode)
                printf("LFO Rate\n");
            scaley_val = scaleybum(0, 128, MIN_LFO_RATE, MAX_LFO_RATE, data2);
            ms->m_lfo1_rate = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
            //    scaley_val = scaleybum(0, 127, X, X, data2);
            //    ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
            //    scaley_val = scaleybum(0, 127, X, X, data2);
            //    ms->X = scaley_val;
        }
        break;
    case 7: // K7 - Filter Frequency Cut
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            if (mixr->debug_mode)
                printf("Filter Frequency!\n");
            scaley_val = scaleybum(0, 127, FILTER_FC_MIN, FILTER_FC_MAX, data2);
            ms->m_fc_control = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
            //    scaley_val = scaleybum(0, 127, X, X, data2);
            //    ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
            //    scaley_val = scaleybum(0, 127, X, X, data2);
            //    ms->X = scaley_val;
        }
        break;
    case 8: // K8 - Filter Q control
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            if (mixr->debug_mode)
                printf("Filter Q!\n");
            scaley_val = scaleybum(0, 127, 1, 10, data2);
            ms->m_q_control = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
            //    scaley_val = scaleybum(0, 127, X, X, data2);
            //    ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
            //    scaley_val = scaleybum(0, 127, X, X, data2);
            //    ms->X = scaley_val;
        }
        break;
    default:
        printf("DATANUM: %d\n", data1);
    }

    minisynth_update(ms);
}

void minisynth_midi_pitchbend(minisynth *ms, unsigned int data1,
                              unsigned int data2)
{
    // printf("Pitch bend, babee: %d %d\n", data1, data2);
    int actual_pitch_bent_val = (int)((data1 & 0x7F) | ((data2 & 0x7F) << 7));

    if (actual_pitch_bent_val != 8192) {
        double normalized_pitch_bent_val =
            (float)(actual_pitch_bent_val - 0x2000) / (float)(0x2000);
        double scaley_val =
            // scaleybum(0, 16383, -100, 100, normalized_pitch_bent_val);
            scaleybum(0, 16383, -600, 600, actual_pitch_bent_val);
        // printf("Cents to bend - %f\n", scaley_val);
        for (int i = 0; i < MAX_VOICES; i++) {
            ms->m_voices[i]->m_voice.m_osc1->m_cents = scaley_val;
            ms->m_voices[i]->m_voice.m_osc2->m_cents = scaley_val + 2.5;
            ms->m_voices[i]->m_voice.m_osc3->m_cents = scaley_val;
            ms->m_voices[i]->m_voice.m_osc4->m_cents = scaley_val + 2.5;
            ms->m_voices[i]->m_voice.m_v_modmatrix.m_sources[SOURCE_PITCHBEND] =
                normalized_pitch_bent_val;
        }
    }
    else {
        for (int i = 0; i < MAX_VOICES; i++) {
            ms->m_voices[i]->m_voice.m_osc1->m_cents = 0;
            ms->m_voices[i]->m_voice.m_osc2->m_cents = 2.5;
            ms->m_voices[i]->m_voice.m_osc3->m_cents = 0;
            ms->m_voices[i]->m_voice.m_osc4->m_cents = 2.5;
        }
    }
}

void minisynth_set_multi_melody_mode(minisynth *ms, bool melody_mode)
{
    ms->multi_melody_mode = melody_mode;
    ms->cur_melody_iteration = ms->melody_multiloop_count[ms->cur_melody];
}

void minisynth_set_melody_loop_num(minisynth *self, int melody_num,
                                   int loop_num)
{
    self->melody_multiloop_count[melody_num] = loop_num;
}

void minisynth_add_melody(minisynth *ms)
{
    ms->num_melodies++;
    ms->cur_melody++;
}

void minisynth_dupe_melody(minisynth *ms)
{
    midi_event **melody = minisynth_copy_midi_loop(ms, ms->cur_melody);
    minisynth_add_midi_loop(ms, melody, ms->cur_melody + 1);
}

void minisynth_switch_melody(minisynth *ms, unsigned int melody_num)
{
    if (melody_num < (unsigned)ms->num_melodies) {
        ms->cur_melody = melody_num;
    }
}

void minisynth_reset_melody_all(minisynth *ms)
{
    for (int i = 0; i < ms->num_melodies; i++) {
        minisynth_reset_melody(ms, i);
    }
}

void minisynth_reset_voices(minisynth *ms)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        minisynth_voice_reset(ms->m_voices[i]);
    }
}

void minisynth_reset_melody(minisynth *ms, unsigned int melody_num)
{
    if (melody_num < (unsigned)ms->num_melodies) {
        for (int i = 0; i < PPNS; i++) {
            if (ms->melodies[melody_num][i] != NULL) {
                midi_event *tmp = ms->melodies[melody_num][i];
                ms->melodies[melody_num][i] = NULL;
                free(tmp);
            }
        }
    }
}

void minisynth_melody_to_string(minisynth *ms, int melody_num,
                                wchar_t melodystr[33])
{
    int cur_quart = 0;
    for (int i = 0; i < PPNS; i += PPS) {
        melodystr[cur_quart] = sparkchars[0];
        for (int j = i; j < (i + PPS); j++) {
            if (ms->melodies[melody_num][j] != NULL &&
                ms->melodies[melody_num][j]->event_type ==
                    144) { // 144 is midi note on
                melodystr[cur_quart] = sparkchars[5];
            }
        }
        cur_quart++;
    }
}

void minisynth_increment_voice_timestamps(minisynth *ms)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (ms->m_voices[i]) {
            if (ms->m_voices[i]->m_voice.m_note_on)
                ms->m_voices[i]->m_voice.m_timestamp++;
        }
    }
}

minisynth_voice *minisynth_get_oldest_voice(minisynth *ms)
{
    int timestamp = -1;
    minisynth_voice *found_voice = NULL;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (ms->m_voices[i]) {
            if (ms->m_voices[i]->m_voice.m_note_on &&
                (int)ms->m_voices[i]->m_voice.m_timestamp > timestamp) {
                found_voice = ms->m_voices[i];
                timestamp = (int)ms->m_voices[i]->m_voice.m_timestamp;
            }
        }
    }
    return found_voice;
}

minisynth_voice *minisynth_get_oldest_voice_with_note(minisynth *ms,
                                                      unsigned int midi_note)
{
    int timestamp = -1;
    minisynth_voice *found_voice = NULL;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (ms->m_voices[i]) {
            if (voice_can_note_off(&ms->m_voices[i]->m_voice) &&
                (int)ms->m_voices[i]->m_voice.m_timestamp > timestamp &&
                ms->m_voices[i]->m_voice.m_midi_note_number == midi_note) {
                found_voice = ms->m_voices[i];
                timestamp = (int)ms->m_voices[i]->m_voice.m_timestamp;
            }
        }
    }
    return found_voice;
}

// sound generator interface //////////////
void minisynth_status(void *self, wchar_t *status_string)
{
    minisynth *ms = (minisynth *)self;

    if (mixr->debug_mode) {
        for (int i = 0; i < ms->num_melodies; i++) {
            printf("Melody %d\n", i);
            for (int j = 0; j < PPNS; j++) {
                if (ms->melodies[i][j] != NULL)
                    print_midi_event_rec(ms->melodies[i][j]);
            }
        }
    }

    // TODO - a shit load of error checking on boundaries and size
    swprintf(
        status_string, MAX_PS_STRING_SZ, WCOOL_COLOR_PINK
        "[SYNTH] - Vol: %.2f Multi: %s, Morph: %s, CurMelody:%d DelayMode: %d Mode: %ls"
        "\n      A:%.2f D/R:%.2f S:%.2f Amp: %2.f LFO1 amp: %.2f rate:%.2f "
        "Filter FC: %.2f Filter Q: %2.f"
        "\n      Delay ms: %.2f Feedback Pct:%.2f Delay Ratio: %.2f Wet Mix: "
        "%2.f"
        "\n      Detune Cents: %.2f Pulse Width Pct:%.2f SubOsc Db: %.2f "
        "NoiseOsc Db: %2.f",
        ms->m_volume_db, ms->multi_melody_mode ? "true" : "false",
        ms->m_morph_mode ? "true" : "false",
        ms->cur_melody, ms->m_delay_mode, s_mode_names[ms->m_voice_mode],
        ms->m_attack_time_msec, ms->m_decay_release_time_msec,
        ms->m_sustain_level, ms->m_volume_db, ms->m_lfo1_amplitude,
        ms->m_lfo1_rate, ms->m_fc_control, ms->m_q_control,
        ms->m_delay_time_msec, ms->m_feedback_pct, ms->m_delay_ratio,
        ms->m_wet_mix, ms->m_detune_cents, ms->m_pulse_width_pct,
        ms->m_sub_osc_db, ms->m_noise_osc_db);

    for (int i = 0; i < ms->num_melodies; i++) {
        wchar_t melodystr[33] = {0};
        wchar_t scratch[128] = {0};
        minisynth_melody_to_string(ms, i, melodystr);
        swprintf(scratch, 127, L"\n      [%d]  %ls  numloops: %d", i, melodystr,
                 ms->melody_multiloop_count[i]);
        wcscat(status_string, scratch);
    }
    wcscat(status_string, WANSI_COLOR_RESET);
}

void minisynth_setvol(void *self, double v)
{
    minisynth *ms = (minisynth *)self;
    if (v < 0.0 || v > 1.0) {
        return;
    }
    ms->m_volume_db = v;
}

double minisynth_getvol(void *self)
{
    minisynth *ms = (minisynth *)self;
    return ms->m_volume_db;
}

// void minisynth_gennext(void* self, double* frame_vals, int framesPerBuffer);
double minisynth_gennext(void *self)
{
    minisynth *ms = (minisynth *)self;
    if (ms->m_morph_mode) {
        ms->m_fc_control = fmod(ms->m_fc_control + 1, 20000) + 5000;
    }
    minisynth_update(ms);

    if (mixr->is_midi_tick) {
        minisynth_play_melody(ms);
    }

    if (ms->m_arp.active) {
        arpeggiate(ms, &ms->m_arp);
    }
    double accum_out_left = 0.0;
    double accum_out_right = 0.0;

    float mix = 0.25;

    double out_left = 0.0;
    double out_right = 0.0;

    for (int i = 0; i < MAX_VOICES; i++) {
        if (ms->m_voices[i]) {
            minisynth_voice_gennext(ms->m_voices[i], &out_left, &out_right);
        }
        accum_out_left += mix * out_left;
        accum_out_right += mix * out_right;
    }

    stereo_delay_process_audio(&ms->m_delay_fx, &accum_out_left,
                               &accum_out_left, &accum_out_left,
                               &accum_out_right);

    accum_out_left = effector(&ms->sound_generator, accum_out_left);
    accum_out_left = envelopor(&ms->sound_generator, accum_out_left);

    accum_out_left *= ms->m_volume_db;

    return limiter_process(&ms->m_limiter, &accum_out_left);
}

midi_event **minisynth_get_midi_loop(minisynth *self)
{
    return self->melodies[self->cur_melody];
}

void minisynth_add_event(minisynth *ms, midi_event *ev)
{
    int tick = ev->tick;
    while (ms->melodies[ms->cur_melody][tick] != NULL) {
        if (mixr->debug_mode)
            printf("Gotsz a tick already - bump!\n");
        tick++;
        if (tick == PPNS) // wrap around
            tick = 0;
    }
    ev->tick = tick;
    ms->melodies[ms->cur_melody][tick] = ev;
}

void minisynth_delete_event(minisynth *ms, int pat_num, int tick)
{
    if (is_valid_melody_num(ms, pat_num)) {
        if (ms->melodies[ms->cur_melody][tick] != NULL) {
            midi_event *ev = ms->melodies[ms->cur_melody][tick];
            ms->melodies[ms->cur_melody][tick] = NULL;
            free(ev);
            printf("Deleted midi event at tick %d\n", tick);
        }
        else {
            printf("Not a valid midi event at tick: %d\n", tick);
        }
    }
    else {
        printf("Not a valid pattern num: %d \n", pat_num);
    }
}

midi_event **minisynth_copy_midi_loop(minisynth *self, int melody_num)
{
    if (melody_num >= self->num_melodies) {
        printf("Dingjie!\n");
        return NULL;
    }
    // midi_event_loop defined in midimaaan.h
    midi_event **new_midi_events_loop =
        (midi_event **)calloc(PPNS, sizeof(midi_event *));
    for (int i = 0; i < PPNS; i++) {
        if (self->melodies[melody_num][i] != NULL) {
            midi_event *ev = self->melodies[melody_num][i];
            new_midi_events_loop[i] =
                new_midi_event(ev->tick, ev->event_type, ev->data1, ev->data2);
        }
    }

    return new_midi_events_loop;
}

void minisynth_add_midi_loop(minisynth *ms, midi_event **events, int melody_num)
{
    if (melody_num >= MAX_NUM_MIDI_LOOPS) {
        printf("Dingjie!\n");
        return;
    }
    for (int i = 0; i < PPNS; i++) {
        if (events[i] != NULL)
            ms->melodies[melody_num][i] = events[i];
    }
    ms->num_melodies++;
    ms->cur_melody++;
    free(events); // get rid of container
    printf("Added new Melody\n");
}

void minisynth_replace_midi_loop(minisynth *ms, midi_event **events,
                                 int melody_num)
{
    if (melody_num >= MAX_NUM_MIDI_LOOPS) {
        printf("Dingjie!\n");
        return;
    }
    for (int i = 0; i < PPNS; i++) {
        if (ms->melodies[melody_num][i] != NULL) {
            free(ms->melodies[melody_num][i]);
            ms->melodies[melody_num][i] = NULL;
        }
        if (events[i] != NULL)
            ms->melodies[melody_num][i] = events[i];
    }
    free(events); // get rid of container
    printf("Replaced Melody %d\n", melody_num);
}

void minisynth_toggle_delay_mode(minisynth *ms)
{
    ms->m_delay_mode = ++(ms->m_delay_mode) % MAX_NUM_DELAY_MODE;
}

void minisynth_set_sustain_override(minisynth *ms, bool b)
{
    ms->m_sustain_override = b;

    for (int i = 0; i < MAX_VOICES; i++) {
        if (ms->m_voices[i]) {
            voice_set_sustain_override(&ms->m_voices[i]->m_voice, b);
        }
    }
}

void minisynth_nudge_melody(minisynth *ms, int melody_num, int sixteenth)
{
    if (sixteenth >= 16) {
        printf("Nah, mate, nudge needs to be less than 16\n");
        return;
    }
    int sixteenth_of_loop = PPNS / 16.0;
    midi_event **orig_loop = minisynth_copy_midi_loop(ms, melody_num);

    midi_event **new_midi_events_loop =
        (midi_event **)calloc(PPNS, sizeof(midi_event *));

    for (int i = 0; i < PPNS; i++) {
        if (orig_loop[i] != NULL) {
            midi_event *ev = orig_loop[i];
            int new_tick = (ev->tick + (sixteenth * sixteenth_of_loop)) % PPNS;
            printf("Old tick: %d with new: %d\n", ev->tick, new_tick);
            new_midi_events_loop[new_tick] =
                new_midi_event(new_tick, ev->event_type, ev->data1, ev->data2);
        }
    }
    free(orig_loop);
    minisynth_replace_midi_loop(ms, new_midi_events_loop, melody_num);
}

bool is_valid_melody_num(minisynth *ms, int melody_num)
{
    if (melody_num < ms->num_melodies) {
        return true;
    }
    return false;
}

// TODO - better function name - this is programatic calls, which
// basically adds a matching delete after use event i.e. == a note off
void minisynth_handle_midi_note(minisynth *ms, int note, int velocity,
                                bool update_last_midi)
{
    if (mixr->debug_mode)
        print_midi_event(note);

    if (update_last_midi) {
        ms->m_last_midi_note = note;
    }
    minisynth_midi_note_on(ms, note, velocity);

    int note_off_tick = (mixr->tick + (PPS * 4 - 7)) % PPNS;

    midi_event *off_event = new_midi_event(note_off_tick, 128, note, velocity);
    ////////////////////////

    if (ms->recording) {
        printf("Recording note!\n");
        int note_on_tick = mixr->tick % PPNS;
        midi_event *on_event =
            new_midi_event(note_on_tick, 144, note, velocity);

        minisynth_add_event(ms, on_event);
        minisynth_add_event(ms, off_event);
    }
    else {
        off_event->delete_after_use = true; // _THIS_ is the magic
        minisynth_add_event(ms, off_event);
    }
}

void minisynth_rand_settings(minisynth *ms)
{
    printf("Randomizing SYNTH!\n");
    int rand_ = 0;
    double scaley_val;

    ms->m_voice_mode = rand() % 4;
    ms->m_detune_cents = ((float)rand()) / RAND_MAX;
    ms->m_lfo1_amplitude = ((float)rand()) / RAND_MAX;
    ms->m_lfo1_rate =
        ((float)rand()) / RAND_MAX * (MAX_LFO_RATE - MIN_LFO_RATE) +
        MIN_LFO_RATE;
    ms->m_fc_control =
        ((float)rand()) / RAND_MAX * (FILTER_FC_MAX - FILTER_FC_MIN) +
        FILTER_FC_MIN;
    ms->m_q_control = ((float)rand()) / RAND_MAX;
    ms->m_attack_time_msec = ((float)rand()) / RAND_MAX * EG_MAXTIME_MS;
    ms->m_decay_release_time_msec = ((float)rand()) / RAND_MAX * EG_MAXTIME_MS;
    ms->m_pulse_width_pct = (((float)rand() / (float)(RAND_MAX)) * 96) + 2;

    // rand_ = (rand() % 127) + 1;
    // scaley_val = scaleybum(0, 127, -0.9, 0.9, rand_);
    // ms->m_delay_ratio = scaley_val;
    // ms->m_delay_time_msec = ((float)rand()) / RAND_MAX * 2000;
    // ms->m_feedback_pct = (float)(rand() % 200) - 100;
    // ms->m_wet_mix = ((float)rand() / (float)(RAND_MAX)) * 100;

    // ms->m_octave = rand() % 4 - 2;

    // ms->m_portamento_time_msec = ((float)rand() / (float)(RAND_MAX)) * 10;
    // ms->m_lfo1_osc_pitch_intensity =
    //    (((float)rand() / (float)(RAND_MAX)) * 2) - 1;
    // ms->m_sub_osc_db = -1.0 * (rand() % 96);
    // ms->m_eg1_osc_intensity = (rand() % 2) + 1;
    // ms->m_eg1_filter_intensity = (rand() % 2) + 1;
    // ms->m_lfo1_filter_fc_intensity = (rand() % 2) + 1;
    //// ms->m_sustain_level = 0.9;
    ms->m_noise_osc_db = -1.0 * (rand() % 96);
    ms->m_lfo1_amp_intensity = ((float)rand() / (float)(RAND_MAX));
    ms->m_lfo1_pan_intensity = ((float)rand() / (float)(RAND_MAX));
    ms->m_eg1_dca_intensity = ((float)rand() / (float)(RAND_MAX));
    ms->m_lfo1_waveform = rand() % MAX_LFO_OSC;
    //// ms->m_volume_db = 1.0;
    // ms->m_legato_mode = rand() % 2;
    // ms->m_pitchbend_range = rand() % 12;
    ms->m_reset_to_zero = rand() % 2;
    // ms->m_filter_keytrack = rand() % 2;
    // ms->m_filter_keytrack_intensity =
    //    (((float)rand() / (float)(RAND_MAX)) * 10) + 0.51;
    ms->m_velocity_to_attack_scaling = rand() % 2;
    ms->m_note_number_to_decay_scaling = rand() % 2;
    ms->m_delay_mode = rand() % MAX_NUM_DELAY_MODE;
    ms->m_eg1_dca_intensity = (rand() % 2) + 1;
    ms->m_sustain_override = rand() % 2;

    minisynth_print_settings(ms);
}

bool minisynth_save_settings(minisynth *ms, char *preset_name)
{
    if (strlen(preset_name) == 0) {
        printf("Play tha game, pal, need a name to save yer synth settings "
               "with\n");
        return false;
    }
    printf("Saving '%s' settings for Minisynth to file %s\n", preset_name,
           PRESET_FILENAME);
    FILE *presetzzz = fopen(PRESET_FILENAME, "a");

    fprintf(presetzzz, "::%s"    // m_settings_name
                       "::%d"    // m_voice_mode
                       "::%f"    // ms->m_detune_cents
                       "::%f"    // ms->m_lfo1_amplitude);
                       "::%f"    // ms->m_lfo1_rate);
                       "::%f"    // ms->m_fc_control);
                       "::%f"    // ms->m_q_control);
                       "::%f"    // ms->m_attack_time_msec);
                       "::%f"    // ms->m_delay_time_msec);
                       "::%f"    // ms->m_decay_release_time_msec);
                       "::%f"    // ms->m_pulse_width_pct);
                       "::%f"    // ms->m_feedback_pct);
                       "::%f"    // ms->m_delay_ratio);
                       "::%f"    // ms->m_wet_mix);
                       "::%d"    // ms->m_octave);
                       "::%f"    // ms->m_portamento_time_msec);
                       "::%f"    // ms->m_lfo1_osc_pitch_intensity);
                       "::%f"    // ms->m_sub_osc_db);
                       "::%f"    // ms->m_eg1_osc_intensity);
                       "::%f"    // ms->m_eg1_filter_intensity);
                       "::%f"    // ms->m_lfo1_filter_fc_intensity);
                       "::%f"    // ms->m_sustain_level);
                       "::%f"    // ms->m_noise_osc_db);
                       "::%f"    // ms->m_lfo1_amp_intensity);
                       "::%f"    // ms->m_lfo1_pan_intensity);
                       "::%f"    // ms->m_eg1_dca_intensity);
                       "::%d"    // ms->m_lfo1_waveform);
                       "::%f"    // ms->m_volume_db);
                       "::%d"    // ms->m_legato_mode);
                       "::%d"    // ms->m_pitchbend_range);
                       "::%d"    // ms->m_reset_to_zero);
                       "::%d"    // ms->m_filter_keytrack);
                       "::%f"    // ms->m_filter_keytrack_intensity);
                       "::%d"    // ms->m_velocity_to_attack_scaling);
                       "::%d"    // ms->m_note_number_to_decay_scaling);
                       "::%d"    // ms->m_delay_mode);
                       "::%f"    // ms->m_eg1_dca_intensity);
                       "::%d\n", // ms->m_sustain_override);
            preset_name, ms->m_voice_mode, ms->m_detune_cents,
            ms->m_lfo1_amplitude, ms->m_lfo1_rate, ms->m_fc_control,
            ms->m_q_control, ms->m_attack_time_msec, ms->m_delay_time_msec,
            ms->m_decay_release_time_msec, ms->m_pulse_width_pct,
            ms->m_feedback_pct, ms->m_delay_ratio, ms->m_wet_mix, ms->m_octave,
            ms->m_portamento_time_msec, ms->m_lfo1_osc_pitch_intensity,
            ms->m_sub_osc_db, ms->m_eg1_osc_intensity,
            ms->m_eg1_filter_intensity, ms->m_lfo1_filter_fc_intensity,
            ms->m_sustain_level, ms->m_noise_osc_db, ms->m_lfo1_amp_intensity,
            ms->m_lfo1_pan_intensity, ms->m_eg1_dca_intensity,
            ms->m_lfo1_waveform, ms->m_volume_db, ms->m_legato_mode,
            ms->m_pitchbend_range, ms->m_reset_to_zero, ms->m_filter_keytrack,
            ms->m_filter_keytrack_intensity, ms->m_velocity_to_attack_scaling,
            ms->m_note_number_to_decay_scaling, ms->m_delay_mode,
            ms->m_eg1_dca_intensity, ms->m_sustain_override);

    fclose(presetzzz);
    return true;
}

void minisynth_print_settings(minisynth *ms)
{
    printf("Voice mode %d\n", ms->m_voice_mode);
    printf("Detune Cents: %f\n", ms->m_detune_cents);
    printf("LFO AMp: %f\n", ms->m_lfo1_amplitude);
    printf("LFO Rate %f\n", ms->m_lfo1_rate);
    printf("Filter Control %f\n", ms->m_fc_control);
    printf("Filter Q Control: %f\n", ms->m_q_control);
    printf("EG Attack time: %f\n", ms->m_attack_time_msec);
    printf("EG Delay Time: %f\n", ms->m_delay_time_msec);
    printf("EG Decay Time: %f\n", ms->m_decay_release_time_msec);
    printf("Pulse Width Pct: %f\n", ms->m_pulse_width_pct);
    printf("Delay Feedback Pct %f\n", ms->m_feedback_pct);
    printf("Delay Ratio: %f\n", ms->m_delay_ratio);
    printf("Delay Wet Mix: %f\n", ms->m_wet_mix);
    printf("Octave: %d\n", ms->m_octave);
    printf("Portamento Time %f\n", ms->m_portamento_time_msec);
    printf("LFO1 Osc Pitch Intensity: %f\n", ms->m_lfo1_osc_pitch_intensity);
    printf("Sub OSC Db: %f\n", ms->m_sub_osc_db);
    printf("EG1 OSc Intensity: %f\n", ms->m_eg1_osc_intensity);
    printf("EG1 Filter Intensity %f\n", ms->m_eg1_filter_intensity);
    printf("LFO1 Filter FC Intensity: %f\n", ms->m_lfo1_filter_fc_intensity);
    printf("Sustain Level: %f\n", ms->m_sustain_level);
    printf("Noise OSC Db: %f\n", ms->m_noise_osc_db);
    printf("LFO1 Amp Intensity: %f\n", ms->m_lfo1_amp_intensity);
    printf("LFO1 Pan Intensity: %f\n", ms->m_lfo1_pan_intensity);
    printf("EG1 DCA Intensity: %f\n", ms->m_eg1_dca_intensity);
    printf("LFO Waveform: %d\n", ms->m_lfo1_waveform);
    printf("Volume: %f\n", ms->m_volume_db);
    printf("LEGATO MODE: %d\n", ms->m_legato_mode);
    printf("Pitchbend Range: %d\n", ms->m_pitchbend_range);
    printf("Reset To Zero: %d\n", ms->m_reset_to_zero);
    printf("Filter Keytrack: %d\n", ms->m_filter_keytrack);
    printf("Filter Keytrack Intensity: %f\n", ms->m_filter_keytrack_intensity);
    printf("Velocity to Attack Scaling: %d\n",
           ms->m_velocity_to_attack_scaling);
    printf("Note Number To Decay Scaling: %d\n",
           ms->m_note_number_to_decay_scaling);
    printf("Delay Mode: %d\n", ms->m_delay_mode);
    printf("EG1 DCA Intensity: %f\n", ms->m_eg1_dca_intensity);
    printf("Sustain Override: %d\n", ms->m_sustain_override);
}

void minisynth_set_arpeggiate(minisynth *ms, bool b) { ms->m_arp.active = b; }

void minisynth_play_melody(minisynth *ms)
{

    int idx = mixr->tick % PPNS;

    // top of the loop, check if we need to progress to next loop
    if (idx == 0) {
        if (ms->multi_melody_mode) {
            ms->cur_melody_iteration--;
            if (ms->cur_melody_iteration == 0) {
                ms->cur_melody = (ms->cur_melody + 1) % ms->num_melodies;
                ms->cur_melody_iteration =
                    ms->melody_multiloop_count[ms->cur_melody];
            }
        }
    }

    if (ms->melodies[ms->cur_melody][idx] != NULL) {
        midi_event *ev = ms->melodies[ms->cur_melody][idx];
        midi_parse_midi_event(ms, ev);
    }
}

void minisynth_import_midi_from_file(minisynth *ms, char *filename)
{
    printf("Importing MIDI from %s\n", filename);
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Couldn't open yer file!\n");
        return;
    }

    char *item, *last_s;
    char const *sep = "::";
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
        int count = 0;
        int tick = 0;
        int status = 0;
        int midi_note = 0;
        int midi_vel = 0;
        for (item = strtok_r(line, sep, &last_s); item;
             item = strtok_r(NULL, sep, &last_s)) {
            switch (count) {
            case 0:
                tick = atoi(item);
                if (tick >= PPNS) {
                    printf("TICK OVER!: %d\n", tick);
                    minisynth_add_melody(ms);
                    // tick = tick % PPNS;
                }
                break;
            case 1:
                status = atoi(item);
                break;
            case 2:
                midi_note = atoi(item);
                break;
            case 3:
                midi_vel = atoi(item);
                break;
            }
            count++;
            printf("ITEM! %s\n", item);
        }
        if (count == 4) {
            printf("GOtzz %d %d %d %d %d\n", count, tick, status, midi_note,
                   midi_vel);
            midi_event *ev = new_midi_event(tick, status, midi_note, midi_vel);
            minisynth_add_event(ms, ev);
        }
    }

    minisynth_set_multi_melody_mode(ms, true);
    fclose(fp);
}

void minisynth_set_filter_mod(minisynth *ms, double mod)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        minisynth_voice_set_filter_mod(ms->m_voices[i], mod);
    }
}

void minisynth_del_self(minisynth *ms)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        minisynth_voice_free_self(ms->m_voices[i]);
    }
    for (int i = 0; i < MAX_NUM_MIDI_LOOPS; i++)
    {
        for (int j = 0; j < PPNS; j++)
        {
            if (ms->melodies[i][j] != NULL)
                midi_event_free(ms->melodies[i][j]);
        }
    }
    printf("Deleting MINISYNTH self\n");
    free(ms);
}

void minisynth_set_morph_mode(minisynth *ms, bool b)
{
    ms->m_morph_mode = b;
    minisynth_midi_note_on(ms, 60, 128);
    ms->m_sustain_override = true;
}
