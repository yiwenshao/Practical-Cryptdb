#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "zmalloc.h"
#include "bio.h"
#include "adlist.h"

int(*userFunction)(unsigned long,void*) = NULL;

static pthread_t bio_threads[REDIS_BIO_NUM_OPS];
static pthread_mutex_t bio_mutex[REDIS_BIO_NUM_OPS];
static pthread_cond_t bio_condvar[REDIS_BIO_NUM_OPS];
//
static list *bio_jobs[REDIS_BIO_NUM_OPS];

static unsigned long long bio_pending[REDIS_BIO_NUM_OPS];

/*function for the working thread*/
void *
bioProcessBackgroundJobs(void *arg);

#define REDIS_THREAD_STACK_SIZE (1024*1024*4)

void bioInit(void) {
    pthread_attr_t attr;
    pthread_t thread;
    size_t stacksize;
    unsigned int j;
    /* Initialization of state vars and objects 
     *
     */
    for (j = 0u; j < REDIS_BIO_NUM_OPS; j++) {
        pthread_mutex_init(&bio_mutex[j],NULL);
        pthread_cond_init(&bio_condvar[j],NULL);
        bio_jobs[j] = listCreate();
        bio_pending[j] = 0;
    }
    /* Set the stack size as by default it may be small in some system 
     *
     */
    pthread_attr_init(&attr);
    pthread_attr_getstacksize(&attr,&stacksize);
    if (!stacksize) stacksize = 1; /* The world is full of Solaris Fixes */
    while (stacksize < REDIS_THREAD_STACK_SIZE) stacksize *= 2;
    pthread_attr_setstacksize(&attr, stacksize);

    /* Ready to spawn our threads. We use the single argument the thread
     * function accepts in order to pass the job ID the thread is
     * responsible of. 
     */
    for (j = 0u; j < REDIS_BIO_NUM_OPS; j++) {
        void *arg = (void*)(unsigned long) j;
        if (pthread_create(&thread,&attr,bioProcessBackgroundJobs,arg) != 0) {
            printf("Fatal: Can't initialize Background Jobs.\n");
            exit(1);
        }
        bio_threads[j] = thread;
    }
}

void bioCreateBackgroundJob(int type,const void *arg1, void *arg2, int stop) {
    struct bio_job *job = (struct bio_job*)zmalloc(sizeof(*job));
    job->time = time(NULL);
    job->arg1 = const_cast<void*>(arg1);
    job->arg2 = arg2;
    job->stop = stop;
    pthread_mutex_lock(&bio_mutex[type]);
    listAddNodeTail(bio_jobs[type],job);
    bio_pending[type]++;
    pthread_cond_signal(&bio_condvar[type]);
    pthread_mutex_unlock(&bio_mutex[type]);
}

//each thread is responsible for the list indexed by type
void *bioProcessBackgroundJobs(void *arg) {
    struct bio_job *job;
    unsigned long type = (unsigned long) arg;
    sigset_t sigset;
    /* Make the thread killable at any time, so that bioKillThreads()
     * can work reliably. */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    pthread_mutex_lock(&bio_mutex[type]);
    /* Block SIGALRM so we are sure that only the main thread will
     * receive the watchdog signal. */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL))
            printf("Warning: can't mask SIGALRM in bio.c thread:\n");
    while(1) {
        listNode *ln;
        /* The loop always starts with the lock hold. */
        if (listLength(bio_jobs[type]) == 0) {
            pthread_cond_wait(&bio_condvar[type],&bio_mutex[type]);
            continue;
        }

        /* Pop the job from the queue. 
         *
         * 取出（但不删除）队列中的首个任务
         */
        ln = listFirst(bio_jobs[type]);
        job = (struct bio_job *)ln->value;

        /* It is now possible to unlock the background system as we know have
         * a stand alone job structure to process.*/
        pthread_mutex_unlock(&bio_mutex[type]);

        int stop = userFunction(type,(void*)job);
        zfree(job);
        if(stop == 1){
            break;
        }else{
        }
        /* Lock again before reiterating the loop, if there are no longer
         * jobs to process we'll block again in pthread_cond_wait(). */
        pthread_mutex_lock(&bio_mutex[type]);
        // 将执行完成的任务从队列中删除，并减少任务计数器
        listDelNode(bio_jobs[type],ln);
        bio_pending[type]--;
    }
    return NULL;
}

unsigned long long bioPendingJobsOfType(int type) {
    unsigned long long val;
    pthread_mutex_lock(&bio_mutex[type]);
    val = bio_pending[type];
    pthread_mutex_unlock(&bio_mutex[type]);
    return val;
}

void bioKillThreads(void) {
    unsigned int err, j;
    for (j = 0; j < REDIS_BIO_NUM_OPS; j++) {
        if ((err = pthread_join(bio_threads[j],NULL)) != 0) {
//            printf("Bio thread for job type can be joined\n");
        } else {
//            printf("Bio thread for job type terminated\n");
        }
    }
}
