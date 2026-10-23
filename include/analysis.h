#ifndef CHS_ANALYSIS_H
#define CHS_ANALYSIS_H

#include "cli.h"
#include <stddef.h>

/** @brief Per-workout results; matching counts determine which averages are available. */
typedef struct {
    float average_heart_rate; /**< Mean bpm, valid when heart_rate_matches is positive. */
    float average_pace; /**< Mean minutes per kilometer, valid when pace_matches is positive. */
    int heart_rate_matches;
    int pace_matches;
    int record_count; /**< Usable intervals after CSV exclusions. */
    size_t input_bytes; /**< Original file size in bytes. */
} WorkoutStatistics;

/**
 * @brief Load, smooth, and analyze a workout, releasing all temporary resources.
 * @param path CSV input path.
 * @param options Validated analysis targets.
 * @param[out] result Statistics on success; unchanged on failure.
 * @return 0 on success, or -1 with errno describing the failure.
 * @note All pointers must be non-NULL. No matching samples is a successful result.
 */
int analyze_file(const char *path, const CliOptions *options, WorkoutStatistics *result);

#endif
