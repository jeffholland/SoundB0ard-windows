#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <defjams.h>
#include <fx/basicfilterpass.h>
#include <fx/bitcrush.h>
#include <fx/distortion.h>
#include <fx/dynamics_processor.h>
#include <fx/fx.h>
#include <fx/modfilter.h>
#include <fx/modular_delay.h>
#include <fx/reverb.h>
#include <fx/stereodelay.h>
#include <fx/waveshaper.h>
#include <soundgenerator.h>

SoundGenerator::SoundGenerator(){};

// extern mixer *mixr;

double SoundGenerator::GetVolume() { return volume; }

void SoundGenerator::SetVolume(double val)
{
    if (val >= 0.0 && val <= 1.0)
        volume = val;
}

void SoundGenerator::start() { active = true; }
void SoundGenerator::stop() { active = false; }

void SoundGenerator::parseMidiEvent(midi_event ev, mixer_timing_info tinfo)
{
    // if (lo_send(mixr->processing_addr, "/img", "i", sg->mixer_idx) == -1)
    //{
    //    printf("OSC error %d: %s\n", lo_address_errno(mixr->processing_addr),
    //           lo_address_errstr(mixr->processing_addr));
    //}

    int cur_midi_tick = tinfo.midi_tick % PPBAR;
    int midi_note = ev.data1;
    // bool is_chord_mode = false;

    // if (!is_midi_note_in_key(midi_note, mixr->key))
    //{
    //    // TODO fix!
    //    // std::cout << "nah mate, not in key: " << midi_note << " " <<
    //    // mixr->key
    //    //          << std::endl;
    //    // return;
    //}

    if (engine.transpose != 0)
        midi_note += engine.transpose;

    if (!ev.delete_after_use || ev.source == EXTERNAL_DEVICE)
    {
        if (ev.event_type == MIDI_ON)
            arp_add_last_note(&engine.arp, midi_note);
    }

    int midi_notes[3] = {midi_note, 0, 0};
    int midi_notes_len = 1; // default single note
    if (engine.chord_mode)
    {
        midi_notes_len = 3;
        if (tinfo.chord_type == MAJOR_CHORD)
            midi_notes[1] = midi_note + 4;
        else
            midi_notes[1] = midi_note + 3;
        midi_notes[2] = midi_note + 7;
    }

    switch (ev.event_type)
    {
    case (MIDI_ON):
    { // Hex 0x80
        if (!sequence_engine_is_masked(&engine))
        {

            for (int i = 0; i < midi_notes_len; i++)
            {
                int note = midi_notes[i];

                noteOn(ev);

                if (ev.source != EXTERNAL_DEVICE) // artificial note-off
                {
                    int sustain_ms = ev.hold ? ev.hold : engine.sustain_note_ms;
                    int sustain_time_in_ticks =
                        sustain_ms * tinfo.ms_per_midi_tick;

                    int note_off_tick =
                        (cur_midi_tick + sustain_time_in_ticks) % PPBAR;
                    midi_event off = new_midi_event(MIDI_OFF, note, 128);
                    off.delete_after_use = true;
                    sequence_engine_add_temporal_event(&engine, note_off_tick,
                                                       off);
                }
            }
        }
        break;
    }
    case (MIDI_OFF):
    { // Hex 0x90
        for (int i = 0; i < midi_notes_len; i++)
        {
            noteOff(ev);
        }
        break;
    }
    case (MIDI_CONTROL):
    { // Hex 0xB0
        control(ev);
        break;
    }
    case (MIDI_PITCHBEND):
    { // Hex 0xE0
        pitchBend(ev);
        break;
    }
    default:
        std::cout
            << "HERE PAL, I've NAE IDEA WHIT KIND OF MIDI EVENT THAT WiS! "
            << ev << std::endl;
    }

    if (ev.delete_after_use)
    {
        midi_event_clear(&ev);
    }
}

