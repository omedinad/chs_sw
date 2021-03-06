#include <stdio.h>
#include <stdlib.h> /* atoi */
#include <string.h> /* memcpy */
#include <errno.h>

#define MAX_HR 190
#define MIN_HR 45
#define HR_TOLERANCE 3 // Bpm
#define PACE_TOLERANCE 10 // Seconds
#define HDR_HR "heartrate"
#define HDR_DIST "dist"
#define HDR_TIME "time"
#define CHECK_FOR_PACE_OUTLIERS 1

typedef struct {
	int min, sec, dec;
} Pace;

int validate_int_HR(int hr){
	return ((hr < MAX_HR)&&(hr > MIN_HR)?hr:0);
}

int validate_str_HR(char * strhr){
	int hr = 0;
	if (strhr != NULL){
		hr = atoi(strhr);
		return validate_int_HR(hr);
	}else{
		return hr;
	}
}

int validate_str_time(const char * tstring, Pace *p){
	int min;
	int sec;
	int dec;
	
	sscanf(tstring, "%d:%d.%d", &min, &sec, &dec );
	
	if(min > 0 && min < 60) p->min = min;
	else return -1;
	if (sec > 0 && sec < 60) p->sec = sec;
	else return -1;
	if(dec >= 0) p->dec = dec;
	else return -1;
    
	return 1;
}

char * pace_to_str( Pace * pace){
	char * result;
	result = (char *)malloc(sizeof(char)*10);
	snprintf(result, 10, "%d:%02d.%d", pace->min, pace->sec, pace->dec);
	return result;
}

int get_field_number(const char * field_name, const char * buffer, int len){
    int llen = -1;
    char * line;
    char * token;
    for (llen = 0; llen < len; llen++)
        if (buffer[llen] == '\n') break;
    
    
    line = (char *)malloc(llen);
    memcpy(line, buffer, llen);
    line[llen]='\0';
    
    for(token = strtok( line , "," ), llen=0; token; token = strtok( NULL , "," ), llen++){
        if(strcmp(token, field_name) == 0){
            return llen;
        }
    }
    
    //printf("%s (line lenght %d)\n", line, llen);
    
    return -1;
}

void float_to_Pace(Pace * pace, float fpace){
    pace->min = (int) fpace;
    pace->sec = (int)((fpace - (float)pace->min) * (float)60);
}

float pace_to_float(const Pace * pace){
    return (float)pace->min + ((float)pace->sec/60);
}

int computeValues(float * hratp, Pace * pathr, char * bytes, const size_t * len, const int * target_hr, const Pace * target_pace ){
	
    int hratp_counter = 0;
    float hratp_sum = 0;
    int pathr_counter = 0;
    float pathr_sum = 0;
    float target_pace_dec;
    
    int hr_f_nr = 0;
    int hr_count = 0;
    int hr = 0;
    long hr_sum = 0;
    
    int dist_f_nr;
    float dist[2];
    float dx_dist;
    int temp_dist;
    
    int time_f_nr;
    int time;
    
    float speed;
    float dx_pace;
    
    char * line_t;
    char * field_t, * field_tr;
    int line_nr;
    int field_nr;
    
    
    
    hr_f_nr = get_field_number(HDR_HR, bytes, *len);
    // fprintf(stderr, "HR Column nr: %d\n", hr_f_nr);
    
    dist_f_nr = get_field_number(HDR_DIST, bytes, *len);
    // fprintf(stderr, "Distance Column nr: %d\n", dist_f_nr);
    
    time_f_nr = get_field_number(HDR_TIME, bytes, *len);
    // fprintf(stderr, "Time Column nr: %d\n", time_f_nr);
    
    if(hr_f_nr >= 0 && dist_f_nr >= 0 && time_f_nr >= 0 ){
        errno = 0;
    }else{
        errno = EINVAL;
        return -1;
    }
    
    // fprintf(stderr, "Remember to implement decims of sec.\n");
    target_pace_dec = pace_to_float(target_pace);
    
    
    line_t = strtok( bytes , "\n" );
    for(line_t = strtok( NULL , "\n" ), line_nr = 1; line_t ; line_t = strtok( NULL , "\n" ), line_nr++){
        // fprintf(stderr, "%d)\t%s\n", line_nr, line_t);
        for(field_t = strtok_r( line_t , ",", &field_tr ), field_nr = 0; field_t; field_t = strtok_r( NULL , ",", &field_tr ), field_nr++){
            if(field_nr == hr_f_nr){
                hr = validate_str_HR(field_t);
            }
            
            if(field_nr == dist_f_nr){
                temp_dist = dist[0];
                dist[0] = atof(field_t);
                dist[1] = temp_dist;
            }
            
            if(field_nr == time_f_nr){
                time = atoi(field_t);
            }
            
        }
        
        /* General Statistics */
        
        dx_dist=dist[0]-dist[1];
        speed=((dx_dist/time)*3600)/1000; // m/h
        dx_pace=60/speed;
        
        // printf("%d\t%.2f\t%d\t%.2f Km/h\t%.2f m/Km\n", hr, dx_dist, time, speed , dx_pace);
        
        /* Collect PACE at desired HR */
        if(hr < (*target_hr+HR_TOLERANCE) && hr > (*target_hr-HR_TOLERANCE)){
            if (CHECK_FOR_PACE_OUTLIERS){
                if(pathr_counter > 20){
                    if (
                        (dx_pace < ((float)1.1 * (pathr_sum/(float)pathr_counter))) &&
                        (dx_pace > ((float)0.9 * (pathr_sum/(float)pathr_counter)))
                        )  {
                        /* printf("+%.2f < %.2f < %.2f \n", ((float)0.99 * (pathr_sum/(float)pathr_counter)),
                         dx_pace ,
                         ((float)1.001 * (pathr_sum/(float)pathr_counter)));
                         */
                        pathr_sum += dx_pace;
                        pathr_counter++;
                    }else{
                        // printf("- %.2f -\n", dx_pace);
                    }
                }else{
                    // printf("#");
                    pathr_sum += dx_pace;
                    pathr_counter++;
                }
                
            }else{
                printf(".");
                pathr_sum += dx_pace;
                pathr_counter++;
            }
            // printf("%d\t%.2f\t%d\t%.2f Km/h\t%.2f m/Km\n", hr, dx_dist, time, speed , dx_pace);
        }
        
        /* Collect HR at desired PACE */
        if(dx_pace > (0.99 * target_pace_dec) &&
           dx_pace < (1.01 * target_pace_dec)){
            
            // printf("%d sum %ld bpm %.2f < %.2f < %.2f\n", hr, hr_sum, (0.99 * target_pace_dec) , dx_pace, (1.11 * target_pace_dec));
            hr_sum += hr;
            hr_count++;
            
        }
        
        
    }
    
    // printf("Pace stats avg pace: %.2f\n", pathr_sum/pathr_counter);
    float_to_Pace(pathr, pathr_sum/pathr_counter);
    *hratp = (float)hr_sum/(float)hr_count;
    
    
    
	return(1);
}

