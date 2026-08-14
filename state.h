#ifndef ARG_STATE_H
#define ARG_STATE_H

#define ARG_STATE_MAGIC 0x41524731u

typedef struct {
    unsigned int magic;

    int stage;
    int audioEvent;
    int glitchCount;
    int interactionCount;

    unsigned int seed;

    int shutdownRequested;
} ARGState;

#endif