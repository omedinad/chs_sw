#include "pace.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Parse a positive pace with fractional seconds.
 * @param text Pace in mm:ss.d or mm:ss.dd format; minutes and seconds must be below 60.
 * @param[out] pace Parsed pace; dec stores hundredths of a second. Unchanged on failure.
 * @return 1 on success, or -1 for invalid input.
 */
int pace_parse(const char *text, Pace *pace) {
    if (!text || !pace || !isdigit((unsigned char)*text)) return -1;
    char *end;
    long minutes = strtol(text, &end, 10);
    if (minutes < 0 || minutes >= 60 || *end++ != ':') return -1;
    if (!isdigit((unsigned char)*end)) return -1;
    char *seconds_start = end;
    long seconds = strtol(seconds_start, &end, 10);
    if (seconds < 0 || seconds >= 60 || *end++ != '.') return -1;
    int decimals = 0, digits = 0;
    while (isdigit((unsigned char)*end) && digits < 2) {
        decimals = decimals * 10 + (*end++ - '0');
        digits++;
    }
    if (!digits || *end) return -1;
    if (digits == 1) decimals *= 10;
    if (minutes == 0 && seconds == 0 && decimals == 0) return -1;
    *pace = (Pace){(int)minutes, (int)seconds, decimals};
    return 1;
}

/**
 * @brief Allocate a pace string formatted as minutes:seconds.hundredths.
 * @param pace Non-NULL pace to format.
 * @return Newly allocated string, or NULL if allocation fails.
 * @note The caller must free the returned string.
 */
char *pace_format(const Pace *pace) {
    char *result = malloc(48);
    if (result) snprintf(result, 48, "%d:%02d.%02d", pace->min, pace->sec, pace->dec);
    return result;
}

/**
 * @brief Convert decimal minutes to a pace rounded to hundredths of a second.
 * @param[out] pace Non-NULL destination, including carry into seconds and minutes.
 * @param minutes Pace in minutes per kilometer.
 * @note Nonfinite, nonpositive, or out-of-range input produces a zero pace.
 */
void pace_from_minutes(Pace *pace, float minutes) {
    *pace = (Pace){0, 0, 0};
    if (!isfinite(minutes) || minutes <= 0 || minutes > INT_MAX / 6000.0) return;
    long long hundredths = llround((double)minutes * 6000);
    pace->min = hundredths / 6000;
    pace->sec = hundredths / 100 % 60;
    pace->dec = hundredths % 100;
}

/**
 * @brief Convert a structured pace to decimal minutes per kilometer.
 * @param pace Non-NULL pace with dec expressed in hundredths of a second.
 * @return Pace in minutes per kilometer.
 */
float pace_to_minutes(const Pace *pace) {
    return pace->min + pace->sec / 60.0f + pace->dec / 6000.0f;
}
