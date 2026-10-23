#ifndef CHS_SAMPLES_H
#define CHS_SAMPLES_H

#include "pace.h"

typedef struct sample {
    Pace pace;
    float distance;
    float time;
    float hr;
    float pace_min;
    float speed;
    struct sample *next;
    struct sample *prev;
} Sample;

/**
 * @brief Free every data node and reset the caller-owned sentinel.
 * @param[in,out] head Sentinel whose nodes were dynamically allocated, or NULL.
 * @note The sentinel itself is never freed; NULL is a no-op.
 */
void samples_free(Sample *head);

#endif
