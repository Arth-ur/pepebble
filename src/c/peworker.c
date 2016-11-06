#include <pebble.h>
#include "peworker.h"


#define ACCEL_SAMPLING_FREQUENCY ACCEL_SAMPLING_100HZ
#define NBSAMPLE 16
#define MFFT 9
#define NFFT (1 << MFFT)

// Utility macro for absolute values
#define ABS(x) ((x) < 0 ? -(x) : (x))

static short samples[NFFT];
static short nsamples = 0;
static uint32_t total_steps = 0;
static uint32_t key_total_steps = 10000;

// Handler for the pedometer event
static void (*pe_handler)(int) = NULL;

/** A structure to hold a cluster with a centroid and a specific count **/
typedef struct {
    int count;
    int centroid;
}C_cluster;

/** Compare to cluster based on their centroid value **/
int comparC_cluster(const void* a, const void* b){
    int c = 0;
    if ( ((C_cluster*)a)->centroid <  ((C_cluster*)b)->centroid ) c = -1;
    if ( ((C_cluster*)a)->centroid == ((C_cluster*)b)->centroid ) c = 0;
    if ( ((C_cluster*)a)->centroid >  ((C_cluster*)b)->centroid ) c = 1;
    return -c;
}

/**
     * Square root function
     * Source: https://forums.pebble.com/t/sqrt-function/3919/2
     */
static float my_sqrt(float num){
    // The maximum iteration count
    const int MAX = 40;
    float a, p, e = 0.001, b;
    int nb = 0;
    a = num;
    p = a * a;
    while( p - num >= e && nb++ < MAX ){
        b = ( a + ( num / a ) ) / 2;
        a = b;
        p = a * a;
    }
    return a;
}

/** Int comparaison */
int compar (const void* a, const void* b){
    if ( *(int16_t*)a <  *(int16_t*)b ) return -1;
    if ( *(int16_t*)a == *(int16_t*)b ) return 0;
    if ( *(int16_t*)a >  *(int16_t*)b ) return 1;
    return 0;
}

/** Find the maxima in the data array and store the in maxIndex. 
    Return the number of index found. 
**/
static int find_maxima(int size_max, int size_samples, short* maxIndex, short* data){
    int nbIndex = 0;
    // Mimimum magnitude
    const int ABSOLUTE_MAGNITUDE_THRESHOLD = 200;
    // Minimum distance between peaks (if 100Hz sampling -> 0.14s)
    const int CLEARANCE = 0.14 * ACCEL_SAMPLING_FREQUENCY;
    for(short i = 0; i < size_max; i++){
        for(short j = 0; j < size_samples; j++){
            if((data[j] > ABSOLUTE_MAGNITUDE_THRESHOLD) && ((maxIndex[i] == -1) || (samples[maxIndex[i]] < data[j]))){
                int8_t tooClose = 0;
                for(short k = 0; k < i; k++){
                    if((j > maxIndex[k] - 14 && j < maxIndex[k] + 14)){
                        tooClose = 1;
                    }
                }
                if(!tooClose){
                    maxIndex[i] = j;
                }
            }
        }
        if(maxIndex[i] > -1){
            nbIndex +=1;
        }
    }
    return nbIndex;
}

