#ifndef CHS_HEART_RATE_H
#define CHS_HEART_RATE_H

/**
 * @brief Check whether a heart rate is within the supported range.
 * @param hr Heart rate in beats per minute.
 * @return hr if strictly between 45 and 190; otherwise 0.
 */
int heart_rate_validate(int hr);

/**
 * @brief Parse and validate an integer heart-rate argument.
 * @param text Null-terminated decimal text, or NULL.
 * @return Valid heart rate in bpm, or 0 for invalid text or an unsupported value.
 */
int heart_rate_parse(const char *text);

#endif
