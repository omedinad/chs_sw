#include <get_goals_rubi_csv.h>
#include <ctype.h>
#include <limits.h>

int validate_int_HR(int hr) {
    return hr > 45 && hr < 190 ? hr : 0;
}

int validate_str_HR(char *text) {
    char *end;
    if (!text || !*text) return 0;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || *end || value < INT_MIN || value > INT_MAX) return 0;
    return validate_int_HR((int)value);
}

/* Pace.dec is hundredths of a second. Accept one or two decimal digits. */
int validate_str_time(const char *text, Pace *pace) {
    if (!text || !pace || !isdigit((unsigned char)*text)) return -1;
    char *end;
    long minutes = strtol(text, &end, 10);
    if (minutes < 0 || minutes >= 60 || *end++ != ':') return -1;
    if (!isdigit((unsigned char)*end)) return -1;
    char *seconds_start = end;
    long seconds = strtol(seconds_start, &end, 10);
    if (seconds < 0 || seconds >= 60 || *end++ != '.') return -1;
    int decimals = 0, digits = 0;
    while (isdigit((unsigned char)*end) && digits < 2) {
        decimals = decimals * 10 + (*end++ - '0');
        digits++;
    }
    if (!digits || *end) return -1;
    if (digits == 1) decimals *= 10;
    if (minutes == 0 && seconds == 0 && decimals == 0) return -1;
    *pace = (Pace){(int)minutes, (int)seconds, decimals};
    return 1;
}

/* Caller owns the returned string. */
char *pace_to_str(Pace *pace) {
    char *result = malloc(48);
    if (result) snprintf(result, 48, "%d:%02d.%02d", pace->min, pace->sec, pace->dec);
    return result;
}

int get_field_number(const char *name, const char *bytes, int len) {
    int start = 0, column = 0;
    if (!name || !bytes || len <= 0) return -1;
    for (int i = 0; i <= len; i++) {
        if (i == len || bytes[i] == ',' || bytes[i] == '\n' || bytes[i] == '\r') {
            if ((size_t)(i - start) == strlen(name) && !memcmp(bytes + start, name, i - start))
                return column;
            if (i == len || bytes[i] != ',') break;
            column++;
            start = i + 1;
        }
    }
    return -1;
}

void float_to_Pace(Pace *pace, float minutes) {
    *pace = (Pace){0, 0, 0};
    if (!isfinite(minutes) || minutes <= 0 || minutes > INT_MAX / 6000.0) return;
    long long hundredths = llround((double)minutes * 6000);
    pace->min = hundredths / 6000;
    pace->sec = hundredths / 100 % 60;
    pace->dec = hundredths % 100;
}

float pace_to_float(const Pace *pace) {
    return pace->min + pace->sec / 60.0f + pace->dec / 6000.0f;
}

void freeSamples(Sample *head) {
    if (!head) return;
    Sample *node = head->next;
    while (node) {
        Sample *next = node->next;
        free(node);
        node = next;
    }
    *head = (Sample){0};
}

static int number(const char *text, float *value) {
    char *end;
    if (!text || !*text) return -1;
    errno = 0;
    *value = strtof(text, &end);
    while (isspace((unsigned char)*end)) end++;
    return errno || *end || !isfinite(*value) ? -1 : 0;
}

/* head is an initialized sentinel, not a data record. time is interval seconds,
 * dist is cumulative meters. Empty CSV fields retain their column positions. */
int loadValues(int *count, Sample *head, char *bytes, const size_t *len) {
    if (!count || !head || !bytes || !len || *len > INT_MAX) {
        errno = EINVAL;
        return -1;
    }
    *count = 0;
    int hr_col = get_field_number("heartrate", bytes, (int)*len);
    int dist_col = get_field_number("dist", bytes, (int)*len);
    int time_col = get_field_number("time", bytes, (int)*len);
    if (hr_col < 0 || dist_col < 0 || time_col < 0) goto invalid;
    Sample *tail = head;
    float previous_distance = 0;
    char *save, *line = strtok_r(bytes, "\n", &save);
    while (line && (line = strtok_r(NULL, "\n", &save))) {
        float hr = 0, distance = 0, seconds = 0;
        int found = 0, column = 0;
        char *field = line;
        do {
            char *next = strchr(field, ',');
            if (next) *next++ = '\0';
            if (column == hr_col) { if (number(field, &hr)) goto invalid; found++; }
            if (column == dist_col) { if (number(field, &distance)) goto invalid; found++; }
            if (column == time_col) { if (number(field, &seconds)) goto invalid; found++; }
            column++;
            field = next;
        } while (field);
        if (found != 3 || distance < previous_distance || seconds < 0) goto invalid;
        float delta = distance - previous_distance;
        previous_distance = distance;
        /* Stationary intervals and sensor dropouts do not describe running pace. */
        if (delta <= 0 || seconds <= 0 || hr <= 45 || hr >= 190) continue;
        Sample *node = calloc(1, sizeof(*node));
        if (!node) { freeSamples(head); return -1; }
        node->distance = delta;
        node->time = seconds;
        node->hr = hr;
        node->speed = delta / seconds * 3.6f;
        node->pace_min = seconds / delta * (1000.0f / 60.0f);
        if (!isfinite(node->speed) || !isfinite(node->pace_min) ||
            node->speed <= 0 || node->pace_min <= 0) {
            free(node);
            goto invalid;
        }
        node->prev = tail;
        tail->next = node;
        tail = node;
        (*count)++;
    }
    if (*count == 0) goto invalid;
    return 0;
invalid:
    freeSamples(head);
    *count = 0;
    errno = EINVAL;
    return -1;
}

int validateWindow(const float *window, const int *len) {
    if (!window || !len || *len <= 0 || *len % 2 == 0) return 0;
    double sum = 0;
    for (int i = 0; i < *len; i++) {
        if (!isfinite(window[i]) || window[i] < 0) return 0;
        sum += window[i];
    }
    return fabs(sum - 1.0) < 1e-6;
}

int lowPassHR(Sample *head, const float *window, const int *len) {
    if (!head || !validateWindow(window, len)) return -1;
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

int average_pace_at_HR(Sample *head, const int *target, int tolerance, float *result) {
    int count = 0;
    double sum = 0;
    *result = 0;
    for (Sample *node = head->next; node; node = node->next) {
        if (fabsf(node->hr - *target) <= tolerance) { sum += node->pace_min; count++; }
    }
    if (count) *result = sum / count;
    return count;
}

int average_hr_at_Pace(Sample *head, const Pace *target, float tolerance, float *result) {
    int count = 0;
    double sum = 0;
    float center = pace_to_float(target);
    *result = 0;
    for (Sample *node = head->next; node; node = node->next) {
        if (fabsf(node->pace_min - center) <= center * tolerance) { sum += node->hr; count++; }
    }
    if (count) *result = sum / count;
    return count;
}
