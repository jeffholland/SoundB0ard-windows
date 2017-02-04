#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "defjams.h"
#include "keys.h"
#include "mixer.h"
#include "utils.h"

extern mixer *mixr;
extern pthread_cond_t midi_tick_cond;
extern pthread_mutex_t midi_tick_lock;

void keys(int soundgen_num)
{
    printf("Entering Keys Mode for %d\n", soundgen_num);
    printf("Press 'q' or 'Esc' to go back to Run Mode\n");
    struct termios new_info, old_info;
    tcgetattr(0, &old_info);
    new_info = old_info;

    new_info.c_lflag &= (~ICANON & ~ECHO);
    new_info.c_cc[VMIN] = 1;
    new_info.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &new_info);

    nanosynth *ns = (nanosynth *)mixr->sound_generators[soundgen_num];

    int ch = 0;
    int quit = 0;

    struct pollfd fdz[1];
    int pollret;

    fdz[0].fd = STDIN_FILENO;
    fdz[0].events = POLLIN;

    while (!quit) {

        pollret = poll(fdz, 1, 0);
        if (pollret == -1) {
            perror("poll");
            return;
        }

        if (fdz[0].revents & POLLIN) {
            ch = getchar();
            printf("C %d\n", ch);
            int midi_num;
            switch (ch) {
            case 27:
            case 113:
                quit = 1;
                break;
            case 49:
                printf("Down an octave...\n");
                change_octave(ns, DOWN);
                break;
            case 50:
                printf("Up an octave...\n");
                change_octave(ns, UP);
                break;
            case 114:
                ns->recording = 1 - ns->recording;
                printf("Toggling REC to %s\n",
                       ns->recording ? "true" : "false");
                break;
            case 122:
                printf("Changing WAVE form of synth->osc1\n");
                nanosynth_change_osc_wave_form(ns, 0, 0, true);
                break;
            case 120:
                printf("Changing WAVE form of synth->osc2\n");
                nanosynth_change_osc_wave_form(ns, 0, 1, true);
                break;
            case 99:
                printf("Changing WAVE form of synth->lfo\n");
                nanosynth_change_osc_wave_form(ns, 0, 2, true);
                break;
            case 118:
                ns->m_filter_keytrack = 1 - ns->m_filter_keytrack;
                printf("Key tracking toggle - val is %d!\n",
                       ns->m_filter_keytrack);
                break;
            case 98:
                printf("Key tracking intensity toggle!\n");
                if (ns->m_filter_keytrack_intensity == 0.5)
                    ns->m_filter_keytrack_intensity = 2.0;
                else if (ns->m_filter_keytrack_intensity == 2.0)
                    ns->m_filter_keytrack_intensity = 1.0;
                else
                    ns->m_filter_keytrack_intensity = 0.5;
                printf("Key tracking intensity toggle! Val is %f\n",
                       ns->m_filter_keytrack_intensity);
                break;

            default:
                // play note
                midi_num = ch_midi_lookup(ch, ns);
                if (midi_num != -1) {
                    print_midi_event(midi_num);
                    nanosynth_midi_note_on(ns, midi_num,
                                           126); // TODO fill in real velocity
                    if (ns->recording) {
                        printf("Recording note!\n");
                        int tick = mixr->tick % PPNS;
                        midi_event *ev = new_midi_event(tick, 144, midi_num, 0);
                        ns->melodies[ns->cur_melody][tick] = ev;
                    }
                }
                printf("CCCC %d\n", ch);
            }
        }
    }
    tcsetattr(0, TCSANOW, &old_info);
}

void *play_melody_loop(void *p)
{
    nanosynth *ns = (nanosynth *)p;

    int note_played_time = 0;

    printf("PLAY melody starting..\n");

    unsigned last_midi_num_played = 0;

    while (1) {
        pthread_mutex_lock(&midi_tick_lock);
        pthread_cond_wait(&midi_tick_cond, &midi_tick_lock);
        pthread_mutex_unlock(&midi_tick_lock);

        int idx = mixr->tick % PPNS;

        if (idx == 0) {
            if (ns->multi_melody_mode) {
                ns->cur_melody_iteration--;
                if (ns->cur_melody_iteration == 0) {
                    ns->cur_melody = (ns->cur_melody + 1) % ns->num_melodies;
                    ns->cur_melody_iteration =
                        ns->melody_multiloop_count[ns->cur_melody];
                }
            }
        }

        if (ns->melodies[ns->cur_melody][idx] != NULL) {
            midi_event *ev = ns->melodies[ns->cur_melody][idx];
            switch (ev->event_type) {
            case (144): {
                nanosynth_midi_note_on(ns, ev->data1, ev->data2);
                break;
            }
            case (128): { // Hex 0x90
                nanosynth_midi_note_off(ns, ev->data1, ev->data2, false);
                break;
            }
            case (176): { // Hex 0xB0
                nanosynth_midi_control(ns, ev->data1, ev->data2);
                break;
            }
            case (224): { // Hex 0xE0
                nanosynth_midi_pitchbend(ns, ev->data1, ev->data2);
                break;
            }
            }
            last_midi_num_played = ev->data1;
            note_played_time = 1;
        }

        // if (ns->sustain > 0 && note_played_time > 0) {
        //     note_played_time++;
        //     if ((note_played_time > ns->sustain) &&
        //         (ns->osc1->m_midi_note_number == last_midi_num_played)) {
        //         eg_note_off(ns->eg1);
        //         note_played_time = 0;
        //     }
        // }
    }

    return NULL;
}
