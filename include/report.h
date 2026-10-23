#ifndef CHS_REPORT_H
#define CHS_REPORT_H

#include "analysis.h"
#include <stdio.h>

/** @brief Per-file sums with independent denominators; initialize all fields to zero. */
typedef struct {
    int processed_files;
    int heart_rate_files;
    int pace_files;
    double heart_rate_sum;
    double pace_sum;
} ReportSummary;

/**
 * @brief Write the report headings and target values.
 * @param out Writable report stream; errors are recorded by the stream.
 * @param options Validated targets to display.
 */
void report_write_header(FILE *out, const CliOptions *options);

/**
 * @brief Write a workout row, using N/A for statistics without matching samples.
 * @param out Writable report stream; errors are recorded by the stream.
 * @param path Input filename to display.
 * @param stats Successfully computed workout statistics.
 */
void report_write_workout(FILE *out, const char *path, const WorkoutStatistics *stats);

/**
 * @brief Add one successful workout using independent statistic denominators.
 * @param[in,out] summary Accumulator initially zero-initialized by the caller.
 * @param stats Successfully computed workout statistics.
 */
void report_summary_add(ReportSummary *summary, const WorkoutStatistics *stats);

/**
 * @brief Write per-file mean statistics and the successful-file count.
 * @param out Writable report stream; errors are recorded by the stream.
 * @param summary Accumulated results; empty statistics are printed as N/A.
 */
void report_write_summary(FILE *out, const ReportSummary *summary);

/**
 * @brief Rewind and copy a completed report to stdout or an output file.
 * @param report Seekable temporary stream; remains open and owned by the caller.
 * @param output_path File to replace, or NULL for stdout.
 * @return 0 on success, or -1 on a stream, open, write, flush, or close failure.
 * @note Call after reading all inputs. Owned output files are closed; stdout stays open.
 */
int report_publish(FILE *report, const char *output_path);

#endif
