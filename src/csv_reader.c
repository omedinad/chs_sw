#include "csv_reader.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Find a column in the first CSV header line without modifying the buffer.
 * @param name Exact, case-sensitive column name.
 * @param bytes Buffer containing the header; a terminating NUL is not required.
 * @param len Number of accessible bytes in the buffer.
 * @return Zero-based column index, or -1 if absent or arguments are invalid.
 */
static int csv_find_column(const char *name, const char *bytes, int len) {
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
 * @brief Parse a finite floating-point CSV field.
 * @param text Null-terminated numeric text; surrounding whitespace is accepted.
 * @param[out] value Non-NULL destination; its contents are unspecified on failure.
 * @return 0 on success, or -1 for missing, malformed, nonfinite, or out-of-range input.
 */
static int csv_parse_number(const char *text, float *value) {
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
 * @note On success, release allocated nodes with samples_free(). Parse failures
 * clear the list and count; invalid arguments return immediately. Invalid data sets errno to EINVAL.
 */
int csv_load_samples(int *count, Sample *head, char *bytes, const size_t *len) {
    if (!count || !head || !bytes || !len || *len > INT_MAX) {
        errno = EINVAL;
        return -1;
    }
    *count = 0;
    int hr_col = csv_find_column("heartrate", bytes, (int)*len);
    int dist_col = csv_find_column("dist", bytes, (int)*len);
    int time_col = csv_find_column("time", bytes, (int)*len);
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
            if (column == hr_col) { if (csv_parse_number(field, &hr)) goto invalid; found++; }
            if (column == dist_col) { if (csv_parse_number(field, &distance)) goto invalid; found++; }
            if (column == time_col) { if (csv_parse_number(field, &seconds)) goto invalid; found++; }
            column++;
            field = next;
        } while (field);
        if (found != 3 || distance < previous_distance || seconds < 0) goto invalid;
        float delta = distance - previous_distance;
        previous_distance = distance;
        /* Stationary intervals and sensor dropouts do not describe running pace. */
        if (delta <= 0 || seconds <= 0 || hr <= 45 || hr >= 190) continue;
        Sample *node = calloc(1, sizeof(*node));
        if (!node) { samples_free(head); return -1; }
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
    samples_free(head);
    *count = 0;
    errno = EINVAL;
    return -1;
}
