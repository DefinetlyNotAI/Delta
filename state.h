#ifndef ARG_STATE_H
#define ARG_STATE_H

typedef struct {
    unsigned int magic;
    int stage;
    int audioEvent;
    int glitchCount;
    int interactionCount;
    unsigned int seed;
} ARGState;

#define ARG_STATE_MAGIC 0x41524731

#endif