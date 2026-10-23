#include "analysis.h"
#include "chs_debug.h"
#include "cli.h"
#include "report.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Coordinate argument parsing, file analysis, and buffered report output.
 * @param argc Number of command-line arguments.
 * @param argv Targets, output options, and CSV input paths.
 * @return 0 for successful processing or help; 1 for argument, input, or output failures.
 * @note Failed inputs are skipped so subsequent files can still be processed.
 */
int main(int argc, char **argv) {
    CliOptions options;
    CliParseResult parsed = cli_parse_options(argc, argv, &options);
    if (parsed == CLI_HELP) return 0;
    if (parsed == CLI_ERROR) return 1;

    FILE *report = tmpfile();
    if (!report) { perror("temporary report"); return 1; }
    ReportSummary summary = {0};
    int failed = 0;
    report_write_header(report, &options);

    for (int i = options.first_input_index; i < argc; i++) {
        WorkoutStatistics stats;
        if (analyze_file(argv[i], &options, &stats) != 0) {
            fprintf(stderr, "Cannot process %s: %s\n", argv[i], strerror(errno));
            failed = 1;
            continue;
        }
        report_write_workout(report, argv[i], &stats);
        report_summary_add(&summary, &stats);
        verbose("Processed %s: %d usable records\n", argv[i], stats.record_count);
    }

    report_write_summary(report, &summary);
    if (report_publish(report, options.output_path) != 0) failed = 1;
    if (fclose(report)) failed = 1;
    return failed || summary.processed_files == 0;
}
