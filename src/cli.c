#include "cli.h"
#include "chs_debug.h"
#include "heart_rate.h"

#include <getopt.h>
#include <stdio.h>

/**
 * @brief Write the command-line usage synopsis to standard error.
 * @param name Executable name to include in the synopsis.
 */
static void print_usage(const char *name) {
    fprintf(stderr, "Usage: %s -h HR -p mm:ss.dd [-o output] [--verbose] file.csv ...\n", name);
}

/** @copydoc cli_parse_options */
CliParseResult cli_parse_options(int argc, char **argv, CliOptions *result) {
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
            if (!(target_hr = heart_rate_parse(optarg))) { fprintf(stderr, "Invalid heart rate: %s\n", optarg); return CLI_ERROR; }
            break;
        case 'p':
            if (pace_parse(optarg, &pace) < 0) { fprintf(stderr, "Invalid pace: %s\n", optarg); return CLI_ERROR; }
            have_pace = 1;
            break;
        case 'o': output = optarg; break;
        case 'u': print_usage(argv[0]); return CLI_HELP;
        default: print_usage(argv[0]); return CLI_ERROR;
        }
    }
    if (!target_hr || !have_pace || optind == argc) { print_usage(argv[0]); return CLI_ERROR; }
    *result = (CliOptions){target_hr, pace, output, optind};
    return CLI_OK;
}
