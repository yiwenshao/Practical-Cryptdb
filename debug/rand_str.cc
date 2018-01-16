#include <string>
#include <iostream>
#include <functional>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static
uint64_t cur_usec() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return ((uint64_t)tv.tv_sec) * 1000000 + tv.tv_usec;
}

using namespace std;
static
std::string
getpRandomName(int out_length = 10){
    static const char valids[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    char output[out_length + 1];
//    std::function<bool()> wrap_srand =[](){srand(time(NULL)); return true;};
    std::function<bool()> wrap_srand =[](){srand(cur_usec()); return true;};
    std::function<void(bool)> do_nothing = [] (bool b) {return;};
    static bool danger_will_robinson = wrap_srand();
    do_nothing(danger_will_robinson);
      
    for (int i = 0; i < out_length; ++i) {
        output[i] = valids[rand() % strlen(valids)];
    }                     
    output[out_length] = 0;
    return std::string(output);
}

int main(int argc,char** argv){
    if(argc!=2){
        exit(0);
    }
    int length = stoi(string(argv[1]));
    cout<<getpRandomName(length)<<endl;
    return 0;
}
