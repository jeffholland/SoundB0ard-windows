CC = clang++
OBJ = \
	afx/biquad_lpf.o \
	afx/combfilter.o \
	afx/delay.o \
	afx/delayapf.o  \
	afx/envelope_follower.o \
	afx/limiter.o \
	afx/lpfcombfilter.o \
	afx/onepolelpf.o \
	algorithm.o \
	arpeggiator.o \
	audioutils.o \
	beatrepeat.o \
	chaosmonkey.o \
	cmdloop.o \
	dca.o \
	ddlmodule.o \
	delayline.o \
	sample_sequencer.o \
	sequencer.o \
	sequencer_utils.o \
	fx.o \
	envelope.o \
	envelope_generator.o \
	filter.o \
	filter_ckthreefive.o \
	filter_moogladder.o \
	filter_onepole.o \
	filter_sem.o \
	help.o \
	keys.o \
	lfo.o \
	main.o \
	midi_freq_table.o \
	midimaaan.o \
	minisynth.o \
	minisynth_voice.o \
	mixer.o \
	modmatrix.o \
	modfilter.o \
	modular_delay.o \
	obliquestrategies.o \
	oscillator.o \
	qblimited_oscillator.o \
	reverb.o \
	looper.o \
	sbmsg.o \
	sparkline.o \
	spork.o \
	stereodelay.o \
	sound_generator.o \
	synthdrum_sequencer.o \
	table.o \
	utils.o \
	voice.o \
	wt_oscillator.o \

LIBS=-lportaudio -lportmidi -lreadline -lm -lpthread -lsndfile

INCDIR=/usr/local/include
LIBDIR=/usr/local/lib
CFLAGS = -std=c11 -Wall -Wextra -pedantic -Wstrict-prototypes -Wmissing-prototypes -g -I$(INCDIR)

%.o: %.c
	$(CC) -c -o $@ -x c $< $(CFLAGS)

TARGET = sbsh

.PHONE: depend clean

all: $(TARGET)
	@ctags -R *
	@echo "\n\x1b[37mBoom! make some noise...\x1b[0m"

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -L$(LIBDIR) -o $@ $^ $(LIBS) $(INCS)

clean:
	rm -f *.o *~ $(TARGET)
	@echo "\n\x1b[37mCleannnnd..!\x1b[0m"

format:
	clang-format -i *{c,h}