/** Handler for the accelerometer data **/
static void accel_data_handler(AccelData *data, uint32_t num_samples) {
    uint32_t max = 0, min = 0;
    uint32_t accel[NBSAMPLE];

    // Compute magnitude
    for(uint32_t i = 0; i < NBSAMPLE; i++){
        accel[i] = data[i].x * data[i].x + data[i].y * data[i].y + data[i].z * data[i].z;
        if(max == 0 || max < accel[i]){
            max = accel[i];
        }
        if(min == 0 || min > accel[i]){
            min = accel[i];
        }
    }
    memmove(samples, &(samples[NBSAMPLE]), sizeof(samples));

    // Add data
    for (uint32_t i = 0; i < NBSAMPLE; i++){
        samples[NFFT - (NBSAMPLE - i)] = accel[i] * 32768.0 * 2.0 / 4294967296.0 * 10;
    }

    nsamples += NBSAMPLE;

    // if enough samples have been collected
    if(nsamples >= NFFT){
        nsamples = 0;   
        int peakNSup = 0;
       
        // Find maximums
        #define MAX_COUNT 30
        short maxIndex [MAX_COUNT];
        memset(maxIndex, -1, sizeof(maxIndex));

        int nbIndex = find_maxima(MAX_COUNT, NFFT, maxIndex, samples);
        
        // clustering
        #define CENTROID_COUNT 4
        #define C_MAX_ITERATION_COUNT 10
        int centroids [CENTROID_COUNT];    // 4 clusters
        // initialize centroids:
        for (int i = 0; i < CENTROID_COUNT; i++){
            centroids[i] = samples[maxIndex[nbIndex / CENTROID_COUNT * i]];
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Init centroid {%d} to [%d] [%d] %d", i, nbIndex / CENTROID_COUNT * i, maxIndex[nbIndex / CENTROID_COUNT * i], centroids[i]);
        }

        // sort index
        qsort(maxIndex, nbIndex, sizeof(int16_t), compar);

        int c_iteration_count = 0;
        int c_changed = 0;
        int c_cluster [nbIndex];
        int c_sum [CENTROID_COUNT];
        int c_count[CENTROID_COUNT];
        C_cluster c_output[CENTROID_COUNT];
        do{
            // Compute distance to each centroid
            for(int j = 0; j < nbIndex; j++){
                int min_distance = 0, min_cluster = -1;
                for(int i = 0; i < CENTROID_COUNT; i++){
                    int distance =  ABS(centroids[i] - samples[maxIndex[j]]);
                    if(min_cluster < 0 || min_distance > distance){
                        min_distance = distance;
                        min_cluster = i;
                    }
                }
                c_cluster[j] = min_cluster;
            }

            // Compute centroids
            memset(c_sum, 0, sizeof(c_sum));
            memset(c_count, 0, sizeof(c_count));
            for(int i = 0; i < nbIndex; i++){
                int assigned_cluster = c_cluster[i];
                c_sum[assigned_cluster] += samples[maxIndex[i]];
                c_count[assigned_cluster] += 1;
            }
            c_changed = 0;
            for(int i = 0; i < CENTROID_COUNT; i++){
                int new_centroid = c_sum[i] / c_count[i];
                if(new_centroid != centroids[i]){
                    c_changed = 1;
                }
                centroids[i] = new_centroid;
                C_cluster my_cluster = {
                    .count = c_count[i],
                    .centroid = centroids[i]
                };
                c_output[i] = my_cluster;
            }

            // Safe-guard if stability could no be reached
            if(c_iteration_count++ > C_MAX_ITERATION_COUNT){
                APP_LOG(APP_LOG_LEVEL_DEBUG, "max iteration count C_MAX_ITERATION_COUNT reached");
                break;
            }
        }while(c_changed);
        
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Clustering: done in %d iterations", c_iteration_count);

        // Count assigned peak in the 3 highest clusters
        qsort(c_output, CENTROID_COUNT, sizeof(C_cluster), comparC_cluster);
        peakNSup = 0;
        for(int i = 0; i < CENTROID_COUNT; i++){
            if(c_output[i].centroid >= 250 && i < CENTROID_COUNT - 1)
                peakNSup += c_output[i].count;
        }

        total_steps += peakNSup;

        // Send the total_steps to the listener
        if(pe_handler != NULL){
            pe_handler(total_steps);
        }
    }
}

/** See peworker.h */
void pe_init() {
    memset(samples, 0,  sizeof(samples));

    // Read total steps from persistent memory
    if (persist_exists(key_total_steps)) {
        total_steps = persist_read_int(key_total_steps);
    } else {
        total_steps = 0;
        persist_write_int(key_total_steps, total_steps);
    }

    // Allow accelerometer event
    accel_data_service_subscribe(NBSAMPLE, accel_data_handler);

    //Define sampling rate
    accel_service_set_sampling_rate(ACCEL_SAMPLING_FREQUENCY);

}

/** See peworker.h */
void pe_deinit() {
    // Unsubscribe from the accelerometer events
    accel_data_service_unsubscribe();

    // Write total steps count to persistent memory
    persist_write_int(key_total_steps, total_steps);
}

/** See peworker.h */
void pe_set(int steps){
    total_steps = steps;
}

/** See peworker.h */
void pe_subscribe(void (*handler)(int)){
    pe_handler = handler;
}