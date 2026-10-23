#ifndef CHS_STATISTICS_H
#define CHS_STATISTICS_H

#include "samples.h"

/**
 * @brief Check that a smoothing kernel has odd length and normalized weights.
 * @param window Array of finite, nonnegative weights.
 * @param len Pointer to a positive, odd number of weights.
 * @return 1 if the weight sum differs from one by less than 1e-6; otherwise 0.
 */
int statistics_validate_window(const float *window, const int *len);

/**
 * @brief Smooth heart rate using a centered kernel and original sample readings.
 * @param[in,out] head Non-NULL sentinel of the sample list.
 * @param window Normalized kernel accepted by statistics_validate_window().
 * @param len Pointer to the kernel length.
 * @return 0 on success, including lists shorter than the kernel; -1 for invalid
 * arguments, an invalid kernel, or allocation failure.
 * @note Edge samples without a complete window remain unchanged.
 */
int statistics_smooth_heart_rate(Sample *head, const float *window, const int *len);

/**
 * @brief Average pace for samples within an inclusive heart-rate tolerance.
 * @param head Non-NULL sentinel of the sample list.
 * @param target Non-NULL target heart rate in bpm.
 * @param tolerance Allowed absolute heart-rate difference in bpm; must be nonnegative.
 * @param[out] result Non-NULL destination for mean pace in min/km, or 0 when no samples match.
 * @return Number of matching samples; each sample receives equal weight.
 */
int statistics_average_pace_at_heart_rate(const Sample *head, const int *target,
                                        int tolerance, float *result);

/**
 * @brief Average heart rate for samples within an inclusive relative pace tolerance.
 * @param head Non-NULL sentinel of the sample list.
 * @param target Non-NULL target pace.
 * @param tolerance Nonnegative fractional tolerance; 0.01 means one percent.
 * @param[out] result Non-NULL destination for mean heart rate in bpm, or 0 when no samples match.
 * @return Number of matching samples; each sample receives equal weight.
 */
int statistics_average_heart_rate_at_pace(const Sample *head, const Pace *target,
                                        float tolerance, float *result);

#endif
