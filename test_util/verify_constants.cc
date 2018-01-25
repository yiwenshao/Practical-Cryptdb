#include "util/constants.hh"
#include <iostream>
using std::cout;
using std::endl;

int 
main(){
    cout<<"loadCount:"<<constGlobalConstants.loadCount<<endl;
    cout<<"pipilineCount:"<<constGlobalConstants.pipelineCount<<endl;
    cout<<"logFile:"<<constGlobalConstants.logFile<<endl;
    return 0;
}
