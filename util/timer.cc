#include "util/timer.hh"
#include <functional>
#include <string>
#include <string.h>

uint64_t gcur_usec(){
    struct timeval tv;
    gettimeofday(&tv, 0);
    return ((uint64_t)tv.tv_sec) * 1000000 + tv.tv_usec;
}


std::string
ggetpRandomName(int out_length){
    static const char valids[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    char output[out_length + 1];
    std::function<bool()> wrap_srand =[](){srand(gcur_usec()); return true;};
    std::function<void(bool)> do_nothing = [] (bool b) {return;};
    static bool danger_will_robinson = wrap_srand();
    do_nothing(danger_will_robinson);

    for (int i = 0; i < out_length; ++i) {
        output[i] = valids[rand() % strlen(valids)];
    }
    output[out_length] = 0;
    return std::string(output);
}