void SoundGenerator::eventNotify(broadcast_event event, mixer_timing_info tinfo)
{
    (void)event;
    int idx = tinfo.midi_tick % PPBAR;

    // this temporal_events table is my first pass at a solution to
    // ensure note off events still happen, even when i'm using the
    // above count_by which ends up not reaching note off events
    // sometimes.
    if (engine.temporal_events[idx].event_type)
    {
        midi_event ev = engine.temporal_events[idx];
        noteOff(ev);
        midi_event_clear(&engine.temporal_events[idx]);
    }

    if (!active)
        return;

    if (tinfo.is_start_of_loop)
    {
        engine.started = true;
        engine.event_mask_counter++;
        if (engine.restore_pending)
        {
            sequence_engine_dupe_pattern(
                &engine.backup_pattern_while_getting_crazy,
                &engine.patterns[engine.cur_pattern]);
            engine.restore_pending = false;
        }
        else if (engine.multi_pattern_mode && engine.num_patterns > 1)
        {
            engine.cur_pattern_iteration--;
            if (engine.cur_pattern_iteration <= 0)
            {
                int next_pattern =
                    (engine.cur_pattern + 1) % engine.num_patterns;

                engine.cur_pattern = next_pattern;
                engine.cur_pattern_iteration =
                    engine.pattern_multiloop_count[engine.cur_pattern];
            }
        }
    }
    if (engine.started)
    {
        if (tinfo.is_sixteenth)
        {
            if (engine.debug)
                printf("CUR_STEP:%d range_start:%d len:%d\n", engine.cur_step,
                       engine.range_start, engine.range_len);

            if (engine.fold_direction == FOLD_FWD)
                engine.cur_step += engine.count_by;
            else
                engine.cur_step -= engine.count_by;

            if (engine.cur_step < engine.range_start || engine.cur_step < 0)
            {
                int over_by = engine.range_start - engine.cur_step;
                if (engine.fold)
                {
                    engine.cur_step = engine.range_start + over_by;
                    engine.fold_direction = FOLD_FWD;
                }
                else
                    engine.cur_step =
                        (engine.range_start + engine.range_len) - over_by - 1;
            }
            else if (engine.cur_step >= 16 ||
                     engine.cur_step >= (engine.range_start + engine.range_len))
            {
                int over_by = 0;
                if (engine.cur_step >= 16)
                    over_by = engine.cur_step - 16;
                else
                    over_by = engine.cur_step -
                              (engine.range_start + engine.range_len);

                if (engine.fold)
                {
                    engine.cur_step =
                        (engine.range_start + engine.range_len) - over_by - 1;
                    engine.fold_direction = FOLD_BAK;
                }
                else
                    engine.cur_step = engine.range_start + over_by;
            }

            int tries = 0;
            while (engine.cur_step >= 16 && tries < 5)
            {
                engine.cur_step -= 16;
                tries++;
            }
            while (engine.cur_step < 0 && tries < 5)
            {
                engine.cur_step += 16;
                tries++;
            }

            if (engine.cur_step >= 16 || engine.cur_step < 0)
                printf("UGH! still out of bounds! %d\n", engine.cur_step);

            engine.range_counter++;
            if (engine.range_counter % engine.range_len == 0)
            {
                engine.range_start += engine.increment_by;
                if (engine.range_start >= 16)
                    engine.range_start -= 16;
                else if (engine.range_start < 0)
                    engine.range_start += 16;
            }

            if (engine.arp.enable && engine.arp.speed == ARP_16)
            {
                midi_event ev{};
                if (sequence_engine_do_arp(&engine, &ev))
                    noteOn(ev);
            }
        }

        if (tinfo.is_thirtysecond)
        {
            if (engine.arp.enable && engine.arp.speed == ARP_32)
            {
                midi_event ev{};
                if (sequence_engine_do_arp(&engine, &ev))
                    noteOn(ev);
            }
        }

        if (tinfo.is_twentyfourth)
        {
            if (engine.arp.enable && engine.arp.speed == ARP_24)
            {
                midi_event ev{};
                if (sequence_engine_do_arp(&engine, &ev))
                    noteOn(ev);
            }
        }
        if (tinfo.is_twelth)
        {
            if (engine.arp.enable && engine.arp.speed == ARP_12)
            {
                midi_event ev{};
                if (sequence_engine_do_arp(&engine, &ev))
                    noteOn(ev);
            }
        }
        if (tinfo.is_eighth)
        {
            if (engine.arp.enable && engine.arp.speed == ARP_8)
            {
                midi_event ev{};
                if (sequence_engine_do_arp(&engine, &ev))
                    noteOn(ev);
            }
        }
        if (tinfo.is_sixth)
        {
            if (engine.arp.enable && engine.arp.speed == ARP_6)
            {
                midi_event ev{};
                if (sequence_engine_do_arp(&engine, &ev))
                    noteOn(ev);
            }
        }
        if (tinfo.is_quarter)
        {
            if (engine.arp.enable && engine.arp.speed == ARP_4)
            {
                midi_event ev{};
                if (sequence_engine_do_arp(&engine, &ev))
                    noteOn(ev);
            }
        }
        if (tinfo.is_third)
        {
            if (engine.arp.enable && engine.arp.speed == ARP_3)
            {
                midi_event ev{};
                if (sequence_engine_do_arp(&engine, &ev))
                    noteOn(ev);
            }
        }

        int idx = ((engine.cur_step * PPSIXTEENTH) +
                   (tinfo.midi_tick % PPSIXTEENTH)) %
                  PPBAR;

        if (idx < 0 || idx >= PPBAR)
            printf("YOUHC! idx out of bounds: %d\n", idx);

        if (engine.patterns[engine.cur_pattern][idx].event_type)
        {
            midi_event ev = engine.patterns[engine.cur_pattern][idx];
            if (rand() % 100 < engine.pct_play)
            {
                // if (ev->event_type == MIDI_ON)
                //    mixer_emit_event(mixr, (broadcast_event){
                //                               .type = SEQUENCER_NOTE,
                //                               .sequencer_src =
                //                               mixer_idx});
                parseMidiEvent(ev, tinfo);
            }
        }

    } // end if engine.started
}

