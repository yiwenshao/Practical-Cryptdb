#pragma once
#include <time.h>
void bioInit(void);
void bioCreateBackgroundJob(int type,const void *arg1, void *arg2, int stop);
unsigned long long bioPendingJobsOfType(int type);
void bioWaitPendingJobsLE(int type, unsigned long long num);
time_t bioOlderJobOfType(int type);
void bioKillThreads(void);
struct bio_job {
    time_t time;
    void *arg1, *arg2;
    int stop;
};
extern 
int(*userFunction)(unsigned long,void*);

const unsigned int REDIS_BIO_NUM_OPS = 10;

