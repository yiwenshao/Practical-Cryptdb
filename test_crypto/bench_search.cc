#include <vector>
#include "util/errstream.hh"
#include "crypto/search.hh"
static void
test_search(int num_of_tests,int len){
    search_priv s("my key");
    std::string input = std::string(len,'a');
    auto cl = s.transform({"hexxxxxxxxxxxxxxxxxxxllo", "world", input});
    std::cout<<cl.size()<<std::endl;
}
int main(){
    test_search(10,8);
    return 0;
}
