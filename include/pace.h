#ifndef CHS_PACE_H
#define CHS_PACE_H

typedef struct {
    int min, sec, dec;
} Pace;

/**
 * @brief Parse a positive pace with fractional seconds.
 * @param text Pace in mm:ss.d or mm:ss.dd format; minutes and seconds must be below 60.
 * @param[out] pace Parsed pace; dec stores hundredths of a second. Unchanged on failure.
 * @return 1 on success, or -1 for invalid input.
 */
int pace_parse(const char *text, Pace *pace);

/**
 * @brief Allocate a pace string formatted as minutes:seconds.hundredths.
 * @param pace Non-NULL pace to format.
 * @return Newly allocated string, or NULL if allocation fails.
 * @note The caller must free the returned string.
 */
char *pace_format(const Pace *pace);

/**
 * @brief Convert decimal minutes to a pace rounded to hundredths of a second.
 * @param[out] pace Non-NULL destination, including carry into seconds and minutes.
 * @param minutes Pace in minutes per kilometer.
 * @note Nonfinite, nonpositive, or out-of-range input produces a zero pace.
 */
void pace_from_minutes(Pace *pace, float minutes);

/**
 * @brief Convert a structured pace to decimal minutes per kilometer.
 * @param pace Non-NULL pace with dec expressed in hundredths of a second.
 * @return Pace in minutes per kilometer.
 */
float pace_to_minutes(const Pace *pace);

#endif
