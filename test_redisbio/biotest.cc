#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <vector>
#include "redisbio/zmalloc.h"
#include "redisbio/adlist.h"
#include "redisbio/bio.h"

//return 0 to continue, return 1 to exit.
static
int
work(unsigned long id, void *input){
    struct bio_job *task = (struct bio_job *)input;
    if(task->stop == 1) return 1;
    char * query = (char*)(task->arg1);
    (void)query;
    return 0;
}

int
main(){
    userFunction = work;
    //create threads, and each thread is waitting for the jobs, each thread has one queue.
    bioInit();
    std::vector<std::string> input{
        "a",
        "b",
        "c",
        "d",
        "e",
        "f",
        "g",
        "h",
        "i",
        "j"
    };
    int type = 0;
    for(int i=0;i<10000;i++) {
        //send to thread, whose number is type; arg1 is (input[i%10].c_str()) and arg2 is NULL
        //the thread can extract the two arguments from JOB
        bioCreateBackgroundJob(type,(input[i%10].c_str()),NULL,0);
        type+=REDIS_BIO_NUM_OPS;
        type%=10;
    }

    //Send 1 to stop the threads
    for(int i=0;i<10;i++) {
        bioCreateBackgroundJob(type,NULL,NULL,1);
        type+=1;
        type%=REDIS_BIO_NUM_OPS;
    }
    printf("tobe joined\n");
    bioKillThreads();
    return 0;
}