void SoundGenerator::noteOffDelayed(midi_event ev, int event_off_tick)
{
    sequence_engine_add_temporal_event(&engine, event_off_tick, ev);
}

double SoundGenerator::GetPan() { return pan; }

void SoundGenerator::SetPan(double val)
{
    if (val >= -1.0 && val <= 1.0)
        pan = val;
}

static int soundgen_add_fx(SoundGenerator *self, Fx *f)
{

    if (self->effects_num < kMaxNumSoundGenFx)
    {
        self->effects[self->effects_num] = f;
        printf("done adding effect\n");
        return self->effects_num++;
    }

    return -1;
}

int add_delay_soundgen(SoundGenerator *self, float duration)
{
    printf("Booya, adding a new DELAY to "
           "SoundGenerator: %f!\n",
           duration);
    StereoDelay *sd = new StereoDelay(duration);
    return soundgen_add_fx(self, (Fx *)sd);
}

int add_reverb_soundgen(SoundGenerator *self)
{
    printf("Booya, adding a new REVERB to "
           "SoundGenerator!\n");
    Reverb *r = new Reverb();
    return soundgen_add_fx(self, (Fx *)r);
}

int add_waveshape_soundgen(SoundGenerator *self)
{
    printf("WAVshape\n");
    WaveShaper *ws = new WaveShaper();
    return soundgen_add_fx(self, (Fx *)ws);
}

int add_basicfilter_soundgen(SoundGenerator *self)
{
    printf("Fffuuuuhfilter!\n");
    FilterPass *fp = new FilterPass();
    return soundgen_add_fx(self, (Fx *)fp);
}

int add_bitcrush_soundgen(SoundGenerator *self)
{
    printf("BITCRUSH!\n");
    BitCrush *bc = new BitCrush();
    return soundgen_add_fx(self, (Fx *)bc);
}

int add_compressor_soundgen(SoundGenerator *self)
{
    printf("COMPresssssion!\n");
    DynamicsProcessor *dp = new DynamicsProcessor();
    return soundgen_add_fx(self, (Fx *)dp);
}

int add_moddelay_soundgen(SoundGenerator *self)
{
    printf("Booya, adding a new MODDELAY to "
           "SoundGenerator!\n");
    ModDelay *md = new ModDelay();
    return soundgen_add_fx(self, (Fx *)md);
}

int add_modfilter_soundgen(SoundGenerator *self)
{
    printf("Booya, adding a new MODFILTERRRRR to "
           "SoundGenerator!\n");
    ModFilter *mf = new ModFilter();
    return soundgen_add_fx(self, (Fx *)mf);
}

int add_distortion_soundgen(SoundGenerator *self)
{
    printf("BOOYA! Distortion all up in this kittycat\n");
    Distortion *d = new Distortion();
    return soundgen_add_fx(self, (Fx *)d);
}

int add_envelope_soundgen(SoundGenerator *self)
{
    printf("Booya, adding a new envelope to "
           "SoundGenerator!\n");
    Envelope *e = new Envelope();
    return soundgen_add_fx(self, (Fx *)e);
}

stereo_val effector(SoundGenerator *self, stereo_val val)
{
    int num_fx = self->effects_num.load();
    for (int i = 0; i < num_fx; i++)
    {
        Fx *f = self->effects[i];
        if (f && f->enabled_)
        {
            val = f->Process(val);
        }
    }
    return val;
}

bool is_synth(SoundGenerator *self)
{
    if (self->type == MINISYNTH_TYPE || self->type == DIGISYNTH_TYPE ||
        self->type == DXSYNTH_TYPE)
        return true;

    return false;
}

bool is_stepper(SoundGenerator *self)
{
    if (self->type == DRUMSYNTH_TYPE || self->type == DRUMSAMPLER_TYPE)
        return true;
    return false;
}