int main(int argc, char **argv){
	Pace pace;
    int target_hr;
	FILE *fd;
	char *file_bytes;
	size_t file_size = 0;
	float hr_at_pace = 0;
	Pace pace_at_hr = {0,0,0};
    Pace total_pace_at_hr = {0,0,0};
    int nr_of_files = 0;
    float sum_of_paces = 0;
    float sum_of_hrs = 0;
    
	if (argc < 4) {
		fprintf(stderr, "Parameters missing <HR> <PACE(mm:ss.ds)> <file>\n");
		return(-1);
	}
    
    target_hr=validate_str_HR(argv[1]);
    fprintf(stderr, "Target HR: %d\n", target_hr);
    fprintf(stderr, "Target Pace(%s): %s\n", (validate_str_time(argv[2], &pace)>0?"OK":"NOK"), pace_to_str(&pace));
	
    fprintf(stdout, "HR at\tPace at\tFile\n");
    fprintf(stdout, "%s\t%d\tName\n", pace_to_str(&pace), target_hr);
    fprintf(stdout, "-----------------------\n");
    for (int f_index = 3; f_index < argc; f_index++, nr_of_files++){
        fd = fopen(argv[f_index], "r");
        
        if (fd == NULL){
            fprintf(stderr, "Cannot open file: %s (errno=%i)\n", argv[f_index], errno);
            switch(errno){
                case ENOENT:
                    fprintf(stderr, "File not found\n");
                    break;
            }
            return (-1);
        }else{
            fseek(fd, 0, SEEK_END);
            file_size = ftell(fd);
            fseek(fd, 0, SEEK_SET);
            
            if(file_size > 0)
                file_bytes = (char *)malloc(sizeof(char)*file_size);
            else{
                fprintf(stderr, "File %s has 0 bytes\n", argv[f_index]);
            }
        }
        
        if (file_bytes != NULL){
            file_size = fread(file_bytes, sizeof(char), file_size, fd );
            if (fclose(fd) != 0){
                fprintf(stderr, "Error closing file (errno = %d)\n", errno);
                nr_of_files--; // If we can't open a file, we skip it!
            }
        }
       
        if(computeValues(&hr_at_pace, &pace_at_hr, file_bytes, &file_size, &target_hr, &pace) > 0){
            fprintf(stdout, "%.1f\t%s\t%s(%dK)\n",  hr_at_pace,pace_to_str(&pace_at_hr), argv[f_index], (int)file_size/1024);
            sum_of_hrs += hr_at_pace;
            sum_of_paces += pace_to_float(&pace_at_hr);
        }else{
            fprintf(stderr, "Error processing file %s (errno = %d)\n", argv[f_index], errno);
            if (errno == EINVAL){
                fprintf(stderr, "\tIncorrect or incompatible first line. No usable fields found.\n");
            }
            nr_of_files--; // If we can't open a file, we skip it!
        }
        
        free(file_bytes);
    }
    
    
    float_to_Pace(&total_pace_at_hr, sum_of_paces/(float)nr_of_files);
    
    fprintf(stdout, "-----------------------\n");
    fprintf(stdout,
                    "%.1f\t%s\t%d files\n",
                     (sum_of_hrs/(float)nr_of_files),pace_to_str(&total_pace_at_hr), nr_of_files);
    
}
