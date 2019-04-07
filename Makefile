CC = clang++
SRC = $(wildcard *.cpp) $(wildcard */*.cpp)
OBJDIR = obj
OBJ = $(patsubst %.cpp, $(OBJDIR)/%.o, $(SRC))

LIBS=-lportaudio -lportmidi -lreadline -lm -lpthread -lsndfile -lprofiler -llo

ABLETONASIOINC=-I/Users/sideboard/Code/link/modules/asio-standalone/asio/include
INCDIRS=-I/Users/sideboard/homebrew/Cellar/readline/7.0.3_1/include -I/usr/local/include -Iinclude -Iinclude/afx -Iinclude/stack -I/Users/sideboard/Code/link/include -I/Users/sideboard/homebrew/include
LIBDIR=/usr/local/lib
HOMEBREWLIBDIR=/Users/sideboard/homebrew/lib
READLINELIBDIR=/Users/sideboard/homebrew/Cellar/readline/7.0.3_1/lib
WARNFLASGS = -Wall -Wextra -pedantic -Wstrict-prototypes -Wmissing-prototypes
CPPFLAGS = -std=c++11 $(WARNFLAGS) -g -pg $(INCDIRS) $(ABLETONASIOINC) -O3 -fsanitize=address

$(OBJDIR)/%.o: %.cpp
	$(CC) -c -o $@ $< $(CPPFLAGS)

TARGET = sbsh

.PHONE: depend clean

all: objdir $(TARGET)
	@ctags -R *
	@cscope -b
	@echo "\n\x1b[37mBoom! make some noise...\x1b[0m"

objdir:
	mkdir -p obj/fx obj/filterz obj/pattern_transformers obj/cmdloop obj/pattern_generators obj/value_generators obj/afx

$(TARGET): $(OBJ)
	$(CC) $(CPPFLAGS) -L$(READLINELIBDIR) -L$(LIBDIR) -L$(HOMEBREWLIBDIR) -o $@ $^ $(LIBS) $(INCS)

clean:
	rm -f *.o *~ $(TARGET)
	find $(OBJDIR) -name "*.o" -exec rm {} \;
	@echo "\n\x1b[37mCleannnnd..!\x1b[0m"

format:
	find . -type f -name "*.[ch]" -exec clang-format -i {} \;
