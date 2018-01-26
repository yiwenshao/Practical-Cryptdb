#pragma once

#include <sys/time.h>
#include <time.h>

class timer {
 private:
    timer(const timer &t);  /* no reason to copy timer objects */

 public:
    timer() { lap(); }

    unsigned long lap() {        /* returns microseconds */
        unsigned long t0 = start;
        unsigned long t1 = cur_usec();
        start = t1;
        return t1 - t0;
    }

 private://uint64_t ?? not a type
    static unsigned long cur_usec() {
        struct timeval tv;
        gettimeofday(&tv, 0);
        return ((unsigned long)tv.tv_sec) * 1000000 + tv.tv_usec;
    }

    unsigned long start;
};
