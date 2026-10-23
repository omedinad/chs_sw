/* Analyze interval-based running CSV exports. */
#include <getopt.h>
#include <get_goals_rubi_csv.h>
#include <limits.h>

static void usage(const char *name) {
    fprintf(stderr, "Usage: %s -h HR -p mm:ss.dd [-o output] [--verbose] file.csv ...\n", name);
}

static int load_file(const char *name, char **bytes, size_t *length) {
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

static void print_pace(FILE *out, float minutes) {
    Pace pace;
    float_to_Pace(&pace, minutes);
    fprintf(out, "%d:%02d.%02d", pace.min, pace.sec, pace.dec);
}

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
            if (!(target_hr = validate_str_HR(optarg))) { fprintf(stderr, "Invalid heart rate: %s\n", optarg); return 1; }
            break;
        case 'p':
            if (validate_str_time(optarg, &pace) < 0) { fprintf(stderr, "Invalid pace: %s\n", optarg); return 1; }
            have_pace = 1;
            break;
        case 'o': output = optarg; break;
        case 'u': usage(argv[0]); return 0;
        default: usage(argv[0]); return 1;
        }
    }
    if (!target_hr || !have_pace || optind == argc) { usage(argv[0]); return 1; }
    /* Read all inputs before opening output, so -o may safely name an input. */
    FILE *out = tmpfile();
    if (!out) { perror("temporary report"); return 1; }
    fprintf(out, "HR at\t\tPace at\t\tFile\n");
    print_pace(out, pace_to_float(&pace));
    fprintf(out, "\t\t%d\t\tName\n----------------------------------------------\n", target_hr);
    float window[] = {0.010333864f, 0.207561207f, 0.564209858f, 0.207561207f, 0.010333864f};
    int window_len = 5, files = 0, hr_files = 0, pace_files = 0, failed = 0;
    double hr_sum = 0, pace_sum = 0;
    for (; optind < argc; optind++) {
        char *bytes = NULL;
        size_t length = 0;
        Sample head = {0};
        int records = 0;
        if (load_file(argv[optind], &bytes, &length) ||
            loadValues(&records, &head, bytes, &length) ||
            lowPassHR(&head, window, &window_len)) {
            fprintf(stderr, "Cannot process %s: %s\n", argv[optind], strerror(errno));
            failed = 1;
            free(bytes);
            freeSamples(&head);
            continue;
        }
        float avg_pace, avg_hr;
        int p_count = average_pace_at_HR(&head, &target_hr, 1, &avg_pace);
        int h_count = average_hr_at_Pace(&head, &pace, 0.01f, &avg_hr);
        if (h_count) { fprintf(out, "%.1f(%d)", avg_hr, h_count); hr_sum += avg_hr; hr_files++; }
        else fprintf(out, "N/A(0)");
        fprintf(out, "\t");
        if (p_count) { print_pace(out, avg_pace); pace_sum += avg_pace; pace_files++; }
        else fprintf(out, "N/A");
        fprintf(out, "(%d)\t%s(%zuK, %d Records)\n", p_count, argv[optind], length / 1024, records);
        files++;
        verbose("Processed %s: %d usable records\n", argv[optind], records);
        free(bytes);
        freeSamples(&head);
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
