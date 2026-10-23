#include "statistics.h"

#include <math.h>
#include <stdlib.h>

/**
 * @brief Check that a smoothing kernel has odd length and normalized weights.
 * @param window Array of finite, nonnegative weights.
 * @param len Pointer to a positive, odd number of weights.
 * @return 1 if the weight sum differs from one by less than 1e-6; otherwise 0.
 */
int statistics_validate_window(const float *window, const int *len) {
    if (!window || !len || *len <= 0 || *len % 2 == 0) return 0;
    double sum = 0;
    for (int i = 0; i < *len; i++) {
        if (!isfinite(window[i]) || window[i] < 0) return 0;
        sum += window[i];
    }
    return fabs(sum - 1.0) < 1e-6;
}

/**
 * @brief Smooth heart rate using a centered kernel and original sample readings.
 * @param[in,out] head Non-NULL sentinel of the sample list.
 * @param window Normalized kernel accepted by statistics_validate_window().
 * @param len Pointer to the kernel length.
 * @return 0 on success, including lists shorter than the kernel; -1 for invalid
 * arguments, an invalid kernel, or allocation failure.
 * @note Edge samples without a complete window remain unchanged.
 */
int statistics_smooth_heart_rate(Sample *head, const float *window, const int *len) {
    if (!head || !statistics_validate_window(window, len)) return -1;
    size_t count = 0;
    for (Sample *node = head->next; node; node = node->next) count++;
    if (count < (size_t)*len) return 0;
    float *original = malloc(count * sizeof(*original));
    if (!original) return -1;
    size_t i = 0;
    for (Sample *node = head->next; node; node = node->next) original[i++] = node->hr;
    i = 0;
    size_t half = *len / 2;
    for (Sample *node = head->next; node; node = node->next, i++) {
        if (i < half || i + half >= count) continue;
        double sum = 0;
        for (int j = 0; j < *len; j++) sum += original[i - half + j] * window[j];
        node->hr = sum;
    }
    free(original);
    return 0;
}

/**
 * @brief Average pace for samples within an inclusive heart-rate tolerance.
 * @param head Non-NULL sentinel of the sample list.
 * @param target Non-NULL target heart rate in bpm.
 * @param tolerance Allowed absolute heart-rate difference in bpm; must be nonnegative.
 * @param[out] result Non-NULL destination for mean pace in min/km, or 0 when no samples match.
 * @return Number of matching samples; each sample receives equal weight.
 */
int statistics_average_pace_at_heart_rate(const Sample *head, const int *target, int tolerance, float *result) {
    int count = 0;
    double sum = 0;
    *result = 0;
    for (const Sample *node = head->next; node; node = node->next) {
        if (fabsf(node->hr - *target) <= tolerance) { sum += node->pace_min; count++; }
    }
    if (count) *result = sum / count;
    return count;
}

/**
 * @brief Average heart rate for samples within an inclusive relative pace tolerance.
 * @param head Non-NULL sentinel of the sample list.
 * @param target Non-NULL target pace.
 * @param tolerance Nonnegative fractional tolerance; 0.01 means one percent.
 * @param[out] result Non-NULL destination for mean heart rate in bpm, or 0 when no samples match.
 * @return Number of matching samples; each sample receives equal weight.
 */
int statistics_average_heart_rate_at_pace(const Sample *head, const Pace *target, float tolerance, float *result) {
    int count = 0;
    double sum = 0;
    float center = pace_to_minutes(target);
    *result = 0;
    for (const Sample *node = head->next; node; node = node->next) {
        if (fabsf(node->pace_min - center) <= center * tolerance) { sum += node->hr; count++; }
    }
    if (count) *result = sum / count;
    return count;
}
