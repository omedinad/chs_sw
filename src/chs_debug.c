
#include <chs_debug.h>

int verbose_flag = 0;

/**
 * @brief Write a formatted diagnostic to standard error when verbosity is enabled.
 * @param format printf-compatible format string.
 * @param ... Arguments required by format.
 * @note Produces output only when verbose_flag equals 1.
 */
void verbose(const char * format, ...) {
    if (verbose_flag == 1) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}
