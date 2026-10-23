#include "csv_reader.h"
#include "heart_rate.h"
/* Analyze interval-based running CSV exports. */
#include "pace.h"
#include "samples.h"
#include "statistics.h"
#include <chs_debug.h>

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Write the command-line usage synopsis to standard error.
 * @param name Executable name to include in the synopsis.
 */
static void print_usage(const char *name) {
    fprintf(stderr, "Usage: %s -h HR -p mm:ss.dd [-o output] [--verbose] file.csv ...\n", name);
}

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

/**
 * @brief Parse options, analyze workout files, and write per-file and summary statistics.
 * @param argc Number of command-line arguments.
 * @param argv Argument vector containing targets, optional output settings, and CSV paths.
 * @return 0 for successful processing or help; 1 for argument, input, or output failures.
 * @note Failed inputs are skipped so subsequent files can still be processed.
 */
int main(int argc, char **argv) {
    int target_hr = 0, have_pace = 0, option;
    Pace pace = {0};
    const char *output = NULL;
    static struct option options[] = {
        {"verbose", no_argument, &verbose_flag, 1},
        {"brief", no_argument, &verbose_flag, 0},
        {"heartrate", required_argument, NULL, 'h'},
        {"pace", required_argument, NULL, 'p'},
        {"out", required_argument, NULL, 'o'},
        {"help", no_argument, NULL, 'u'},
        {NULL, 0, NULL, 0}
    };
    while ((option = getopt_long(argc, argv, "h:p:o:", options, NULL)) != -1) {
        switch (option) {
        case 0: break;
        case 'h':
            if (!(target_hr = heart_rate_parse(optarg))) { fprintf(stderr, "Invalid heart rate: %s\n", optarg); return 1; }
            break;
        case 'p':
            if (pace_parse(optarg, &pace) < 0) { fprintf(stderr, "Invalid pace: %s\n", optarg); return 1; }
            have_pace = 1;
            break;
        case 'o': output = optarg; break;
        case 'u': print_usage(argv[0]); return 0;
        default: print_usage(argv[0]); return 1;
        }
    }
    if (!target_hr || !have_pace || optind == argc) { print_usage(argv[0]); return 1; }
    /* Read all inputs before opening output, so -o may safely name an input. */
    FILE *out = tmpfile();
    if (!out) { perror("temporary report"); return 1; }
    fprintf(out, "HR at\t\tPace at\t\tFile\n");
    print_pace(out, pace_to_minutes(&pace));
    fprintf(out, "\t\t%d\t\tName\n----------------------------------------------\n", target_hr);
    float window[] = {0.010333864f, 0.207561207f, 0.564209858f, 0.207561207f, 0.010333864f};
    int window_len = 5, files = 0, hr_files = 0, pace_files = 0, failed = 0;
    double hr_sum = 0, pace_sum = 0;
    for (; optind < argc; optind++) {
        char *bytes = NULL;
        size_t length = 0;
        Sample head = {0};
        int records = 0;
        if (read_input_file(argv[optind], &bytes, &length) ||
            csv_load_samples(&records, &head, bytes, &length) ||
            statistics_smooth_heart_rate(&head, window, &window_len)) {
            fprintf(stderr, "Cannot process %s: %s\n", argv[optind], strerror(errno));
            failed = 1;
            free(bytes);
            samples_free(&head);
            continue;
        }
        float avg_pace, avg_hr;
        int p_count = statistics_average_pace_at_heart_rate(&head, &target_hr, 1, &avg_pace);
        int h_count = statistics_average_heart_rate_at_pace(&head, &pace, 0.01f, &avg_hr);
        if (h_count) { fprintf(out, "%.1f(%d)", avg_hr, h_count); hr_sum += avg_hr; hr_files++; }
        else fprintf(out, "N/A(0)");
        fprintf(out, "\t");
        if (p_count) { print_pace(out, avg_pace); pace_sum += avg_pace; pace_files++; }
        else fprintf(out, "N/A");
        fprintf(out, "(%d)\t%s(%zuK, %d Records)\n", p_count, argv[optind], length / 1024, records);
        files++;
        verbose("Processed %s: %d usable records\n", argv[optind], records);
        free(bytes);
        samples_free(&head);
    }
    fprintf(out, "----------------------------------------------\n");
    if (hr_files) fprintf(out, "%.1f", hr_sum / hr_files); else fprintf(out, "N/A");
    fprintf(out, "\t");
    if (pace_files) print_pace(out, pace_sum / pace_files); else fprintf(out, "N/A");
    fprintf(out, "\t%d files\n", files);
    if (ferror(out) || fflush(out) || fseek(out, 0, SEEK_SET)) { fclose(out); return 1; }
    FILE *destination = output ? fopen(output, "w") : stdout;
    if (!destination) { perror(output); fclose(out); return 1; }
    int c;
    while ((c = fgetc(out)) != EOF) if (fputc(c, destination) == EOF) { failed = 1; break; }
    if (ferror(out)) failed = 1;
    fclose(out);
    if (fflush(destination)) failed = 1;
    if (output && fclose(destination)) failed = 1;
    return failed || !files ? 1 : 0;
}
