#include "heart_rate.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

/**
 * @brief Check whether a heart rate is within the supported range.
 * @param hr Heart rate in beats per minute.
 * @return hr if strictly between 45 and 190; otherwise 0.
 */
int heart_rate_validate(int hr) {
    return hr > 45 && hr < 190 ? hr : 0;
}

/**
 * @brief Parse and validate an integer heart-rate argument.
 * @param text Null-terminated decimal text, or NULL.
 * @return Valid heart rate in bpm, or 0 for invalid text or an unsupported value.
 */
int heart_rate_parse(const char *text) {
    char *end;
    if (!text || !*text) return 0;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || *end || value < INT_MIN || value > INT_MAX) return 0;
    return heart_rate_validate((int)value);
}
