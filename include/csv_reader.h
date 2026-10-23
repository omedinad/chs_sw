#ifndef CHS_CSV_READER_H
#define CHS_CSV_READER_H

#include "samples.h"
#include <stddef.h>

/**
 * @brief Parse a workout CSV into a doubly linked list of usable intervals.
 * @param[out] count Number of usable intervals on success.
 * @param[in,out] head Caller-owned, empty sentinel initialized to {0}.
 * @param[in,out] bytes Writable, NUL-terminated CSV buffer; tokenized in place.
 * @param len Non-NULL pointer to the byte count, excluding the terminating NUL.
 * @return 0 on success, or -1 for invalid input or allocation failure.
 * @details dist is cumulative meters, time is interval seconds, and heartrate is bpm.
 * Stationary intervals, zero durations, and unsupported heart rates are skipped.
 * @note On success, release allocated nodes with samples_free(). Parse failures
 * clear the list and count; invalid arguments return immediately. Invalid data sets errno to EINVAL.
 */
int csv_load_samples(int *count, Sample *head, char *bytes, const size_t *len);

#endif
