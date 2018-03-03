#include <stdlib.h>
#include <string.h>

#include <cmdloop.h>
#include <mixer.h>
#include <pattern_parser.h>
#include <sample_sequencer.h>
#include <stepper_cmds.h>
#include <synthdrum_sequencer.h>
#include <utils.h>

extern mixer *mixr;

bool parse_stepper_cmd(int num_wurds, char wurds[][SIZE_OF_WURD])
{
    if (strncmp("samp", wurds[0], 3) == 0 ||
        strncmp("step", wurds[0], 4) == 0 || strncmp("beat", wurds[0], 4) == 0)
    {

        if (strncmp("ls", wurds[1], 2) == 0)
        {
            synthdrum_list_patches();
            return true;
        }

        int soundgen_num = -1;
        int target_pattern_num = -1;
        sscanf(wurds[1], "%d:%d", &soundgen_num, &target_pattern_num);

        if (mixer_is_valid_soundgen_num(mixr, soundgen_num) &&
            is_stepper(mixr->sound_generators[soundgen_num]))
        {
            if (parse_step_sequencer_command(soundgen_num, target_pattern_num,
                                             wurds, num_wurds))
            {
                // no-op, all good
            }
            else if (mixr->sound_generators[soundgen_num]->type ==
                     SEQUENCER_TYPE)
            {
                sample_sequencer *s =
                    (sample_sequencer *)mixr->sound_generators[soundgen_num];

                if (strncmp("load", wurds[2], 4) == 0 ||
                    strncmp("import", wurds[2], 6) == 0)
                {
                    if (is_valid_file(wurds[3]))
                    {
                        printf("Changing Loaded "
                               "FILE!\n");
                        sample_seq_import_file(s, wurds[3]);
                    }
                    else
                        printf("%s is not a valid file\n", wurds[3]);
                }
                else if (strncmp("pitch", wurds[2], 5) == 0)
                {
                    double v = atof(wurds[3]);
                    sample_sequencer_set_pitch(s, v);
                }
            }
            else if (mixr->sound_generators[soundgen_num]->type ==
                     SYNTHDRUM_TYPE)
            {
                parse_drumsynth_cmd(soundgen_num, &wurds[2], num_wurds - 2);
            }
        }
        return true;
    }
    return false;
}

bool parse_drumsynth_cmd(int soundgen_num, char wurds[][SIZE_OF_WURD],
                         int num_wurds)
{
    (void)num_wurds;
    double val = atof(wurds[1]);
    printf("Drum synth! val:%f\n", val);
    if (mixer_is_valid_soundgen_num(mixr, soundgen_num) &&
        mixr->sound_generators[soundgen_num]->type == SYNTHDRUM_TYPE)
    {
        synthdrum_sequencer *synth =
            (synthdrum_sequencer *)mixr->sound_generators[soundgen_num];
        if (strncmp("save", wurds[0], 4) == 0)
        {
            synthdrum_save_patch(synth, wurds[1]);
        }
        if (strncmp("open", wurds[0], 4) == 0 ||
            strncmp("load", wurds[0], 4) == 0)
        {
            synthdrum_open_patch(synth, wurds[1]);
        }
        else if (strncmp("distortion_threshold", wurds[0], 20) == 0)
        {
            synthdrum_set_distortion_threshold(synth, val);
        }
        else if (strncmp("o1_wav", wurds[0], 6) == 0)
        {
            synthdrum_set_osc_wav(synth, 1, val);
        }
        else if (strncmp("o1_fo", wurds[0], 6) == 0)
        {
            synthdrum_set_osc_fo(synth, 1, val);
        }
        else if (strncmp("o1_amp", wurds[0], 6) == 0)
        {
            synthdrum_set_osc_amp(synth, 1, val);
        }
        else if (strncmp("e2_o2_int", wurds[0], 8) == 0)
        {
            synthdrum_set_eg2_osc_intensity(synth, val);
        }
        else if (strncmp("e1_att", wurds[0], 6) == 0)
        {
            synthdrum_set_eg_attack(synth, 1, val);
        }
        else if (strncmp("e1_dec", wurds[0], 6) == 0)
        {
            synthdrum_set_eg_decay(synth, 1, val);
        }
        else if (strncmp("e1_sus_lvl", wurds[0], 6) == 0)
        {
            synthdrum_set_eg_sustain_lvl(synth, 1, val);
        }
        else if (strncmp("e1_sus_ms", wurds[0], 6) == 0)
        {
            synthdrum_set_eg_sustain_ms(synth, 1, val);
        }
        else if (strncmp("e1_rel", wurds[0], 6) == 0)
        {
            synthdrum_set_eg_release(synth, 1, val);
        }
        else if (strncmp("o2_wav", wurds[0], 6) == 0)
        {
            synthdrum_set_osc_wav(synth, 2, val);
        }
        else if (strncmp("o2_fo", wurds[0], 6) == 0)
        {
            synthdrum_set_osc_fo(synth, 2, val);
        }
        else if (strncmp("o2_amp", wurds[0], 6) == 0)
        {
            synthdrum_set_osc_amp(synth, 2, val);
        }
        else if (strncmp("mod_pitch_semitones", wurds[0], 19) == 0)
        {
            synthdrum_set_mod_semitones_range(synth, val);
        }
        else if (strncmp("e2_att", wurds[0], 6) == 0)
        {
            synthdrum_set_eg_attack(synth, 2, val);
        }
        else if (strncmp("e2_dec", wurds[0], 6) == 0)
        {
            synthdrum_set_eg_decay(synth, 2, val);
        }
        else if (strncmp("e2_sus_lvl", wurds[0], 6) == 0)
        {
            synthdrum_set_eg_sustain_lvl(synth, 2, val);
        }
        else if (strncmp("e2_sus_ms", wurds[0], 6) == 0)
        {
            synthdrum_set_eg_sustain_ms(synth, 2, val);
        }
        else if (strncmp("e2_rel", wurds[0], 6) == 0)
        {
            synthdrum_set_eg_release(synth, 2, val);
        }
        else if (strncmp("e3_att", wurds[0], 6) == 0)
        {
            synthdrum_set_eg_attack(synth, 2, val);
        }
        else if (strncmp("e3_dec", wurds[0], 6) == 0)
        {
            synthdrum_set_eg_decay(synth, 2, val);
        }
        else if (strncmp("e3_sus_lvl", wurds[0], 10) == 0)
        {
            synthdrum_set_eg_sustain_lvl(synth, 3, val);
        }
        else if (strncmp("e3_sus_ms", wurds[0], 9) == 0)
        {
            synthdrum_set_eg_sustain_ms(synth, 3, val);
        }
        else if (strncmp("e3_rel", wurds[0], 6) == 0)
        {
            synthdrum_set_eg_release(synth, 3, val);
        }
        else if (strncmp("filter_type", wurds[0], 11) == 0)
        {
            synthdrum_set_filter_type(synth, val);
        }
        else if (strncmp("freq", wurds[0], 4) == 0)
        {
            synthdrum_set_filter_freq(synth, val);
        }
        else if (strncmp("q", wurds[0], 1) == 0)
        {
            synthdrum_set_filter_q(synth, val);
        }
    }
    return true;
}

