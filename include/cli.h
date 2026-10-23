#ifndef CHS_CLI_H
#define CHS_CLI_H

#include "pace.h"

/** @brief Validated targets and borrowed command-line output/input locations. */
typedef struct {
    int target_heart_rate;
    Pace target_pace;
    const char *output_path;
    int first_input_index;
} CliOptions;

typedef enum { CLI_OK, CLI_HELP, CLI_ERROR } CliParseResult;

/**
 * @brief Parse command-line options and print help or argument errors to stderr.
 * @param argc Argument count.
 * @param[in,out] argv Argument vector; getopt may reorder entries.
 * @param[out] result Parsed options on CLI_OK; output_path borrows argv storage.
 * @return CLI_OK, CLI_HELP, or CLI_ERROR. Call once at process startup.
 */
CliParseResult cli_parse_options(int argc, char **argv, CliOptions *result);

#endif
