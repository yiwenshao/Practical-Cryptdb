#include <vector>
#include "util/errstream.hh"
#include "crypto/search.hh"
static void
test_search(){
    search_priv s("my key");
    auto cl = s.transform({"hello", "world", "hello", "testing", "test"});
    throw_c(s.match(cl, s.wordkey("hello")));
    throw_c(!s.match(cl, s.wordkey("Hello")));
    throw_c(s.match(cl, s.wordkey("world")));
}
int main(){
    test_search();
    return 0;
}
