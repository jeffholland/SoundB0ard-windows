#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

enum sequence_gen_type
{
    EUCLIDEAN,
    BITSHIFT,
    MARKOV,
    RECURSIVE_SEQ_GEN,
};

typedef struct sequence_generator
{
    unsigned int type;
    bool debug;
    void (*status)(void *self, wchar_t *wstring);
    uint16_t (*generate)(void *self, void *data);
    void (*set_debug)(void *self, bool b);
    void (*event_notify)(void *self, unsigned int event_type);
} sequence_generator;
