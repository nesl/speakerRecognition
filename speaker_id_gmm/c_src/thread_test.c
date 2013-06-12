/*
* Chenguang Shen and Kasturi Raghavan
* Networked and Embedded Systems Lab
* UCLA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <hexagon_standalone.h>
#include "hexagon_sim_timer.h"

#define COMPUTE_THREADS 1
// might be the minimum stack size for each thread
#define STACK_SIZE 3192

#define FORMAT "%lf\n"
#define PREC "%.3f\n"
#define NR_SPEAKER 5
#define SAMPLE_SIZE 167
#define SCALE 1000.0

typedef double real_T;
typedef double Word16;

typedef struct by_speaker {
    char name[10];
    Word16 m_m[108];
    Word16 m_v[108];
    Word16 m_w[9];
} by_speaker;

static const int num_mixture = 9;
static const int num_speaker = NR_SPEAKER;
static by_speaker speaker_gmm[NR_SPEAKER];
static Word16 test_sample[SAMPLE_SIZE][12];
static char ground_truth[10];
static char filename[100];
static char str[5];
static real_T logprob[NR_SPEAKER];
static real_T min, max;

static void compute (int * speaker_id, int i);
static int Mx;
static char stack [COMPUTE_THREADS][STACK_SIZE] __attribute__ ((__aligned__(8))); ;
static int speaker_ids [5] = {0,1,2,3,4};


float gaussmixp(Word16 x[12], Word16 mu[108], Word16 sigma[108], Word16 w[9]) {
    int i = 0, j = 0;
    real_T score = 0;
    for (i = 0; i < 9; i++) {
        real_T y = 0;
        for (j = 0; j < 12; j++) {
            Word16 m = mu[j * 9 + i];
            real_T s = 1.0 / sigma[j * 9 + i];
            //printf("%d %d %lf %lf\n", j, i, m, s);
            y += (x[j] - m) * (x[j] - m) * s ;
        }
        score += w[i] * (1.0/pow(6.828 ,6)) * (1.0 / pow(1.0,0.5)) * exp(-0.5 * y);
    }
    return log(score);
}

Word16 round_real(real_T x) {
    if (x > max) {
        max = x;
    }
    if (x < min) {
        min = x;
    }
    return x;
    // real_T result = 0;
    // char buffer[128];
    // memset(buffer, 0, sizeof(buffer));
    // sprintf(buffer, PREC, x);
    // sscanf(buffer, FORMAT, &result);
    // return result;
}

int get_label(const char *name) {
    if (strcmp(name, "manae") == 0) {
        return 1;
    }
    if (strcmp(name, "manah") == 0) {
        return 2;
    }
    if (strcmp(name, "manaj") == 0) {
        return 3;
    }
    if (strcmp(name, "manal") == 0) {
        return 4;
    }
    if (strcmp(name, "manar") == 0) {
        return 5;
    }
    return -1;
}

void load_speaker_gmm () {
    printf("In load_speaker_gmm\n");
    memset(speaker_gmm, 0, sizeof(speaker_gmm));
    char buffer[100];
    memset(buffer, 0, sizeof(buffer));
    FILE *fin = fopen("by_speaker.txt", "r");
    fgets(buffer, sizeof(buffer), fin);

    int i = 0;
    for (i = 0; i < num_speaker; i++) {
        // get name
        memset(buffer, 0, sizeof(buffer));
        fgets(buffer, sizeof(buffer), fin);
        // printf("*%s*\n", buffer);
        // if (strcmp(buffer, "manae\n")) {
        //  printf("equals!");
        // }
        strncpy(speaker_gmm[i].name, buffer, strlen(buffer) - 1);
        //printf("%d: %s ", i, speaker_gmm[i].name);


        // get M
        memset(buffer, 0, sizeof(buffer));
        fgets(buffer, sizeof(buffer), fin);
        if (strncmp(buffer, "M\n", 2) == 0) {
            //printf("read M!\n");
            int j = 0;
            for (j = 0; j < 108; j++) {
                real_T temp;
                fscanf(fin, FORMAT, &temp);
                speaker_gmm[i].m_m[j] = round_real(temp);
            }
            // for (j = 0; j < 108; j++) {
            //  printf("%d: %lf\n", j, speaker_gmm[i].m_m[j]);
            // }
        }

        // get V
        memset(buffer, 0, sizeof(buffer));
        fgets(buffer, sizeof(buffer), fin);
        if (strncmp(buffer, "V\n", 2) == 0) {
            //printf("read V!\n");
            int j = 0;
            for (j = 0; j < 108; j++) {
                real_T temp;
                fscanf(fin, FORMAT, &temp);
                speaker_gmm[i].m_v[j] = round_real(temp);
            }
            // for (j = 0; j < 108; j++) {
            //  printf("%d: %lf\n", j, speaker_gmm[i].m_v[j]);
            // }
        }

        // get W
        memset(buffer, 0, sizeof(buffer));
        fgets(buffer, sizeof(buffer), fin);
        if (strncmp(buffer, "W\n", 2) == 0) {
            //printf("read W!\n");
            int j = 0;
            for (j = 0; j < 9; j++) {
                real_T temp;
                fscanf(fin, FORMAT, &temp);
                speaker_gmm[i].m_w[j] = round_real(temp);
            }
            // for (j = 0; j < 9; j++) {
            //  printf("%d: %lf\n", j, speaker_gmm[i].m_w[j]);
            // }
        }
    }
    printf("Finish reading gmms.\n");
    fclose(fin);
}

int load_test_sample (const char *file_name) {
    memset(test_sample, 0, sizeof(test_sample));
    printf("In load_test_sample\n");
    FILE *fin = fopen(file_name, "r");
    int num = -1;
    int n = -1;
    char buffer[100];
    memset(buffer, 0, sizeof(buffer));  
    fgets(buffer, sizeof(buffer), fin);

    fscanf(fin, "%d", &num);
    memset(ground_truth, 0, sizeof(ground_truth));
    fscanf(fin, "%s", ground_truth);
    printf("test data #%d: %s\n", num, ground_truth);
    fscanf(fin, "%d", &n);
    if (n != -1) {
        int i = 0, j = 0;
        for (i = 0; i < n; i++) {
            for (j = 0; j < 12; j++) {
                real_T temp;
                fscanf(fin, FORMAT, &temp);
                test_sample[i][j] = round_real(temp);
            }
        }
        fclose(fin);
        return n;
    }
    else {
        fclose(fin);
        return -1;
    }

}


int main (int argc, char **argv){
    load_speaker_gmm();
    int k = 1, i = 0, j = 0;
    int correct = 0;
    
    memset(logprob, 0, sizeof(logprob));
    memset(filename, 0, sizeof(filename));
    memset(str, 0, sizeof(str));

    sprintf(str, "%d", k);
    strcpy(filename, "../../test_data_5/sample_data");
    strcat(filename, str);
    strcat(filename, ".txt");
    printf("%d %s\n", k, filename);

    int size = load_test_sample(filename);
    printf("%d\n", size);

    hexagon_sim_init_timer();
    hexagon_sim_start_timer();


    // /* gmm prob for each frame per each speaker */
    // for (j = 0; j < num_speaker; j++) {

    // }
    j = 0;
    while (j < num_speaker) {
        for (i = 0; i < COMPUTE_THREADS && j < num_speaker; i++, j++) {
            thread_create ((void *) compute, &stack [i][STACK_SIZE], i + 1, speaker_ids + j);
            printf("create thread %d for j=%d\n", (i+1), j);
        }
        thread_join (((1 << COMPUTE_THREADS) - 1) << 1);
    }


    int max_prob = -1e6;
    int max_index = -1;
    for (j = 0; j < num_speaker; j++) {
        printf("result=");
        printf(FORMAT, logprob[j]);
        if (logprob[j] > max_prob) {
            max_prob = logprob[j];
            max_index = j;
        }
    }
    printf("speaker label = %d\n", (max_index + 1));
    printf("max=%f\nmin=%f\n", max, min);


    // Create threads to compute four speaker_ids of the fractal
    // each thread is allocated a stack base and parameter

    // j = 0;
    // while (j < 10)
    // {
    //     for (i = 0; i < COMPUTE_THREADS && j < 10; i++, j++) {
    //         thread_create ((void *) compute_fractal, &stack [i][STACK_SIZE], i + 1, speaker_ids + j);
    //         printf("create thread %d for j=%d\n", (i+1), j);
    //     }
    //     //thread_join (((1 << COMPUTE_THREADS) - 1) << 1);
    // }
    

    // printf("Threads done.\n");
    hexagon_sim_end_timer();
    hexagon_sim_show_timer(stdout);
    return (0);
}

static void compute (int * speaker_id, int i)
{
    while (1)
    {
        if (trylockMutex (&Mx))
        {
            printf("thread %d for: %d\n", thread_get_tnum(), *speaker_id);
            break;
        }

    }
    unlockMutex (&Mx);

    int j = *speaker_id;
    by_speaker gmm = speaker_gmm[j];
    int iii = 0;
    for (iii = 0; iii < 167; iii++) {
        real_T result = gaussmixp(test_sample[iii], gmm.m_m, gmm.m_v, gmm.m_w);
        logprob[j] += result;
    }

    lockMutex (&Mx);
    {
        printf("\nDone computing gmm prob for thread %d.\n", thread_get_tnum());
    }
    unlockMutex (&Mx);
}

// 1 thread, stack size=3192
// use double: 22574464 cycles, 94136B memory
// use float: 5326153 cycles, 81576B memory

// 2 threads, stack size=3192 
// use double: 14817336 cycles, 97536B memory
// use float: 3449244 cycles, 84800B memory