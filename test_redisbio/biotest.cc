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
//    printf("id=%lu %s\n",id,query);
    return 0;
}

int
main(){
    userFunction = work;
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
        bioCreateBackgroundJob(type,(input[i%10].c_str()),NULL,0);
        type+=1;
        type%=10;
    }
    for(int i=0;i<10;i++) {
        bioCreateBackgroundJob(type,NULL,NULL,1);
        type+=1;
        type%=10;
    }
    printf("tobe joined\n");
    bioKillThreads();
    return 0;
}
