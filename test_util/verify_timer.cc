#include "util/timer.hh"
#include <iostream>
#include <string>
#include <unistd.h>
using std::cout;
using std::endl;

int
main(){
    timer t;
    sleep(1);
    cout<<t.lap()<<endl;
    return 0;
}
