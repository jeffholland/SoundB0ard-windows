#include <math.h>
#include <pthread.h>
#include <stdlib.h>

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

    //ms->vol = 0.7;
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
    ms->m_volume_db = 1.0;
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

    // start loop player running
    pthread_t melody_looprrr;
    if (pthread_create(&melody_looprrr, NULL, play_melody_loop, ms)) {
        fprintf(stderr, "Err running loop\n");
    }
    else {
        pthread_detach(melody_looprrr);
    }

    minisynth_update(ms);

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
            if(mixr->debug_mode)
                printf("Envelope attack!\n");
            scaley_val = scaleybum(1, 128, EG_MINTIME_MS, EG_MAXTIME_MS, data2);
            ms->m_attack_time_msec = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
            scaley_val = scaleybum(1, 128, 0, 1000, data2);
            ms->m_delay_time_msec = scaley_val;
            if(mixr->debug_mode)
                printf("DELAY MS: %f\n", ms->m_delay_time_msec);
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
            if(mixr->debug_mode)
                printf("EG1->DCA intensity!\n");
            scaley_val = scaleybum(1, 128, 0, 10, data2);
            ms->m_eg1_dca_intensity = scaley_val;
        }
        break;
    case 2: // K2 - Envelope Decay Time Msec
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            if(mixr->debug_mode)
                printf("Envelope decay!\n");
            scaley_val = scaleybum(1, 128, EG_MINTIME_MS, EG_MAXTIME_MS, data2);
            ms->m_decay_release_time_msec = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
            scaley_val = scaleybum(1, 128, 0, 10, data2);
            //ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
            //scaley_val = scaleybum(1, 128, X, X, data2);
            //ms->X = scaley_val;
        }
        break;
    case 3: // K3 - Envelope Sustain Level
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            if(mixr->debug_mode)
                printf("Env Sustain level\n");
            scaley_val = scaleybum(1, 128, 0, 1, data2);
            ms->m_sustain_level = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
            // scaley_val = scaleybum(1, 128, X, X, data2);
            // ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
            // scaley_val = scaleybum(1, 128, X, X, data2);
            // ms->X = scaley_val;
        }
        break;
    case 4: // K4 - Synth Volume
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            printf("Synth volume\n");
            scaley_val = scaleybum(1, 128, 0, 1, data2);
            ms->m_volume_db = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
        //    scaley_val = scaleybum(1, 128, X, X, data2);
        //    ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
        //    scaley_val = scaleybum(1, 128, X, X, data2);
        //    ms->X = scaley_val;
        }
        break;
    case 5: // K6 - LFO amplitude
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            if(mixr->debug_mode)
                printf("LFO Amplitude\n");
            scaley_val = scaleybum(0, 128, 0.0, 1.0, data2);
            ms->m_lfo1_amplitude = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
        //    scaley_val = scaleybum(1, 128, X, X, data2);
        //    ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
        //    scaley_val = scaleybum(1, 128, X, X, data2);
        //    ms->X = scaley_val;
        }
        break;
    case 6: // K5 - LFO rate
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            if(mixr->debug_mode)
                printf("LFO Rate\n");
            scaley_val = scaleybum(0, 128, MIN_LFO_RATE, MAX_LFO_RATE, data2);
            ms->m_lfo1_rate = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
        //    scaley_val = scaleybum(1, 128, X, X, data2);
        //    ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
        //    scaley_val = scaleybum(1, 128, X, X, data2);
        //    ms->X = scaley_val;
        }
        break;
    case 7: // K7 - Filter Frequency Cut
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            if(mixr->debug_mode)
                printf("Filter Frequency!\n");
            scaley_val = scaleybum(1, 128, FILTER_FC_MIN, FILTER_FC_MAX, data2);
            ms->m_fc_control = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
        //    scaley_val = scaleybum(1, 128, X, X, data2);
        //    ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
        //    scaley_val = scaleybum(1, 128, X, X, data2);
        //    ms->X = scaley_val;
        }
        break;
    case 8: // K8 - Filter Q control
        if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_ONE) {
            if(mixr->debug_mode)
                printf("Filter Q!\n");
            scaley_val = scaleybum(1, 128, 1, 10, data2);
            ms->m_q_control = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_TWO) {
        //    scaley_val = scaleybum(1, 128, X, X, data2);
        //    ms->X = scaley_val;
        }
        else if (ms->m_midi_knob_mode == MIDI_KNOB_MODE_THREE) {
        //    scaley_val = scaleybum(1, 128, X, X, data2);
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
    minisynth_update(ms);
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
        for (int i = 0; i < PPNS; i++) {
            if (ms->melodies[ms->cur_melody][i] != NULL)
                print_midi_event_rec(ms->melodies[ms->cur_melody][i]);
        }
    }

    // TODO - a shit load of error checking on boundaries and size
    swprintf(
        status_string, MAX_PS_STRING_SZ, WCOOL_COLOR_PINK
        "[SYNTH] - Vol: %.2f Multi: %s, CurMelody:%d DelayMode: %d Mode: %ls"
        "\n      A:%.2f D/R:%.2f S:%.2f Amp: %2.f LFO1 amp: %.2f rate:%.2f "
        "Filter FC: %.2f Filter Q: %2.f"
        "\n      Delay ms: %.2f Feedback Pct:%.2f Delay Ratio: %.2f Wet Mix: "
        "%2.f"
        "\n      Detune Cents: %.2f Pulse Width Pct:%.2f SubOsc Db: %.2f "
        "NoiseOsc Db: %2.f",
        ms->m_volume_db, ms->multi_melody_mode ? "true" : "false", ms->cur_melody,
        ms->m_delay_mode, s_mode_names[ms->m_voice_mode],
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
    // minisynth_update(ms);

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

    return accum_out_left * ms->m_volume_db;
}

