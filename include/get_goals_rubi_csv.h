#ifndef GET_GOALS_RUBI_CSV_H_INCLUDED
#define GET_GOALS_RUBI_CSV_H_INCLUDED

#include <stdio.h>
#include <stdlib.h> /* atoi */
#include <string.h> /* memcpy */
#include <errno.h>
#include <assert.h>
#include <chs_debug.h>
#include <math.h>

typedef struct {
	int min, sec, dec;
} Pace;

typedef struct a_sample{
    Pace pace;
    float distance;
    float time;
    float hr;
    float pace_min;
    float speed;
    struct a_sample * next;
    struct a_sample * prev;
} Sample;

int validate_int_HR(int );
int validate_str_HR(char * );
int validate_str_time(const char * , Pace *);
char * pace_to_str( Pace * );
int get_field_number(const char * , const char * , int );
void float_to_Pace(Pace * , float );
float pace_to_float(const Pace * );
/* Initialize head to {0}; it is a sentinel owned by the caller.
 * freeSamples releases its data nodes on completion. */
int loadValues(int *count , Sample * head, char * bytes,  const size_t * len);
void freeSamples(Sample *head);
int lowPassHR (Sample * head, const float * window ,const int * len);
int validateWindow(const float * window ,const int * len);
int average_pace_at_HR(Sample * head, const int * target_hr, int,  float * f_pace);
int average_hr_at_Pace(Sample * head, const Pace * target_pace, float, float * hr_at_pace);

#endif