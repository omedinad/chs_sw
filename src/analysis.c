#include "analysis.h"
#include "csv_reader.h"
#include "samples.h"
#include "statistics.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Read a nonempty text file into a newly allocated, NUL-terminated buffer.
 * @param name Path to the input file.
 * @param[out] bytes Buffer on success; the caller must free it. Do not use on failure.
 * @param[out] length Byte count excluding the added NUL, valid only on success.
 * @return 0 on success, or -1 with errno describing the failure.
 * @note Files larger than INT_MAX and files containing embedded NUL bytes are rejected.
 */
static int read_input_file(const char *name, char **bytes, size_t *length) {
    FILE *file = fopen(name, "rb");
    if (!file) return -1;
    long size;
    if (fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 || size > INT_MAX ||
        fseek(file, 0, SEEK_SET)) {
        fclose(file);
        errno = EINVAL;
        return -1;
    }
    *bytes = malloc((size_t)size + 1);
    if (!*bytes) { fclose(file); return -1; }
    *length = fread(*bytes, 1, size, file);
    int failed = ferror(file) || *length != (size_t)size;
    if (fclose(file)) failed = 1;
    if (failed) { free(*bytes); *bytes = NULL; errno = EIO; return -1; }
    (*bytes)[*length] = '\0';
    if (memchr(*bytes, '\0', *length)) {
        free(*bytes); *bytes = NULL; errno = EINVAL; return -1;
    }
    return 0;
}

/** @copydoc analyze_file */
int analyze_file(const char *path, const CliOptions *options, WorkoutStatistics *result) {
    char *bytes = NULL;
    size_t length = 0;
    Sample head = {0};
    WorkoutStatistics stats = {0};
    const float window[] = {0.010333864f, 0.207561207f, 0.564209858f, 0.207561207f, 0.010333864f};
    const int window_len = 5;
    if (read_input_file(path, &bytes, &length) ||
        csv_load_samples(&stats.record_count, &head, bytes, &length) ||
        statistics_smooth_heart_rate(&head, window, &window_len)) {
        int saved_errno = errno;
        free(bytes);
        samples_free(&head);
        errno = saved_errno;
        return -1;
    }
    stats.input_bytes = length;
    stats.pace_matches = statistics_average_pace_at_heart_rate(
        &head, &options->target_heart_rate, 1, &stats.average_pace);
    stats.heart_rate_matches = statistics_average_heart_rate_at_pace(
        &head, &options->target_pace, 0.01f, &stats.average_heart_rate);
    free(bytes);
    samples_free(&head);
    *result = stats;
    return 0;
}