bool parse_step_sequencer_command(int soundgen_num, int target_pattern_num,
                                  char wurds[][SIZE_OF_WURD], int num_wurds)
{
    soundgenerator *sg = mixr->sound_generators[soundgen_num];
    sequencer *seq;
    if (mixr->sound_generators[soundgen_num]->type == SEQUENCER_TYPE)
    {
        sample_sequencer *s = (sample_sequencer *)sg;
        seq = &s->m_seq;
    }
    else if (mixr->sound_generators[soundgen_num]->type == SYNTHDRUM_TYPE)
    {
        synthdrum_sequencer *s = (synthdrum_sequencer *)sg;
        seq = &s->m_seq;
    }

    bool cmd_found = true;

    if (target_pattern_num != -1)
    {
        if (seq_is_valid_pattern_num(seq, target_pattern_num))
        {
            if (strncmp("print", wurds[2], 5) == 0)
                seq_print_pattern(seq, target_pattern_num);
            else if (strncmp("numloops", wurds[2], 8) == 0)
            {
                int numloops = atoi(wurds[3]);
                if (numloops != 0)
                {
                    seq_change_num_loops(seq, target_pattern_num, numloops);
                }
            }
            else if (strncmp("swing", wurds[2], 5) == 0)
            {
                int swing_setting = atoi(wurds[3]);
                seq_swing_pattern(seq, target_pattern_num, swing_setting);
            }
            else
            {
                soundgenerator *sg = mixr->sound_generators[soundgen_num];
                check_and_set_pattern(sg, target_pattern_num, BEAT_PATTERN,
                                      &wurds[2], num_wurds - 2);
            }
        }
    }
    else
    {
        if (strncmp("generate", wurds[2], 8) == 0 ||
            strncmp("gen", wurds[2], 3) == 0)
        {
            if (strncmp("every", wurds[3], 5) == 0)
            {
                int num_gens = atoi(wurds[4]);
                if (num_gens > 0)
                {
                    seq_set_generate_mode(seq, true);
                    seq->generate_every_n_loops = num_gens;
                }
                else
                {
                    printf("Need a number for every 'n'\n");
                }
            }
            else if (strncmp("for", wurds[3], 3) == 0)
            {
                int num_gens = atoi(wurds[4]);
                if (num_gens > 0)
                {
                    seq_set_generate_mode(seq, true);
                    seq_set_max_generations(seq, num_gens);
                }
                else
                {
                    printf("Need a number for 'for'\n");
                }
            }
            else if (strncmp("source", wurds[3], 6) == 0 ||
                     strncmp("src", wurds[3], 3) == 0)
            {
                int generate_src = atoi(wurds[4]);
                if (mixer_is_valid_seq_gen_num(mixr, generate_src))
                {
                    seq_set_generate_src(seq, generate_src);
                }
                else
                    printf("not a valid generate SRC: %d\n", generate_src);
            }
            else
            {
                seq_set_generate_mode(seq, 1 - seq->generate_mode);
            }
        }
        else if (strncmp("multi", wurds[2], 5) == 0)
        {
            bool b = atoi(wurds[3]);
            seq_set_multi_pattern_mode(seq, b);
            printf("Sequencer multi mode : %s\n",
                   seq->multi_pattern_mode ? "true" : "false");
        }
        else if (strncmp("randamp", wurds[2], 6) == 0)
        {
            seq_set_randamp(seq, 1 - seq->randamp_on);
            printf("Toggling randamp to %s \n",
                   seq->randamp_on ? "true" : "false");
        }
        else if (strncmp("visualize", wurds[2], 9) == 0)
        {
            bool b = atoi(wurds[3]);
            printf("Setting visualize to %s\n", b ? "true" : "false");
            seq->visualize = b;
        }
        else
            cmd_found = false;
    }
    return cmd_found;
}
