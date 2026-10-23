#include <get_goals_rubi_csv.h>
#include <ctype.h>
#include <limits.h>

/**
 * @brief Check whether a heart rate is within the supported range.
 * @param hr Heart rate in beats per minute.
 * @return hr if strictly between 45 and 190; otherwise 0.
 */
int validate_int_HR(int hr) {
    return hr > 45 && hr < 190 ? hr : 0;
}

/**
 * @brief Parse and validate an integer heart-rate argument.
 * @param text Null-terminated decimal text, or NULL.
 * @return Valid heart rate in bpm, or 0 for invalid text or an unsupported value.
 */
int validate_str_HR(char *text) {
    char *end;
    if (!text || !*text) return 0;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || *end || value < INT_MIN || value > INT_MAX) return 0;
    return validate_int_HR((int)value);
}

/**
 * @brief Parse a positive pace with fractional seconds.
 * @param text Pace in mm:ss.d or mm:ss.dd format; minutes and seconds must be below 60.
 * @param[out] pace Parsed pace; dec stores hundredths of a second. Unchanged on failure.
 * @return 1 on success, or -1 for invalid input.
 */
int validate_str_time(const char *text, Pace *pace) {
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
char *pace_to_str(Pace *pace) {
    char *result = malloc(48);
    if (result) snprintf(result, 48, "%d:%02d.%02d", pace->min, pace->sec, pace->dec);
    return result;
}

/**
 * @brief Find a column in the first CSV header line without modifying the buffer.
 * @param name Exact, case-sensitive column name.
 * @param bytes Buffer containing the header; a terminating NUL is not required.
 * @param len Number of accessible bytes in the buffer.
 * @return Zero-based column index, or -1 if absent or arguments are invalid.
 */
int get_field_number(const char *name, const char *bytes, int len) {
    int start = 0, column = 0;
    if (!name || !bytes || len <= 0) return -1;
    for (int i = 0; i <= len; i++) {
        if (i == len || bytes[i] == ',' || bytes[i] == '\n' || bytes[i] == '\r') {
            if ((size_t)(i - start) == strlen(name) && !memcmp(bytes + start, name, i - start))
                return column;
            if (i == len || bytes[i] != ',') break;
            column++;
            start = i + 1;
        }
    }
    return -1;
}

/**
 * @brief Convert decimal minutes to a pace rounded to hundredths of a second.
 * @param[out] pace Non-NULL destination, including carry into seconds and minutes.
 * @param minutes Pace in minutes per kilometer.
 * @note Nonfinite, nonpositive, or out-of-range input produces a zero pace.
 */
void float_to_Pace(Pace *pace, float minutes) {
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
float pace_to_float(const Pace *pace) {
    return pace->min + pace->sec / 60.0f + pace->dec / 6000.0f;
}

/**
 * @brief Free every data node and reset the caller-owned sentinel.
 * @param[in,out] head Sentinel whose nodes were dynamically allocated, or NULL.
 * @note The sentinel itself is never freed; NULL is a no-op.
 */
void freeSamples(Sample *head) {
    if (!head) return;
    Sample *node = head->next;
    while (node) {
        Sample *next = node->next;
        free(node);
        node = next;
    }
    *head = (Sample){0};
}

/**
 * @brief Parse a finite floating-point CSV field.
 * @param text Null-terminated numeric text; surrounding whitespace is accepted.
 * @param[out] value Non-NULL destination; its contents are unspecified on failure.
 * @return 0 on success, or -1 for missing, malformed, nonfinite, or out-of-range input.
 */
static int number(const char *text, float *value) {
    char *end;
    if (!text || !*text) return -1;
    errno = 0;
    *value = strtof(text, &end);
    while (isspace((unsigned char)*end)) end++;
    return errno || *end || !isfinite(*value) ? -1 : 0;
}

/**
 * @brief Parse a workout CSV into a doubly linked list of usable intervals.
 * @param[out] count Number of usable intervals on success.
 * @param[in,out] head Caller-owned, empty sentinel initialized to {0}.
 * @param[in,out] bytes Writable, NUL-terminated CSV buffer; tokenized in place.
 * @param len Non-NULL pointer to the byte count, excluding the terminating NUL.
 * @return 0 on success, or -1 for invalid input or allocation failure.
 * @details dist is cumulative meters, time is interval seconds, and heartrate is bpm.
 * Stationary intervals, zero durations, and unsupported heart rates are skipped.
 * @note On success, release allocated nodes with freeSamples(). Parse failures
 * clear the list and count; invalid arguments return immediately. Invalid data sets errno to EINVAL.
 */
int loadValues(int *count, Sample *head, char *bytes, const size_t *len) {
    if (!count || !head || !bytes || !len || *len > INT_MAX) {
        errno = EINVAL;
        return -1;
    }
    *count = 0;
    int hr_col = get_field_number("heartrate", bytes, (int)*len);
    int dist_col = get_field_number("dist", bytes, (int)*len);
    int time_col = get_field_number("time", bytes, (int)*len);
    if (hr_col < 0 || dist_col < 0 || time_col < 0) goto invalid;
    Sample *tail = head;
    float previous_distance = 0;
    char *save, *line = strtok_r(bytes, "\n", &save);
    while (line && (line = strtok_r(NULL, "\n", &save))) {
        float hr = 0, distance = 0, seconds = 0;
        int found = 0, column = 0;
        char *field = line;
        do {
            char *next = strchr(field, ',');
            if (next) *next++ = '\0';
            if (column == hr_col) { if (number(field, &hr)) goto invalid; found++; }
            if (column == dist_col) { if (number(field, &distance)) goto invalid; found++; }
            if (column == time_col) { if (number(field, &seconds)) goto invalid; found++; }
            column++;
            field = next;
        } while (field);
        if (found != 3 || distance < previous_distance || seconds < 0) goto invalid;
        float delta = distance - previous_distance;
        previous_distance = distance;
        /* Stationary intervals and sensor dropouts do not describe running pace. */
        if (delta <= 0 || seconds <= 0 || hr <= 45 || hr >= 190) continue;
        Sample *node = calloc(1, sizeof(*node));
        if (!node) { freeSamples(head); return -1; }
        node->distance = delta;
        node->time = seconds;
        node->hr = hr;
        node->speed = delta / seconds * 3.6f;
        node->pace_min = seconds / delta * (1000.0f / 60.0f);
        if (!isfinite(node->speed) || !isfinite(node->pace_min) ||
            node->speed <= 0 || node->pace_min <= 0) {
            free(node);
            goto invalid;
        }
        node->prev = tail;
        tail->next = node;
        tail = node;
        (*count)++;
    }
    if (*count == 0) goto invalid;
    return 0;
invalid:
    freeSamples(head);
    *count = 0;
    errno = EINVAL;
    return -1;
}

/**
 * @brief Check that a smoothing kernel has odd length and normalized weights.
 * @param window Array of finite, nonnegative weights.
 * @param len Pointer to a positive, odd number of weights.
 * @return 1 if the weight sum differs from one by less than 1e-6; otherwise 0.
 */
int validateWindow(const float *window, const int *len) {
    if (!window || !len || *len <= 0 || *len % 2 == 0) return 0;
    double sum = 0;
    for (int i = 0; i < *len; i++) {
        if (!isfinite(window[i]) || window[i] < 0) return 0;
        sum += window[i];
    }
    return fabs(sum - 1.0) < 1e-6;
}

/**
 * @brief Smooth heart rate using a centered kernel and original sample readings.
 * @param[in,out] head Non-NULL sentinel of the sample list.
 * @param window Normalized kernel accepted by validateWindow().
 * @param len Pointer to the kernel length.
 * @return 0 on success, including lists shorter than the kernel; -1 for invalid
 * arguments, an invalid kernel, or allocation failure.
 * @note Edge samples without a complete window remain unchanged.
 */
int lowPassHR(Sample *head, const float *window, const int *len) {
    if (!head || !validateWindow(window, len)) return -1;
    size_t count = 0;
    for (Sample *node = head->next; node; node = node->next) count++;
    if (count < (size_t)*len) return 0;
    float *original = malloc(count * sizeof(*original));
    if (!original) return -1;
    size_t i = 0;
    for (Sample *node = head->next; node; node = node->next) original[i++] = node->hr;
    i = 0;
    size_t half = *len / 2;
    for (Sample *node = head->next; node; node = node->next, i++) {
        if (i < half || i + half >= count) continue;
        double sum = 0;
        for (int j = 0; j < *len; j++) sum += original[i - half + j] * window[j];
        node->hr = sum;
    }
    free(original);
    return 0;
}

/**
 * @brief Average pace for samples within an inclusive heart-rate tolerance.
 * @param head Non-NULL sentinel of the sample list.
 * @param target Non-NULL target heart rate in bpm.
 * @param tolerance Allowed absolute heart-rate difference in bpm; must be nonnegative.
 * @param[out] result Non-NULL destination for mean pace in min/km, or 0 when no samples match.
 * @return Number of matching samples; each sample receives equal weight.
 */
int average_pace_at_HR(Sample *head, const int *target, int tolerance, float *result) {
    int count = 0;
    double sum = 0;
    *result = 0;
    for (Sample *node = head->next; node; node = node->next) {
        if (fabsf(node->hr - *target) <= tolerance) { sum += node->pace_min; count++; }
    }
    if (count) *result = sum / count;
    return count;
}

/**
 * @brief Average heart rate for samples within an inclusive relative pace tolerance.
 * @param head Non-NULL sentinel of the sample list.
 * @param target Non-NULL target pace.
 * @param tolerance Nonnegative fractional tolerance; 0.01 means one percent.
 * @param[out] result Non-NULL destination for mean heart rate in bpm, or 0 when no samples match.
 * @return Number of matching samples; each sample receives equal weight.
 */
int average_hr_at_Pace(Sample *head, const Pace *target, float tolerance, float *result) {
    int count = 0;
    double sum = 0;
    float center = pace_to_float(target);
    *result = 0;
    for (Sample *node = head->next; node; node = node->next) {
        if (fabsf(node->pace_min - center) <= center * tolerance) { sum += node->hr; count++; }
    }
    if (count) *result = sum / count;
    return count;
}