midi_event **minisynth_get_midi_loop(minisynth *self)
{
    return self->melodies[self->cur_melody];
}

void minisynth_add_event(minisynth *ms, midi_event *ev)
{
    int tick = ev->tick;
    while (ms->melodies[ms->cur_melody][tick] != NULL) {
        printf("Gotsz a tick already - bump!\n");
        tick++;
        if (tick == PPNS) // wrap around
            tick = 0;
    }
    ev->tick = tick;
    if (mixr->debug_mode)
        printf("Adding Event: Tick: %d Type: %s Midi: %d\n", tick,
               ev->event_type == 144 ? "NOTEON" : "NOTEOFF", ev->data1);
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
    } else {
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

void minisynth_add_midi_loop(minisynth *ms, midi_event **events,
                             int melody_num)
{
    if (melody_num >= MAX_NUM_MIDI_LOOPS) {
        printf("Dingjie!\n");
        return;
    }
    for ( int i = 0; i < PPNS; i++) {
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
    for ( int i = 0; i < PPNS; i++) {
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
            int new_tick = (ev->tick + (sixteenth*sixteenth_of_loop)) % PPNS;
            printf("Old tick: %d with new: %d\n", ev->tick, new_tick);
            new_midi_events_loop[new_tick] = new_midi_event(new_tick, ev->event_type, ev->data1, ev->data2);
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
void minisynth_handle_midi_note(minisynth *ms, int note, int velocity)
{
	if (mixr->debug_mode)
		print_midi_event(note);

    minisynth_midi_note_on(ms, note, velocity);

    int note_off_tick =
        ((mixr->tick % PPNS) + PPS * 4) %
        PPNS; // rough guess - PPS is pulses per quart note
              // and PPNS is pulses per minisynth Loop

    midi_event *off_event = new_midi_event(note_off_tick, 128,
                                    note, velocity);
    ////////////////////////

    if (ms->recording) {
        printf("Recording note!\n");
        int note_on_tick = mixr->tick % PPNS;
        midi_event *on_event = new_midi_event(
            note_on_tick, 144, note, velocity);

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
    ms->m_voice_mode = rand() % 4;
    ms->m_detune_cents = ((float)rand())/RAND_MAX;
    ms->m_lfo1_amplitude = ((float)rand())/RAND_MAX;
    ms->m_lfo1_rate = ((float)rand())/RAND_MAX * (MAX_LFO_RATE - MIN_LFO_RATE) + MIN_LFO_RATE;
    ms->m_fc_control = ((float)rand())/RAND_MAX * (FILTER_FC_MAX - FILTER_FC_MIN) + FILTER_FC_MIN;
    ms->m_q_control = ((float)rand())/RAND_MAX;
    ms->m_attack_time_msec = ((float)rand())/RAND_MAX * EG_MAXTIME_MS;
    ms->m_delay_time_msec = ((float)rand())/RAND_MAX * EG_MAXTIME_MS;
    ms->m_decay_release_time_msec = ((float)rand())/RAND_MAX * EG_MAXTIME_MS;
    ms->m_pulse_width_pct = (((float)rand()/(float)(RAND_MAX)) * 96) + 2;
    //ms->m_feedback_pct = ((float)rand()/(float)(RAND_MAX));
    //ms->m_delay_ratio = ((float)rand())/RAND_MAX;
    //ms->m_wet_mix = ((float)rand()/(float)(RAND_MAX));
    //ms->m_octave = rand() % 4 - 2;
    //ms->m_portamento_time_msec = ((float)rand()/(float)(RAND_MAX)) * 10;
    //ms->m_lfo1_osc_pitch_intensity = ((float)rand()/(float)(RAND_MAX));
    //ms->m_sub_osc_db = -96.000000;
    //ms->m_eg1_osc_intensity = 0.0;
    //ms->m_eg1_filter_intensity = 0.0;
    //ms->m_lfo1_filter_fc_intensity = 0.0;
    //// ms->m_sustain_level = 0.510000;
    //ms->m_sustain_level = 0.9;
    //ms->m_noise_osc_db = -96.000000;
    //ms->m_lfo1_amp_intensity = 0.0;
    //ms->m_lfo1_pan_intensity = 0.0;
    //ms->m_eg1_dca_intensity = 1.0;
    //ms->m_lfo1_waveform = 0;
    //ms->m_volume_db = 1.0;
    //ms->m_legato_mode = DEFAULT_LEGATO_MODE;
    //ms->m_pitchbend_range = 1;
    //ms->m_reset_to_zero = DEFAULT_RESET_TO_ZERO;
    //ms->m_filter_keytrack = DEFAULT_FILTER_KEYTRACK;
    //ms->m_filter_keytrack_intensity = DEFAULT_FILTER_KEYTRACK_INTENSITY;
    //ms->m_velocity_to_attack_scaling = 0;
    //ms->m_note_number_to_decay_scaling = 0;
    //ms->m_delay_mode = 0;
    //ms->m_eg1_dca_intensity = 1.0;
    //ms->m_sustain_override = false;
    //
    minisynth_print_settings(ms);
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
    printf("Velocity to Attack Scaling: %d\n", ms->m_velocity_to_attack_scaling);
    printf("Note Number To Decay Scaling: %d\n", ms->m_note_number_to_decay_scaling);
    printf("Delay Mode: %d\n", ms->m_delay_mode);
    printf("EG1 DCA Intensity: %f\n", ms->m_eg1_dca_intensity);
    printf("Sustain Override: %d\n", ms->m_sustain_override);
}
