#include <math.h>
#include <portmidi.h>
#include <porttime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defjams.h"
#include "digisynth.h"
#include "dxsynth.h"
#include "midi_freq_table.h"
#include "midimaaan.h"
#include "minisynth.h"
#include "mixer.h"
#include "sample_sequencer.h"
#include "synthdrum_sequencer.h"
#include "utils.h"

extern mixer *mixr;

extern char *s_synth_waves[6];

void *midi_init()
{
    printf("MIDI maaaaan!\n");
    pthread_setname_np("Midimaaaan");

    PmError retval = Pm_Initialize();
    if (retval != pmNoError)
        printf("Err running Pm_Initialize: %s\n", Pm_GetErrorText(retval));

    int cnt;
    const PmDeviceInfo *info;

    int dev = 0;

    if ((cnt = Pm_CountDevices()))
    {
        for (int i = 0; i < cnt; i++)
        {
            info = Pm_GetDeviceInfo(i);
            if (info->input && (strncmp(info->name, "MPKmini2", 8) == 0))
            {
                dev = i;
                strncpy(mixr->midi_controller_name, info->name, 127);
                break;
            }
        }
    }
    else
    {
        Pm_Terminate();
        return NULL;
    }

    retval = Pm_OpenInput(&mixr->midi_stream, dev, NULL, 512L, NULL, NULL);
    if (retval != pmNoError)
    {
        printf("Err opening input for MPKmini2: %s\n", Pm_GetErrorText(retval));
        Pm_Terminate();
        return NULL;
    }

    mixr->have_midi_controller = true;

    return NULL;
}

void print_midi_event_rec(midi_event ev)
{
    printf("[Midi] note: %d\n", ev.data1);
}

midi_event new_midi_event(int event_type, int data1, int data2)
{
    midi_event ev = {.event_type = event_type,
                     .data1 = data1,
                     .data2 = data2,
                     .delete_after_use = false};
    return ev;
}

