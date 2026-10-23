
#include <chs_debug.h>

int verbose_flag = 0;

 void verbose(const char * format, ...) {
    if (verbose_flag == 1) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}
