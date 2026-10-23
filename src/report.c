#include "report.h"

/**
 * @brief Write a pace rounded to hundredths of a second to a stream.
 * @param out Open output stream.
 * @param minutes Pace in minutes per kilometer.
 */
static void print_pace(FILE *out, float minutes) {
    Pace pace;
    pace_from_minutes(&pace, minutes);
    fprintf(out, "%d:%02d.%02d", pace.min, pace.sec, pace.dec);
}

/** @copydoc report_write_header */
void report_write_header(FILE *out, const CliOptions *options) {
    fprintf(out, "HR at\t\tPace at\t\tFile\n");
    print_pace(out, pace_to_minutes(&options->target_pace));
    fprintf(out, "\t\t%d\t\tName\n----------------------------------------------\n",
            options->target_heart_rate);
}

/** @copydoc report_write_workout */
void report_write_workout(FILE *out, const char *path, const WorkoutStatistics *stats) {
    if (stats->heart_rate_matches)
        fprintf(out, "%.1f(%d)", stats->average_heart_rate, stats->heart_rate_matches);
    else fprintf(out, "N/A(0)");
    fprintf(out, "\t");
    if (stats->pace_matches) print_pace(out, stats->average_pace);
    else fprintf(out, "N/A");
    fprintf(out, "(%d)\t%s(%zuK, %d Records)\n", stats->pace_matches, path,
            stats->input_bytes / 1024, stats->record_count);
}

/** @copydoc report_summary_add */
void report_summary_add(ReportSummary *summary, const WorkoutStatistics *stats) {
    summary->processed_files++;
    if (stats->heart_rate_matches) {
        summary->heart_rate_sum += stats->average_heart_rate;
        summary->heart_rate_files++;
    }
    if (stats->pace_matches) {
        summary->pace_sum += stats->average_pace;
        summary->pace_files++;
    }
}

/** @copydoc report_write_summary */
void report_write_summary(FILE *out, const ReportSummary *summary) {
    fprintf(out, "----------------------------------------------\n");
    if (summary->heart_rate_files)
        fprintf(out, "%.1f", summary->heart_rate_sum / summary->heart_rate_files);
    else fprintf(out, "N/A");
    fprintf(out, "\t");
    if (summary->pace_files) print_pace(out, summary->pace_sum / summary->pace_files);
    else fprintf(out, "N/A");
    fprintf(out, "\t%d files\n", summary->processed_files);
}

/** @copydoc report_publish */
int report_publish(FILE *report, const char *output_path) {
    if (ferror(report) || fflush(report) || fseek(report, 0, SEEK_SET)) return -1;
    FILE *destination = output_path ? fopen(output_path, "w") : stdout;
    if (!destination) { perror(output_path); return -1; }
    int failed = 0, c;
    while ((c = fgetc(report)) != EOF) {
        if (fputc(c, destination) == EOF) { failed = 1; break; }
    }
    if (ferror(report)) failed = 1;
    if (fflush(destination)) failed = 1;
    if (output_path && fclose(destination)) failed = 1;
    return failed ? -1 : 0;
}