void midi_parse_midi_event(soundgenerator *sg, midi_event *ev)
{
    int cur_midi_tick = mixr->timing_info.midi_tick % PPBAR;

    int note = ev->data1;

    if (is_synth(sg))
    {
        synthbase *base = get_synthbase(sg);
        if (!ev->delete_after_use || ev->source == EXTERNAL_DEVICE)
        {
            if (ev->event_type == MIDI_ON)
                arp_add_last_note(&base->arp, note);
        }
    }

    if (sg->type == MINISYNTH_TYPE)
    {
        minisynth *ms = (minisynth *)sg;

        switch (ev->event_type)
        {
        case (MIDI_ON):
        { // Hex 0x80
            minisynth_midi_note_on(ms, note, ev->data2);

            if (ev->source != EXTERNAL_DEVICE)
            {
                int sustain_ms = ev->hold ? ev->hold : ms->base.sustain_note_ms;
                int sustain_time_in_ticks =
                    sustain_ms * mixr->timing_info.ms_per_midi_tick;

                int note_off_tick =
                    (cur_midi_tick + sustain_time_in_ticks) % PPBAR;
                midi_event off = new_midi_event(MIDI_OFF, note, 128);
                off.delete_after_use = true;
                synthbase_add_event(&ms->base, ms->base.cur_pattern,
                                    note_off_tick, off);
            }
            break;
        }
        case (MIDI_OFF):
        { // Hex 0x90
            minisynth_midi_note_off(ms, note, ev->data2, false);
            break;
        }
        case (MIDI_CONTROL):
        { // Hex 0xB0
            minisynth_midi_control(ms, ev->data1, ev->data2);
            break;
        }
        case (MIDI_PITCHBEND):
        { // Hex 0xE0
            minisynth_midi_pitchbend(ms, ev->data1, ev->data2);
            break;
        }
        default:
            printf(
                "HERE PAL, I've NAE IDEA WHIT KIND OF MIDI EVENT THAT WiS\n");
        }
    }
    else if (sg->type == DXSYNTH_TYPE)
    {
        dxsynth *dx = (dxsynth *)sg;

        switch (ev->event_type)
        {
        case (144):
        { // Hex 0x80
            dxsynth_midi_note_on(dx, note, ev->data2);
            if (ev->source != EXTERNAL_DEVICE)
            {
                int sustain_time_in_ticks = dx->base.sustain_note_ms *
                                            mixr->timing_info.ms_per_midi_tick;
                int note_off_tick =
                    (cur_midi_tick + sustain_time_in_ticks) % PPBAR;
                midi_event off = new_midi_event(128, note, 128);
                off.delete_after_use = true;
                synthbase_add_event(&dx->base, dx->base.cur_pattern,
                                    note_off_tick, off);
            }
            break;
        }
        case (128):
        { // Hex 0x90
            dxsynth_midi_note_off(dx, note, ev->data2, true);
            break;
        }
        case (176):
        { // Hex 0xB0
            dxsynth_midi_control(dx, ev->data1, ev->data2);
            break;
        }
        case (224):
        { // Hex 0xE0
            dxsynth_midi_pitchbend(dx, ev->data1, ev->data2);
            break;
        }
        default:
            printf(
                "HERE PAL, I've NAE IDEA WHIT KIND OF MIDI EVENT THAT WiS\n");
        }
    }
    else if (sg->type == DIGISYNTH_TYPE)
    {
        digisynth *ds = (digisynth *)sg;
        switch (ev->event_type)
        {
        case (MIDI_ON):
        { // Hex 0x80
            digisynth_midi_note_on(ds, note, ev->data2);
            int sustain_time_in_ticks =
                ds->base.sustain_note_ms * mixr->timing_info.ms_per_midi_tick;
            int note_off_tick = (cur_midi_tick + sustain_time_in_ticks) % PPBAR;
            midi_event off = new_midi_event(128, note, 128);
            off.delete_after_use = true;
            synthbase_add_event(&ds->base, 0, note_off_tick, off);
            break;
        }
        case (MIDI_OFF):
        { // Hex 0x90
            digisynth_midi_note_off(ds, note, ev->data2, true);
            break;
        }
        }
    }
    else if (sg->type == SYNTHDRUM_TYPE)
    {
        synthdrum_sequencer *drumsynth = (synthdrum_sequencer *)sg;
        // printf("DRUMSYNTH MIDI! Type:%d DATA1:%d DATA2:%d\n", ev->event_type,
        // ev->data1, ev->data2);
        if (ev->event_type == 176)
        {
            switch (ev->data1)
            {
            case (1):
                if (mixr->midi_bank_num == 0)
                {
                    // o1wav
                    int wav_type = scaleybum(0, 127, 0, MAX_OSC - 1, ev->data2);
                    // printf("o1 WAV_TYPE! %s -- %d\n",
                    // s_synth_waves[wav_type],
                    //       wav_type);
                    synthdrum_set_osc_wav(drumsynth, 1, wav_type);
                }
                else if (mixr->midi_bank_num == 1)
                {
                    // o2wav
                    int wav_type = scaleybum(0, 127, 0, MAX_OSC - 1, ev->data2);
                    // printf("o2 WAV_TYPE! %s -- %d\n",
                    // s_synth_waves[wav_type],
                    //       wav_type);
                    synthdrum_set_osc_wav(drumsynth, 2, wav_type);
                }
                else if (mixr->midi_bank_num == 2)
                {
                    // o2wav
                    // printf("K1:: data1:%d data2:%d\n", ev->data1, ev->data2);
                }
                break;
            case (2):
                if (mixr->midi_bank_num == 0)
                {
                    // o1freq
                    double freq = scaleybum(0, 127, OSC_FO_MIN, 200, ev->data2);
                    // printf("o1 FREQ!%f\n", freq);
                    synthdrum_set_osc_fo(drumsynth, 1, freq);
                }
                else if (mixr->midi_bank_num == 1)
                {
                    // o2freq
                    double freq = scaleybum(0, 127, OSC_FO_MIN, 200, ev->data2);
                    // printf("o2 FREQ!%f\n", freq);
                    synthdrum_set_osc_fo(drumsynth, 2, freq);
                }
                break;
            case (3):
                if (mixr->midi_bank_num == 0)
                {
                    // o1amp
                    double amp = scaleybum(0, 127, 0., 1., ev->data2);
                    // printf("o1 AMP!%f\n", amp);
                    synthdrum_set_osc_amp(drumsynth, 1, amp);
                }
                else if (mixr->midi_bank_num == 1)
                {
                    // o1amp
                    double amp = scaleybum(0, 127, 0., 1., ev->data2);
                    // printf("o2 AMP!%f\n", amp);
                    synthdrum_set_osc_amp(drumsynth, 2, amp);
                }
                break;
            case (4):
                if (mixr->midi_bank_num == 0)
                {
                    double intensity = scaleybum(0, 127, 0., 1., ev->data2);
                    // printf("o1 INT!%f\n", intensity);
                    synthdrum_set_eg_osc_intensity(drumsynth, 1, 1, intensity);
                }
                else if (mixr->midi_bank_num == 1)
                {
                    double intensity = scaleybum(0, 127, 0., 1., ev->data2);
                    synthdrum_set_eg_osc_intensity(drumsynth, 2, 2, intensity);
                }
                else if (mixr->midi_bank_num == 2)
                {
                    double distortion_threshold =
                        scaleybum(0, 127, 0.1, 0.9, ev->data2);
                    synthdrum_set_distortion_threshold(drumsynth,
                                                       distortion_threshold);
                }
                break;
            case (5):
                if (mixr->midi_bank_num == 0)
                {
                    double ms =
                        scaleybum(0, 128, EG_MINTIME_MS, 800, ev->data2);
                    synthdrum_set_eg_attack(drumsynth, 1, ms);
                }
                else if (mixr->midi_bank_num == 1)
                {
                    double ms =
                        scaleybum(0, 128, EG_MINTIME_MS, 700, ev->data2);
                    synthdrum_set_eg_attack(drumsynth, 2, ms);
                }
                break;
            case (6):
                if (mixr->midi_bank_num == 0)
                {
                    double ms =
                        scaleybum(0, 128, EG_MINTIME_MS, 2000, ev->data2);
                    synthdrum_set_eg_decay(drumsynth, 1, ms);
                }
                else if (mixr->midi_bank_num == 1)
                {
                    double ms =
                        scaleybum(0, 128, EG_MINTIME_MS, 1000, ev->data2);
                    synthdrum_set_eg_decay(drumsynth, 2, ms);
                }
                break;
            case (7):
                if (mixr->midi_bank_num == 0)
                {
                    double ms = scaleybum(0, 128, 0, 1, ev->data2);
                    synthdrum_set_eg_sustain_lvl(drumsynth, 1, ms);
                }
                else if (mixr->midi_bank_num == 1)
                {
                    double ms = scaleybum(0, 128, 0, 1, ev->data2);
                    synthdrum_set_eg_sustain_lvl(drumsynth, 2, ms);
                }
                else if (mixr->midi_bank_num == 2)
                {
                    double freq = scaleybum(0, 127, 180, 18000, ev->data2);
                    synthdrum_set_filter_freq(drumsynth, freq);
                }
                break;
            case (8):
                if (mixr->midi_bank_num == 0)
                {
                    double ms = scaleybum(0, 128, EG_MINTIME_MS, EG_MAXTIME_MS,
                                          ev->data2);
                    synthdrum_set_eg_release(drumsynth, 1, ms);
                }
                else if (mixr->midi_bank_num == 1)
                {
                    double ms = scaleybum(0, 128, EG_MINTIME_MS, EG_MAXTIME_MS,
                                          ev->data2);
                    synthdrum_set_eg_release(drumsynth, 2, ms);
                }
                else if (mixr->midi_bank_num == 2)
                {
                    double q = scaleybum(0, 127, 1, 8, ev->data2);
                    synthdrum_set_filter_q(drumsynth, q);
                }
                break;
            }
        }
    }

    if (ev->delete_after_use)
    {
        midi_event_clear(ev);
    }
}

void midi_pattern_quantize(midi_pattern *pattern)
{
    printf("Quantizzzzzzing\n");

    midi_pattern quantized_loop = {};

    for (int i = 0; i < PPBAR; i++)
    {
        midi_event ev = (*pattern)[i];
        if (ev.event_type)
        {
            int amendedtick = 0;
            int tickdiv16 = i / PPSIXTEENTH;
            int lower16th = tickdiv16 * PPSIXTEENTH;
            int upper16th = lower16th + PPSIXTEENTH;
            if ((i - lower16th) < (upper16th - i))
                amendedtick = lower16th;
            else
                amendedtick = upper16th;

            quantized_loop[amendedtick] = ev;
            printf("Amended TICK: %d\n", amendedtick);
        }
    }

    for (int i = 0; i < PPBAR; i++)
        (*pattern)[i] = quantized_loop[i];
}

void midi_pattern_print(midi_event *loop)
{
    for (int i = 0; i < PPBAR; i++)
    {
        midi_event ev = loop[i];
        if (ev.event_type)
        {
            char type[20] = {0};
            switch (ev.event_type)
            {
            case (144):
                strcpy(type, "note_on");
                break;
            case (128):
                strcpy(type, "note_off");
                break;
            case (176):
                strcpy(type, "midi_control");
                break;
            case (224):
                strcpy(type, "pitch_bend");
                break;
            default:
                strcpy(type, "no_idea");
                break;
            }
            printf("[Tick: %5d] - note: %4d velocity: %4d type: %s "
                   "delete_after_use: %s\n",
                   i, ev.data1, ev.data2, type,
                   ev.delete_after_use ? "true" : "false");
        }
    }
}
void midi_event_cp(midi_event *from, midi_event *to)
{
    to->event_type = from->event_type;
    to->data1 = from->data1;
    to->data2 = from->data2;
    to->delete_after_use = from->delete_after_use;
}

void midi_event_clear(midi_event *ev) { memset(ev, 0, sizeof(midi_event)); }

int get_midi_note_from_string(char *string)
{
    if (strlen(string) > 4)
    {
        printf("DINGIE!\n");
        return -1;
    }
    char note[3] = {0};
    int octave = -1;
    sscanf(string, "%[a-z#]%d", note, &octave);
    if (octave == -1)
        return -1;

    octave = 12 + (octave * 12);

    printf("MIDI NOTE:%s %d \n", note, octave);
    //// twelve semitones:
    //// C C#/Db D D#/Eb E F F#/Gb G G#/Ab A A#/Bb B
    ////
    int midinotenum = -1;
    if (!strcasecmp("c", note))
        midinotenum = 0 + octave;
    else if (!strcasecmp("c#", note) || !strcasecmp("db", note) ||
             !strcasecmp("dm", note))
        midinotenum = 1 + octave;
    else if (!strcasecmp("d", note))
        midinotenum = 2 + octave;
    else if (!strcasecmp("d#", note) || !strcasecmp("eb", note) ||
             !strcasecmp("em", note))
        midinotenum = 3 + octave;
    else if (!strcasecmp("e", note))
        midinotenum = 4 + octave;
    else if (!strcasecmp("f", note))
        midinotenum = 5 + octave;
    else if (!strcasecmp("f#", note) || !strcasecmp("gb", note) ||
             !strcasecmp("gm", note))
        midinotenum = 6 + octave;
    else if (!strcasecmp("g", note))
        midinotenum = 7 + octave;
    else if (!strcasecmp("g#", note) || !strcasecmp("ab", note) ||
             !strcasecmp("am", note))
        midinotenum = 8 + octave;
    else if (!strcasecmp("a", note))
        midinotenum = 9 + octave;
    else if (!strcasecmp("a#", note) || !strcasecmp("bb", note) ||
             !strcasecmp("bm", note))
        midinotenum = 10 + octave;
    else if (!strcasecmp("b", note))
        midinotenum = 11 + octave;
    printf("MIDI NOTE NUM:%d \n", midinotenum);

    return midinotenum;
}
int get_midi_note_from_mixer_key(unsigned int key, int octave)
{
    int midi_octave = 12 + (octave * 12);
    return key + midi_octave;
}

void midi_pattern_set_velocity(midi_event *pattern, unsigned int midi_tick,
                               unsigned int velocity)
{
    if (!pattern)
    {
        printf("Dingie, gimme a REAL pattern!\n");
        return;
    }

    if (midi_tick < PPBAR && velocity < 128)
        pattern[midi_tick].data2 = velocity;
    else
        printf("Nae valid!? Midi_tick:%d // velocity:%d\n", midi_tick,
               velocity);
}

void midi_pattern_rand_amp(midi_event *pattern)
{
    for (int i = 0; i < PPBAR; i++)
    {
        if (pattern[i].event_type == MIDI_ON)
            pattern[i].data2 = rand() % 127;
    }
}
